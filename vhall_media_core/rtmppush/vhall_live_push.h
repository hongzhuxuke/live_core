//  VhallPush.hpp
//  VinnyLive
//
//  Created by liwenlong on 16/6/21.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef VhallPush_hpp
#define VhallPush_hpp

#include <stdlib.h>
#include <string>
#include <stdint.h>
#include <atomic>
#include <list>
#include "../common/live_sys.h"
#include "../common/live_open_define.h"

class LivePushListener;
struct LivePushParam;
class VHallMonitorLog;
class EncodeInterface;
class MediaQualityInterface;
class LiveStatusListener;
class TimestampSync;
class EventParam;
class RateControl;
class DataCombineSplit;
class Timer;
class AudioOutputTS;
class EnocoderRateController;

NS_VH_BEGIN
class MediaMuxer;
class AudioResamples;
class NoiseCancelling;
NS_VH_END

USING_NS_VH;

class VHallLivePush
{
public:
   VHallLivePush();
   
   ~VHallLivePush();
   
   /**
    设置push相关参数
    
    @param param param
    @return 0是成功，-1是失败
    */
   int LiveSetParam(LivePushParam *param);
   /**
    *  设置监控日志的参数 注意参数是json string,开始直播前设置，之后设置无效
    *  return 0设置成功，-1是json解析失败
    */
   int SetMonitorLogParam(const char * param);
   /**
    设置事件监听
    
    @param listener 监听对象
    @return 0是成功，-1是失败
    */
   int SetListener(LivePushListener *listener);
    /**
        设置码率控制的回调函数
     */
   int SetRateControl(EnocoderRateController * rc);
   /**
    *  add a muxer return muxerId
    *
    *  @param type      muxer 类型
    *  @param param     参数 type==RTMP_MUXER时，为rtmp url，type==FILE_FLV_MUXER时，为写文件地址
    *
    *  @return id是成功，-1失败
    */
   int AddMuxer(VHMuxerType type,void * param);
   /**
    *  remove a muxer with muxerId
    *
    *  @param muxer_id     muxer id
    *
    *  @return 0是成功，-1失败
    */
   int RemoveMuxer(int muxer_id);
   /**
    *  remove all muxer
    *
    *  @return 0是成功，-1失败
    */
   int RemoveAllMuxer();
   /**
    *  start
    *
    *  @param muxer_id  muxer id
    *
    *  @return 0是成功，-1失败
    */
   int StartMuxer(int muxer_id);
   /**
    *  stop
    *
    *  @param muxer_id  muxer id
    *
    *  @return 0是成功，-1失败
    */
   int StopMuxer(int muxer_id);
   /**
    *  remove stop muxer
    *
    *  @return 0是成功，-1失败
    */
   int StopAllMuxer();
   /**
    *  get muxerStatus with muxerId
    *
    *  @param muxer_id  muxer id
    *
    *  @return muxer statuts 0 start 1 stop 2 unknow
    */
   int GetMuxerStatus(int muxer_id);
   /**
    *  get muxerStatus with muxerId
    *
    *  @param muxer_id  muxer id
    *
    *  @return speed
    */
   int GetDumpSpeed(int muxer_id);
   /**
    获取Muxer的类型
    
    @param muxer_id muxer id
    @return muxer 的类型
    */
   const VHMuxerType GetMuxerType(int muxer_id);
   
   /**
    *  push音频数据
    *
    *  @param data 音频数据（pcm数据）
    *  @param size 音频数据大小
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushAudio(const char * data,const int size);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(YUV420sp格式数据)
    *  @param size      视频数据的大小
    *  @param extendParam  扩展参数
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushVideo(const char * data,const int size, const LiveExtendParam *extendParam=NULL);
   
   /**
    *  push音频数据
    *
    *  @param audio_data 音频数据（pcm数据）
    *  @param size 音频数据大小
    
    *  @param timestamp 音频时间戳 单位MS
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushAudio(const char * audio_data,
                     const int size,
                     const uint64_t timestamp);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(YUV420sp格式数据)
    *  @param size      视频数据的大小
    *  @param timestamp 视频时间戳 单位MS
    *  @param extendParam  扩展参数
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushVideo(const char * data,
                     const int size,
                     const uint64_t timestamp,
                     const LiveExtendParam *extendParam=NULL);
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(h264编码的数据)
    *  @param size      视频数据的大小
    *  @param type      帧类型 3代表I帧，4代表p帧，5代表b帧
    *  @return 0是成功，非0是失败
    *
    */
   int LivePushVideoHW(const char * data,const int size ,const int type, const uint64_t timestamp);
   /**
    *  push音频数据
    *
    *  @param data      音频数据（pcm数据）
    *  @param size      音频数据大小
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushAudioHW(const char * data,const int size, const uint64_t timestamp);
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(h264编码的数据)
    *  @param size      视频数据的大小
    *  @param type      帧类型 0代表视频头 3代表I帧，4代表p帧，5代表b帧
    *  @param timestamp 视频时间戳 单位MS
    *
    *  @return 0是成功，非0是失败 -1代表参数data为NULL
    */
   int LivePushH264DataTs(const char * data,
                          const int size,
                          const int type,
                          const uint64_t timestamp);
   /**
    *  push音频数据
    *
    *  @param data      音频数据（aac编码的数据）
    *  @param size      音频数据大小
    *  @param type      帧类型 1代表音频头，2代表音频数据
    *  @param timestamp 音频时间戳 单位MS
    *
    *  @return 0是成功，非0是失败 -1代表参数data为NULL
    */
   int LivePushAACDataTs(const char * data,
                         const int size,
                         const int type,
                         const uint64_t timestamp);
   /**
    *  发送Amf0消息
    *
    *  @param msg      消息体
    *  @return 0是成功，非0是失败
    */
   int LivePushAmf0Msg(std::string msg);
   /**
    * 获得实时状态信息，返回json string
    *
    *  @return json string
    */
   std::string LiveGetRealTimeStatus();
   /**
    设置声音增益的大小，可以在直播过程中修改，
    @param size 声音增益的大小值，范围在[0.0f,1.0f]之间
    */
   void SetVolumeAmplificateSize(float size);
   /**
    打开噪音消除
    @param open  ture打开,false关闭
    */
   int OpenNoiseCancelling(bool open);
   
   /**
    开关网络码率自适应模块，默认开
    */
   void RateControlSwitch(bool on);
   
private:
   void NotifyEvent(const int type, const EventParam &param);
   void LogReportMsg(const std::string &msg);
   int  OnAmf0Msg(const std::string &msg,const uint64_t timestamp);
   void OnNSAudioData(const int8_t* audio_data,const int size);
   void OnResamplesAudioData(const int8_t * audio_data,const int size);
   void PushPCMAudioData(const int8_t * audio_data,const int size);
   void OnTimerSelector();
   std::string GetRealTimeStatus();
   uint64_t GetOutputTs(const int size);
   VHallLivePush(const VHallLivePush& )=delete;//禁用copy方法
   const VHallLivePush& operator=( const VHallLivePush& )=delete;//禁用赋值方法
private:
   LivePushParam           *mParam;
   EncodeInterface         *mRtmpEncode;
   MediaQualityInterface   *mMediaQualityAssess;
   MediaMuxer              *mRtmpPublish;
   LiveStatusListener      *mListenerImpl;
   LivePushListener        *mListener;
   TimestampSync           *mTSSync;
   VHallMonitorLog         *mMonitorLog;
   RateControl             *mRateControl;
   NoiseCancelling         *mNoiseCancelling;
   AudioResamples          *mAudioResamples;
   Timer                   *mTimer;
   vhall_lock_t            mMutex;
   std::atomic_bool        mIsConnection;
   std::atomic_bool        mIsPushAudioTs;
   float                   mVolumeAmplificateSize;
   AudioOutputTS          *mAudioOutputTS;
   uint64_t                mPushLastAudioTs;
};

#endif /* VhallPush_hpp */
