//
//  auto_push_switch.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/1.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef auto_push_switch_h
#define auto_push_switch_h

#include <stdio.h>
#include <map>
#include "../common/live_sys.h"
#include "../common/live_open_define.h"

class LivePushInterface;

namespace Vhall {
   
   class UrlItem{
   public:
      UrlItem():mReconnectCount(0){};
      ~UrlItem(){};
      std::string mUrl;
      int mReconnectCount;
   };
   
   class SwitchEventListener {
   public:
      virtual void OnSwitchChange(const int old_muxer_id,const int new_muxer_id,const std::string url) = 0;
      virtual int  OnEvent(const int type, const std::string content) = 0;
   };

   class AutoPushSwitch : public LivePushListener {
      
   public:
      AutoPushSwitch();
      ~AutoPushSwitch();
      int Init(const std::string &urls);//json数组 字符串
      void SetSwitchEventListener(SwitchEventListener *eventListener);
      void SetLivePushObject(LivePushInterface *live_push);
   private:
      virtual int OnEvent(const int muxer_id, const int type, const std::string content);
      void Destory();
      std::map<int,UrlItem*> mUrls;
      SwitchEventListener *mEventListener;
      LivePushInterface *mLivePush;
      int                mReadPosition;
      vhall_lock_t  mMutex;
   };
}

#endif /* auto_push_switch_hpp */
