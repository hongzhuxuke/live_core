//
//  audio_output_ts.hpp
//  VhallLiveApi
//
//  Created by ilong on 2018/1/4.
//  Copyright © 2018年 vhall. All rights reserved.
//

#ifndef audio_output_ts_h
#define audio_output_ts_h

#include <stdio.h>
#include <atomic>
#include "../common/live_sys.h"
#include "live_open_define.h"

class AudioOutputTS {
public:
   AudioOutputTS();
   ~AudioOutputTS();
   void Init(const int channel_num,const VHAVSampleFormat sample_fmt, const int sample_rate);
   void Reset();
   void SetDataSizeAndTS(const uint64_t ts,const int audio_data_size);
   uint64_t GetOutputTS(const int audio_data_size);
   int                       mAudioTimePres;
   volatile uint64_t         mPushLastAudioTs;
   volatile std::atomic_long mPushAudioDataSize;
   vhall_lock_t              mMutex;
};

#endif /* audio_output_ts_h */
