#ifndef _RTMP_READER_H_
#define _RTMP_READER_H_

#include <srs_librtmp.h>
#include "talk/base/messagequeue.h"
#include <iostream>
#include "../rtmpplayer/media_notify.h"
#include "demuxer_interface.h"
#include "rtmp_flv_utility.h"

//llc change class to struct

//class SrsAvcAacCodec;
//class SrsCodecSample;
//class SrsCommonMessage;
class VhallPlayerInterface;
namespace talk_base {
   class Thread;
}

class RtmpReader : public talk_base::MessageHandler,public DemuxerInterface {
public:
   RtmpReader(VhallPlayerInterface*player);
   ~RtmpReader();
   virtual void AddMediaInNotify(IMediaNotify* mediaNotify);
   virtual void ClearMediaInNotify();
   virtual void SetParam(LivePlayerParam*param);
   virtual void Stop();
   virtual void Start(const char *url);
private:
   bool OnConnect(int timeout);
   int  OnRecv();
   void CloseRtmp();
   void DestoryRtmp();
   std::string GetServerIp();
   void OnMessage(talk_base::Message* msg);

   int  OnAudio(uint32_t timestamp, char*data, int size);
   int  OnVideo(uint32_t timestamp, char*data, int size);
   int  OnMetaData(uint32_t timestamp, char*data, int size);

   void OnComputeSpeed();
   void Init();
   void Destory();
   void GetAudioParam(AudioParam &audioParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample);
   void GetVideoParam(VideoParam &videoParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample);
private:
   enum {
      MSG_RTMP_Connect,
      MSG_RTMP_Recving,
      MSG_RTMP_ComputeSpeed,
      MSG_RTMP_Close
   };
   LivePlayerParam    *mParam;
   talk_base::Thread* mWorkerThread;
   talk_base::Thread* mComputeSpeedThread;
   std::vector<IMediaNotify*> mMediaOutputNotify;
   volatile uint32_t    mTotalSize;
   FlvTagDemuxer        *mFlvTagDemuxer;
   vhall_lock_t         mDeatoryRtmpMutex;
   VhallPlayerInterface *mPlayer;
   bool                 mGotVideoKeyFrame;
   bool                 mGotAudioPkt;
   srs_rtmp_t           mRtmp;
   std::string          mUrl;
   bool                 mStart;
   int                  mConnectTimeout;
   int                  mMaxConnectTryTime;
   int                  mConnectTryTime;
};

#endif
