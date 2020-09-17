#ifndef __VINNY_SRS_RTMP_PUBLISHER_H__
#define __VINNY_SRS_RTMP_PUBLISHER_H__
#include <string>
#include <srs_librtmp.h>
#include "../common/live_define.h"
#include "muxer_interface.h"
#include <live_sys.h>
#include "../ratecontrol/rate_control_interface.h"
#include "talk/base/thread.h"
#include "talk/base/messagehandler.h"
#include <atomic>
#include "time_jitter.h"

class SrsRtmpPublisher
	: public talk_base::MessageHandler,
	  public MuxerInterface, 
	  public SafeDataQueueStateListener,
     public BufferState
{
public:

	//from MuxerInterface
	SrsRtmpPublisher(MuxerListener *listener, std::string tag, std::string url, LivePushParam *param);
   ~SrsRtmpPublisher();
   virtual bool Start();
   virtual bool PushData(SafeData *data);
   virtual std::list<SafeData*> Stop();
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

   virtual int GetQueueSize();
   virtual int GetMaxNum();
   virtual uint32_t GetQueueDataDuration();
   virtual int GetQueueDataSize();
   virtual int GetKeyFrameCount();

protected:
	virtual int ReportMuxerEvent(int type, MuxerEventParam *param);
private:

   int Init();
   bool Reset(bool isStop = true);
   virtual void Destroy();
   int Sending();
   virtual bool Publish(SafeData *frame);

   bool SendHeaders();
   //bool SendKeyFrame(char * data, int size, int type, uint32_t timestamp);
   bool SendMetadata(srs_rtmp_t pRtmp, LPRTMPMetadata lpMetaData, uint64_t timestamp);
   bool SendPpsAndSpsData(srs_rtmp_t pRtmp, LPRTMPMetadata lpMetaData, uint64_t timestamp);
   bool SendAudioInfoData();
   bool SendPacket(srs_rtmp_t rtmp,char type, uint64_t timestamp, char* data, int size);
   bool SendH264Packet(srs_rtmp_t pRtmp, char *data, long size,bool bIsKeyFrame,uint64_t nTimeStamp);
   bool SendAudioPacket(srs_rtmp_t pRtmp, char *data, int size, uint64_t nTimeStamp);
   bool SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData,int * dataSize);
   void UpdataSpeed();
   //new add from MessageHandler
   enum SELF_MSG{
	   SELF_MSG_START  = 0,
	   SELF_MSG_SEND,
	   SELF_MSG_STOP,
	   SELF_MSG_RC,
   };
   virtual void OnMessage(talk_base::Message* msg);
private:
   std::string         mUrl;
   bool                mIsMultiTcp;
   srs_rtmp_t          mRtmp;
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

   TimeJitter          *mTimeJitter;

   std::atomic_bool    mHasInQueueVideoHeader;
   std::atomic_bool    mHasInQueueAudioHeader;

   SafeData            *mVideoHeader;
   SafeData            *mAudioHeader;

   bool                mHasSendHeaders;
   bool                mHasSendKeyFrame;

   int                 mReConnectCount;

   RTMPMetadata        mMetaData;
   std::string         mRemoteIp;
};

#endif
