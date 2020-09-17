//
//  IMediaNotify.h
//  VinnyLive
//
//  Created by liwenlong on 16/6/20.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef IMediaNotify_h
#define IMediaNotify_h

#include "../common/live_define.h"
#include "buffer_queue.h"

class VinnyLive;

class IMediaNotify {
public:
   virtual bool InitLiveModel(LiveModel liveModel) = 0;
   virtual bool InitAudio(AudioParam* audioParam) = 0;
   virtual bool InitVideo(VideoParam* videoParam) = 0;
   virtual void Destory() = 0;
   virtual DataUnit* MallocDataUnit(const STREAM_TYPE& streamType, const long& bufferSize, const int& dropCnt) = 0;
   virtual bool AppendStreamPacket(const STREAM_TYPE& streamType,DataUnit* dataUnit) = 0;
};

#endif /* IMediaNotify_h */
