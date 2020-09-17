//
//  voice_transition.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/24.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef voice_transition_h
#define voice_transition_h

#include <stdio.h>
#include <stdint.h>
#include <list>
#include "live_open_define.h"
#include <atomic>
#include "../common/live_sys.h"

enum TextPosition{
   TextPositionTop = 0,
   TextPositionBottom = 1,
};

namespace talk_base {
   class Thread;
}

class DataCombineSplit;
class ImageTextMixing;
class SafeDataQueue;
class SafeDataPool;
class AudioOutputTS;

NS_VH_BEGIN
class VoiceDictatePacking;
class AudioResamples;

class VoiceTransition{
   
public:
   class VoiceTransitionDelegate {
      
   public:
      VoiceTransitionDelegate(){};
      virtual ~VoiceTransitionDelegate(){};
      virtual void OnOutputVideoData(const int8_t *data, const int size, const uint64_t timestamp, const LiveExtendParam *extendParam) = 0 ;
      virtual void OnOutputAudioData(const int8_t *data, const int size, const uint64_t timestamp) = 0;
      virtual void OnResult(const std::string &result, bool is_last){};
   };
   VoiceTransition();
   ~VoiceTransition();
   
   /**
    设置音频信息

    @param channel_num 声道
    @param sample_fmt 采样点的精度
    @param sample_rate 音频采样频率
    @return 0 成功，非0失败
    */
   int SetAudioInfo(const int channel_num,const VHAVSampleFormat sample_fmt, const int sample_rate);
   
   /**
    设置视频信息

    @param width 视频宽
    @param height 视频高
    @param in_pix_fmt 视频格式
    @param frame_rate 视频帧率
    @return 0 成功，非0失败
    */
   int SetVideoInfo(const int width,const int height,const EncodePixFmt in_pix_fmt,const int frame_rate);
   
   /**
    设置回调

    @param delegate 回调
    */
   void SetDelegate(VoiceTransitionDelegate *delegate);
   
   /**
    设置buffer缓冲时间 单位MS

    @param time 时间 defualt 1000 ms
    */
   void SetBufferTime(const int time);
   /**
    开始准备语音听写

    @param app_id 科大讯飞appid
    @return 0 成功，非0失败
    */
   int StartPrepare(const std::string &app_id);
   int StopPrepare();
   bool IsStarted();
   void InputVideoData(const int8_t *data,const int size, const uint64_t timestamp = 0, const LiveExtendParam *extendParam=NULL);
   void InputAudioData(const int8_t *data,const int size, const uint64_t timestamp = 0);
   /**
    设置显示字体大小
    
    @param font_size 字体大小 默认值：30
    */
   void SetFontSize(const int font_size);
   
   /**
    设置显示字体样式
    
    @param font_file_path 字体库的路径
    */
   void SetFontFile(const std::string &font_file_path);
   
   /**
    设置字体显示的颜色
    
    @param font_color 字体颜色  eg:green red white... 默认值：white
    */
   void SetFontColor(const std::string &font_color);
   
   /**
    设置口音
    
    @param std::string&accent 可取值：mandarin：普通话 cantonese：粤语 lmz：四川话 默认值：mandarin
    */
   void SetAccent(const std::string&accent);
   /**
    设置文本显示的位置
    
    @param position
    */
   void SetTextPosition(enum TextPosition position);
   /**
    是否开启边界处理,防止文本被裁减
    
    @param enable true 开启，false 关闭，默认开启
    */
   void SetFixTextBounds(const bool enable);
   
   /**
    设置字体描边的颜色
    
    @param border_color 颜色值 默认黑色
    */
   void SetFontBorderColor(const std::string &border_color);
   
   /**
    设置字体描边的宽度
    
    @param w 默认是1
    */
   void SetFontBorderW(const int w);
private:
   enum {
      MSG_VOICE_TRANSITION,
      MSG_DELAT_LAST_RUN,
      MSG_DELAT_SPLIT_RUN,
   };
   void OnResult(const std::string &result, bool is_last);
   void OnSpeedBegin();
   void OnSpeedEnd(int reason);/* 0 if VAD.  others, error : see E_SR_xxx and msp_errors.h  */
   void OnResamplesAudioData(const int8_t * audio_data,const int size);
   void OnVDProcess(const int8_t*data_pcm,const int size);
   void OnCombineSplitData(const int8_t* audio_data,const int size);
   void OnDelayLastRun();
   void OnDelaySplitRun(const std::string& text);
   VoiceTransition(const VoiceTransition& )=delete;//禁用copy方法
   const VoiceTransition& operator=( const VoiceTransition& )=delete;//禁用赋值方法
private:
   AudioResamples *mAudioResamples;
   ImageTextMixing *mImageTextMixing;
   VoiceDictatePacking *mVoiceDictatePacking;
   VoiceTransitionDelegate *mVoiceTransitionDelegate;
   talk_base::Thread   *mVoiceTransitionThread;
   class WorkDelegateMessage;
   WorkDelegateMessage *mWorkDelegateMessage;
   SafeDataQueue      *mVideoQueue;
   SafeDataQueue      *mAudioQueue;
   DataCombineSplit   *mInDataCombineSplit;
   SafeDataPool       *mDataPool;
   std::string mVoiceText;
   std::string mLastTextLength;
   int mFrameRate;
   int mVideoHeight;
   int mVideoWidth;
   int mDataBufferTime; //ms
   int mIsTransitionFinish;
   std::list<LiveExtendParam> mExtendParamList;
   std::atomic_bool        mIsStarted;
   vhall_lock_t            mMutex;
   
   AudioOutputTS          *mAudioOutputTS;
};
NS_VH_END

#endif /* voice_transition_hpp */
