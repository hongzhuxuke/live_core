//
//  PublishInterface.hpp
//  VinnyLive
//
//  Created by ilong on 2016/11/7.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef PublishInterface_hpp
#define PublishInterface_hpp

#include <stdio.h>
#include <live_get_status.h>
#include "live_define.h"

class LiveStatusListener;
struct LivePushParam;
class RateControl;

class MediaMuxerInterface:public LiveGetStatus
{
public:
   MediaMuxerInterface(){};
   virtual ~MediaMuxerInterface(){}
   virtual int LiveSetParam(LivePushParam *param) = 0;
   virtual void SetStatusListener(LiveStatusListener * listener) = 0;
   virtual int  AddMuxer(VHMuxerType type,void * param) = 0;
   virtual void RemoveMuxer(int muxer_id) = 0;
   virtual void StartMuxer(int muxer_id) = 0;
   virtual void StopMuxer(int muxer_id) = 0;
   virtual void StopAllMuxer() = 0;
   virtual void RemoveAllMuxer() = 0;
   virtual int GetMuxerStatus(int muxer_id) = 0;
   virtual int GetMuxerStartCount() = 0;
   virtual int GetPushMuxerStartCount() = 0;
   virtual int GetMuxerCount() = 0;
   virtual const VHMuxerType GetMuxerType(int muxer_id) = 0;
   virtual int GetDumpSpeed(int muxer_id) = 0;
   virtual void SetRateControl(RateControl *rate_control) = 0;
};

#endif /* PublishInterface_hpp */
