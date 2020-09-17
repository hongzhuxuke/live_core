//
//  vhall_play_monitor_log.h
//  VhallLiveApi
//
//  Created by ilong on 2017/11/23.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef vhall_play_monitor_log_h
#define vhall_play_monitor_log_h

#include "talk/base/messagehandler.h"
#include "talk/base/sigslot.h"
#include <stdio.h>
#include <string>
#include "../common/live_sys.h"
#include "monitor_define.h"
#include <atomic>

typedef enum {
   MOB_PLAY_START    = 102001,
   MOB_PLAY_INFO     = 102002,
   MOB_PLAY_ALIVE    = 102003,
   MOB_PLAY_ERR      = 104001,
}PlayLogKeyValue;

namespace VHJson {
   class Value;
}

namespace talk_base {
   class Thread;
   class AsyncHttpRequest;
   class SignalThread;
}

class VHallLivePlayer;
class BaseLiveParam;

class VHallPlayMonitor:public talk_base::MessageHandler,public sigslot::has_slots<> {

public:
   
   class ReportTime {
   public:
      ReportTime(){
         mAccumulateTime = 0;
         mReportTime = 0;
      }
      void ClearData(){
         mAccumulateTime = 0;
         mReportTime = 0;
      }
      int mAccumulateTime;
      uint64_t mReportTime;
   };
   
   VHallPlayMonitor();
   ~VHallPlayMonitor();
   int Init(BaseLiveParam*param);
   int SetExtendParam(const char *param);
   void SetLogMsgListener(const LogMsgListener &listener);
   void ReportLog(const PlayLogKeyValue key,const int errcode = 0);
   void StartPlay(const std::string &url);
   void StopPlay();
private:
   enum {
      MSG_RTMP_HTTP_REQUEST,
      MSG_RTMP_HEART_BEAT,
      MSG_RTMP_INFO_BEAT
   };
   void SetPlayUrl(const std::string &url);
   void OnMessage(talk_base::Message* msg);
   void OnNotifyEvent(const int type,const std::string &content);
   void HttpRequest(const std::string &url_str,const PlayLogKeyValue key);
   talk_base::AsyncHttpRequest* CreateGetRequest(const std::string& host,const int port,
                                                 const std::string& path);
   void OnRequestDone(talk_base::SignalThread* request);
   void UpdateUrl(const std::string &url);
   void OnHeartBeat();
   void OnInfoBeat();
private:
   friend class VhallLive;
   BaseLiveParam     *mLiveParam;
   std::string       mResolution;
   std::string       mSessionId;
   std::string       mHost;
   std::string       mUri;
   std::string       mStreamName;
   std::string       mLogReportUrl;  //日志上报host
   std::string       mServerIp;
   VHJson::Value     *mExtendParam;
   VHJson::Value     *mBaseParam;
   talk_base::Thread *mWorkThread;
   std::atomic_bool  mIsInited;
   std::atomic_bool  mIsStoped;
   int               mBu;
   LogMsgListener    mLogMsgListener;
   vhall_lock_t      mLogMutex;
   long              mDownloadAliveDataSize;
   long              mDownloadInfoDataSize;
   int               mBufferCount;
   uint64_t          mBufferStartTime;
   ReportTime        mInfoTime;
   ReportTime        mAliveTime;
};

#endif /* vhall_play_monitor_log_hpp */
