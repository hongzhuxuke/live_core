//
//  VHallMonitorLog.cpp
//  VinnyLive
//
//  Created by liwenlong on 16/3/22.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "vhall_monitor_log.h"
#include "../common/vhall_log.h"
#include "talk/base/base64.h"
#include "talk/base/asynchttprequest.h"
#include "talk/base/stringencode.h"
#include "talk/base/httpcommon-inl.h"
#include "../common/live_message.h"
#include "../utility/utility.h"
#include "../3rdparty/json/json.h"
#include "../utility/http_request.h"
#include "../common/live_define.h"
#include "../common/auto_lock.h"
#include <sstream>

#define CATION_REPORT_DELAYED_TIME   5000 //延时5s

class LogItem{
public:
   LogItem():mStartTime(0),mStartBufferTime(0),mIsStart(false),mAllBufferTime(0),mCatonCount(0){
      memset(mSessionId, 0, sizeof(mSessionId));
   }
   std::string         mTag;
   std::string         mRtmpDomain;
   std::string         mStreamName;
   std::string         mServerIp;
   LogKeyValue         mAlivekey;
   uint64_t            mStartTime;
   uint64_t            mStartBufferTime;
   int                 mAllBufferTime;
   int                 mCatonCount;
   bool                mIsStart;
   char                mSessionId[225];
};

class LogParam {
   
public:
   LogParam(){
      mVersion = SDK_VERSION;
      mPlatform = 0;
   };
   ~LogParam(){};
   void SetResolution(const int width,const int height){
      char resolution[32]={0};
      snprintf(resolution, sizeof(resolution),"%d*%d", width, height);
      mResolution = resolution;
   }
   LogParam& operator= (const LogParam item){
      if(this==&item) return *this;
      mPlatform = item.mPlatform;
      mVersion = item.mVersion;
      mDeviceType = item.mDeviceType;
      mResolution = item.mResolution;
      mDeviceIdentifier = item.mDeviceIdentifier;
      return *this;
   }
public:
   int mPlatform; //平台类型 0代表iOS 1代表Android
   std::string mVersion;
   std::string mDeviceType;
   std::string mResolution;
   std::string mDeviceIdentifier;
};

VHallMonitorLog::VHallMonitorLog():
mLogParam(NULL)
,mHttpRequestThread(NULL)
,mExtendParam(NULL)
,mBaseParam(NULL)
{
   mDownloadData = 0;
   mLogReportUrl = LOG_HOST;
   mLogParam = new LogParam();
   if (mLogParam==NULL) {
      LOGE("mLogParam new error!");
   }
   mHttpRequestThread = new talk_base::Thread();
   if (mHttpRequestThread){
      mHttpRequestThread->Start();
   }else{
      LOGE("mhttpRequestThread new is error!");
   }
   vhall_lock_init(&mLogMutex);
   Init();
}

VHallMonitorLog::~VHallMonitorLog(){
   Destroy();
   vhall_lock_destroy(&mLogMutex);
   LOGI("VHallMonitorLog::~VHallMonitorLog()");
}

void VHallMonitorLog::Init(){
   mHttpRequestThread->PostDelayed(HEART_BEAT_INTERVAL_TIME, this , MSG_RTMP_HEART_BEAT);
}

void VHallMonitorLog::Destroy(){
   VHALL_THREAD_DEL(mHttpRequestThread);
   RemoveAllLog();
   VHALL_DEL(mLogParam);
   VHALL_DEL(mExtendParam);
   VHALL_DEL(mBaseParam);
}

// add report log
void VHallMonitorLog::AddReportLog(const std::string& url ,const int log_id,const LogKeyValue key){
   VhallAutolock lock(&mLogMutex);
   auto iter=mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LogItem *item = new LogItem();
      // int switch string
      std::stringstream stream;
      stream<<log_id;
      item->mTag = "MSID" + stream.str();
      item->mAlivekey = key;
      ParseUrl(url,*item);
      mLogMap.insert(std::pair<int,LogItem*>(log_id,item));
   }
}

// remove report log
void VHallMonitorLog::RemoveReportLog(const int log_id){
   VhallAutolock lock(&mLogMutex);
   auto iter=mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",log_id);
   }
   else
   {
      VHALL_DEL(iter->second);
      mLogMap.erase(iter);  //delete muxerId;
   }
}

void VHallMonitorLog::RemoveAllLog(){
   //VhallAutolock lock(&mLogMutex);
   for (auto iter = mLogMap.begin( ); iter != mLogMap.end( ); iter++ ){
      VHALL_DEL(iter->second);
   }
   mLogMap.clear();
}

void VHallMonitorLog::StartLog(const int log_id){
   VhallAutolock lock(&mLogMutex);
   auto iter = mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",log_id);
   }else{
     iter->second->mStartTime = Utility::GetTimestampMs();
     iter->second->mIsStart = true;
     memset(iter->second->mSessionId, 0, sizeof(iter->second->mSessionId));
     snprintf(iter->second->mSessionId, sizeof(iter->second->mSessionId),"%s%llu",iter->second->mTag.c_str(),Utility::GetTimestampMs()-1000000);
   }
}

void VHallMonitorLog::StopLog(const int log_id){
   VhallAutolock lock(&mLogMutex);
   auto iter = mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",log_id);
   }else{
      if (iter->second->mIsStart) {
         if (iter->second->mAlivekey == MOB_PULL_ALIVE) {
            ReportLog(MOB_PULL_STOP,iter->first);
         }else{
            ReportLog(MOB_PUSH_STOP,iter->first);
         }
         iter->second->mIsStart = false;
      }
   }
}

void VHallMonitorLog::StopAllLog(){
   VhallAutolock lock(&mLogMutex);
   for (auto iter = mLogMap.begin( ); iter != mLogMap.end( ); iter++ ){
      if (iter->second->mIsStart) {
         if (iter->second->mAlivekey == MOB_PULL_ALIVE) {
            ReportLog(MOB_PULL_STOP,iter->first);
         }else{
            ReportLog(MOB_PUSH_STOP,iter->first);
         }
         iter->second->mIsStart = false;
      }
   }
}

void VHallMonitorLog::OnNotifyEvent(const int type, const EventParam &param){
   LogItem * item = NULL;
   auto iter = mLogMap.find(param.mId);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",param.mId);
      return;
   }else{
      item = static_cast<LogItem*>(iter->second);
   }
   switch (type) {
      case OK_PUBLISH_CONNECT:{
         ReportLog(MOB_PUSH_START,param.mId);
      }
         break;
      case SERVER_IP:{
		  SetServerIp(param.mDesc, param.mId);
      }
         break;
      case ERROR_PUBLISH_CONNECT:{
         ReportLog(MOB_PUSH_ERR, param.mId);
         StopLog(param.mId);
      }
         break;
      case UPLOAD_NETWORK_EXCEPTION:{
         item->mStartBufferTime = Utility::GetTimestampMs();
      }
         break;
      case UPLOAD_NETWORK_OK:{
         ReportLog(MOB_PUSH_CATON,param.mId);
      }
         break;
      case VIDEO_ENCODE_BUSY:{
      
      }
         break;
      case VIDEO_ENCODE_OK:{
         ReportLog(MOB_ENCODE_CATON, param.mId);
      }
         break;
      case UPDATE_URL:{
         UpdateUrl(param);
      }
         break;
      case OK_WATCH_CONNECT:{
         mStartBufferTime = 0;
         ReportLog(MOB_PULL_START, param.mId);
      }
         break;
      case ERROR_WATCH_CONNECT:{
         ReportLog(MOB_PULL_ERR, param.mId);
         StopLog(param.mId);
      }
         break;
      case START_BUFFERING:{
         item->mStartBufferTime = Utility::GetTimestampMs();
      }
         break;
      case STOP_BUFFERING:{
         item->mAllBufferTime += Utility::GetTimestampMs()-item->mStartBufferTime;
         item->mCatonCount++;
         if (mStartBufferTime==0) {
            mStartBufferTime = Utility::GetTimestampMs();
            break;
         }
         if (Utility::GetTimestampMs()-mStartBufferTime>=1000*60) {
            ReportLog(MOB_PULL_CATON, param.mId);
            mStartBufferTime = Utility::GetTimestampMs();
         }
      }
         break;
      case INFO_SPEED_DOWNLOAD:{
         mDownloadData+=atoi(param.mDesc.c_str());
      }
         break;
      default:
         break;
   }
}

void VHallMonitorLog::SetLiveParam(BaseLiveParam &liveParam){
   mLogParam->mPlatform = liveParam.platform;
   mLogParam->mDeviceType = liveParam.device_type;
   mLogParam->mDeviceIdentifier = liveParam.device_identifier;
   mLiveParam = &liveParam;
   VHALL_DEL(mBaseParam);
   mBaseParam = new VHJson::Value();
   liveParam.GetJsonObject(mBaseParam);
}

void VHallMonitorLog::SetLogMsgListener(const LogMsgListener &listener){
   mLogMsgListener = listener;
}

void VHallMonitorLog::ReportLog(const LogKeyValue key,const int log_id){
   VhallAutolock lock(&mLogMutex);
   LogItem * item = NULL;
   auto iter = mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",log_id);
      return;
   }else{
      item = static_cast<LogItem*>(iter->second);
   }
   if (item->mIsStart==false) {
      return;
   }
   VHJson::Value path(*mBaseParam);
   if (mExtendParam) {
      VHJson::Value::Members members = mExtendParam->getMemberNames();
      for (auto iter=members.begin();iter!=members.end();iter++){
         path[*iter] = (*mExtendParam)[*iter];
      }
   }
   path["sd"] = VHJson::Value(item->mRtmpDomain);
   path["si"] = VHJson::Value(item->mServerIp);
   bool pf = path["pf"].empty();
   if (pf) {
      path["pf"] = VHJson::Value(mLogParam->mPlatform); //通过外部传进来
   }
   path["_v"] = VHJson::Value(mLogParam->mVersion);
   path["_dt"] = VHJson::Value(mLogParam->mDeviceType);
   path["_r"] = VHJson::Value(mLogParam->mResolution);
   path["p"] = VHJson::Value(item->mStreamName);
   path["di"] = VHJson::Value(mLogParam->mDeviceIdentifier);
   if (key == MOB_PULL_START||key == MOB_PULL_CATON||key == MOB_PULL_ALIVE||key == MOB_PULL_STOP||key ==MOB_PUSH_START||key == MOB_PUSH_CATON||key == MOB_PUSH_ALIVE||key == MOB_PUSH_STOP) {
      path["tt"] = VHJson::Value((int)(Utility::GetTimestampMs()-item->mStartTime));
   }
   if (key == MOB_PUSH_CATON) {
      path["_bt"] = VHJson::Value((int)(Utility::GetTimestampMs()-item->mStartBufferTime));
   }
   if (key == MOB_PULL_CATON||key == MOB_PULL_STOP) {
      path["_bt"] = VHJson::Value((int)item->mAllBufferTime);
      path["_bc"] = VHJson::Value(item->mCatonCount);
      item->mAllBufferTime = 0;
      item->mCatonCount = 0;
   }
   if (key == MOB_PULL_ALIVE||key == MOB_PULL_STOP) {
      int dataSize = (int)mDownloadData;
      dataSize = dataSize*1024/8;
      mDownloadData = 0;
      path["tf"] = VHJson::Value(dataSize);
   }
   VHJson::FastWriter root;
   std::string json_str = root.write(path);
   char curi[256]={0};
   snprintf(curi, sizeof(curi),"k=%d&id=%s&s=%s",key,GetLogId(item->mTag),item->mSessionId);
   std::string uri(curi);
   LOGI("url :%s?%s %s",mLogReportUrl.c_str(),curi,json_str.c_str());
   std::string url = mLogReportUrl + std::string("?")+uri+std::string("&token=")+talk_base::Base64::Encode(json_str);
   mHttpRequestThread->Post(this, MSG_RTMP_HTTP_REQUEST, new HttpDataMessageData(url, key));
}

void VHallMonitorLog::SetServerIp(const std::string &ip,int log_id)
{
   VhallAutolock lock(&mLogMutex);
   auto iter = mLogMap.find(log_id);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",log_id);
   }else{
      iter->second->mServerIp = ip;
   }
}

int VHallMonitorLog::SetExtendParam(const char *param){
   VHJson::Reader reader;
   VHJson::Value root;
   LOGI("MonitorLogParam:%s", param);
   if (!reader.parse(param, root, false)){
      LOGE("MonitorLogParam json pares error!!!");
      return -1;
   }
   VHALL_DEL(mExtendParam);
   mExtendParam = new VHJson::Value(root);
   if (mExtendParam->isMember("host")) {
      mLogReportUrl = (*mExtendParam)["host"].asString();
      mExtendParam->removeMember("host");
   }else{
      mLogReportUrl = LOG_HOST;
   }
   return 0;
}

void VHallMonitorLog::UpdateUrl(const EventParam &param){
   VhallAutolock lock(&mLogMutex);
   auto iter = mLogMap.find(param.mId);
   if(iter==mLogMap.end()){
      LOGW("we do not find log:%d",param.mId);
   }else{
      ParseUrl(param.mDesc,*iter->second);
   }
}

void VHallMonitorLog::SetResolution(const int width, const int height){
   if (mLogParam) {
      mLogParam->SetResolution(width, height);
   }
}

void VHallMonitorLog::ParseUrl(const std::string &rtmp_url,LogItem &item){
   std::string result = rtmp_url;
   if (item.mAlivekey==MOB_PULL_ALIVE) {
      std::vector<std::string> fields;
      talk_base::split(rtmp_url, '?', &fields);
      result = fields[0];
   }
   std::string url(result);
   int j = 0, k = 0 , s = 0, index = 0;
   std::string tdomain;
   std::string host,path;
   std::string::size_type pos=0;
   if ((pos=url.find("rtmp://",0))!=std::string::npos||(pos=url.find("http://",0))!=std::string::npos){
      index = 7;
   }else if((pos=url.find("aestp://",0))!=std::string::npos){
      index = 8;
   }
   if (index>0) {
      for (int i = index ; i < url.length(); i++) {
         if (url.at(i) == ':') {
            k = 1;
            j = 0;
            continue;
         }
         if (url.at(i) == '/') {
            k = 2;
            j = 0;
            s = i+1;
         }
         // read domain
         if (k == 0)
            tdomain += url.at(i);
         
         j++;
      }
      item.mRtmpDomain = tdomain;
      if (j>0) {
         item.mStreamName = url.substr(s,j);
      }else{
         item.mStreamName = url;
      }
   }else{
      item.mRtmpDomain = url;
      item.mStreamName = url;
   }
}

const char * VHallMonitorLog::GetLogId(const std::string& tag)
{
   memset(mLogId, 0, sizeof(mLogId));
   snprintf(mLogId, sizeof(mLogId),"%s%llu",tag.c_str(),Utility::GetTimestampMs());
   return mLogId;
}

void VHallMonitorLog::OnMessage(talk_base::Message *msg){
   switch (msg->message_id) {
      case MSG_RTMP_HTTP_REQUEST:{
         HttpDataMessageData * obs = static_cast<HttpDataMessageData*>(msg->pdata);
         OnHttpRequest(obs->mUrl,(LogKeyValue)obs->key);
      }
         break;
      case MSG_RTMP_HEART_BEAT:{
         OnHeartBeat();
      }
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void VHallMonitorLog::OnHttpRequest(const std::string &url_str,const LogKeyValue key){
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

void VHallMonitorLog::OnHeartBeat(){
   VhallAutolock lock(&mLogMutex);
   for (auto iter = mLogMap.begin(); iter != mLogMap.end(); iter++){
     ReportLog(iter->second->mAlivekey,iter->first);
   }
   mHttpRequestThread->PostDelayed(HEART_BEAT_INTERVAL_TIME, this, MSG_RTMP_HEART_BEAT);
}

talk_base::AsyncHttpRequest* VHallMonitorLog::CreateGetRequest(const std::string& host,const int port,const std::string& path) {
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
   request->SignalWorkDone.connect(this,&VHallMonitorLog::OnRequestDone);
   request->request().verb = talk_base::HV_GET;
   request->response().document.reset(new talk_base::MemoryStream());
   request->set_host(host);
   request->set_port(port);
   request->request().path = path;

   return request;
}

void VHallMonitorLog::OnRequestDone(talk_base::SignalThread* request){
   talk_base::AsyncHttpRequest* req = (talk_base::AsyncHttpRequest*)request;
   if (req->response().scode==talk_base::HC_OK) {
      req->response().document->Rewind();
      std::string response;
      req->response().document->ReadLine(&response);
      LOGI("http request response:%s",response.c_str());
   }else{
      LOGI("http request error code:%d",req->response().scode);
   }
}
