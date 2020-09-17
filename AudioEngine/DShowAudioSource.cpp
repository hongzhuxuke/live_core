#include "Utility.h"     
#include "Logging.h"
#include "DShowAudioSource.h"
void OBSApiUIWarning(wchar_t *d);
void dslog(DShow::LogType type, const wchar_t *msg,
           void *param) {
   g_pLogger->logInfo(msg);
}
#ifdef DEBUG_DS_AUDIO_CAPTURE
FILE*    mRawFile;
#endif

#define AUDIO_DEBUG(X,Y) {static int logIndex = 0;if(logIndex %X == 0) {logIndex = 0;Y;}logIndex++;}

DShowAudioSource::DShowAudioSource(const wchar_t* dshowDeviceName, const wchar_t* dshowDevicePath, UINT dstSampleRateHz) :
IAudioSource(eAudioSource_DShowAudio),
mAudioDevice(NULL),
mDShowAudioName(""),
mDShowAudioPath(""),
mInputChannels(2),
mInputSamplesPerSec(48000),
mInputBitsPerSample(2),
mInputBlockSize(0),
mSampleFrameCountIn100MS(0),
mSampleSegmentSizeIn100Ms(0),
mLastestAudioSampleTimeInBuffer(0),
mStartTimeInMs(0) ,
mLastGetBufferTime(0){

   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::DShowAudioSource \n");
   mDstSampleRateHz = dstSampleRateHz;
   if (dshowDeviceName)
      mDShowAudioName = dshowDeviceName;
   if (dshowDevicePath)
      mDShowAudioPath = dshowDevicePath;
   mAudioDevice = NULL;
   mAudioMutex = OSCreateMutex();
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::DShowAudioSource End\n");
}

DShowAudioSource::~DShowAudioSource() {
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::~DShowAudioSource()\n");
   if (mAudioDevice) {
      
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Info,L"DShowAudioSource::~DShowAudioSource() mAudioDevice stop\n");
      mAudioDevice->Stop();
      
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Info,L"DShowAudioSource::~DShowAudioSource() delete mAudioDevice\n");
      delete mAudioDevice;
      mAudioDevice = NULL;
   }

   if (mAudioMutex) {
      OSCloseMutex(mAudioMutex);
      mAudioMutex = NULL;
   }
   
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::~DShowAudioSource() End\n");
}
bool DShowAudioSource::GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) {

#define AUDIO_DEBUG(X,Y) {static int logIndex = 0;if(logIndex %X == 0) {logIndex = 0;Y;}logIndex++;}
	//每隔 400 次 打印日志一次
   if(mLastGetBufferTime==0){
      mLastGetBufferTime = GetQPCTimeMS();      
      AUDIO_DEBUG(1,DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Info,L"DShowAudioSource::GetNextBuffer mLastGetBufferTime = 0\n"));   
   }

   QWORD currentTimeMs = GetQPCTimeMS();
   if(mLastGetBufferTime!=0)
   {
      if(currentTimeMs > 5000 + mLastGetBufferTime)//最后一次获取数据是5s前
      {
         AUDIO_DEBUG(1,DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Warning,L"DShowAudioSource::GetNextBuffer currentTimeMs(%llu)>5000+mLastGetBufferTime(%llu)\n",currentTimeMs,mLastGetBufferTime));   
         OBSApiUIWarning(L"声音采集异常，正在重新加载设备");
         if(mAudioDevice)
         {      
            AUDIO_DEBUG(1,DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Warning,L"DShowAudioSource::GetNextBuffer mAudioDevice->ReStart\n"));   
            mStartTimeInMs=0;
            mLastGetBufferTime = 0;
            mAudioDevice->ReStart();            
         }
      }
   }
   bool ret = false;
   if (mSampleBuffer.Num() >= mSampleSegmentSizeIn100Ms&&mSampleSegmentSizeIn100Ms != 0) {
      OSEnterMutex(mAudioMutex);
      memcpy(mOutputBuffer.Array(), mSampleBuffer.Array(), mSampleSegmentSizeIn100Ms);
      mSampleBuffer.RemoveRange(0, mSampleSegmentSizeIn100Ms);
      *buffer = mOutputBuffer.Array();
      *numFrames = mSampleFrameCountIn100MS;
      *timestamp = mLastestAudioSampleTimeInBuffer - mSampleBuffer.Num() / mSampleSegmentSizeIn100Ms;
      mLastGetBufferTime = GetQPCTimeMS();
      ret = true;
      OSLeaveMutex(mAudioMutex);
      AUDIO_DEBUG(1000, DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Debug, L"DShowAudioSource::GetNextBuffer\n"));
   }
   return ret;
}
void DShowAudioSource::ReleaseBuffer() {

}

void DShowAudioSource::ClearAudioBuffer() {
   OSEnterMutex(mAudioMutex);
   int num = mSampleBuffer.Num();
   mSampleBuffer.Clear();
   mStartTimeInMs = 0;
   OSLeaveMutex(mAudioMutex);
}

CTSTR DShowAudioSource::GetDeviceName() const {
   return mDShowAudioName.Array();
}

void DShowAudioSource::StartCapture() {
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::StartCapture()\n");
   g_pLogger->logInfo("DShowAudioSource::StartCapture will start audio device capture");
   if (mAudioDevice){      
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Info,L"DShowAudioSource::StartCapture() mAudioDevice->Start\n");
      mAudioDevice->Start();
   }
   
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::StartCapture() end\n");
}
void DShowAudioSource::StopCapture() {
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::StopCapture()\n");
   if (mAudioDevice) {
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Info,L"DShowAudioSource::StopCapture() mAudioDevice->Stop\n");
      mAudioDevice->Stop();
   }
#ifdef DEBUG_DS_AUDIO_CAPTURE   
   fclose(mRawFile);
#endif  
   g_pLogger->logInfo("DShowAudioSource::StopCapture");
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::StopCapture() End\n");
}

void DShowAudioSource::AudioReceiveFun(const AudioConfig &config,
                                       unsigned char *data, size_t size,
                                       long long startTime, long long stopTime) {
   DShowAudioSource* dsAudioSource = (DShowAudioSource*)config.context;
   if (dsAudioSource) {
      AUDIO_DEBUG(1000,DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Info,L"DShowAudioSource::AudioReceiveFun dsAudioSource->AudioReceive\n"));
      dsAudioSource->AudioReceive(config, data, size, startTime, stopTime);
   }
}


int pcm_db_count(const unsigned char* ptr, size_t size)  
{  
    int ndb = 0;  
  
    short int value;  
  
    int i;  
    long v = 0;  
    for(i=0; i<size; i+=2)  
    {     
        memcpy((char*)&value, ptr+i, 1);   
        memcpy((char*)&value+1, ptr+i+1, 1);   
        v += abs(value);  
    }     
  
    v = v/(size/2);  
  
    if(v != 0) {  
        ndb = (int)(20.0*log10((double)v / 65535.0 ));   
    }     
    else {  
        ndb = -96;  
    }     
  
    return ndb;  
}  

void DShowAudioSource::AudioReceive(const AudioConfig &config,
                                    unsigned char *data, size_t size,
                                    long long startTime, long long stopTime) {
   if (data) {
      int db = pcm_db_count(data,size);
      AUDIO_DEBUG(400,DShowLog(DShowLogType_Level2_ALL,DShowLogLevel_Debug,L"DShowAudioSource::AudioReceive PCM volumn %d\n",db));
      AUDIO_DEBUG(400,DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Debug,L"DShowAudioSource::AudioReceive data is not NULL\n"));
      QWORD currentTime=GetQPCTimeMS();
      if (mStartTimeInMs == 0 || config.isReset==true) {  
         DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Info,L"DShowAudioSource::AudioReceive Reset %d\n",config.isReset==true?1:0);
         mStartTimeInMs = currentTime - stopTime / 10000;         
         OSEnterMutex(mAudioMutex);
         mSampleBuffer.Clear();
         OSLeaveMutex(mAudioMutex);

         //if (mLastSyncTime > 0 && currentTime < mLastSyncTime + 5000) {
         //   OBSApiUIWarning(L"正在重新加载设备...");
         //   g_pLogger->logInfo("DShowAudioSource::AudioReceive  capture err currentTime<mLastSyncTime+5000\n");
         //   DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Error,L"DShowAudioSource::AudioReceive tip reset\n");
         //}
         mLastSyncTime=currentTime;
      }
      mLastestAudioSampleTimeInBuffer = mStartTimeInMs + stopTime / 10000;
      //DShowLog(DShowLogType_Level1_Receive, DShowLogLevel_Info, L"DShowAudioSource::AudioReceive stopTime %lld\n", stopTime);
      if(currentTime > mLastestAudioSampleTimeInBuffer + 200){
         DShowLog(DShowLogType_Level1_Receive,DShowLogLevel_Warning,L"DShowAudioSource::AudioReceive currentTime %llu mLastestAudioSampleTimeInBuffer %llu\n",currentTime,mLastestAudioSampleTimeInBuffer);
         mStartTimeInMs=0;
      }
      OSEnterMutex(mAudioMutex);
      mSampleBuffer.AppendArray(data, size);
      mSampleBuffer.Num();
      OSLeaveMutex(mAudioMutex);
   }
}

bool DShowAudioSource::Initialize() {
	if (mAudioDevice) {
		DShowLog(DShowLogType_Level3_AudioSource, DShowLogLevel_Info, L"DShowAudioSource::~DShowAudioSource() mAudioDevice stop\n");
		mAudioDevice->Stop();
		DShowLog(DShowLogType_Level3_AudioSource, DShowLogLevel_Info, L"DShowAudioSource::~DShowAudioSource() delete mAudioDevice\n");
		delete mAudioDevice;
		mAudioDevice = NULL;
	}

   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::Initialize()\n");
   bool ret = false;
   bool  bFloat = false;
   DWORD inputChannelMask;
   mAudioConfig = std::make_shared<DShow::AudioConfig>();

   VHLogDebug("DShowAudioSource::Initialize\n");
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::Initialize() new DShow::Device\n");

   mAudioDevice = new DShow::Device(InitGraph::True);
   mStartTimeInMs = 0;
   mAudioConfig->callback = AudioReceiveFun;
   mAudioConfig->deviceType=DShowDeviceType_Audio;
   mAudioConfig->pinType=DShowDevicePinType_Audio;
   mAudioConfig->sampleRate = 48000;
   mAudioConfig->channels = 2;
   mAudioConfig->context = this;
   mAudioConfig->format = AudioFormat::Wave16bit;
   mAudioConfig->useDefaultConfig = false;
   mAudioConfig->name = mDShowAudioName.IsEmpty() ? L"" : mDShowAudioName;
   mAudioConfig->path = mDShowAudioPath.IsEmpty() ? L"" : mDShowAudioPath;

#ifdef DEBUG_DS_AUDIO_CAPTURE
   // mAudioEncoder = CreateAACEncoder(32, GetSampleRateHz(), NumAudioChannels());
   //  mAacFile = fopen("d:\\aac.aac", "wb");
   mRawFile = fopen("d:\\DSAudioCapture.pcm", "wb");
#endif 
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::Initialize() mAudioDevice->SetAudioConfig\n");
   g_pLogger->logInfo("DShowAudioSource::Initialize will SetAudioConfig");
   ret = mAudioDevice->SetAudioConfig(mAudioConfig.get());
   if (ret == false) {
      gLogger->logInfo("DShowAudioSource::Initialize init SetAudioConfig failed");      
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Warning,L"DShowAudioSource::Initialize() mAudioDevice->SetAudioConfig Failed!\n");
      return false;
   }
   
   DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::Initialize() ConnectFilters\n");
   gLogger->logInfo("DShowAudioSource::Initialize will ConnectFilters");
	//mLastSyncTime = GetQPCTimeMS();
   ret = mAudioDevice->ConnectFilters();
   if (ret == false) {      
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Warning,L"DShowAudioSource::Initialize() ConnectFilters Failed!\n");
      gLogger->logInfo("DShowAudioSource::Initialize init SetAudioConfig failed");
      return false;
   }

   if (ret) {
      /*  mAudioDevice->GetAudioConfig(audioConfig);
        if (ret == false) {
        gLogger->logInfo("DShowAudioSource::Initialize init SetAudioConfig failed");
        return false;
        }*/
      if (mAudioConfig->format == AudioFormat::WaveFloat) {
         mInputBitsPerSample = 32;
         bFloat = true;
      } else if (mAudioConfig->format == AudioFormat::Wave16bit) {
         mInputBitsPerSample = 16;
         bFloat = false;
      }
      mInputSamplesPerSec = mAudioConfig->sampleRate;
      mInputChannels = mAudioConfig->channels;
      mInputBlockSize = (mAudioConfig->channels*mInputBitsPerSample) / 8;
      inputChannelMask = 0;
      InitAudioData(bFloat, mInputChannels, mInputSamplesPerSec, mInputBitsPerSample, mInputBlockSize, inputChannelMask);
      mSampleFrameCountIn100MS = mInputSamplesPerSec / 100;
      mSampleSegmentSizeIn100Ms = mInputBlockSize*mSampleFrameCountIn100MS;
      mOutputBuffer.SetSize(mSampleSegmentSizeIn100Ms);
      gLogger->logInfo("DShowAudioSource::Initialize init device sucess mInputChannels= %d mInputSamplesPerSec=%d mInputBitsPerSample=%d.",
                       mInputChannels, mInputSamplesPerSec, mInputBitsPerSample);
      DShowLog(DShowLogType_Level3_AudioSource,DShowLogLevel_Debug,L"DShowAudioSource::Initialize() End\n");

      return true;
   }
   return ret;
}