#ifndef __VINNY_SRS_FLV_RECORDER_H__
#define __VINNY_SRS_FLV_RECORDER_H__
#include <string>
#include <srs_librtmp.h>
#include "../common/live_define.h"
#include "muxer_interface.h"
#include <live_sys.h>
#include "talk/base/thread.h"
#include "talk/base/messagehandler.h"
#include <atomic>
#include "time_jitter.h"
//class LiveParam;

class SrsFlvRecorder
	: public talk_base::MessageHandler,
	public MuxerInterface,
	public SafeDataQueueStateListener
{
public:

	//from MuxerInterface
	SrsFlvRecorder(MuxerListener *listener, std::string tag, std::string url, LivePushParam *param);
	~SrsFlvRecorder();
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

protected:
	virtual int ReportMuxerEvent(int type, MuxerEventParam *param);
private:

	bool Init();
	bool Reset();
	virtual void Destroy();
	bool Sending();
	virtual bool Publish(SafeData *frame);
	bool WriteHeaders();

	//bool SendKeyFrame(char * data, int size, int type, uint32_t timestamp);
	bool WriteMetadata(srs_flv_t pFlv, LPRTMPMetadata lpMetaData, uint64_t timestamp);
	bool WritePpsAndSpsData(srs_flv_t pFlv, LPRTMPMetadata lpMetaData, uint64_t timestamp);
	bool WriteAudioInfoData();
	bool WritePacket(srs_flv_t pFlv, char type, uint64_t timestamp, char* data, int size);
	bool WriteH264Packet(srs_flv_t pFlv, char *data, long size, bool bIsKeyFrame, uint64_t nTimeStamp);
	bool WriteAudioPacket(srs_flv_t pFlv, char *data, int size, uint64_t nTimeStamp);
	bool SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData, int * dataSize);
	void UpdataSpeed();
	void RepairMetaData();
	//new add from MessageHandler
	enum SELF_MSG{
		SELF_MSG_START = 0,
		SELF_MSG_SEND,
		SELF_MSG_STOP,
	};
   bool PathExists(const std::string& path);
	virtual void OnMessage(talk_base::Message* msg);
private:
	const std::string   mPath;
	srs_flv_t          mFlv;
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

	TimeJitter          *mTimeJitter;

	std::atomic_bool    mHasInQueueVideoHeader;
	std::atomic_bool    mHasInQueueAudioHeader;

	SafeData           *mVideoHeader;
	SafeData           *mAudioHeader;

	bool                mHasSendFileHeaders;
	bool                mHasSendHeaders;
	bool                mHasSendKeyFrame;

	volatile int64_t    mMetaDataOffset;
	RTMPMetadata        mMetaData;
};

#endif
