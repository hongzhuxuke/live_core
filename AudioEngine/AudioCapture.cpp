#include "Utility.h"
#include "Logging.h"
#include <Avrt.h>      
#include "CoreAudioSource.h"
#include "AudioCapture.h"
#include "DeckLinkAudioSource.h"
#include "DShowAudioSource.h"
//#include "GetDevices.h"
#include "IEncoder.h"
#include "MediaAudioSource.h"
#include "AudioNoiseGateFilter.h"
#include "DShowAudioSourceVideoFilter.h"
#include "AudioMixing.h"

#include <windows.h>
#include <mmiscapi2.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib,"Winmm.lib")

#define LogWarn
HANDLE mSleepHandle = NULL;
std::atomic_bool mbAudioProcessed = true; //音频处理已经完成。

void OBSApiUIWarning(wchar_t *d);
inline float toDB(float RMS) {
   float db = 20.0f * log10(RMS);
   if (!_finite(db))
      return VOL_MIN;
   return db;
}
float DBtoLog(float db) {
   /* logarithmic scale for audio meter */
   return -log10(0.0f - (db - 6.0f));
}

int MakeAdtsHeader(byte* adtsHeader, int frame_size,
   int sampling_frequency_index, int channel_configuration) {
   adtsHeader[0] = (byte)0xFF;
   adtsHeader[1] = (byte)0xF1; // 0xF9 (MPEG2)
   adtsHeader[2] = (byte)(0x40 | (sampling_frequency_index << 2) | (channel_configuration >> 2));
   adtsHeader[3] = (byte)(((channel_configuration & 0x3) << 6) | ((frame_size + 7) >> 11));
   adtsHeader[4] = (byte)(((frame_size + 7) >> 3) & 0xFF);
   adtsHeader[5] = (byte)((((frame_size + 7) << 5) & 0xFF) | 0x1F);
   adtsHeader[6] = (byte)0xFC;
   return 0;
}

QWORD GetVideoTime() {
   return 0;
}

AudioCapture::AudioCapture(IDataReceiver* dataReiciever, int SampleRateHz) :
   mDataReceiver(dataReiciever) {
   mSampleRateHz = SampleRateHz; // 44100;  48000 if not aac
   mAudioChannels = 2;
   mForceMicMono = false;
   mRecievedFirstAudioFrame = false;
   mAuxAudioMutex = OSCreateMutex();
   mSaveMicAudioMutex = OSCreateMutex();
   mAudioFilterMutex = OSCreateMutex();
   //mCurDesktopVol = 1.0f;
   mCurMicVol = 1.0f;
   mMicVol = 1.0f;
   mPlaybackBackVol = 0.0f;
   //mPlaybackVol = 0.0f;
   mPlaybackAudio = NULL;
   mMicAudio = NULL;
   mMicBackVol = 1.0f;
   m_bSaveMicOrignalAudio = false;
   mSleepHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);
   //mMixAudioFile = fopen("E:\\a.pcm", "wb");
   //mMicAudioFile = fopen("E:\\mMicAudioFile.pcm", "wb");
   //mMediaAudioFile = fopen("E:\\mMediaAudioFile.pcm", "wb");
   //mPlayerAudioFile = fopen("E:\\mPlayerAudioFile.pcm", "wb");
   //mRawFile = fopen("E:\\mBeforeMix.pcm", "wb");
   mPhaseCheckPtr = new PhaseCheck();
   mPhaseCheckPtr->Init(mSampleRateHz, false);
}


AudioCapture::~AudioCapture() {
   Shutdown();
   AudioCapture::WaitForShutdownComplete();
   ClearRecodingSource();
   if (mPlaybackAudio) {
      mPlaybackAudio->StopCapture();
      delete mPlaybackAudio;
      mPlaybackAudio = NULL;
   }
   OSCloseMutex(mAuxAudioMutex);
   OSCloseMutex(mAudioFilterMutex);
   OSCloseMutex(mSaveMicAudioMutex);
   if (mPhaseCheckPtr) {
      delete mPhaseCheckPtr;
      mPhaseCheckPtr = nullptr;
   }
}
//初始化扬声器
bool AudioCapture::InitPlaybackSource(wchar_t* deviceName) {
   if (gLogger) {
      gLogger->logInfo("%s", __FUNCTION__);
   }
   String strPlaybackDevice;
   if (deviceName == NULL) {
      strPlaybackDevice = L"Default";
   }
   else {
      strPlaybackDevice = deviceName;
   }
   if (gLogger) {
      gLogger->logInfo("%s OSEnterMutex(mAuxAudioMutex)", __FUNCTION__);
   }
   OSEnterMutex(mAuxAudioMutex);
   if (gLogger) {
      gLogger->logInfo("%s new CoreAudioSource", __FUNCTION__);
   }

   CoreAudioSource *source = new CoreAudioSource(false, GetVideoTime, mSampleRateHz);
   if (source->Initialize(false, strPlaybackDevice)) {
      if (gLogger) {
         gLogger->logInfo("%s Initialize", __FUNCTION__);
      }
      IAudioSource* tmp = mPlaybackAudio;
      mPlaybackAudio = NULL;
      if (tmp) {
         tmp->StopCapture();
         if (gLogger) {
            gLogger->logInfo("%s StopCapture", __FUNCTION__);
         }
         delete tmp;
      }
      mPlaybackAudio = source;
      if (mStart && mPlaybackBackVol > 0)
         mPlaybackAudio->StartCapture();

      if (gLogger) {
         gLogger->logInfo("%s StartCapture", __FUNCTION__);
      }
      OSLeaveMutex(mAuxAudioMutex);
      return true;
   }

   delete source;
   OSLeaveMutex(mAuxAudioMutex);
   if (gLogger) {
      gLogger->logInfo("%s mAuxAudioMutex", __FUNCTION__);
   }
   return false;
}
bool AudioCapture::SetMicDeviceInfo(DeviceInfo deviceInfo, bool isNoise, int closeThreshold, int openThreshold, int iAudioSampleRate, HRESULT& reuslt) {
   bool add = true;
   bool changeNoise = false;
   bool changeSampleRate = false;
   if (mSampleRateHz != iAudioSampleRate)
   {
      mSampleRateHz = iAudioSampleRate;
      changeSampleRate = true;
      OSEnterMutex(mAuxAudioMutex);
      if (mMicAudio) {
         mMicAudio->StopCapture();
         mMicAudio->SetDstSampleRateHz(mSampleRateHz);
         mMicAudio->Initialize();
      }
      OSLeaveMutex(mAuxAudioMutex);
   }
   if (gLogger) {
      gLogger->logInfo("%s micDeviceInfo:ws type:%d", __FUNCTION__, deviceInfo.m_sDeviceDisPlayName, deviceInfo.m_sDeviceType);
   }
   IAudioSource *source = NULL;
   OSEnterMutex(mAuxAudioMutex);
   if (mMicAudio) {
      DeviceInfo micDeviceInfo = mMicAudio->GetDeviceInfo();
      if (micDeviceInfo == deviceInfo) {
         add = false;
      }
      UINT count = mMicAudio->NumAudioFilters();
      if (isNoise&&count == 0 || !isNoise&&count != 0 || isNoise && count > 0 || iAudioSampleRate && count == 0) {
         changeNoise = true;
      }
   }
   OSLeaveMutex(mAuxAudioMutex);
   if (add) {
      OSEnterMutex(mAuxAudioMutex);
      if (mMicAudio) {
         mMicAudio->StopCapture();
         delete mMicAudio;
         mMicAudio = NULL;
      }
      OSLeaveMutex(mAuxAudioMutex);

      switch (deviceInfo.m_sDeviceType) {
      case TYPE_COREAUDIO: {
         CoreAudioSource *coreAudioSource = new CoreAudioSource(true, GetVideoTime, mSampleRateHz);
         if (!coreAudioSource) {
            return false;
         }

         if (coreAudioSource->Initialize(true, deviceInfo.m_sDeviceID)) {
            source = coreAudioSource;
         }
         else {
            reuslt = coreAudioSource->GetErrorType();
            delete coreAudioSource;
         }
         break;
      }
      case TYPE_DECKLINK: {
         DeckLinkAudioSource *decklinkAudioSource = new DeckLinkAudioSource(deviceInfo.m_sDeviceName, mSampleRateHz);
         if (!decklinkAudioSource) {
            return false;
         }
         decklinkAudioSource->Initialize();
         source = decklinkAudioSource;
         break;
      }
      case TYPE_DSHOW_AUDIO:
      {
         DShowAudioSource *dshowAudioSource = new DShowAudioSource(deviceInfo.m_sDeviceName, deviceInfo.m_sDeviceID, mSampleRateHz);
         if (!dshowAudioSource) {
            return false;
         }
         dshowAudioSource->Initialize();
         source = dshowAudioSource;
         break;
      }
      case TYPE_DSHOW_VIDEO:
      {
         DShowAudioSourceVideoFilter *dshowAudioSource = new DShowAudioSourceVideoFilter();
         if (!dshowAudioSource) {
            break;
         }
         dshowAudioSource->Initialize(deviceInfo);
         source = dshowAudioSource;
         break;
      }
      default:
         break;
      }

      if (!source) {
         return false;
      }

      OSEnterMutex(mAuxAudioMutex);
      if (mStart) {
         source->StartCapture();
      }

      if (isNoise) {
         AudioNoiseGateFilter* filter = new AudioNoiseGateFilter();
         if (filter) {
            filter->Init(mSampleRateHz, true, closeThreshold, openThreshold);
            source->AddAudioFilter(filter);
         }
      }

      mMicAudio = source;
      mMicAudio->SetDeviceInfo(deviceInfo);
      OSLeaveMutex(mAuxAudioMutex);

   }
   else {
      if (changeNoise) {
         OSEnterMutex(mAuxAudioMutex);
         if (mMicAudio && isNoise && mMicAudio->NumAudioFilters() > 0) {
            for (int i = 0; i < mMicAudio->NumAudioFilters(); i++) {
               IAudioFilter *filter = mMicAudio->GetAudioFilter(i);
               if (filter != NULL) {
                  filter->SetThresHoldValue(closeThreshold, openThreshold);
               }
            }
         }
         else if (isNoise && mMicAudio) {
            AudioNoiseGateFilter* filter = new AudioNoiseGateFilter();
            if (filter) {
               filter->Init(this->mSampleRateHz, isNoise, closeThreshold, openThreshold);
               mMicAudio->AddAudioFilter(filter);
            }
         }
         else {
            while (mMicAudio && mMicAudio->NumAudioFilters()) {
               IAudioFilter *filter = mMicAudio->GetAudioFilter(0);
               mMicAudio->RemoveAudioFilter((UINT)0);
               delete filter;
            }
         }
         OSLeaveMutex(mAuxAudioMutex);
      }
   }
   if (gLogger) {
      gLogger->logInfo("%s set mic end", __FUNCTION__);
   }
   return true;
}

//添加数据源
bool AudioCapture::AddRecodingSource(const TYPE_RECODING_SOURCE& sourceType,
   void **pAudioSource,
   const bool& isNoiseGate,
   void **param,
   const int& paramCnt) {
   if (sourceType != TYPE_MEDIAOUT) {
      return false;
   }

   bool isMic = true;
   IAudioSource* newAudioSource = NULL;
   newAudioSource = new MediaAudioSource((IMediaOutput*)param[0], mSampleRateHz);
   gLogger->logInfo("%s new MediaAudioSource\n", __FUNCTION__);
   if (newAudioSource->Initialize()) {
      AddAudioSource(newAudioSource);
      if (pAudioSource != NULL) {
         *pAudioSource = newAudioSource;
      }

      if (mStart) {
         newAudioSource->StartCapture();
      }

      //降噪
      if (isNoiseGate)
      {
         AudioNoiseGateFilter* filter = new AudioNoiseGateFilter();
         newAudioSource->AddAudioFilter(filter);
      }
      if (gLogger) {
         gLogger->logInfo("%s  end", __FUNCTION__);
      }
      return true;
   }
   if (gLogger) {
      gLogger->logInfo("%s false end", __FUNCTION__);
   }
   return false;
}

bool AudioCapture::SetNoiseGate(bool isNoise, int closeThreshold, int openThreshold) {
   OSEnterMutex(mAuxAudioMutex);
   if (isNoise) {
      if (mMicAudio && mMicAudio->NumAudioFilters() > 0) {
         for (int i = 0; i < mMicAudio->NumAudioFilters(); i++) {
            IAudioFilter *filter = mMicAudio->GetAudioFilter(i);
            if (filter != NULL) {
               filter->SetThresHoldValue(closeThreshold, openThreshold);
            }
         }
         return true;
      }
      else if (mMicAudio) {
         AudioNoiseGateFilter* filter = new AudioNoiseGateFilter();
         if (filter) {
            filter->Init(this->mSampleRateHz, isNoise, closeThreshold, openThreshold);
            mMicAudio->AddAudioFilter(filter);
         }
         return true;
      }
      else {
         return false;
      }
   }
   else {
      while (mMicAudio && mMicAudio->NumAudioFilters()) {
         IAudioFilter *filter = mMicAudio->GetAudioFilter(0);
         mMicAudio->RemoveAudioFilter((UINT)0);
         delete filter;
      }
   }
   OSLeaveMutex(mAuxAudioMutex);
   return true;
}

bool AudioCapture::AddCoreAudioSource(DeviceInfo *device, const bool& isNoiseGate, void **pAudioSource) {
   if (gLogger) {
      gLogger->logInfo("%s ", __FUNCTION__);
   }
   String strDevice;
   DeviceInfo deviceInfo;
   if (device == NULL) {
      strDevice = L"Default";
      wcscpy(deviceInfo.m_sDeviceName, strDevice.Array());
      deviceInfo.m_sDeviceType = TYPE_COREAUDIO;
   }
   else {
      strDevice = device->m_sDeviceID;
      deviceInfo = *device;
   }

   CoreAudioSource *source = new CoreAudioSource(true, GetVideoTime, mSampleRateHz);
   if (source->Initialize(true, strDevice)) {

      OSEnterMutex(mAuxAudioMutex);
      IAudioSource* tmp = NULL;
      if (mMicAudio) {
         tmp = mMicAudio;
      }
      if (mStart)
         source->StartCapture();
      if (isNoiseGate) {
         AudioNoiseGateFilter* filter = new AudioNoiseGateFilter();
         source->AddAudioFilter(filter);
      }

      mMicAudio = source;
      mMicAudio->SetDeviceInfo(deviceInfo);
      *pAudioSource = source;

      if (tmp) {
         tmp->StopCapture();
         delete tmp;
      }
      OSLeaveMutex(mAuxAudioMutex);
      if (gLogger) {
         gLogger->logInfo("%s end ", __FUNCTION__);
      }
      return true;
   }
   else {
      delete source;
      if (gLogger) {
         gLogger->logInfo("%s end ", __FUNCTION__);
      }
      return false;
   }
}

bool AudioCapture::DelAudioSource(void *audioSource) {
   if (gLogger) {
      gLogger->logInfo("%s ", __FUNCTION__);
   }
   if (audioSource == NULL) {
      return false;
   }

   OSEnterMutex(mAuxAudioMutex);
   RemoveAudioSource((IAudioSource *)audioSource);
   IAudioSource *source = static_cast<IAudioSource *>(audioSource);
   if (source) {
      delete source;
   }
   if (mMicAudio == source) {
      mMicAudio = NULL;
   }
   OSLeaveMutex(mAuxAudioMutex);
   if (gLogger) {
      gLogger->logInfo("%s end ", __FUNCTION__);
   }
   return true;
}
void AudioCapture::SetPlaybackVolume(const float & plackBackVol) {
   OSEnterMutex(mAuxAudioMutex);
   mPlaybackBackVol = plackBackVol;
   if (mPlaybackAudio && mPlaybackBackVol > 0) {
      mPlaybackAudio->StartCapture();
   }
   else if (mPlaybackAudio && mPlaybackBackVol <= 0) {
      mPlaybackAudio->StopCapture();
      mPlaybackAudio->ClearBuffer();
   }
   OSLeaveMutex(mAuxAudioMutex);
}
void AudioCapture::SetMicVolume(const float & micVol) {
   OSEnterMutex(mAuxAudioMutex);
   mMicVol = micVol;
   OSLeaveMutex(mAuxAudioMutex);
}

void AudioCapture::ClearRecodingSource() {
   OSEnterMutex(mAuxAudioMutex);
   while (auxAudioSources.Num() > 0) {
      gLogger->logInfo("%s auxAudioSources\n", __FUNCTION__);
      delete auxAudioSources[0];
      auxAudioSources.Remove(0);
   }

   if (mMicAudio) {
      gLogger->logInfo("%s mMicAudio\n", __FUNCTION__);
      mMicAudio->StopCapture();
      delete mMicAudio;
      mMicAudio = NULL;
   }
   OSLeaveMutex(mAuxAudioMutex);
}

void AudioCapture::SetAudioListening(IDataReceiver *dataListening) {
   OSEnterMutex(mAuxAudioMutex);
   mDataListening = dataListening;
   OSLeaveMutex(mAuxAudioMutex);
}

bool AudioCapture::Start() {
   if (mStart) {
      return true;
   }
   if (gLogger) {
      gLogger->logInfo("%s Start ", __FUNCTION__);
   }
   mStart = true;
   mCaptureThread = CreateThread(NULL, 0, AudioCapture::CaptureAudioThread, this, 0, NULL);
   if (!mCaptureThread) {
      DWORD error = GetLastError();
      if (gLogger) {
         gLogger->logInfo("%s false ", __FUNCTION__);
      }
      return false;
   }

   if (mPlaybackAudio && mPlaybackBackVol > 0)
      mPlaybackAudio->StartCapture();

   OSEnterMutex(mAuxAudioMutex);
   if (mMicAudio)
      mMicAudio->StartCapture();
   for (UINT i = 0; i < auxAudioSources.Num(); i++) {
      if (auxAudioSources[i])
         auxAudioSources[i]->StartCapture();
   }
   OSLeaveMutex(mAuxAudioMutex);

   if (gLogger) {
      gLogger->logInfo("%s end ", __FUNCTION__);
   }
   return true;
}
void AudioCapture::SetPriority(int priority) {
   if (priority <= 0) {
      priority = 0;
   }
   else if (priority >= 15) {
      priority = 15;
   }

   this->m_priority = priority;
}
void AudioCapture::SetInterval(int interval) {
   if (interval > 3) {
      interval = 3;
   }
   else if (interval < 1) {
      interval = 1;
   }


   this->m_interval = interval;
}

void AudioCapture::Shutdown() {
   if (!mStart) {
      return;
   }
   if (gLogger) {
      gLogger->logInfo("%s SetEvent(mSleepHandle)", __FUNCTION__);
   }
   mStart = false;
   SetEvent(mSleepHandle);
   WaitForShutdownComplete();
   if (gLogger) {
      gLogger->logInfo("%s end", __FUNCTION__);
   }
}

UINT AudioCapture::GetSampleRateHz() const {
   return mSampleRateHz;
}
UINT AudioCapture::NumAudioChannels() const {
   return mAudioChannels;
}
UINT AudioCapture::GetBitsPerSample() const {
   return sizeof(float);
}

void AudioCapture::GetAudioMeter(float& audioMag, float& audioPeak, float& audioMax) {
   audioMag = mMixAudioMag;
   audioPeak = mMixAudioPeak;
   audioMax = mMixAudioMax;

   static int i = 0;
   if (i % 9 == 0) {
      DShowLog(DShowLogType_Level2_ALL, DShowLogLevel_Info, L"AudioCapture::GetAudioMeter After MixAudio %f\n", audioMag);
      i = 0;
   }
   i++;
}

void AudioCapture::GetMicOrignalAudiometer(float& audioMag, float& audioPeak, float& audioMax) {
   audioMag = mMixMicAudioMag;
   audioPeak = mMixMicAudioPeak;
   audioMax = mMixMicAudioMax;

   static int i = 0;
   if (i % 9 == 0) {
      DShowLog(DShowLogType_Level2_ALL, DShowLogLevel_Info, L"AudioCapture::GetMicOrignalAudiometer After MixAudio %f\n", audioMag);
      i = 0;
   }
   i++;
}

void AudioCapture::AddAudioSource(IAudioSource *source) {
   OSEnterMutex(mAuxAudioMutex);
   gLogger->logInfo("%s\n", __FUNCTION__);
   auxAudioSources << source;
   OSLeaveMutex(mAuxAudioMutex);
}
void AudioCapture::RemoveAudioSource(IAudioSource *source) {
   OSEnterMutex(mAuxAudioMutex);
   gLogger->logInfo("%s\n", __FUNCTION__);
   auxAudioSources.RemoveItem(source);
   OSLeaveMutex(mAuxAudioMutex);
}
float AudioCapture::MuteMic(bool ok) {
   if (ok) {
      if (mMicVol != 0.0f) {
         mMicBackVol = mMicVol;
         SetMicVolume(0.0f);
      }
   }
   else {
      if (mMicBackVol == 0.0f) {
         mMicBackVol = 1.0f;
      }

      if (mMicVol == 0.0f) {
         SetMicVolume(mMicBackVol);
      }
   }

   return mMicVol;
}
float AudioCapture::MuteSpeaker(bool ok) {
   if (ok) {
      SetPlaybackVolume(0.0f);
   }
   else {
      SetPlaybackVolume(1.0f);
   }
   return mPlaybackBackVol;
}
float AudioCapture::GetMicVolunm() {
   return mMicVol;
}
float AudioCapture::GetSpekerVolumn() {
   return mPlaybackBackVol;
}
bool AudioCapture::GetCurrentMic(DeviceInfo &deviceInfo) {
   OSEnterMutex(mAuxAudioMutex);
   bool bFind = false;
   if (mMicAudio) {
      deviceInfo = mMicAudio->GetDeviceInfo();
      bFind = true;
   }

   OSLeaveMutex(mAuxAudioMutex);
   return bFind;
}

void AudioCapture::SetForceMono(bool bForceMono) {

   OSEnterMutex(mAuxAudioMutex);
   mForceMicMono = bForceMono;
   OSLeaveMutex(mAuxAudioMutex);

}
bool AudioCapture::GetForceMono() {
   bool mForceMono;
   OSEnterMutex(mAuxAudioMutex);
   mForceMono = mForceMicMono;
   OSLeaveMutex(mAuxAudioMutex);
   return mForceMono;
}

void PASCAL OneAudioMixProc(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD dwl, DWORD dw2)
{
   mbAudioProcessed = false;
   AudioCapture* pObj = (AudioCapture*)dwUser;
   pObj->Process();
   mbAudioProcessed = true;
}

/*
   缓存基准时间戳
   判断各个数据源是否有新的音频数据
   */
bool AudioCapture::QueryAudioBuffers(QWORD &sleepTime) {
   QWORD duration = 0;
   bool bGotSomeAudio = false;
   if (!mLatestAudioTime) {
      mLatestAudioTime = GetQPCTimeMS();
      mLatestQueryAudioTime = mLatestAudioTime;
      sleepTime = mLatestAudioTime;
   }
   else {
      QWORD latestDesktopTimestamp = GetQPCTimeMS();
      sleepTime = latestDesktopTimestamp;
      //当前时间与基准时间戳最新值之差大于700毫秒    重置基准事件列表
      if (abs((long)(mLatestAudioTime - latestDesktopTimestamp)) > AUDIOENGINE_MAXBUFFERING_TIME_MS) {
         gLogger->logInfo("%s updata baseLine mLatestAudioTime - latestDesktopTimestamp:%d", __FUNCTION__, mLatestAudioTime - latestDesktopTimestamp);
         mLatestAudioTime = latestDesktopTimestamp;
      }
      //大于10毫秒且小于700毫秒
      else {
         mLatestAudioTime += AUDIOENGINE_PERSAMPLE_TIME_MS;
      }
   }
   //----------------------------缓存基准时间戳---------------------------------
   mBufferedAudioTimes << mLatestAudioTime;
   //判断各个数据源，是否有新的音频数据
   //---------------------------------------------------------------------------
   OSEnterMutex(mAuxAudioMutex);
   //for (UINT i = 0; i < auxAudioSources.Num(); i++) {
   //   if (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true, duration) != NoAudioAvailable) {
   //      bGotSomeAudio = true;
   //   }
   //}
   if (mMicAudio != NULL) {
      if (mMicAudio->QueryAudio2(mCurMicVol, true, IsSaveMicOrignalAudioState(), duration) != NoAudioAvailable) {
         bGotSomeAudio = true;
      }
   }
   if (mPlaybackAudio != NULL && mPlaybackBackVol > 0) {
      if (mPlaybackAudio->QueryAudio2(mPlaybackBackVol, true, duration) != NoAudioAvailable) {
         bGotSomeAudio = true;
      }
   }
   OSLeaveMutex(mAuxAudioMutex);
   //bGotSomeAudio = false;
   return true;
}
//查询音频数据
bool AudioCapture::QueryNewAudio(QWORD& sleepTime) {
   //----------------------------------缓存基准时间戳--------------------------------
   /*
      QueryAudioBuffers
      1.缓存基准时间戳
      2.判断各个数据源是否有新的数据到来
      */
   if (QueryAudioBuffers(sleepTime)) {
      ////数据源中有新的数据到来
      QWORD timestamp;
      OSEnterMutex(mAuxAudioMutex);
      //查询音频数据源
      for (UINT i = 0; i < auxAudioSources.Num(); i++) {
         //重采样，数据源插入队列
         int count = 0;
         auxAudioSources[i]->SetCurrentBaseTime(sleepTime);
         while (auxAudioSources[i]->QueryAudio2(auxAudioSources[i]->GetVolume(), true) != NoAudioAvailable) {
            count++;
         }
         //gLogger->logInfo("%s get media file data:%d", __FUNCTION__, count);
         //整理时间戳
         /*if (auxAudioSources[i]->GetLatestTimestamp(timestamp))
             auxAudioSources[i]->SortAudio(timestamp);*/

      }
      ////查询麦克风数据源
      //if (mMicAudio) {
      //   while (mMicAudio->QueryAudio2(mCurMicVol, true, IsSaveMicOrignalAudioState()) != NoAudioAvailable){
      //   }
      //   
      //   if (mMicAudio->GetLatestTimestamp(timestamp))
      //      mMicAudio->SortAudio(timestamp);

      //   if (mMicAudio->GetLatestMicAudioTimestamp(timestamp)) {
      //      mMicAudio->SortMicOrignaleAudio(timestamp);
      //   }
      //}
      ////查询扬声器数据
      //if (mPlaybackAudio) {
      //   while (mPlaybackAudio->QueryAudio2(mCurDesktopVol, true) != NoAudioAvailable){
      //   }
      //   
      //   if (mPlaybackAudio->GetLatestTimestamp(timestamp))
      //      mPlaybackAudio->SortAudio(timestamp);
      //}

      OSLeaveMutex(mAuxAudioMutex);
      return true;
   }
   return false;
}



#define INVALID_LL 0xFFFFFFFFFFFFFFFFLL

inline void CalculateVolumeLevels(float *buffer, int totalFloats, float mulVal, float &RMS, float &MAX) {
   float sum = 0.0f;
   int totalFloatsStore = totalFloats;

   float Max = 0.0f;

   if ((UPARAM(buffer) & 0xF) == 0) {
      UINT alignedFloats = totalFloats & 0xFFFFFFFC;
      __m128 sseMulVal = _mm_set_ps1(mulVal);

      for (UINT i = 0; i < alignedFloats; i += 4) {
         __m128 sseScaledVals = _mm_mul_ps(_mm_load_ps(buffer + i), sseMulVal);

         /*compute squares and add them to the sum*/
         __m128 sseSquares = _mm_mul_ps(sseScaledVals, sseScaledVals);
         sum += sseSquares.m128_f32[0] + sseSquares.m128_f32[1] + sseSquares.m128_f32[2] + sseSquares.m128_f32[3];

         /*
         sse maximum of squared floats
         concept from: http://stackoverflow.com/questions/9795529/how-to-find-the-horizontal-maximum-in-a-256-bit-avx-vector
         */
         __m128 sseSquaresP = _mm_shuffle_ps(sseSquares, sseSquares, _MM_SHUFFLE(1, 0, 3, 2));
         __m128 halfmax = _mm_max_ps(sseSquares, sseSquaresP);
         __m128 halfmaxP = _mm_shuffle_ps(halfmax, halfmax, _MM_SHUFFLE(0, 1, 2, 3));
         __m128 maxs = _mm_max_ps(halfmax, halfmaxP);

         Max = max(Max, maxs.m128_f32[0]);
      }

      buffer += alignedFloats;
      totalFloats -= alignedFloats;
   }

   for (int i = 0; i < totalFloats; i++) {
      float val = buffer[i] * mulVal;
      float pow2Val = val * val;
      sum += pow2Val;
      Max = max(Max, pow2Val);
   }

   RMS = sqrt(sum / totalFloatsStore);
   MAX = sqrt(Max);
}

void AudioCapture::Process() {
   //----------------------结束退出-----------------
   if (!mStart)
      return;
   float maxAlpha = 0.15f;
   float rmsAlpha = 0.15f;
   const unsigned int audioSamplesPerSec = mSampleRateHz;         //码率
   const unsigned int audioSampleSize = audioSamplesPerSec / 100;//采样频率
   UINT peakMeterDelayFrames = audioSamplesPerSec * 3;

   mMixAudioMax = VOL_MIN;
   mMixAudioPeak = VOL_MIN;

   List<float> mixBuffer;
   List<float> micMixBuffer;
   mixBuffer.SetSize(audioSampleSize * 2);
   micMixBuffer.SetSize(audioSampleSize * 2);

   UINT audioFramesSinceMixaudioMaxUpdate = 0;
   float *pDataIn = NULL;
   float *pDataOut = NULL;
   int iFrameLength = 0;
   int iFrameSamples = 0;
   int iBits = 4;

   iFrameLength = mSampleRateHz * PROCUNIT / 1000 * iBits;
   iFrameSamples = mSampleRateHz * PROCUNIT / 1000;
   pDataIn = new float[iFrameLength * ALL_CH];
   pDataOut = new float[iFrameLength * (ALL_CH + 1)];
   {
      int mixChannelCount = 0;
      int offSet = 0;
      memset(pDataIn, 0, iFrameLength * ALL_CH * sizeof(float));
      memset(pDataOut, 0, iFrameLength * (ALL_CH + 1) * sizeof(float));

      float *desktopBuffer = nullptr;
      float *micBuffer = nullptr;
      float *micOrignalBuffer = nullptr;

      mCurMicVol = mMicVol;
      //mCurDesktopVol = mPlaybackVol;
      QWORD sleepTime = 1;
      bool queryNewAudioRet = QueryNewAudio(sleepTime);
      /*
      查询音频数据
      1.缓存基准时间戳
      2.查询各个数据源的音频数据
      */
      if (queryNewAudioRet) {
         //------------------------取得基准时间---------------------
         QWORD timestamp = mBufferedAudioTimes[0];

         mBufferedAudioTimes.Remove(0);
         zero(mixBuffer.Array(), audioSampleSize * 2 * sizeof(float));
         zero(micMixBuffer.Array(), audioSampleSize * 2 * sizeof(float));
         OSEnterMutex(mAuxAudioMutex);

         //---------------------------混音--------------------------
         //扬声器
         if (mPlaybackAudio && mPlaybackBackVol > 0) {
            QWORD curTimeStamp;
            if (mPlaybackAudio->GetBuffer(&desktopBuffer, sleepTime, curTimeStamp)) {
               if (mPhaseCheckPtr) {
                  mPhaseCheckPtr->Check(desktopBuffer, iFrameSamples);
               }
               mixChannelCount++;
               memcpy(pDataIn + offSet, desktopBuffer, iFrameSamples * iBits);
               offSet += iFrameSamples;
               //if (mPlayerAudioFile) {
               //   fwrite(desktopBuffer, iBits, iFrameSamples, mPlayerAudioFile);
               //}
            }
         }
         //麦克风
         if (mMicAudio != NULL) {
            QWORD curTimeStamp;
            QWORD target = sleepTime;
            if (mPlaybackAudio && mPlaybackBackVol <= 0 && auxAudioSources.Num() == 0) {
               //target = 0;
            }
            if (mMicAudio->GetBuffer(&micBuffer, target, curTimeStamp)) {
               if (target == 0) {
                  sleepTime = curTimeStamp;
               }
               if (mCurMicVol > 0) {
                  if (mPhaseCheckPtr) {
                     mPhaseCheckPtr->Check(micBuffer, iFrameSamples);
                  }
                  mixChannelCount++;
                  memcpy(pDataIn + offSet, micBuffer, iFrameSamples * iBits);
                  offSet += iFrameSamples;
               }
               //if (mMicAudioFile) {
               //   fwrite(micBuffer, iBits, iFrameSamples, mMicAudioFile);
               //}
            }
            //用于用户噪音阀显示的音量。
            if (IsSaveMicOrignalAudioState()) {
               if (mMicAudio->GetMicOrignalBuffer(&micOrignalBuffer, sleepTime)) {
                  SyncMicAudioVolumn(micOrignalBuffer, audioFramesSinceMixaudioMaxUpdate);
               }
            }
            else {
               mMicAudio->ClearMicOrignalBuffer();
            }
         }

         //其他音频
         for (UINT i = 0; i < auxAudioSources.Num(); i++) {
            float *auxBuffer;
            QWORD curTimeStamp;
            if (auxAudioSources[i]->GetBuffer(&auxBuffer, timestamp, curTimeStamp)) {
               //g_pLogger->logInfo("audio process time mic_timestamp:%lld curFileTimeStamp:%lld", timestamp, curTimeStamp);
               if (mMicAudio && mPlaybackAudio && mCurMicVol <= 0 && mPlaybackBackVol <= 0) {
                  sleepTime = curTimeStamp;
               }
               if (mPhaseCheckPtr) {
                  mPhaseCheckPtr->Check(auxBuffer, iFrameSamples);
               }
               mixChannelCount++;
               memcpy(pDataIn + offSet, auxBuffer, iFrameSamples * iBits);
               offSet += iFrameSamples;
               //if (mMediaAudioFile) {
               //   fwrite(auxBuffer, iBits, iFrameSamples, mMediaAudioFile);
               //}
            }
         }
         //if (mRawFile) {
         //   fwrite(pDataIn, iBits, iFrameSamples, mRawFile);
         //}
         //g_pLogger->logInfo("mixChannelCount:%d", mixChannelCount);
         VhallAudioMixing(pDataIn, pDataOut, mixChannelCount, mSampleRateHz, 1, 1);
         //if (mMixAudioFile) {
         //   fwrite(pDataOut, iBits, iFrameSamples, mMixAudioFile);
         //}
         //}

         //计算音量
         SyncAudioVolumn(pDataOut, audioFramesSinceMixaudioMaxUpdate);

         //---------------------将原始音频编码-----------------------------------------------
         if (mDataReceiver) {
            mDataReceiver->PushAudioSegment(pDataOut, audioSampleSize, sleepTime);
         }

         if (mDataListening) {
            mDataListening->PushAudioSegment(pDataOut, audioSampleSize, sleepTime);
         }
         OSLeaveMutex(mAuxAudioMutex);
      }
   }
   if (pDataIn != NULL) {
      delete[]pDataIn;
      pDataIn = NULL;
   }
   if (pDataOut != NULL) {
      delete[]pDataOut;
      pDataOut = NULL;
   }
}

//音频采集循环
void AudioCapture::CaptureAudioLoop() {
   if (gLogger) {
      gLogger->logInfo("%s ", __FUNCTION__);
   }
   int eventId = 0;
   mBufferedAudioTimes.Clear();
   mLatestAudioTime = 0;
   eventId = timeSetEvent(10, 1, (LPTIMECALLBACK)OneAudioMixProc, (DWORD_PTR)(this), TIME_PERIODIC);
   if (eventId == 0) {
      gLogger->logInfo("%s start timeSetEvent err", __FUNCTION__);
      return;
   }
   gLogger->logInfo("%s start timeSetEvent eventId:%d", __FUNCTION__, eventId);
   WaitForSingleObject(mSleepHandle, INFINITE);
   timeKillEvent(eventId);  //timeKillEvent和回调函数是异步的，即如果回调正在执行，timeKillEvent不会等待回调执行完才返回，而是直接返回
   while (!mbAudioProcessed) {
      gLogger->logInfo("%s wait callback processed", __FUNCTION__);
      Sleep(20);
   }
   gLogger->logInfo("%s timeKillEvent eventId:%d", __FUNCTION__, eventId);
   return;
}

DWORD STDCALL AudioCapture::CaptureAudioThread(LPVOID lpUnused) {
   AudioCapture* thisObj = (AudioCapture*)lpUnused;
   CoInitialize(0);
   thisObj->CaptureAudioLoop();
   CoUninitialize();
   return 0;
}

void AudioCapture::WaitForShutdownComplete() {
   if (mCaptureThread) {  //  mCaptureThread
      DWORD waitResult = WaitForSingleObject(mCaptureThread, 5000);
      if (waitResult == WAIT_OBJECT_0) {
         gLogger->logInfo("capture audio  thread exit.");
      }
      else if (waitResult == WAIT_TIMEOUT) {
         // _ASSERT(FALSE);
         gLogger->logError("capture audio  thread shutdown timeout.");
      }
      else {
         _ASSERT(FALSE);
         gLogger->logError("capture audio thread shutdown failed.");
      }
      CloseHandle(mCaptureThread);
      mCaptureThread = NULL;
   }
   gLogger->logInfo("capture video  audio shutdown complete.");
}
void AudioCapture::SyncAudioVolumn(float* mixBuffer/*List<float> &mixBuffer*/, UINT &audioFramesSinceMixaudioMaxUpdate) {
   const unsigned int audioSamplesPerSec = mSampleRateHz;
   const unsigned int audioSampleSize = audioSamplesPerSec / 100;
   float       mixAudioRMS, mixAudioMx;
   float maxAlpha = 0.15f;
   float rmsAlpha = 0.15f;
   UINT peakMeterDelayFrames = audioSamplesPerSec * 3;

   CalculateVolumeLevels(mixBuffer, audioSampleSize * 2, 1.0f, mixAudioRMS, mixAudioMx);

   mixAudioRMS = toDB(mixAudioRMS);
   mixAudioMx = toDB(mixAudioMx);

   if (mixAudioMx > mMixAudioMax)
      mMixAudioMax = mixAudioMx;
   else
      mMixAudioMax = maxAlpha * mixAudioMx + (1.0f - maxAlpha) * mMixAudioMax;

   if (mMixAudioMax > mMixAudioPeak || audioFramesSinceMixaudioMaxUpdate > peakMeterDelayFrames) {
      mMixAudioPeak = mMixAudioMax;
      audioFramesSinceMixaudioMaxUpdate = 0;
   }
   else {
      audioFramesSinceMixaudioMaxUpdate += audioSampleSize;
   }
   mMixAudioMag = rmsAlpha * mixAudioRMS + mMixAudioMag * (1.0f - rmsAlpha);


}

void AudioCapture::SyncMicAudioVolumn(float* mixBuffer/*List<float> &mixBuffer*/, UINT &audioFramesSinceMixaudioMaxUpdate) {
   const unsigned int audioSamplesPerSec = mSampleRateHz;
   const unsigned int audioSampleSize = audioSamplesPerSec / 100;
   float       mixAudioRMS, mixAudioMx;
   float maxAlpha = 0.15f;
   float rmsAlpha = 0.15f;
   UINT peakMeterDelayFrames = audioSamplesPerSec * 3;

   CalculateVolumeLevels(mixBuffer, audioSampleSize * 2, 1.0f, mixAudioRMS, mixAudioMx);

   mixAudioRMS = toDB(mixAudioRMS);
   mixAudioMx = toDB(mixAudioMx);

   if (mixAudioMx > mMixMicAudioMax)
      mMixMicAudioMax = mixAudioMx;
   else
      mMixMicAudioMax = maxAlpha * mixAudioMx + (1.0f - maxAlpha) * mMixMicAudioMax;

   if (mMixMicAudioMax > mMixMicAudioPeak || audioFramesSinceMixaudioMaxUpdate > peakMeterDelayFrames) {
      mMixMicAudioPeak = mMixMicAudioMax;
      audioFramesSinceMixaudioMaxUpdate = 0;
   }
   else {
      audioFramesSinceMixaudioMaxUpdate += audioSampleSize;
   }
   mMixMicAudioMag = rmsAlpha * mixAudioRMS + mMixMicAudioMag * (1.0f - rmsAlpha);
}

bool AudioCapture::IsSaveMicOrignalAudioState() {
   bool bSave = false;
   OSEnterMutex(mSaveMicAudioMutex);
   bSave = m_bSaveMicOrignalAudio;
   OSLeaveMutex(mSaveMicAudioMutex);
   return bSave;
}

void AudioCapture::SetSaveMicAudio(bool bSave) {
   OSEnterMutex(mSaveMicAudioMutex);
   m_bSaveMicOrignalAudio = bSave;
   OSLeaveMutex(mSaveMicAudioMutex);
}

void AudioCapture::SetSampleRateHz(const int& iSampleRateHz)
{
   if (mSampleRateHz != iSampleRateHz)
   {
      OSEnterMutex(mAuxAudioMutex);

      if (NULL != mPlaybackAudio)
      {
         mPlaybackAudio->StopCapture();
         mPlaybackAudio->SetDstSampleRateHz(iSampleRateHz);
         mPlaybackAudio->Initialize();
      }
      if (NULL != mMicAudio)
      {
         mMicAudio->StopCapture();
         mMicAudio->SetDstSampleRateHz(iSampleRateHz);
         mMicAudio->Initialize();
      }
      for (UINT i = 0; i < auxAudioSources.Num(); i++) {
         if (NULL != auxAudioSources[i]) {
            auxAudioSources[i]->StopCapture();
            auxAudioSources[i]->SetDstSampleRateHz(iSampleRateHz);
            auxAudioSources[i]->Initialize();
         }
      }
      OSLeaveMutex(mAuxAudioMutex);
      //mSampleRateHz = iSampleRateHz;
   }
}
