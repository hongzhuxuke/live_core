//
//  VhallPush.cpp
//  VinnyLive
//
//  Created by liwenlong on 16/6/21.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "vhall_live_push.h"
#include "../common/vhall_log.h"
#include "../encoder/media_encode.h"

#if !defined(IOS) && !defined(ANDROID)
#include "../mediaquality/media_assess.h"
#endif

#include "../muxers/media_muxer.h"
#include "../common/live_define.h"
#include "talk/base/thread.h"
#include "../encoder/media_data_send.h"
#include "../common/live_status_listener_Impl.h"
#include "../3rdparty/json/json.h"
#include "../utility/utility.h"
#include "../common/live_get_status.h"
#include "timestamp_sync.h"
#include "../os/vhall_monitor_log.h"
#include "../ratecontrol/rate_control.h"
#include "../audioprocess/noise_cancelling.h"
#include "../utility/timer.h"
#include "../utility/audio_resamples.h"
#include <srs_librtmp.h>
#include "audio_output_ts.h"
#include "../common/auto_lock.h"
#include "../ratecontrol/rate_control_interface.h"

#ifdef __cplusplus
extern "C" {
#endif
   
#include <signal.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
   
#ifdef __cplusplus
}
#endif

#define TIME_INTERVAL            10*1000 //10s

USING_NS_VH;

void SetModuleLog(std::string path, int level)
{
   if (path == ""){
      ConsoleInitParam param;
      memset(&param, 0, sizeof(ConsoleInitParam));
      param.nType = 0;
      ADD_NEW_LOG(VHALL_LOG_TYPE_CONSOLE, &param, level);
   }else{
      FileInitParam param;
      memset(&param, 0, sizeof(FileInitParam));
      param.pFilePathName = path.c_str();
      ADD_NEW_LOG(VHALL_LOG_TYPE_FILE, &param, level);
   }
}

VHallLivePush::VHallLivePush()
:mRtmpEncode(NULL),
mRtmpPublish(NULL),
mParam(NULL),
mListener(NULL),
mTSSync(NULL),
mMonitorLog(NULL),
mNoiseCancelling(NULL),
mRateControl(NULL),
mAudioResamples(NULL),
mAudioOutputTS(nullptr),
mMediaQualityAssess(nullptr) {
   
   mPushLastAudioTs = 0;
   mVolumeAmplificateSize = 0;
   mIsConnection = false;
   mIsPushAudioTs = false;
   avcodec_register_all();
   av_log_set_level(AV_LOG_FATAL);
   vhall_lock_init(&mMutex);
#ifndef WIN32
   //用于解决 Broken pipe
   sigset_t signal_mask;
   sigemptyset (&signal_mask);
   sigaddset (&signal_mask, SIGPIPE);
   int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
   if (rc != 0) {
      LOGE("block sigpipe error");
   }
   signal(SIGPIPE, SIG_IGN);
#endif
   mMonitorLog = new(std::nothrow) VHallMonitorLog();
   if (mMonitorLog) {
      mMonitorLog->SetLogMsgListener(VH_CALLBACK_1(VHallLivePush::LogReportMsg,this));
   }else{
      LOGE("m_monitor_log is NULL!");
   }
   
   mListenerImpl = new(std::nothrow) LiveStatusListenerImpl(VH_CALLBACK_2(VHallLivePush::NotifyEvent, this));
   if (mListenerImpl==NULL) {
      LOGE("m_listener_impl is NULL!");
   }
   
   mRtmpEncode = new(std::nothrow) MediaEncode();
   if (mRtmpEncode == NULL) {
      LOGE("m_rtmp_encode is NULL!");
   }
#if !defined(IOS) && !defined(ANDROID)
   mMediaQualityAssess = new(std::nothrow) MediaQualityAssess();
   if (mMediaQualityAssess == NULL) {
      LOGE("m_media_quality_assess is NULL!");
   }
#endif
   mTSSync = new(std::nothrow) TimestampSync(mRtmpEncode);
   if (mTSSync==NULL) {
      LOGE("m_ts_sync is NULL");
   }else{
      mTSSync->SetOutputDataDelegate(VH_CALLBACK_2(VHallLivePush::OnAmf0Msg, this));
   }
   mRtmpEncode->SetStatusListener(mListenerImpl);
   
   mRtmpPublish = new(std::nothrow) MediaMuxer();
   if (mRtmpPublish == NULL) {
      LOGE("m_rtmp_publish is NULL!");
   }
   
   mRtmpPublish->SetStatusListener(mListenerImpl);
   mRtmpEncode->SetOutputListener(static_cast<MediaDataSend*>(mRtmpPublish));
   
   mRateControl = new(std::nothrow) RateControl();
   if (mRateControl==NULL) {
      LOGE("mRateControl==NULL");
   }
#if defined(WIN32)
   mRateControl->SetEncoder(mRtmpEncode);
   mRtmpPublish->SetRateControl(mRateControl);
#endif
   
   mNoiseCancelling = new(std::nothrow) NoiseCancelling();
   if (mNoiseCancelling==NULL) {
      LOGE("mNoiseCancelling==NULL!");
   }else{
      mNoiseCancelling->SetOutputDataDelegate(VH_CALLBACK_2(VHallLivePush::OnNSAudioData,this));
   }

   mTimer = new(std::nothrow) Timer(TIME_INTERVAL);//10s
   if (mTimer==NULL) {
      LOGE("mTimer==NULL");
   }else{
      mTimer->SetSelectorMethod(VH_CALLBACK_0(VHallLivePush::OnTimerSelector,this));
   }

   mAudioResamples = new(std::nothrow) AudioResamples();
   if (mAudioResamples==NULL) {
      LOGE("mAudioResamples==NULL");
   }else{
      mAudioResamples->SetOutputDelegate(VH_CALLBACK_2(VHallLivePush::OnResamplesAudioData,this));
   }
   mAudioOutputTS = new(std::nothrow) AudioOutputTS();
   if (mAudioOutputTS==NULL) {
      LOGE("mAudioOutputTS==NULL");
   }
}

VHallLivePush::~VHallLivePush(){
   RemoveAllMuxer();
   VHALL_DEL(mRtmpEncode);
   VHALL_DEL(mRtmpPublish);
   VHALL_DEL(mListenerImpl);
   VHALL_DEL(mTSSync);
   VHALL_DEL(mMonitorLog);
   VHALL_DEL(mRateControl);
   VHALL_DEL(mNoiseCancelling);
   VHALL_DEL(mTimer);
   VHALL_DEL(mAudioOutputTS);
   VHALL_DEL(mAudioResamples);
#if !defined(IOS) && !defined(ANDROID)
   VHALL_DEL(mMediaQualityAssess);
#endif
   vhall_lock_destroy(&mMutex);
   LOGI("VHallLivePush::~VHallLivePush()");
}

int VHallLivePush::LiveSetParam(LivePushParam *param){
   VhallAutolock _l(&mMutex);
   if (mIsConnection) {
      LOGW("live push connected!");
      return -1;
   }
   if (param!=NULL) {
      mParam = param;
      mParam->dst_sample_rate = param->sample_rate;
      if (mRtmpEncode) {
         mRtmpEncode->LiveSetParam(mParam);
      }
#if !defined(IOS) && !defined(ANDROID)
      if (mMediaQualityAssess) {
         mParam->score_frames_period = mParam->frame_rate * 15;//get a score every 15s
         mMediaQualityAssess->Init(mParam);
         if (mRtmpEncode && mMediaQualityAssess->isInit()) {
            mRtmpEncode->SetMediaQAListener((MQADataInteract *)mMediaQualityAssess);
         }
      }
#endif
      if (mRtmpPublish) {
         mRtmpPublish->LiveSetParam(mParam);
      }
      if (mTSSync) {
         mTSSync->LiveSetParam(mParam);
      }
      if (mMonitorLog) {
         mMonitorLog->SetLiveParam(*mParam);
         mMonitorLog->SetResolution(mParam->width, mParam->height);
      }
      if (mNoiseCancelling) {
         mNoiseCancelling->Init(param->dst_sample_rate, NOISE_SUPPRESSION_LEVEL, param->ch_num, param->src_sample_fmt);
      }
      if (mAudioResamples) {
         int ret = mAudioResamples->Init(mParam->ch_num, mParam->src_sample_fmt, mParam->dst_sample_rate, mParam->ch_num, mParam->src_sample_fmt, mParam->sample_rate);
         if (ret<0) {
            LOGE("mAudioResamples Init Error Return %d",ret);
         }
      }
      if (mAudioOutputTS) {
         mAudioOutputTS->Init(mParam->ch_num, mParam->src_sample_fmt, mParam->dst_sample_rate);
      }
      return 0;
   }
   return -1;
}

int VHallLivePush::SetMonitorLogParam(const char * param){
   if (mMonitorLog) {
      return mMonitorLog->SetExtendParam(param);
   }
   return -1;
}

int VHallLivePush::SetListener(LivePushListener *listener){
   mListener = listener;
   return 0;
}

int VHallLivePush::SetRateControl(EnocoderRateController * rc){
#if defined(IOS)
   mRateControl->SetEncoder(rc);
#endif
   return 0;
}

// add a muxer return muxerId
int  VHallLivePush::AddMuxer(VHMuxerType type,void *param){
   if (mRtmpPublish) {
      int muxerId = mRtmpPublish->AddMuxer(type, param);
      if (mMonitorLog&&(type == RTMP_MUXER||type == HTTP_FLV_MUXER)) {
         mMonitorLog->AddReportLog(std::string((char*)param), muxerId, MOB_PUSH_ALIVE);
      }
      return muxerId;
   }
   return -1;
}

// remove a muxer with muxerId
int VHallLivePush::RemoveMuxer(int muxer_id){
   if (mMonitorLog) {
      mMonitorLog->RemoveReportLog(muxer_id);
   }
   if (mRtmpPublish) {
      mRtmpPublish->RemoveMuxer(muxer_id);
      return 0;
   }
   return -1;
}

// remove all muxer
int VHallLivePush::RemoveAllMuxer(){
   LOGD("VHallLivePush::RemoveAllMuxer().");
   if (mRtmpPublish) {
      mRtmpPublish->RemoveAllMuxer();
      if (mMonitorLog) {
         mMonitorLog->RemoveAllLog();
      }
      /*if (mRtmpPublish->GetMuxerStartCount()==0) {
         if (mRateControl) {
            mRateControl->Stop();
         }
         mIsConnection = false;
         mNoiseCancelling->Stop();
         mRtmpEncode->Stop();
#if !defined(IOS) && !defined(ANDROID)
         if (mMediaQualityAssess) {
            mMediaQualityAssess->StopVideoAssess();
         }
#endif
         mTSSync->StopPublish();
         mTimer->Stop();
      }*/
      return 0;
   }
   return -1;
}

// start
int VHallLivePush::StartMuxer(int muxer_id){
   VhallAutolock _l(&mMutex);
   if (!mRtmpEncode->isInit()) {
      //mNoiseCancelling->Start();
      mRtmpEncode->Start();
#if !defined(IOS) && !defined(ANDROID)
      if (mMediaQualityAssess) {
         mMediaQualityAssess->StartVideoAssess();
      }
#endif
      mTSSync->StartPublish();
      mTimer->Start();
      mAudioOutputTS->Reset();
   }
   if (mRtmpPublish) {
      mRtmpPublish->StartMuxer(muxer_id);
      if (mMonitorLog) {
         mMonitorLog->StartLog(muxer_id);
      }
      return 0;
   }
   return -1;
}

// stop
int VHallLivePush::StopMuxer(int muxer_id){
   VhallAutolock _l(&mMutex);
   if (mRtmpPublish) {
      mRtmpPublish->StopMuxer(muxer_id);
      if (mMonitorLog) {
         mMonitorLog->StopLog(muxer_id);
      }
      if (mRtmpPublish->GetMuxerStartCount()==0) {
         if (mRateControl) {
            mRateControl->Stop();
         }
         mIsConnection = false;
         mNoiseCancelling->Stop();
         mRtmpEncode->Stop();
#if !defined(IOS) && !defined(ANDROID)
         if (mMediaQualityAssess) {
            mMediaQualityAssess->StopVideoAssess();
         }
#endif
         mTSSync->StopPublish();
         mTimer->Stop();
      }
      return 0;
   }
   return -1;
}

int VHallLivePush::StopAllMuxer(){
   VhallAutolock _l(&mMutex);
   if (mRtmpPublish) {
      mRtmpPublish->StopAllMuxer();
      if (mMonitorLog) {
         mMonitorLog->StopAllLog();
      }
      if (mRtmpPublish->GetMuxerStartCount()==0) {
         mIsConnection = false;
         mNoiseCancelling->Stop();
         if (mRateControl) {
            mRateControl->Stop();
         }
         mRtmpEncode->Stop();
#if !defined(IOS) && !defined(ANDROID)
         if (mMediaQualityAssess) {
            mMediaQualityAssess->StopVideoAssess();
         }
#endif
         mTSSync->StopPublish();
         mTimer->Stop();
      }
      return 0;
   }
   return -1;
}

// get muxerStatus with muxerId
int VHallLivePush::GetMuxerStatus(int muxer_id){
   if (mRtmpPublish) {
      return mRtmpPublish->GetMuxerStatus(muxer_id);
   }
   return 2;
}

int VHallLivePush::GetDumpSpeed(int muxer_id){
   if (mRtmpPublish) {
      return mRtmpPublish->GetDumpSpeed(muxer_id);
   }
   return 0;
}

const VHMuxerType VHallLivePush::GetMuxerType(int muxer_id){
   if (mRtmpPublish) {
      return mRtmpPublish->GetMuxerType(muxer_id);
   }
   return MUXER_NONE;
};

int VHallLivePush::LivePushVideo(const char * data,const int size,const LiveExtendParam *extendParam){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      mTSSync->ResetVideoFrameTS();
      return -2;
   }
   if (mTSSync&&data!=NULL) {
	   mTSSync->LivePushVideo(data, size, extendParam);
      return 0;
   }
   return -1;
}

int VHallLivePush::LivePushAudio(const char * data,const int size){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      mTSSync->ResetVideoFrameTS();
      return -2;
   }
   if (data==NULL||size<=0) {
      return -1;
   }
   mIsPushAudioTs = false;
   if (mAudioResamples) {
      mAudioResamples->AudioResamplesProcess((int8_t*)data, size);
   }
   return 0;
}

int VHallLivePush::LivePushVideo(const char * data, const int size, const uint64_t timestamp, const LiveExtendParam *extendParam){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      return -2;
   }
   if (mRtmpEncode) {
      mRtmpEncode->EncodeVideo(data, size, timestamp,extendParam);
   }
   return 0;
}

int VHallLivePush::LivePushAudio(const char * audio_data, const int size, const uint64_t timestamp){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      mAudioOutputTS->Reset();
      return -2;
   }
   if (audio_data==NULL||size<=0) {
      return -1;
   }
   mIsPushAudioTs = true;
   if (mAudioResamples) {
      mAudioOutputTS->SetDataSizeAndTS(timestamp, size);
      mAudioResamples->AudioResamplesProcess((int8_t*)audio_data, size);
   }
   return 0;
}

int VHallLivePush::LivePushVideoHW(const char * data,const int size ,const int type, const uint64_t timestamp){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      return -2;
   }
   if (mRtmpEncode&&data!=NULL) {
      mRtmpEncode->EncodeVideoHW(data, size, type, timestamp);
      return 0;
   }
   return -1;
}

int VHallLivePush::LivePushAudioHW(const char * data,const int size, const uint64_t timestamp){
   if (!mIsConnection) {
      LOGW("rtmp connect is false!");
      return -2;
   }
   if (data==NULL||size<=0) {
      return -1;
   }
   mIsPushAudioTs = true;
   if (mAudioResamples) {
      mAudioOutputTS->SetDataSizeAndTS(timestamp, size);
      mAudioResamples->AudioResamplesProcess((int8_t*)data, size);
   }
   return 0;
}

int VHallLivePush::LivePushH264DataTs(const char * data, const int size, const int type ,const uint64_t timestamp){
   if (!mRtmpPublish || !data){
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   mRtmpPublish->OnSendVideoData(data, size, type, timestamp);
   return 0;
}

int VHallLivePush::LivePushAACDataTs(const char * data, const int size ,const int type ,const uint64_t timestamp){
   if (!mRtmpPublish || !data){
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   mRtmpPublish->OnSendAudioData(data, size, type, timestamp);
   return 0;
}

int VHallLivePush::LivePushAmf0Msg(std::string msg){
   if(mIsPushAudioTs){
      OnAmf0Msg(msg,mPushLastAudioTs);
   }else{
      if (mTSSync) {
         return mTSSync->LivePushAmf0Msg(msg);
      }
   }
   return -1;
}

std::string VHallLivePush::LiveGetRealTimeStatus(){
   VhallAutolock _l(&mMutex);
   return GetRealTimeStatus();
}

std::string VHallLivePush::GetRealTimeStatus(){
   if (mIsConnection==false) {
      return "";
   }
   VHJson::FastWriter root;
   VHJson::Value push(VHJson::objectValue);
   push["Name"] = VHJson::Value("LivePush");
   //TODD and other items in this level
   
   VHJson::Value encoders(VHJson::objectValue), muxers(VHJson::objectValue), tssync(VHJson::objectValue);
   bool ret = mRtmpEncode->LiveGetRealTimeStatus(encoders);
   if (ret){
      push["MediaEncoder"] = encoders;
   } else {
      LOGE("Get encoder realtime status failed!");
   }
   ret = mRtmpPublish->LiveGetRealTimeStatus(muxers);
   if (ret){
      push["MediaMuxer"] = muxers;
   }else{
      LOGE("Get muxer realtime status failed!");
   }
   
   ret = mTSSync->LiveGetRealTimeStatus(tssync);
   if (ret) {
      push["TSSync"] = tssync;
   }else{
      LOGE("Get TS Sync realtime status failed!");
   }
#undef write
   return root.write(push);
}

void VHallLivePush::NotifyEvent(const int type, const EventParam &param){
   switch (type) {
      case OK_PUBLISH_CONNECT:{
         if (mRtmpPublish->GetMuxerCount()<2||(mRtmpPublish->GetMuxerCount()>1&&mRtmpPublish->GetMuxerType(param.mId)!=FILE_FLV_MUXER)) {
            mIsConnection = true;
         }
      }
         break;
      case ERROR_PUBLISH_CONNECT:
      case ERROR_SEND:{
         if (mRtmpPublish->GetMuxerStartCount()<2||(mRtmpPublish->GetMuxerStartCount()>1&&mRtmpPublish->GetMuxerType(param.mId)!=FILE_FLV_MUXER)) {
            mIsConnection = false;
         }
      }
      case NEW_KEY_FRAME:{
         if (mRtmpEncode) {
            bool ret = mRtmpEncode->RequestKeyframe();
            if (ret==false) {
               LOGW("request key frame is error!");
            }
         }
      }
         break;
      default:
         break;
   }
   if (mListener && type<SERVER_IP) {
      mListener->OnEvent(type,param.mDesc);
   }
   if (mMonitorLog) {
      mMonitorLog->OnNotifyEvent(type, param);
   }
}

void VHallLivePush::OnResamplesAudioData(const int8_t * audio_data,const int size){
   if (mNoiseCancelling) {
      mNoiseCancelling->NoiseCancellingProcess((int8_t*)audio_data, size);
   }
}

void VHallLivePush::LogReportMsg(const std::string &msg){
#if defined(IOS) || defined(ANDROID)
   if (mListener) {
      mListener->OnEvent(HTTPS_REQUEST_MSG, msg);
   }
#endif
}

void VHallLivePush::OnNSAudioData(const int8_t* audio_data,const int size){
   PushPCMAudioData(audio_data, size);
}

void VHallLivePush::PushPCMAudioData(const int8_t* audio_data,const int size){
   if (mVolumeAmplificateSize>0) {
      if (mParam->src_sample_fmt==VH_AV_SAMPLE_FMT_S16) {
         NoiseCancelling::VolumeAmplificateS16((int8_t*)audio_data, size, mVolumeAmplificateSize);
      }else if(mParam->src_sample_fmt==VH_AV_SAMPLE_FMT_FLT){
         NoiseCancelling::VolumeAmplificateFLT((int8_t*)audio_data, size, mVolumeAmplificateSize);
      }
   }
   if (mIsPushAudioTs) {
      if (mRtmpEncode) {
         uint64_t ts = mAudioOutputTS->GetOutputTS(size);
         if (mParam->encode_type == ENCODE_TYPE_SOFTWARE) {
            mRtmpEncode->EncodeAudio((char *)audio_data, size, ts);
         }else{
            mRtmpEncode->EncodeAudioHW((char *)audio_data, size, ts);
         }
         mPushLastAudioTs = ts;
      }
   }else{
      if (mTSSync) {
         if (mParam->encode_type == ENCODE_TYPE_SOFTWARE) {
            mTSSync->LivePushAudio((char*)audio_data, size);
         }else{
            mTSSync->LivePushAudioHW((char*)audio_data, size);
         }
      }
   }
}

void VHallLivePush::SetVolumeAmplificateSize(float size){
   mVolumeAmplificateSize = size;
}

int VHallLivePush::OpenNoiseCancelling(bool open){
   if (mNoiseCancelling) {
      if (open) {
         mNoiseCancelling->Start();
      }else{
         mNoiseCancelling->Stop();
      }
      return 0;
   }
   return -1;
}

void VHallLivePush::RateControlSwitch(bool on){
   if(on){
      if(mRateControl){
         mRateControl->Start();
      }
   }else{
      if(mRateControl){
         mRateControl->ResetBitrate();
         mRateControl->Stop();
      }
   }
}

void VHallLivePush::OnTimerSelector(){
   if (vhall_log_enalbe) {
      std::string json = GetRealTimeStatus();
      LOGI("RealTimeStatus:%s",json.c_str());
   }
}

int VHallLivePush::OnAmf0Msg(const std::string &msg,const uint64_t timestamp){
   if (mRtmpPublish) {
      char *amf0Msg = (char *)calloc(1, msg.size()+100);
      char *dataP = amf0Msg;
      int dfSize, mdSize, obSize;
      {
         srs_amf0_t str1Amf0 = srs_amf0_create_string("@setDataFrame");
         dfSize = srs_amf0_size(str1Amf0);
         srs_amf0_serialize(str1Amf0, dataP, dfSize);
         dataP += dfSize;
         srs_amf0_free(str1Amf0);
         
         srs_amf0_t str2Amf0 = srs_amf0_create_string("onCuePoint");
         mdSize = srs_amf0_size(str2Amf0);
         srs_amf0_serialize(str2Amf0, dataP, mdSize);
         dataP += mdSize;
         srs_amf0_free(str2Amf0);
         
         srs_amf0_t objectAmf0 = srs_amf0_create_object();
         srs_amf0_t str3Amf0 = srs_amf0_create_string(msg.c_str());
         srs_amf0_object_property_set(objectAmf0, "content", str3Amf0);
      
         obSize = srs_amf0_size(objectAmf0);
         srs_amf0_serialize(objectAmf0, dataP, obSize);
         srs_amf0_free(objectAmf0);
      }
      mRtmpPublish->OnSendAmf0Msg(amf0Msg, obSize + mdSize + dfSize, SCRIPT_FRAME, timestamp);
      if (amf0Msg) {
         free(amf0Msg);
      }
      return 0;
   }
   return -1;
}
