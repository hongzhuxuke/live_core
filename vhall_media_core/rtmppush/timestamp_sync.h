//
//  TimestampSync.hpp
//  VinnyLive
//
//  Created by ilong on 2016/12/6.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef TimestampSync_hpp
#define TimestampSync_hpp

#include <stdlib.h>
#include "live_get_status.h"
#include <functional>
#include <stdint.h>
#include <string>

class EncodeInterface;
struct LivePushParam;
struct LiveExtendParam;
namespace VHJson {
   class Value;
}

typedef std::function<int (const std::string &msg,const uint64_t timestamp)> TSOutputDataDelegate;

class TimestampSync:public LiveGetStatus
{
   
public:
   TimestampSync(EncodeInterface *encode);
   ~TimestampSync();
   
   void StartPublish();
   void StopPublish();
   
   int LiveSetParam(LivePushParam *param);
   
   void SetOutputDataDelegate(const TSOutputDataDelegate &delegate);
   
   void LivePushVideo(const char * data,const int size ,const LiveExtendParam*extendParam=NULL);
   
   void LivePushAudio(const char * data,const int size);
   
   void LivePushVideoHW(const char * data,const int size ,const int type);
   
   void LivePushAudioHW(const char * data,const int size);
   
   /**
    *  发送Amf0消息
    *
    *  @param msg      消息体
    *  @return 0是成功，非0是失败
    */
   int LivePushAmf0Msg(std::string &msg);
   
   void ResetVideoFrameTS();

   virtual bool LiveGetRealTimeStatus(VHJson::Value & value);

private:
   class VideoFrameTS{
   public:
      VideoFrameTS(){
         mFirstTS = 0;
         mTmpTS = 0;
         mVideoCount = 0;
      }
      void ResetData(){
         mFirstTS = 0;
         mTmpTS = 0;
         mVideoCount = 0;
      }
      uint64_t mFirstTS;
      uint64_t mTmpTS;
      unsigned int mVideoCount;
   };
   EncodeInterface         *mEncoder;
   LivePushParam           *mParam;
   volatile uint64_t       mVideoTS;
   volatile uint64_t       mAudioTS;
   VideoFrameTS	         *mVideoFrameTS;
   int                     mVideoDuration;
   int                     mAudioTimePres;
   uint64_t                mAudioDataSize;
   TSOutputDataDelegate    mOutputDataDelegate;
};

#endif /* TimestampSync_hpp */
