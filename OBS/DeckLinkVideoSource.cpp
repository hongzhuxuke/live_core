#include "Main.h"
#include "DeckLinkVideoSource.h"  
#include "IDeckLinkDevice.h"
#include "IDShowPlugin.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/convert_argb.h"
#include "arraysize.h"
BITMAPINFO bitmap;
std::unique_ptr<uint8_t[]> rgbImage;
struct ResSize {
   UINT cx;
   UINT cy;
};

enum {
   COLORSPACE_AUTO,
   COLORSPACE_709,
   COLORSPACE_601
};

#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   EXTERN_C const GUID DECLSPEC_SELECTANY name \
   = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

#define NEAR_SILENT  3000
#define NEAR_SILENTf 3000.0



DeinterlacerConfig deinterlacerConfigs[DEINTERLACING_TYPE_LAST] = {
   { DEINTERLACING_NONE, FIELD_ORDER_NONE, DEINTERLACING_PROCESSOR_CPU },
   { DEINTERLACING_DISCARD, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_CPU },
   { DEINTERLACING_RETRO, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_CPU | DEINTERLACING_PROCESSOR_GPU, true },
   { DEINTERLACING_BLEND, FIELD_ORDER_NONE, DEINTERLACING_PROCESSOR_GPU },
   { DEINTERLACING_BLEND2x, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU, true },
   { DEINTERLACING_LINEAR, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU },
   { DEINTERLACING_LINEAR2x, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU, true },
   { DEINTERLACING_YADIF, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU },
   { DEINTERLACING_YADIF2x, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU, true },
   { DEINTERLACING__DEBUG, FIELD_ORDER_TFF | FIELD_ORDER_BFF, DEINTERLACING_PROCESSOR_GPU },
};
void DeckLinkVideoSource::ReInit()
{
   String deviceName = mDataElement->GetString(L"deviceName");
   OSEnterMutex(mDeckLinkDeviceMutex);

   mDeckLinkDevice = GetDeckLinkDevice(deviceName,this);
   if (mDeckLinkDevice->IsCapturing() == false) {
      mDeckLinkDevice->StartCapture();
   }
   OSLeaveMutex(mDeckLinkDeviceMutex);

   UpdateSettings();

   return ;
}
void DeckLinkVideoSource::Reload()
{
   ReInit();
}

bool DeckLinkVideoSource::Init(XElement *mDataElement) {
   mDeckLinkDeviceMutex=OSCreateMutex();
   if (!mDeckLinkDeviceMutex) {
      AppWarning(TEXT("DShowPlugin: could not create device mutex"));
      return false;
   }
   bool ret = true;
   mSampleMutex = OSCreateMutex();
   mTipTexture=NULL;
   if (!mSampleMutex) {
      AppWarning(TEXT("DShowPlugin: could not create sample mutex"));
      return false;
   }

   this->mDataElement = mDataElement;
   String deviceName = mDataElement->GetString(L"deviceName");

   
   OSEnterMutex(mDeckLinkDeviceMutex);
   mDeckLinkDevice = GetDeckLinkDevice(deviceName,this);
   if(mDeckLinkDevice==NULL)
   {
      OSLeaveMutex(mDeckLinkDeviceMutex);
      return false;
   }
   
   mStrDeviceName=deviceName;
   if (mDeckLinkDevice->IsCapturing() == false) {
      ret = mDeckLinkDevice->StartCapture();
   }
   OSLeaveMutex(mDeckLinkDeviceMutex);

   UpdateSettings();

   mTipTexture=CreateTextureFromFile(L"disconnected.png",TRUE);
   mDeviceExist=true;

   mDeckLinkEventHandle=RegistterDeckLinkDeviceEvent(DeckLinkVideoSource::DeckLinkEventCallBack,this);
   
   Log(TEXT("Using DeckLinkVideoSource input"));

   return true;
}
void DeckLinkVideoSource::DeckLinkEventCallBack(DeckLinkDeviceEventEnum e,void *p,void *_this)
{
   DeckLinkVideoSource *__this=(DeckLinkVideoSource *)_this;
   if(__this)
   {
      __this->EventNotify(e,p);
   }
}
void DeckLinkVideoSource::EventNotify(DeckLinkDeviceEventEnum e,void *devWCharId)
{
   wchar_t *deviceId=(wchar_t *)devWCharId;
   if(mStrDeviceName==(CTSTR)deviceId)
   {
      switch(e)
      {
         case DeckLinkDeviceAdd:
            SetDeviceExist(true);
            //UpdateSettings();
            //Init(this->mDataElement);
            ReInit();
            break;
         case DeckLinkDeviceRemove:
            SetDeviceExist(false);
            break;
         case DeckLinkDeviceImageFormatChanged:
            ImageFormatChanged();
            break;
         default:
            break;
      }
   }
}
void DeckLinkVideoSource::SetDeviceExist(bool exist)
{
   OSEnterMutex(mDeckLinkDeviceMutex);
   mDeviceExist=exist;
   OSLeaveMutex(mDeckLinkDeviceMutex);
}
bool DeckLinkVideoSource::GetDeviceExist()
{
   bool exist=false;
   OSEnterMutex(mDeckLinkDeviceMutex);
   exist=mDeviceExist;
   OSLeaveMutex(mDeckLinkDeviceMutex);
   return exist;
}
void DeckLinkVideoSource::ImageFormatChanged()
{
   OSEnterMutex(mDeckLinkDeviceMutex);

   UINT videoWidth=0;
   UINT videoHeight = 0;
   int frameDuration=0;
   
   mDeckLinkDevice->GetDeviceInfo(videoWidth, videoHeight, frameDuration);
   mImageCX=videoWidth;
   mImageCY=videoHeight;

   OSLeaveMutex(mDeckLinkDeviceMutex);
}

DeckLinkVideoSource::~DeckLinkVideoSource() {
   if(mDeckLinkEventHandle)
   {
      UnRegistterDeckLinkDeviceEvent(mDeckLinkEventHandle);
   }
   
   Stop();
   UnloadFilters();
   FlushSamples();

   if (mSampleMutex)
   {
      OSCloseMutex(mSampleMutex);
      mSampleMutex=NULL;
   }

   if(mDeckLinkDeviceMutex)
   {
      OSCloseMutex(mDeckLinkDeviceMutex);
      mDeckLinkDeviceMutex=NULL;
   }
   
   if(mTipTexture)
   {
      delete mTipTexture;
      mTipTexture=NULL;
   }
   
   ReleaseDeckLinkDevice(mDeckLinkDevice,this);
}

#define SHADER_PATH TEXT("shaders/")

String DeckLinkVideoSource::ChooseShader() {
   String strShader;
   strShader << SHADER_PATH;
   strShader << TEXT("UYVToRGB.pShader");
   return strShader;
}

String DeckLinkVideoSource::ChooseDeinterlacingShader() {
   String shader;
   shader << SHADER_PATH << TEXT("Deinterlace_");

#ifdef _DEBUG
#define DEBUG__ _DEBUG
#undef _DEBUG
#endif
#define SELECT(x) case DEINTERLACING_##x: shader << String(TEXT(#x)).MakeLower(); break;
   switch (mDeinterlacer.type) {
      SELECT(RETRO)
         SELECT(BLEND)
         SELECT(BLEND2x)
         SELECT(LINEAR)
         SELECT(LINEAR2x)
         SELECT(YADIF)
         SELECT(YADIF2x)
         SELECT(_DEBUG)
   }
   return shader << TEXT(".pShader");
#undef SELECT
#ifdef DEBUG__
#define _DEBUG DEBUG__
#undef DEBUG__
#endif
}

const float yuv709Mat[16] = { 0.182586f, 0.614231f, 0.062007f, 0.062745f,
-0.100644f, -0.338572f, 0.439216f, 0.501961f,
0.439216f, -0.398942f, -0.040274f, 0.501961f,
0.000000f, 0.000000f, 0.000000f, 1.000000f };

const float yuvMat[16] = { 0.256788f, 0.504129f, 0.097906f, 0.062745f,
-0.148223f, -0.290993f, 0.439216f, 0.501961f,
0.439216f, -0.367788f, -0.071427f, 0.501961f,
0.000000f, 0.000000f, 0.000000f, 1.000000f };

const float yuvToRGB601[2][16] =
{
   {
      1.164384f, 0.000000f, 1.596027f, -0.874202f,
      1.164384f, -0.391762f, -0.812968f, 0.531668f,
      1.164384f, 2.017232f, 0.000000f, -1.085631f,
      0.000000f, 0.000000f, 0.000000f, 1.000000f
   },
   {
      1.000000f, 0.000000f, 1.407520f, -0.706520f,
      1.000000f, -0.345491f, -0.716948f, 0.533303f,
      1.000000f, 1.778976f, 0.000000f, -0.892976f,
      0.000000f, 0.000000f, 0.000000f, 1.000000f
   }
};

const float yuvToRGB709[2][16] = {
   {
      1.164384f, 0.000000f, 1.792741f, -0.972945f,
      1.164384f, -0.213249f, -0.532909f, 0.301483f,
      1.164384f, 2.112402f, 0.000000f, -1.133402f,
      0.000000f, 0.000000f, 0.000000f, 1.000000f
   },
   {
      1.000000f, 0.000000f, 1.581000f, -0.793600f,
      1.000000f, -0.188062f, -0.469967f, 0.330305f,
      1.000000f, 1.862906f, 0.000000f, -0.935106f,
      0.000000f, 0.000000f, 0.000000f, 1.000000f
   }
};

bool DeckLinkVideoSource::LoadFilters() {
   if (mIsCapturing || mIsFiltersLoaded)
      return false;
   bool bSucceeded = false;
   bool bAddedVideoCapture = false, bAddedAudioCapture = false, bAddedDevice = false;
   String strShader;

   mDeinterlacer.isReady = true;
   mIsUseThreadedConversion = OBSAPI_UseMultithreadedOptimizations() && (OSGetTotalCores() > 1);
   //------------------------------------------------
   // basic initialization vars

   mIsUseCustomResolution = mDataElement->GetInt(TEXT("customResolution"));
   mStrDevice = mDataElement->GetString(TEXT("device"));
   m_privew_renderHwnd = (HWND)mDataElement->GetInt(L"privew_renderHwnd", NULL);

   mStrDeviceName = mDataElement->GetString(TEXT("deviceName"),L"");
   if(mStrDeviceName.Length())
   {
      wcscpy(mDeviceInfo.m_sDeviceName,mStrDeviceName.Array());
   }
   mStrDeviceID = mDataElement->GetString(TEXT("deviceID"),L"");
   if(mStrDeviceID.Length())
   {
      wcscpy(mDeviceInfo.m_sDeviceID,mStrDeviceID.Array());
   }
   mDisplayName= mDataElement->GetString(TEXT("displayName"),L"");
   if(mDisplayName.Length())
   {
      wcscpy(mDeviceInfo.m_sDeviceDisPlayName,mDisplayName.Array());
   }
   mDeviceInfo.m_sDeviceType=TYPE_DECKLINK;
   UINT Twidth;

   UINT Theight;
   int TframeInternal;
   DeinterlacingType deinterlacingType=DEINTERLACING_NONE;
   VideoFormat format;
   if(GetDeviceDefaultAttribute(mDeviceInfo,Twidth,Theight,TframeInternal,deinterlacingType,format))
   {
      mDeinterlacer.type=deinterlacingType;
   }
   else
   {
      deinterlacingType=DEINTERLACING_NONE;
   }


   mFullRange = mDataElement->GetInt(TEXT("fullrange")) != 0;
   mIsUse709 = false;
   

   mIsFlipVertical = mDataElement->GetInt(TEXT("flipImage")) != 0;
   mIsFlipHorizontal = mDataElement->GetInt(TEXT("flipImageHorizontal")) != 0;
   mIsUsePointFiltering = mDataElement->GetInt(TEXT("usePointFiltering")) != 0;

   bool elgato = sstri(mStrDeviceName, L"elgato") != nullptr;

   mOpacity = mDataElement->GetInt(TEXT("opacity"), 100);

   float volume = mDataElement->GetFloat(TEXT("volume"), 1.0f);

   mIsUseBuffering = mDataElement->GetInt(TEXT("useBuffering")) != 0;
   mBufferTime = mDataElement->GetInt(TEXT("mBufferTime")) * 10000;

   //------------------------------------------------
   // chrom key stuff

   mIsUseChromaKey = mDataElement->GetInt(TEXT("useChromaKey")) != 0;
   mKeyColor = mDataElement->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
   mKeySimilarity = mDataElement->GetInt(TEXT("keySimilarity"));
   mKeyBlend = mDataElement->GetInt(TEXT("keyBlend"), 80);
   mKeySpillReduction = mDataElement->GetInt(TEXT("keySpillReduction"), 50);

//   mDeinterlacer.type = mDataElement->GetInt(TEXT("deinterlacingType"), 0);




   mDeinterlacer.fieldOrder = mDataElement->GetInt(TEXT("deinterlacingFieldOrder"), 0);
   mDeinterlacer.processor = mDataElement->GetInt(TEXT("deinterlacingProcessor"), 0);
   mDeinterlacer.doublesFramerate = mDataElement->GetInt(TEXT("deinterlacingDoublesFramerate"), 0) != 0;

   if (mKeyBaseColor.x < mKeyBaseColor.y && mKeyBaseColor.x < mKeyBaseColor.z)
      mKeyBaseColor -= mKeyBaseColor.x;
   else if (mKeyBaseColor.y < mKeyBaseColor.x && mKeyBaseColor.y < mKeyBaseColor.z)
      mKeyBaseColor -= mKeyBaseColor.y;
   else if (mKeyBaseColor.z < mKeyBaseColor.x && mKeyBaseColor.z < mKeyBaseColor.y)
      mKeyBaseColor -= mKeyBaseColor.z;


   mRenderCX = mRenderCY = mNewCX = mNewCY = 0;
   mFrameInterval = 0;

   long long mFrameDuration;
   long long mTimeScale;
   UINT cx, cy;
   int frameInternal;
   
   OSEnterMutex(mDeckLinkDeviceMutex);
   bSucceeded=mDeckLinkDevice->GetDeviceInfo(cx,cy,frameInternal);
   OSLeaveMutex(mDeckLinkDeviceMutex);
   

   if (bSucceeded == true) {
      mRenderCX = mNewCX = cx;
      mRenderCY = mNewCY = cy;
   } else {
      mRenderCX = mNewCX = 1920;
      mRenderCY = mNewCY = 1080;
   }
   mFrameInterval = 25;


   if (!mRenderCX || !mRenderCY || !mFrameInterval) {
      AppWarning(TEXT("DShowPlugin: Invalid size/fps specified"));
      goto cleanFinish;
   }

   mPreferredOutputType = (mDataElement->GetInt(TEXT("usePreferredType")) != 0) ? mDataElement->GetInt(TEXT("preferredType")) : -1;
   mIsFirstFrame = true;

   mColorType = DeviceOutputType_UYVY;

   strShader = ChooseShader();
   if (strShader.IsValid())
      mColorConvertShader = CreatePixelShaderFromFile(strShader);

   if (mColorType != DeviceOutputType_RGB && !mColorConvertShader) {
      AppWarning(TEXT("DShowPlugin: Could not create color space conversion pixel shader"));
      goto cleanFinish;
   }

   //------------------------------------------------
   // set chroma details   
   mKeyBaseColor = Color4().MakeFromRGBA(mKeyColor);
   //Matrix4x4TransformVect(mKeyChroma, (mColorType == DeviceOutputType_HDYC || (mColorType <= DeviceOutputType_ARGB32 || mColorType >= DeviceOutputType_Y41P)) ? (float*)yuv709Mat : (float*)yuvMat, mKeyBaseColor);
   Matrix4x4TransformVect(mKeyChroma, (mColorType == DeviceOutputType_HDYC || mColorType == DeviceOutputType_RGB) ? (float*)yuv709Mat : (float*)yuvMat, mKeyBaseColor);
   mKeyChroma *= 2.0f;

   bAddedVideoCapture = true;
   bAddedDevice = true;
   //------------------------------------------------
   // connect all pins and set up the whole capture thing

   bool bConnected = false;
   bSucceeded = true;
cleanFinish:

   /*  for (UINT i = 0; i < outputList.Num(); i++)
        outputList[i].FreeData();*/

   if (!bSucceeded) {
      mIsCapturing = false;

      if (mStopSampleEvent) {
         CloseHandle(mStopSampleEvent);
         mStopSampleEvent = NULL;
      }

      if (mColorConvertShader) {
         delete mColorConvertShader;
         mColorConvertShader = NULL;
      }

      if (mImageBuffer) {
         Free(mImageBuffer);
         mImageBuffer = NULL;
      }
      mIsReadyToDraw = true;
   } else
      mIsReadyToDraw = false;

   // Updated check to ensure that the source actually turns red instead of
   // screwing up the size when SetFormat fails.
   if (mRenderCX <= 0 || mRenderCX >= 8192) { mNewCX = mRenderCX = 32; mImageCX = mRenderCX; }
   if (mRenderCY <= 0 || mRenderCY >= 8192) { mNewCY = mRenderCY = 32; mImageCY = mRenderCY; }

   ChangeSize(bSucceeded, true);

   return bSucceeded;
}

void DeckLinkVideoSource::UnloadFilters() {
   if (mTexture) {
      delete mTexture;
      mTexture = NULL;
   }
   if (mPreviousTexture) {
      delete mPreviousTexture;
      mPreviousTexture = NULL;
   }

   if (mIsFiltersLoaded) {
      mIsFiltersLoaded = false;
   }

   if (mColorConvertShader) {
      delete mColorConvertShader;
      mColorConvertShader = NULL;
   }

   if (mImageBuffer) {
      Free(mImageBuffer);
      mImageBuffer = NULL;
   }
}

void DeckLinkVideoSource::Start() {
   if (mIsCapturing)
      return;
   OSEnterMutex(mDeckLinkDeviceMutex);
   if (mDeckLinkDevice->IsCapturing() == false) {
      mDeckLinkDevice->StartCapture();
   }
   OSLeaveMutex(mDeckLinkDeviceMutex);
   //mDeckLinkDevice->EnableVideo(true, NULL);

   mDrawShader = CreatePixelShaderFromFile(TEXT("shaders\\DrawTexture_ColorAdjust.pShader"));

   mIsCapturing = true;
}

void DeckLinkVideoSource::Stop() {
   delete mDrawShader;
   mDrawShader = NULL;

   if (!mIsCapturing)
      return;

   mIsCapturing = false;

   FlushSamples();
}

void DeckLinkVideoSource::BeginScene() {
   Start();
}

void DeckLinkVideoSource::EndScene() {
   Stop();
}
UINT DeckLinkVideoSource::GetSampleInsertIndex(LONGLONG timestamp) {
   UINT index;
   for (index = 0; index<mSamples.Num(); index++) {
      if (mSamples[index]->timestamp > timestamp)
         return index;
   }

   return index;
}
void DeckLinkVideoSource::ChangeSize(bool bSucceeded, bool bForce) {
   if (!bForce && mRenderCX == mNewCX && mRenderCY == mNewCY)
      return;

   mRenderCX = mNewCX;
   mRenderCY = mNewCY;
   mColorType = DeviceOutputType_UYVY;
   //mDeinterlacer.type = DEINTERLACING_NONE;
   switch (mColorType) {
   case DeviceOutputType_I420:
   case DeviceOutputType_YV12:
      mLineSize = mRenderCX; //per plane
      break;
   case DeviceOutputType_YVYU:
   case DeviceOutputType_YUY2:
   case DeviceOutputType_UYVY:
   case DeviceOutputType_HDYC:
      mLineSize = (mRenderCX * 2);
      break;
  //case DeviceOutputType_RGB24:
  //case DeviceOutputType_RGB32:
  //case DeviceOutputType_ARGB32:
   case DeviceOutputType_RGB:
	   mLineSize = mRenderCX * 4;
	   break;
   default:break;
   }

   mLinePitch = mLineSize;
   mLineShift = 0;
   mImageCX = mRenderCX;
   mImageCY = mRenderCY;

   mDeinterlacer.mImageCX = mRenderCX;
   mDeinterlacer.mImageCY = mRenderCY;

   if (mDeinterlacer.doublesFramerate)
      mDeinterlacer.mImageCX *= 2;

   switch (mDeinterlacer.type) {
   case DEINTERLACING_DISCARD:
      mDeinterlacer.mImageCY = mRenderCY / 2;
      mLinePitch = mLineSize * 2;
      mRenderCY /= 2;
      break;

   case DEINTERLACING_RETRO:
      mDeinterlacer.mImageCY = mRenderCY / 2;
      if (mDeinterlacer.processor != DEINTERLACING_PROCESSOR_GPU) {
         mLineSize *= 2;
         mLinePitch = mLineSize;
         mRenderCY /= 2;
         mRenderCX *= 2;
      }
      break;

   case DEINTERLACING__DEBUG:
      mDeinterlacer.mImageCX *= 2;
      mDeinterlacer.mImageCY *= 2;
   case DEINTERLACING_BLEND2x:
      //case DEINTERLACING_MEAN2x:
   case DEINTERLACING_YADIF:
   case DEINTERLACING_YADIF2x:
      mDeinterlacer.needsPreviousFrame = true;
      break;
   }

   if (mDeinterlacer.type != DEINTERLACING_NONE && mDeinterlacer.processor == DEINTERLACING_PROCESSOR_GPU) {
      mDeinterlacer.vertexShader.reset(CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader")));
      mDeinterlacer.pixelShader = CreatePixelShaderFromFileAsync(ChooseDeinterlacingShader());
      mDeinterlacer.isReady = false;
   }

   if (mTexture) {
      delete mTexture;
      mTexture = NULL;
   }
   if (mPreviousTexture) {
      delete mPreviousTexture;
      mPreviousTexture = NULL;
   }

   //-----------------------------------------------------
   // create the mTexture regardless, will just show up as red to indicate failure
   BYTE *textureData = (BYTE*)Allocate(mRenderCX*mRenderCY * 4);

  //if we're working with planar YUV, we can just use regular RGB textures instead
   msetd(textureData, 0xFF000000, mRenderCX*mRenderCY * 4);
   mTexture = CreateTexture(mRenderCX, mRenderCY, GS_RGB, textureData, FALSE, FALSE);
   if (bSucceeded && mDeinterlacer.needsPreviousFrame)
      mPreviousTexture = CreateTexture(mRenderCX, mRenderCY, GS_RGB, textureData, FALSE, FALSE);
   if (bSucceeded && mDeinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
      mDeinterlacer.texture.reset(CreateRenderTarget(mDeinterlacer.mImageCX, mDeinterlacer.mImageCY, GS_BGRA, FALSE));

   Free(textureData);
   mIsFiltersLoaded = bSucceeded;
   mImageSize = mRenderCY*  mLineSize;
}

void DeckLinkVideoSource::SetPreviewState(bool is_preivew) {
   m_bIsPreView = is_preivew;
}

void BitMapSetSize(DeviceColorType type, int width, int height) {
   if (width == bitmap.bmiHeader.biWidth && height == bitmap.bmiHeader.biHeight) {
      return;
   }

   bitmap.bmiHeader.biWidth = width;
   bitmap.bmiHeader.biHeight = height;
   bitmap.bmiHeader.biSizeImage = type == DeviceOutputType_RGB ? (width * height * 3) : (width * height * 4);
   bitmap.bmiHeader.biBitCount = DeviceOutputType_RGB ? 24 : 32;
   bitmap.bmiHeader.biPlanes = 1;
   bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
}

void ImageRender(DeviceColorType type, HWND desHwnd, LPBYTE bytes, int cx, int cy) {
   if (desHwnd == NULL) {
      return;
   }

   BitMapSetSize(type, cx, cy);
   HWND wnd_ = desHwnd;
   RECT rc;
   ::GetClientRect(wnd_, &rc);

   BITMAPINFO& bmi = bitmap;
   int height = abs(bmi.bmiHeader.biHeight);
   int width = bmi.bmiHeader.biWidth;

   const uint8_t* image = bytes;
   if (image != NULL) {
      auto mWindowDC = ::GetDC(wnd_);
      HDC dc_mem = ::CreateCompatibleDC(mWindowDC);
      ::SetStretchBltMode(dc_mem, HALFTONE);

      // Set the map mode so that the ratio will be maintained for us.
      HDC all_dc[] = { mWindowDC, dc_mem };
      for (int i = 0; i < arraysize(all_dc); ++i) {
         SetMapMode(all_dc[i], MM_ISOTROPIC);
         SetWindowExtEx(all_dc[i], width, height, NULL);
         SetViewportExtEx(all_dc[i], rc.right, rc.bottom, NULL);
      }

      HBITMAP bmp_mem = ::CreateCompatibleBitmap(mWindowDC, rc.right, rc.bottom);
      HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);

      POINT logical_area = { rc.right, rc.bottom };
      DPtoLP(mWindowDC, &logical_area, 1);

      HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
      RECT logical_rect = { 0, 0, logical_area.x, logical_area.y };
      ::FillRect(dc_mem, &logical_rect, brush);
      ::DeleteObject(brush);

      int x = (logical_area.x / 2) - (width / 2);
      int y = (logical_area.y / 2) - (height / 2);
      if (DeviceOutputType_BGRX == type) {
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      if (DeviceOutputType_I420 == type || DeviceOutputType_YV12 == type) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * height / 4;
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::I420ToARGB(bytes, width, src_u, width / 2, src_v, width / 2, rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      else if (type == DeviceOutputType_YVYU || type == DeviceOutputType_YUY2) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * height / 4;
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::YUY2ToARGB(bytes, width * 2, rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      else if (type == DeviceOutputType_UYVY || type == DeviceOutputType_HDYC) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * height / 4;
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::UYVYToARGB(bytes, width * 2, rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      ::SelectObject(dc_mem, bmp_old);
      ::DeleteObject(bmp_mem);
      ::DeleteDC(dc_mem);
      ::ReleaseDC(wnd_, mWindowDC);
      //delete []dst_argb;
   }
}

bool DeckLinkVideoSource::Preprocess() {
   if (!mIsCapturing)
      return true;

   
   SampleData *lastSample = NULL;


   long long mFrameDuration;
   long long mTimeScale;
   UINT cx, cy;
   //OSEnterMutex(mDeckLinkDeviceMutex);
   int frameInternal;
   mDeckLinkDevice->GetDeviceInfo(cx,cy,frameInternal);
   //OSLeaveMutex(mDeckLinkDeviceMutex);
   mNewCX = (UINT)cx;
   mNewCY = (UINT)cy;
   //----------------------------------------

   int numThreads = MAX(OSGetTotalCores() - 2, 1);
   {
      if (mPreviousTexture) {
         Texture *tmp = mTexture;
         mTexture = mPreviousTexture;
         mPreviousTexture = tmp;
      }
      mDeinterlacer.curField = mDeinterlacer.processor == DEINTERLACING_PROCESSOR_GPU ? false : (mDeinterlacer.fieldOrder == FIELD_ORDER_BFF);
      mDeinterlacer.bNewFrame = true;
      mColorType = DeviceOutputType_UYVY;


      LPBYTE lpData;
      UINT pitch;

      ChangeSize();

      if (mTexture->Map(lpData, pitch)) {
         unsigned long long bufferSize;
         void *buffer = NULL;
         unsigned long long videoTime = 0;
         bool ret = mDeckLinkDevice->GetNextVideoBuffer(&buffer, &bufferSize, &videoTime);
         if (ret && buffer && mImageSize <= bufferSize) {
            mColorType = DeviceOutputType_UYVY;
            if (m_bIsPreView && buffer) {
               ImageRender(DeviceOutputType_UYVY, m_privew_renderHwnd, lastSample->lpData, mNewCX, mNewCY);
            }
            Convert422To444(lpData, (LPBYTE)buffer, pitch, false);
         }
         mTexture->Unmap();
         mIsReadyToDraw = true;
      }

      if (lastSample)lastSample->Release();

      if (mIsReadyToDraw &&
          mDeinterlacer.type != DEINTERLACING_NONE &&
          mDeinterlacer.processor == DEINTERLACING_PROCESSOR_GPU &&
          mDeinterlacer.texture.get() &&
          mDeinterlacer.pixelShader.Shader()) {

         SetRenderTarget(mDeinterlacer.texture.get());

         Shader *oldVertShader = GetCurrentVertexShader();
         LoadVertexShader(mDeinterlacer.vertexShader.get());

         Shader *oldShader = GetCurrentPixelShader();
         LoadPixelShader(mDeinterlacer.pixelShader.Shader());

         HANDLE hField = mDeinterlacer.pixelShader.Shader()->GetParameterByName(TEXT("field_order"));
         if (hField)
            mDeinterlacer.pixelShader.Shader()->SetBool(hField, mDeinterlacer.fieldOrder == FIELD_ORDER_BFF);

         Ortho(0.0f, float(mDeinterlacer.mImageCX), float(mDeinterlacer.mImageCY), 0.0f, -100.0f, 100.0f);
         SetViewport(0.0f, 0.0f, float(mDeinterlacer.mImageCX), float(mDeinterlacer.mImageCY));

         if (mPreviousTexture)
            LoadTexture(mPreviousTexture, 1);

         DrawSpriteEx(mTexture, 0xFFFFFFFF, 0.0f, 0.0f, float(mDeinterlacer.mImageCX), float(mDeinterlacer.mImageCY), 0.0f, 0.0f, 1.0f, 1.0f);

         if (mPreviousTexture)
            LoadTexture(nullptr, 1);

         LoadPixelShader(oldShader);
         LoadVertexShader(oldVertShader);
         mDeinterlacer.isReady = true;
      }
   }
   return true;
}

void DeckLinkVideoSource::Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType) {
   if (mTexture && mIsReadyToDraw && mDeinterlacer.isReady) {
      Shader *oldShader = GetCurrentPixelShader();
      SamplerState *sampler = NULL;

      mGamma = mDataElement->GetInt(TEXT("gamma"), 100);
      float fGamma = float(-(mGamma - 100) + 100) * 0.01f;
      
      if (mColorConvertShader) {
         LoadPixelShader(mColorConvertShader);
         if (mIsUseChromaKey) {
            float fSimilarity = float(mKeySimilarity) / 1000.0f;
            float fBlendVal = float(max(mKeyBlend, 1) / 1000.0f);
            float fSpillVal = (float(max(mKeySpillReduction, 1)) / 1000.0f);

            Vect2 pixelSize = 1.0f / GetSize();

            mColorConvertShader->SetColor(mColorConvertShader->GetParameterByName(TEXT("keyBaseColor")), Color4(mKeyBaseColor));
            mColorConvertShader->SetColor(mColorConvertShader->GetParameterByName(TEXT("chromaKey")), Color4(mKeyChroma));
            mColorConvertShader->SetVector2(mColorConvertShader->GetParameterByName(TEXT("pixelSize")), pixelSize);
            mColorConvertShader->SetFloat(mColorConvertShader->GetParameterByName(TEXT("keySimilarity")), fSimilarity);
            mColorConvertShader->SetFloat(mColorConvertShader->GetParameterByName(TEXT("keyBlend")), fBlendVal);
            mColorConvertShader->SetFloat(mColorConvertShader->GetParameterByName(TEXT("keySpill")), fSpillVal);
         }
         mColorConvertShader->SetFloat(mColorConvertShader->GetParameterByName(TEXT("gamma")), fGamma);

         float mat[16];
         bool actuallyUse709 = (mColorSpace == COLORSPACE_AUTO) ? !!mIsUse709 : (mColorSpace == COLORSPACE_709);

         if (actuallyUse709)
            memcpy(mat, yuvToRGB709[mFullRange ? 1 : 0], sizeof(float)* 16);
         else
            memcpy(mat, yuvToRGB601[mFullRange ? 1 : 0], sizeof(float)* 16);

         mColorConvertShader->SetValue(mColorConvertShader->GetParameterByName(TEXT("yuvMat")), mat, sizeof(float)* 16);
      } else {
         if (fGamma != 1.0f && mIsFiltersLoaded) {
            LoadPixelShader(mDrawShader);
            HANDLE hGamma = mDrawShader->GetParameterByName(TEXT("gamma"));
            if (hGamma)
               mDrawShader->SetFloat(hGamma, fGamma);
         }
      }

      bool bFlip = !mIsFlipVertical;
      float x, x2;
      if (mIsFlipHorizontal) {
         x2 = pos.x;
         x = x2 + size.x;
      } else {
         x = pos.x;
         x2 = x + size.x;
      }

      float y = pos.y,
         y2 = y + size.y;
      if (!bFlip) {
         y2 = pos.y;
         y = y2 + size.y;
      }

      float fOpacity = float(mOpacity)*0.01f;
      DWORD opacity255 = DWORD(fOpacity*255.0f);

      if (mIsUsePointFiltering) {
         SamplerInfo samplerinfo;
         samplerinfo.filter = GS_FILTER_POINT;
         sampler = CreateSamplerState(samplerinfo);
         LoadSamplerState(sampler, 0);
      }

      Texture *tex = (mDeinterlacer.processor == DEINTERLACING_PROCESSOR_GPU && mDeinterlacer.texture.get()) ? mDeinterlacer.texture.get() : mTexture;
      if (mDeinterlacer.doublesFramerate) {
         if (!mDeinterlacer.curField)
            DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, 0.f, 0.0f, .5f, 1.f);
         else
            DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, .5f, 0.0f, 1.f, 1.f);
      } else
         DrawSprite(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2);
      
      

      
      if (mDeinterlacer.bNewFrame) {
         mDeinterlacer.curField = !mDeinterlacer.curField;
         mDeinterlacer.bNewFrame = false; //prevent switching from the second field to the first field
      }

      if (mIsUsePointFiltering) delete(sampler);

      if (mColorConvertShader || fGamma != 1.0f)
         LoadPixelShader(oldShader);

      if(!GetDeviceExist()&&mTipTexture)
      {
         int tx;
         int ty;
         int tw;
         int th;


         int cx=x;
         int cy=y;
         int cw=(x2-x)/2;
         int ch=(y2-y)/2;
         int ow=tex->Width();
         int oh=tex->Height();
         
         if(ow<mTipTexture->Width()||oh<mTipTexture->Height())
         {
            float texK=cw;
            texK/=ch;
            float tipK=mTipTexture->Width();
            tipK/=mTipTexture->Height();
            if(texK>tipK)
            {
               th=ch;
               tw=ch*tipK;
            }
            else
            {
               tw=cw;
               if(tipK!=0)
               {
                  th=tw/tipK;
               }
               else
               {
                  th=mTipTexture->Height();
               }
            }
            
            tx=(cw*2-tw)/2+cx;
            ty=(ch*2-th)/2+cy;
            
         }
         else
         {
            float sizeK=size.x;
            sizeK/=tex->Width();
            sizeK*=mTipTexture->Width();
            tw=sizeK;


            sizeK=size.y;
            sizeK/=tex->Height();
            sizeK*=mTipTexture->Height();
            th=sizeK;

            tx=(size.x-tw)/2+pos.x;
            ty=(size.y-th)/2+pos.y;
         }

         DrawSprite(mTipTexture,  0xFFFFFFFF,tx, ty, tw+tx, th+ty);
      }
   } 
}

void DeckLinkVideoSource::UpdateSettings() {
      OBSAPI_EnterSceneMutex();

      bool bWasCapturing = mIsCapturing;
      if (bWasCapturing)
         Stop();

      UnloadFilters();
      LoadFilters();

      if (bWasCapturing)
         Start();

      OBSAPI_LeaveSceneMutex();
}

Vect2 DeckLinkVideoSource::GetSize() const {
   return Vect2(float(mImageCX), float(mImageCY));
}

//now properly takes CPU cache into account - it's just so much faster than it was.
void PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT renderCX, UINT renderCY, UINT pitch, UINT startY, UINT endY, UINT linePitch, UINT lineShift) {
   LPBYTE output = convertBuffer;
   LPBYTE input = lpPlanar + lineShift;
   LPBYTE input2 = input + (renderCX*renderCY);
   LPBYTE input3 = input2 + (renderCX*renderCY / 4);

   UINT halfStartY = startY / 2;
   UINT halfX = renderCX / 2;
   UINT halfY = endY / 2;

   for (UINT y = halfStartY; y < halfY; y++) {
      LPBYTE lpLum1 = input + y * 2 * linePitch;
      LPBYTE lpLum2 = lpLum1 + linePitch;
      LPBYTE lpChroma1 = input2 + y*(linePitch / 2);
      LPBYTE lpChroma2 = input3 + y*(linePitch / 2);
      LPDWORD output1 = (LPDWORD)(output + (y * 2)*pitch);
      LPDWORD output2 = (LPDWORD)(((LPBYTE)output1) + pitch);

      for (UINT x = 0; x < halfX; x++) {
         DWORD out = (*(lpChroma1++) << 8) | (*(lpChroma2++) << 16);

         *(output1++) = *(lpLum1++) | out;
         *(output1++) = *(lpLum1++) | out;

         *(output2++) = *(lpLum2++) | out;
         *(output2++) = *(lpLum2++) | out;
      }
   }
}

void DeckLinkVideoSource::Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY) {
   DWORD size = mLineSize;
   DWORD dwDWSize = size >> 2;

   if (bLeadingY) {
      for (UINT y = 0; y < mRenderCY; y++) {
         LPDWORD output = (LPDWORD)(convertBuffer + (y*pitch));
         LPDWORD inputDW = (LPDWORD)(lp422 + (y*mLinePitch) + mLineShift);
         LPDWORD inputDWEnd = inputDW + dwDWSize;

         while (inputDW < inputDWEnd) {
            register DWORD dw = *inputDW;

            output[0] = dw;
            dw &= 0xFFFFFF00;
            dw |= BYTE(dw >> 16);
            output[1] = dw;

            output += 2;
            inputDW++;
         }
      }
   } else {
      for (UINT y = 0; y < mRenderCY; y++) {
         LPDWORD output = (LPDWORD)(convertBuffer + (y*pitch));
         LPDWORD inputDW = (LPDWORD)(lp422 + (y*mLinePitch) + mLineShift);
         LPDWORD inputDWEnd = inputDW + dwDWSize;

         while (inputDW < inputDWEnd) {
            register DWORD dw = *inputDW;

            output[0] = dw;
            dw &= 0xFFFF00FF;
            dw |= (dw >> 16) & 0xFF00;
            output[1] = dw;

            output += 2;
            inputDW++;
         }
      }
   }
}
void DeckLinkVideoSource::FlushSamples() {
   OSEnterMutex(mSampleMutex);
   for (UINT i = 0; i < mSamples.Num(); i++)
      delete mSamples[i];
   mSamples.Clear();
   //SafeRelease(mLatestVideoSample);
   OSLeaveMutex(mSampleMutex);
}

ImageSource* STDCALL CreateDeckLinkVideoSource(XElement *data) {
   String deviceName = data->GetString(DECKLINK_DEVICE_NAME);

   DeckLinkVideoSource* newDeckLinkSource = new DeckLinkVideoSource();
   DEBUG_NEW_ADDRESS(newDeckLinkSource);
   if (!newDeckLinkSource->Init(data)) {
      delete newDeckLinkSource;
      return NULL;
   }
   newDeckLinkSource->SetInteractiveResize(false);
   return newDeckLinkSource;
}

