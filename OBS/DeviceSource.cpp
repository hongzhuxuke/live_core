/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 ********************************************************************************/
#include "OBSApi.h"
#include <dshow.h>
#include <Amaudio.h>
#include <Dvdmedia.h>
#include "VH_ConstDeff.h"
#include "arraysize.h"
#include "IDShowPlugin.h"
#include "DeviceSource.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/convert_argb.h"
#define MAX_SUPPORT_W   848
#define MAX_SUPPORT_H   480

class LiveVideoFilterParam {
public:
   int beautyLevel = 0; /* 仅对摄像头有效 */
   bool enableEdgeEnhance = true; /* 仅对桌面视频有效 */
   bool enableDenoise = false; /* 仅对摄像头有效 */
   bool enableBrighter = false; /* 仅对摄像头有效 */
};
LiveVideoFilterParam mFilterParam;
FILE* mPlayerAudioFile = nullptr;
int g_imageProcess = 0;
BITMAPINFO bmi_;
std::unique_ptr<uint8_t[]> image_;
BOOL SaveBitmapToFile(HBITMAP   hBitmap, wchar_t * szfilename);
bool isEnableImageSharpening = true;



void SaveHDC(wchar_t *filename, HDC hDC, int w, int h) {


   HDC hDCMem = ::CreateCompatibleDC(hDC);//创建兼容DC   

   HBITMAP hBitMap = ::CreateCompatibleBitmap(hDC, w, h);//创建兼容位图   
   HBITMAP hOldMap = (HBITMAP)::SelectObject(hDCMem, hBitMap);//将位图选入DC，并保存返回值   

   ::BitBlt(hDCMem, 0, 0, w, h, hDC, 0, 0, SRCCOPY);//将屏幕DC的图象复制到内存DC中   

   SaveBitmapToFile(hBitMap, filename);

   ::SelectObject(hDCMem, hOldMap);//选入上次的返回值   

   //释放   
   ::DeleteObject(hBitMap);
   ::DeleteDC(hDCMem);

}


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

#define STDCALL                 __stdcall
DWORD STDCALL PackPlanarThread(ConvertData *data);

#define NEAR_SILENT  3000
#define NEAR_SILENTf 3000.0

static DeinterlacerConfig deinterlacerConfigs[DEINTERLACING_TYPE_LAST] = {
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
const int inputPriority[] =
{
   1,
   6,
   7,
   7,

   12,
   12,

   -1,
   -1,

   13,
   13,
   13,
   13,

   5,
   -1,

   10,
   10,
   10,

   9
};

BOOL SaveBitmapToFile(HBITMAP   hBitmap, wchar_t * szfilename) {
   HDC     hDC;
   //当前分辨率下每象素所占字节数            
   int     iBits;
   //位图中每象素所占字节数            
   WORD     wBitCount;
   //定义调色板大小，     位图中像素字节大小     ，位图文件大小     ，     写入文件字节数                
   DWORD     dwPaletteSize = 0, dwBmBitsSize = 0, dwDIBSize = 0, dwWritten = 0;
   //位图属性结构                
   BITMAP     Bitmap;
   //位图文件头结构            
   BITMAPFILEHEADER     bmfHdr;
   //位图信息头结构                
   BITMAPINFOHEADER     bi;
   //指向位图信息头结构                    
   LPBITMAPINFOHEADER     lpbi;
   //定义文件，分配内存句柄，调色板句柄                
   HANDLE     fh, hDib, hPal, hOldPal = NULL;

   //计算位图文件每个像素所占字节数                
   hDC = CreateDC(L"DISPLAY", NULL, NULL, NULL);
   iBits = GetDeviceCaps(hDC, BITSPIXEL)     *     GetDeviceCaps(hDC, PLANES);
   DeleteDC(hDC);
   if (iBits <= 1)
      wBitCount = 1;
   else  if (iBits <= 4)
      wBitCount = 4;
   else if (iBits <= 8)
      wBitCount = 8;
   else
      wBitCount = 24;

   GetObject(hBitmap, sizeof(Bitmap), (LPSTR)&Bitmap);
   bi.biSize = sizeof(BITMAPINFOHEADER);
   bi.biWidth = Bitmap.bmWidth;
   bi.biHeight = Bitmap.bmHeight;
   bi.biPlanes = 1;
   bi.biBitCount = wBitCount;
   bi.biCompression = BI_RGB;
   bi.biSizeImage = 0;
   bi.biXPelsPerMeter = 0;
   bi.biYPelsPerMeter = 0;
   bi.biClrImportant = 0;
   bi.biClrUsed = 0;

   dwBmBitsSize = ((Bitmap.bmWidth *wBitCount + 31) / 32) * 4 * Bitmap.bmHeight;

   //为位图内容分配内存                
   hDib = GlobalAlloc(GHND, dwBmBitsSize + dwPaletteSize + sizeof(BITMAPINFOHEADER));
   lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
   *lpbi = bi;

   //     处理调色板                    
   hPal = GetStockObject(DEFAULT_PALETTE);
   if (hPal) {
      hDC = ::GetDC(NULL);
      hOldPal = ::SelectPalette(hDC, (HPALETTE)hPal, FALSE);
      RealizePalette(hDC);
   }

   //     获取该调色板下新的像素值                
   GetDIBits(hDC, hBitmap, 0, (UINT)Bitmap.bmHeight,
             (LPSTR)lpbi + sizeof(BITMAPINFOHEADER)+dwPaletteSize,
             (BITMAPINFO *)lpbi, DIB_RGB_COLORS);

   //恢复调色板                    
   if (hOldPal) {
      ::SelectPalette(hDC, (HPALETTE)hOldPal, TRUE);
      RealizePalette(hDC);
      ::ReleaseDC(NULL, hDC);
   }

   //创建位图文件                    
   fh = CreateFile(szfilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

   if (fh == INVALID_HANDLE_VALUE)         return     FALSE;

   //     设置位图文件头                
   bmfHdr.bfType = 0x4D42;     //     "BM"                
   dwDIBSize = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+dwPaletteSize + dwBmBitsSize;
   bmfHdr.bfSize = dwDIBSize;
   bmfHdr.bfReserved1 = 0;
   bmfHdr.bfReserved2 = 0;
   bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER)+(DWORD)sizeof(BITMAPINFOHEADER)+dwPaletteSize;
   //     写入位图文件头                
   WriteFile(fh, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
   //     写入位图文件其余内容                
   WriteFile(fh, (LPSTR)lpbi, dwDIBSize, &dwWritten, NULL);
   //清除                    
   GlobalUnlock(hDib);
   GlobalFree(hDib);
   CloseHandle(fh);

   return     TRUE;

}

bool DeviceSource::Init(XElement *data) {
   hSampleMutex = OSCreateMutex();
   if (!hSampleMutex) {
      AppWarning(TEXT("DShowPlugin: could not create sample mutex"));
      return false;
   }

   m_bDeviceExist = true;
   m_device = NULL;
   int numThreads = MAX(OSGetTotalCores() - 2, 1);
   hConvertThreads = (HANDLE*)Allocate(sizeof(HANDLE)*numThreads);
   convertData = (ConvertData*)Allocate(sizeof(ConvertData)*numThreads);

   zero(hConvertThreads, sizeof(HANDLE)*numThreads);
   zero(convertData, sizeof(ConvertData)*numThreads);

   this->mElementData = data;
   UpdateSettings();

   Log(TEXT("Using directshow input"));

   m_imageBufferSize = 0;

   return true;
}
DeviceSource::DeviceSource() :tipTexture(NULL)
, m_LastSyncTime(0)
, m_deviceNameTexture(NULL)
, m_device(NULL) {
   tipTexture = CreateTextureFromFile(L"cameraLoading.png", TRUE);
   Log(L"tipTexture %p\n", tipTexture);
   m_disconnectTexture = CreateTextureFromFile(L"disconnected.png", TRUE);
   Log(L"m_disconnectTexture %p\n", m_disconnectTexture);
   m_failedTexture = CreateTextureFromFile(L"cameraLoadingFailed.png", TRUE);
   Log(L"m_failedTexture %p\n", m_failedTexture);
   m_blackBackgroundTexture = CreateTexture(100, 100, GS_BGRA, NULL, FALSE, FALSE);
   Log(L"m_blackBackgroundTexture %p\n", m_blackBackgroundTexture);
}
DeviceSource::~DeviceSource() {
   if (mBeautifyFilter) {
      mBeautifyFilter.reset();
      mBeautifyFilter = nullptr;
   }
   Stop();
   UnloadFilters();

   if (hConvertThreads)
      Free(hConvertThreads);

   if (convertData)
      Free(convertData);

   if (hSampleMutex)
      OSCloseMutex(hSampleMutex);

   ReleaseDShowVideoFilter(this);
   if (tipTexture) {
      delete tipTexture;
      tipTexture = NULL;
   }
   if (m_disconnectTexture) {
      delete m_disconnectTexture;
      m_disconnectTexture = NULL;
   }
   if (m_failedTexture) {
      delete m_failedTexture;
      m_failedTexture = NULL;
   }
   if (m_blackBackgroundTexture) {
      delete m_blackBackgroundTexture;
      m_blackBackgroundTexture = NULL;
   }
   if (m_deviceNameTexture) {
      delete m_deviceNameTexture;
      m_deviceNameTexture = NULL;
   }
   if (m_renderView != NULL) {
      OBSGraphics_DestoryRenderTargetView(m_renderView);
      m_renderView = NULL;
   }
}

#define SHADER_PATH TEXT("shaders/")


void DeviceSource::CreateBeautyFilter() {
   //回调数据是RGB24
   mBeautifyFilter.reset(new vhall::VHBeautifyRender(mBeautyLevel, [this](std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t ts) ->void {
      SampleData *lastSample = NULL;
      long bufferSize = width * height * 4;
      if (this->m_setting.colorType == DeviceOutputType_YV12 || this->m_setting.colorType == DeviceOutputType_I420) {
         bufferSize = (long)(width * height * 1.5);
         uint8_t* src_u = mRenderData.get() + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         if (this->m_setting.colorType == DeviceOutputType_I420) {
            libyuv::RGB24ToI420(data.get(), width * 3, mRenderData.get(), width, src_u, width >> 1, src_v, width >> 1, width, height);
         }
         else {
            libyuv::RGB24ToI420(data.get(), width * 3, mRenderData.get(), width, src_v , width >> 1, src_u, width >> 1, width, height);
         }
         //if (mPlayerAudioFile == nullptr) {
         //   mPlayerAudioFile = fopen("I:\\mPlayerAudioFile.yuv", "wb");
         //}
         //if (mPlayerAudioFile) {
         //   fwrite(dst_data.get(), sizeof(uint8_t), width * height * 3 / 2, mPlayerAudioFile);
         //}
      }
      else if(this->m_setting.colorType == DeviceOutputType_RGB){
         libyuv::RGB24ToARGB(data.get(), width * 3, mRenderData.get(), width * 4, width, height);
      }
      else if (this->m_setting.colorType == DeviceOutputType_YVYU || this->m_setting.colorType == DeviceOutputType_YUY2) {
         bufferSize = (long)(width * height * 2);
         libyuv::RGB24ToARGB(data.get(), width * 3, mBeautyCallbackData.get(), width * 4, width, height);
         libyuv::ARGBToYUY2(mBeautyCallbackData.get(), width * 4, mRenderData.get(), width * 2, width, height);
      }
      else if (this->m_setting.colorType == DeviceOutputType_UYVY || this->m_setting.colorType == DeviceOutputType_HDYC) {
         bufferSize = (long)(width * height * 2);
         libyuv::RGB24ToARGB(data.get(), width * 3, mBeautyCallbackData.get(), width * 4, width, height);
         libyuv::ARGBToUYVY(mBeautyCallbackData.get(), width * 4, mRenderData.get(), width * 2, width, height);
      }
      lastSample = new SampleData();
      lastSample->lpData = (LPBYTE)malloc(bufferSize);
      memcpy(lastSample->lpData, mRenderData.get(), bufferSize);
      lastSample->dataLength = bufferSize;
      lastSample->cx = width;
      lastSample->cy = height;
      newCX = width;
      newCY = height;
      lastSample->timestamp = ts;
      this->OnProcessVideoFrame(lastSample, false);
   }));
}

void DeviceSource::SetBeautyLevel(int level) {
   mBeautyLevel = level;
   if (mBeautifyFilter) {
      mBeautifyFilter->SetBeautifyLevel(level);
   }
}

String DeviceSource::ChooseShader() {

   if (m_setting.colorType == DeviceOutputType_RGB)
	//if(m_setting.colorType <= DeviceOutputType_ARGB32 /*|| m_setting.colorType >= DeviceOutputType_Y41P*/)
      return String();

   String strShader;
   strShader << SHADER_PATH;

   if (m_setting.colorType == DeviceOutputType_I420)
      strShader << TEXT("YUVToRGB.pShader");
   else if (m_setting.colorType == DeviceOutputType_YV12)
      strShader << TEXT("YVUToRGB.pShader");
   else if (m_setting.colorType == DeviceOutputType_YVYU)
      strShader << TEXT("YVXUToRGB.pShader");
   else if (m_setting.colorType == DeviceOutputType_YUY2)
      strShader << TEXT("YUXVToRGB.pShader");
   else if (m_setting.colorType == DeviceOutputType_UYVY)
      strShader << TEXT("UYVToRGB.pShader");
   else if (m_setting.colorType == DeviceOutputType_HDYC)
      strShader << TEXT("HDYCToRGB.pShader");
   else
      strShader << TEXT("ColorKey_RGB.pShader");

   return strShader;
}

String DeviceSource::ChooseDeinterlacingShader() {
   String shader;
   shader << SHADER_PATH << TEXT("Deinterlace_");
#ifdef _DEBUG
#define DEBUG__ _DEBUG
#undef _DEBUG
#endif
#define SELECT(x) case DEINTERLACING_##x: shader << String(TEXT(#x)).MakeLower(); break;
   switch (deinterlacer.type) {
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

void DeviceSource::SetDeviceExist(bool ok) {
   if (m_bDeviceExist == ok) {
      return;
   }

   if (!m_bDeviceExist) {
      Reload();
   }

   m_bDeviceExist = ok;
}
HWND gTest = NULL;
//内存泄漏
bool DeviceSource::LoadFilters() {
   if (bCapturing || bFiltersLoaded)
      return false;
   m_LastSyncTime = 0;
   m_bIsHasNoData = false;
   String          strDeviceDisplayName;
   String          strDeviceName;
   String          strDeviceID;

   INT            frameInterval;
   bool bSucceeded = false;

   bool bAddedVideoCapture = false;
   HRESULT err;

   m_renderHWND = (HWND)mElementData->GetInt(L"renderHwnd", NULL);
   //如果此处不重新加载窗口，会导致放到窗口时图像与窗口比例不正确。
   OBSAPI_EnterSceneMutex();
   if (m_renderHWND != NULL) {
      if (m_renderView != NULL) {
         OBSGraphics_DestoryRenderTargetView(m_renderView);
         m_renderView = NULL;
      }
      m_renderView = OBSGraphics_CreateRenderTargetView(m_renderHWND, 0, 0);
   }
   OBSAPI_LeaveSceneMutex();

   m_privew_renderHwnd = (HWND)mElementData->GetInt(L"privew_renderHwnd", NULL);

   deinterlacer.isReady = true;
   bUseThreadedConversion = OBSAPI_UseMultithreadedOptimizations() && (OSGetTotalCores() > 1);
   strDeviceDisplayName = mElementData->GetString(TEXT("displayName"));
   strDeviceName = mElementData->GetString(TEXT("deviceName"));
   strDeviceID = mElementData->GetString(TEXT("deviceID"));

   //deinterlacer.type = data->GetInt(TEXT("deinterlacingType"), 0);
   //deinterlacer.type = data->GetInt(TEXT("deinterlacingType"), 2);

   //此下三项没有配置
   deinterlacer.fieldOrder = mElementData->GetInt(TEXT("deinterlacingFieldOrder"), 0);
   deinterlacer.processor = mElementData->GetInt(TEXT("deinterlacingProcessor"), 0);
   deinterlacer.doublesFramerate = mElementData->GetInt(TEXT("deinterlacingDoublesFramerate"), 0) != 0;
   if (strDeviceDisplayName.Length()) {
      wcscpy(deviceInfo.m_sDeviceDisPlayName, strDeviceDisplayName.Array());
      if (m_deviceNameTexture) {
         delete m_deviceNameTexture;
         m_deviceNameTexture = NULL;
      }

      m_deviceNameTexture = CreateTextureFromText(deviceInfo.m_sDeviceDisPlayName);
   }

   if (strDeviceName.Length()) {
      wcscpy(deviceInfo.m_sDeviceName, strDeviceName.Array());
   }

   if (strDeviceID.Length()) {
      wcscpy(deviceInfo.m_sDeviceID, strDeviceID.Array());
   }

   deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;

   DShowVideoManagerEnterMutex();
   ReleaseDShowVideoFilter(this);

   //m_setting.type=(DeinterlacingType);
   //m_setting 用于获取色彩空间
   if (!GetDShowVideoFilter(m_device,
      deviceInfo,
      m_setting,
      DShowDeviceType_Video,
      DShowDevicePinType_Video,
      this, NULL)) {
      Log(L"[GetDShowVideoFilter] Failed! [%s]", deviceInfo.m_sDeviceDisPlayName);
      DShowVideoManagerLeaveMutex();
      return false;
   }

   DShowVideoManagerLeaveMutex();
   //if (m_setting.colorType < 0) {
   //   return false;
   //}
   FrameInfo currFrameInfo;
   if (!GetDShowVideoFrameInfoList(deviceInfo, NULL, &currFrameInfo, m_setting.type)) {
      Log(L"[GetDShowVideoFrameInfoList] Failed! [%s]", deviceInfo.m_sDeviceDisPlayName);
      return false;
   }
   deinterlacer.type = m_setting.type;

   //分辨率
   renderCX = newCX = currFrameInfo.maxCX;
   renderCY = newCY = currFrameInfo.maxCY;

   //帧率
   frameInterval = currFrameInfo.maxFrameInterval;

   bFirstFrame = true;
   bSucceeded = true;

cleanFinish:

   if (!bSucceeded) {
      bCapturing = false;


      if (colorConvertShader) {
         delete colorConvertShader;
         colorConvertShader = NULL;
      }

      if (lpImageBuffer) {
         Free(lpImageBuffer);
         lpImageBuffer = NULL;
      }

      bReadyToDraw = true;
   } else
      bReadyToDraw = false;

   // Updated check to ensure that the source actually turns red instead of
   // screwing up the size when SetFormat fails.
   if (renderCX <= 0 || renderCX >= 8192) { newCX = renderCX = 32; imageCX = renderCX; }
   if (renderCY <= 0 || renderCY >= 8192) { newCY = renderCY = 32; imageCY = renderCY; }

   if (colorConvertShader == NULL) {
      String strShader;
      strShader = ChooseShader();
      if (strShader.IsValid())
         colorConvertShader = CreatePixelShaderFromFile(strShader);
      ChangeSize(true, true);
   }

   ChangeSize(bSucceeded, true);

   return bSucceeded;
}

void DeviceSource::UnloadFilters() {
   if (mBeautifyFilter) {
      mBeautifyFilter.reset();
      mBeautifyFilter = nullptr;
   }
   if (m_deviceSourceTexture) {
      delete m_deviceSourceTexture;
      m_deviceSourceTexture = NULL;
   }
   if (previousTexture) {
      delete previousTexture;
      previousTexture = NULL;
   }
   if (m_deviceSourceBeautyTexture) {
      delete m_deviceSourceBeautyTexture;
      m_deviceSourceBeautyTexture = nullptr;
   }
   if (previousBeautyTexture) {
      delete previousBeautyTexture;
      previousBeautyTexture = nullptr;
   }

   KillThreads();

   if (bFiltersLoaded) {
      bFiltersLoaded = false;
   }
   if (colorConvertShader) {
      delete colorConvertShader;
      colorConvertShader = NULL;
   }

   if (lpImageBuffer) {
      Free(lpImageBuffer);
      lpImageBuffer = NULL;
   }


}

void DeviceSource::Start() {
   if (bCapturing)
      return;
   if (drawShader) {
      delete drawShader;
      drawShader = NULL;
   }
   drawShader = CreatePixelShaderFromFile(TEXT("shaders\\DrawTexture_ColorAdjust.pShader"));
   bCapturing = true;
}

void DeviceSource::Stop() {
   if (drawShader) {
      delete drawShader;
      drawShader = NULL;
   }
   if (!bCapturing)
      return;
   bCapturing = false;
}

void DeviceSource::BeginScene() {

   if (m_blackBackgroundTexture) {
      BYTE *lpData = NULL;
      UINT pitch = 0;
      if (m_blackBackgroundTexture->Map(lpData, pitch)) {
         if (lpData) {
            for (int i = 0; i < m_blackBackgroundTexture->Height(); i++) {
               for (int j = 0; j < pitch; j += 4) {
                  *(unsigned int *)&lpData[i*pitch + j] = 0xFF000000;
               }
            }
         }
         m_blackBackgroundTexture->Unmap();
      }
   }
   Start();
}

void DeviceSource::EndScene() {
   Stop();
}

void DeviceSource::GlobalSourceLeaveScene() {
   if (!enteredSceneCount)
      return;
   if (--enteredSceneCount)
      return;
}

void DeviceSource::GlobalSourceEnterScene() {
   if (enteredSceneCount++)
      return;
}
void DeviceSource::KillThreads() {
   int numThreads = MAX(OSGetTotalCores() - 2, 1);
   for (int i = 0; i < numThreads&&hConvertThreads; i++) {
      if (hConvertThreads[i]) {
         convertData[i].bKillThread = true;
         SetEvent(convertData[i].hSignalConvert);

         OSTerminateThread(hConvertThreads[i], 10000);
         hConvertThreads[i] = NULL;
      }

      convertData[i].bKillThread = false;

      if (convertData[i].hSignalConvert) {
         CloseHandle(convertData[i].hSignalConvert);
         convertData[i].hSignalConvert = NULL;
      }

      if (convertData[i].hSignalComplete) {
         CloseHandle(convertData[i].hSignalComplete);
         convertData[i].hSignalComplete = NULL;
      }
   }
}

void DeviceSource::ChangeSize(bool bSucceeded, bool bForce) {
   if (!bForce && renderCX == newCX && renderCY == newCY)
      return;

   if (newCX == 0 || newCY == 0) {
      return;
   }
   renderCX = newCX;
   renderCY = newCY;

   switch (m_setting.colorType) {

      //yuv420   
   case DeviceOutputType_I420:
   case DeviceOutputType_YV12:
      lineSize = renderCX; //per plane
      break;
      //yuv422   
   case DeviceOutputType_YVYU:
   case DeviceOutputType_YUY2:
   case DeviceOutputType_UYVY:
   case DeviceOutputType_HDYC:
      lineSize = (renderCX * 2);
      break;
	  //RGB
   case DeviceOutputType_RGB://case DeviceOutputType_RGB:
	   lineSize = renderCX * 4;
	   break;
   default:break;
   }

   linePitch = lineSize;
   lineShift = 0;
   imageCX = renderCX;
   imageCY = renderCY;

   deinterlacer.imageCX = renderCX;
   deinterlacer.imageCY = renderCY;

   if (deinterlacer.doublesFramerate)
      deinterlacer.imageCX *= 2;
   //去交错类型
   switch (deinterlacer.type) {
   case DEINTERLACING_DISCARD:
      deinterlacer.imageCY = renderCY / 2;
      linePitch = lineSize * 2;
      renderCY /= 2;
      break;

   case DEINTERLACING_RETRO:
      deinterlacer.imageCY = renderCY / 2;
      if (deinterlacer.processor != DEINTERLACING_PROCESSOR_GPU) {
         lineSize *= 2;
         linePitch = lineSize;
         renderCY /= 2;
         renderCX *= 2;
      }
      break;

   case DEINTERLACING__DEBUG:
      deinterlacer.imageCX *= 2;
      deinterlacer.imageCY *= 2;
   case DEINTERLACING_BLEND2x:
      //case DEINTERLACING_MEAN2x:
   case DEINTERLACING_YADIF:
   case DEINTERLACING_YADIF2x:
      deinterlacer.needsPreviousFrame = true;
      break;
   }

   if (deinterlacer.type != DEINTERLACING_NONE
       && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU) {

      deinterlacer.vertexShader.reset
         (CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader")));


      deinterlacer.pixelShader = CreatePixelShaderFromFileAsync
         (ChooseDeinterlacingShader());


      deinterlacer.isReady = false;

   }

   KillThreads();

   int numThreads = MAX(OSGetTotalCores() - 2, 1);
   for (int i = 0; i < numThreads; i++) {
      convertData[i].width = lineSize;
      convertData[i].height = renderCY;
      convertData[i].sample = NULL;
      convertData[i].hSignalConvert = CreateEvent(NULL, FALSE, FALSE, NULL);
      convertData[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
      convertData[i].mLinePitch = linePitch;
      convertData[i].mLineShift = lineShift;

      if (i == 0)
         convertData[i].startY = 0;
      else
         convertData[i].startY = convertData[i - 1].endY;

      if (i == (numThreads - 1))
         convertData[i].endY = renderCY;
      else
         convertData[i].endY = ((renderCY / numThreads)*(i + 1)) & 0xFFFFFFFE;
   }

   if (m_setting.colorType == DeviceOutputType_YV12 || m_setting.colorType == DeviceOutputType_I420) {
      for (int i = 0; i < numThreads; i++)
         hConvertThreads[i] = OSCreateThread((XTHREAD)PackPlanarThread, convertData + i);
   }
   if (mBeautifyFilter) {
      mBeautifyFilter.reset();
      mBeautifyFilter = nullptr;
   }
   if (m_deviceSourceTexture) {
      delete m_deviceSourceTexture;
      m_deviceSourceTexture = NULL;
   }
   if (previousTexture) {
      delete previousTexture;
      previousTexture = NULL;
   }
   if (m_deviceSourceBeautyTexture) {
      delete m_deviceSourceBeautyTexture;
      m_deviceSourceBeautyTexture = nullptr;
   }
   if (previousBeautyTexture) {
      delete previousBeautyTexture;
      previousBeautyTexture = nullptr;
   }
   //-----------------------------------------------------
   // create the texture regardless, will just show up as red to indicate failure
   // 创建纹理空间
   BYTE *textureData = (BYTE*)Allocate(renderCX*renderCY * 4);
   for (int i = 0; i < renderCX*renderCY; i++) {
      *(unsigned int *)&textureData[i * 4] = 0x00000000;
   }
   
   //美颜需要的纹理。RG格式的需要的是BGR
   m_deviceSourceBeautyTexture = CreateTexture(renderCX, renderCY, GS_BGR,textureData, FALSE, FALSE);
   if (bSucceeded && deinterlacer.needsPreviousFrame)
      previousBeautyTexture = CreateTexture(renderCX, renderCY, GS_BGR, textureData, FALSE, FALSE);


   //---------------RGB------------------
   // DSHOW 的RGB 实际上是BGR
   if (m_setting.colorType == DeviceOutputType_RGB){
      //you may be confused, 
      //but when directshow outputs RGB, it's actually outputting BGR
      //RGB 格式的需要的是BGR
      m_deviceSourceTexture = CreateTexture(renderCX, renderCY,GS_BGR,textureData, FALSE, FALSE);

      if (bSucceeded && deinterlacer.needsPreviousFrame)
         previousTexture = CreateTexture(renderCX, renderCY, GS_BGR, textureData, FALSE, FALSE);
      if (bSucceeded && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
         deinterlacer.texture.reset(
         CreateRenderTarget(deinterlacer.imageCX,
         deinterlacer.imageCY, GS_BGRA, FALSE));
   }
   //---------------YUV-------------------
   else{
      //在YUV平面上工作，使用RGB问题代替
      //if we're working with planar YUV,
      // we can just use regular RGB textures instead
      //创建纹理
      m_deviceSourceTexture = CreateTexture(renderCX, renderCY,GS_RGB,textureData, FALSE, FALSE);
      if (bSucceeded && deinterlacer.needsPreviousFrame)
         previousTexture = CreateTexture
         (renderCX, renderCY, GS_RGB, textureData, FALSE, FALSE);

      if (bSucceeded && deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU)
         deinterlacer.texture.reset(
         CreateRenderTarget(deinterlacer.imageCX,
         deinterlacer.imageCY, GS_BGRA, FALSE));
   }

   //如果成功并且使用多线程转换
   if (bSucceeded && bUseThreadedConversion) {
      if (m_setting.colorType == DeviceOutputType_I420 || m_setting.colorType == DeviceOutputType_YV12) {

         LPBYTE lpData;
         if (m_deviceSourceTexture->Map(lpData, texturePitch))
            m_deviceSourceTexture->Unmap();
         else
            texturePitch = renderCX * 4;

         if (!lpImageBuffer || m_imageBufferSize != texturePitch * renderCY) {
            lpImageBuffer = (LPBYTE)Allocate(texturePitch*renderCY);
         }

         m_imageBufferSize = texturePitch*renderCY;
      }
   }

   Free(textureData);
   bFiltersLoaded = bSucceeded;
}

static DWORD STDCALL PackPlanarThread(ConvertData *data) {
   do {
      WaitForSingleObject(data->hSignalConvert, INFINITE);
      if (data->bKillThread) break;

      PackPlanar(data->output, data->input, data->width, data->height,
                 data->pitch,
                 data->startY, data->endY, data->mLinePitch, data->mLineShift);
      data->sample->Release();

      SetEvent(data->hSignalComplete);
   } while (!data->bKillThread);

   return 0;
}

void SetSize(DeviceColorType type, int width, int height) {
   if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight) {
      return;
   }

   bmi_.bmiHeader.biWidth = width;
   bmi_.bmiHeader.biHeight = height;
   bmi_.bmiHeader.biSizeImage = type == DeviceOutputType_RGB ? (width * height * 3) : (width * height * 4);
   bmi_.bmiHeader.biBitCount = DeviceOutputType_RGB ? 24 : 32;
   bmi_.bmiHeader.biPlanes = 1;
   bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   //         ZeroMemory(&desBitmapInfo, sizeof(BITMAPINFO));
   //         bmi_.bmiHeader.biBitCount = 32;
   //         bmi_.bmiHeader.biCompression = 0;
   //         bmi_.bmiHeader.biHeight = desHeight;
   //         bmi_.bmiHeader.biPlanes = 1;
   //         bmi_.bmiHeader.biSizeImage = desWidth*desHeight * 4;
   //         bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   //         bmi_.bmiHeader.biWidth = desWidth;
}

//rgb到bgrx的转换
static void convert_rgb_to_bgr(uint8_t* src, uint8_t* dest, int width)
{
   int x;
   for (x = 0; x < width; x++) {
      dest[0] = src[2];
      dest[1] = src[1];
      dest[2] = src[0];
      dest[3] = src[3];
      src += 4;
      dest += 4;
   }
}

typedef struct RGBPixel {
   unsigned char A;
   unsigned char R;
   unsigned char G;
   unsigned char B;
} RGBPixel;

typedef struct YUVPixel {
   int Y;
   int U;
   int V;
} YUVPixel;

unsigned char clip_sh_0_255(int in) {
   unsigned char out;
   if (in >= 0 && in <= 255) {
      out = (unsigned char)in;
   }
   else if (in > 255) {
      out = (unsigned char)255;
   }
   else {
      out = 0;
   }
   return out;
}

void YUV444_TO_RGB(const YUVPixel *yuv, uint8_t *rgb) {
   *rgb = clip_sh_0_255((298 * (yuv->Y - 16) + 516 * (yuv->U - 128) + 128) >> 8); //b
   rgb++;
   *rgb = clip_sh_0_255((298 * (yuv->Y - 16) - 208 * (yuv->V - 128) - 100 * (yuv->U - 128) + 128) >> 8);   //g
   rgb++;
   *rgb = clip_sh_0_255((298 * (yuv->Y - 16) + 409 * (yuv->V - 128) + 128) >> 8);//r
   rgb++;
   *rgb = 255;
   rgb++;
}

void DeviceSource::HDBeautyRender(DeviceColorType type, LPBYTE bytes, int width, int height) {
   const uint8_t* image = bytes;
   if (bytes != NULL) {
      if (DeviceOutputType_RGB == type) {
         libyuv::ARGBToRGB24(bytes, width * 4, mBeautyRgb24.get(), width * 3, width, height);
         mBeautifyFilter->loadFrame(mBeautyRgb24, width * height * 3, width, height, m_LastSyncTime);

      }
      else if (DeviceOutputType_I420 == type || DeviceOutputType_YV12 == type) {
         //if (mPlayerAudioFile == nullptr) {
         //   mPlayerAudioFile = fopen("I:\\mPlayerAudioFile.yuv", "wb");
         //}
         //if (mPlayerAudioFile) {
         //   fwrite(bytes, sizeof(uint8_t), width * height * 3 / 2, mPlayerAudioFile);
         //}
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + (int)(width * height * 0.25);
         if (DeviceOutputType_I420 == type) {
            libyuv::I420ToRGB24(bytes, width, src_u, width >> 1, src_v, width >> 1, mBeautyRgb24.get(), width * 3, width, height);
         }
         else {
            libyuv::I420ToRGB24(bytes, width, src_v, width >> 1, src_u, width >> 1, mBeautyRgb24.get(), width * 3, width, height);
         }
         mBeautifyFilter->loadFrame(mBeautyRgb24, width * height * 3, width, height, m_LastSyncTime);

      }
      else if (type == DeviceOutputType_YVYU || type == DeviceOutputType_YUY2) {
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         libyuv::YUY2ToARGB(bytes, width * 2, mCaptureData.get(), width * 4, width, height);
         libyuv::ARGBToRGB24(mCaptureData.get(), width * 4, mBeautyRgb24.get(), width* 3, width, height);
         mBeautifyFilter->loadFrame(mBeautyRgb24, width * height * 3, width, height, m_LastSyncTime);
      }
      else if (type == DeviceOutputType_UYVY || type == DeviceOutputType_HDYC) {
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         libyuv::UYVYToARGB(bytes, width * 2, mCaptureData.get(), width * 4, width, height);
         libyuv::ARGBToRGB24(mCaptureData.get(), width * 4, mBeautyRgb24.get(), width * 3, width, height);
         mBeautifyFilter->loadFrame(mBeautyRgb24, width * height * 3, width, height, m_LastSyncTime);
      }
   }
}

void DeviceSource::HDCRender(DeviceColorType type, HWND desHwnd, LPBYTE bytes, int cx, int cy) {
   if (desHwnd == NULL) {
      return;
   }

   SetSize(type, cx, cy);
   HWND wnd_ = desHwnd;
   RECT rc;
   ::GetClientRect(wnd_, &rc);

   BITMAPINFO& bmi = bmi_;
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

      int x = (logical_area.x >> 1) - (width >> 1);
      int y = (logical_area.y >> 1) - (height >> 1);
      if (DeviceOutputType_RGB == type) {
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      else if (DeviceOutputType_BGRX == type) {
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
      }
      else if (DeviceOutputType_I420 == type || DeviceOutputType_YV12 == type) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         if (DeviceOutputType_I420 == type) {
            libyuv::I420ToARGB(bytes, width, src_u, width >> 1, src_v, width >> 1, rgb, width * 4, width, height);
         }
         else {
            libyuv::I420ToARGB(bytes, width, src_v, width >> 1,src_u, width >> 1, rgb, width * 4, width, height);
         }
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      else if (type == DeviceOutputType_YVYU || type == DeviceOutputType_YUY2) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::YUY2ToARGB(bytes, width * 2, rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      else if (type == DeviceOutputType_UYVY || type == DeviceOutputType_HDYC) {
         uint8_t* rgb = new uint8_t[width * height * 4];
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * (height >> 2);
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         libyuv::UYVYToARGB(bytes, width * 2, rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete[]rgb;
      }
      else if (DeviceOutputType_YUV444 == type) {
         //if (mPlayerAudioFile == nullptr) {
         //   mPlayerAudioFile = fopen("I:\\mPlayerAudioFile.yuv", "wb");
         //}
         //if (mPlayerAudioFile) {
         //    fwrite(bytes, sizeof(uint8_t), width * height * 3, mPlayerAudioFile);
         //}
         uint8_t* rgb = new uint8_t[width * height * 4];      
         uint8_t* src_u = bytes + width * height;
         uint8_t* src_v = src_u + width * height;
         YUVPixel yuv_pixel;
         RGBPixel rgb_pixel;
         uint8_t *rgb_ptr = rgb;
         for (int j = 0; j < height; j++){
            for (int i = 0; i < width; i++){
               //yuv_pixel.Y = bytes[width * j + i];
               //yuv_pixel.U = bytes[width * j + i + width * height];
               //yuv_pixel.V = bytes[width * j + i + width * height * 2];

               yuv_pixel.Y = bytes[width * j * 3 + i * 3 ];
               yuv_pixel.U = bytes[width * j * 3 + i * 3 + 1];
               yuv_pixel.V = bytes[width * j * 3 + i * 3 + 2];

               YUV444_TO_RGB(&yuv_pixel, rgb_ptr);
               rgb_ptr = rgb_ptr + 4;
            }
         }
         bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
         //libyuv::J444ToARGB(bytes, width, src_u, width , src_v, width , rgb, width * 4, width, height);
         StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, rgb, &bmi, DIB_RGB_COLORS, SRCCOPY);
         BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);
         delete []rgb;
      }
      ::SelectObject(dc_mem, bmp_old);
      ::DeleteObject(bmp_mem);
      ::DeleteDC(dc_mem);
      ::ReleaseDC(wnd_, mWindowDC);
      //delete []dst_argb;
   }


   //return;
   //char dbg[256] = { 0 };
   //HDC hdc = GetDC(desHwnd);
   //if (!hdc) {
   //   return;
   //}

   //HDC mdc = CreateCompatibleDC(hdc);
   //if (!mdc) {
   //   DeleteDC(hdc);
   //   return;
   //}

   //HBITMAP hbmp = CreateCompatibleBitmap(hdc, cx, cy);
   //if (!hbmp) {
   //   DeleteDC(mdc);
   //   DeleteDC(hdc);
   //   return;
   //}

   //RECT desRect;
   //if (!GetWindowRect(desHwnd, &desRect)) {
   //   DeleteObject(hbmp);
   //   DeleteDC(mdc);
   //   DeleteDC(hdc);
   //   return;
   //}

   //int desWidth = desRect.right - desRect.left;
   //int desHeight = desRect.bottom - desRect.top;


   //if (cx > 0 && cy > 0 && desWidth > 0 && desHeight > 0) {

   //   BITMAPINFO binfo;
   //   ZeroMemory(&binfo, sizeof(BITMAPINFO));
   //   binfo.bmiHeader.biBitCount = 32;
   //   binfo.bmiHeader.biCompression = 0;
   //   binfo.bmiHeader.biHeight = cy;
   //   binfo.bmiHeader.biPlanes = 1;
   //   binfo.bmiHeader.biSizeImage = 0;
   //   binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   //   binfo.bmiHeader.biWidth = cx;

   //   SetDIBits(hdc, hbmp, 0, cy, bytes, &binfo, DIB_RGB_COLORS);
   //   SelectObject(mdc, hbmp);


   //   HDC mhdc = CreateCompatibleDC(hdc);
   //   if (mhdc) {
   //      HBITMAP b2map = CreateCompatibleBitmap(hdc, desWidth, desHeight);
   //      if (b2map) {
   //         SelectObject(mhdc, b2map);
   //         StretchBlt(hdc, 0, 0, desWidth, desHeight, mdc, 0, 0, cx, cy, SRCCOPY);
   //         BITMAPINFO desBitmapInfo;
   //         ZeroMemory(&desBitmapInfo, sizeof(BITMAPINFO));
   //         desBitmapInfo.bmiHeader.biBitCount = 32;
   //         desBitmapInfo.bmiHeader.biCompression = 0;
   //         desBitmapInfo.bmiHeader.biHeight = desHeight;
   //         desBitmapInfo.bmiHeader.biPlanes = 1;
   //         desBitmapInfo.bmiHeader.biSizeImage = desWidth*desHeight * 4;
   //         desBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   //         desBitmapInfo.bmiHeader.biWidth = desWidth;

   //         unsigned char *data = new unsigned char[desWidth*desHeight * 4];
   //         if (data) {
   //            memset(data, 0, desWidth*desHeight * 4);
   //            int ret;
   //            ret = GetDIBits(mhdc, b2map, 0, desHeight, data, &desBitmapInfo, DIB_RGB_COLORS);

   //            for (int y = 0; y < desHeight; y++) {
   //               for (int x = 0; x < desWidth; x++) {
   //                  unsigned char a = data[y*desWidth * 4 + x * 4 + 0];
   //                  unsigned char r = data[y*desWidth * 4 + x * 4 + 1];
   //                  unsigned char g = data[y*desWidth * 4 + x * 4 + 2];
   //                  unsigned char b = data[y*desWidth * 4 + x * 4 + 3];
   //                  unsigned int rgba = b << 24 | g << 16 | r << 8 | a;
   //                  *(unsigned int *)&data[(desHeight - y)*desWidth * 4 + x * 4 + 0] = rgba;
   //               }
   //            }


   //            ret = SetDIBits(mhdc, b2map, 0, desHeight, data, &desBitmapInfo, DIB_RGB_COLORS);
   //            //sprintf(dbg,"SetDIBits %d\n",ret);OutputDebugStringA(dbg);

   //            static int index = 0;
   //            index++;
   //            if (index == 30) {
   //               //SaveBitmapToFile(b2map, L"D:\\a.bmp");
   //            }



   //            SelectObject(mhdc, b2map);
   //            // BitBlt(hdc,0,0,desWidth,desHeight,mhdc,0,0,SRCCOPY);


   //            delete data;
   //            data = NULL;
   //         }
   //         DeleteObject(b2map);
   //      }
   //      DeleteDC(mhdc);
   //   }
   //}

   //DeleteObject(hbmp);
   //DeleteDC(mdc);
   //DeleteDC(hdc);

}


void DeviceSource::OnProcessVideoFrame(SampleData* lastSample, bool bForceRGB24 /*= false*/) {
   //获得CPU核数
   int numThreads = MAX(OSGetTotalCores() - 2, 1);
   if (lastSample) {
      //交换纹理
      if (previousTexture) {
         if (bForceRGB24) {
            Texture *tmp = m_deviceSourceBeautyTexture;
            m_deviceSourceBeautyTexture = previousBeautyTexture;
            previousBeautyTexture = tmp;
         }
         else {
            Texture *tmp = m_deviceSourceTexture;
            m_deviceSourceTexture = previousTexture;
            previousTexture = tmp;
         }
      }

      //当前场
      deinterlacer.curField =
         //去交错处理器为GPU
         deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU ?
         //
         false :
         //TFF 顶场 \ BFF  底场

         //场序为BFF
         (deinterlacer.fieldOrder == FIELD_ORDER_BFF);

      deinterlacer.bNewFrame = true;

      //-------------------色彩空间转换------------------
      //之后将图像复制到texture中
      //色彩空间为RGB
     //Log(L"DeviceSource::Preprocess(): 1036 DeviceOutputType: %d", m_setting.colorType);

       //if (m_setting.colorType == DeviceOutputType_RGB24) {
      if (bForceRGB24 && m_deviceSourceBeautyTexture && lastSample != NULL && lastSample->lpData != NULL) {
         ChangeSize();
         //ProcessImageHance(lastSample, m_setting.colorType == DeviceOutputType_I420 ? PixelFmt_I420 : PixelFmt_YV12);
         if (m_deviceSourceBeautyTexture && lastSample && lastSample->lpData) {
            double dataVerify = (1.0*lastSample->dataLength) / (lastSample->cx*lastSample->cy);
            if (4.0 == dataVerify) {
               if (m_bIsPreView && lastSample) {
                  HDCRender(DeviceOutputType_BGRX, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
               }
               m_deviceSourceBeautyTexture->SetImage(lastSample->lpData, GS_IMAGEFORMAT_BGRX, bForceRGB24 ? lastSample->cx * 4 : linePitch);
            }
            else if (3.0 == dataVerify) {
               if (m_bIsPreView && lastSample) {
                  HDCRender(DeviceOutputType_RGB, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
               }
               m_deviceSourceBeautyTexture->SetImage(lastSample->lpData, GS_IMAGEFORMAT_BGR, linePitch);
            }
         }
         bReadyToDraw = true;
      }
      else if (m_setting.colorType == DeviceOutputType_RGB) {
         if (m_deviceSourceTexture && lastSample != NULL && lastSample->lpData != NULL) {
            ChangeSize();
            //ProcessImageHance(lastSample, m_setting.colorType == DeviceOutputType_I420 ? PixelFmt_I420 : PixelFmt_YV12);
            if (m_deviceSourceTexture && lastSample && lastSample->lpData) {
               double dataVerify = (1.0*lastSample->dataLength) / (lastSample->cx*lastSample->cy);
               if (4.0 == dataVerify) {
                  if (m_bIsPreView && lastSample) {
                     HDCRender(DeviceOutputType_BGRX, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
                  }
                  m_deviceSourceTexture->SetImage(lastSample->lpData, GS_IMAGEFORMAT_BGRX, bForceRGB24 ? lastSample->cx * 4 : linePitch);
               }
               else if (3.0 == dataVerify) {
                  if (m_bIsPreView && lastSample) {
                     HDCRender(DeviceOutputType_RGB, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
                  }
                  m_deviceSourceTexture->SetImage(lastSample->lpData, GS_IMAGEFORMAT_BGR, linePitch);
               }
            }
            bReadyToDraw = true;
         }
      }
      //色彩空间为YUV420
      else if (m_setting.colorType == DeviceOutputType_I420 || m_setting.colorType == DeviceOutputType_YV12) {
         //ProcessImageHance(lastSample, m_setting.colorType == DeviceOutputType_I420 ? PixelFmt_I420 : PixelFmt_YV12);
         if (m_bIsPreView && lastSample) {
            HDCRender(m_setting.colorType, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
         }
         //多线程转换
         if (bUseThreadedConversion) {
            if (!bFirstFrame) {
               List<HANDLE> events;
               for (int i = 0; i < numThreads; i++) {
                  ConvertData data = convertData[i];
                  events << data.hSignalComplete;
               }
               WaitForMultipleObjects(numThreads, events.Array(), TRUE, INFINITE);
               if (m_deviceSourceTexture) {
                  m_deviceSourceTexture->SetImage(lpImageBuffer, GS_IMAGEFORMAT_RGBX, texturePitch);
                  bReadyToDraw = true;
               }
               else {
                  bReadyToDraw = false;
               }
            }
            else
               bFirstFrame = false;

            ChangeSize();

            for (int i = 0; i < numThreads; i++)
               lastSample->AddRef();

            for (int i = 0; i < numThreads; i++) {
               convertData[i].input = lastSample->lpData;
               convertData[i].sample = lastSample;
               convertData[i].pitch = texturePitch;
               convertData[i].output = lpImageBuffer;
               convertData[i].mLinePitch = linePitch;
               convertData[i].mLineShift = lineShift;
               SetEvent(convertData[i].hSignalConvert);
            }
         }
         else {
            LPBYTE lpData;
            UINT pitch;
            ChangeSize();
            if (m_deviceSourceTexture) {
               if (m_deviceSourceTexture->Map(lpData, pitch)) {
                  //不科学 。lastSample->lpData转成YUV444
                  PackPlanar(lpData, lastSample->lpData, renderCX, renderCY, pitch, 0, renderCY, linePitch, lineShift);
                  m_deviceSourceTexture->Unmap();
               }
               bReadyToDraw = true;
            }
            else {
               bReadyToDraw = false;
            }
         }
      }
      else if (m_setting.colorType == DeviceOutputType_YVYU || m_setting.colorType == DeviceOutputType_YUY2) {
         LPBYTE lpData;
         UINT pitch;

         ChangeSize();
         if (m_deviceSourceTexture) {
            //ProcessImageHance(lastSample, m_setting.colorType == DeviceOutputType_YVYU ? PixelFmt_YVYU : PixelFmt_YUY2);
            if (m_bIsPreView && lastSample) {
               HDCRender(m_setting.colorType, m_privew_renderHwnd, lastSample->lpData, renderCX, renderCY);
            }
            if (m_deviceSourceTexture->Map(lpData, pitch)) {
               //YUV444
               Convert422To444(lpData, lastSample->lpData, pitch, true);
               m_deviceSourceTexture->Unmap();
            }
            bReadyToDraw = true;
         }
         else {
            bReadyToDraw = false;
         }
      }
      else if (m_setting.colorType == DeviceOutputType_UYVY || m_setting.colorType == DeviceOutputType_HDYC) {
         LPBYTE lpData;
         UINT pitch;
         ChangeSize();
         //ProcessImageHance(lastSample, m_setting.colorType == DeviceOutputType_UYVY ? PixelFmt_UYVY : PixelFmt_HDYC);
         if (m_deviceSourceTexture) {
            if (m_bIsPreView && lastSample) {
               HDCRender(m_setting.colorType, m_privew_renderHwnd, lpData, renderCX, renderCY);
            }
            if (m_deviceSourceTexture->Map(lpData, pitch)) {
               Convert422To444(lpData, lastSample->lpData, pitch, false);
               m_deviceSourceTexture->Unmap();
            }
            bReadyToDraw = true;
         }
         else {
            bReadyToDraw = false;
         }
      }

      lastSample->Release();

      //------------------------色彩空间转换完毕------------------------------
      //-------------------------------将纹理绘制到去交错纹理上-------------------
      //已经将图像数据写入到纹理中
      if (
         //准备好绘制
         bReadyToDraw &&
         //去交错类型不为空
         deinterlacer.type != DEINTERLACING_NONE &&
         //去交错处理为GPU处理
         deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU &&
         //去交错纹理不为空
         deinterlacer.texture.get() &&
         //去交错像素着色器不为空
         deinterlacer.pixelShader.Shader()) {

         //-------------------------设置渲染目标为去交错的目标---------------------------
         SetRenderTarget(deinterlacer.texture.get());

         //获得当前的顶点着色器
         Shader *oldVertShader = GetCurrentVertexShader();
         //加载去交错内的定点着色器
         LoadVertexShader(deinterlacer.vertexShader.get());
         //获得当前的像素着色器
         Shader *oldShader = GetCurrentPixelShader();
         //加载去交错内的像素着色器
         LoadPixelShader(deinterlacer.pixelShader.Shader());

         Ortho(0.0f, float(deinterlacer.imageCX), float(deinterlacer.imageCY), 0.0f, -100.0f, 100.0f);

         SetViewport(0.0f, 0.0f, float(deinterlacer.imageCX), float(deinterlacer.imageCY));

         if (bForceRGB24) {
            if (previousBeautyTexture) {
               LoadTexture(previousBeautyTexture, 1);
            }
         }
         else{
            if (previousTexture) {
               LoadTexture(previousTexture, 1);
            }
         }

         //-------------------将texture绘制到去交错纹理上---------------------
         if (bForceRGB24 && m_deviceSourceBeautyTexture) {
            DrawSpriteEx(m_deviceSourceBeautyTexture, 0xFFFFFFFF, 0.0f, 0.0f,
               float(deinterlacer.imageCX),
               float(deinterlacer.imageCY),
               0.0f, 0.0f, 1.0f, 1.0f);
         }
         else if (m_deviceSourceTexture) {
            DrawSpriteEx(m_deviceSourceTexture, 0xFFFFFFFF, 0.0f, 0.0f,
               float(deinterlacer.imageCX),
               float(deinterlacer.imageCY),
               0.0f, 0.0f, 1.0f, 1.0f);
         }
         if (bForceRGB24) {
            if (previousBeautyTexture) {
               LoadTexture(nullptr, 1);
            }
         }
         else {
            if (previousTexture) {
               LoadTexture(nullptr, 1);
            }
         }

         LoadPixelShader(oldShader);
         LoadVertexShader(oldVertShader);
         deinterlacer.isReady = true;
      }
      if (m_device) {
         m_device->ReleaseVideoBuffer();
      }
   }

   DShowVideoManagerLeaveMutex();
}

void DeviceSource::SetBeautyPreviewHandle(HWND m_PriviewRenderHwnd, int hide_preview) {
   m_beauty_privew_renderHwnd = m_PriviewRenderHwnd;
   bHideBeautyPreviewDraw = hide_preview == 1 ? false :true;
}

//渲染之前的预处理
bool DeviceSource::Preprocess() {
   //如果没有捕获，退出
   if (!bCapturing)
      return true;


   //交换样本数据指针
   SampleData *lastSample = NULL;
   DShowVideoManagerEnterMutex();
   if (!m_device || !IsDShowVideoFilterExist(this)) {
      m_bDeviceExist = false;
      DShowVideoManagerLeaveMutex();
      return true;
   }

   unsigned char *buffer = NULL;
   unsigned long long  bufferSize = 0;
   unsigned long long  timestamp = 0;
   if (m_device->GetNextVideoBuffer((void **)&buffer, &bufferSize, &timestamp, m_setting)) {
      if (buffer != NULL) {
         lastSample = new SampleData();
         lastSample->lpData = (LPBYTE)malloc(bufferSize);
         memcpy(lastSample->lpData, buffer, bufferSize);
         lastSample->dataLength = bufferSize;
         newCX = lastSample->cx = m_setting.cx;
         newCY = lastSample->cy = m_setting.cy;
         lastSample->timestamp = timestamp;
         m_LastSyncTime = GetQPCTimeMS();
      }
      m_device->ReleaseVideoBuffer();
   }
   if ((mLastHeight != newCY || mLastWidth != newCX) || (mBeautyLevel > 0 && mBeautifyFilter == nullptr)) {
      if (mBeautifyFilter) {
         mBeautifyFilter.reset();
         mBeautifyFilter = nullptr;
      }
      mLastWidth = newCX;
      mLastHeight = newCY;
      CreateBeautyFilter();
      if (mCaptureData == nullptr) {
         mCaptureData.reset();
      }
      if (mBeautyRgb24 == nullptr) {
         mBeautyRgb24.reset();
      }
      if (mBeautyCallbackData == nullptr) {
         mBeautyCallbackData.reset();
      }
      if (mRenderData == nullptr) {
         mRenderData.reset();
      }
      mRenderData.reset(new unsigned char[newCX * newCY * 4]);
      mBeautyCallbackData.reset(new unsigned char[newCX * newCY * 4]);
      mCaptureData.reset(new unsigned char[newCX * newCY * 4]);
      mBeautyRgb24.reset(new unsigned char[newCX * newCY * 4]);
   }

   if (mBeautyLevel > 0 && mBeautifyFilter && lastSample && newCX <= MAX_SUPPORT_W && newCY <= MAX_SUPPORT_H) {
      HDBeautyRender(m_setting.colorType, lastSample->lpData, newCX, newCY);
      lastSample->Release();
      DShowVideoManagerLeaveMutex();
   } 
   else {
      //if (bHideBeautyPreviewDraw && m_beauty_privew_renderHwnd) {
      //   HDCRender(m_setting.colorType, m_beauty_privew_renderHwnd, lastSample->lpData, newCX, newCY);
      //}
      OnProcessVideoFrame(lastSample);
   }
   return true;
}

void DeviceSource::ProcessImageHance(SampleData* lastSample, int data_format) {
   return;
   if (isEnableImageSharpening) {
      if (mLastWidth != lastSample->cx || mLastHeight != lastSample->cy) {
         if (m_ImageEnhance) {
            delete m_ImageEnhance;
            m_ImageEnhance = nullptr;
         }
         m_ImageEnhance = new ImageEnhance;
         VideoFilterParam param;
         param.width = lastSample->cx;
         param.height = lastSample->cy;
         if (m_ImageEnhance) {
            m_ImageEnhance->Init(&param);
         }
         mLastWidth = lastSample->cx;
         mLastWidth = lastSample->cy;
      }
      if (m_ImageEnhance) {
         m_ImageEnhance->Denoise(lastSample->lpData, lastSample->lpData, (PixelFmt)data_format);
         m_ImageEnhance->Brighter(lastSample->lpData, lastSample->lpData, (PixelFmt)data_format);
      }
   }
}

int DeviceSource::HasRendData() {
   return m_bIsHasNoData ? 0 : 1;
}

void DeviceSource::Render(const Vect2 &pos, const Vect2 &size, SceneRenderType renderType) {
   if (m_blackBackgroundTexture && size.x > 0 && size.y > 0 && SceneRenderType::SceneRenderType_Preview == renderType) {
      float x, x2;
      x = pos.x;
      x2 = x + size.x;

      float y = pos.y,
         y2 = y + size.y;

      DrawSprite(m_blackBackgroundTexture, 0xFFFFFFFF, x, y, x2, y2);
   }
   QWORD currentTime = GetQPCTimeMS();
   if (m_LastSyncTime == 0) {
      m_LastSyncTime = currentTime;
   }

   bool isHasNoData = (currentTime - m_LastSyncTime) > 3000;
   m_bIsHasNoData = isHasNoData;

   if (SceneRenderType::SceneRenderType_Desktop == renderType && m_renderHWND != nullptr && IsWindowVisible(m_renderHWND)) {
      QWORD currentTime = GetQPCTimeMS();
      if (currentTime - m_LastTickTime > 100) {
         m_LastTickTime = currentTime;
         Shader *currentPixelShader = NULL;
         //颜色转换着色器
         if (colorConvertShader) {
            currentPixelShader = GetCurrentPixelShader();
            //加载颜色转换着色器
            LoadPixelShader(colorConvertShader);
            //向着色器设置伽马值
            colorConvertShader->SetFloat
               (colorConvertShader->GetParameterByName(TEXT("gamma")), 1.0f);

            //
            float mat[16];
            bool actuallyUse709 = !!m_setting.use709;

            if (actuallyUse709)
               memcpy(mat, yuvToRGB709[0], sizeof(float)* 16);
            else
               memcpy(mat, yuvToRGB601[0], sizeof(float)* 16);

            //设置YUV MAT
            colorConvertShader->SetValue(
               colorConvertShader->GetParameterByName(TEXT("yuvMat")),
               mat, sizeof(float)* 16);
         }

         bool bFlip = false;
         if (m_setting.colorType != DeviceOutputType_RGB)
		 //if (m_setting.colorType>DeviceOutputType_ARGB32 /*&& m_setting.colorType<DeviceOutputType_Y41P*/)
            bFlip = !bFlip;

         DWORD opacity255 = 0xFFFFFF;

         Texture *tex =
            //去交错使用GPU
            (deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU
            //去交错纹理非空
            && deinterlacer.texture.get()) ?
            //去交错纹理
            deinterlacer.texture.get() :
            //当前的纹理
            /*mBeautyLevel > 0 ? m_deviceSourceBeautyTexture :*/ m_deviceSourceTexture;

         //if (!m_bIsPreView) {
         //   OBSGraphics_Present();
         //   OBSGraphics_Flush();
         //}
         OBSGraphics_Present();
         OBSGraphics_Flush();

         OBSAPI_EnterSceneMutex();
         if (m_renderHWND) {
            if (m_renderView != NULL) {
               OBSGraphics_DestoryRenderTargetView(m_renderView);
               m_renderView = NULL;
            }
            m_renderView = OBSGraphics_CreateRenderTargetView(m_renderHWND, 0, 0);
            OBSGraphics_SetRenderTargetView(m_renderView);
         }
         OBSAPI_LeaveSceneMutex();

         //if (!m_bIsPreView) {
         //   EnableBlending(FALSE);
         //   ClearColorBuffer(0x000000);
         //}
         EnableBlending(FALSE);
         ClearColorBuffer(0x000000);

         RECT rect;
         if (m_renderHWND) {
            GetClientRect(m_renderHWND, &rect);
         }
         else {
            rect.bottom = 0;
            rect.left = 0;
            rect.right = 0;
            rect.top = 0;
         }
         float w = rect.right - rect.left;
         float h = rect.bottom - rect.top;
         float k = w / h;

         float w2 = renderCX;
         float h2 = renderCY;
         float k2 = w2 / h2;

         Ortho(0.0f, w, h, 0.0f, -100.0f, 100.0f);
         SetViewport(0.0f, 0.0f, w, h);
         float x, x2, y, y2;

         if (k > k2) {
            x = 0;
            y = 0 - (w / k2 - h) / 2;
            x2 = x + w;
            y2 = y + h + (w / k2 - h);
         } else {
            y = 0;
            x = 0 - (h*k2 - w) / 2;
            y2 = y + h;
            x2 = x + w + (h*k2 - w);
         }

         if (!bFlip) {
            float tmp_y = y2;
            y2 = y;
            y = tmp_y;
         }

         //去交错双倍帧率
         if (deinterlacer.doublesFramerate) {
            //丢弃当场
            if (!deinterlacer.curField)
               DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, 0.f, 0.0f, .5f, 1.f);
            //其他
            else
               DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, .5f, 0.0f, 1.f, 1.f);
         }
         //绘制整个纹理
         else
            DrawSprite(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2);
         if (currentPixelShader) {
            LoadPixelShader(currentPixelShader);
         }
         OBSGraphics_Present();
         OBSGraphics_Flush();
         OBSGraphics_SetRenderTargetView(NULL);
      }
   }

   if (tipTexture && !isHasNoData && SceneRenderType::SceneRenderType_Preview == renderType) {
      if (size.x != 0 && size.y != 0 && tipTexture->Width() != 0 && tipTexture->Height() != 0) {
         int x, y, w, h;
         if (tipTexture->Width() <= size.x&&tipTexture->Height() <= size.y) {
            w = tipTexture->Width();
            h = tipTexture->Height();
            x = int(size.x - w);
            x /= 2;
            y = size.y - h;
            y /= 2;

            x += pos.x;
            y += pos.y;
         } else {
            float ktip = tipTexture->Width();
            ktip /= tipTexture->Height();
            float ksize = size.x;
            ksize /= size.y;
            if (ktip > ksize) {
               w = size.x;
               h = w / ktip;
               x = pos.x;
               y = pos.y + (size.y - h) / 2;
            } else {
               h = size.y;
               w = h*ktip;
               y = pos.y;
               x = pos.x + (size.x - w) / 2;
            }
         }

         DrawSprite(tipTexture, 0xFFFFFFFF, x, y, x + w, y + h);
      }
   }

   //-------------------------渲染---------------------------------
   //纹理不为空
   if (m_deviceSourceTexture
       //准备好去绘制
       && bReadyToDraw
       //去交错完毕
       && deinterlacer.isReady) {

      //获得当前的像素着色器
      Shader *oldShader = GetCurrentPixelShader();
      SamplerState *sampler = NULL;

      //颜色转换着色器
      if (colorConvertShader) {
         //加载颜色转换着色器
         LoadPixelShader(colorConvertShader);
         //向着色器设置伽马值
         colorConvertShader->SetFloat(colorConvertShader->GetParameterByName(TEXT("gamma")), 1.0f);
         //
         float mat[16];
         bool actuallyUse709 = !!m_setting.use709;

         if (actuallyUse709)
            memcpy(mat, yuvToRGB709[0], sizeof(float)* 16);
         else
            memcpy(mat, yuvToRGB601[0], sizeof(float)* 16);

         //设置YUV MAT
         colorConvertShader->SetValue(colorConvertShader->GetParameterByName(TEXT("yuvMat")),mat, sizeof(float)* 16);
      }
      bool bFlip = false;
      if (m_setting.colorType != DeviceOutputType_RGB) {
         bFlip = !bFlip;
      }
	  //if (m_setting.colorType > DeviceOutputType_ARGB32 /*&& m_setting.colorType < DeviceOutputType_Y41P*/)

      //起始和结束坐标
      float x, x2;
      x = pos.x;
      x2 = x + size.x;

      float y = pos.y,
         y2 = y + size.y;
      if (!bFlip) {
         y2 = pos.y;
         y = y2 + size.y;
      }


      DWORD opacity255 = 0xFFFFFF;

      Texture *tex =
         //去交错使用GPU
         (deinterlacer.processor == DEINTERLACING_PROCESSOR_GPU
         //去交错纹理非空
         && deinterlacer.texture.get()) ?
         //去交错纹理
         deinterlacer.texture.get() :
         //当前的纹理
         /*mBeautyLevel > 0 ? m_deviceSourceBeautyTexture :*/ m_deviceSourceTexture;

      //去交错双倍帧率
      if (deinterlacer.doublesFramerate) {
         //丢弃当场
         if (!deinterlacer.curField)
            DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, 0.f, 0.0f, .5f, 1.f);
         //其他
         else
            DrawSpriteEx(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2, .5f, 0.0f, 1.f, 1.f);
      }
      //绘制整个纹理
      else
         DrawSprite(tex, (opacity255 << 24) | 0xFFFFFF, x, y, x2, y2);

      //去交错需要新的一帧
      if (deinterlacer.bNewFrame) {
         deinterlacer.curField = !deinterlacer.curField;
         deinterlacer.bNewFrame = false;
         //prevent switching from the second field to the first field
      }

      if (colorConvertShader)
         LoadPixelShader(oldShader);
   }

   if (!m_bDeviceExist&&m_disconnectTexture&&SceneRenderType::SceneRenderType_Preview == renderType) {
      //起始和结束坐标
      float x, x2;
      x = pos.x;
      x2 = x + size.x;

      float y = pos.y,
         y2 = y + size.y;
      bool bFlip = false;
      if (m_setting.colorType != DeviceOutputType_RGB)
	  //if (m_setting.colorType > DeviceOutputType_ARGB32 /*&& m_setting.colorType < DeviceOutputType_Y41P*/)
         bFlip = !bFlip;


      if (!bFlip) {
         y2 = pos.y;
         y = y2 + size.y;
      }


      int tx;
      int ty;
      int tw;
      int th;


      int cx = x;
      int cy = y;
      int cw = (x2 - x) / 2;
      int ch = (y2 - y) / 2;
      int ow = size.x;
      int oh = size.y;

      if (ow < m_disconnectTexture->Width() || oh<m_disconnectTexture->Height()) {
         float texK = cw;
         texK /= ch;
         float tipK = m_disconnectTexture->Width();
         tipK /= m_disconnectTexture->Height();
         if (texK>tipK) {
            th = ch;
            tw = ch*tipK;
         } else {
            tw = cw;
            if (tipK != 0) {
               th = tw / tipK;
            } else {
               th = m_disconnectTexture->Height();
            }
         }

         tx = (cw * 2 - tw) / 2 + cx;
         ty = (ch * 2 - th) / 2 + cy;

      } else {
         float sizeK = size.x;
         sizeK /= ow;
         sizeK *= m_disconnectTexture->Width();
         tw = sizeK;


         sizeK = size.y;
         sizeK /= oh;
         sizeK *= m_disconnectTexture->Height();
         th = sizeK;

         tx = (size.x - tw) / 2 + pos.x;
         ty = (size.y - th) / 2 + pos.y;
      }

      DrawSprite(m_disconnectTexture, 0xFFFFFFFF, tx, ty, tw + tx, th + ty);
   } else if (isHasNoData&&SceneRenderType::SceneRenderType_Preview == renderType) {
      int imgx = 0, imgy = 0, imgw = 0, imgh = 0;
      if (size.x != 0 && size.y != 0 && m_failedTexture && m_failedTexture->Width() != 0 && m_failedTexture->Height() != 0) {
         if (m_failedTexture->Width() <= size.x&&m_failedTexture->Height() <= size.y) {
            imgw = m_failedTexture->Width();
            imgh = m_failedTexture->Height();
            imgx = size.x - imgw;
            imgx /= 2;
            imgy = size.y - imgh;
            imgy /= 2;

            imgx += pos.x;
            imgy += pos.y;
         } else {
            float ktip = m_failedTexture->Width();
            ktip /= m_failedTexture->Height();
            float ksize = size.x;
            ksize /= size.y;
            if (ktip > ksize) {
               imgw = size.x;
               imgh = imgw / ktip;
               imgx = pos.x;
               imgy = pos.y + (size.y - imgh) / 2;
            } else {
               imgh = size.y;
               imgw = imgh*ktip;
               imgy = pos.y;
               imgx = pos.x + (size.x - imgw) / 2;
            }
         }

         DrawSprite(m_failedTexture, 0xFFFFFFFF, imgx, imgy, imgx + imgw, imgy + imgh);
      }

      if (m_deviceNameTexture&&imgw != 0 && imgh != 0) {
         int x, y, w, h;
         if (size.x <= imgw * 3) {
            w = size.x;
         } else {
            w = imgw * 3;
         }

         float ktip = m_deviceNameTexture->Width();
         ktip /= m_deviceNameTexture->Height();

         h = w / ktip;


         x = imgx + (imgw - w) / 2;
         y = imgy + imgh;


         DrawSprite(m_deviceNameTexture, 0xFFFFFFFF, x, y, x + w, y + h);
      }
   }
}

void DeviceSource::SetPreviewState(bool is_preivew) {
   m_bIsPreView = is_preivew;
}

void DeviceSource::UpdateSettings() {
   OBSAPI_EnterSceneMutex();

   bool bWasCapturing = bCapturing;
   if (bWasCapturing)
      Stop();

   UnloadFilters();
   bool bRet = LoadFilters();
   if (bRet && bWasCapturing) {
      Start();
   }

   OBSAPI_LeaveSceneMutex();
}
void DeviceSource::Reload() {
   UpdateSettings();
}

void DeviceSource::Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY) {
   DWORD size = lineSize;
   DWORD dwDWSize = size >> 2;

   if (bLeadingY) {
      for (UINT y = 0; y < renderCY; y++) {
         LPDWORD output = (LPDWORD)(convertBuffer + (y*pitch));
         LPDWORD inputDW = (LPDWORD)(lp422 + (y*linePitch) + lineShift);
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
      for (UINT y = 0; y < renderCY; y++) {
         LPDWORD output = (LPDWORD)(convertBuffer + (y*pitch));
         LPDWORD inputDW = (LPDWORD)(lp422 + (y*linePitch) + lineShift);
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
//创建DSHOW Video Filter
ImageSource * STDCALL CreateDShowVideoFilterVideoSource(XElement *data) {
   DeviceSource *source = new DeviceSource();
   Log(L"CreateDShowVideoFilterVideoSource %p", source);
   if (source) {
      if (!source->Init(data)) {
         delete source;
         return NULL;
      }
   }
   return source;
}

