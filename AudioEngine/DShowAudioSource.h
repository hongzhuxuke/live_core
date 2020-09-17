#ifndef _DSHOW_AUDIO_SOURCE_INCLUDE__
#define _DSHOW_AUDIO_SOURCE_INCLUDE__

#include "IAudioSource.h"
#include "dshowcapture.hpp"  
#include "BufferQueue.h"
#include <atomic>
using namespace  DShow;

#define MAX_REMOVE_SIZE 5
// #define DEBUG_DS_AUDIO_CAPTURE 1
class __declspec(dllexport)DShowAudioSource : public IAudioSource {
public:
   DShowAudioSource(const wchar_t* dshowDeviceName, const wchar_t* dshowDevicePath, UINT dstSampleRateHz/* = 44100*/);
   ~DShowAudioSource();
protected:
   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
   virtual void ReleaseBuffer();
   virtual void ClearAudioBuffer();
   virtual CTSTR GetDeviceName() const;


   static void AudioReceiveFun(const AudioConfig &config,
                               unsigned char *data, size_t size,
                               long long startTime, long long stopTime);
   void AudioReceive(const AudioConfig &config,
                     unsigned char *data, size_t size,
                     long long startTime, long long stopTime);

public:
   virtual bool Initialize();
   virtual void StartCapture();
   virtual void StopCapture();
   std::shared_ptr<DShow::AudioConfig> GetAudioConfig() {
     return mAudioConfig;
   };
private:
   Device* mAudioDevice = nullptr;
   String  mDShowAudioName;
   String  mDShowAudioPath;
   UINT  mInputChannels;
   UINT  mInputSamplesPerSec;
   UINT  mInputBitsPerSample;
   UINT  mInputBlockSize;
   std::shared_ptr<DShow::AudioConfig> mAudioConfig = nullptr;

   HANDLE mAudioMutex;
   List<BYTE> mSampleBuffer;
   List<BYTE> mOutputBuffer;
   UINT  mSampleFrameCountIn100MS;   // only return 100ms data at once
   UINT mSampleSegmentSizeIn100Ms;   // 100ms data size
   QWORD mLastestAudioSampleTimeInBuffer;
   std::atomic_ullong mStartTimeInMs;             // first audio paket time 
   QWORD mLastGetBufferTime;
   QWORD mLastSyncTime=0;

};

#endif 
