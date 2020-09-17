//
//  data_combine_split.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/6/22.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef data_combine_split_h
#define data_combine_split_h

#include <stdio.h>
#include <stdlib.h>
#include "../common/live_define.h"

class BufferData {
   
public:
   BufferData(int buffer_size):mBufferSize(buffer_size),mDataSize(0),mData(NULL){
      mData = (int8_t*)calloc(1,buffer_size);
   }
   ~BufferData(){
      VHALL_DEL(mData);
   }
   void ClearData(){
      mDataSize = 0;
   }
   int  mBufferSize;
   int  mDataSize;
   int8_t * mData;
};

class DataCombineSplit {
   
public:
   DataCombineSplit();
   ~DataCombineSplit();
   
   /**
    设置输出数据回调

    @param delegate
    */
   void SetOutputDataDelegate(const OutputDataDelegate &delegate);
   
   /**
    清空buffer中的数据
    */
   void ClearBuffer();
   
   /**
    初始化对象

    @param output_size 输出数据的大小
    @return 0 是成功，非0失败
    */
   int Init(const int output_size);
   
   /**
    数据处理过程

    @param input_data 输入数据
    @param size 数据大小
    @return 0 是成功，非0失败
    */
   int DataCombineSplitProcess(const int8_t* input_data, const int size);
   
private:
   void Destory();
   BufferData    *mBufferData;
   OutputDataDelegate  mOutputDataDelegate;
};

#endif /* data_combine_split_hpp */
