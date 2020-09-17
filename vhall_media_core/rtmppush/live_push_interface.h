//
//  live_push_interface.h
//  VhallLiveApi
//
//  Created by ilong on 2017/9/4.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef live_push_interface_h
#define live_push_interface_h

#include "../common/live_open_define.h"

class LivePushInterface {

public:
   LivePushInterface(){};
   ~LivePushInterface(){};
   /**
    *  add a muxer return muxerId
    *
    *  @param type      muxer 类型
    *  @param param     参数 type==RTMP_MUXER时，为rtmp url，type==FILE_FLV_MUXER时，为写文件地址
    *
    *  @return id是成功，-1失败
    */
   virtual int AddMuxer(VHMuxerType type,void * param) = 0;
   /**
    *  remove a muxer with muxerId
    *
    *  @param muxer_id     muxer id
    *
    *  @return 0是成功，-1失败
    */
   virtual int RemoveMuxer(int muxer_id) = 0;
   /**
    *  remove all muxer
    *
    *  @return 0是成功，-1失败
    */
   virtual int RemoveAllMuxer() = 0;
   /**
    *  start
    *
    *  @param muxer_id  muxer id
    *
    *  @return 0是成功，-1失败
    */
   virtual int StartMuxer(int muxer_id) = 0;
   /**
    *  stop
    *
    *  @param muxer_id  muxer id
    *
    *  @return 0是成功，-1失败
    */
   virtual int StopMuxer(int muxer_id) = 0;
};

#endif /* live_push_interface_h */
