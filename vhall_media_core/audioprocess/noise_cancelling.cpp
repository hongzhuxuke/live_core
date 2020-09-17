//
//  noise_cancelling.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/6/20.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "noise_cancelling.h"
#include "talk/base/thread.h"
#include "../utility/utility.h"
#include "../common/vhall_log.h"
#include "noise_suppression_packing.h"
#include "../common/live_message.h"
#include "../utility/data_combine_split.h"
#include "talk/base/messagehandler.h"
#include "talk/base/stringdigest.h"

NS_VH_BEGIN

class NoiseCancelling::WorkDelegateMessage:public talk_base::MessageHandler  {
   
public:
   WorkDelegateMessage():mNoiseCancelling(NULL){
      
   }
   ~WorkDelegateMessage(){
      mNoiseCancelling = NULL;
   }
   void SetObserve(NoiseCancelling * noise_cancelling){
      mNoiseCancelling = noise_cancelling;
   }
private:
   void OnMessage(talk_base::Message* msg){
      if (mNoiseCancelling) {
         switch(msg->message_id){
            case MSG_NOISE_CANCELLING_PROCESS:{
               DataMessageData * obs = static_cast<DataMessageData*>(msg->pdata);
               if (mNoiseCancelling) {
                  mNoiseCancelling->OnProcess(obs->data_, obs->size_);
               }
            }
               break;
            case MSG_NOISE_INIT_CANCELLING:{
               if (mNoiseCancelling->mSampleRate<=0||mNoiseCancelling->mSampleRate>SUPPORT_MAX_NOISE_SUPPRESSION_SAMPLING_RATE||mNoiseCancelling->mSampleFormat==VH_AV_SAMPLE_FMT_NONE||mNoiseCancelling->mNCObjectMap.size()>0) {
                  return;
               }
               for (int i = 0; i<mNoiseCancelling->mChannelNum; i++) {
                  NoiseSuppressionPacking * nsp = new NoiseSuppressionPacking();
                  nsp->Init(mNoiseCancelling->mSampleRate,mNoiseCancelling->mLevel,mNoiseCancelling->mSampleFormat);
                  mNoiseCancelling->mNCObjectMap.insert(std::pair<int,NoiseSuppressionPacking*>(i,nsp));
               }
            }
               break;
            case MSG_NOISE_DESTORY_CANCELLING:{
               for (auto iter = mNoiseCancelling->mNCObjectMap.begin(); iter != mNoiseCancelling->mNCObjectMap.end(); iter++ ){
                  if (iter->second) {
                     VHALL_DEL(iter->second);
                  }
               }
               mNoiseCancelling->mNCObjectMap.clear();
            }
               break;
            default:
               break;
         }
      }
      VHALL_DEL(msg->pdata);
   }
   NoiseCancelling * mNoiseCancelling;
};

NoiseCancelling::NoiseCancelling():
mInputBufferData(NULL),
mOutputBufferData(NULL),
mNoiseCancellingThread(NULL),
mOutBufferData(NULL),
mInBufferData(NULL),
mWorkDelegateMessage(NULL){
   mSampleRate = 0;
   mLevel = -1;
   mSampleFormat = VH_AV_SAMPLE_FMT_NONE;
   mChannelNum = -1;
   mNoiseCancellingThread = new(std::nothrow) talk_base::Thread();
   if (mNoiseCancellingThread) {
      if (!mNoiseCancellingThread->started()) {
         mNoiseCancellingThread->Start();
      }
      mNoiseCancellingThread->Restart();
   }else{
      LOGE("mNoiseCancellingThread new is error!");
   }
   mInputBufferData = new(std::nothrow) DataCombineSplit();
   if (mInputBufferData==NULL) {
      LOGE("DataCombineSplit new is error!");
   }
   mOutputBufferData = new(std::nothrow) DataCombineSplit();
   if (mOutputBufferData==NULL) {
      LOGE("DataCombineSplit new is error!");
   }
   mWorkDelegateMessage = new(std::nothrow) WorkDelegateMessage();
   if (mWorkDelegateMessage==NULL) {
      LOGE("WorkDelegateMessage new error!");
   }else{
      mWorkDelegateMessage->SetObserve(this);
   }
}

NoiseCancelling::~NoiseCancelling(){
   VHALL_THREAD_DEL(mNoiseCancellingThread);
   VHALL_DEL(mWorkDelegateMessage);
   Destory();
   VHALL_DEL(mOutBufferData);
   VHALL_DEL(mInBufferData);
   VHALL_DEL(mInputBufferData);
   VHALL_DEL(mOutputBufferData);
}

void NoiseCancelling::Destory(){
   for (auto iter = mNCObjectMap.begin(); iter != mNCObjectMap.end(); iter++ ){
      if (iter->second) {
         VHALL_DEL(iter->second);
      }
   }
   mNCObjectMap.clear();
}

void NoiseCancelling::SetOutputDataDelegate(const OutputDataDelegate &delegate){
   mOutputDataDelegate = delegate;
}

int NoiseCancelling::Init(int sample_rate, int level,int channel_num,VHAVSampleFormat sample_format){
   mChannelNum = channel_num;
   mSampleFormat = sample_format;
   mSampleRate = sample_rate>=SUPPORT_MAX_NOISE_SUPPRESSION_SAMPLING_RATE?SUPPORT_MAX_NOISE_SUPPRESSION_SAMPLING_RATE:sample_rate;
   mLevel      = level;
   mInputBufferData->SetOutputDataDelegate(VH_CALLBACK_2(NoiseCancelling::OnInputBufferData, this));
   int bufferSize = NS_PROCESS_SIZE/2*channel_num*Utility::GetBitNumWithSampleFormat(mSampleFormat)/8;
   mInputBufferData->Init(bufferSize);
   
   if (mOutBufferData==NULL) {
      mOutBufferData = (int8_t*)calloc(1,bufferSize);
   }
   if (channel_num>1&&mInBufferData==NULL) {
      mInBufferData = (int8_t*)calloc(1,bufferSize);
   }
   mOutputBufferData->SetOutputDataDelegate(VH_CALLBACK_2(NoiseCancelling::OnOutputBufferData, this));
   int size = PCM_FRAME_SIZE*Utility::GetBitNumWithSampleFormat(sample_format)/8*channel_num;
   mOutputBufferData->Init(size);
   return 0;
}

int NoiseCancelling::Start(){
   mNoiseCancellingThread->Post(mWorkDelegateMessage,MSG_NOISE_INIT_CANCELLING);
   return 0;
}

int NoiseCancelling::Stop(){
   mNoiseCancellingThread->Clear(mWorkDelegateMessage);
   mNoiseCancellingThread->Post(mWorkDelegateMessage,MSG_NOISE_DESTORY_CANCELLING);
   return 0;
}

void NoiseCancelling::OnProcess(const int8_t *input_data, const int size){
   int ret = DataSplitting(input_data, size);
   if (ret<0) {
      LOGE("DataSplitting error!");
   }
}

int NoiseCancelling::NoiseCancellingProcess(const int8_t *input_data, const int size){
   if (mNoiseCancellingThread) {
      mNoiseCancellingThread->Post(mWorkDelegateMessage,MSG_NOISE_CANCELLING_PROCESS,new DataMessageData(input_data,size));
      return 0;
   }
   return -1;
}

int NoiseCancelling::DataSplitting(const int8_t *input_data, const int size){
   if (mInputBufferData) {
      mInputBufferData->DataCombineSplitProcess(input_data, size);
   }
   return 0;
}

void NoiseCancelling::OnInputBufferData(const int8_t *input_data, const int size){
   int ret = -1;
   int count = (int)mNCObjectMap.size();
   if (count==2) {
      int processRet = -1;
      processRet = NoiseCancelling::AudioDataSplitLR(input_data,size,mOutBufferData,mSampleFormat);
      if (processRet<0) {
         LOGE("AudioDataSplitLR Error!");
      }else{
         for (int i = 0; i<count; i++) {
            auto iter = mNCObjectMap.find(i);
            processRet = iter->second->NoiseSuppressionProcess(mOutBufferData+i*size/2, size/2, mInBufferData+i*size/2);
            if (processRet<0) {
               LOGE("NoiseSuppressionProcess Error!");
               break;
            }
         }
         if (processRet==0) {
            processRet = NoiseCancelling::AudioDataCombineLR(mInBufferData, size, mOutBufferData,mSampleFormat);
            if (processRet<0) {
               LOGE("AudioDataCombineLR Error!");
            }
         }
         ret = processRet;
      }
   }else if(count == 1){
      auto iter = mNCObjectMap.find(0);
      ret = iter->second->NoiseSuppressionProcess(input_data, size, mOutBufferData);
   }else{
      memcpy(mOutBufferData, input_data, size);
      ret = 0;
   }
   if (ret==0) {
      if (mOutputBufferData) {
         mOutputBufferData->DataCombineSplitProcess(mOutBufferData, size);
      }
   }else{
      LOGE("NCProcess error!");
   }
}

void NoiseCancelling::OnOutputBufferData(const int8_t *input_data, const int size){
   if (mOutputDataDelegate) {
      mOutputDataDelegate(input_data,size);
   }
}

int NoiseCancelling::AudioDataSplitLR(const int8_t* input_data,const int size,const int8_t* output_data,VHAVSampleFormat sample_format){
   if (input_data==NULL||output_data==NULL||size<=0||sample_format == VH_AV_SAMPLE_FMT_NONE) {
      return -1;
   }
   if (sample_format == VH_AV_SAMPLE_FMT_S16){
      return AudioDataSplitLR((int16_t*)input_data, size, output_data);
   }else if(sample_format == VH_AV_SAMPLE_FMT_S32){
      return AudioDataSplitLR((int32_t*)input_data, size, output_data);
   }else if(sample_format == VH_AV_SAMPLE_FMT_FLT){
      return AudioDataSplitLR((float*)input_data, size, output_data);
   }
   return -1;
}

template<typename T>
int NoiseCancelling::AudioDataSplitLR(T* input_data, const int size, const int8_t *output_data){
   if (input_data==NULL||output_data==NULL||size<=0) {
      return -1;
   }
   T* inputData = (T*)input_data;
   T* lData = (T*)output_data;
   T* rData = (T*)((int8_t*)output_data+size/2);
   int count = size/sizeof(T);
   for (int i = 0,index = 0; i< count; i+=2, index++) {
      lData[index] = inputData[i];
      rData[index] = inputData[i+1];
   }
   return 0;
}

int NoiseCancelling::AudioDataCombineLR(const int8_t* input_data,const int size,const int8_t* output_data,VHAVSampleFormat sample_format){
   if (input_data==NULL||output_data==NULL||size<=0||sample_format == VH_AV_SAMPLE_FMT_NONE) {
      return -1;
   }
   if (sample_format == VH_AV_SAMPLE_FMT_S16) {
      return AudioDataCombineLR((int16_t*)input_data, size, output_data);
   }else if(sample_format == VH_AV_SAMPLE_FMT_S32){
      return AudioDataCombineLR((int32_t*)input_data, size, output_data);
   }else if(sample_format == VH_AV_SAMPLE_FMT_FLT){
      return AudioDataCombineLR((float*)input_data, size, output_data);
   }
   return -1;
}

template<typename T>
int NoiseCancelling::AudioDataCombineLR(T* input_data, const int size, const int8_t *output_data){
   if (input_data==NULL||output_data==NULL||size<=0) {
      return -1;
   }
   T* lData = (T*)input_data;
   T* rData = (T*)((int8_t*)input_data+size/2);
   T* outputData = (T*)output_data;
   int count = size/sizeof(T);
   for (int i = 0,index = 0; i< count; i+=2,index++) {
      outputData[i] = lData[index];
      outputData[i+1] = rData[index];
   }
   return 0;
}

int NoiseCancelling::VolumeAmplificateS16(const int8_t* audio_data,const int data_size,float alpha){
   int16_t * audioData = (int16_t *)audio_data;
   int dataLength = data_size/sizeof(int16_t);
   if (audioData == NULL || dataLength <= 0){
      return -1;
   }
   if (alpha <= 0){
      alpha = 0;
      return 0;// if alpha is 0, there is nothing to do
   }
   if (alpha > 1){
      alpha = 1.0;
   }
   int iDelta = 10922 * alpha;//32767 / 3 = 10922
   //-32768-----k1------k2------k3------k4------32767
   //there are 5 intervals
   int iK1, iK2, iK3, iK4;
   iK1 = -32768 + 2 * iDelta;
   iK2 = -iDelta;
   iK3 = iDelta;
   iK4 = 32767 - 2 * iDelta;
   int i;
   int temp = 0;
   for (i = 0; i < dataLength; i++){
      if (audioData[i] > iK4){
         temp = (32767 + audioData[i]) / 2; //audioData[i] + (32767 - audioData[i])/2
      }else if (audioData[i] < iK1){
         temp = (audioData[i] - 32768) / 2; //audioData[i] - (32768 + audioData[i])/2
      }else if (audioData[i] > iK3){
         temp = audioData[i] + iDelta;
      }else if (audioData[i] < iK2){
         temp = audioData[i] - iDelta;
      }else{
         temp = audioData[i] *2;
      }
      if (temp > 32767){
         temp = 32767;
      }else if (temp < -32768){
         temp = -32768;
      }
      audioData[i] = (short)temp;
   }
   return 0;
};

int NoiseCancelling::VolumeAmplificateFLT(const int8_t *audio_data, const int data_size,float alpha){
   float * audioData = (float *)audio_data;
   int dataLength = data_size/sizeof(float);
   if (audioData == NULL || dataLength <= 0 || alpha < 0){
      return -1;
   }
   if (alpha > 1){
      alpha = 1.0;
   }
   float iDelta = 1.0 / 3 * alpha;
   //-1.0-----k1------k2------k3------k4------1.0
   //there are 5 intervals
   float iK1, iK2, iK3, iK4;
   iK1 = -1.0 + 2 * iDelta;
   iK2 = -iDelta;
   iK3 = iDelta;
   iK4 = 1.0 - 2 * iDelta;
   int i;
   float temp = 0;
   for (i = 0; i < dataLength; i++){
      if (audioData[i] > iK4){
		  temp = (1.0 + audioData[i]) / 2;//audioData[i] + (1.0 - audioData[i])/2
      }else if (audioData[i] < iK1){
		  temp = (audioData[i] - 1.0) / 2;//audioData[i] - (1.0 + audioData[i])/2
      }else if (audioData[i] > iK3){
         temp = audioData[i] + iDelta;
      }else if (audioData[i] < iK2){
         temp = audioData[i] - iDelta;
      }else{
         temp = audioData[i] * 2;
      }
	  if (temp > 1.0){
		  temp = 1.0;
      }else if (temp < -1.0){
		  temp = -1.0;
      }
      audioData[i] = temp;
   }
   return 0;
};

NS_VH_END

