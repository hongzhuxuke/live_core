//
//  LiveOpenDefine.h
//  VinnyLive
//
//  Created by ilong on 2016/12/9.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef LiveOpenDefine_h
#define LiveOpenDefine_h

#include <string>
#include <map>

//视频预处理标志位
#define VIDEO_PROCESS_SCENETYPE  0x01 //开启场景自动检测
#define VIDEO_PROCESS_DENOISE    0x02 //开启图像去噪(仅自然场景)
#define VIDEO_PROCESS_DIFFCHECK  0x04 //开启一致性宏块帧skip

/// @name namespace vhall
/// @{
#ifdef __cplusplus
#define NS_VH_BEGIN                     namespace vhall {
#define NS_VH_END                       }
#define USING_NS_VH                     using namespace vhall;
#define NS_VH                           ::vhall
#else
#define NS_VH_BEGIN
#define NS_VH_END
#define USING_NS_VH
#define NS_VH
#endif
//  end of namespace group
/// @}

namespace VHJson {
   class Value;
}

/*
 设置vhall_media_core push 模块的日志
 @path 日志路径 如果是空则输出到console。
 否则就是文件路径 如 "D://test.log"
 真正的日志文件明是"D://test.log_time"
 @level 日志级别：
 1   debug
 2   info
 3   warn
 4   error
 */
extern void SetModuleLog(std::string path = "", int level = 4);

class LivePushListener {

public:
	LivePushListener(){};
	virtual ~LivePushListener(){};
	/**
	*  事件回调
	*
	*  @param type    事件类型
	*  @param content 事件内容
	*
	*  @return 是否成功
	*/
	virtual int OnEvent(int type, const std::string content) = 0;
   
    virtual int OnJNIDetachEventThread(){return 0;};
    /**
     *
     enum NetworkState {
       NetworkState_Normal = 0,
       NetworkState_Underuse,
       NetworkState_Overuse,
       NetworkState_Block,
       NetworkState_Reset
     };
     */
    virtual bool OnRateControlAdjust(int state) {return true;};
   
    virtual void OnPushBufferInfo(std::string info) {};
};

//底层编码数据的支持格式
typedef enum
{
	ENCODE_PIX_FMT_YUV420SP_NV21 = 0,
	ENCODE_PIX_FMT_YUV420SP_NV12 = 1,
   ENCODE_PIX_FMT_YUV420P_I420 = 2,
   ENCODE_PIX_FMT_YUV420P_YV12 = 3,
}EncodePixFmt;

//sdk使用的编码类型 1
typedef enum
{
	ENCODE_TYPE_SOFTWARE = 0,
	ENCODE_TYPE_HARDWARE = 1
}EncodeType;

//推流模式
typedef enum
{
   LIVE_TYPE_NONE = 0,
	LIVE_TYPE_VIDEO_AND_AUDIO = 1,
	LIVE_TYPE_VIDEO_ONLY = 2,
	LIVE_TYPE_AUDIO_ONLY = 3,
}LiveModel;

typedef enum VHAVSampleFormat {
   VH_AV_SAMPLE_FMT_NONE = -1,
   VH_AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
   VH_AV_SAMPLE_FMT_S16,         ///< signed 16 bits
   VH_AV_SAMPLE_FMT_S32,         ///< signed 32 bits
   VH_AV_SAMPLE_FMT_FLT,         ///< float
   VH_AV_SAMPLE_FMT_DBL,         ///< double
   
   VH_AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
   VH_AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
   VH_AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
   VH_AV_SAMPLE_FMT_FLTP,        ///< float, planar
   VH_AV_SAMPLE_FMT_DBLP,        ///< double, planar
   
   VH_AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
}VHAVSampleFormat;

typedef enum DropFrameType{

	/**DROP_NONE
	*  do not drop any.
	*/
	DROP_NONE = 0,

	/**DROP_GOPS
	*  drop gop and audio bettow it.
	*  may drop half ,if the other half have sent,
	*  may drop more than one gop, we only leave the lastest gop in buffer.
	*/
	DROP_GOPS,

	/**DROP_ALL_VIDEO
	*  Drop all video in buffer.
	*  if there is no video, do not drop
	*/
	DROP_ALL_VIDEO,

	/**DROP_ALL
	*  clear all in queue, and wait for new I.
	*/
	DROP_ALL,

	/**DROP_ALL_B: only drop B frame
	*  first drop all B frames in buffer, if there have B frames in buffer
	*  if there is no B frame in buffer， do not drop
	*/
	DROP_ALL_B,

	/**DROP_ALL_B_P: drop B P Frame in order,
	*  first drop all B frames in buffer, if there have B frames in buffer
	*  if there is on B frame in buffer, drop all P frame in buffer
	*  if there is no B/P frame in buffer，  do not drop
	*/
	DROP_ALL_B_P,

	/**DROP_ALL_B_P_I: drop B P I Frame in order,
	*  first drop all B frames in buffer, if there have B frames in buffer
	*  if there is on B frame in buffer, drop all P frame in buffer
	*  if there is no B/P frame in buffer drop all I frame in buffer
	*  if there is no B/P/I frame in buffer, do not drop
	*/
	DROP_ALL_B_P_I,

	/**DROP_ALL_B_P_I_A  B P I Audio Frame in order, never failed
	*  first drop all B frames in buffer, if there have B frames in buffer
	*  if there is on B frame in buffer, drop all P frame in buffer
	*  if there is no B/P frame in buffer drop all I frame in buffer
	*  if there is no B/P/I frame in buffer drop ALL in Buffer
	*/
	DROP_ALL_B_P_I_A,

	/** DROP_ONE_B: same as DROP_ALL_B, but one time just drop one frame。
	*   buffer will still be full after next add
	*/
	DROP_ONE_B,

	/** DROP_ONE_B_P: same as DROP_ALL_B_P, but one time just drop one frame。
	*   buffer will still be full after next add
	*/
	DROP_ONE_B_P,

	/** DROP_ONE_B_P_I: same as DROP_ALL_B_P_I, but one time just drop one frame。
	*   buffer will still be full after next add
	*/
	DROP_ONE_B_P_I,

	/** DROP_ONE_B_P_I_A: same as DROP_ALL_B_P_I_A, but one time just drop one frame。
	*   buffer will still be full after next add
	*/
	DROP_ONE_B_P_I_A
}DropFrameType;

enum ProxyTypes {
   PPROXY_NONE,
   PPROXY_HTTPS,
   PPROXY_SOCKS5,
   PPROXY_UNKNOWN
};

//视频场景类型
typedef enum VideoSceneType{
   SceneType_Unknown = 0,
   SceneType_Natural,
   SceneType_Artificial
}VideoSceneType;

struct ProxyDetail
{
   ProxyTypes type;
   std::string host;
   int port;
   std::string autoconfig_url;
   bool autodetect;
   std::string bypass_list;
   std::string username;
   std::string password;

   ProxyDetail() : type(PPROXY_UNKNOWN),autodetect(false) { }
};

typedef class BaseLiveParam {
public:
   //用于日志上报
   int platform; //平台类型 0代表iOSAPP 1代表AndroidAPP 2代表flash 3代表wap 4代表IOSSDK 5代表AndroidSDK 6代表小助手
   std::string device_type; //设备类型
   std::string device_identifier; //设备唯一标志符
   LiveModel live_model; //直播推流的模式
   
   bool is_http_proxy;
   ProxyDetail proxy;
   
   virtual bool GetJsonObject(VHJson::Value *value) = 0;
   
}BaseLiveParam;

typedef class LivePushParam:public BaseLiveParam
{
public:
	int width; //视频宽度
	int height; //视频高度
	int frame_rate; //视频采集的帧率
	int bit_rate; // 编码码率
	float gop_interval; //gop时间间隔单位秒  0表示使用默认值2.0s
	DropFrameType drop_frame_type;   

	int sample_rate; //音频采样率
   int dst_sample_rate; //编码时的音频采样率
	int ch_num;       //声道
	int audio_bitrate; //音频编码码率
   VHAVSampleFormat  src_sample_fmt;//音频输入数据的格式
   VHAVSampleFormat  encode_sample_fmt;//音频编码数据的格式
   
	int publish_timeout; //发起超时
	int publish_reconnect_times; //发起重练次数
   
	EncodeType encode_type;    //编码的类型
	EncodePixFmt encode_pix_fmt; //软编码时输入的数据格式
	
   
   /* data pre-process*/
   int video_process_filters;//视频数据预处理流程的标识
   int audio_process_filters;//音频数据预处理流程的标识
      
   /* configurable option */
   bool is_quality_limited;//true - have a bit rate upper limit to avoid high bandwidth consumption. false - no upper limit
   bool is_quality_protect;//true - have a bit rate lower limit to avoid poor encode quality. false - no lower limit
   bool is_adjust_bitrate;//用于表示是否允许码率调整。当此参数为true时，允许其他模块(如网络带宽估计)在编码过程中动态修改码率。
   int  high_codec_open;   //编码质量设定 0 - 正常编码，复杂场景下会有一定马赛克存在，有较严格的码率限制，较高的码率设定被无效化
							      //			      1~9 - 可调节高清编码，1到9数字越大编码结果马赛克效应越少，画面越清晰，但码率增长越大
	//metadate 中增加的额外信息。
	std::map<std::string, std::string> extra_metadata;

   //debug option
   bool is_encoder_debug;     //true表示开启debug，输出x264日志统计
   bool is_saving_data_debug;  //true表示开启debug，保存采集与预处理的视频YUV数据

   /* media quality option */
   bool is_assess_video_quality; //open video quality assessment or not, true is open
   int  score_frames_period;      //indicating how many frames will generate a video quality score

   bool is_assess_audio_quality; //open audio quality assessment or not, true is open(not enabled yet)

   int  fast_assess_mode;        //set as 1 to open a fast assess mode.


   LivePushParam(){
      width = 960;
      height = 540;
      frame_rate = 15;
      bit_rate = 400*1000;
      gop_interval = 2; //s
      drop_frame_type = DROP_GOPS;
      sample_rate = 44100;
      dst_sample_rate = 44100;
      ch_num = 2;
      audio_bitrate = 48*1000;
      src_sample_fmt = VH_AV_SAMPLE_FMT_NONE;
      encode_sample_fmt = VH_AV_SAMPLE_FMT_NONE;
      publish_timeout = 5000;
      publish_reconnect_times = 10;
      
      encode_type = ENCODE_TYPE_SOFTWARE;
      encode_pix_fmt = ENCODE_PIX_FMT_YUV420SP_NV12;
      live_model = LIVE_TYPE_VIDEO_AND_AUDIO;
      
      video_process_filters = 0;
      audio_process_filters = 0;
      
      is_adjust_bitrate = false;
      is_quality_limited = true; 
      is_quality_protect = false;
      high_codec_open = 0;

      is_encoder_debug = false;
      is_saving_data_debug = false;

      is_assess_video_quality = false;
      score_frames_period = frame_rate * 30;

      is_assess_audio_quality = false;

      fast_assess_mode = 1;
      
      platform = 0;
      device_type = "";
      device_identifier = "";
      
      extra_metadata.clear();
      is_http_proxy = false;
   }
   bool GetJsonObject(VHJson::Value *value);
}LivePushParam;

typedef class LivePlayerParam:public BaseLiveParam {
public:
   int video_decoder_mode;   //1.soft;2, hard;
   int watch_timeout;   //观看超时
   int watch_reconnect_times; // 观看重练次数
   int buffer_time;    //s
   std::string dispatch_url;
   std::string default_play_url;

   LivePlayerParam(){
      watch_timeout = 5000;
      watch_reconnect_times = 5;
      buffer_time = 2;
      video_decoder_mode = 2;
      
      platform = 0;
      device_type = "";
      device_identifier = "";
      
	   is_http_proxy = false;
   }
   bool GetJsonObject(VHJson::Value *value);
}LivePlayerParam;

typedef struct LiveExtendParam{
   int scene_type;  //场景类型
   unsigned int same_last;   //是否与上一帧相同  0不同 1相同
   unsigned char *frame_diff_mb; //与参考帧相比各宏块是否发生变化的标记 0相同 255不同
}LiveExtendParam;

//type is also used as important level
//so there index can never be changed
typedef enum VHFrameType{
    SCRIPT_FRAME = -1, //amf0消息
    VIDEO_HEADER = 0,
    AUDIO_HEADER = 1,
    AUDIO_FRAME  = 2,
    VIDEO_I_FRAME= 3,
    VIDEO_P_FRAME= 4,
    VIDEO_B_FRAME= 5,
}VHFrameType;

typedef enum VHMuxerType{
   MUXER_NONE = -1,
	RTMP_MUXER = 0, //rtmp和aestp
	FILE_FLV_MUXER, //写本地flv文件
   HTTP_FLV_MUXER  //http-flv
}VHMuxerType;

typedef enum {
   CAMERA_FRONT = 0,
   CAMERA_BACK = 1
}CameraType;

typedef enum {
   PUSH_CONNECT_OK = 100,
   PUSH_CONNECT_ERROR = 101,
   PUSH_PARAM_ERROR = 102,
   PUSH_SEND_ERROR = 103,
   PUSH_SEND_SPEED = 104,
   PUSH_NETWORK_OK = 105,
   PUSH_NETWORK_EXCEPTION = 106,
   PUSH_ENCODE_BUSY = 107,
   PUSH_ENCODE_OK = 108,
}VHLivePushStatusCode;

typedef enum {
   PLAY_CONNECT_OK = 200,
   PLAY_CONNECT_ERROR = 201,
   PLAY_PARAM_ERROR = 202,
   PLAY_RECV_ERROR = 203,
   PLAY_RECV_SPEED = 204,
   PLAY_START_BUFFERING = 205,
   PLAY_STOP_BUFFERING = 206,
   PLAY_STREAM_TYPE = 207,
   PLAY_VIDEO_INFO = 208,
   PLAY_AUDIO_INFO = 209,
}VHLivePlayStatusCode;

//对外输出的事件
typedef enum
{
	OK_PUBLISH_CONNECT = 0,
	ERROR_PUBLISH_CONNECT = 1,
	OK_WATCH_CONNECT = 2,
	ERROR_WATCH_CONNECT = 3,
	START_BUFFERING = 4,
	STOP_BUFFERING = 5,
	ERROR_PARAM = 6,
	ERROR_RECV = 7,
	ERROR_SEND = 8,
	INFO_SPEED_UPLOAD = 9,
	INFO_SPEED_DOWNLOAD = 10,
	INFO_NETWORK_STATUS = 11,//暂时未使用
	INFO_DECODED_VIDEO = 12,
	INFO_DECODED_AUDIO = 13,
	UPLOAD_NETWORK_EXCEPTION = 14,
	UPLOAD_NETWORK_OK = 15,
	RECV_STREAM_TYPE = 17,
	VIDEO_QUEUE_FULL = 18,
	AUDIO_QUEUE_FULL = 19,
   VIDEO_ENCODE_BUSY = 20,
   VIDEO_ENCODE_OK = 21,
   RECONNECTING = 22,
   ONCUEPOINT_AMF_MSG = 23,
   RC_BITRATE_FIRST_DOWN = 24,
   
	VIDEO_HWDECODER_INIT = 101,
	VIDEO_HWDECODER_DESTORY = 102,
   
   DEMUX_METADATA_SUCCESS = 201,
   HTTPS_REQUEST_MSG = 202
}VHLiveStatusCode;
#endif /* LiveOpenDefine_h */
