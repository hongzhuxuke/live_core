#include "Utility.h"
#include "IDeckLinkDevice.h"
#include "DeckLinkAudioSource.h"
#include "Logging.h"
void DeckLinkAudioSource::DeckLinkEventCallBack(DeckLinkDeviceEventEnum e,void *p,void *_this)
{
   DeckLinkAudioSource *__this=(DeckLinkAudioSource *)_this;
   if(__this)
   {
      __this->EventNotify(e,p);
   }

}
void DeckLinkAudioSource::ReInit()
{
   mLastSize=0;
   mFirstAudioTime=0;
   IDeckLinkDevice* deckLinkDevice = NULL;
   deckLinkDevice = GetDeckLinkDevice(mDeviceName,this);
   if (deckLinkDevice) {
      if(deckLinkDevice->IsCapturing())
      {
         deckLinkDevice->StopCapture();
      }
      if (deckLinkDevice->IsCapturing() == false) {
         deckLinkDevice->StartCapture();
      }
      OSEnterMutex(mDeckLinkDeviceMutex);
      mDeckLinkDevice=deckLinkDevice;
      if(mDeckLinkDevice){
         mDeckLinkDevice->EnableAudio(true);
      }
      OSLeaveMutex(mDeckLinkDeviceMutex);
      mSampleBuffer.Clear();
   } 
}
void DeckLinkAudioSource::EventNotify(DeckLinkDeviceEventEnum e,void *devWCharId)
{
   wchar_t *deviceId=(wchar_t *)devWCharId;
   if(mDeviceName==(CTSTR)deviceId)
   {
      switch(e)
      {
         case DeckLinkDeviceAdd:
            ReInit();
            break;
         case DeckLinkDeviceRemove:
            mLastSize=0;
            mFirstAudioTime=0;
            break;
         case DeckLinkDeviceImageFormatChanged:
            mLastSize=0;
            mFirstAudioTime=0;
            break;
         default:
            break;
      }
   }

}
DeckLinkAudioSource::DeckLinkAudioSource(wchar_t * deckLinkDeviceName, UINT dstSampleRateHz) 
:IAudioSource(eAudioSource_DeckLink),
mDeckLinkDevice(NULL)
{
   mDeviceName=deckLinkDeviceName;
   mDeckLinkDeviceMutex=OSCreateMutex();
   if(!mDeckLinkDeviceMutex)
   {
      gLogger->logError("DeckLinkAudioSource::DeckLinkAudioSource OSCreateMutex Failed!");
      return ;
   }

   IDeckLinkDevice* deckLinkDevice = NULL;
   deckLinkDevice = GetDeckLinkDevice(deckLinkDeviceName,this);
   if (deckLinkDevice) {
      if (deckLinkDevice->IsCapturing() == false) {
         deckLinkDevice->StartCapture();
      }
      else
      {

      }
      OSEnterMutex(mDeckLinkDeviceMutex);
      if(deckLinkDevice)
      {
         mDeckLinkDevice=deckLinkDevice;
         mDeckLinkDevice->EnableAudio(true);
         //mDeckLinkDevice->AudioReInit();
      }

      OSLeaveMutex(mDeckLinkDeviceMutex);
   } 
   else
   {
      gLogger->logError("DeckLinkAudioSource::DeckLinkAudioSource unknown decklink device");
   }

   mDstSampleRateHz = dstSampleRateHz;
   mIsDeckLinkSourceInit = false;

   mFirstAudioTime=0;
   mLastSize=0;
   
   mDeckLinkEventHandle=RegistterDeckLinkDeviceEvent(DeckLinkAudioSource::DeckLinkEventCallBack,this);
}


DeckLinkAudioSource::~DeckLinkAudioSource() {
   if(mDeckLinkEventHandle)
   {
      UnRegistterDeckLinkDeviceEvent(mDeckLinkEventHandle);
      mDeckLinkEventHandle=NULL;
   }
   
   
   mIsDeckLinkSourceInit = false;
   OSEnterMutex(mDeckLinkDeviceMutex);
   if(mDeckLinkDevice){
      mDeckLinkDevice->EnableAudio(false);
   }
   OSLeaveMutex(mDeckLinkDeviceMutex);   
   ReleaseDeckLinkDevice(mDeckLinkDevice,this);
}
static int VHallDebug(const char * format, ...)
{
#define LOGBUGLEN 1024
   char logBuf[LOGBUGLEN];
   va_list arg_ptr;
   va_start(arg_ptr, format);
   //int nWrittenBytes = sprintf_s(logBuf, format, arg_ptr);
   int nWrittenBytes = vsnprintf_s(logBuf,LOGBUGLEN ,format, arg_ptr);
   va_end(arg_ptr);
   WCHAR   wstr[LOGBUGLEN*2] = { 0 };
   MultiByteToWideChar(CP_ACP, 0, logBuf, -1, wstr, sizeof(wstr));
   OutputDebugString(wstr);
   return nWrittenBytes;
}

bool DeckLinkAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) {
   QWORD osTime=GetQPCTimeMS();
   bool ret = false;
   if (mSampleBuffer.Num() < mSampleSegmentSizeIn100Ms) {
      void *newbuffer = NULL;
      UINT newNumFrames = 0;
      QWORD newTime = 0;
      ret = mDeckLinkDevice->GetNextAudioBuffer(&newbuffer, &newNumFrames, &newTime);
      if (ret == true) {
         mSampleBuffer.AppendArray((BYTE*)newbuffer, mInputBlockSize*newNumFrames);
         
         // gLogger->logInfo("DeckLinkAudioSource::GetNextBuffer numFrames %d timestamp %I64d.", newNumFrames, newTime);
         mLastestAudioSampleTime = newTime;
         if (newTime ==0) {
            #ifdef _DEBUG
            assertmsg(newTime!=0,"TIMESTAMP FAILED!")
            #endif
            newTime = osTime;
         }


         static int logIndex=0;
         if(logIndex%300==0)
         {
            logIndex=0;
             gLogger->logInfo(TEXT("SIN %llu OS:%llu "),newTime,osTime);
         }
         logIndex++;
         
         //Log(TEXT("SIN %llu OS:%llu "),newTime,osTime);
      }
   }
   
   if (mSampleBuffer.Num() >= mSampleSegmentSizeIn100Ms) {
      memcpy(mOutputBuffer.Array(), mSampleBuffer.Array(), mSampleSegmentSizeIn100Ms);
      mSampleBuffer.RemoveRange(0, mSampleSegmentSizeIn100Ms);
      *buffer = mOutputBuffer.Array();
      *numFrames = mSampleFrameCountIn100MS;
      *timestamp = mLastestAudioSampleTime - mSampleBuffer.Num() / mSampleSegmentSizeIn100Ms;
      ret = true;
      static int logIndex=0;
      if(logIndex%300==0)
      {
         logIndex=0;
          gLogger->logInfo(TEXT("SOUT %llu OS:%llu SampleBufferSize:%u"),*timestamp,osTime,mSampleBuffer.Num());
      }
      logIndex++;
   }
   return ret;
}

void DeckLinkAudioSource::ReleaseBuffer() {

}
CTSTR DeckLinkAudioSource::GetDeviceName() const {
   OSEnterMutex(mDeckLinkDeviceMutex);
   if (mDeckLinkDevice) {
      const wchar_t*decklinkDeviceName = mDeckLinkDevice->GetDeckLinkDeviceName();
      OSLeaveMutex(mDeckLinkDeviceMutex);
      return decklinkDeviceName;
   } else {
      OSLeaveMutex(mDeckLinkDeviceMutex);
      return NULL;
   }
}

bool DeckLinkAudioSource::Initialize() {
   bool ret = false;
   bool  bFloat = false;
   DWORD inputChannelMask;
   OSEnterMutex(mDeckLinkDeviceMutex);
   if (!mDeckLinkDevice) {
      OSLeaveMutex(mDeckLinkDeviceMutex);
      return false;
   }
   ret = mDeckLinkDevice->GetAudioParam(mInputChannels, mInputSamplesPerSec, mInputBitsPerSample);

   OSLeaveMutex(mDeckLinkDeviceMutex);
   if (ret == true) {
      mInputBlockSize = (mInputChannels*mInputBitsPerSample) / 8;
      inputChannelMask = 0;
      InitAudioData(bFloat, mInputChannels, mInputSamplesPerSec, mInputBitsPerSample, mInputBlockSize, inputChannelMask);
      mSampleFrameCountIn100MS = mInputSamplesPerSec / 100;
      mSampleSegmentSizeIn100Ms = mInputBlockSize*mSampleFrameCountIn100MS;
      mOutputBuffer.SetSize(mSampleSegmentSizeIn100Ms);
      mIsDeckLinkSourceInit = true;
      gLogger->logInfo("DeckLinkAudioSource::Initialize init device sucess mInputChannels= %d mInputSamplesPerSec=%d mInputBitsPerSample=%d.",
                       mInputChannels, mInputSamplesPerSec, mInputBitsPerSample);
   } else {
      mIsDeckLinkSourceInit = false;
   }
   return ret;
}
