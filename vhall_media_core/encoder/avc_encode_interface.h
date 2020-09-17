//
//  VACEncodeInterface.hpp
//  VinnyLive
//
//  Created by ilong on 2016/11/7.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef VACEncodeInterface_hpp
#define VACEncodeInterface_hpp

#include <stdio.h>
#include <live_sys.h>
#include <stdint.h>
#include <live_get_status.h>
#include "../ratecontrol/rate_control_interface.h"

//struct LivePushParam;
struct LiveExtendParam;

//底层编码数据的支持格式
typedef enum
{
   PIX_FMT_YUV420SP_NV21 = 0,
   PIX_FMT_YUV420SP_NV12 = 1,
   PIX_FMT_YUV420P_I420 = 2,
   PIX_FMT_YUV420P_YV12 = 3,
}PixFmt;

typedef enum
{
   VIDEO_TYPE_NORMAL = 0,
   VIDEO_TYPE_DESKTOP = 1,
}VideoType;

typedef class VideoEncParam {
public:
   int width; //视频宽度
   int height; //视频高度
   int frame_rate; //视频采集的帧率
   int bit_rate; // 编码码率
   float gop_interval; //gop时间间隔单位秒  0表示使用默认值4s

   PixFmt pixel_format; //软编码时输入的数据格式

   /* data pre-process*/
   int video_process_filters;//视频数据预处理流程的标识

   /* configurable option */
   bool is_quality_limited;//true - have a bit rate upper limit to avoid high bandwidth consumption. false - no upper limit
   bool is_quality_protect;//true - have a bit rate lower limit to avoid poor encode quality. false - no lower limit
   bool is_adjust_bitrate;//用于表示是否允许码率调整。当此参数为true时，允许其他模块(如网络带宽估计)在编码过程中动态修改码率。
   int  high_codec_open;   //编码质量设定 0 - 正常编码，复杂场景下会有一定马赛克存在，有较严格的码率限制，较高的码率设定被无效化
                           //			      1~9 - 可调节高清编码，1到9数字越大编码结果马赛克效应越少，画面越清晰，但码率增长越大

   //debug option
   bool is_encoder_debug;     //true表示开启debug，输出x264日志统计
   bool is_saving_data_debug;  //true表示开启debug，保存采集与预处理的视频YUV数据

   VideoEncParam() {
      width = 960;
      height = 540;
      frame_rate = 15;
      bit_rate = 400 * 1000;
      gop_interval = 2; //s
      pixel_format = PIX_FMT_YUV420SP_NV12;
      video_process_filters = 0;
      is_adjust_bitrate = false;
      is_quality_limited = true;
      is_quality_protect = true;
      high_codec_open = 0;

      is_encoder_debug = false;
      is_saving_data_debug = false;
   }
}VideoEncParam;

class AVCEncodeInterface:public LiveGetStatus
{
public:
    AVCEncodeInterface(){};
    virtual ~AVCEncodeInterface(){};
    //virtual bool Init(LivePushParam * param) = 0;
    virtual bool Init(VideoEncParam * param) = 0;
    virtual int Encode(const char * indata,
                int insize,
                char * outdata,
                int * p_out_size,
                int * p_frame_type,
                uint64_t in_ts,
                uint64_t *out_ts, VideoType type = VIDEO_TYPE_NORMAL) = 0;
   virtual bool GetSpsPps(char*data,int *size) = 0;
   virtual bool RequestKeyframe() = 0;
   virtual bool GetRecentlyDecodeYUVData(char**data, int* size) = 0;
   virtual bool SetVideoQuality(double grade) = 0;
   virtual bool RateControlAdjust(NetworkState state) = 0;
};
#endif /* VACEncodeInterface_hpp */
