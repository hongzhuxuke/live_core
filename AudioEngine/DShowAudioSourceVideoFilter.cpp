#include "Utility.h"     
#include "Logging.h"
#include "DShowAudioSourceVideoFilter.h"

DShowAudioSourceVideoFilter::DShowAudioSourceVideoFilter() :
IAudioSource(eAudioSource_DShowAudio),
   mInputChannels(2),
   mInputSamplesPerSec(48000),
   mInputBitsPerSample(2),
   mInputBlockSize(0),
   mSampleFrameCountIn100MS(0xFFFFFFFF),
   mSampleSegmentSizeIn100Ms(0xFFFFFFFF),
   mLastestAudioSampleTimeInBuffer(0),
   mStartTimeInMs(0),
   m_device(NULL)
{
   mDstSampleRateHz=44100;
   mAudioMutex = OSCreateMutex();
}

DShowAudioSourceVideoFilter::~DShowAudioSourceVideoFilter() {
   ReleaseDShowVideoFilter(this);
   if (mAudioMutex) {
      OSCloseMutex(mAudioMutex);
      mAudioMutex = NULL;
   }
}

bool DShowAudioSourceVideoFilter::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) {
   
   bool ret = false;
   if (mSampleBuffer.Num() >= mSampleSegmentSizeIn100Ms && mSampleBuffer.Num() > 0) {
      OSEnterMutex(mAudioMutex);
      memcpy(mOutputBuffer.Array(), mSampleBuffer.Array(), mSampleSegmentSizeIn100Ms);
      mSampleBuffer.RemoveRange(0, mSampleSegmentSizeIn100Ms);
      *buffer = mOutputBuffer.Array();
      *numFrames = mSampleFrameCountIn100MS;
      *timestamp = mLastestAudioSampleTimeInBuffer - mSampleBuffer.Num() / mSampleSegmentSizeIn100Ms;
      OSLeaveMutex(mAudioMutex);
      ret = true;
   }
   return ret;
}

void DShowAudioSourceVideoFilter::ReleaseBuffer() {

}

void DShowAudioSourceVideoFilter::AudioReceiveFun(unsigned char *data, 
      size_t size,
      long long startTime,
      long long stopTime,
      HANDLE handle)
{
   DShowAudioSourceVideoFilter *filter=(DShowAudioSourceVideoFilter *)handle;
   if(filter)
   {
      filter->AudioReceive(data,size,startTime,stopTime);
   }
}
void DShowAudioSourceVideoFilter::AudioReceive(
                                    unsigned char *data, size_t size,
                                    long long startTime, long long stopTime) {                                       
   if(mStartTimeInMs>stopTime)
   {
      mStartTimeInMs=0;
   }
   if (data) {
      OSEnterMutex(mAudioMutex);
      mSampleBuffer.AppendArray(data, size);

      if (mStartTimeInMs == 0) {
         mStartTimeInMs = GetQPCTimeMS() - stopTime / 10000;
      }
      mLastestAudioSampleTimeInBuffer = mStartTimeInMs + stopTime / 10000;
      OSLeaveMutex(mAudioMutex);
   }
}
bool DShowAudioSourceVideoFilter::Initialize(DeviceInfo deviceInfo) 
{
   OSEnterMutex(mAudioMutex);

   bool ret = false;
   bool  bFloat = false;
   DWORD inputChannelMask=0;
   ret=GetDShowVideoFilter(m_device,deviceInfo,VideoDeviceSetting(),DShowDeviceType_Video,DShowDevicePinType_Audio,this,
      DShowAudioSourceVideoFilter::AudioReceiveFun);
   if (ret) 
   {
      if(!ret)
      {
         OSLeaveMutex(mAudioMutex);
         return false;
      }

      m_device->GetAudioConfig(mInputBitsPerSample,
          bFloat,
          mInputSamplesPerSec,
          mInputChannels,
          mInputBlockSize);
    
      InitAudioData(bFloat, mInputChannels,
         mInputSamplesPerSec, mInputBitsPerSample, mInputBlockSize, inputChannelMask);
      mSampleFrameCountIn100MS = mInputSamplesPerSec / 100;
      mSampleSegmentSizeIn100Ms = mInputBlockSize*mSampleFrameCountIn100MS;

      mOutputBuffer.SetSize(mSampleSegmentSizeIn100Ms);

      OSLeaveMutex(mAudioMutex);
      return true;
   }
   
   OSLeaveMutex(mAudioMutex);
   return ret;
}
