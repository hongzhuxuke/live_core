//
//  monitor_define.h
//  VhallLiveApi
//
//  Created by ilong on 2017/11/23.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef monitor_define_h
#define monitor_define_h

#include <functional>
#define HEART_BEAT_INTERVAL_TIME     60*1000 //单位毫秒
#define INFO_BEAT_INTERVAL_TIME      30*1000 //单位毫秒
#define LOG_HOST  "https://la.e.vhall.com/login"
typedef std::function<void(const std::string &log_str)> LogMsgListener;

#endif /* monitor_define_h */
