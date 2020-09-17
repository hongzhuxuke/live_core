#ifndef __VINNY_SRS_HTTP_FLV_STREAMER_H__
#define __VINNY_SRS_HTTP_FLV_STREAMER_H__
#include <string>
#include <srs_librtmp.h>
#include "../common/live_define.h"
#include "muxer_interface.h"
#include <live_sys.h>
#include "talk/base/thread.h"
#include "../ratecontrol/rate_control_interface.h"
#include "talk/base/messagehandler.h"
#include <atomic>
#include "time_jitter.h"

class SrsDataStream;
class SrsAsyncHttpRequest;
class SrsHttpFlvEncoder;

namespace talk_base {
   class SignalThread;
}

#define SRS_FLV_TAG_HEADER_SIZE 11
#define SRS_FLV_PREVIOUS_TAG_SIZE 4

class SrsHttpFlvMuxer
   : public talk_base::MessageHandler,
   public MuxerInterface,
   public SafeDataQueueStateListener,
   public BufferState
{
public:
   //from MuxerInterface
   SrsHttpFlvMuxer(MuxerListener *listener, std::string tag, std::string url, LivePushParam *param);
   ~SrsHttpFlvMuxer();

   virtual bool Start();
   virtual bool PushData(SafeData *data);
   virtual std::list<SafeData*> Stop();
   virtual void Restart();
   virtual AVHeaderGetStatues GetAVHeaderStatus();
   // get dump speed unit kb/s
   virtual int  GetDumpSpeed();
   virtual std::string GetDest();
   virtual int GetState();
   virtual const VHMuxerType GetMuxerType();
   
   //frome SafeDataQueueStateListener
   void OnSafeDataQueueChange(SafeDataQueueState state, std::string tag);

   //from LiveGetStatus
   virtual bool LiveGetRealTimeStatus(VHJson::Value & value);

   //from self use the follow func you master use SrsRtmpPublisher point not MuxerInterface
   virtual int GetKeyFrameCount();
   virtual int GetQueueDataSize();
   virtual uint32_t GetQueueDataDuration();
   virtual int GetQueueSize();
   virtual int GetMaxNum();

protected:
   virtual int ReportMuxerEvent(int type, MuxerEventParam *param);

private:
   bool Init();
   bool Reset(bool isStop = true);
   virtual void Destroy();
   bool Sending();
   virtual bool Publish(SafeData *frame);

   bool SendHeaders();
   bool SendFlvFileHeaders();

   //bool SendKeyFrame(char * data, int size, int type, uint32_t timestamp);
   bool SendMetadata(LPRTMPMetadata lpMetaData, uint32_t timestamp);
   bool SendPpsAndSpsData(LPRTMPMetadata lpMetaData, uint32_t timestamp);
   bool SendAudioInfoData();
   bool SendPacket(char type, uint32_t timestamp, char* data, int size);
   bool SendH264Packet(char *data, long size, bool bIsKeyFrame, unsigned int nTimeStamp);
   bool SendAudioPacket(char *data, int size, int nTimeStamp);
   bool SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData, int * dataSize);
   void UpdataSpeed();
   //new add from MessageHandler
   enum SELF_MSG{
      SELF_MSG_START = 0,
      SELF_MSG_SEND,
      SELF_MSG_STOP,
      SELF_MSG_CONN_SUCC,
      SELF_MSG_CONN_RC
   };

   virtual void OnMessage(talk_base::Message* msg);

public:
   void HttpFlvOpenWrite(const char* file);
   void HttpFlvClose();
   int HttpFlvWriteHeader(char header[9]);
   int HttpFlvWriteTag(char type, int32_t time, char* data, int size);

private:
   const std::string   mUrl;

   SrsHttpFlvEncoder *mEncoder;
   SrsAsyncHttpRequest *mWriter;
   char                *mFrameData;
   //   int 				     mH264HeaderSize;
   //   int 				     mIdrHeaderSize;
   //   bool 			        mHasSendMetadata;
   //   uint32_t			     mFirstTimestamp;
   //   uint32_t            mPTime;
   vhall_lock_t        mMutex;
   //new add
   SafeDataQueue       *mBufferQueue;
   talk_base::Thread   *mThread;
   LivePushParam       *mParam;
   MuxerEventParam     mMuxerEvent;

   std::atomic_llong   mStartTime;
   std::atomic_llong   mLastSpeedUpdateTime;
   std::atomic_llong   mLastSendBytes;
   std::atomic_llong   mCurentSendBytes;

   std::atomic_bool    mAsyncStoped;
   std::atomic_llong   mSendFrameCount;

   // kbps
   std::atomic_llong   mCurentSpeed;
   std::atomic_int     mState;
   bool                mHasEverStarted;
   //因为MuxerStates未扩展开始后是否成功的状态，增加该值，表示是否人为停止
   bool                mStopedByCommand;

   TimeJitter          *mTimeJitter;


   std::atomic_bool    mHasInQueueVideoHeader;
   std::atomic_bool    mHasInQueueAudioHeader;

   SafeData           *mVideoHeader;
   SafeData           *mAudioHeader;

   bool                mHasSendHeaders;
   bool                mHasSendKeyFrame;
   bool                mHasSendFileHeader;

   int                 mReConnectCount;

   RTMPMetadata        mMetaData;
   std::string         mRemoteIp;
};

class SrsHttpFlvEncoder
{
public:
   SrsHttpFlvEncoder();
   ~SrsHttpFlvEncoder();
private:
   SrsAsyncHttpRequest* writer;
private:
   SrsDataStream* tag_stream;
   char tag_header[SRS_FLV_TAG_HEADER_SIZE];
public:
   virtual int initialize(SrsAsyncHttpRequest* fr);
public:
   virtual int write_header();
   virtual int write_header(char flv_header[9]);
   virtual int write_metadata(char type, char* data, int size);
   virtual int write_audio(int64_t timestamp, char* data, int size);
   virtual int write_video(int64_t timestamp, char* data, int size);
private:
   virtual int write_metadata_to_cache(char type, char* data, int size, char* cache);
   virtual int write_audio_to_cache(int64_t timestamp, char* data, int size, char* cache);
   virtual int write_video_to_cache(int64_t timestamp, char* data, int size, char* cache);
   virtual int write_pts_to_cache(int size, char* cache);
   virtual int write_tag(char* header, int header_size, char* tag, int tag_size);
};


#endif
