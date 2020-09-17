#ifndef _HTTP_FLV_READER_H_
#define _HTTP_FLV_READER_H_

#include <srs_librtmp.h>
#include "talk/base/messagequeue.h"
#include <iostream>
#include "../rtmpplayer/media_notify.h"
#include "demuxer_interface.h"
#include "rtmp_flv_utility.h"
#include "talk/base/httpclient.h"
#include "talk/base/sslsocketfactory.h"
#include <atomic>
//llc change class to struct

class VhallPlayerInterface;
namespace talk_base {
   class Thread;
}
#define MAX_FLV_TAG_SIZE  1000*1000*5

class HttpFlvDemuxer 
	: public talk_base::MessageHandler,
	public DemuxerInterface,
	public sigslot::has_slots<>{
public:
   HttpFlvDemuxer(VhallPlayerInterface*player);
   ~HttpFlvDemuxer();
   virtual void AddMediaInNotify(IMediaNotify* mediaNotify);
   virtual void ClearMediaInNotify();
   virtual void SetParam(LivePlayerParam*param);
   virtual void Stop();
   virtual void Start(const char *url);
private:
   bool OnConnect(int timeout);
   int  RecvOneTag();
   int  ReadFull(void* buffer, size_t buffer_len, size_t* read, int* error);
   //void CloseRtmp();
   void DestoryClient();
   std::string GetServerIp();
   void OnMessage(talk_base::Message* msg);

  
   int  OnAudio(uint32_t timestamp, char*data, int size);
   int  OnVideo(uint32_t timestamp, char*data, int size);
   int  OnMetaData(uint32_t timestamp, char*data, int size);

   //int  OnAudio(SrsCommonMessage* audio, SrsAvcAacCodec* avcAacCodec);
   //int  OnVideo(SrsCommonMessage* video, SrsAvcAacCodec* avcAacCodec);
   //int  OnMetaData(SrsCommonMessage* amf0, SrsAvcAacCodec* avcAacCodec);
   void OnComputeSpeed();
   void Init();
   void Destory();
   void GetAudioParam(AudioParam &audioParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample);
   void GetVideoParam(VideoParam &videoParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample);

   void OnHeaderAvailable(talk_base::HttpClient* client, bool isfinal, size_t doc_length);
   void OnHttpClientComplete(talk_base::HttpClient* client, talk_base::HttpErrorType type);
   void OnStreamEvent(talk_base::StreamInterface* stream, int type, int error);
   void Retry();
private:
   enum {
	  MSG_TIMEOUT,
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
   //SrsAvcAacCodec      *mAvcAacCodec;
   FlvTagDemuxer      *mFlvTagDemuxer;
   vhall_lock_t        mDeatoryRtmpMutex;
   VhallPlayerInterface  *mPlayer;
   std::atomic_bool   mGotHttpHeader;
   std::atomic_bool   mGotFlvFileHeader;
   bool               mGotVideoKeyFrame;
   bool               mGotAudioPkt;
   //srs_rtmp_t         mRtmp;
   talk_base::HttpClient   *mClient;
   talk_base::StreamInterface *mDataStream;
   std::vector<char>   mTagBuffer;

   //talk_base::SslSocketFactory *factory_;
   //talk_base::ReuseSocketPool *pool_;

   std::string        mUrl;
   bool               mStart;
   int                mConnectTimeout;
   int                mMaxConnectTryTime;
   int                mConnectTryTime;
   //char            mCurentFlvTag[MAX_FLV_TAG_SIZE];
};

#endif
