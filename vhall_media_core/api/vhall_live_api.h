#ifndef __VINNY_LIVE_API_H__
#define __VINNY_LIVE_API_H__

#include "../common/live_obs.h"
#include "../common/live_open_define.h"

class VhallLive;

/**
 *  获取底层SDK的版本号
 */
std::string GetVersion();

class VhallLiveApi {
public:
   VhallLiveApi(const LiveCreateType liveType);
   VhallLiveApi(const LiveCreateType liveType,const char *logFilePath);
   ~VhallLiveApi();
   /**
    *  打开debug模式(debug模式会打印LOG日志信息)
    *
    *  @param enable 是否打开
    */
   static void LiveEnableDebug(const bool enable);
   /**
    *  添加播放直播的监听
    *
    *  @param obs 监听对象(继承监听接口)
    *
    *  @return 0是成功，非0是失败
    */
   int LiveAddPlayerObs(LiveObs * obs);
   /**
    *  添加推流直播的监听
    *
    *  @param obs 监听对象(继承监听接口)
    *
    *  @return 0是成功，非0是失败
    */
   int LiveAddPushObs(LivePushListener* obs);
   /**
    *  设置参数
    *
    *  @param param 是json格式
    {
    "audio_bitrate" :"音频编码码率",
    "bit_rate" :"视频编码码率",
    "ch_num" : "音频声道数",
    "frame_rate" :"视频帧率",
    "height" :"视频高度",
    "is_hw_encoder" :"是否使用硬编码",
    "orientation" : "横竖屏",
    "publish_reconnect_times" : "推流重连次数",
    "publish_timeout" : "推流超时时间，单位毫秒",
    "watch_timeout":"拉流超时时间",
    "watch_reconnect_times":"拉流重连次数",
    "buffer_time" : "buffer缓冲时间，单位秒",
    "sample_rate" :"音频采样率",
    "width" :"视频宽度",
    "live_publish_model":"直播推流的类型 0代表只是推流，1代表推流和数据回调，2代表只是数据回调，3是只推音频",
    "encode_pix_fmt":"软编码时输入的数据格式 0代表NV21 1代表YUV420sp",
    "video_decoder_mode":"解码模式，仅限android 1代表软解;2代表硬解; 默认是1",
    //用于日志监控
    "platform":"0代表iOS，1代表Android",
    "device_type":"设备类型,例如iphone6 小米3...",
    "device_identifier":"设备唯一标识符",
    }
    *
    *  @return 0是成功，非0是失败
    */
   int LiveSetParam(const char * param,const LiveCreateType liveType); // json
   /**
    *  设置监控日志的参数 注意参数是json string,开始直播前设置，之后设置无效
    *  return 0设置成功，-1是json解析失败
    */
   int SetMonitorLogParam(const char * param);
   /**
    *  连接推流地址
    *
    *  @param url 流地址(rtmp://...)
    *
    *  @return 0是成功，非0是失败
    */
   int LiveStartPublish(const char * url); // rtmp://...
   
   /**
    *  断开推流连接
    *
    *  @return 0是成功，非0是失败
    */
   int LiveStopPublish(void);
   
   /**
    连接接受数据地址

    @param url 观看地址
    @return 0是成功，非0是失败
    */
   int LiveStartRecv(const char * url);
   
   /**
    *   断开连接
    *
    *  @return 0是成功，非0是失败
    */
   int LiveStopRecv(void);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(YUV420sp格式数据)
    *  @param size      数据大小
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushVideoData(const char * data, const int size);
   
   /**
    *  push音频数据
    *
    *  @param data 音频数据（pcm数据）
    *  @param size 音频数据大小
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushAudioData(const char * data,const int size);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(YUV420sp格式数据)
    *  @param size      数据大小
    *  @param timestamp 时间戳
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushVideoData(const char * data, const int size, const uint64_t timestamp);
   
   /**
    *  push音频数据
    *
    *  @param data 音频数据（pcm数据）
    *  @param size 音频数据大小
    *  @param timestamp 时间戳
    *
    *  @return 0是成功，非0是失败
    */
   int LivePushAudioData(const char * data,const int size, const uint64_t timestamp);
   
   /**
    push视频数据

    @param data 视屏数据(h264编码的数据)
    @param size 视频数据的大小
    @param type 是否是关键帧 2代表关键帧，3代表p帧
    @param timestamp 时间戳
    @return 0是成功，非0是失败
    */
   int LivePushH264Data(const char * data, const int size, const int type, const uint64_t timestamp);
   
   /**
    push音频数据

    @param data 音频数据（pcm数据）
    @param size 音频数据大小
    @param timestamp 时间戳
    @return 0是成功，非0是失败
    */
   int LivePushAACData(const char * data, const int size, const uint64_t timestamp);
   
   /**
    *  push视频数据
    *
    *  @param data      视屏数据(h264编码的数据)
    *  @param size      视频数据的大小
    *  @param type      帧类型 0代表视频头 3代表I帧，4代表p帧，5代表b帧
    *  @param timestamp 视频时间戳 单位MS
    *  @return 0是成功，非0是失败
    */
   int LivePushH264DataTs(const char * data, const int size, const int type, const uint64_t timestamp);
   
   /**
    *  push音频数据
    *
    *  @param data      音频数据（aac编码的数据）
    *  @param size      音频数据大小
    *  @param type      帧类型 1代表音频头，2代表音频数据
    *  @param timestamp 音频时间戳 单位MS
    *  @return 0是成功，非0是失败
    */
   int LivePushAACDataTs(const char * data, const int size ,const int type ,const uint64_t timestamp);
   
   /**
    *  获取观看端实际的buffer时长
    *
    *  @return 返回buffer时长,单位毫秒
    */
   int GetPlayerRealityBufferTime(void);
   
   /**
    设置声音增益的大小，范围[0.0 1.0]

    @param size float 增益大小的值
    @return 0是成功，非0是失败
    */
   int SetVolumeAmplificateSize(float size);
   
   /**
    打开噪音消除
    
    @param open  ture打开,false关闭
    */
   int OpenNoiseCancelling(bool open);
   
private:
   VhallLive * p_vinny_live;
   /**
    *  创建VinnyLive对象
    *
    *  @param liveType 对象类型
    *
    *  @return 0是成功，非0是失败
    */
   int LiveCreate(const LiveCreateType liveType);
   /**
    *  创建VinnyLive对象
    *
    *  @param liveType    liveType 对象类型
    *  @param logFilePath log保存地址
    *
    *  @return 0是成功，非0是失败
    */
   int LiveCreate(const LiveCreateType liveType,const char *logFilePath);
   
   /**
    *  销毁VinnyLive对象
    */
   void LiveDeatory(void);
};

#endif
