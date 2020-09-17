//
//  LiveStatusListenerImpl.cpp
//  VinnyLive
//
//  Created by ilong on 2016/11/30.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "live_status_listener_Impl.h"
#include "../common/live_message.h"
#include "../rtmppush/vhall_live_push.h"
#include "../common/vhall_log.h"

LiveStatusListenerImpl::LiveStatusListenerImpl(const StatusListener &status_listener):mEeventThread(NULL),mStatusListener(status_listener)
{
   mEeventThread = new talk_base::Thread();
   if (mEeventThread) {
      mEeventThread->Start();
   }
}

LiveStatusListenerImpl::~LiveStatusListenerImpl()
{
   VHALL_THREAD_DEL(mEeventThread);
}

void LiveStatusListenerImpl::NotifyEvent(const int type, const EventParam &param){
   if (mEeventThread) {
      mEeventThread->Post(this, MSG_NOTIFY_EVENT, new EventMessageData(type, param));
   }
}

void LiveStatusListenerImpl::OnMessage(talk_base::Message * msg)
{
   switch(msg->message_id) {
      case MSG_NOTIFY_EVENT:
      {
         EventMessageData * obs = static_cast<EventMessageData *>(msg->pdata);
         OnNotifyEvent(obs->type_, obs->param_);
      }
       break;
      default:
       break;
   }
   VHALL_DEL(msg->pdata);
}

void LiveStatusListenerImpl::OnNotifyEvent(const int type, const EventParam &param){
   if (mStatusListener) {
      mStatusListener(type, param);
   }
}
