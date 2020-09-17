#pragma once

#include <memory>
#include "VH_ConstDeff.h"

#include "IDeckLinkDevice.h"

void __declspec(dllexport) PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT mRenderCX, UINT mRenderCY, UINT pitch, UINT startY, UINT endY, UINT mLinePitch, UINT mLineShift);

struct _Deinterlacer {
   int                         type; //DeinterlacingType
   char                        fieldOrder; //DeinterlacingFieldOrder
   char                        processor; //DeinterlacingProcessor
   bool                        curField, bNewFrame;
   bool                        doublesFramerate;
   bool                        needsPreviousFrame;
   bool                        isReady;
   std::unique_ptr<Texture>    texture;
   UINT                        mImageCX, mImageCY;
   std::unique_ptr<Shader>     vertexShader;
   FuturePixelShader           pixelShader;
};


class DeckLinkVideoSource;
class IDeckLinkDevice;
class __declspec(dllexport) DeckLinkVideoSource : public ImageSource {
private:
   void ChangeSize(bool bSucceeded = true, bool bForce = false);
   String ChooseShader();
   String ChooseDeinterlacingShader();
   void Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY);
   void FlushSamples();

   UINT GetSampleInsertIndex(LONGLONG timestamp);
   bool LoadFilters();
   void UnloadFilters();
   void Start();
   void Stop();
   void ReInit();
   void Reload();

public:
   bool Init(XElement *data);
   ~DeckLinkVideoSource();
   void UpdateSettings();
   bool Preprocess();
   void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType);
   void BeginScene();
   void EndScene();
   Vect2 GetSize() const;
   virtual void SetPreviewState(bool is_preivew);
   static void DeckLinkEventCallBack(DeckLinkDeviceEventEnum,void *,void *);
   void EventNotify(DeckLinkDeviceEventEnum,void *);
   void SetDeviceExist(bool);
   bool GetDeviceExist();
   void ImageFormatChanged();
private:
   void *mDeckLinkEventHandle = nullptr;

   bool            mDeviceExist;
   DeviceColorType mColorType;
   String          mStrDevice, mStrDeviceName, mStrDeviceID,mDisplayName;
   bool            mIsFlipVertical, mIsFlipHorizontal, mIsUsePointFiltering;
   UINT64          mFrameInterval;
   UINT            mRenderCX, mRenderCY;
   UINT            mNewCX, mNewCY;
   UINT            mImageCX, mImageCY;
   UINT            mLinePitch, mLineShift, mLineSize;
   UINT            mImageSize;
   BOOL            mIsUseCustomResolution;
   UINT            mPreferredOutputType;
   BOOL            mFullRange;
   int             mColorSpace;
   BOOL            mIsUse709;

   bool            mIsFirstFrame;
   bool            mIsUseThreadedConversion;
   bool            mIsReadyToDraw;
   std::atomic_bool m_bIsPreView = false;
   HWND            m_privew_renderHwnd = nullptr;
   Texture         *mTexture = nullptr, *mPreviousTexture = nullptr;
   XElement        *mDataElement = nullptr;
   UINT            mTexturePitch;
   bool            mIsCapturing, mIsFiltersLoaded;
   Shader          *mColorConvertShader = nullptr;
   Shader          *mDrawShader = nullptr;

   bool            mIsUseBuffering;
   HANDLE          mStopSampleEvent;
   HANDLE          mSampleMutex;
   UINT            mBufferTime;				// 100-nsec units (same as REFERENCE_TIME)
   List<SampleData*> mSamples;
   UINT            mOpacity;
   int             mGamma;
   //---------------------------------   
   LPBYTE          mImageBuffer;

   bool            mIsUseChromaKey;
   DWORD           mKeyColor;
   Color4          mKeyChroma;
   Color4          mKeyBaseColor;
   int             mKeySimilarity;
   int             mKeyBlend;
   int             mKeySpillReduction;
   IDeckLinkDevice* mDeckLinkDevice = nullptr;
   HANDLE            mDeckLinkDeviceMutex;
   _Deinterlacer  mDeinterlacer;
   Texture *mTipTexture = nullptr;
   DeviceInfo mDeviceInfo;
};


ImageSource* STDCALL CreateDeckLinkVideoSource(XElement *data);

