#ifndef _DECK_LINK_AUDIO_SOURCE_INCLUDE__
#define _DECK_LINK_AUDIO_SOURCE_INCLUDE__

#include "IAudioSource.h"
class IDeckLinkDevice;
class __declspec(dllexport)DeckLinkAudioSource : public IAudioSource {
public:
   DeckLinkAudioSource(wchar_t * deckLinkDeviceName, UINT dstSampleRateHz/* = 44100*/);
   ~DeckLinkAudioSource();
   static void DeckLinkEventCallBack(DeckLinkDeviceEventEnum e,void *p,void *_this);
   void EventNotify(DeckLinkDeviceEventEnum e,void *devWCharId);
   void ReInit();
   
   virtual bool isDecklinkDevice(){return true;}
protected:
   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
   virtual void ReleaseBuffer();
   virtual CTSTR GetDeviceName() const;
public:
   virtual bool Initialize();
private:
   IDeckLinkDevice* mDeckLinkDevice = nullptr;
   HANDLE mDeckLinkDeviceMutex;
   UINT  mInputChannels;
   UINT  mInputSamplesPerSec;
   UINT  mInputBitsPerSample;
   UINT  mInputBlockSize;
   bool  mIsDeckLinkSourceInit;

   List<BYTE> mSampleBuffer;
   List<BYTE> mOutputBuffer;
   UINT  mSampleFrameCountIn100MS;   // only return 100ms data at once
   UINT mSampleSegmentSizeIn100Ms;   // 100ms data size
   QWORD mLastestAudioSampleTime;
   QWORD mFirstAudioTime;
   String mDeviceName ;
   void *mDeckLinkEventHandle = nullptr;
   int mLastSize;
};

#endif 
