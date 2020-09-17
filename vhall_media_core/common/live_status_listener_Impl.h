//
//  LiveStatusListenerImpl.hpp
//  VinnyLive
//
//  Created by ilong on 2016/11/30.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef LiveStatusListenerImpl_hpp
#define LiveStatusListenerImpl_hpp

#include "../common/live_status_listener.h"
#include "talk/base/messagehandler.h"
#include "../common/live_define.h"

namespace talk_base {
   class Thread;
}
class EventParam;

typedef std::function<void(const int type, const EventParam &param)> StatusListener;

class LiveStatusListenerImpl:public talk_base::MessageHandler,public LiveStatusListener {
   
public:
   LiveStatusListenerImpl(const StatusListener &status_listener);
   ~LiveStatusListenerImpl();
   virtual void NotifyEvent(const int type, const EventParam &param);
private:
   enum {
      MSG_NOTIFY_EVENT,
   };
   void OnMessage(talk_base::Message* msg);
   void OnNotifyEvent(const int type, const EventParam &param);
   StatusListener       mStatusListener;
   talk_base::Thread    *mEeventThread;
};

#endif /* LiveStatusListenerImpl_hpp */
