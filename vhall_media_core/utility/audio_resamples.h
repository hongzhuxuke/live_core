//
//  audio_resamples.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/21.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef audio_resamples_h
#define audio_resamples_h

#include <stdio.h>
#include "../common/live_define.h"

#ifdef __cplusplus
extern "C" {
#endif
   
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
   
#ifdef __cplusplus
}
#endif

//note: this class only deal with unplane audio data

class DataCombineSplit;

NS_VH_BEGIN
class AudioResamples {
   
public:
   AudioResamples();
   ~AudioResamples();
   int Init(int out_ch, VHAVSampleFormat out_sample_fmt, int out_sample_rate,
            int  in_ch, VHAVSampleFormat  in_sample_fmt, int  in_sample_rate);
   void SetOutputDelegate(const OutputDataDelegate &delegate);
   int AudioResamplesProcess(const int8_t*data,const int size);
private:
   void Destroy();
   void OnCombineSplitData(const int8_t* audio_data,const int size);
   int OnAudioResamplesProcess(const int8_t*data,const int size);
   
   struct SwrContext  *mSwrContext;
   uint8_t            **mDstData;
   OutputDataDelegate mDeleagte;
   DataCombineSplit   *mInDataCombineSplit;
   int                mDstLineSize;
   enum AVSampleFormat mOutSampleFmt;
   enum AVSampleFormat mInSampleFmt;
   int                mOutCh;
   int                mInCh;
   int                mOutSampleRate;
   int                mInSampleRate;
   int                mMaxDstNbSamples;
   int                mDstNbSamples;
   int                mSrcNbSamples;
};
NS_VH_END
#endif /* audio_resamples_h */
