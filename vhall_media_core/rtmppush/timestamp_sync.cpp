//
//  TimestampSync.cpp
//  VinnyLive
//
//  Created by ilong on 2016/12/6.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "timestamp_sync.h"
#include "../encoder/encode_interface.h"
#include "../common/live_define.h"
#include "../utility/utility.h"
#include "../common/vhall_log.h"
#include <stdio.h>
#include "../3rdparty/json/json.h"

#define Max_AV_Diff_T           300 //音视频的最大时间差值，单位MS

TimestampSync::TimestampSync(EncodeInterface *encode):mEncoder(encode)
{
   mVideoTS = 0;
   mAudioTS = 0;
   mAudioDataSize = 0;
   mVideoDuration = 0;
   mAudioTimePres = 0;
   mVideoFrameTS = new VideoFrameTS();
}

TimestampSync::~TimestampSync(){
   VHALL_DEL(mVideoFrameTS);
}

void TimestampSync::StartPublish(){
   mVideoTS = 0;
   mAudioTS = 0;
   mAudioDataSize = 0;
   mVideoFrameTS->ResetData();
}

void TimestampSync::StopPublish(){
   ResetVideoFrameTS();
}

int TimestampSync::LiveSetParam(LivePushParam *param){
   if (param) {
      mParam = param;
      mVideoDuration = param->frame_rate==0?0:1000/param->frame_rate;
      mAudioTimePres = param->dst_sample_rate*Utility::GetBitNumWithSampleFormat(mParam->src_sample_fmt)*param->ch_num;
      return 0;
   }
   return -1;
}

void TimestampSync::SetOutputDataDelegate(const TSOutputDataDelegate &delegate){
   mOutputDataDelegate = delegate;
}

void TimestampSync::LivePushVideo(const char * data,const int size ,const LiveExtendParam *extendParam){
   
   if (mParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      LOGW("only audio mode,not push video");
      return;
   }
   
   VideoFrameTS * p = mVideoFrameTS;
   if (p->mVideoCount == 0) {
      p->mFirstTS = Utility::GetTimestampMs();
      mAudioTS = mVideoTS;
   } else {
      //过滤多余的帧数
      uint64_t current_time = Utility::GetTimestampMs();
      uint64_t interval_time = current_time - p->mFirstTS;
      if ((current_time-p->mTmpTS)>=1000&&interval_time>0) {
         float avg = p->mVideoCount*1000.0f/interval_time;
         LOGD("pushvideo frame rate: %.3f", avg);
         p->mTmpTS = current_time;
      }
      if ((p->mVideoCount>mParam->frame_rate)&&(interval_time * (mParam->frame_rate)) < (p->mVideoCount * 1000)) {
         LOGW("discard excess video data!");
         return;
      }
   }
   int time_difference = (int)(mVideoTS - mAudioTS);
   if (mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO) {
      if (time_difference>Max_AV_Diff_T) {
         //如果视频时间戳大于音频就丢视频帧;
         LOGW("discard excess video data!");
         return;
      }
   }
   if(abs(time_difference)<Max_AV_Diff_T||mParam->live_model==LIVE_TYPE_VIDEO_ONLY){
      mVideoTS += mVideoDuration;
   }else{
      //如果视频时间戳小于音频300毫秒就跳帧
      mVideoTS += mVideoDuration*2;
   }
   if (mEncoder) {
      mEncoder->EncodeVideo(data, size, mVideoTS,extendParam);
   }
   p->mVideoCount++;
}

void TimestampSync::LivePushAudio(const char * data,const int size){
   if (mParam->live_model == LIVE_TYPE_VIDEO_ONLY) {
      LOGW("only video mode,not push audio");
      return;
   }
   if (mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO&&mVideoFrameTS->mVideoCount<=0) {
      LOGW("mVideoFrameTS->mVideoCount<=0!");
      return;
   }
   if (mParam->live_model == LIVE_TYPE_VIDEO_AND_AUDIO) {//判断推流模式，如果不是纯音频直播就进入
      int time_difference = (int)(mAudioTS - mVideoTS);
      if (time_difference > Max_AV_Diff_T) {
         LOGW("discard excess audio data!");
         return;
      }
   }
   mAudioDataSize += size;
   mAudioTS = (uint32_t)(mAudioDataSize*1000*8/mAudioTimePres);
   if (mEncoder) {
      mEncoder->EncodeAudio(data, size, mAudioTS);
   }
   mAudioTS = (mAudioDataSize*1000*8/mAudioTimePres);
}

void TimestampSync::LivePushVideoHW(const char * data,const int size ,const int type){
   if (mParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      LOGW("only audio mode,not push video");
      return;
   }
   VideoFrameTS * p = mVideoFrameTS;
   if (p->mVideoCount == 0) {
      p-> mFirstTS= Utility::GetTimestampMs();
      mAudioTS = mVideoTS;
   } else {
      //切忌硬编码不能过滤多余的帧数,因为硬编码是在iOS层编码的,否则会出现马赛克
      uint64_t current_time = Utility::GetTimestampMs();
      uint64_t interval_time = current_time - p->mFirstTS;
      if ((current_time-p->mTmpTS)>1000&&interval_time>0) {
         float avg = p->mVideoCount*1000.0f/interval_time;
         LOGD("pushvideo frame rate: %.3f", avg);
         p->mTmpTS = current_time;
      }
   }
   if (mEncoder) {
      mEncoder->EncodeVideoHW(data, size, type, mVideoTS);
   }
   if (mParam->live_model == LIVE_TYPE_VIDEO_AND_AUDIO) {
      int time_difference = (int)(mVideoTS - mAudioTS);
      if(time_difference < (-1*Max_AV_Diff_T)){
         //如果视频时间戳小于音频300毫秒就跳帧
         mVideoTS += mVideoDuration*2;
         LOGW("Audio data much more.");
      }else if(time_difference > Max_AV_Diff_T){
         //如果视频时间戳大于音频就丢视频帧;
         mVideoTS += mVideoDuration/2;
         LOGW("Video data much more.");
      }else {
         mVideoTS += mVideoDuration;
      }
   }else{
      mVideoTS += mVideoDuration;
   }
   p->mVideoCount++;
}

void TimestampSync::LivePushAudioHW(const char * data,const int size){
   if (mParam->live_model==LIVE_TYPE_VIDEO_ONLY) {
      LOGW("only video mode,not push audio");
      return;
   }
   if (mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO&&mVideoFrameTS->mVideoCount<=0) {
      LOGW("mVideoFrameTS->mVideoCount<=0!");
      return;
   }
   if (mParam->live_model == LIVE_TYPE_VIDEO_AND_AUDIO) {//判断推流模式，如果不是纯音频直播就进入
      int time_difference = (int)(mAudioTS - mVideoTS);
      if (time_difference>Max_AV_Diff_T) {
         LOGW("discard excess audio data!");
         return;
      }
   }
   // TODO:
   if (mEncoder) {
      mAudioDataSize += size;
      mEncoder->EncodeAudioHW(data, size, mAudioTS);
   }
   mAudioTS = (mAudioDataSize*1000*8/mAudioTimePres);
}

/**
 *  发送Amf0消息
 *
 *  @param msg      消息体
 *  @return 0是成功，非0是失败
 */
int TimestampSync::LivePushAmf0Msg(std::string &msg){
   if (mParam&&(mParam->live_model==LIVE_TYPE_VIDEO_AND_AUDIO||mParam->live_model==LIVE_TYPE_AUDIO_ONLY)) {
      if (mOutputDataDelegate) {
        return mOutputDataDelegate(msg,mAudioTS);
      }
   }else{
      if (mOutputDataDelegate) {
        return mOutputDataDelegate(msg,mVideoTS);
      }
   }
   return -1;
}

void TimestampSync::ResetVideoFrameTS(){
   mVideoFrameTS->ResetData();
}

bool TimestampSync::LiveGetRealTimeStatus(VHJson::Value & value){

	value["Name"] = VHJson::Value("TimestampSync");
	//TODO make this call thread safe 
	value["video_timestamp"] = VHJson::Value((uint32_t)mVideoTS);
	value["audio_timestamp"] = VHJson::Value((uint32_t)mAudioTS);
	value["video_duration"] = VHJson::Value(mVideoDuration);
	value["m_audio_time_pres"] = VHJson::Value(mAudioTimePres);
	value["m_audio_data_size"] = VHJson::Value((int)mAudioDataSize);
   return true;
}
