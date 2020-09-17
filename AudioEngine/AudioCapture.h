#ifndef __AUDIO_CAPTURE_INCLUDE__
#define __AUDIO_CAPTURE_INCLUDE__

#include "IAudioCapture.h"
#include <atomic>
#include "PhaseCheck.h"
#include <atomic>
class  IDataReceiver;
class IAudioEncoder;
#define ALL_CH 8			//混音路数，最大为8
class  AudioCapture : public IAudioCapture {
public:
	AudioCapture(IDataReceiver *dataReiciever, int SampleRateHz);
   ~AudioCapture();

public:
	virtual void SetSampleRateHz(const int& iSampleRateHz);
   virtual bool InitPlaybackSource(wchar_t* deviceId);
   virtual bool AddRecodingSource(const TYPE_RECODING_SOURCE& sourceType,
                                  void **pAudioSource,
                                  const bool& isNoiseGate,
                                  void **param,
                                  const int& paramCnt);

   virtual bool SetNoiseGate(bool isNoise, int closeThreshold, int openThreshold);
   //添加麦克风设备
   virtual bool SetMicDeviceInfo(DeviceInfo,bool isNoise,int closeThreshold,int openThreshold, int iAudioSampleRate, HRESULT& reuslt);

   virtual bool DelAudioSource(void *auduiSource);
   virtual void SetPlaybackVolume(const float& plackBackVol);
   virtual void SetMicVolume(const float& micVol);   
   virtual void ClearRecodingSource();
   virtual void SetAudioListening(IDataReceiver*);
   virtual bool Start();
   virtual void SetPriority(int);
   virtual void SetInterval(int);
   virtual void Shutdown();
   virtual UINT GetSampleRateHz() const;
   virtual UINT NumAudioChannels() const;
   virtual UINT GetBitsPerSample() const;
   virtual void GetAudioMeter(float& audioMag, float& audioPeak, float& audioMax);
   virtual void GetMicOrignalAudiometer(float& audioMag, float& audioPeak, float& audioMax);
   virtual float MuteMic(bool);
   virtual float MuteSpeaker(bool);
   virtual float GetMicVolunm();
   virtual float GetSpekerVolumn();
   virtual bool GetCurrentMic(DeviceInfo &deviceInfo);
   virtual void SetForceMono(bool);
   virtual bool GetForceMono();
   virtual void SetSaveMicAudio(bool bSave);
   void CaptureAudioLoop();
   void Process();
private:
   bool AddCoreAudioSource(DeviceInfo *device, const bool& isNoiseGate,void **);

   void AddAudioSource(IAudioSource *source);
   void RemoveAudioSource(IAudioSource *source);
   bool QueryAudioBuffers(QWORD &sleepTime);
   bool QueryNewAudio(QWORD& sleepTime);

   static DWORD __stdcall CaptureAudioThread(LPVOID lpUnused);
   void WaitForShutdownComplete();
   void SyncAudioVolumn(float* mixBuffer/*List<float> &mixBuffer*/, UINT &audioFramesSinceMixaudioMaxUpdate);
   void SyncMicAudioVolumn(float* mixBuffer/*List<float> &mixBuffer*/, UINT &audioFramesSinceMixaudioMaxUpdate);
   bool IsSaveMicOrignalAudioState();
private:

#ifdef DEBUG_AUDIO_CAPTURE
   // IAudioEncoder* mAudioEncoder;
   // FILE*    mAacFile;
   FILE*    mRawFile;
   FILE*    mDeckFile;
#endif 
   FILE*    mRawFile;
   FILE*    mMixAudioFile;
   FILE*    mMicAudioFile;
   FILE*    mMediaAudioFile;
   FILE*    mPlayerAudioFile;
private:
   UINT mSampleRateHz;
   UINT mAudioChannels;
   bool mForceMicMono;   //true 为单声道
   bool mUsingPushToTalk;
   CircularList<uint64_t> mBufferedAudioTimes;

   IAudioSource  *mPlaybackAudio = nullptr;
   IAudioSource  *mMicAudio = nullptr;
   List<IAudioSource*> auxAudioSources;

   HANDLE mAuxAudioMutex;
   HANDLE mAudioFilterMutex;
   PhaseCheck *mPhaseCheckPtr = nullptr;
   uint64_t  mLatestAudioTime;
   uint64_t  mLatestQueryAudioTime;
   //float mCurDesktopVol;
   //扬声器音量
   //std::atomic<float> mPlaybackVol;
   std::atomic<float> mPlaybackBackVol;

   //麦克风音量
   float mCurMicVol;
   float mMicVol;
   float mMicBackVol;
   
   bool mRunning;
   std::atomic_bool mStart;

   HANDLE mCaptureThread;

   float mPlaybackMag;
   float mMicMag;
   float mPlaybackPeak;
   float mMicPeak;
   float mPlaybackMax;
   float mMicMax;

   //for output audio   音频输出
   float mMixAudioMag;  //混音后杂质
   float mMixAudioPeak; //混音后峰值
   float mMixAudioMax; //混音后音响最大

   //for micMix audio 
   float mMixMicAudioMag;
   float mMixMicAudioPeak;
   float mMixMicAudioMax;

   // HANDLE mSoundDataMutex;
   IDataReceiver *mDataReceiver = nullptr;  //mediaCore
   IDataReceiver *mDataListening = nullptr;  //回调监听
   List<float> mInputBuffer;
   bool mRecievedFirstAudioFrame;

   int m_interval = 2;
   int m_priority = THREAD_PRIORITY_NORMAL;

   HANDLE mSaveMicAudioMutex;
   bool m_bSaveMicOrignalAudio;
   QWORD mDurationTime = 0;
};

#endif
