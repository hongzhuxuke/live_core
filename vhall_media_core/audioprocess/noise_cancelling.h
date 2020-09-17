//
//  noise_cancelling.h
//  VhallLiveApi
//
//  Created by ilong on 2017/6/20.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef noise_cancelling_h
#define noise_cancelling_h

#include "../common/live_define.h"
#include <stdio.h>
#include <stdlib.h>
#include <map>

#define NOISE_SUPPRESSION_LEVEL  3 //取值为0，1，2，3 值越大效果越好

class DataCombineSplit;
class NoiseSuppressionPacking;

namespace talk_base {
   class Thread;
}

NS_VH_BEGIN

class NoiseCancelling{
   
public:
   NoiseCancelling();
   ~NoiseCancelling();
   /**
    创建并初始化噪音抑制对象
    @param sample_rate 音频采样率
    @param level 噪音消除的等级取值为0，1，2，3，值越大效果越好
    @param channel_num 声道数
    @return 0 是成功，非0失败
    */
   int Init(int sample_rate,int level,int channel_num,VHAVSampleFormat sample_format);
   
   /**
    设置数据回掉

    @param delegate
    */
   void SetOutputDataDelegate(const OutputDataDelegate &delegate);
   
   /**
    开始处理

    @return 0 是成功，非0失败
    */
   int Start();
   /**
    停止处理

    @return 0 是成功，非0失败
    */
   int Stop();
   /**
    去噪处理过程方法

    @param input_data 输入的数据
    @param size 输入数据的大小
    @return 0 是成功，非0失败
    */
   int NoiseCancellingProcess(const int8_t * input_data,const int size);
   
   /**
     pcm数据拆分，将lrlrlrlr的数据拆分为llllrrrr

    @param input_data 输入的数据，lrlrlrlr
    @param size 输入的数据大小
    @param output_data 输出的数据大小llllrrrr
    @return 0成功，非0失败
    */
   static int AudioDataSplitLR(const int8_t* input_data,const int size,const int8_t* output_data,VHAVSampleFormat sample_format);
   /**
    pcm数据合并，将llllrrrr的数据合并为lrlrlrlr

    @param input_data 输入的数据，llllrrrr
    @param size  输入的数据大小
    @param output_data 输出的数据大小lrlrlrlr
    @return 0成功，非0失败
    */
   static int AudioDataCombineLR(const int8_t* input_data,const int size,const int8_t* output_data,VHAVSampleFormat sample_format);
   
   static int VolumeAmplificateS16(const int8_t* audio_data,const int data_size,float alpha);
   static int VolumeAmplificateFLT(const int8_t *audio_data,const int data_size,float alpha);
private:
   enum {
      MSG_NOISE_CANCELLING_START,
      MSG_NOISE_CANCELLING_STOP,
      MSG_NOISE_CANCELLING_PROCESS,
      MSG_NOISE_INIT_CANCELLING,
      MSG_NOISE_DESTORY_CANCELLING
   };
   template<typename T>
   static int AudioDataSplitLR(T* input_data,const int size,const int8_t* output_data);
   template<typename T>
   static int AudioDataCombineLR(T* input_data,const int size,const int8_t* output_data);
   void OnProcess(const int8_t *input_data, const int size);
   void Destory();
   int  DataSplitting(const int8_t *input_data, const int size);
   void OnInputBufferData(const int8_t *input_data, const int size);
   void OnOutputBufferData(const int8_t *input_data, const int size);
   NoiseCancelling(const NoiseCancelling& )=delete;//禁用copy方法
   const NoiseCancelling& operator=( const NoiseCancelling& )=delete;//禁用赋值方法
private:
   std::map<int,NoiseSuppressionPacking*> mNCObjectMap;
   talk_base::Thread   *mNoiseCancellingThread;
   OutputDataDelegate  mOutputDataDelegate;
   int                 mChannelNum;
   VHAVSampleFormat    mSampleFormat;
   int                 mSampleRate;
   int                 mLevel;
   int8_t *            mInBufferData;
   int8_t *            mOutBufferData;
   
   class WorkDelegateMessage;
   WorkDelegateMessage *mWorkDelegateMessage;
   DataCombineSplit    *mInputBufferData;
   DataCombineSplit    *mOutputBufferData;
};

NS_VH_END

#endif /* noise_cancelling_hpp */
