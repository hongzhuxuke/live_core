#include "Utility.h"
#include <comutil.h>
#include "BufferQueue.h"
#include "DeckLinkDevice.h"
#include "Logging.h"  
#define DECKLINKAUDIODEBUG_
using namespace std;

#define MAX_BUFFER_NUM 3
#define MAXAUDIOBUFFER 3
#define MAXCACHEAUDIODATAUNIT   1

DeckLinkDevice::DeckLinkDevice(IDeckLink* device) :
mDeckLink(device),
mDeckLinkInput(NULL),
mSupportsFormatDetection(false),
mRefCount(1),
mCurrentlyCapturing(false),
mApplyDetectedInputMode(false),
mEnableAudio(false),
mEnableVideo(false),
mVideoInputChangedNotify(0),
mIsInit(false),
mAudioBufferQueue(0),
mVideoBufferQueue(0),
mLastAudioBufferQueue(0),
mLastVideoBufferQueue(0),
mAudioOutputDataUnit(0),
mVideoOutputDataUnit(0),
mChannels(2),
mSampleRate(bmdAudioSampleRate48kHz),
mSampleType(bmdAudioSampleType32bitInteger),
mOutputPft(bmdFormat8BitYUV) {
   mDeckLink->AddRef();
   v_mtuex_init(&mDeviceMutex);
   mStartTimeInMs = 0;
   mNullVideoFrameCnt = 0;
   mStartTimeAudioMs = 0;
   mVideoOutputDataUnitLast = NULL;
   mLastAudioPts = 0;
   mSyncAudio = false;
   mLastAudioBufferQueue = NULL;
   //mLastAudioOutputDataUnit=NULL;
   mFormatChangedMutex = OSCreateMutex();


   mDisplayWidth = 1280;
   mDisplayHeight = 720;
   mDisplayFrameInternal = 166666;
}

DeckLinkDevice::~DeckLinkDevice() {
   Destory();

   if (mDeckLink != NULL) {
      mDeckLink->Release();
      mDeckLink = NULL;
   }
   v_mtuex_destory(&mDeviceMutex);
   if (mAudioBufferQueue) {
      delete mAudioBufferQueue;
      mAudioBufferQueue = NULL;
   }
   if (mVideoBufferQueue) {
      delete mVideoBufferQueue;
      mVideoBufferQueue = NULL;
   }

   if (mFormatChangedMutex) {
      OSCloseMutex(mFormatChangedMutex);
      mFormatChangedMutex = NULL;
   }
}
void DeckLinkDevice::Destory() {
   if (mDeckLinkInput != NULL) {
      mDeckLinkInput->Release();
      mDeckLinkInput = NULL;
   }
}
HRESULT	STDMETHODCALLTYPE DeckLinkDevice::QueryInterface(REFIID iid, LPVOID *ppv) {
   HRESULT			result = E_NOINTERFACE;

   if (ppv == NULL)
      return E_INVALIDARG;

   // Initialise the return result
   *ppv = NULL;

   // Obtain the IUnknown interface and compare it the provided REFIID
   if (iid == IID_IUnknown) {
      *ppv = this;
      AddRef();
      result = S_OK;
   } else if (iid == IID_IDeckLinkInputCallback) {
      *ppv = (IDeckLinkInputCallback*)this;
      AddRef();
      result = S_OK;
   } else if (iid == IID_IDeckLinkNotificationCallback) {
      *ppv = (IDeckLinkNotificationCallback*)this;
      AddRef();
      result = S_OK;
   }

   return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDevice::AddRef(void) {
   return InterlockedIncrement((LONG*)&mRefCount);
}

ULONG STDMETHODCALLTYPE DeckLinkDevice::Release(void) {
   int		newRefValue;

   newRefValue = InterlockedDecrement((LONG*)&mRefCount);
   if (newRefValue == 0) {
      delete this;
      return 0;
   }

   return newRefValue;
}
//此函数内发生内存泄漏
bool DeckLinkDevice::Init() {
   IDeckLinkAttributes*            deckLinkAttributes = NULL;
   IDeckLinkDisplayModeIterator*   displayModeIterator = NULL;
   IDeckLinkDisplayMode*           displayMode = NULL;
   BSTR							deviceNameBSTR = NULL;
   //init device 

   // Get input interface
   if (mDeckLink->QueryInterface(IID_IDeckLinkInput, (void**)&mDeckLinkInput) != S_OK)
      return false;

   // Check if input mode detection is supported.
   if (mDeckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes) == S_OK) {
      if (deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &mSupportsFormatDetection) != S_OK)
         mSupportsFormatDetection = false;

      deckLinkAttributes->Release();
   }

   long videoWidth = 0;
   long videoHeight = 0;
   // Retrieve and cache mode list
   if (mDeckLinkInput->GetDisplayModeIterator(&displayModeIterator) == S_OK)

   {
      while (displayModeIterator->Next(&displayMode) == S_OK) 
      {  
         
         SIZE resolution = { 1280, 720 };
         if (displayMode->GetWidth() / 16 == displayMode->GetHeight() / 9&&
            displayMode->GetWidth()<=1920
            &&
            displayMode->GetHeight()<=1080) 
         {
            mDisplayWidth = displayMode->GetWidth();
            mDisplayHeight = displayMode->GetHeight();

            BMDTimeValue frameDuration;
            BMDTimeScale timeScale;
            displayMode->GetFrameRate(&frameDuration, &timeScale);
   
            mDisplayFrameInternal = frameDuration * 10000000 / 60000;
            //mDisplayFrameInternal = frameDuration;
            mTimeScale = timeScale;

            mDisplayMode=displayMode->GetDisplayMode();
         }
         
         
         videoWidth = displayMode->GetWidth();
         videoHeight = displayMode->GetHeight();
         

      }
      
      displayModeIterator->Release();
   }

   // Get device name
   if (mDeckLink->GetDisplayName(&deviceNameBSTR) == S_OK) {
      mDeviceName = deviceNameBSTR;
      ::SysFreeString(deviceNameBSTR);
   } else {
      mDeviceName = L"DeckLink";
   }
   mIsInit = true;
   mNullVideoFrameCnt = 0;
   
   return true;
}

const std::wstring&DeckLinkDevice::GetDeviceName() {
   return mDeviceName;
};

bool DeckLinkDevice::SupportsFormatDetection() {
   return (mSupportsFormatDetection == TRUE);
};

IDeckLink* DeckLinkDevice::DeckLinkInstance() {
   return mDeckLink;
}
long DeckLinkDevice::GetFrameSize(BMDPixelFormat    pft, long width, long height) {
   switch (pft) {
   case bmdFormat8BitARGB: //
      return   (width * 32 / 8) * height;
   case bmdFormat8BitYUV:   // UYVY
      return   (width * 16 / 8) * height;
   default:
      break;
   }
   return 0;
}
HRESULT DeckLinkDevice::VideoInputFormatChanged(/* in */ BMDVideoInputFormatChangedEvents notificationEvents,
                                                /* in */ IDeckLinkDisplayMode *newMode,
                                                /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags) {
   OSSleep(500);
   OSEnterMutex(mFormatChangedMutex);

   Log(TEXT("DeckLinkDevice::VideoInputFormatChanged"));

   // Restart capture with the new video mode if told to
   if (!mApplyDetectedInputMode) {
      OSLeaveMutex(mFormatChangedMutex);
      return S_OK;
   }

   // Stop the capture
   mDeckLinkInput->StopStreams();
   //  v_lock_mutex(&mDeviceMutex);
   unsigned long width = newMode->GetWidth();
   unsigned long height = newMode->GetHeight();
   BMDTimeValue frameDuration;
   BMDTimeScale timeScale;
   newMode->GetFrameRate(&frameDuration, &timeScale);
   VAppInfo("DeckLinkDevice::VideoInputFormatChanged new input format %dx%d", width, height);

   mDisplayWidth = width;
   mDisplayHeight = height;
   mDisplayFrameInternal = frameDuration * 10000000 / 60000;

   BufferQueue* newVideoQueue = new BufferQueue(GetFrameSize(mOutputPft, width, height), MAX_BUFFER_NUM);

   if (mVideoOutputDataUnitLast) {
      if (mLastVideoBufferQueue) {
         mLastVideoBufferQueue->FreeDataUnit(mVideoOutputDataUnitLast);
      }
   }

   if (mLastVideoBufferQueue) {
      delete mLastVideoBufferQueue;

   }

   mLastVideoBufferQueue = mVideoBufferQueue;


   VAppInfo("DeckLinkDevice::VideoInputFormatChanged ResetVideoQueue");
   mVideoBufferQueue = newVideoQueue;
   mVideoOutputDataUnitLast = mVideoOutputDataUnit;

   BufferQueue *newAudioQueue = new BufferQueue(mSampleRate*(mSampleType / 8)*mChannels, MAXAUDIOBUFFER);

   VAppInfo("mAudioBufferQueue  new BufferQueue %p", mAudioBufferQueue);

   if (mLastAudioOutputDataUnitList.Num() >= MAXCACHEAUDIODATAUNIT) {
      DataUnit *firstUnit = mLastAudioOutputDataUnitList[0];
      VAppInfo("VideoInputFormatChanged delete mLastAudioOutputDataUnit %p mLastAudioBufferQueue %p", firstUnit, mLastAudioBufferQueue);
      //      delete firstUnit;
      mLastAudioBufferQueue->FreeDataUnit(firstUnit);

      mLastAudioOutputDataUnitList.Remove(0);
      VAppInfo("VideoInputFormatChanged delete mLastAudioOutputDataUnit %p End", firstUnit);
   }

   if (mAudioOutputDataUnit != NULL) {
      mLastAudioOutputDataUnitList.Add(mAudioOutputDataUnit);
   }

   mAudioOutputDataUnit = NULL;

   if (mLastAudioBufferQueue != NULL) {
      VAppInfo("delete mLastAudioBufferQueue %p", mLastAudioBufferQueue);
      delete mLastAudioBufferQueue;
   }
   VAppInfo("delete mLastAudioBufferQueue end mLastAudioBufferQueue=%p", mAudioBufferQueue);
   mLastAudioBufferQueue = mAudioBufferQueue;



   VAppInfo("DeckLinkDevice::VideoInputFormatChanged ResetAudioQueue %p", newAudioQueue);
   mAudioBufferQueue = newAudioQueue;

   mVideoOutputDataUnit = NULL;
   mStartTimeInMs = 0;
   mLastBytes.Clear();
   mLastAudioPts = 0;


   if (mVideoInputChangedNotify)
      mVideoInputChangedNotify->OnVideoChanged(width, height, frameDuration, timeScale);
   // Set the video input mode
   if (mDeckLinkInput->EnableVideoInput(newMode->GetDisplayMode(), mOutputPft, bmdVideoInputEnableFormatDetection) != S_OK) {
      // Let the UI know we couldnt restart the capture with the detected input mode
      VAppError("DeckLinkDevice::VideoInputFormatChanged mDeckLinkInput->EnableVideoInput failed");

      goto bail;
   }

   // Start the capture
   if (mDeckLinkInput->StartStreams() != S_OK) {
      // Let the UI know we couldnt restart the capture with the detected input mode
      VAppError("DeckLinkDevice::VideoInputFormatChanged mDeckLinkInput->StartStreams failed");
      goto bail;
   }


   DeckLinkEventNotify(DeckLinkDeviceImageFormatChanged, (void *)this->mDeviceName.c_str());
bail:
   OSLeaveMutex(mFormatChangedMutex);
   // v_unlock_mutex(&mDeviceMutex);
   return S_OK;
}
HRESULT DeckLinkDevice::VideoInputFrameArrived(/* in */ IDeckLinkVideoInputFrame* videoFrame,
                                               /* in */ IDeckLinkAudioInputPacket* audioPacket)
{
   HRESULT hr = S_OK;
   BMDTimeValue frameTime;
   BMDTimeValue frameDuration;
   int width = 0;
   int height = 0;
   if (videoFrame) {
      mHasVideo = true;
      void *frameBytes = NULL;

      videoFrame->GetBytes(&frameBytes);
      videoFrame->GetStreamTime(&frameTime, &frameDuration, 1000);

      if (videoFrame->GetFlags() & bmdFrameHasNoInputSource) {
         if (mNullVideoFrameCnt < 50) {
            mNullVideoFrameCnt++;
            return S_OK;
         } else {
            if (videoFrame->GetPixelFormat() == bmdFormat8BitYUV) {
               unsigned bars[8] = {
                  0xEA80EA80, 0xD292D210, 0xA910A9A5, 0x90229035,
                  0x6ADD6ACA, 0x51EF515A, 0x286D28EF, 0x10801080 };
               width = videoFrame->GetWidth();
               height = videoFrame->GetHeight();
               unsigned *p = (unsigned *)frameBytes;

               for (int y = 0; y < height; y++) {
                  for (int x = 0; x < width; x += 2)
                     *p++ = bars[(x * 8) / width];
               }
            }
         }
         //VAppWarn("DeckLinkDevice::VideoInputFrameArrived No input signal detected width:%d height:%d", width, height);
      } else {
         mNullVideoFrameCnt = 0;
      }

      long size = videoFrame->GetRowBytes() * videoFrame->GetHeight();
      DataUnit* newVideo = mVideoBufferQueue->MallocDataUnit();
      if (newVideo == NULL) {
         //if not free buffer, need update frame, so remove old frame
         DataUnit* updateBuffer = mVideoBufferQueue->GetDataUnit(false);
         if (updateBuffer) {
            if (updateBuffer) {
               mVideoBufferQueue->FreeDataUnit(updateBuffer);
            }
         }
         // VAppWarn("drop video ");
         newVideo = mVideoBufferQueue->MallocDataUnit();
      }
      if (newVideo) {
         newVideo->dataSize = size;
         newVideo->timestap = frameTime;
         memcpy(newVideo->unitBuffer, frameBytes, size);
         mVideoBufferQueue->PutDataUnit(newVideo);
      }
   } else {
      mHasVideo = false;
   }

   QWORD osTime = GetQPCTimeMS();
   if (mStartTimeInMs == 0) {
      mStartTimeInMs = osTime;
      mStartTimeAudioMs = 0;
      mLastBytes.Clear();
      mLastAudioPts = 0;
   }
   static int index = 0;
   index++;

   static QWORD loseCount = 0;

   if (audioPacket) {
      if (mResetAudioStatus) {
         mResetAudioStatus = false;
         mLastAudioPts = 0;
         mStartTimeAudioMs = 0;
         mStartTimeInMs = 0;
         mAudioBuffer.Clear();

      }

      if (mLastAudioPts == 0) {
         mLastAudioPts = osTime;

      }
      mHasAudio = true;

      QWORD msSize = mChannels*mSampleRate*mSampleType / 8 / 1000;

      void *audioFrameBytes = NULL;
      BMDTimeValue audio_pts;
      long size = audioPacket->GetSampleFrameCount() *mChannels * (mSampleType / 8);
      audioPacket->GetBytes(&audioFrameBytes);
      hr = audioPacket->GetPacketTime(&audio_pts, 1000); // ms

      mAudioBuffer.AppendArray((char *)audioFrameBytes, size);
      if (mAudioBuffer.Num() > msSize * 300) {
         mAudioBuffer.RemoveRange(0, (UINT)(mAudioBuffer.Num() - msSize * 300));
      }

      if (mStartTimeInMs == 0) {
         mStartTimeInMs = osTime;
      }

      //int syncBufLen=msSize*300;
      if (mSyncAudio) {
         mSyncAudio = false;

      }

      //300毫秒缓存
      if (mAudioBuffer.Num() >= msSize * 300) {
         double dpts = (double)mAudioBuffer.Num();
         dpts = (double)(dpts / (double)msSize);
         QWORD pts = (QWORD)dpts;

         DataUnit* newAudio = mAudioBufferQueue->MallocDataUnit();
         if (newAudio) {
            newAudio->dataSize = msSize * 100;

            newAudio->timestap = (uint64_t)(osTime - 300);
            memcpy(newAudio->unitBuffer, mAudioBuffer.Array(), (size_t)msSize * 100);
            mAudioBufferQueue->PutDataUnit(newAudio);
            static int logIndex = 0;
            if (logIndex % 300 == 0) {
               logIndex = 0;
               VAppInfo(TEXT("PUSH OS:%llu,PTS:%llu,DIF:%llu"), osTime, newAudio->timestap, osTime - newAudio->timestap);
               VAppInfo(TEXT("OS:\t%llu\t APS:\t%llu\tDif:\t%llu"), osTime, mStartTimeInMs - 100, osTime - mStartTimeInMs + 100);
               VAppInfo(TEXT("mChannels:\t%lu\t mSampleRate:\t%d\tmSampleType:\t%d"),
                        mChannels, (int)mSampleRate, (int)mSampleType);

            }
            logIndex++;
         } else {

            // VAppWarn("drop audio ");
         }
         mAudioBuffer.RemoveRange(0, (UINT)msSize * 100);


      }

      DataUnit* newAudio = mAudioBufferQueue->MallocDataUnit();
      if (newAudio) {
         newAudio->dataSize = size;
         //newAudio->timestap = mStartTimeInMs + frameTime;
         newAudio->timestap = GetQPCTimeMS();

         memcpy(newAudio->unitBuffer, audioFrameBytes, size);
         mAudioBufferQueue->PutDataUnit(newAudio);
      } else {
         // VAppWarn("drop audio ");
      }

   } else {
      mHasAudio = false;
   }
   return S_OK;
}

//-------------------------------------------------
//implement IDeckLinkDevice
void DeckLinkDevice::EnableAudio(const bool& enableAudio) {
   mEnableAudio = enableAudio;
   mStartTimeAudioMs = mStartTimeInMs = 0;
   mResetAudioStatus = true;
}
void DeckLinkDevice::EnableVideo(const bool& enableVideo, IVideoChangedNotify* videoNotify) {
   mEnableVideo = enableVideo;
   mVideoInputChangedNotify = videoNotify;
}

bool DeckLinkDevice::GetNextAudioBuffer(void **buffer, unsigned int *numFrames, unsigned long long *timestamp) {
   if (mAudioBufferQueue) {
      DataUnit* updateBuffer = mAudioBufferQueue->GetDataUnit(false);
      if (updateBuffer) {
         *buffer = updateBuffer->unitBuffer;
         *numFrames = (unsigned int)(updateBuffer->dataSize / mChannels / (mSampleType / 8));
         *timestamp = updateBuffer->timestap;
         mCurrentOutputTime = updateBuffer->timestap;

         //VAppInfo(TEXT("DeckLinkDevice::GetNextAudioBuffer mAudioOutputDataUnit %p mAudioBufferQueue %p"),mAudioOutputDataUnit,mAudioBufferQueue);
         if (mAudioOutputDataUnit) {
            mAudioBufferQueue->FreeDataUnit(mAudioOutputDataUnit);
         }

         mAudioOutputDataUnit = updateBuffer;

         static int logIndex = 0;
         if (logIndex % 300 == 0) {

            logIndex = 0;
            QWORD osTime = GetQPCTimeMS();
            //VAppInfo(TEXT("DeckLinkDevice::GetNextAudioBuffer PTS[%llu] OS[%llu] AQueueSize[%d]"), *timestamp, osTime, mAudioBufferQueue->GetUnitCount());
         }
         logIndex++;


         return true;
      } else {
         return false;
      }
   }
   return false;
}

bool DeckLinkDevice::GetNextVideoBuffer(void **buffer, unsigned long long *bufferSize, unsigned long long *timestamp) {

   bool isUpdateFrame = true;
   if (mVideoBufferQueue == NULL)
      return false;

   //update frame, swap output video frame
   if (isUpdateFrame) {
      DataUnit* updateBuffer = mVideoBufferQueue->GetDataUnit(false);
      if (updateBuffer) {
         if (mVideoOutputDataUnit) {
            mVideoBufferQueue->FreeDataUnit(mVideoOutputDataUnit);
         }
         mVideoOutputDataUnit = updateBuffer;
         if (mHasAudio == false)
            mCurrentOutputTime = updateBuffer->timestap;
         // return true;
      }
   }
   if (mVideoOutputDataUnit) {
      *buffer = mVideoOutputDataUnit->unitBuffer;
      *bufferSize = mVideoOutputDataUnit->dataSize;
      *timestamp = mVideoOutputDataUnit->timestap;
#ifdef _DEBUG
      //gLogger->logInfo("timestamp %I64d.", mVideoOutputDataUnit->timestap);
#endif
      return true;
   }
   return false;
}

bool DeckLinkDevice::StartCapture() {
   OSEnterMutex(mFormatChangedMutex);

   if (mIsInit == false) {
      if (Init() == false) {
         OSLeaveMutex(mFormatChangedMutex);
         return false;
      }
   } else {
      mIsInit = false;
      Destory();
      OSLeaveMutex(mFormatChangedMutex);
      return false;
   }



   VAppInfo("DeckLinkDevice::StartCapture");

   BMDVideoInputFlags		videoInputFlags = bmdVideoInputFlagDefault;
   mApplyDetectedInputMode = true;

   // Enable input video mode detection if the device supports it
   if (mSupportsFormatDetection == TRUE)
      videoInputFlags |= bmdVideoInputEnableFormatDetection;


   if (!mDeckLinkInput) {
      Init();
      OSLeaveMutex(mFormatChangedMutex);
      return false;
   }
   mDeckLinkInput->SetCallback(NULL);
   mDeckLinkInput->StopStreams();
   // Set the screen preview
   mDeckLinkInput->SetScreenPreviewCallback(NULL);

   // Set capture callback
   mDeckLinkInput->SetCallback(this);
   //mMemoryAllocator = new DeckLinkMemoryAllocator();
   //mDeckLinkInput->SetVideoInputFrameMemoryAllocator(mMemoryAllocator);


   if (mLastAudioBufferQueue != NULL) {
      VAppInfo("DeckLinkDevice::StartCapture delete mLastAudioBufferQueue");
      delete mLastAudioBufferQueue;
   }
   VAppInfo("DeckLinkDevice::StartCapture delete mLastAudioBufferQueue End");
   mLastAudioBufferQueue = mAudioBufferQueue;

   if (mLastAudioOutputDataUnitList.Num() >= MAXCACHEAUDIODATAUNIT) {
      DataUnit *firstUnit = mLastAudioOutputDataUnitList[0];
      VAppInfo("StartCapture delete mLastAudioOutputDataUnit %p", firstUnit);

      mLastAudioBufferQueue->FreeDataUnit(firstUnit);
      mLastAudioOutputDataUnitList.Remove(0);
      VAppInfo("StartCapture delete mLastAudioOutputDataUnit %p End", firstUnit);
   }
   if (mAudioOutputDataUnit) {
      mLastAudioOutputDataUnitList.Add(mAudioOutputDataUnit);
   }

   mAudioOutputDataUnit = NULL;

   mAudioBufferQueue = new BufferQueue(mSampleRate*(mSampleType / 8)*mChannels, MAXAUDIOBUFFER);

   if (mVideoBufferQueue) {
      VAppInfo("DeckLinkDevice::StartCapture delete mVideoBufferQueue");
      delete mVideoBufferQueue;
   }
   VAppInfo("DeckLinkDevice::StartCapture delete mVideoBufferQueue End");

   mVideoBufferQueue = new BufferQueue(GetFrameSize(mOutputPft, mDisplayWidth, mDisplayHeight), 4);
   mHasAudio = false;
   mHasVideo = false;
   // Set the video input mode

   VAppInfo("DeckLinkDevice::StartCapture EnableVideoInput mDisplayMode(0x%X)",mDisplayMode);
   if (mDeckLinkInput->EnableVideoInput(mDisplayMode, mOutputPft, videoInputFlags) != S_OK) {
      VAppError("This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use.");
      OSLeaveMutex(mFormatChangedMutex);
      return false;
   }

   if (mDeckLinkInput->EnableAudioInput(mSampleRate, mSampleType, mChannels) != S_OK) {
      VAppError("Enable audio input failed. May be not support sampe rate[%d], or bit[%d], or channel [%d]",
                mSampleRate, mSampleType, mChannels);
      OSLeaveMutex(mFormatChangedMutex);
      return false;
   }

   // Start the capture
   if (mDeckLinkInput->StartStreams() != S_OK) {
      VAppError("This application was unable to start the capture. Perhaps, the selected device is currently in-use.");
      OSLeaveMutex(mFormatChangedMutex);
      return false;
   }
   mCurrentlyCapturing = true;
   OSLeaveMutex(mFormatChangedMutex);
   return true;
}

void DeckLinkDevice::StopCapture() {
   VAppInfo("DeckLinkDevice::StopCapture");
   OSEnterMutex(mFormatChangedMutex);
   if (mDeckLinkInput != NULL) {

      mDeckLinkInput->DisableVideoInput();
      mDeckLinkInput->DisableAudioInput();



      mDeckLinkInput->SetScreenPreviewCallback(NULL);
      // Delete capture callback
      mDeckLinkInput->SetCallback(NULL);

      // Stop the capture
      mDeckLinkInput->StopStreams();
      SafeRelease(mDeckLinkInput);
   }
   mCurrentlyCapturing = false;
   mNullVideoFrameCnt = 0;
   OSLeaveMutex(mFormatChangedMutex);
}
void DeckLinkDevice::AudioReInit() {
   Destory();
   mIsInit = false;
   StartCapture();
}
bool DeckLinkDevice::IsCapturing() {
   return mCurrentlyCapturing;
}
bool DeckLinkDevice::GetAudioParam(unsigned int& channels, unsigned int& samplesPerSec, unsigned int& bitsPerSample) {
   channels = mChannels;
   samplesPerSec = mSampleRate;
   bitsPerSample = mSampleType;
   return true;
}
bool DeckLinkDevice::GetVideoParam(unsigned long& videoWidth,
   unsigned long& videoHeight,
   long long & frameDuration, 
   long long &timeScale) {// default pixfm

   videoWidth = mDisplayWidth;
   videoHeight = mDisplayHeight;
   frameDuration= mDisplayFrameInternal;

   return false;
}

const wchar_t* DeckLinkDevice::GetDeckLinkDeviceName() const {
   return mDeviceName.c_str();
}
void DeckLinkDevice::AudioSync() {
   mSyncAudio = true;
}
DeckLinkDevicesManager::DeckLinkDevicesManager() {
   mDeviceDiscovery = NULL;
   mLastDecklinkDevice = NULL;
   mManagerMutex = OSCreateMutex();
   mIsInit = false;
}
DeckLinkDevicesManager::~DeckLinkDevicesManager() {
   BSTR		deviceNameBSTR = NULL;
   HRESULT						result;

   if (mDeviceDiscovery != NULL) {
      mDeviceDiscovery->Disable();
      //mDeviceDiscovery->Release();
      delete mDeviceDiscovery;
      mDeviceDiscovery = NULL;
   }
   
   for(
      std::map< std::wstring, DeckLinkDevice*>::iterator itera=mDevicesList.begin();
      itera!=mDevicesList.end();
   itera++)
   {
      DeckLinkDevice *iDevice=itera->second;
      if(iDevice)
      {
         IDeckLink* iDecklink=iDevice->DeckLinkInstance();
         if(iDecklink)
         {
            result = iDecklink->GetModelName(&deviceNameBSTR);
            if(result == S_OK)
            {
               DeckLinkEventNotify(DeckLinkDeviceRemove,deviceNameBSTR);
               iDevice->StopCapture();
               iDevice->Destory();
            }

         }
         delete iDevice;
         iDevice=NULL;
      }
   }

   mDevicesList.clear();
   if (mManagerMutex) {
      OSCloseMutex(mManagerMutex);
      mManagerMutex = NULL;
   }
}

bool DeckLinkDevicesManager::Init() {
   mDeviceDiscovery = new DeckLinkDeviceDiscovery(this);
   if (!mDeviceDiscovery->Enable()) {
      VAppError("DeckLinkDevicesManager::Init Please install the Blackmagic Desktop Video drivers to use the features of this application,"
                " This application requires the Desktop Video drivers installed");
      delete mDeviceDiscovery;
      mDeviceDiscovery=NULL;
      return false;
   }
   return true;
}
void DeckLinkDevicesManager::UnInit() {
   if (mDeviceDiscovery != NULL) {
      mDeviceDiscovery->Disable();
      delete mDeviceDiscovery;
      mDeviceDiscovery = NULL;
   }
}

void DeckLinkDevicesManager::AddDevice(IDeckLink* deckLink) {
   BSTR		deviceNameBSTR = NULL;
   HRESULT						result;
   VAppInfo("will add device");
   result = deckLink->GetModelName(&deviceNameBSTR);
   if (result == S_OK) {
      wstring deviceName = deviceNameBSTR;
      if (mDevicesList.find(deviceName) == mDevicesList.end()) {
         mDevicesList[deviceName] = new DeckLinkDevice(deckLink);
         DeckLinkEventNotify(DeckLinkDeviceAdd, deviceNameBSTR);
      } else {
         //restart this device
         VAppError("This device is in-use.");
      }

   }
}
void DeckLinkDevicesManager::RemoveDevice(IDeckLink* deckLink) {
   BSTR		deviceNameBSTR = NULL;
   HRESULT						result;
   result = deckLink->GetModelName(&deviceNameBSTR);
   if (result == S_OK) {
      wstring deviceName = deviceNameBSTR;
      if (mDevicesList.find(deviceName) != mDevicesList.end()) {
         DeckLinkDevice* deckLinkDevice = mDevicesList[deviceName];
         if (deckLinkDevice) {

            DeckLinkEventNotify(DeckLinkDeviceRemove, deviceNameBSTR);
            deckLinkDevice->StopCapture();
            deckLinkDevice->Destory();
            if (mLastDecklinkDevice != NULL) {
               delete mLastDecklinkDevice;
            }
            mLastDecklinkDevice = deckLinkDevice;
            //delete deckLinkDevice;
         }
         mDevicesList.erase(deviceName);
      }
   }
}
int DeckLinkDevicesManager::GetDeviceNum() {
   return mDevicesList.size();
}
const wchar_t* DeckLinkDevicesManager::GetDeviceName(const unsigned int &index) {
   if (index >= mDevicesList.size())
      return NULL;
   std::map< std::wstring, DeckLinkDevice*>::iterator iter = mDevicesList.begin();
   unsigned int cnt = 0;
   while (cnt < index) {
      iter++;
      cnt++;
   }
   return  iter->first.c_str();
}

IDeckLinkDevice* DeckLinkDevicesManager::GetDevice(const wchar_t*deviceName, void *key) {
   OutputDebugString(L"DeckLinkDevicesManager::GetDevice\n");
   wstring wsName;
   if (deviceName)
      wsName = deviceName;
   if (mDevicesList.find(wsName) != mDevicesList.end()) {
      IDeckLinkDevice *device = mDevicesList[wsName];
      if (device&&key) {
         mUseList[key] = device;
      }
      return device;
   }
   return  NULL;
}
void DeckLinkDevicesManager::ReleaseDevice(IDeckLinkDevice *device, void *key) {
   std::map<void *, IDeckLinkDevice *>::iterator itera = mUseList.find(key);
   if (itera != mUseList.end()) {
      mUseList.erase(itera);
   }
   bool isReleaseDevice = true;
   for (itera = mUseList.begin(); itera != mUseList.end(); itera++) {
      if (itera->second == device) {
         isReleaseDevice = false;
         break;
      }
   }
   if (isReleaseDevice == true) {
      if (device) {
         device->StopCapture();
      }
   }
}

void *DeckLinkDevicesManager::RegistterDeckLinkDeviceEvent(DeckLinkDeviceEventCallBack callBack, void *param) {
   DeckLinkDeviceCallBackStruct *cb = new DeckLinkDeviceCallBackStruct(callBack, param);
   if (cb) {
      OSEnterMutex(mManagerMutex);
      mCallBackList.push_back(cb);
      OSLeaveMutex(mManagerMutex);
   }
   return cb;
}
void DeckLinkDevicesManager::UnRegistterDeckLinkDeviceEvent(void *handle) {
   OSEnterMutex(mManagerMutex);
   for (std::list<DeckLinkDeviceCallBackStruct *>::iterator  itera = mCallBackList.begin();
        itera != mCallBackList.end(); itera++) {
      if (*itera == handle) {
         delete *itera;
         mCallBackList.erase(itera);
         break;
      }
   }
   OSLeaveMutex(mManagerMutex);

}

void DeckLinkDevicesManager::DeckLinkEventNotify(DeckLinkDeviceEventEnum e, void *p) {
   OSEnterMutex(mManagerMutex);
   for (std::list<DeckLinkDeviceCallBackStruct *>::iterator  itera = mCallBackList.begin();
        itera != mCallBackList.end(); itera++) {
      (*itera)->Notify(e, p);
   }
   OSLeaveMutex(mManagerMutex);
}
IDeckLinkDiscovery *CreateDeckLinkDiscoveryInstance(void) {
   IDeckLinkDiscovery *instance;
   const HRESULT result = CoCreateInstance(CLSID_CDeckLinkDiscovery,
                                           nullptr,
                                           CLSCTX_ALL,
                                           IID_IDeckLinkDiscovery,
                                           (void **)&instance);


   return result == S_OK ? instance : nullptr;
}

//-------------------------------------
DeckLinkDeviceDiscovery::DeckLinkDeviceDiscovery(DeckLinkDevicesManager* devicesManager)
: mDevicesManager(devicesManager), m_deckLinkDiscovery(NULL), m_refCount(1), mInitialized(false) {
   m_deckLinkDiscovery = CreateDeckLinkDiscoveryInstance();
}


DeckLinkDeviceDiscovery::~DeckLinkDeviceDiscovery() {

}

bool DeckLinkDeviceDiscovery::Enable() {
   HRESULT     result = E_FAIL;

   // Install device arrival notifications
   if (m_deckLinkDiscovery != NULL){
      result = m_deckLinkDiscovery->InstallDeviceNotifications(this);
      if(result==S_OK)
      {
         mInitialized=true;
      }
   }
   return result == S_OK;

}

void DeckLinkDeviceDiscovery::Disable() {
   if(mInitialized)
   {
      mInitialized=false;
   }
   else
   {
      return ;
   }
   // Uninstall device arrival notifications
   if (m_deckLinkDiscovery != NULL)
      m_deckLinkDiscovery->UninstallDeviceNotifications();
}

HRESULT DeckLinkDeviceDiscovery::DeckLinkDeviceArrived(/* in */ IDeckLink* deckLink) {
   deckLink->AddRef();
   // Update UI (add new device to menu) from main thread
   mDevicesManager->AddDevice(deckLink);
   return S_OK;
}

HRESULT DeckLinkDeviceDiscovery::DeckLinkDeviceRemoved(/* in */ IDeckLink* deckLink) {
   // Update UI (remove device from menu) from main thread
   mDevicesManager->RemoveDevice(deckLink);
   deckLink->Release();
   return S_OK;
}

HRESULT	STDMETHODCALLTYPE DeckLinkDeviceDiscovery::QueryInterface(REFIID iid, LPVOID *ppv) {
   HRESULT			result = E_NOINTERFACE;

   if (ppv == NULL)
      return E_INVALIDARG;

   // Initialise the return result
   *ppv = NULL;

   // Obtain the IUnknown interface and compare it the provided REFIID
   if (iid == IID_IUnknown) {
      *ppv = this;
      AddRef();
      result = S_OK;
   } else if (iid == IID_IDeckLinkDeviceNotificationCallback) {
      *ppv = (IDeckLinkDeviceNotificationCallback*)this;
      AddRef();
      result = S_OK;
   }

   return result;
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::AddRef(void) {
   return InterlockedIncrement((LONG*)&m_refCount);
}

ULONG STDMETHODCALLTYPE DeckLinkDeviceDiscovery::Release(void) {
   ULONG		newRefValue;

   newRefValue = InterlockedDecrement((LONG*)&m_refCount);
   if (newRefValue == 0) {
      delete this;
      return 0;
   }
   return newRefValue;
}

DeckLinkMemoryAllocator::DeckLinkMemoryAllocator() {
   /*  mBuffer = NULL;
     mBufferSize = 0;*/
}
DeckLinkMemoryAllocator::~DeckLinkMemoryAllocator() {
   for (std::map<unsigned int, void*>::iterator iter = mBufferList.begin(); iter != mBufferList.end(); iter++) {
      free(iter->second);
   }
   mBufferList.clear();
   mBufferRef.clear();
}
HRESULT STDMETHODCALLTYPE DeckLinkMemoryAllocator::AllocateBuffer(
   /* [in] */ unsigned int bufferSize,
   /* [out] */ void **allocatedBuffer) {
   HRESULT ret = S_OK;
   if (mBufferList.find(bufferSize) != mBufferList.end()) {
      if (mBufferRef[mBufferList[bufferSize]] == 0) {
         *allocatedBuffer = mBufferList[bufferSize];
         mBufferRef[mBufferList[bufferSize]] ++;
      } else {
         *allocatedBuffer = mBufferList[bufferSize];
         mBufferRef[mBufferList[bufferSize]] ++;
      }

   } else {
      *allocatedBuffer = malloc(bufferSize);
      if (*allocatedBuffer == NULL)
         ret = E_OUTOFMEMORY;
      else {
         mBufferList[bufferSize] = *allocatedBuffer;
         mBufferRef[*allocatedBuffer] = 1;
      }
   }
   return ret;
}

HRESULT STDMETHODCALLTYPE DeckLinkMemoryAllocator::ReleaseBuffer(
   /* [in] */ void *buffer) {
   if (mBufferRef.find(buffer) != mBufferRef.end()) {
      mBufferRef[buffer] --;
   } else {
      if (buffer)
         free(buffer);
   }
   return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkMemoryAllocator::Commit(void) {
   for (std::map<unsigned int, void*>::iterator iter = mBufferList.begin(); iter != mBufferList.end(); iter++) {
      free(iter->second);
   }
   mBufferList.clear();
   mBufferRef.clear();
   return S_OK;
}

HRESULT STDMETHODCALLTYPE DeckLinkMemoryAllocator::Decommit(void) {
   for (std::map<unsigned int, void*>::iterator iter = mBufferList.begin(); iter != mBufferList.end(); iter++) {
      free(iter->second);
   }
   mBufferList.clear();
   mBufferRef.clear();
   return S_OK;
}



HRESULT	STDMETHODCALLTYPE DeckLinkMemoryAllocator::QueryInterface(REFIID iid, LPVOID *ppv) {
   HRESULT			result = E_NOINTERFACE;

   if (ppv == NULL)
      return E_INVALIDARG;

   // Initialise the return result
   *ppv = NULL;

   // Obtain the IUnknown interface and compare it the provided REFIID
   if (iid == IID_IUnknown) {
      *ppv = this;
      AddRef();
      result = S_OK;
   } else if (iid == IID_IDeckLinkMemoryAllocator) {
      *ppv = (DeckLinkMemoryAllocator*)this;
      AddRef();
      result = S_OK;
   }

   return result;
}

ULONG STDMETHODCALLTYPE DeckLinkMemoryAllocator::AddRef(void) {
   return InterlockedIncrement((LONG*)&m_refCount);
}

ULONG STDMETHODCALLTYPE DeckLinkMemoryAllocator::Release(void) {
   ULONG		newRefValue;

   newRefValue = InterlockedDecrement((LONG*)&m_refCount);
   if (newRefValue == 0) {
      delete this;
      return 0;
   }
   return newRefValue;
}
