//
//  audio_output_ts.cpp
//  VhallLiveApi
//
//  Created by ilong on 2018/1/4.
//  Copyright © 2018年 vhall. All rights reserved.
//

#include "audio_output_ts.h"
#include "../utility/utility.h"
#include "../common/auto_lock.h"

AudioOutputTS::AudioOutputTS():
mAudioTimePres(0),
mPushLastAudioTs(0)
{
	mPushAudioDataSize = 0;
   vhall_lock_init(&mMutex);
}

AudioOutputTS::~AudioOutputTS(){
   vhall_lock_destroy(&mMutex);
}

void AudioOutputTS::Reset(){
   mPushLastAudioTs = 0;
   mPushAudioDataSize = 0;
}

void AudioOutputTS::Init(const int channel_num,const VHAVSampleFormat sample_fmt, const int sample_rate){
   mAudioTimePres = sample_rate*Utility::GetBitNumWithSampleFormat(sample_fmt)*channel_num/8;
}

void AudioOutputTS::SetDataSizeAndTS(const uint64_t ts,const int audio_data_size){
   VhallAutolock lock(&mMutex);
   mPushLastAudioTs = ts;
   mPushAudioDataSize += audio_data_size;
}

uint64_t AudioOutputTS::GetOutputTS(const int audio_data_size){
   VhallAutolock lock(&mMutex);
   mPushAudioDataSize -= audio_data_size;
   uint64_t ts = mPushLastAudioTs - (mPushAudioDataSize*1000.0/mAudioTimePres);
   return ts;
}
