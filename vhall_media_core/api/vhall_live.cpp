#include "vhall_live.h"
#include "../rtmppush/vhall_live_push.h"
#include "../os/vhall_monitor_log.h"
#include "../utility/utility.h"
#include "../common/live_obs.h"
#include "../3rdparty/json/json.h"
#include "../common/vhall_log.h"
#include "../common/live_message.h"
#include "../common/live_define.h"
#include "../rtmpplayer/vhall_live_player.h"
#include "../common/live_message.h"
#include "../os/vhall_play_monitor_log.h"

#ifndef WIN32
	#include <pthread.h>
	#include <signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
   
#include <signal.h>
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
   
#ifdef __cplusplus
}
#endif

std::string VhallLive::m_documents_path = "";

VhallLive::VhallLive(const char *logFilePath):
m_event_thread(NULL),
m_vhall_player(NULL),
m_vhall_push(NULL){
   m_muxer_id = 0;
   m_listener = NULL;
   mPlayerObs = NULL;
   mMonitorLog = NULL;
   if (logFilePath) {
      //初始化保存文件的日志系统
      m_documents_path = std::string(logFilePath);
   }
   av_log_set_level(AV_LOG_FATAL);
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
   
   m_event_thread = new(std::nothrow) talk_base::Thread();
   if(m_event_thread== nullptr){
      LOGE("m_event_thread is null.");
   }
   mPushParam.is_quality_limited = false;
#if defined(IOS)
   mPushParam.is_adjust_bitrate = true;
#endif
   mPushParam.dst_sample_rate = 44100;
//   mPushParam.is_open_noise_suppression = false;
//   mPushParam.video_process_filters = VIDEO_PROCESS_DENOISE;
//   mPushParam.is_http_proxy = true;
//   mPushParam.proxy.type = PPROXY_HTTPS;
//   mPushParam.proxy.host = "192.168.1.124";
//   mPushParam.proxy.port = 8080;
//   mPushParam.proxy.username = "fuzhuang";
//   mPushParam.proxy.password = "vhall.123";
}

VhallLive::~VhallLive(){
   VHALL_THREAD_DEL(m_event_thread)
   VHALL_DEL(m_vhall_player);
   VHALL_DEL(m_vhall_push);
   VHALL_DEL(mMonitorLog);
   m_listener = NULL;
   mPlayerObs = NULL;
   LOGI("VhallLive::~VhallLive()");
}

void VhallLive::CreateVhallPush(){
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_push==NULL) {
      m_vhall_push = new VHallLivePush();
   }
}

void VhallLive::CreateVhallPlayer(){
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_player==NULL) {
      m_vhall_player = new(std::nothrow) VHallLivePlayer();
      mMonitorLog = new(std::nothrow) VHallPlayMonitor();
      if (mMonitorLog) {
         mMonitorLog->SetLogMsgListener(VH_CALLBACK_1(VhallLive::LogReportMsg, this));
      }else{
         LOGE("mMonitorLog new error!");
      }
   }
}

void VhallLive::AddPlayerObs(LiveObs * obs){
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_player) {
      mPlayerObs = obs;
      m_vhall_player->AddObs(this);
   }
}

void VhallLive::AddPushObs(LivePushListener * obs){
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_push) {
      m_listener = obs;
      m_vhall_push->SetListener(this);
      m_vhall_push->SetRateControl(this);
   }
}

int VhallLive::SetParam(const char * param,const LiveCreateType liveType){
   std::lock_guard<std::mutex> lock(mMutex);
   if (liveType == LIVE_TYPE_PUBLISH) {
      if (!OnSetPushParam(&mPushParam, param)) {
         LOGE("setParam error!");
         return -1;
      }
      if (m_vhall_push) {
         m_vhall_push->LiveSetParam(&mPushParam);
      }
   }else{
      if (!OnSetPlayerParam(&mPlayerParam, param)) {
         LOGE("setParam error!");
         return -1;
      }
      if (m_vhall_player) {
         m_vhall_player->LiveSetParam(&mPlayerParam);
      }
      if (mMonitorLog) {
         mMonitorLog->Init(&mPushParam);
      }
   }
   return 0;
}

int VhallLive::GetPlayerBufferTime(){
   if (m_vhall_player) {
      return m_vhall_player->GetPlayerBufferTime();
   }
   return 0;
}

int VhallLive::SetVolumeAmplificateSize(float size){
   if (m_vhall_push) {
      m_vhall_push->SetVolumeAmplificateSize(size);
      return 0;
   }
   return -1;
}

int VhallLive::OpenNoiseCancelling(bool open){
   if (m_vhall_push) {
      return m_vhall_push->OpenNoiseCancelling(open);
   }
   return -1;
}

int VhallLive::StartPublish(const char * url){
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_push) {
      if(!m_event_thread->started()){
         m_event_thread->Start();
      }
      m_event_thread->Restart();
      m_event_thread->Post(this,MSG_UPLOAD_SPEED);
      int muxerId = -1;
      m_vhall_push->RemoveAllMuxer();
      if (mLiveFmt==0||mLiveFmt==1) {
         muxerId = m_vhall_push->AddMuxer(RTMP_MUXER, (void*)url);
      }else{
         muxerId = m_vhall_push->AddMuxer(HTTP_FLV_MUXER, (void*)url);
      }
      if (muxerId>0) {
         m_muxer_id = muxerId;
      }
      m_vhall_push->StartMuxer(m_muxer_id);
      return 0;
   }
   return -1;
}

void VhallLive::StopPublish(void) {
   std::lock_guard<std::mutex> lock(mMutex);
   if (m_vhall_push) {
      m_vhall_push->StopAllMuxer();
      m_vhall_push->RemoveAllMuxer();
   }
   m_event_thread->Post(this,MSG_DETACH_EVENT_THREAD);
   m_event_thread->Stop();
}

int VhallLive::StartRecv(const char * url){
   std::lock_guard<std::mutex> lock(mMutex);
   if(!m_event_thread->started()){
      m_event_thread->Start();
   }
   m_event_thread->Restart();
   if (m_vhall_player) {
      m_vhall_player->SetBufferTime(mPlayerParam.buffer_time);
      if (mLiveFmt==2) {
         m_vhall_player->SetDemuxer(HTTP_FLV_MUXER);
      }else{
         m_vhall_player->SetDemuxer(RTMP_MUXER);
      }
      if (m_vhall_player) {
         m_vhall_player->Start(url);
      }
      if (mMonitorLog) {
         mMonitorLog->StartPlay(url);
      }
      return 0;
   }
   return -1;
}

void VhallLive::StopRecv(void) {
   std::lock_guard<std::mutex> lock(mMutex);
   m_event_thread->Clear(this);
   m_event_thread->Post(this,MSG_DETACH_EVENT_THREAD);
   m_event_thread->Stop();
   if (m_vhall_player) {
      m_vhall_player->Stop();
   }
   if (mMonitorLog) {
      mMonitorLog->StopPlay();
   }
}

void VhallLive::PushAudioData(const char * data, int size){
   if (m_vhall_push) {
      m_vhall_push->LivePushAudio(data, size);
   }
}

void VhallLive::PushVideoData(const char * data, int size){
   if (m_vhall_push) {
      m_vhall_push->LivePushVideo(data, size);
   }
}

void VhallLive::LivePushAudio(const char * audio_data,
                              const int size,
                              const uint64_t timestamp){
   if (m_vhall_push) {
      m_vhall_push->LivePushAudio(audio_data, size, timestamp);
   }
}

void VhallLive::LivePushVideo(const char * data,
                              const int size,
                              const uint64_t timestamp){
   if (m_vhall_push) {
      m_vhall_push->LivePushVideo(data, size, timestamp);
   }
}

void VhallLive::PushH264Data(const char * data, int size, int type, const uint64_t timestamp){
   if (m_vhall_push) {
      m_vhall_push->LivePushVideoHW(data, size, type, timestamp);
   }
}

// pcm(暂时使用pcm数据)
void VhallLive::PushAACData(const char * data, int size, const uint64_t timestamp){
   if (m_vhall_push) {
      m_vhall_push->LivePushAudioHW(data, size, timestamp);
   }
}

int VhallLive::LivePushH264DataTs(const char * data, const int size, const int type ,const uint64_t timestamp){
   if (m_vhall_push) {
      return m_vhall_push->LivePushH264DataTs(data, size, type, timestamp);
   }
   return -1;
}

int VhallLive::LivePushAACDataTs(const char * data, const int size , const int type ,const uint64_t timestamp){
   if (m_vhall_push) {
      return m_vhall_push->LivePushAACDataTs(data, size, type, timestamp);
   }
   return -1;
}

void VhallLive::OnMessage(talk_base::Message * msg)
{
   switch(msg->message_id) {
      case MSG_UPLOAD_SPEED:{
         OnGetUplaodSpeed();
      }
         break;
      case MSG_EVENT:{
         EventMessageData * obs = static_cast<EventMessageData *>(msg->pdata);
         if (m_listener) {
            m_listener->OnEvent(obs->type_, obs->param_.mDesc);
         }
         if (mPlayerObs) {
            mPlayerObs->OnEvent(obs->type_, obs->param_.mDesc);
         }
      }
         break;
      case MSG_DETACH_EVENT_THREAD:{
         if (m_listener) {
            m_listener->OnJNIDetachEventThread();
         }
         if (mPlayerObs) {
            mPlayerObs->OnJNIDetachEventThread();
         }
      }
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void VhallLive::OnGetUplaodSpeed(){
   if (m_vhall_push) {
      int speed = m_vhall_push->GetDumpSpeed(m_muxer_id);
      std::string s = Utility::ToString(speed);
      m_listener->OnEvent(INFO_SPEED_UPLOAD, s);
   }
   m_event_thread->PostDelayed(1000,this,MSG_UPLOAD_SPEED);
}

bool VhallLive::OnSetPlayerParam(LivePlayerParam * outparam, const std::string inparam)
{
   VHJson::Reader reader;
   VHJson::Value root;
   LOGI("VinnyLive::OnSetParam %s", inparam.c_str());
   if (!reader.parse(inparam, root, false)){
      return false;
   }
   outparam->video_decoder_mode      = root["video_decoder_mode"].asInt();
   outparam->watch_timeout           = root["watch_timeout"].asInt();
   outparam->watch_reconnect_times   = root["watch_reconnect_times"].asInt();
   outparam->buffer_time             = root["buffer_time"].asInt();
   
   outparam->device_type             = root["device_type"].asString();
   outparam->device_identifier       = root["device_identifier"].asString();
   outparam->platform                = root["platform"].asInt();
   
   mLiveFmt                          = root["live_format"].asInt();
   
   return true;
}

int VhallLive::SetMonitorLogParam(const char * param){
   int ret = -1;
   if (m_vhall_push) {
      ret = m_vhall_push->SetMonitorLogParam(param);
      if (ret<0) {
         return ret;
      }
   }
   if (mMonitorLog) {
      return mMonitorLog->SetExtendParam(param);
   }
   return 0;
}

bool VhallLive::OnSetPushParam(LivePushParam * outparam, const std::string inparam)
{
   VHJson::Reader reader;
   VHJson::Value root;
   LOGI("VhallLive::OnSetParam %s", inparam.c_str());
   if (!reader.parse(inparam, root, false)){
      return false;
   }
   outparam->width 		             = root["width"].asInt();
   outparam->height		             = root["height"].asInt();
   outparam->frame_rate 	          = root["frame_rate"].asInt();
   outparam->bit_rate 		          = root["bit_rate"].asInt();
   outparam->gop_interval            = root["gop"].asDouble(); //s
   
   outparam->sample_rate             = root["sample_rate"].asInt();
   outparam->ch_num 		             = root["ch_num"].asInt();
   outparam->audio_bitrate	          = root["audio_bitrate"].asInt();
   outparam->src_sample_fmt          = (VHAVSampleFormat)root["src_sample_fmt"].asInt();
   outparam->encode_sample_fmt       = VH_AV_SAMPLE_FMT_FLTP;
   
   outparam->publish_timeout         = root["publish_timeout"].asInt();
   int count = root["publish_reconnect_times"].asInt();
   if (count<=0) {count = 1;}
   outparam->publish_reconnect_times = count;
   
   outparam->encode_type             = (EncodeType)root["encode_type"].asInt();
   outparam->encode_pix_fmt          = (EncodePixFmt)root["encode_pix_fmt"].asInt();
   outparam->live_model      = (LiveModel)root["live_publish_model"].asInt();
   
   outparam->device_type             = root["device_type"].asString();
   outparam->device_identifier       = root["device_identifier"].asString();
   outparam->platform                = root["platform"].asInt();
   mLiveFmt                          = root["live_format"].asInt();
   
   return true;
}

int VhallLive::OnEvent(int type, const std::string content){
   std::lock_guard<std::mutex> lock(mMutex);
   if (mMonitorLog) {
      mMonitorLog->OnNotifyEvent(type, content);
   }
   if (m_event_thread&&!m_event_thread->IsQuitting()) {
      EventParam param;
      param.mDesc = content;
      m_event_thread->Post(this,MSG_EVENT, new EventMessageData(type,param));
   }
   return 0;
}

bool VhallLive::RateControlAdjust(NetworkState state){
    if(m_listener){
        return m_listener->OnRateControlAdjust(state);
    }
    return false;
}

void VhallLive::OnPushBufferInfo(PushBufferInfo info){
   if(m_listener){
      return m_listener->OnPushBufferInfo(info.print());
   }
}

void VhallLive::LogReportMsg(const std::string &msg){
#if defined(IOS) || defined(ANDROID)
   if (mPlayerObs) {
      mPlayerObs->OnEvent(HTTPS_REQUEST_MSG, msg);
   }
#endif
}

int VhallLive::OnJNIDetachEventThread(){
   return 0;
}

int VhallLive::OnRawVideo(const char *data, int size, int w, int h){
   if (mPlayerObs) {
      return mPlayerObs->OnRawVideo(data, size, w, h);
   }
   return -1;
}

int VhallLive::OnJNIDetachVideoThread(){
   if (mPlayerObs) {
      return mPlayerObs->OnJNIDetachVideoThread();
   }
   return 0;
}

int VhallLive::OnRawAudio(const char *data, int size){
   if (mPlayerObs) {
      return mPlayerObs->OnRawAudio(data, size);
   }
   return 0;
}

int VhallLive::OnJNIDetachAudioThread(){
   if (mPlayerObs) {
      return mPlayerObs->OnJNIDetachAudioThread();
   }
   return 0;
}

int VhallLive::OnHWDecodeVideo(const char *data, int size, int w, int h, int64_t ts){
   if (mPlayerObs) {
      return mPlayerObs->OnHWDecodeVideo(data, size, w, h, ts);
   }
   return 0;
}

DecodedVideoInfo * VhallLive::GetHWDecodeVideo(){
   if (mPlayerObs) {
     return mPlayerObs->GetHWDecodeVideo();
   }
   return 0;
}
