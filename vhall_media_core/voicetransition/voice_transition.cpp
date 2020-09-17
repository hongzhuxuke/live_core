//
//  voice_transition.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/10/24.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "voice_transition.h"
#include "../common/vhall_log.h"
#include "../common/auto_lock.h"
#include "../common/live_message.h"
#include "../utility/audio_resamples.h"
#include "../utility/image_text_mixing.h"
#include "talk/base/thread.h"
#include "../common/live_message.h"
#include "voice_dictate_packing.h"
#include "../utility/data_combine_split.h"
#include "talk/base/messagehandler.h"
#include "../common/safe_buffer_data.h"
#include "../utility/utility.h"
#include "../rtmppush/audio_output_ts.h"

NS_VH_BEGIN

#define OUTPUT_SAMPLE_RATE  16000 //音频重采样后的输出采样率
#define OUTPUT_CHANNEL_NUM  1     //音频重采样后的输出声道数

class VoiceTransition::WorkDelegateMessage:public talk_base::MessageHandler,public VoiceDictatePacking::VoiceDictateDelegate {
   
public:
   WorkDelegateMessage():mVoiceTransition(NULL){
      
   };
   
   ~WorkDelegateMessage(){
      mVoiceTransition = NULL;
   };
   
   void SetObserve(VoiceTransition * voice_transition){
      mVoiceTransition = voice_transition;
   }
   
private:
   virtual void OnResult(const std::string &result, bool is_last) override {
      if (mVoiceTransition) {
         mVoiceTransition->OnResult(result, is_last);
      }
   }
   
   virtual void OnSpeedBegin() override {
      if (mVoiceTransition) {
         mVoiceTransition->OnSpeedBegin();
      }
   }
   
   virtual void OnSpeedEnd(int reason) override{
      if (mVoiceTransition) {
         mVoiceTransition->OnSpeedEnd(reason);
      }
   }/* 0 if VAD.  others, error : see E_SR_xxx and msp_errors.h  */
   
   virtual void OnMessage(talk_base::Message* msg) override{
      switch(msg->message_id){
         case MSG_VOICE_TRANSITION:{
            DataMessageData * obs = static_cast<DataMessageData*>(msg->pdata);
            if (mVoiceTransition&&mVoiceTransition->mAudioResamples) {
               mVoiceTransition->mAudioResamples->AudioResamplesProcess(obs->data_, obs->size_);
            }
         }
            break;
         case MSG_DELAT_LAST_RUN:{
            if (mVoiceTransition) {
               mVoiceTransition->OnDelayLastRun();
            }
         }
            break;
         case MSG_DELAT_SPLIT_RUN:{
            if (mVoiceTransition) {
               StringMessageData * obs = static_cast<StringMessageData*>(msg->pdata);
               mVoiceTransition->OnDelaySplitRun(obs->str_);
            }
         }
            break;
         default:
            break;
      }
      VHALL_DEL(msg->pdata);
   }
   
   VoiceTransition * mVoiceTransition;
};

VoiceTransition::VoiceTransition():
mAudioResamples(NULL),
mVoiceDictatePacking(NULL),
mVoiceTransitionDelegate(NULL),
mVoiceTransitionThread(NULL),
mImageTextMixing(NULL),
mWorkDelegateMessage(NULL),
mVideoQueue(NULL),
mAudioQueue(NULL),
mInDataCombineSplit(NULL),
mAudioOutputTS(NULL){
   vhall_lock_init(&mMutex);
   mFrameRate = 0;
   mLastTextLength = "";
   mIsTransitionFinish = false;
   mDataBufferTime = 1500;//ms
   mIsStarted = false;
   mVoiceDictatePacking = new(std::nothrow) VoiceDictatePacking();
   if (mVoiceDictatePacking) {
   }else{
      LOGE("mVoiceDictatePacking new is error!");
   }
   mImageTextMixing = new(std::nothrow) ImageTextMixing();
   if (mImageTextMixing==NULL) {
      LOGE("mImageTextMixing is nil.");
   }
   mVoiceTransitionThread = new(std::nothrow) talk_base::Thread();
   if (mVoiceTransitionThread) {
      if (!mVoiceTransitionThread->started()) {
         mVoiceTransitionThread->Start();
      }
      mVoiceTransitionThread->Restart();
   }else{
      LOGE("mVoiceTransitionThread new is error!");
   }
   mWorkDelegateMessage = new(std::nothrow) WorkDelegateMessage();
   if (mWorkDelegateMessage==NULL) {
      LOGE("new WorkDelegateMessage() is error!");
   }
   mVideoQueue = new(std::nothrow) SafeDataQueue();
   if (mVideoQueue==NULL) {
      LOGE("video SafeDataQueue new error!");
   }
   mAudioQueue = new(std::nothrow) SafeDataQueue();
   if (mAudioQueue==NULL) {
      LOGE("audio SafeDataQueue new error!");
   }
   mInDataCombineSplit = new(std::nothrow) DataCombineSplit();
   if (mInDataCombineSplit==NULL) {
      LOGE("DataCombineSplit new error!");
   }else{
      mInDataCombineSplit->SetOutputDataDelegate(VH_CALLBACK_2(VoiceTransition::OnCombineSplitData, this));
   }
   mDataPool = new SafeDataPool(50);
   if (mDataPool==NULL) {
      LOGE("SafeDataPool new error!");
   }
   mAudioOutputTS = new(std::nothrow) AudioOutputTS();
   if (mAudioOutputTS==NULL) {
      LOGE("mAudioOutputTS==NULL");
   }
}

VoiceTransition::~VoiceTransition(){
   VHALL_THREAD_DEL(mVoiceTransitionThread);
   VHALL_DEL(mImageTextMixing);
   mVoiceTransitionDelegate = NULL;
   VHALL_DEL(mAudioResamples);
   VHALL_DEL(mVoiceDictatePacking);
   VHALL_DEL(mWorkDelegateMessage);
   VHALL_DEL(mInDataCombineSplit);
   VHALL_DEL(mVideoQueue);
   VHALL_DEL(mAudioQueue);
   VHALL_DEL(mDataPool);
   VHALL_DEL(mAudioOutputTS);
   vhall_lock_destroy(&mMutex);
}

int VoiceTransition::SetAudioInfo(const int channel_num, const VHAVSampleFormat sample_fmt,const int sample_rate){
   VhallAutolock _l(&mMutex);
   if (mAudioResamples==NULL) {
      mAudioResamples = new(std::nothrow) AudioResamples();
   }
   if (mAudioResamples) {
	  mAudioResamples->Init(OUTPUT_CHANNEL_NUM, VH_AV_SAMPLE_FMT_S16, OUTPUT_SAMPLE_RATE, channel_num, sample_fmt, sample_rate);
      mAudioResamples->SetOutputDelegate(VH_CALLBACK_2(VoiceTransition::OnResamplesAudioData,this));
   }else{
      LOGE("AudioResamples new is error!");
   }
   if (mVoiceDictatePacking) {
      mVoiceDictatePacking->SetDelegate(mWorkDelegateMessage);
   }
   if (mAudioQueue) {
	   int num = sample_rate *mDataBufferTime/1000.0/PCM_FRAME_SIZE;
	   mAudioQueue->SetMaxNum(num);
   }
   if (mInDataCombineSplit) {
      int size = PCM_FRAME_SIZE*Utility::GetBitNumWithSampleFormat(sample_fmt)/8*channel_num;
      mInDataCombineSplit->Init(size);
   }
   mAudioOutputTS->Init(channel_num, sample_fmt, sample_rate);
   return 0;
}

int VoiceTransition::SetVideoInfo(const int width,const int height,const EncodePixFmt in_pix_fmt,const int frame_rate){
   VhallAutolock _l(&mMutex);
   mFrameRate = frame_rate;
   mVideoHeight = height;
   mVideoWidth = width;
   enum AVPixelFormat in_pixel_format = AV_PIX_FMT_NONE;
   if (mImageTextMixing) {
      if (in_pix_fmt==ENCODE_PIX_FMT_YUV420P_I420||in_pix_fmt==ENCODE_PIX_FMT_YUV420P_YV12) {
         in_pixel_format = AV_PIX_FMT_YUV420P;
      }else if(in_pix_fmt==ENCODE_PIX_FMT_YUV420SP_NV21){
         in_pixel_format = AV_PIX_FMT_NV21;
      }else if(in_pix_fmt==ENCODE_PIX_FMT_YUV420SP_NV12){
         in_pixel_format = AV_PIX_FMT_NV12;
      }
      mImageTextMixing->Init(width, height, in_pixel_format);
   }
   if (mVideoQueue) {
	   int num = frame_rate * mDataBufferTime/1000.0;
	   mVideoQueue->SetMaxNum(num);
   }
   return 0;
}

void VoiceTransition::SetDelegate(VoiceTransitionDelegate *delegate){
   mVoiceTransitionDelegate = delegate;
}

void VoiceTransition::SetBufferTime(const int time){
   mDataBufferTime = time;
}

int VoiceTransition::StartPrepare(const std::string &app_id){
   VhallAutolock _l(&mMutex);
   if (mIsStarted) {
      return 0;
   }
   if (!mVoiceTransitionThread->started()) {
      mVoiceTransitionThread->Start();
   }
   mExtendParamList.clear();
   mVoiceTransitionThread->Restart();
   if (mWorkDelegateMessage) {
      mWorkDelegateMessage->SetObserve(this);
   }
   if (mVoiceDictatePacking) {
      int ret = mVoiceDictatePacking->MSPLogin(app_id);
      if (ret<0) {
         LOGE("MSPLogin error!");
         return ret;
      }
      mVoiceDictatePacking->Init();
      ret = mVoiceDictatePacking->SessionBegin();
      if (ret<0) {
         LOGI("QISRSessionBegin error!");
         return ret;
      }
      return 0;
   }
   mIsStarted = true;
   mAudioOutputTS->Reset();
   return -1;
}

bool VoiceTransition::IsStarted(){
   return mIsStarted;
}

int VoiceTransition::StopPrepare(){
   VhallAutolock _l(&mMutex);
   mVoiceTransitionThread->Clear(mWorkDelegateMessage);
   mVoiceTransitionThread->Stop();
   if (mVoiceDictatePacking) {
      mVoiceDictatePacking->SessionEnd();
      mVoiceDictatePacking->MSPLogout();
   }
   if (mAudioQueue) {
      mAudioQueue->ClearAllQueue();
   }
   if (mVideoQueue) {
      mVideoQueue->ClearAllQueue();
   }
   mExtendParamList.clear();
   mIsStarted = false;
   return 0;
}

void VoiceTransition::InputVideoData(const int8_t *data,const int size, const uint64_t timestamp, const LiveExtendParam *extendParam){
   if(extendParam){
      mExtendParamList.push_back(*extendParam);
   }
   if (mVideoQueue->GetMaxNum()<=0) {
      if (mVoiceTransitionDelegate) {
         if (mExtendParamList.size()>0) {
            LiveExtendParam param = std::move(mExtendParamList.front());
            mVoiceTransitionDelegate->OnOutputVideoData(data, size, timestamp, &param);
            mExtendParamList.pop_front();
         }else{
            mVoiceTransitionDelegate->OnOutputVideoData(data, size, timestamp ,NULL);
         }
      }
      return;
   }
   bool ret = false;
   if (mVideoQueue) {
      ret = mVideoQueue->IsFull();
   }else{
       goto VideoPush;
   }
   if (ret) {
      SafeData *mVideoItem = NULL;
      if (mVideoQueue) {
         mVideoItem = mVideoQueue->ReadQueue();
      }
      if (mVoiceTransitionDelegate&&mVideoItem) {
		  if (mImageTextMixing) {
			  if (mVoiceText != mLastTextLength){
				  mImageTextMixing->AppleFilters(mVoiceText);
				  mLastTextLength = mVoiceText;
			  }
			  mImageTextMixing->MixingProcess((int8_t *)mVideoItem->mData, mVideoItem->mSize);
		   }
         if (mExtendParamList.size()>0) {
            LiveExtendParam param = std::move(mExtendParamList.front());
            mVoiceTransitionDelegate->OnOutputVideoData((int8_t*)mVideoItem->mData, mVideoItem->mSize, mVideoItem->mTs, &param);
            mExtendParamList.pop_front();
         }else{
            mVoiceTransitionDelegate->OnOutputVideoData((int8_t*)mVideoItem->mData, mVideoItem->mSize, mVideoItem->mTs, NULL);
         }
      }
      if (mVideoItem) {
         mVideoItem->SelfRelease();
      }
      goto VideoPush;
   }else{
VideoPush:
      SafeData * safeData = mDataPool->GetSafeData((char*)data, size, 0, timestamp);
      mVideoQueue->PushQueue(safeData);
   }
}

void VoiceTransition::InputAudioData(const int8_t *data, const int size, const uint64_t timestamp){
	if (mVoiceTransitionThread&&mVoiceTransitionThread->size()<100) {
      if (!mVoiceTransitionThread->started()) {
         LOGE("mVoiceTransitionThread not launch!");
      }
		mVoiceTransitionThread->Post(mWorkDelegateMessage, MSG_VOICE_TRANSITION, new DataMessageData(data, size));
	}
   if (mInDataCombineSplit) {
      mAudioOutputTS->SetDataSizeAndTS(timestamp, size);
      mInDataCombineSplit->DataCombineSplitProcess(data, size);
   }
}

void VoiceTransition::SetFontSize(const int font_size){
   if (mImageTextMixing) {
      mImageTextMixing->SetFontSize(font_size);
   }
}

void VoiceTransition::SetFontFile(const std::string &font_file_path){
   if (mImageTextMixing) {
      mImageTextMixing->SetFontFile((std::string &)font_file_path);
   }
}

void VoiceTransition::SetFontColor(const std::string &font_color){
   if (mImageTextMixing) {
      mImageTextMixing->SetFontColor((std::string &)font_color);
   }
}

void VoiceTransition::SetAccent(const std::string&accent){
   if (mVoiceDictatePacking) {
      mVoiceDictatePacking->SetAccent((std::string &)accent);
   }
}

void VoiceTransition::SetTextPosition(enum TextPosition position){
   if (mImageTextMixing) {
      std::string textPosition;
      if (position == TextPositionBottom) {
         textPosition = "h-text_h*2";
      }else if(position == TextPositionTop){
         textPosition = "text_h";
      }
	   mImageTextMixing->SetTextPositionY(textPosition);
   }
}

void VoiceTransition::SetFixTextBounds(const bool enable){
   if (mImageTextMixing) {
      mImageTextMixing->SetFixTextBounds(enable);
   }
}

void VoiceTransition::SetFontBorderColor(const std::string &border_color){
   if (mImageTextMixing) {
      mImageTextMixing->SetFontBorderColor(border_color);
   }
}

void VoiceTransition::SetFontBorderW(const int w){
   if (mImageTextMixing) {
      mImageTextMixing->SetFontBorderW(w);
   }
}

void VoiceTransition::OnResult(const std::string &result, bool is_last){
   mIsTransitionFinish = is_last;
   int fontSize = mImageTextMixing->GetFontSize();
   
   if((mVoiceText.length()+result.length())*fontSize >= mVideoWidth*0.8&&result.length()>1){
      mVoiceText = result.substr(1,result.length()-1);
   }else{
      mVoiceText += result;
   }
   if (mVoiceTransitionDelegate) {
      mVoiceTransitionDelegate->OnResult(mVoiceText, is_last);
   }
   if (is_last) {
	   mVoiceTransitionThread->PostDelayed(400, mWorkDelegateMessage, MSG_DELAT_LAST_RUN);
   }
}

void VoiceTransition::OnSpeedBegin(){
   LOGI("OnSpeedBegin");
}

void VoiceTransition::OnSpeedEnd(int reason){
   mVoiceText = "";
   LOGI("OnSpeedEnd reason:%d",reason);
}

void VoiceTransition::OnResamplesAudioData(const int8_t *audio_data,const int size){
   OnVDProcess(audio_data, size);
}

void VoiceTransition::OnVDProcess(const int8_t*data_pcm,const int size){
   if (mVoiceDictatePacking) {
      mVoiceDictatePacking->VDProcess(data_pcm,size);
   }
}

void VoiceTransition::OnCombineSplitData(const int8_t* audio_data,const int size){
   uint64_t ts = mAudioOutputTS->GetOutputTS(size);
   if (mAudioQueue->GetMaxNum()<=0) {
      if (mVoiceTransitionDelegate) {
         mVoiceTransitionDelegate->OnOutputAudioData(audio_data, size,ts);
      }
      return;
   }
   bool ret = false;
   if (mAudioQueue) {
      ret = mAudioQueue->IsFull();
   }else{
      goto AudioPush;
   }
   if (ret) {
      SafeData *mAudioItem = NULL;
      if (mAudioQueue) {
         mAudioItem = mAudioQueue->ReadQueue();
      }
      if (mVoiceTransitionDelegate&&mAudioItem) {
         mVoiceTransitionDelegate->OnOutputAudioData((int8_t*)mAudioItem->mData, mAudioItem->mSize,mAudioItem->mTs);
      }
      if (mAudioItem) {
         mAudioItem->SelfRelease();
      }
      goto AudioPush;
   }else{
AudioPush:
      SafeData * safeData = mDataPool->GetSafeData((char*)audio_data, size, 0, ts);
      mAudioQueue->PushQueue(safeData);
   }
}

void VoiceTransition::OnDelayLastRun(){
	if (mIsTransitionFinish) {
      mVoiceText = "";
   }
}

void VoiceTransition::OnDelaySplitRun(const std::string& text){
   mVoiceText = text;
   if (mVoiceTransitionDelegate) {
	   mVoiceTransitionDelegate->OnResult(mVoiceText, mIsTransitionFinish);
   }
}
NS_VH_END
