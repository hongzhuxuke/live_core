#ifndef _MEDIA_CORE_INCLUDE_H__
#define _MEDIA_CORE_INCLUDE_H__
#include <map>
#include <stdint.h>
#include "IMediaCore.h"

#include "live_open_define.h"
#include "vhall_live_push.h"
#include "voice_transition.h"

#define VoiceTransition_APPID          "5a011c3d"          //科大讯飞使用SDK注册的appid
#define SupportLanguageStr_Mandarin    "mandarin"          //普通话
#define SupportLanguageStr_Cantonese   "cantonese"         //粤语
#define SupportLanguageStr_Lmz         "lmz"               //四川话

#define ENABLE_PUSH_STREAM_LOG   true

//支持的语音文件类型。
typedef enum SupportLanguageType {
   SupportLanguageType_Mandarin = 0,
   SupportLanguageType_Cantonese,
   SupportLanguageType_Lmz
};

class ImageEnhance;

class MediaCore :public IMediaCore, public IDataReceiver, public LivePushListener, public VoiceTransition::VoiceTransitionDelegate {
public:
   MediaCore();
   virtual ~MediaCore();
   virtual void SetLogLevel(char *path,int);
   virtual void SetUploadLogParam(int bu,std::string vid);
   virtual bool InitCapture(IMediaCoreEvent* mediaCoreEvent,IGraphics *graphic);
   virtual void UninitCapture();
   virtual bool StartRtmp(const char *rtmpURI);
   virtual bool StopRtmp(const char *rtmpURI);
 
   virtual void StartRecord(const char *fileName);
   virtual void StopRecord(/*const char *fileName*/);
   virtual void SuspendRecord();  //暂停录制
   virtual void RecoveryRecord(); //恢复录制
   //virtual void SetRecordFileNum(const int& iFilesNum);

   virtual int  RtmpCount();
      
   virtual bool GetStreamStatus(int index,StreamStatus *);

   virtual bool ResetPublishInfo(const char *currentUrl,const char *nextUrl);
      
   virtual int GetSumSpeed();
   
   virtual UINT64 GetSendVideoFrameCount(int index);

   virtual IDataReceiver *GetDataReceiver();
   
   virtual void PushAudioSegment(float *buffer, unsigned int numFrames, unsigned long long timestamp);
   virtual void PushVideoSegment(unsigned char *buffer, unsigned int size, unsigned long long timestamp,bool bSame);

   virtual void SetAudioParam(int channels,int samplesPerSecond,int samplesBits);
   virtual void SetOpenNoiseCancelling(bool bNoiseReduction);

	virtual void SetAudioParamReduction(const int& samplesPerSecond, const int& samplesBits, const bool& bNoiseReduction);
	virtual bool IsSamplesBitsChanged(const int& samplesBits);////音频编码码率
	virtual bool IsSamplePerSecondChange(const int& samplesPerSecond);//音频采样率
   virtual int GetSamplesBits();

   virtual void SetVideoParam(float gop,int fps,int bits,int w,int h);
   virtual void SetVideoParamEx(int video_process_filters,bool is_adjust_bitrate,bool is_quality_limited,bool is_encoder_debug,bool is_saving_data_debug,int high_codec_open);
   virtual void SetProxy(bool isProxy,std::string host, int port, std::string username, std::string password);
	virtual int OnEvent(int type, const std::string content);
   virtual void SetSceneInfo(VideoSceneType sceneType);
   virtual void GetSceneInfo(VideoSceneType& sceneType);
   virtual void SetVersion(const char *);   
   virtual void GetVideoParamInfo(wchar_t *);
   virtual void RateControlSwitch(bool on);
	virtual int GetHighCodec();
	void SetVolumeAmplificateSize(float size);
   int LivePushAmf0Msg(const char* data, int len);
   //virtual int GetHighCodec();

   virtual void SetVideoQuality(bool quality);

   virtual void StartVoiceTransition(bool start = false,int font = 15, int lan = 0);
   virtual void ResetVoiceTransition();

   //语音转换回调处理
   virtual void OnOutputVideoData(const int8_t *data, const int size, const uint64_t timestamp, const LiveExtendParam *extendParam);
   virtual void OnOutputAudioData(const int8_t *data, const int size, const uint64_t timestamp);
   virtual void OnResult(const std::string &result, bool is_last);

   virtual void GetMediaFileWidthAndHeight(const char* path, int &width, int& height);
   virtual void DesktopEnhanceSwitchOn(bool open);
   virtual void SetDesktopStatus(bool isDesktopShare);
private:
   void StartPushVideo();
   void StopPushVideo();
   static DWORD __stdcall VideoPushThread(MediaCore *);
   void VideoPushThreadLoop();
   void VideoPush();
   void SetEventPushVideo(unsigned long long timestamp);
   
   bool IsEnableVoiceTransition();
   void SetEnableVoiceTransition(bool enable);
   void CreateImageHance(int width, int height);
private:
   LivePushParam mLiveParam;
   VHallLivePush *mLivePusher = nullptr;
   HANDLE mRtmpUrlsMutex = nullptr;
   std::map<int,std::string> m_rtmpUris;
   std::map<int,std::string> m_fileUris;

   std::atomic_bool mbIsDesktopImageHance = false;
   std::atomic_bool mbIsDesktopShare = false;
   unsigned char *yuvData = NULL;
   IGraphics *mGraphic = NULL;
   IMediaCoreEvent *mMediaCoreEvent = NULL;
   HANDLE mPushStreamMutex;
   std::atomic<int> mVideoIndex = 0;

   std::atomic_bool mbIsStartPushStream = false;
   int mFps=25;
   
   std::atomic<unsigned long long> firstAudioTimeStamp = 0;
   std::atomic<unsigned long long> mLastAudioTimeStamp = 0;
   std::atomic<unsigned long long> mLastVideoTimeStamp = 0;
   int mSumSpeed = 0;
   int mSumVideoFrameCount = 0;
   VideoSceneType mSceneType = SceneType_Unknown;
   ImageEnhance* g_ImageEnhance = nullptr;
   //////////////////////////////////////////////
   HANDLE mVideoPush_mutex = NULL;
   HANDLE mVideoPush_event = NULL;  
   HANDLE mVideoPush_thread = NULL;
   
   volatile bool mVideoPush_stop = false;
   VoiceTransition* mVoiceTransition = NULL; //采集后的音频数据源

   std::atomic_bool  mbEnableVoiceTransition;
   int mVoiceTransitionFontSize = 15;
   SupportLanguageType mCurrentSupportLanguageType = SupportLanguageType_Mandarin;
   //FILE *PCM;
   
   std::atomic_ullong mAudioFrameCount;
   std::atomic_ullong mAudioSendCount;
   std::atomic_ullong mAudioFrameSize;
   FILE*    mMixYUV = nullptr;
   std::atomic_ullong mVideoFrameCount;
   std::atomic_ullong mVideoFrameSize;
   int mIRecordFileId; //当前录制的文件ID
   int mLogID = 0;
   //int mIRecordFileNum;
};
;
#endif //_MEDIA_CAPTURE_INCLUDE_H__
