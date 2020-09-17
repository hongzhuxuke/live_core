#ifndef _MEDIA_AUDIO_SOURCE_INCLUDE__
#define _MEDIA_AUDIO_SOURCE_INCLUDE__

#include "IAudioSource.h"
class IMediaOutput;
class MediaAudioSource : public IAudioSource {
public:
   MediaAudioSource(IMediaOutput* deckLinkDevice, UINT dstSampleRateHz/* = 44100*/);
   ~MediaAudioSource();
   virtual void ClearAudioBuffer();
   virtual void SetCurrentBaseTime(QWORD baseTime) ;
protected:
   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
   virtual void ReleaseBuffer();
   virtual CTSTR GetDeviceName() const;
public:
   virtual bool Initialize();
private:
   IMediaOutput* mMediaOutput;
   UINT  mInputChannels;
   UINT  mInputSamplesPerSec;
   UINT  mInputBitsPerSample;
   UINT  mInputBlockSize;
   bool  mIsMediaOutInit;

   List<BYTE> mSampleBuffer;
   List<BYTE> mOutputBuffer;
   UINT  mSampleFrameCountIn10MS;   // only return 10ms data at once
   UINT mSampleSegmentSizeIn10Ms;   //100ms data size
   QWORD mLastestAudioSampleTime;
   int mLogCount = 0;
   QWORD mBaseTime = 0;
};

#endif 
