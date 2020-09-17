#ifndef __CORE_AUDIO_SOURCE_INCLUDE__
#define __CORE_AUDIO_SOURCE_INCLUDE__

#include "IAudioSource.h"
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <propsys.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <atomic>

//#define DEBUG_CORE_AUDIO_SOURCE      1 
typedef  QWORD(*FUN_GetVideoTime)();

class CBlankAudioPlayback {
public:
   CBlankAudioPlayback(CTSTR lpDevice);
   ~CBlankAudioPlayback();
private:
   IMMDeviceEnumerator *mmEnumerator;
   IMMDevice           *mmDevice;
   IAudioClient        *mmClient;
   IAudioRenderClient  *mmRender;
};
//BlankAudioPlayback

//Core Audio
class CoreAudioSource : public IAudioSource {
public:
   CoreAudioSource(bool useInputDevice, FUN_GetVideoTime funGetVideoTime, UINT dstSampleRateHz/* = 44100*/, bool useQPC = false, bool useVideoTime = false, int globalAdjust = 0);
   ~CoreAudioSource();
   QWORD GetTimestampInMs(QWORD qpcTimestamp);
   bool Reinitialize();
   void FreeData();
   HRESULT GetErrorType() ;
protected:
   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp);
   virtual void ReleaseBuffer();


   void StartBlankSoundPlayback(CTSTR lpDevice);
   void StopBlankSoundPlayback();
public:
	bool Initialize();
   bool Initialize(bool bMic, CTSTR lpID);
   void Reset();
   virtual void StartCapture();
   virtual void StopCapture();
private:
   CTSTR GetDeviceName() const;
private:
   bool     mUseInputDevice;  // this device is input device

   IMMDeviceEnumerator *mmEnumerator = nullptr;//多媒体设备枚举器
   IMMDevice           *mmDevice = nullptr;//声卡接口   在多媒体设备枚举器 中 获取
   IAudioClient        *mmClient = nullptr; // 声卡客户端  可以通过 声卡接口获取， （声卡输出的音频参数、初始化声卡、获取声卡输出缓冲区的大小、开启/停止对声卡输出的采集）
   IAudioCaptureClient *mmCapture = nullptr;//声卡采集客户端接口   （采集的声卡输出数据，并对内部缓冲区进行控制）
   IAudioClock         *mmClock = nullptr;
   bool bIsMic;
   bool bFirstFrameReceived;
   bool deviceLost;
   QWORD reinitTimer;
   FUN_GetVideoTime mFunGetVideoTime;
   //QWORD fakeAudioTimer;

   //UINT32 numFramesRead;

   UINT32 mNumTimesInARowNewDataSeen;
   String mDeviceId;
   String mDeviceName;
   bool mUseVideoTime;
   int mGlobalAdjustTime;
   QWORD mLastVideoTime;
   QWORD mCurVideoTime;
   UINT mSampleWindowSize;
   List<float> mInputBuffer;

   UINT mInputBufferSize;
   QWORD mFirstTimestamp;
   QWORD mLastQPCTimestamp;
   UINT32 mAngerThreshold;
   bool mUseQPC;
   HRESULT mInitErr;
   std::atomic_bool mbIsStart = false;
   CBlankAudioPlayback *mCurBlankPlaybackThingy = nullptr;
#ifdef DEBUG_CORE_AUDIO_SOURCE
   FILE* mPcmFile;
#endif
};

#endif

