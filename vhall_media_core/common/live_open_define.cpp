//
//  live_open_define.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/8/30.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "../3rdparty/json/json.h"
#include "live_open_define.h"
#include "vhall_log.h"
#include <cstring>

void SetPlayModuleLog(std::string path, int level)
{
   if (path == ""){
      ConsoleInitParam param;
      memset(&param, 0, sizeof(ConsoleInitParam));
      param.nType = 0;
      ADD_NEW_LOG(VHALL_LOG_TYPE_CONSOLE, &param, level);
   }else{
      FileInitParam param;
      memset(&param, 0, sizeof(FileInitParam));
      param.pFilePathName = path.c_str();
      ADD_NEW_LOG(VHALL_LOG_TYPE_FILE, &param, level);
   }
}

bool LivePushParam::GetJsonObject(VHJson::Value *value){
   if (value==NULL) {
      return false;
   }
   (*value)["frame_rate"] = VHJson::Value(frame_rate);
   (*value)["bit_rate"] = VHJson::Value(bit_rate);
   (*value)["gop_interval"] = VHJson::Value(gop_interval);
   (*value)["sample_rate"] = VHJson::Value(sample_rate);
   (*value)["ch_num"] = VHJson::Value(ch_num);
   (*value)["audio_bitrate"] = VHJson::Value(audio_bitrate);
   (*value)["publish_timeout"] = VHJson::Value(publish_timeout);
   (*value)["publish_reconnect_times"] = VHJson::Value(publish_reconnect_times);
   return true;
};

bool LivePlayerParam::GetJsonObject(VHJson::Value *value){
   if (value==NULL) {
      return false;
   }
   (*value)["watch_timeout"] = VHJson::Value(watch_timeout);
   (*value)["watch_reconnect_times"] = VHJson::Value(watch_reconnect_times);
   (*value)["buffer_time"] = VHJson::Value(buffer_time);
   return true;
};
