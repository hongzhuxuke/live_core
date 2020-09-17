#include "Utility.h"
#include "IDeckLinkDevice.h"
#include "MediaAudioSource.h"
#include "IMediaOutput.h"
#include "Logging.h"

MediaAudioSource::MediaAudioSource(IMediaOutput* mediaOutput, UINT dstSampleRateHz)
:IAudioSource(eAudioSource_Media),mMediaOutput(mediaOutput) {
   mDstSampleRateHz = dstSampleRateHz;
   mIsMediaOutInit = false;
   gLogger->logInfo("%s\n", __FUNCTION__);
}


MediaAudioSource::~MediaAudioSource() {
   gLogger->logInfo("%s\n",__FUNCTION__);
   mIsMediaOutInit = false;
}

void MediaAudioSource::ClearAudioBuffer() {
   //mSampleBuffer.Clear();
   if (mMediaOutput) {
      mMediaOutput->ResetPlayFileAudioData();
   }
}

void MediaAudioSource::SetCurrentBaseTime(QWORD baseTime) {
   if (mMediaOutput) {
      mMediaOutput->SetCurrentBaseTime(baseTime);
      mBaseTime = baseTime;
   }
}

bool MediaAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) {
   bool ret = false;
   if (mSampleBuffer.Num() < mSampleSegmentSizeIn10Ms) {
      void *newbuffer = NULL;
      UINT newNumFrames = 0;
	   uint64_t newTime = 0;
      bool tret = mMediaOutput->GetNextAudioBuffer(&newbuffer, &newNumFrames, &newTime);
      if (tret == true) {
         mSampleBuffer.AppendArray((BYTE*)newbuffer, mInputBlockSize*newNumFrames);
         mLastestAudioSampleTime = newTime;
      }
   }
   if (mSampleBuffer.Num() >= mSampleSegmentSizeIn10Ms) {
      memcpy(mOutputBuffer.Array(), mSampleBuffer.Array(), mSampleSegmentSizeIn10Ms);
      mSampleBuffer.RemoveRange(0, mSampleSegmentSizeIn10Ms);
      *buffer = mOutputBuffer.Array();
      *numFrames = mSampleFrameCountIn10MS;
      *timestamp = mLastestAudioSampleTime - mSampleBuffer.Num()*10 / mSampleSegmentSizeIn10Ms;
      ret = true;
   }
   return ret;
}
void MediaAudioSource::ReleaseBuffer() {

}
CTSTR MediaAudioSource::GetDeviceName() const {
   return NULL;
   /*if (mMediaOutput) {
      return mDeckLinkDevice->GetDeckLinkDeviceName();
   } else {
      return NULL;
   }*/
}

bool MediaAudioSource::Initialize() {
   bool ret = false;
   bool  bFloat = false;
   DWORD inputChannelMask;

   ret = mMediaOutput->GetAudioParam(mInputChannels, mInputSamplesPerSec, mInputBitsPerSample);
   if (ret == true) {
      mInputBlockSize = (mInputChannels*mInputBitsPerSample) / 8;
      inputChannelMask = 0;
      InitAudioData(bFloat, mInputChannels, mInputSamplesPerSec, mInputBitsPerSample, mInputBlockSize, inputChannelMask);
      mSampleFrameCountIn10MS = mInputSamplesPerSec / 100;
      mSampleSegmentSizeIn10Ms = mInputBlockSize*mSampleFrameCountIn10MS;
      mOutputBuffer.SetSize(mSampleSegmentSizeIn10Ms);
      mIsMediaOutInit = true;
      gLogger->logInfo("MediaAudioSource::Initialize init device sucess mInputChannels= %d mInputSamplesPerSec=%d mInputBitsPerSample=%d.",
                       mInputChannels, mInputSamplesPerSec, mInputBitsPerSample);
   } else {
      mIsMediaOutInit = false;
   }
   return ret;
}
