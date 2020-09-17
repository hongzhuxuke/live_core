#ifndef _DSHOW_AUDIO_SOURCE_VIDEO_FILTER_INCLUDE__
#define _DSHOW_AUDIO_SOURCE_VIDEO_FILTER_INCLUDE__

#include "IAudioSource.h"
#include "IDShowPlugin.h"
#include "dshowcapture.hpp"
class DShowAudioSourceVideoFilter : public IAudioSource {
public:
   DShowAudioSourceVideoFilter();
   ~DShowAudioSourceVideoFilter();   
   virtual CTSTR GetDeviceName()const{return m_deviceInfo.m_sDeviceDisPlayName;}
protected:
   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
   virtual void ReleaseBuffer();
   void AudioReceive(
      unsigned char *data, 
      size_t size,
      long long startTime,
      long long stopTime);
   static void AudioReceiveFun(unsigned char *data, 
      size_t size,
      long long startTime,
      long long stopTime,
      HANDLE handle);
public:
   virtual bool Initialize(DeviceInfo);

private:
   UINT  mInputChannels;
   UINT  mInputSamplesPerSec;
   UINT  mInputBitsPerSample;
   UINT  mInputBlockSize;

   HANDLE mAudioMutex;
   List<BYTE> mSampleBuffer;
   
   List<BYTE> mOutputBuffer;
   
   UINT  mSampleFrameCountIn100MS;   // only return 100ms data at once
   UINT mSampleSegmentSizeIn100Ms;   //100ms data size
   QWORD mLastestAudioSampleTimeInBuffer;
   QWORD mStartTimeInMs;             //first audio paket time 
   IDShowVideoFilterDevice *m_device = nullptr;
   DeviceInfo m_deviceInfo;
};

#endif 
