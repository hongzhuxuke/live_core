#ifndef __VINNY_LIVE_H__
#define __VINNY_LIVE_H__

#include <string>
//llc change for windows
#include <live_sys.h>
#include "talk/base/thread.h"
#include "talk/base/messagehandler.h"
#include "live_interface.h"
#include "../common/live_define.h"
#include "../common/live_obs.h"
#include "../ratecontrol/rate_control_interface.h"
#include <mutex>

class MediaMuxerInterface;
class VHallLivePlayer;
class VHallLivePush;
class VHallPlayMonitor;

class VhallLive : public talk_base::MessageHandler,
                  public LiveInterface,
                  public LiveObs,
                  public LivePushListener,
                  public EnocoderRateController
{
public:
   VhallLive(const char *logFilePath);
   
   ~VhallLive();
   
   void CreateVhallPush();
   
   void CreateVhallPlayer();
   
   void AddPlayerObs(LiveObs * obs);
   
   void AddPushObs(LivePushListener * obs);
   
   int  SetParam(const char * param,const LiveCreateType liveType);
   /**
    *  设置监控日志的参数 注意参数是json string,开始直播前设置，之后设置无效
    *  return 0设置成功，-1是json解析失败
    */
   int SetMonitorLogParam(const char * param);
   
   int  StartRecv(const char * urls);
   
   void StopRecv(void);
   
   void PushAudioData(const char * data, int size);
   
   void PushVideoData(const char * data, int size);
   
   void LivePushAudio(const char * data,
                     const int size,
                     const uint64_t timestamp);
   
   void LivePushVideo(const char * data,
                     const int size,
                     const uint64_t timestamp);
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(h264编码的数据)
    *  @param size      视频数据的大小
    *  @param type      帧类型 3代表I帧，4代表p帧，5代表b帧
    */
   void PushH264Data(const char * data, int size, int type, const uint64_t timestamp);
   // pcm(暂时使用pcm数据)
   void PushAACData(const char * data, int size, const uint64_t timestamp);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(h264编码的数据)
    *  @param size      视频数据的大小
    *  @param type      帧类型 0代表视频头 3代表I帧，4代表p帧，5代表b帧
    *  @param timestamp 视频时间戳 单位MS
    *  @return 0是成功，非0是失败 -1代表参数data为NULL
    */
   int LivePushH264DataTs(const char * data, const int size, const int type ,const uint64_t timestamp);
   /**
    *  push音频数据
    *
    *  @param data      音频数据（aac编码的数据）
    *  @param size      音频数据大小
    *  @param type      帧类型 1代表音频头，2代表音频数据
    *  @param timestamp 音频时间戳 单位MS
    *  @return 0是成功，非0是失败 -1代表参数data为NULL
    */
   int LivePushAACDataTs(const char * data, const int size , const int type ,const uint64_t timestamp);

   virtual int StartPublish(const char * url);
    
   virtual void StopPublish(void);
   
   int GetPlayerBufferTime();
   int SetVolumeAmplificateSize(float size);
   int OpenNoiseCancelling(bool open);
private:
   VhallLive(){};
   enum {
      MSG_UPLOAD_SPEED,
      MSG_EVENT,
      MSG_DETACH_EVENT_THREAD
   };
   void LogReportMsg(const std::string &msg);
   virtual int OnEvent(int type, const std::string content);
   virtual int OnJNIDetachEventThread();
   virtual bool RateControlAdjust(NetworkState state);
   virtual void OnPushBufferInfo(PushBufferInfo info);
   virtual int OnRawVideo(const char *data, int size, int w, int h);
   virtual int OnJNIDetachVideoThread();
   virtual int OnRawAudio(const char *data, int size);
   virtual int OnJNIDetachAudioThread();
   virtual int OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts);
   virtual DecodedVideoInfo * GetHWDecodeVideo();
   
   void OnMessage(talk_base::Message * msg);
   bool OnSetPushParam(LivePushParam * outparam, const std::string inparam);
   bool OnSetPlayerParam(LivePlayerParam * outparam, const std::string inparam);
   void OnNotifyEvent(int type, const std::string content);
   void OnGetUplaodSpeed();
public:
   static std::string      m_documents_path;
private:
   talk_base::Thread       *m_event_thread;
   std::mutex mMutex;
   int                     m_muxer_id;
   int                     mLiveFmt;
   VHallLivePlayer         *m_vhall_player;
   VHallLivePush           *m_vhall_push;
   LivePushListener        *m_listener;
   LivePushParam           mPushParam;
   LivePlayerParam         mPlayerParam;
   LiveObs                 *mPlayerObs;
   VHallPlayMonitor        *mMonitorLog;
};

#endif
