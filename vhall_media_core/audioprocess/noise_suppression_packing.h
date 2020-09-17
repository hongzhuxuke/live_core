//
//  noise_suppression_packing.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/6/23.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef noise_suppression_packing_h
#define noise_suppression_packing_h

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <stdint.h>
#endif
#include "../common/live_open_define.h"

#define NS_PROCESS_SIZE    640

class NoiseSuppressionPacking {
   
public:
   NoiseSuppressionPacking();
   ~NoiseSuppressionPacking();
   
   /**
    初始化噪音抑制对象

    @param sample_rate 音频采样率
    @param level 噪音抑制模式
    @return 0是成功，非0失败
    */
   int Init(int sample_rate,int level,VHAVSampleFormat sample_format);
   
   /**
    重置过滤状态

    @return 0是成功，非0失败
    */
   int ResetFilterState();
   
   /**
    进行去噪抑制处理

    @param input_data 输入的数据，必须是640个字节的数据
    @param size 数据大小 必须是640个字节的数据
    @param output_data 处理完输出的数据
    @return 0是成功，非0失败
    */
   int NoiseSuppressionProcess(const int8_t *input_data, const int size, const int8_t *output_data);
   
private:
   int8_t *mInBufferL;
   int8_t *mInBufferH;
   int8_t *mOutBufferL;
   int8_t *mOutBufferH;
   int8_t *mFilterState1;
   int8_t *mFilterState2;
   int8_t *mFilterState3;
   int8_t *mFilterState4;
   void   *mNSHandle;
   VHAVSampleFormat mSampleFormat;
};

#endif /* noise_suppression_packing_hpp */
