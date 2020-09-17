#ifndef _RTMP_PUBLISHERREAL_INCLUDE_H__
#define _RTMP_PUBLISHERREAL_INCLUDE_H__
#include <Iphlpapi.h>
#include <map>
#include <vector>
#include "MediaDefs.h"
#define RTMPREALDEBUG_
struct NetworkPacketReal {
   List<BYTE> data;
   DWORD timestamp;
   PacketType type;
   DWORD distanceFromDroppedFrame;
};
enum RTMPPublisherStatus
{
   STATUS_STOPPED,
   STATUS_STOPPING,   
   STATUS_CONNECTED,
   STATUS_CONNECTING,
   STATUS_END
};

#define ARG_KEY_Publish  "Publish"
#define ARG_KEY_PlayPath  "PlayPath"
#define ARG_KEY_Username  "Username"
#define ARG_KEY_Password  "Password"
#define ARG_KEY_Token      "token"
#define MAXQUEUEBUFFERTIME 10000
#define QUEUEDROPBFRAMETIMES 1000
#define QUEUEDROPPFRAMETIMES 3000
class AudioEncoder;
class VideoEncoder;
class IMediaCoreEvent;
class RTMPPublisherReal : public NetworkStream {
public:
   
   virtual DWORD GetTimeSlot();
   double GetPacketStrain() const;
   QWORD GetCurrentSentBytes();
   uint64_t GetUpdateTime();
   UINT GetChunkSize();
   UINT GetConnectCount();
   unsigned long GetByteSpeed();
   UINT64 GetSendVideoFrameCount();
   DWORD NumDroppedFrames() const;
   DWORD NumDroppedFramesByReconnected() const;
   DWORD NumTotalVideoFrames() const;
   void BeginPublishing();
   void ResetEncoder(void* audioEncoder, void* videoEncoder);
   
   void SetAutoSpeed(bool);
   int GetSpeedLevel();
   
   void ResetMultiConn( int multiConnNum, int multiConnBufSize);
   
   void SetDispath(bool,const Dispatch_Param &);
protected:
   void SendLoop();
public:
	RTMPPublisherReal(AudioEncoder* audioEncoder, VideoEncoder* videoEncoder, IMediaCoreEvent* mediaCoreEvent, int iMultiConnNum, int iMultiConnBufSize);
   ~RTMPPublisherReal();
   void SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type);
   bool  IsCanRestart();
   void Shutdown();
   //由于MEDIACORE线程没有结束，导致退出崩溃-----
   void WaitForShutdownComplete();
   virtual std::string GetServerIP();
   virtual std::string GetStreamID();
private:
   void AddArgument(const char* key, const char* value);   
   #if 0
   UINT FindClosestQueueIndex(DWORD timestamp);
   UINT FindClosestBufferIndex(DWORD timestamp);
   #endif
private:   
   UINT BufferNum();
   void BufferPushRtmp(BYTE *data, UINT size, DWORD timestamp, PacketType type);
   void BufferClear();
   NetworkPacketReal *BufferFront();
   NetworkPacketReal *BufferFrontPop();
   void BufferDelete(NetworkPacketReal *);
   UINT QueueNum();
   NetworkPacketReal *QueueLast();
   NetworkPacketReal *QueueFrontPop();
   
   void QueueDelete(NetworkPacketReal *);
   void QueuePushRtmp(NetworkPacketReal *);
   void QueueClear();
   void QueuePrint();
   void BufferPrint();
   void BufferQueueTidy();
   NetworkPacketReal *NetWorkPacketCreateRtmp(BYTE *data, UINT size, DWORD timestamp, PacketType type);
   NetworkPacketReal *NetWorkPacketCreate(BYTE *data, UINT size, DWORD timestamp, PacketType type);
   void NetWorkPacketDestory(NetworkPacketReal *);

   void RTMPInit();
   void RTMPDestory();
   bool RTMPSetup();
   bool RTMPConnect();
   void RTMPInitMetaData();
   bool RTMPRecvDiscardBytes();
   bool RTMPSendMetaData();
   bool RTMPSendVideoHeader();
   bool RTMPSendAudioHeader();
   bool RTMPSendPacket(NetworkPacketReal *);
   bool RTMPSendInvoke();

   enum RTMPPublisherStatus StatusGet();
   bool StatusSet(enum RTMPPublisherStatus);
   bool StatusSetInternal(enum RTMPPublisherStatus);
   void StatusLock();
   void StatusUnlock();
   bool StatusMachineNop();
   bool StatusMachineSet(enum RTMPPublisherStatus expectStatus,enum RTMPPublisherStatus targetStatus);
   bool StatusMachineCallbackDoConnect();
   bool StatusMachineCallbackDoStop();
   bool StatusMachineDoConnect();
   bool StatusMachineDoStop();

   void RTMPThreadCreate();
   void RTMPThreadTerminate();
   void RTMPThreadExit();
   static DWORD WINAPI RTMPThread(RTMPPublisherReal *publisher);
   DWORD WINAPI RTMPThreadLoop();
   
   static DWORD WINAPI RateControlThread(RTMPPublisherReal *publisher);
   DWORD WINAPI RateControlThreadLoop();

   bool DoIFrameDelay(bool bBFramesOnly);
   //----丢帧策略BUG需要修复-----
   void QueueDropFrames();
   void QueueDropFrames2();
   void DropFrame(DWORD timeStamp);

   void VideoBitUpper();
   void VideoBitLower();
   bool GetAutoSpeed();
   void VideoBitUpperCountPlus();
   void VideoBitUpperCountClear();
   bool GetIsVideoBitUpper();
   
private:
   std::list<NetworkPacketReal *> mBufferedPackets;
   std::list<NetworkPacketReal *> mQueuedPackets;

   
   
   RTMP *mRtmp;

   DWORD  mTimeSlot;
   HANDLE mDataMutex;
   HANDLE mSendSempahore;
   HANDLE mConnectSempahore;
   HANDLE mRTMPMutex;
   HANDLE mRtmpThreadHandle;
   HANDLE mRtmpThreadHandleMutex;
   HANDLE mRtmpThreadExitEvent;
   HANDLE mAutoSpeedMutex;
   HANDLE mVideoBitUpperCountMutex;

   UINT mVideoBitUpperCount;
   //int   mPacketWaitType;
   bool  mIsStreamStarted;
   DWORD mFirstTimestamp;
   bool mIsTidySuccessed;
   UINT mVideoBits;
   UINT mCurrentVideoBits;
   UINT mQueueBytes;
   int mLevel;
   volatile bool mWaitKeyFrame;
   PacketType mWaitPacketType;
   
   DWORD mTotalVideoFrames;
   DWORD mNumFramesDumped;
   DWORD mNumFramesDumpedByReconnected;
   QWORD mBytesSent;
   double mSpeed;   
   DWORD mMinFramedropTimestsamp;   
   UINT mLastBFrameDropTime;
   uint64_t mAVSendTime;
   uint64_t mVideoSendTime;
   uint64_t mSpeedChangeTime;
   UINT64 mVideoSendCount;
   UINT mVideoSendSize;
   UINT mAVSendSize;
   double mAVSpeed;
   bool mIsAutoSpeed;
   uint64_t update_time;
   UINT chunk_size;
   UINT mConnect_count;
   
   bool mExit;
private:
   PacketType mLastSendVideoType;
   IMediaCoreEvent* mMediaCoreEvent;
   AudioEncoder* mAudioEncoder;
   VideoEncoder* mVideoEncoder;
   String mPublishURL;
   String mPublishPlayPath;
   String mPublishUsername;
   String mPublishPassword;
   String mPublishToken;
   std::string mServiceIP;
   std::string mStreamID;
   std::string mServerUrl;
   struct NetworkConnectParam mNetworkConnectParam;
   bool mIsEncoderDataInitialized = false;
   std::vector<char> metaDataPacketBuffer;
   DataPacket mAudioHeaders, mVideoHeaders;
   
   LPSTR mlpAnsiURL ;
   LPSTR mlpAnsiPlaypath;
private:
   enum RTMPPublisherStatus mStatus;
   HANDLE mStatusMutex;
   typedef bool (RTMPPublisherReal::*StatusMachine)();
   StatusMachine statusMachine[STATUS_END][STATUS_END];

   int mMultiConnNum;
   int mMultiConnBufSize;
   
   bool m_bDispatch = false;
   struct Dispatch_Param m_dispatchParam;
};

NetworkStream* CreateRTMPPublisherReal(AudioEncoder* audioEncoder, VideoEncoder* videoEncoder, IMediaCoreEvent*, int iMultiConnNum, int iMultiConnBufSize);
#endif 


