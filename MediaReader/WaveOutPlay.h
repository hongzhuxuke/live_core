#pragma once
#include <Windows.h>
#include <mmsystem.h>
#include <list>
#include <atomic>
#include <vector>
#include "concurrentqueue.h"

#define LOOP_SIZE 1

class Logger;

class AudioData {
public:
   AudioData(unsigned char* _data, long _len);
   ~AudioData();

   unsigned char* mData;
   long mDataLen;
};

#define MAX_CACHE_AUDIO_SIZE 10    //vlc音频数据会提前与视频数据200ms左右，所以需要先缓存音频数据，否则出现音视频不同步现象。
#define MAX_WAVE_PLAY_HEAD 4       //多播放缓冲区
#define MAX_CLEAR_AUDIO_DATA_SIZE 100

class WaveOutPlay
{
public:
   WaveOutPlay(const wchar_t* logPath = NULL);
   ~WaveOutPlay();

   void InitFormat(int sampleRate, int channel, int bitPerSample);
   void ResetInitFormat(int sampleRate, int channel, int bitPerSample);
   void PlayAudio(unsigned char* data, long dataLen);
   void StopPlay();
   void SetEnablePlayOut(bool enable);
   void WaveOutPlayReset();
   void SetPlayVolume(int vol);
   bool IsHasWaveHandle();
   void Process(DWORD dw1, DWORD dw2);
   void CheckAudioHead(DWORD_PTR dw1, DWORD_PTR dw2);

private:
   void ClearAudioData();
   AudioData *GetPlayAudio();
private:
   UINT mTimeEventId = 0;
   HWAVEOUT       mhWaveOut = NULL;
   WAVEFORMATEX   mWaveFormate;
   HANDLE          mWaitPlayCallBack = NULL;
   HANDLE          mPlayOutThread = NULL;
   WAVEHDR         mWaveHeader[4];

   std::atomic_int mnPlayIndex = 0;
   std::atomic_bool mbDirectPlay = false;
   std::atomic_bool mbIsOpenPlayOutFunc = false;
   std::atomic_bool bIsCacheMaxPlayAudio = false;

   unsigned int   mChannels = 0;
   unsigned int   mSampleRate = 0;
   unsigned int   mBitPerSample = 0; //bits per sample 
   Logger *mpWaveLogger = NULL;
   //git下载地址https://github.com/cameron314/concurrentqueue
   moodycamel::ConcurrentQueue<AudioData*> mLockFreePlayQueue;//原子操作队列。
   moodycamel::ConcurrentQueue<AudioData*> mLockFreeClearQueue;//原子操作队列。
};

