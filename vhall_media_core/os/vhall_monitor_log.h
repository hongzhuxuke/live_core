//
//  VHallMonitorLog.hpp
//  VinnyLive
//
//  Created by liwenlong on 16/3/22.
//  Copyright 2016年 vhall. All rights reserved.
//

#ifndef VHallMonitorLog_hpp
#define VHallMonitorLog_hpp

#include "talk/base/messagehandler.h"
#include "talk/base/sigslot.h"
#include <string>
#include "monitor_define.h"
#include "../common/live_sys.h"
#include <stdint.h>
#include <atomic>
#include <map>

typedef enum {
   LOG_KEY_VALUE     = 0,
   // Mobile
   MOB_PUSH_START    = 242001,
   MOB_PUSH_STOP     = 242002,
   MOB_PUSH_ALIVE    = 242005,
   MOB_PUSH_CATON    = 244001,
   MOB_ENCODE_CATON  = 244003,
   MOB_PUSH_ERR      = 244005,
   
   MOB_PULL_START    = 62003,
   MOB_PULL_STOP     = 62004,
   MOB_PULL_ALIVE    = 62006,
   MOB_PULL_CATON    = 64002,
   MOB_DECODE_CATON  = 64004,
   MOB_PULL_ERR      = 64006,
   
}LogKeyValue;

namespace VHJson {
   class Value;
}

namespace talk_base {
   class Thread;
   class AsyncHttpRequest;
   class SignalThread;
}

class LogItem;
class LogParam;
struct BaseLiveParam;
class EventParam;
class VHallLivePush;

class VHallMonitorLog:public talk_base::MessageHandler,public sigslot::has_slots<> {
   
public:
   VHallMonitorLog();
   ~VHallMonitorLog();
   void SetLiveParam(BaseLiveParam &live_param);
   void SetLogMsgListener(const LogMsgListener &listener);
   // add report log
   void AddReportLog(const std::string& url ,const int logId, const LogKeyValue key);
   // remove report log
   void RemoveReportLog(const int log_id);
   
   void RemoveAllLog();
   
   void StartLog(const int log_id);
   
   void StopLog(const int log_id);
   
   void StopAllLog();
   
   void ReportLog(const LogKeyValue key, const int log_id);

   void SetResolution(const int width,const int height);
   
   void SetServerIp(const std::string &ip,int log_id);
   
   int  SetExtendParam(const char *param);
   
private:
   void OnNotifyEvent(const int type, const EventParam &param);
   void OnMessage(talk_base::Message* msg);
   void OnHttpRequest(const std::string &url,const LogKeyValue key);
   void OnHeartBeat();
   void Init();
   void Destroy();
   void ParseUrl(const std::string &rtmp_url,LogItem &item);
   const char * GetLogId(const std::string& tag);
   void UpdateUrl(const EventParam &param);
   talk_base::AsyncHttpRequest* CreateGetRequest(const std::string& host,const int port,
                                                 const std::string& path);
   void OnRequestDone(talk_base::SignalThread* request);
private:
   enum {
      MSG_RTMP_HTTP_REQUEST,
      MSG_RTMP_HEART_BEAT,
   };
   friend class VHallLivePush;
   std::map<int,LogItem*> mLogMap;
   talk_base::Thread      *mHttpRequestThread;
   BaseLiveParam          *mLiveParam;
   vhall_lock_t           mLogMutex;
   char                   mLogId[225];
   LogParam               *mLogParam;
   uint64_t               mStartBufferTime;
   VHJson::Value          *mExtendParam;
   VHJson::Value          *mBaseParam;
   LogMsgListener         mLogMsgListener;
   std::atomic_llong      mDownloadData;
   std::string            mLogReportUrl;  //日志上报host
};

#endif /* VHallMonitorLog_hpp */
