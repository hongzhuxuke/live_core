#include "WaveOutPlay.h"
#include "BufferQueue.h"
#include "Logging.h"
#include <mutex>
#pragma comment(lib, "winmm.lib")

std::mutex mAudioMutex;
std::atomic_bool bIsWaveOutReset = false;
std::atomic_bool mbIsStop = false;
std::atomic_bool mbIsVLCCallBackPlayEnd = false;
HANDLE gSectionHandle = NULL;

AudioData::AudioData(unsigned char* _data, long _len) {
   mData = new unsigned char[_len + 1];
   memset(mData, 0, _len + 1);
   memcpy(mData, _data, _len);
   mDataLen = _len;
}

AudioData::~AudioData() {
   delete[] mData;
   mData = NULL;
}



HANDLE OSCreateMutex()
{
   CRITICAL_SECTION *pSection = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
   InitializeCriticalSection(pSection);

   return (HANDLE)pSection;
}

void  OSEnterMutex(HANDLE hMutex)
{
   //assert(hMutex);
   EnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

BOOL OSTryEnterMutex(HANDLE hMutex)
{
   //assert(hMutex);
   return TryEnterCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSLeaveMutex(HANDLE hMutex)
{
   //assert(hMutex);
   LeaveCriticalSection((CRITICAL_SECTION*)hMutex);
}

void OSCloseMutex(HANDLE hMutex)
{
   //assert(hMutex);
   DeleteCriticalSection((CRITICAL_SECTION*)hMutex);
   free(hMutex);
}

WaveOutPlay::WaveOutPlay(const wchar_t* logPath /*= NULL*/){
   mbIsOpenPlayOutFunc = false;
   if (mpWaveLogger == NULL) {
      wchar_t lwzLogFileName[255] = { 0 };
      if (logPath == NULL) {
         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", L"\\vhlog\\", L"WaveOutPlay", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         mpWaveLogger = new Logger(lwzLogFileName, USER);
      }
      else {
         if (!CreateDirectoryW(logPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
            OutputDebugStringW(L"Logger::Logger: CreateDirectoryW failed.");
         }
         SYSTEMTIME loSystemTime;
         GetLocalTime(&loSystemTime);
         wsprintf(lwzLogFileName, L"%s%s_%4d_%02d_%02d_%02d_%02d%s", logPath, L"WaveOutPlay", loSystemTime.wYear, loSystemTime.wMonth, loSystemTime.wDay, loSystemTime.wHour, loSystemTime.wMinute, L".log");
         mpWaveLogger = new Logger(lwzLogFileName, None);
      }
   }
   gSectionHandle = OSCreateMutex();
}

WaveOutPlay::~WaveOutPlay(){
   StopPlay();
   if (mpWaveLogger) {
      delete mpWaveLogger;
      mpWaveLogger = NULL;
   }
   OSCloseMutex(gSectionHandle);
}


AudioData *WaveOutPlay::GetPlayAudio() {
   AudioData *playData = NULL;
   if (mLockFreePlayQueue.size_approx() > 0) {
      if (mLockFreePlayQueue.try_dequeue(playData)) {
         mLockFreeClearQueue.enqueue(playData);
         return playData;
      }
      else {
         return NULL;
      }
   }
   else {
      return NULL;
   }
}

void WaveOutPlay::CheckAudioHead(DWORD_PTR dw1, DWORD_PTR dw2) {
  
   LPWAVEHDR pHeader = (LPWAVEHDR)dw1;
   pHeader->dwUser = 0;

   MMRESULT ret = waveOutUnprepareHeader(mhWaveOut, pHeader, sizeof(WAVEHDR));//准备一个波形数据块用于播放
   if (ret != MMSYSERR_NOERROR) {
      mpWaveLogger->logInfo("%s waveOutUnprepareHeader err :%d", __FUNCTION__, ret);
   }

   AudioData *playData = GetPlayAudio();
   if (playData == NULL) {
      pHeader->lpData = NULL;
      pHeader->dwBufferLength = 0;
      pHeader->dwFlags = 0L;
      pHeader->dwLoops = 1L;
      pHeader->dwUser = 1;
   }
   else {
      pHeader->lpData = (char*)playData->mData;
      pHeader->dwBufferLength = playData->mDataLen;
      pHeader->dwFlags = 0L;
      pHeader->dwLoops = 1L;
      pHeader->dwUser = 1;
   }

   ret = waveOutPrepareHeader(mhWaveOut, pHeader, sizeof(WAVEHDR));//准备一个波形数据块用于播放
   if (ret != MMSYSERR_NOERROR) {
      mpWaveLogger->logInfo("%s waveOutPrepareHeader err :%d", __FUNCTION__, ret);
   }
   ret = waveOutWrite(mhWaveOut, pHeader, sizeof(WAVEHDR));//在音频媒体中播放第二个函数wh指定的数据
   if (ret != MMSYSERR_NOERROR) {
      mpWaveLogger->logInfo("%s waveOutWrite err :%d", __FUNCTION__, ret);
   }
   //此处数据需要先保存，否则数据被立刻清无法正常播放。
   while (mLockFreeClearQueue.size_approx() >= MAX_CLEAR_AUDIO_DATA_SIZE) {
      AudioData *deleteData = NULL;
      if (mLockFreeClearQueue.try_dequeue(deleteData) && deleteData) {
         delete deleteData;
      }
   }
}

void WaveOutPlay::Process(DWORD dw1, DWORD dw2) {
   if (mnPlayIndex == MAX_WAVE_PLAY_HEAD) {
      return;
   }

   AudioData *playData = GetPlayAudio();
   if (playData == NULL) {
      return;
   }   
   
   mWaveHeader[mnPlayIndex].lpData = (char*)playData->mData;
   mWaveHeader[mnPlayIndex].dwBufferLength = playData->mDataLen;
   mWaveHeader[mnPlayIndex].dwFlags = 0L;
   mWaveHeader[mnPlayIndex].dwLoops = 1L;
   mWaveHeader[mnPlayIndex].dwUser = 1;
   MMRESULT ret = waveOutPrepareHeader(mhWaveOut, &mWaveHeader[mnPlayIndex], sizeof(WAVEHDR));//准备一个波形数据块用于播放
   if (ret != MMSYSERR_NOERROR) {
      mpWaveLogger->logInfo("%s waveOutPrepareHeader err :%d", __FUNCTION__, ret);
   }
   ret = waveOutWrite(mhWaveOut, &mWaveHeader[mnPlayIndex], sizeof(WAVEHDR));//在音频媒体中播放第二个函数wh指定的数据
   if (ret != MMSYSERR_NOERROR) {
      mpWaveLogger->logInfo("%s waveOutWrite err :%d", __FUNCTION__, ret);
   }

   mnPlayIndex++;
   //此处数据需要先保存，否则数据被立刻清无法正常播放。
   while (mLockFreeClearQueue.size_approx() >= MAX_CLEAR_AUDIO_DATA_SIZE) {
      AudioData *deleteData = NULL;
      if (mLockFreeClearQueue.try_dequeue(deleteData) && deleteData) {
         delete deleteData;
      }
   }
}

void PASCAL OneAudioMixProc(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dwl, DWORD dw2)
{
   WaveOutPlay* pObj = (WaveOutPlay*)dwUser;
   pObj->Process(dwl, dw2);
}

VOID CALLBACK WaveCallbackFunc(HWAVEOUT hwo, UINT32 uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
   OSEnterMutex(gSectionHandle);
   if (uMsg == WOM_DONE && !bIsWaveOutReset && mbIsVLCCallBackPlayEnd) {
      WaveOutPlay* obj = (WaveOutPlay*)dwInstance;
      obj->CheckAudioHead(dwParam1, dwParam2);
   }
   OSLeaveMutex(gSectionHandle);
}

void WaveOutPlay::InitFormat(int sampleRate, int channel, int bitPerSample) {
   mpWaveLogger->logInfo("%s START",__FUNCTION__);
   StopPlay();
   std::unique_lock<std::mutex> lock(mAudioMutex);
   mWaveFormate.wFormatTag = WAVE_FORMAT_PCM;
   mWaveFormate.nChannels = channel;
   mWaveFormate.nSamplesPerSec = sampleRate;
   mWaveFormate.wBitsPerSample = bitPerSample;
   mWaveFormate.cbSize = 0;
   mWaveFormate.nBlockAlign = mWaveFormate.wBitsPerSample * mWaveFormate.nChannels / 8;
   mWaveFormate.nAvgBytesPerSec = mWaveFormate.nChannels * mWaveFormate.nSamplesPerSec * mWaveFormate.wBitsPerSample / 8;
   //MMRESULT ret = waveOutOpen(&mhWaveOut, WAVE_MAPPER, &mWaveFormate, (DWORD_PTR)mWaitPlayCallBack, 0L, CALLBACK_EVENT);
   MMRESULT ret = waveOutOpen(&mhWaveOut, WAVE_MAPPER, &mWaveFormate, (DWORD_PTR)WaveCallbackFunc, (DWORD_PTR)this, CALLBACK_FUNCTION);
   if (ret == MMSYSERR_NOERROR) {
      mbIsStop = false;
      //mPlayOutThread = CreateThread(NULL, 0, PlayAudioThread, this, 0, NULL);
      bIsWaveOutReset = false;
      for (int i = 0; i < MAX_WAVE_PLAY_HEAD; i++) {
         mWaveHeader[i].dwUser = 0;
         waveOutPrepareHeader(mhWaveOut, &mWaveHeader[i], sizeof(WAVEHDR));//准备一个波形数据块用于播放
      }
   }
   else {
      mpWaveLogger->logInfo("waveOutOpen err :%d", ret);
   }
   mSampleRate = sampleRate;
   mBitPerSample = bitPerSample;
   mChannels = channel;

   if (mTimeEventId) {
      timeKillEvent(mTimeEventId);
      mTimeEventId = 0;
   }
   mpWaveLogger->logInfo("%s END", __FUNCTION__);
}

void WaveOutPlay::ResetInitFormat(int sampleRate, int channel, int bitPerSample) {
   {
      std::unique_lock<std::mutex> lock(mAudioMutex);
      if (sampleRate == mSampleRate && channel == mChannels && bitPerSample == mBitPerSample && mhWaveOut != NULL) {
         return;
      }
   }
   InitFormat(sampleRate, channel, bitPerSample);
}

uint64_t GetTimestampMs() {
   auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   return ms.count();
}

void WaveOutPlay::PlayAudio(unsigned char* data, long dataLen) {
   if (!mbIsOpenPlayOutFunc) {
      return;
   }
   if (data == NULL || dataLen <= 0) {
      return;
   }

   
   if (mbIsStop) {
      return;
   }
   std::unique_lock<std::mutex> lock(mAudioMutex);
   if (mhWaveOut == NULL) {
      return;
   }

   OSEnterMutex(gSectionHandle);
   if (bIsWaveOutReset) {
      OSLeaveMutex(gSectionHandle);
      return;
   }
   OSLeaveMutex(gSectionHandle);

   //音频会早于视频200ms，为了播放同步先缓冲部分音频数据，然后再进行播放。
   if (!bIsCacheMaxPlayAudio && mLockFreePlayQueue.size_approx() < MAX_CACHE_AUDIO_SIZE) {
      bIsCacheMaxPlayAudio = false;
      AudioData *dataAudio = new AudioData(data, dataLen);
      mLockFreePlayQueue.enqueue(dataAudio);
      return;
   }
   else {
      bIsCacheMaxPlayAudio = true;
      AudioData *dataAudio = new AudioData(data, dataLen);
      mLockFreePlayQueue.enqueue(dataAudio);
   }
   OSEnterMutex(gSectionHandle);
   mbIsVLCCallBackPlayEnd = false;
   OSLeaveMutex(gSectionHandle);
   for (int i = 0; i < MAX_WAVE_PLAY_HEAD; i++) {
      Process(0, 0);
   }
   OSEnterMutex(gSectionHandle);
   mbIsVLCCallBackPlayEnd = true;
   OSLeaveMutex(gSectionHandle);
   return;
}

bool WaveOutPlay::IsHasWaveHandle() {
   bool bExist = false;
   std::unique_lock<std::mutex> lock(mAudioMutex);
   if (mbIsOpenPlayOutFunc != NULL) {
      bExist = true;
   }
   return bExist;
}

void WaveOutPlay::SetPlayVolume(int vol) {
   std::unique_lock<std::mutex> lock(mAudioMutex);
   if (mhWaveOut) {
      waveOutSetVolume(mhWaveOut, vol);
   }
}

void WaveOutPlay::WaveOutPlayReset() {
   mpWaveLogger->logInfo("%s start", __FUNCTION__);
   OSEnterMutex(gSectionHandle);
   bIsWaveOutReset = true;
   OSLeaveMutex(gSectionHandle);
   mnPlayIndex = 0;
   bIsCacheMaxPlayAudio = false;
   {
      std::unique_lock<std::mutex> lock(mAudioMutex);
      if (mhWaveOut != NULL) {
         mpWaveLogger->logInfo("%s waveOutReset start", __FUNCTION__);
         MMRESULT ret = ::waveOutReset(mhWaveOut);
         if (ret != MMSYSERR_NOERROR) {
            mpWaveLogger->logInfo("%s waveOutReset err", __FUNCTION__);
         }
         mpWaveLogger->logInfo("%s waveOutReset end", __FUNCTION__);
         for (int i = 0; i < MAX_WAVE_PLAY_HEAD; i++) {
            waveOutUnprepareHeader(mhWaveOut, &mWaveHeader[i], sizeof(WAVEHDR));//准备一个波形数据块用于播放
            mWaveHeader[i].dwUser = 0;
         }
         mpWaveLogger->logInfo("%s waveOutUnprepareHeader end", __FUNCTION__);
      }
   }
   ClearAudioData();
   bIsWaveOutReset = false;
   mpWaveLogger->logInfo("%s end", __FUNCTION__);
}

void WaveOutPlay::StopPlay() {
   mpWaveLogger->logInfo("%s start", __FUNCTION__);
   OSEnterMutex(gSectionHandle);
   bIsWaveOutReset = true; //确保回调不调用。
   OSLeaveMutex(gSectionHandle);
   mbIsStop = true;
   bIsCacheMaxPlayAudio = false;
   mpWaveLogger->logInfo("%s", __FUNCTION__);
   mnPlayIndex = 0;
   {
      std::unique_lock<std::mutex> lock(mAudioMutex); //加锁确保与vlc回调，调用PlayAudio异步。
      if (mhWaveOut != NULL) {
         mpWaveLogger->logInfo("%s waveOutReset start", __FUNCTION__);
         MMRESULT ret = ::waveOutReset(mhWaveOut);
         if (ret != MMSYSERR_NOERROR) {
            mpWaveLogger->logInfo("%s waveOutReset err", __FUNCTION__);
         }
         mpWaveLogger->logInfo("%s waveOutReset end", __FUNCTION__);
         for (int i = 0; i < MAX_WAVE_PLAY_HEAD; i++) {
            waveOutUnprepareHeader(mhWaveOut, &mWaveHeader[i], sizeof(WAVEHDR));//准备一个波形数据块用于播放
            mWaveHeader[i].dwUser = 0;
         }
         ret = ::waveOutClose(mhWaveOut);
         if (ret != MMSYSERR_NOERROR) {
            mpWaveLogger->logInfo("%s waveOutClose err :%d", __FUNCTION__, ret);
         }
         mpWaveLogger->logInfo("%s waveOutClose end", __FUNCTION__);
         mhWaveOut = NULL;
      }
   }
   ClearAudioData();
   mpWaveLogger->logInfo("%s end", __FUNCTION__);
}

void WaveOutPlay::ClearAudioData() {
   while (mLockFreeClearQueue.size_approx() > 0) {
      AudioData *deleteData = NULL;
      if (mLockFreeClearQueue.try_dequeue(deleteData) && deleteData) {
         delete deleteData;
      }
   }

   while (mLockFreePlayQueue.size_approx() > 0) {
      AudioData *deleteData = NULL;
      if (mLockFreePlayQueue.try_dequeue(deleteData) && deleteData) {
         delete deleteData;
      }
   }
}

void WaveOutPlay::SetEnablePlayOut(bool enable) {
   mbIsOpenPlayOutFunc = enable;
   if (!mbIsOpenPlayOutFunc) {
      StopPlay();
   }
}
