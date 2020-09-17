#ifndef __LIVE_RTMP_ENCODE_H__
#define __LIVE_RTMP_ENCODE_H__

#include "encode_interface.h"
#include "talk/base/messagehandler.h"
#include "live_get_status.h"
#include "../common/live_sys.h"
#include <atomic>
#include <stdio.h>

namespace talk_base {
   class Thread;
}
namespace VHJson {
   class Value;
}
class AVCEncodeInterface;
class LiveInterface;
class AACEncoder;
struct LivePushParam;
struct LiveExtendParam;
struct VideoEncParam;
class MediaDataSend;
class LiveStatusListener;
class SafeDataPool;
class VideoPreprocess;
class RateControl;
//enum VideoType;


class MediaEncode : public talk_base::MessageHandler,public EncodeInterface
{
public:
   MediaEncode();
   ~MediaEncode();
   virtual int LiveSetParam(LivePushParam *param)override;
   //设置状态的监听事件
   virtual void SetStatusListener(LiveStatusListener * listener)override;
   virtual void SetOutputListener(MediaDataSend * output_listener)override;
   virtual void SetMediaQAListener(MQADataInteract * listener)override;
   virtual void EncodeVideo(const char * data, int size, uint64_t timestamp, const LiveExtendParam *extendParam = NULL)override;
   virtual void EncodeVideoHW(const char * data, int size ,int type, uint64_t timestamp)override;
   virtual void EncodeAudio(const char * data, int size, uint64_t timestamp)override;
   virtual void EncodeAudioHW(const char * data, int size, uint64_t timestamp)override;
   virtual void Start()override;
   virtual bool RequestKeyframe()override;
   virtual void Stop()override;
   virtual bool isInit()override;
   virtual bool LiveGetRealTimeStatus(VHJson::Value &value)override;
   virtual void SetVideoQualityGrade(double grade) override;
   virtual bool RateControlAdjust(NetworkState state) override;
private:
   enum {
      MSG_RTMP_VIDEO_START,
      MSG_RTMP_AUDIO_START,
      MSG_RTMP_VIDEO_STOP,
      MSG_RTMP_AUDIO_STOP,
      MSG_RTMP_ENCODE_VIDEO,
      MSG_RTMP_ENCODE_VIDEOHW,
      MSG_RTMP_ENCODE_AUDIO,
      MSG_RTMP_ENCODE_AUDIOHW
   };
   void OnMessage(talk_base::Message* msg) override;
   void OnStart();
   void OnEncodeVideo(const char * data, int size, int rotate, uint64_t timestamp, LiveExtendParam *extendParam = NULL);
   void OnEncodeVideoHW(const char * data, int size, int type, uint64_t timestamp);
   void OnEncodeAudio(const char * data, int size, uint64_t timestamp);
   //void OnEncodeAudioHW(const char * data, int size, uint32_t timestamp);
private:
   talk_base::Thread   *mVideoWorkThread;
   talk_base::Thread   *mAudioWorkThread;
   char                *mVideoEncodedData;
   char                *mAudioEncodedData;
   AVCEncodeInterface  *mH264Encoder;
   AACEncoder          *mAacEncoder;
   SafeDataPool        *mVideoPool;
   SafeDataPool        *mAudioPool;
   LivePushParam       *mParam;
   LiveStatusListener  *mStatusListener;
   MediaDataSend       *mOutputListener; 
   MQADataInteract     *mMediaQADataInOut;
   VideoEncParam       *mVideoEncParam;

   int                 mVideoCount;
   uint64_t            mVideoFirstTS;
   volatile bool       mIsEncodeBusy;
   volatile uint64_t   mVideoLastPostTS;
   std::atomic_bool    mIsAudioInited;
   std::atomic_bool    mIsVideoInited;
   int                 mFrameRate;
   bool                mIsFirstRCBitrateDown;
};

#endif
