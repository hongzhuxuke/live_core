#include "vhall_live_api.h"
#include "vhall_live.h"
#include "../os/vhall_monitor_log.h"
#include "../muxers/media_muxer.h"
#include "../common/vhall_log.h"

std::string GetVersion(){
   return SDK_VERSION;
}

VhallLiveApi::VhallLiveApi(const LiveCreateType liveType):
p_vinny_live(NULL){
   LiveCreate(liveType);
}

VhallLiveApi::VhallLiveApi(const LiveCreateType liveType,
                           const char *logFilePath):
p_vinny_live(NULL){
   
   LiveCreate(liveType, logFilePath);
}

VhallLiveApi::~VhallLiveApi(){
   
   LiveDeatory();
}

int VhallLiveApi::LiveCreate(const LiveCreateType liveType)
{
   return LiveCreate(liveType,NULL);
}

int VhallLiveApi::LiveCreate(const LiveCreateType liveType,const char *logFilePath)
{
   REMOVE_ALL_LOG;
   ConsoleInitParam param;
   param.nType = 1;
   ADD_NEW_LOG(VhallLogType::VHALL_LOG_TYPE_CONSOLE, &param, VhallLogLevel::VHALL_LOG_LEVEL_DEBUG);
   p_vinny_live = new VhallLive(logFilePath);
   if (p_vinny_live == NULL) {
      LOGE("p_vinny_live new failed.");
      return -1;
   }
   if (liveType == LIVE_TYPE_PUBLISH) {
      p_vinny_live->CreateVhallPush();
   }else if(liveType == LIVE_TYPE_PLAY){
      p_vinny_live->CreateVhallPlayer();
   }
   return 0;
}

void VhallLiveApi::LiveEnableDebug(const bool enable)
{
   VHALL_LOG_ENABLE(enable);
}

int VhallLiveApi::LiveAddPlayerObs(LiveObs * obs)
{
   if (!p_vinny_live || !obs)
   {
      LOGE("p_vinny_live or param is NULL!");
      return -1;
   }
   p_vinny_live->AddPlayerObs(obs);
   return 0;
}

int VhallLiveApi::LiveAddPushObs(LivePushListener* obs){
   if (!p_vinny_live || !obs)
   {
      LOGE("p_vinny_live or param is NULL!");
      return -1;
   }
   p_vinny_live->AddPushObs(obs);
   return 0;
}

int VhallLiveApi::LiveSetParam(const char * param,const LiveCreateType liveType)
{
   if (!p_vinny_live || !param)
   {
      LOGE("p_vinny_live or param is NULL!");
      return -1;
   }
   return p_vinny_live->SetParam(param,liveType);
}

int VhallLiveApi::SetMonitorLogParam(const char * param){
   if (!p_vinny_live || !param)
   {
      LOGE("p_vinny_live or param is NULL!");
      return -1;
   }
   return p_vinny_live->SetMonitorLogParam(param);
}

int VhallLiveApi::LiveStartPublish(const char * url)
{
   if (!p_vinny_live || !url)
   {
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   return p_vinny_live->StartPublish(url);
}

int VhallLiveApi::LiveStopPublish(void)
{
   if (!p_vinny_live)
   {
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   p_vinny_live->StopPublish();
   return 0;
}

int VhallLiveApi::LiveStartRecv(const char * urls)
{
   if (!p_vinny_live || !urls)
   {
      LOGE("p_vinny_live or url is NULL!");
      return -1;
  	}
   return p_vinny_live->StartRecv(urls);
}

int VhallLiveApi::LiveStopRecv(void)
{
   if (!p_vinny_live)
   {
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   p_vinny_live->StopRecv();
   return 0;
}

int VhallLiveApi::LivePushVideoData(const char * data, int size){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL");
      return -1;
   }
   p_vinny_live->PushVideoData(data, size);
   return 0;
}

int VhallLiveApi::LivePushAudioData(const char * data, int size){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   p_vinny_live->PushAudioData(data, size);
   return 0;
}

int VhallLiveApi::LivePushVideoData(const char * data, const int size, const uint64_t timestamp){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL");
      return -1;
   }
   p_vinny_live->LivePushVideo(data, size, timestamp);
   return 0;
}

int VhallLiveApi::LivePushAudioData(const char * data,const int size, const uint64_t timestamp){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   p_vinny_live->LivePushAudio(data, size, timestamp);
   return 0;
}

int VhallLiveApi::LivePushAACData(const char * data,
                                  const int size,
                                  const uint64_t timestamp){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL");
      return -1;
   }
   p_vinny_live->PushAACData(data, size, timestamp);
   return 0;
}

int VhallLiveApi::LivePushH264Data(const char * data,
                                   const int size,
                                   const int type,
                                   const uint64_t timestamp){
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   p_vinny_live->PushH264Data(data, size, type, timestamp);
   return 0;
}

int VhallLiveApi::LivePushH264DataTs(const char * data,
                                     const int size,
                                     const int type,
                                     const uint64_t timestamp)
{
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   return p_vinny_live->LivePushH264DataTs(data, size, type, timestamp);
}

int VhallLiveApi::LivePushAACDataTs(const char * data,
                                    const int size,
                                    const int type,
                                    const uint64_t timestamp)
{
   if (!p_vinny_live || !data)
   {
      LOGE("p_vinny_live or data is NULL!");
      return -1;
   }
   return p_vinny_live->LivePushAACDataTs(data, size, type, timestamp);
}

void VhallLiveApi::LiveDeatory()
{
   VHALL_DEL(p_vinny_live);
   REMOVE_ALL_LOG;
}

int VhallLiveApi::GetPlayerRealityBufferTime()
{
   if (!p_vinny_live){
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   return p_vinny_live->GetPlayerBufferTime();
}

int VhallLiveApi::SetVolumeAmplificateSize(float size){
   if (!p_vinny_live){
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   return p_vinny_live->SetVolumeAmplificateSize(size);;
}

int VhallLiveApi::OpenNoiseCancelling(bool open){
   if (!p_vinny_live){
      LOGE("p_vinny_live is NULL!");
      return -1;
   }
   return p_vinny_live->OpenNoiseCancelling(open);
}
