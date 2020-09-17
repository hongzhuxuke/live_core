//
//  vhall_play_monitor_log.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/11/23.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "vhall_play_monitor_log.h"
#include "../3rdparty/json/json.h"
#include "../common/vhall_log.h"
#include "../common/live_define.h"
#include "talk/base/stringencode.h"
#include "../utility/utility.h"
#include "talk/base/base64.h"
#include "talk/base/stringdigest.h"
#include "talk/base/asynchttprequest.h"
#include "talk/base/httpcommon-inl.h"
#include "../common/auto_lock.h"
#include "../common/live_message.h"

VHallPlayMonitor::VHallPlayMonitor():
mExtendParam(NULL),
mBaseParam(NULL),
mLiveParam(NULL),
mLogMsgListener(NULL),
mIsInited(false),
mIsStoped(true),
mWorkThread(NULL){
   mDownloadAliveDataSize = 0;
   mDownloadInfoDataSize = 0;
   mBufferCount = 0;
   mBufferStartTime = 0;
   mBu = 0;
   vhall_lock_init(&mLogMutex);
   mWorkThread = new(std::nothrow) talk_base::Thread();
   mLogReportUrl = LOG_HOST;
   if (!mWorkThread->started()){
      mWorkThread->Start();
      mWorkThread->Restart();
   }else{
      LOGE("mhttpRequestThread new is error!");
   }
}

VHallPlayMonitor::~VHallPlayMonitor(){
   mWorkThread->Clear(this);
   VHALL_DEL(mExtendParam);
   VHALL_DEL(mBaseParam);
   VHALL_THREAD_DEL(mWorkThread);
   mIsInited = false;
   vhall_lock_destroy(&mLogMutex);
   LOGI("VHallPlayMonitor::~VHallPlayMonitor()");
}

int VHallPlayMonitor::Init(BaseLiveParam*param){
   VhallAutolock lock(&mLogMutex);
   mLiveParam = param;
   VHALL_DEL(mBaseParam);
   mBaseParam = new VHJson::Value();
   mLiveParam->GetJsonObject(mBaseParam);
   if (mIsInited==false&&mExtendParam) {
      ReportLog(MOB_PLAY_START);
      mIsInited = true;
   }
   return 0;
}

int VHallPlayMonitor::SetExtendParam(const char *param){
   VhallAutolock lock(&mLogMutex);
   VHJson::Reader reader;
   VHJson::Value root;
   LOGI("MonitorLogParam:%s", param);
   if (!reader.parse(param, root, false)){
      LOGE("MonitorLogParam json pares error!!!");
      return -1;
   }
   VHALL_DEL(mExtendParam);
   mExtendParam = new VHJson::Value(root);
   if (mExtendParam->isMember("s")) {
      mSessionId = (*mExtendParam)["s"].asString();
   }else{
      std::string id = Utility::ToString(Utility::GetTimestampMs());
      if (mExtendParam->isMember("ndi")) {
         id += (*mExtendParam)["ndi"].asString();
      }
      mSessionId = talk_base::MD5(id);
      (*mExtendParam)["s"] = VHJson::Value(mSessionId);
   }
   if (mExtendParam->isMember("bu")) {
      VHJson::Value &value = (*mExtendParam)["bu"];
      mBu = value.isInt()?value.asInt():atoi(value.asCString());
      mExtendParam->removeMember("bu");
   }else{
      LOGE("not has bu field!");
   }
   if (mExtendParam->isMember("host")) {
      mLogReportUrl = (*mExtendParam)["host"].asString();
      mExtendParam->removeMember("host");
   }else{
      mLogReportUrl = LOG_HOST;
   }
   if (mIsInited==false&&mLiveParam) {
      ReportLog(MOB_PLAY_START);
      mIsInited = true;
   }
   return 0;
}

void VHallPlayMonitor::SetLogMsgListener(const LogMsgListener &listener){
   VhallAutolock lock(&mLogMutex);
   mLogMsgListener = listener;
}

void VHallPlayMonitor::SetPlayUrl(const std::string &play_url){
   VhallAutolock lock(&mLogMutex);
   talk_base::Url<char> url(play_url.c_str());
   mHost = url.host();
   mUri = url.path();
   std::vector<std::string> fields;
   talk_base::split(mUri, '/', &fields);
   mStreamName = fields[fields.size()-1];
}

void VHallPlayMonitor::ReportLog(const PlayLogKeyValue key,const int errcode){
   VhallAutolock lock(&mLogMutex);
   VHJson::Value path;
   if (mBaseParam) {
      path = *mBaseParam;
   }
   if (mExtendParam) {
      VHJson::Value::Members members = mExtendParam->getMemberNames();
      for (auto iter=members.begin();iter!=members.end();iter++){
         path[*iter] = (*mExtendParam)[*iter];
      }
   }
   path["ver"] = VHJson::Value(SDK_VERSION);
   if (key != MOB_PLAY_START) {
      path["fd"] = VHJson::Value(mUri);
      path["si"] = VHJson::Value(mServerIp);
      path["p"] = VHJson::Value(mStreamName);
      path["sd"] = VHJson::Value(mHost);
   }
   if (key == MOB_PLAY_ERR||key == MOB_PLAY_INFO) {
      if (errcode!=0) {
         path["errcode"] = VHJson::Value(errcode);
      }
   }
   if(key == MOB_PLAY_INFO){
      int diffTime = (int)(Utility::GetTimestampMs()-mInfoTime.mReportTime);
      diffTime = MIN(30*1000, diffTime);
      diffTime = MAX(0, diffTime);
      path["tt"] = VHJson::Value(Utility::ToString(diffTime-mInfoTime.mAccumulateTime));
      path["bc"] = VHJson::Value(Utility::ToString(mBufferCount));
      mBufferCount = 0;
      path["bt"] = VHJson::Value(Utility::ToString(mInfoTime.mAccumulateTime));
      mInfoTime.mAccumulateTime = 0;
      mInfoTime.mReportTime = Utility::GetTimestampMs();
      long dataSize = mDownloadInfoDataSize*1024/8;
      mDownloadInfoDataSize = 0;
      path["tf"] = VHJson::Value(Utility::ToString(dataSize));
   }
   if(key == MOB_PLAY_ALIVE){
      int diffTime = (int)(Utility::GetTimestampMs()-mAliveTime.mReportTime);
      diffTime = MIN(30*1000, diffTime);
      diffTime = MAX(0, diffTime);
      path["tt"] = VHJson::Value(Utility::ToString(diffTime-mAliveTime.mAccumulateTime));
      mAliveTime.mAccumulateTime = 0;
      mAliveTime.mReportTime = Utility::GetTimestampMs();
      long dataSize = mDownloadAliveDataSize*1024/8;
      mDownloadAliveDataSize = 0;
      path["tf"] = VHJson::Value(Utility::ToString(dataSize));
   }
   VHJson::FastWriter root;
   std::string json_str = root.write(path);
   char curi[256]={0};
   std::string logId = Utility::ToString(Utility::GetTimestampMs());
   snprintf(curi, sizeof(curi),"k=%d&id=%s&s=%s&bu=%d",key,logId.c_str(),mSessionId.c_str(),mBu);
   std::string uri(curi);
   LOGI("url :%s?%s %s",mLogReportUrl.c_str(),curi,json_str.c_str());
   std::string url = mLogReportUrl + std::string("?")+uri+std::string("&token=")+talk_base::Base64::Encode(json_str);
   mWorkThread->Post(this, MSG_RTMP_HTTP_REQUEST, new HttpDataMessageData(url,key));
}

void VHallPlayMonitor::StartPlay(const std::string &url){
   if (!mWorkThread->started()){
      mWorkThread->Start();
      mWorkThread->Restart();
   }
   mAliveTime.ClearData();
   mInfoTime.ClearData();
   SetPlayUrl(url);
   mDownloadAliveDataSize = 0;
   mDownloadInfoDataSize = 0;
   mBufferCount = 0;
   mBufferStartTime = 0;
   mInfoTime.ClearData();
   mAliveTime.ClearData();
}

void VHallPlayMonitor::StopPlay(){
   if (mIsStoped==false) {
      mWorkThread->Clear(this,MSG_RTMP_HEART_BEAT);
      mWorkThread->Clear(this,MSG_RTMP_INFO_BEAT);
      ReportLog(MOB_PLAY_INFO,2002);
      ReportLog(MOB_PLAY_ALIVE);
      mIsStoped = true;
      mWorkThread->Stop();
#if defined(ANDROID)
      mIsInited = false;
#endif
   }
}

void VHallPlayMonitor::OnMessage(talk_base::Message* msg){
   switch (msg->message_id) {
      case MSG_RTMP_HTTP_REQUEST:{
         HttpDataMessageData * obs = static_cast<HttpDataMessageData*>(msg->pdata);
         HttpRequest(obs->mUrl, (PlayLogKeyValue)obs->key);
      }
         break;
      case MSG_RTMP_HEART_BEAT:{
         OnHeartBeat();
      }
         break;
      case MSG_RTMP_INFO_BEAT:{
         OnInfoBeat();
      }
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void VHallPlayMonitor::OnNotifyEvent(const int type,const std::string &content){
   switch (type) {
      case SERVER_IP:{
         mServerIp = content;
      }
         break;
      case OK_WATCH_CONNECT:{
         mInfoTime.mAccumulateTime = 0;
         mInfoTime.mReportTime = Utility::GetTimestampMs();
         mAliveTime.mAccumulateTime = 0;
         mAliveTime.mReportTime = Utility::GetTimestampMs();
         mWorkThread->Clear(this,MSG_RTMP_HEART_BEAT);
         mWorkThread->Clear(this,MSG_RTMP_INFO_BEAT);
         mWorkThread->PostDelayed(HEART_BEAT_INTERVAL_TIME, this, MSG_RTMP_HEART_BEAT);
         mWorkThread->PostDelayed(INFO_BEAT_INTERVAL_TIME, this, MSG_RTMP_INFO_BEAT);
      }
         break;
      case DEMUX_METADATA_SUCCESS:{
         mIsStoped = false;
      }
         break;
      case ERROR_WATCH_CONNECT:{
         mWorkThread->Clear(this);
         if (mIsStoped) {
            ReportLog(MOB_PLAY_ERR,4001);
         }
      }
         break;
      case ERROR_RECV:{
         mBufferCount++;
         mWorkThread->Clear(this);
         if (mIsStoped) {
            ReportLog(MOB_PLAY_INFO,4001);
         }
         ReportLog(MOB_PLAY_ALIVE);
      }
         break;
      case START_BUFFERING:{
         mBufferStartTime = Utility::GetTimestampMs();
         mBufferCount++;
      }
         break;
      case STOP_BUFFERING:{
         if (mBufferStartTime<=0) {
            break;
         }
         int diffTime = (int)(Utility::GetTimestampMs()-mBufferStartTime);
         diffTime = MIN(30*1000, diffTime);
         diffTime = MAX(0, diffTime);
         mInfoTime.mAccumulateTime += diffTime;
         mAliveTime.mAccumulateTime += diffTime;
      }
         break;
      case INFO_SPEED_DOWNLOAD:{
         mDownloadAliveDataSize += atoi(content.c_str());
         mDownloadInfoDataSize += atoi(content.c_str());
      }
         break;
      default:
         break;
   }
}

void VHallPlayMonitor::HttpRequest(const std::string &url_str,const PlayLogKeyValue key){
#if defined(IOS) || defined(ANDROID)
   if (mLogMsgListener) {
      mLogMsgListener(url_str);
   }
#else
   std::string::size_type pos=0;
   std::string httpUrl(url_str);
   if ((pos=httpUrl.find("https",0))!=std::string::npos){
      httpUrl.replace(0, 5, "http");
   }
   talk_base::Url<char> url(httpUrl.c_str());
   talk_base::AsyncHttpRequest* req = CreateGetRequest(url.host(),url.port(),url.full_path());
   req->Start();
   req->Release();
#endif
}

talk_base::AsyncHttpRequest* VHallPlayMonitor::CreateGetRequest(const std::string& host,const int port,const std::string& path) {
   talk_base::AsyncHttpRequest* request = new talk_base::AsyncHttpRequest("vhall");
   if (mLiveParam&&mLiveParam->is_http_proxy) {
      ProxyDetail *pd = &mLiveParam->proxy;
      //TODO talk with fuzhuang make it better
      talk_base::ProxyInfo prox;
      prox.autodetect = false;
      prox.address = talk_base::SocketAddress(pd->host, pd->port);
      prox.type = talk_base::PROXY_HTTPS;
      prox.username = pd->username;
      talk_base::InsecureCryptStringImpl ins_pw;
      ins_pw.password() = pd->password;
      talk_base::CryptString pw(ins_pw);
      prox.password = pw;
      request->set_proxy(prox);
   }
   request->SignalWorkDone.connect(this,&VHallPlayMonitor::OnRequestDone);
   request->request().verb = talk_base::HV_GET;
   request->response().document.reset(new talk_base::MemoryStream());
   request->set_host(host);
   request->set_port(port);
   request->request().path = path;
   return request;
}

void VHallPlayMonitor::OnRequestDone(talk_base::SignalThread* request){
   talk_base::AsyncHttpRequest* req = (talk_base::AsyncHttpRequest*)request;
   if (req->response().scode==talk_base::HC_OK) {
      req->response().document->Rewind();
      std::string response;
      req->response().document->ReadLine(&response);
      LOGI("http request response:%s",response.c_str());
   }else{
      LOGW("http request error code:%d",req->response().scode);
   }
}

void VHallPlayMonitor::UpdateUrl(const std::string &url){
   SetPlayUrl(url);
}

void VHallPlayMonitor::OnHeartBeat(){
   ReportLog(MOB_PLAY_ALIVE);
   if (mIsStoped==false) {
      mWorkThread->PostDelayed(HEART_BEAT_INTERVAL_TIME, this, MSG_RTMP_HEART_BEAT);
   }
}

void VHallPlayMonitor::OnInfoBeat(){
   ReportLog(MOB_PLAY_INFO,2002);
   if (mIsStoped==false) {
      mWorkThread->PostDelayed(INFO_BEAT_INTERVAL_TIME, this, MSG_RTMP_INFO_BEAT);
   }
}
