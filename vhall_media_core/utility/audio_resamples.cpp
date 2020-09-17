//
//  audio_resamples.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/21.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "audio_resamples.h"
#include "../common/vhall_log.h"
#include "data_combine_split.h"
#include "utility.h"

NS_VH_BEGIN

AudioResamples::AudioResamples():mSwrContext(NULL),mDstData(NULL),mInDataCombineSplit(NULL){
   mDstLineSize = 0;
   mOutSampleFmt = AV_SAMPLE_FMT_NONE;
   mOutCh = 0;
   mInCh = 0;
   mOutSampleRate = 0;
   mInSampleRate = 0;
   mMaxDstNbSamples = 0;
   mDstNbSamples = 0;
   mSrcNbSamples = PCM_FRAME_SIZE;
   mInDataCombineSplit = new(std::nothrow) DataCombineSplit();
   if (mInDataCombineSplit==NULL) {
      LOGE("mDataCombineSplit==NULL");
   }else{
      mInDataCombineSplit->SetOutputDataDelegate(VH_CALLBACK_2(AudioResamples::OnCombineSplitData, this));
   }
}

AudioResamples::~AudioResamples(){
   VHALL_DEL(mInDataCombineSplit);
   Destroy();
   LOGI("AudioResamples::~AudioResamples");
}

void AudioResamples::Destroy(){
   if(mSwrContext){
      swr_close(mSwrContext);
      swr_free(&mSwrContext);
      mSwrContext = NULL;
   }
   if (mDstData) {
      av_freep(&mDstData[0]);
   }
   av_freep(&mDstData);
   mDstData = NULL;
}

void AudioResamples::SetOutputDelegate(const OutputDataDelegate &delegate){
   mDeleagte = delegate;
}

int AudioResamples::Init(int out_ch, VHAVSampleFormat out_sample_fmt, int out_sample_rate,
                         int  in_ch, VHAVSampleFormat  in_sample_fmt, int  in_sample_rate){
   mOutSampleRate = out_sample_rate;
   mInSampleRate = in_sample_rate;
   mInCh = in_ch;
   mOutCh = out_ch;
   mOutSampleFmt = (AVSampleFormat)out_sample_fmt;
   mInSampleFmt = (AVSampleFormat)in_sample_fmt;
   if ((AVSampleFormat)out_sample_fmt>AV_SAMPLE_FMT_DBL||(AVSampleFormat)in_sample_fmt>AV_SAMPLE_FMT_DBL) {
      LOGE("this class only deal with noplane audio data!");
      return -5;
   }
   Destroy();
   mSwrContext = swr_alloc_set_opts(NULL,
                                    av_get_default_channel_layout(out_ch),
                                    (AVSampleFormat)out_sample_fmt,
                                    (AVSampleFormat)out_sample_rate,
                                    av_get_default_channel_layout(in_ch),
                                    (AVSampleFormat)in_sample_fmt,
                                    (AVSampleFormat)in_sample_rate,
                                    0,
                                    NULL);
   if (mSwrContext==NULL) {
      LOGE("mSwrContent allpc is error!");
      return -1;
   }
   int ret = swr_init(mSwrContext);
   if (ret<0) {
      LOGE("m_swrContext init error!");
      Destroy();
      return -2;
   }
   
   mMaxDstNbSamples = mDstNbSamples = (int)av_rescale_rnd(mSrcNbSamples, mOutSampleRate, mInSampleRate, AV_ROUND_UP);
   ret = av_samples_alloc_array_and_samples(&mDstData,&mDstLineSize,
                                            out_ch,//number of audio channels
                                            mDstNbSamples,
                                            (AVSampleFormat)out_sample_fmt,0);
   if (ret<0) {
      Destroy();
      LOGE("mDstLineSize <= 0!");
      return -3;
   }

   int size = PCM_FRAME_SIZE*Utility::GetBitNumWithSampleFormat(in_sample_fmt)/8*in_ch;
   mInDataCombineSplit->Init(size);
   return 0;
}

int AudioResamples::AudioResamplesProcess(const int8_t*data,const int size){
   if (mInDataCombineSplit) {
      return  mInDataCombineSplit->DataCombineSplitProcess(data, size);
   }
   return -1;
}

int AudioResamples::OnAudioResamplesProcess(const int8_t*data,const int size){
   if (mSwrContext==NULL) {
      LOGD("mSwrContext==NULL,you need init audio resamples!");
      return -1;
   }
   if (mInSampleRate==mOutSampleRate&&mOutCh==mInCh) {
      if (mDeleagte) {
         mDeleagte((const int8_t*)data,size);
      }
   }else{
      int srcNbSamples = size/(Utility::GetBitNumWithSampleFormat(mInSampleFmt)/8*mInCh);
      mDstNbSamples = (int)av_rescale_rnd(swr_get_delay(mSwrContext, mInSampleRate) +
                                          srcNbSamples, mOutSampleRate, mInSampleRate, AV_ROUND_UP);
      if (mDstNbSamples > mMaxDstNbSamples) {
         av_freep(&mDstData[0]);
         int ret = av_samples_alloc(mDstData,&mDstLineSize,
                                                      mOutCh,//number of audio channels
                                                      mDstNbSamples,
                                                      mOutSampleFmt,1);
         if (ret >= 0){
            mMaxDstNbSamples = mDstNbSamples;
         }
      }
      uint8_t *in[] = {(uint8_t *)data};
      int ret = swr_convert(mSwrContext,mDstData,mDstNbSamples, (const uint8_t **)in,srcNbSamples);
      if (ret<0) {
         LOGE("Error while converting!");
         return ret;
      }
      int dstSamplesSize = av_samples_get_buffer_size(&mDstLineSize, mOutCh,
                                                      ret, mOutSampleFmt, 1);
      if (dstSamplesSize<=0) {
         LOGE("av_samples_get_buffer_size return <= 0!");
         return -4;
      }
      if (mDeleagte) {
         mDeleagte((const int8_t*)mDstData[0],dstSamplesSize);
      }
   }
   return 0;
}

void AudioResamples::OnCombineSplitData(const int8_t *audio_data, const int size){
  int ret = OnAudioResamplesProcess(audio_data, size);
   if (ret<0) {
      LOGW("OnAudioResamplesProcess ret:%d",ret);
   }
}
NS_VH_END
