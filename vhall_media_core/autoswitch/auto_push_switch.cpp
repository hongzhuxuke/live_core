//
//  auto_push_switch.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/1.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "auto_push_switch.h"
#include <stdlib.h>
#include "../common/vhall_log.h"
#include "../common/auto_lock.h"
#include "../3rdparty/json/json.h"
#include "../common/live_define.h"
#include "../rtmppush/live_push_interface.h"

enum Msg{
   HTTP_MSG_PUSH_ERROR_UNAUTHORIZED = 401,
   HTTP_MSG_PUSH_ERROR_FORBIDDEN = 403,
   HTTP_MSG_PUSH_ERROR_NOT_FOUND = 404,
   HTTP_MSG_PUSH_ERROR_PROXY_AUTHENTICATION_REQUIRED = 407,
   
   RTMP_MSG_PUSH_ERROR_NET_DISCONNECT = 4003,
   RTMP_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN = 4004,
   RTMP_MSG_PUSH_ERROR_REJECTED_NOT_IN_WHITELIST = 4005,
   RTMP_MSG_PUSH_ERROR_REJECTED_IN_BLACKLIST = 4006,
   RTMP_MSG_PUSH_ERROR_REJECTED_ALREADY_EXISTS = 4007,
   RTMP_MSG_PUSH_ERROR_REJECTED_PROHIBIT = 4008,
   RTMP_MSG_PUSH_ERROR_UNSUPPORTED_RESOLUTION = 4009,
   RTMP_MSG_PUSH_ERROR_UNSUPPORTED_SAMPLERATE = 4010,
   RTMP_MSG_PUSH_ERROR_ARREARAGE = 4011
};

namespace Vhall {
   
   AutoPushSwitch::AutoPushSwitch():mLivePush(NULL){
      vhall_lock_init(&mMutex);
      mReadPosition = 0;
   }
   
   AutoPushSwitch::~AutoPushSwitch(){
      Destory();
      vhall_lock_destroy(&mMutex);
      LOGI("AutoPushSwitch::~AutoPushSwitch()");
   }
   
   int AutoPushSwitch::Init(const std::string &urls){
      if ((&urls) == NULL) {
         LOGE("urls is NULL pointer !");
         return -1;
      }
      VHJson::Reader reader;
      VHJson::Value  root;
      if (!reader.parse(urls, root, false)){
         LOGE("json parse error!");
         return -1;
      }
      if (root.type()!=VHJson::arrayValue) {
         LOGE("urls is not arrayValue json format!");
         return -2;
      }
      VhallAutolock lock(&mMutex);
      Destory();
      int count = root.size();
      for (int i = 0; i<count; i++) {
         UrlItem * item = new UrlItem();
         item->mUrl = root[i].asString();
         mUrls.insert(std::pair<int,UrlItem*>(i,item));
      }
      return 0;
   }
   void AutoPushSwitch::Destory(){
      for (auto iter = mUrls.begin(); iter != mUrls.end(); iter++ ){
         VHALL_DEL(iter->second);
      }
      mUrls.clear();
   }
   void AutoPushSwitch::SetSwitchEventListener(SwitchEventListener *eventListener){
      mEventListener = eventListener;
   }
   
   void AutoPushSwitch::SetLivePushObject(LivePushInterface *live_push){
      mLivePush = live_push;
   }
   
   int AutoPushSwitch::OnEvent(const int muxer_id,const int type, const std::string content){
      if (mLivePush==NULL) {
         LOGE("mLivePush is NULL pointer");
         return -1;
      }
      if ((type == ERROR_PUBLISH_CONNECT||type == ERROR_SEND)) {
         int value = atoi(content.c_str());
         if (value != RTMP_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN&&value !=RTMP_MSG_PUSH_ERROR_REJECTED_NOT_IN_WHITELIST&&value != RTMP_MSG_PUSH_ERROR_REJECTED_IN_BLACKLIST&&value != RTMP_MSG_PUSH_ERROR_REJECTED_ALREADY_EXISTS&&value != RTMP_MSG_PUSH_ERROR_ARREARAGE&&value != HTTP_MSG_PUSH_ERROR_UNAUTHORIZED&&value != HTTP_MSG_PUSH_ERROR_FORBIDDEN&&value !=HTTP_MSG_PUSH_ERROR_NOT_FOUND&& value != HTTP_MSG_PUSH_ERROR_PROXY_AUTHENTICATION_REQUIRED) {
            VhallAutolock lock(&mMutex);
            while (mReadPosition<mUrls.size()) {
               int muxerId = 0;
               //TODO reconnect
               mLivePush->StopMuxer(muxer_id);
               mLivePush->RemoveMuxer(muxer_id);
               std::string url;
               auto iter=mUrls.find(muxer_id);
               if(iter==mUrls.end()){
                  LOGE("we do not find muxer:%d",muxer_id);
                  break;
               }else{
                  url = iter->second->mUrl;
               }
               std::string::size_type pos = 0;
               if ((pos=url.find("http://",0))!=std::string::npos) {
                  muxerId = mLivePush->AddMuxer(HTTP_FLV_MUXER, (void*)url.c_str());
               }else if((pos=url.find("rtmp://",0))!=std::string::npos||(pos=url.find("aestp://",0))!=std::string::npos){
                  muxerId = mLivePush->AddMuxer(RTMP_MUXER, (void*)url.c_str());
               }else {
                  LOGE("url Format is error!");
                  
                  continue;
               }
               iter->second->mReconnectCount++;
               mReadPosition++;
               if (mEventListener) {
                  mEventListener->OnSwitchChange(muxer_id, muxerId, url);
               }
               mLivePush->StartMuxer(muxerId);
               
               return 0;
            }
         }
      }
      if (type == UPLOAD_NETWORK_OK) {
         //TODO
      }
      if (type == UPLOAD_NETWORK_EXCEPTION) {
         //TODO
      }
      if (mEventListener) {
         mEventListener->OnEvent(type, content);
         return 0;
      }
      return -1;
   }
}

