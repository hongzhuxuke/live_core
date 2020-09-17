#include "Utility.h"
#include "MediaDefs.h"
#include "IEncoder.h"
#include "RTMPStuff.h"
#include "RTMPPublisherReal.h"
#include "IEncoder.h"
#include "Logging.h"

#define SPEEDTIMECHANGEINTERVAL 1
#define VIDEOBITUPPERSLEEP      3
//#define RTMPREALDEBUG
//HANDLE debugMutex = NULL;
static int VHallDebug(const char * format, ...) {
#define LOGBUGLEN 1024
   char logBuf[LOGBUGLEN];
   va_list arg_ptr;
   va_start(arg_ptr, format);
   //int nWrittenBytes = sprintf_s(logBuf, format, arg_ptr);
   int nWrittenBytes = vsnprintf_s(logBuf, LOGBUGLEN, format, arg_ptr);
   va_end(arg_ptr);
   WCHAR   wstr[LOGBUGLEN * 2] = { 0 };
   MultiByteToWideChar(CP_ACP, 0, logBuf, -1, wstr, sizeof(wstr));
   OutputDebugString(wstr);
   return nWrittenBytes;
}


static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static inline uint64_t get_clockfreq(void) {
	if (!have_clockfreq)
		QueryPerformanceFrequency(&clock_freq);
	return clock_freq.QuadPart;
}

static uint64_t os_gettime_ns(void) {
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}

#define RTMPSOCKETDEBUG //if(mRtmp){VHallDebug("SOCKET:%lu ",mRtmp->m_sb.sb_socket);}else{VHallDebug("SOCKET:null ");}
#define DEBUGRTMP //OSEnterMutex(debugMutex);RTMPSOCKETDEBUG;VHallDebug(__FUNCTION__);VHallDebug(" ");VHallDebug("[%d]",__LINE__);VHallDebug("\n");OSLeaveMutex(debugMutex);

#define NETWORKPACKETDEBUG(X) //EnterMutex(debugMutex);VHallDebug(__FUNCTION__);VHallDebug(" ");VHallDebug("[%d]",__LINE__);VHallDebug("%p\n",X);OSLeaveMutex(debugMutex);
#define NETWORKPACKETDEBUG2(X) //OSEnterMutex(debugMutex);VHallDebug(__FUNCTION__);VHallDebug(" ");VHallDebug("[%d]",__LINE__);VHallDebug("THREAD[%ld]",GetCurrentThreadId());VHallDebug("type:%d ",X->type);VHallDebug("time:%ld\n",X->timestamp);OSLeaveMutex(debugMutex);
#define LOCKDEBUG(...) //OSEnterMutex(debugMutex);VHallDebug(__VA_ARGS__);OSLeaveMutex(debugMutex);
extern Logger *gLogger;
#define logWarn gLogger->logWarning
#define logInfo gLogger->logInfo

void VHallLibrtmpErrorCallback(int level, const char *format, va_list vl) {
   char ansiStr[2048];
   TCHAR logStr[2048];

   if (level > RTMP_LOGINFO)
      return;

   vsnprintf(ansiStr, sizeof(ansiStr)-1, format, vl);
   ansiStr[sizeof(ansiStr)-1] = 0;
   
   MultiByteToWideChar(CP_UTF8, 0, ansiStr, -1, logStr, _countof(logStr) - 1);
   logWarn(TEXT("RTMP Error: %s"), logStr);
   
}

NetworkStream* CreateRTMPPublisherReal(AudioEncoder* audioEncoder, VideoEncoder* videoEncoder, IMediaCoreEvent* mediaCoreEvent,int iMultiConnNum, int iMultiConnBufSize) {
	return new RTMPPublisherReal(audioEncoder, videoEncoder, mediaCoreEvent, iMultiConnNum, iMultiConnBufSize);
}
void RTMPPublisherReal::ResetEncoder(void* audioEncoder, void* videoEncoder) {
   this->mAudioEncoder = (AudioEncoder *)audioEncoder;
   this->mVideoEncoder = (VideoEncoder *)videoEncoder;
   mIsEncoderDataInitialized = false;
   BufferClear();
   QueueClear();
   mTotalVideoFrames = 0;
   mNumFramesDumped = 0;
   DWORD mNumFramesDumpedByReconnected = 0;
   mSpeed = 0.0f;
   mWaitPacketType = PacketType_VideoDisposable;
   mAVSendSize=0;
   mAVSpeed=0;
   logInfo(TEXT("RTMPPublisherReal::ResetEncoder %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
}

void RTMPPublisherReal::BeginPublishing() {
}
DWORD RTMPPublisherReal::GetTimeSlot()
{
   DWORD timeSlot=0;
   OSEnterMutex(mDataMutex);
   timeSlot=mTimeSlot;
   OSLeaveMutex(mDataMutex);   
   return timeSlot;
}
//速度
double RTMPPublisherReal::GetPacketStrain() const {
   return 0;
}
//当前发送字节数
QWORD RTMPPublisherReal::GetCurrentSentBytes() {
   return mBytesSent;
}
unsigned long RTMPPublisherReal::GetByteSpeed()
{
   unsigned long speed=mAVSpeed;
   return speed;
}
UINT64 RTMPPublisherReal::GetSendVideoFrameCount(){
   return mVideoSendCount;
}
uint64_t RTMPPublisherReal::GetUpdateTime()
{
	uint64_t time = update_time;
	return time;
}

UINT RTMPPublisherReal::GetChunkSize()
{
	UINT size = chunk_size;
	return size;
}

UINT RTMPPublisherReal::GetConnectCount()
{
	UINT count = mConnect_count;
	return count;
}

//当前丢帧数
DWORD RTMPPublisherReal::NumDroppedFrames() const {
   return mNumFramesDumped;
}
DWORD RTMPPublisherReal::NumDroppedFramesByReconnected() const {
	return mNumFramesDumpedByReconnected;
}

//总视频帧数
DWORD RTMPPublisherReal::NumTotalVideoFrames() const {
   return mTotalVideoFrames;
}
void RTMPPublisherReal::AddArgument(const char* key, const char* value) {
   String strVal;
   strVal = value;
   if (lstrcmpiA(key, ARG_KEY_Publish) == 0) {
      mPublishURL = strVal;
      mServerUrl=value;
   }
   if (lstrcmpiA(key, ARG_KEY_PlayPath) == 0) {
      mPublishPlayPath = strVal;
      mStreamID=value;
   }
   if (lstrcmpiA(key, ARG_KEY_Username) == 0) {
      mPublishUsername = strVal;
   }
   if (lstrcmpiA(key, ARG_KEY_Password) == 0) {
      mPublishPassword = strVal;
   }
   if (lstrcmpiA(key, ARG_KEY_Token) == 0) {
      mPublishToken = strVal;
   }

}

#define MAX_BUFFERED_PACKETS 10
RTMPPublisherReal::RTMPPublisherReal(AudioEncoder* audioEncoder, VideoEncoder* videoEncoder, IMediaCoreEvent* mediaCoreEvent, int iMultiConnNum, int iMultiConnBufSize) {
   static bool setLogBack=false;
   mConnect_count = 0;
   if(!setLogBack)
   {
      setLogBack=true;
      RTMP_LogSetCallback(VHallLibrtmpErrorCallback);
   }
   mMediaCoreEvent = mediaCoreEvent;
   mAudioEncoder = audioEncoder;
   mVideoEncoder = videoEncoder;
   mRtmp = NULL;
   mStatus = STATUS_STOPPED;
   mRtmpThreadHandle = NULL;
   mStatusMutex = OSCreateMutex();
   mRtmpThreadHandleMutex = OSCreateMutex();
   mLevel = 0;
   mTimeSlot=0;
   mLastSendVideoType=PacketType_VideoDisposable;
   mExit=false;

   mSendSempahore = CreateSemaphore(NULL, 0, 0x7FFFFFFFL, NULL);
   if (!mSendSempahore)
      CrashError(TEXT("RTMPPublisher: Could not create semaphore"));
   mConnectSempahore = CreateSemaphore(NULL, 0, 1, NULL);
   if (!mConnectSempahore)
      CrashError(TEXT("RTMPPublisher: Could not create semaphore connect"));

   mDataMutex = OSCreateMutex();
   if (!mDataMutex)
      CrashError(TEXT("RTMPPublisher: Could not create mutex"));

   mRTMPMutex = OSCreateMutex();
   mAutoSpeedMutex = OSCreateMutex();
   mVideoBitUpperCountMutex = OSCreateMutex();

   mVideoBitUpperCount = 0;

   for (int i = 0; i < STATUS_END; i++) {
      for (int j = 0; j < STATUS_END; j++) {
         statusMachine[i][j] = &RTMPPublisherReal::StatusMachineNop;
      }
   }

   statusMachine[STATUS_STOPPED][STATUS_CONNECTING] = &RTMPPublisherReal::StatusMachineCallbackDoConnect;
   statusMachine[STATUS_STOPPING][STATUS_CONNECTING] = &RTMPPublisherReal::StatusMachineCallbackDoConnect;
   statusMachine[STATUS_STOPPING][STATUS_STOPPED] = &RTMPPublisherReal::StatusMachineCallbackDoStop;
   statusMachine[STATUS_CONNECTED][STATUS_STOPPING] = &RTMPPublisherReal::StatusMachineCallbackDoStop;
   statusMachine[STATUS_CONNECTING][STATUS_STOPPING] = &RTMPPublisherReal::StatusMachineCallbackDoStop;
   statusMachine[STATUS_CONNECTING][STATUS_CONNECTED] = &RTMPPublisherReal::StatusMachineCallbackDoConnect;
   mIsTidySuccessed = false;
   mWaitKeyFrame = false;

   mIsAutoSpeed = false;
   mlpAnsiURL =NULL;
   mlpAnsiPlaypath=NULL;

   mMultiConnNum = iMultiConnNum;
   mMultiConnBufSize = iMultiConnBufSize;
   update_time = os_gettime_ns() / 1000000;
}

RTMPPublisherReal::~RTMPPublisherReal() {
   
   mExit=true;
   StatusSet(STATUS_STOPPING);
   WaitForShutdownComplete();
   QueueClear();
   BufferClear();

   if (mDataMutex) {
      OSCloseMutex(mDataMutex);
      mDataMutex = NULL;
   }
   if (mSendSempahore) {
      CloseHandle(mSendSempahore);
      mSendSempahore = NULL;
   }
   if (mConnectSempahore) {
      CloseHandle(mConnectSempahore);
      mConnectSempahore = NULL;
   }
   if (mRTMPMutex) {
      OSCloseMutex(mRTMPMutex);
      mRTMPMutex = NULL;
   }
   if (mStatusMutex) {
      OSCloseMutex(mStatusMutex);
      mStatusMutex = NULL;
   }
   if (mRtmpThreadHandleMutex) {
      OSCloseMutex(mRtmpThreadHandleMutex);
      mRtmpThreadHandleMutex = NULL;
   }
   if (mAutoSpeedMutex) {
      OSCloseMutex(mAutoSpeedMutex);
      mAutoSpeedMutex = NULL;
   }
   if (mVideoBitUpperCountMutex) {
      OSCloseMutex(mVideoBitUpperCountMutex);
      mVideoBitUpperCountMutex = NULL;
   }

   if(mlpAnsiURL)
   {
      Free(mlpAnsiURL);
      mlpAnsiURL=NULL;
   }

   if(mlpAnsiPlaypath)
   {
      Free(mlpAnsiPlaypath);
      mlpAnsiPlaypath=NULL;
   }
}
#if 0
UINT RTMPPublisherReal::FindClosestQueueIndex(DWORD timestamp) {
  
   UINT index;
   for (index = 0; index<mQueuedPackets.size(); index++) {
      if (mQueuedPackets[index]->timestamp > timestamp)
         break;
   }
   return index;
}

UINT RTMPPublisherReal::FindClosestBufferIndex(DWORD timestamp) {
   UINT index;
   for (index = 0; index<mBufferedPackets.size(); index++) {
      if (mBufferedPackets[index]->timestamp > timestamp)
         break;
   }
   return index;
}
#endif
void RTMPPublisherReal::ResetMultiConn( int multiConnNum, int multiConnBufSize) {
   mMultiConnNum = multiConnNum;
   mMultiConnBufSize = multiConnBufSize;
}

void RTMPPublisherReal::BufferPushRtmp
(BYTE *data, UINT size, DWORD timestamp, PacketType type) {
   #if 0
   if(type<mPacketWaitType)
   {
      Log(TEXT("[skip][%d][%lu]"),type,timestamp);
      return ;
   }
   else if(mPacketWaitType!=PacketType_Audio)
   {
      mPacketWaitType=PacketType_VideoDisposable;
   }
   #endif

   OSEnterMutex(mDataMutex);
   if (mWaitKeyFrame == true) {
      
      if (type != PacketType_VideoHighest) {
         
         LOCKDEBUG("RTMPPublisherReal::BufferPushRtmp return %d\n ",type);   
         OSLeaveMutex(mDataMutex);
		 if (type != PacketType_Audio) {
			 mNumFramesDumpedByReconnected++;
		 }
         return;
      }
      
      LOCKDEBUG("Set mWaitKeyFrame false %ld\n",__LINE__);
      mWaitKeyFrame = false;
      mFirstTimestamp = timestamp;
   }
   else
   {
     // LOCKDEBUG("BufferPushRtmp mWaitKeyFrame == false %p\n",this);

   }

   //Log(TEXT("BufferPush mFirstTimestamp[%lu]"),mFirstTimestamp);
   
   NetworkPacketReal *packet = NetWorkPacketCreateRtmp(data, size, timestamp, type);
   NETWORKPACKETDEBUG(packet);
   //NETWORKPACKETDEBUG2(packet);
   if (packet) {
      #if 0
      //查找时间戳
      std::map<DWORD, NetworkPacketReal *>::iterator itera
         = mBufferedPackets.find(timestamp);

      //if (itera != mBufferedPackets.end()) {
         //NetWorkPacketDestory(itera->second);
      //}
      #endif
      if (type != PacketType_Audio) {
         mTotalVideoFrames++;
      }
      DWORD droppedFrameVal = 10000;
      #if 1
      if (mQueuedPackets.size()) {
         std::list<NetworkPacketReal *>::iterator itera = mQueuedPackets.end();
         itera--;
         droppedFrameVal = (*itera)->distanceFromDroppedFrame+1;
      }
      #endif
      packet->distanceFromDroppedFrame = droppedFrameVal;
      //mBufferedPackets[timestamp] = packet;
      mBufferedPackets.push_back(packet);
   }
   
   OSLeaveMutex(mDataMutex);
}
void RTMPPublisherReal::QueuePushRtmp(NetworkPacketReal *networkPack) {
   OSEnterMutex(mDataMutex);
   if (networkPack->type < mWaitPacketType) {
#ifdef RTMPREALDEBUG
      //Log(TEXT("[skip][%d][%lu][%d]"),networkPack->type,networkPack->timestamp,mWaitPacketType);
#endif

      mNumFramesDumped++;
      NetWorkPacketDestory(networkPack);
      OSLeaveMutex(mDataMutex);
      return;
   } else if (networkPack->type != PacketType_Audio) {
#ifdef RTMPREALDEBUG
      //Log(TEXT("[QuIn][%d][%lu][%d]"),networkPack->type,networkPack->timestamp,mWaitPacketType);
#endif
   }

   if (networkPack->type == PacketType_VideoHighest)

   {
      mWaitPacketType = PacketType_VideoDisposable;
#ifdef RTMPREALDEBUG
      //Log(TEXT("[%s][%d]Set mWaitPacketType %d"),__FUNCTION__,__LINE__,mWaitPacketType);
#endif
   }
#ifdef RTMPREALDEBUG

   Log(TEXT("QueuePushRtmp %lu %lu"),networkPack->timestamp,mFirstTimestamp);
   
#endif
   networkPack->timestamp -= mFirstTimestamp;
   bool bInsert = false;
   for (auto itor = mQueuedPackets.begin(); itor != mQueuedPackets.end(); itor++) {
      if (networkPack->timestamp<(*itor)->timestamp) {
         mQueuedPackets.insert(itor, networkPack);
         bInsert = true;
         break;
      }
   }

   if (!bInsert) {
         mQueuedPackets.push_back(networkPack);
   }

   //mQueuedPackets[networkPack->timestamp] = networkPack;

   NETWORKPACKETDEBUG(networkPack);
   mQueueBytes += networkPack->data.Num();

   if (mQueuedPackets.size()>1) {
      mTimeSlot = networkPack->timestamp - mQueuedPackets.front()->timestamp;
   }


   OSLeaveMutex(mDataMutex);
}




void RTMPPublisherReal::QueueClear() {
   OSEnterMutex(mDataMutex);
   for (std::list<NetworkPacketReal *>::iterator itera 
         = mQueuedPackets.begin();
        itera != mQueuedPackets.end();
        itera++) {

	   if ((*itera)->type != PacketType_Audio){
		   mNumFramesDumpedByReconnected++;
	   }
      NetWorkPacketDestory(*itera);
      
   }
        
   mQueuedPackets.clear();
   mTimeSlot=0;
   OSLeaveMutex(mDataMutex);
}
void RTMPPublisherReal::QueuePrint() {
   OSEnterMutex(mDataMutex);
   for (std::list<NetworkPacketReal *>::iterator itera 
         = mQueuedPackets.begin();
        itera != mQueuedPackets.end();
        itera++) {
      NetworkPacketReal *packet = *itera;
      VHallDebug("[%lu]", packet->timestamp);
      VHallDebug("[%d]\n", packet->type);
   }
   OSLeaveMutex(mDataMutex);
}
void RTMPPublisherReal::BufferPrint() {
   OSEnterMutex(mDataMutex);
   for (std::list<NetworkPacketReal *>::iterator itera
         = mBufferedPackets.begin();
        itera != mBufferedPackets.end();
        itera++) {
      NetworkPacketReal *packet = *itera;
      VHallDebug("[%lu]", packet->timestamp);
      VHallDebug("[%d]\n", packet->type);
   }
   OSLeaveMutex(mDataMutex);
}

UINT RTMPPublisherReal::BufferNum() {
   UINT num = 0;
   OSEnterMutex(mDataMutex);
   num = mBufferedPackets.size();
   OSLeaveMutex(mDataMutex);
   return num;
}
UINT RTMPPublisherReal::QueueNum() {
   UINT num = 0;
   OSEnterMutex(mDataMutex);
   num = mQueuedPackets.size();
   OSLeaveMutex(mDataMutex);
   return num;
}
void RTMPPublisherReal::BufferQueueTidy() {
   bool isFindKeyFrame = false;
   OSEnterMutex(mDataMutex);
   QueueClear();
   BufferClear();
   LOCKDEBUG("BufferQueueTidy set mWaitKeyFrame true %p\n",this);
      
   LOCKDEBUG("Set mWaitKeyFrame true %ld\n",__LINE__);
   mWaitKeyFrame = true;
   if (mVideoEncoder) {
      mVideoEncoder->SetNeedRequestKeyframe(true);
   }
   OSLeaveMutex(mDataMutex);
   mVideoSendCount = 0;
   mVideoSendSize = 0;
   mVideoSendTime = 0;
   mIsTidySuccessed = isFindKeyFrame;
}

void RTMPPublisherReal::BufferClear() {
   OSEnterMutex(mDataMutex);
   int num=0;
   
   for (std::list<NetworkPacketReal *>::iterator itera
         = mBufferedPackets.begin();
        itera != mBufferedPackets.end();itera++) {
      NetworkPacketReal *packet = *itera;
	  if ((*itera)->type != PacketType_Audio){
		  mNumFramesDumpedByReconnected++;
	  }
      NetWorkPacketDestory(packet);
   }
   mBufferedPackets.clear();
   OSLeaveMutex(mDataMutex);
}
NetworkPacketReal *RTMPPublisherReal::BufferFront() {
   NetworkPacketReal *networkPacket = NULL;

   OSEnterMutex(mDataMutex);
   if (mBufferedPackets.size() > 0) {
      networkPacket = mBufferedPackets.front();
   }
   OSLeaveMutex(mDataMutex);
   return networkPacket;
}
NetworkPacketReal *RTMPPublisherReal::BufferFrontPop() {
   NetworkPacketReal *networkPacket = NULL;
   OSEnterMutex(mDataMutex);
   if (mBufferedPackets.size() > 0) {
      #if 0
      std::vector<NetworkPacketReal *>::iterator itera 
         = mBufferedPackets.begin();
      networkPacket = *itera;
      mBufferedPackets.erase(itera);
      #else
      networkPacket=mBufferedPackets.front();
      mBufferedPackets.erase(mBufferedPackets.begin());
      #endif
   }

   OSLeaveMutex(mDataMutex);
   NETWORKPACKETDEBUG(networkPacket);

   return networkPacket;
}
NetworkPacketReal *RTMPPublisherReal::QueueFrontPop() {
   NetworkPacketReal *networkPacket = NULL;
   OSEnterMutex(mDataMutex);
   if (mQueuedPackets.size() > 0) {
      std::list<NetworkPacketReal *>::iterator itera = mQueuedPackets.begin();
      networkPacket = *itera;
      mQueuedPackets.erase(mQueuedPackets.begin());
      mQueueBytes -= networkPacket->data.Num();
      if(networkPacket)
      {
         mLastSendVideoType=networkPacket->type;
      }
   }
   OSLeaveMutex(mDataMutex);
   NETWORKPACKETDEBUG(networkPacket);
   return networkPacket;


}
NetworkPacketReal *RTMPPublisherReal::QueueLast() {
   OSEnterMutex(mDataMutex);
   NetworkPacketReal *networkPacket = NULL;
   if (mQueuedPackets.size() > 0) {
      std::list<NetworkPacketReal *>::iterator itera = mQueuedPackets.end();
      itera--;
      networkPacket = *itera;
   }
   OSLeaveMutex(mDataMutex);
   return networkPacket;
}
void RTMPPublisherReal::BufferDelete(NetworkPacketReal *networkPacket) {
   OSEnterMutex(mDataMutex);
   if (networkPacket != NULL) {
      NETWORKPACKETDEBUG(networkPacket);
      NetWorkPacketDestory(networkPacket);

      std::list<NetworkPacketReal *>::iterator itera 
         = mBufferedPackets.begin();
      #if 0
      if (*itera == networkPacket) {
         mBufferedPackets.erase(networkPacket->timestamp);
      }
      else {
      #endif   
         for (itera = mBufferedPackets.begin(); itera != mBufferedPackets.end(); itera++) {
            if (*itera == networkPacket) {
               mBufferedPackets.erase(itera);
               break;
            }
         }
      #if 0
      }
      #endif
   }
   OSLeaveMutex(mDataMutex);

}
void RTMPPublisherReal::QueueDelete(NetworkPacketReal *networkPacket) {
   OSEnterMutex(mDataMutex);
   if (networkPacket != NULL) {
      NETWORKPACKETDEBUG(networkPacket);
      NetWorkPacketDestory(networkPacket);
      std::list<NetworkPacketReal *>::iterator itera = mQueuedPackets.begin();
      #if 0
      if (itera->second == networkPacket) {
         mQueuedPackets.erase(itera);
      } else {
      #endif
         for (itera = mQueuedPackets.begin(); 
         itera != mQueuedPackets.end(); itera++) {
            if (*itera == networkPacket) {
               mQueuedPackets.erase(itera);
               break;
            }
         }
      #if 0
      }
      #endif
   }
   OSLeaveMutex(mDataMutex);

}
NetworkPacketReal *RTMPPublisherReal::NetWorkPacketCreateRtmp(BYTE *data, UINT size, DWORD timestamp, PacketType type) {
   if (!data || size == 0) {
      return NULL;
   }

   NetworkPacketReal *networkPacket = NULL;
   networkPacket = new NetworkPacketReal;
   if (networkPacket) {
      networkPacket->data.SetSize(size + RTMP_MAX_HEADER_SIZE);
      mcpy(networkPacket->data.Array() + RTMP_MAX_HEADER_SIZE, data, size);
      networkPacket->timestamp = timestamp;
      networkPacket->type = type;
      networkPacket->distanceFromDroppedFrame = 100000;
   }
   NETWORKPACKETDEBUG(networkPacket);
   return networkPacket;
}

NetworkPacketReal *RTMPPublisherReal::NetWorkPacketCreate(BYTE *data, UINT size, DWORD timestamp, PacketType type) {
   if (!data || size == 0) {
      return NULL;
   }

   NetworkPacketReal *networkPacket = NULL;
   networkPacket = new NetworkPacketReal;
   if (networkPacket) {
      networkPacket->data.CopyArray(data, size);
      networkPacket->timestamp = timestamp;
      networkPacket->type = type;
      networkPacket->distanceFromDroppedFrame = 100000;
   }
   NETWORKPACKETDEBUG(networkPacket);
   return networkPacket;
}
void RTMPPublisherReal::NetWorkPacketDestory(NetworkPacketReal *networkPacket) {
   if (networkPacket != NULL) {
      delete networkPacket;
      NETWORKPACKETDEBUG(networkPacket);
   }
}
bool RTMPPublisherReal::RTMPRecvDiscardBytes() {
   //DEBUGRTMP

   bool retType = true;
//#define DISCARDBUFSIZE 512
   OSEnterMutex(mRTMPMutex);

   RTMP_RecvDiscardBytes(mRtmp);
   /*
   int recv_size = 0;
   int ret = 0;
   ret = ioctlsocket(mRtmp->m_sb.sb_socket, FIONREAD,
                     (u_long*)&recv_size);
   if (ret >= 0 && recv_size > 0) {
      char buff[DISCARDBUFSIZE];
      while (recv_size > 0) {
         if (recv_size <= DISCARDBUFSIZE) {
            ret = recv(mRtmp->m_sb.sb_socket, buff, recv_size, 0);
            if (ret <= 0) {
               retType = false;
               break;
            }
            recv_size -= ret;
         } else {
            ret = recv(mRtmp->m_sb.sb_socket, buff, DISCARDBUFSIZE, 0);
            if (ret <= 0) {
               retType = false;
               break;
            }
            recv_size -= ret;
         }
      }
   }
   */
   OSLeaveMutex(mRTMPMutex);
   return retType;
}
void RTMPPublisherReal::RTMPInit() {
   DEBUGRTMP

      OSEnterMutex(mRTMPMutex);
   mRtmp = RTMP_Alloc();
   RTMP_Init(mRtmp, mMultiConnNum, mMultiConnBufSize);
   OSLeaveMutex(mRTMPMutex);

}
void RTMPPublisherReal::RTMPDestory() {
   DEBUGRTMP

      OSEnterMutex(mRTMPMutex);
   if (mRtmp) {
      RTMP_Close(mRtmp);
      RTMP_Free(mRtmp);
      mRtmp = NULL;
   }
   OSLeaveMutex(mRTMPMutex);
}

bool RTMPPublisherReal::RTMPSetup() {
   DEBUGRTMP

   OSEnterMutex(mRTMPMutex);
   String strBindIP;
   String strURL = mPublishURL;
   String strPlayPath = mPublishPlayPath;

   if(mlpAnsiURL)
   {
      Free(mlpAnsiURL);
      mlpAnsiURL=NULL;
   }

   if(mlpAnsiPlaypath)
   {
      Free(mlpAnsiPlaypath);
      mlpAnsiPlaypath=NULL;
   }

   char *rtmpUser = NULL;
   char *rtmpPass = NULL;
   
   if(m_bDispatch) {
         strURL += "?vhost=";
         strURL += m_dispatchParam.vhost;
         strURL += "?token=";
         strURL += m_dispatchParam.token;
         strURL += "?webinar_id=";
         strURL += m_dispatchParam.webinar_id;
         strURL += "?ismix=";         
         strURL += (m_dispatchParam.ismix==0?"0":"1");
         strURL += "?mixserver=";
         strURL += m_dispatchParam.mixserver;
         strURL += "?accesstoken=";
         strURL += m_dispatchParam.accesstoken;

         if(strcmp(m_dispatchParam.role,"host")==0) {
            strPlayPath+="@host@";
         }
         else{
            strPlayPath+="@";
            strPlayPath+=m_dispatchParam.userId;
            strPlayPath+="@";
         }
   }
   else {
      if (mPublishToken.IsEmpty() == false) {
         strURL += "?vhost=indexVhost&&";
         strURL += ARG_KEY_Token;
         strURL += "=";
         strURL += mPublishToken;
      }
   }

   
   
   strURL.KillSpaces();
   strPlayPath.KillSpaces();
   
   mlpAnsiURL = strURL.CreateUTF8String();
   
   #if 1
   mlpAnsiPlaypath = strPlayPath.CreateUTF8String();
   rtmpUser = mPublishUsername.CreateUTF8String();
   rtmpPass = mPublishPassword.CreateUTF8String();

   if (rtmpUser) {
      mRtmp->Link.pubUser.av_val = rtmpUser;
      mRtmp->Link.pubUser.av_len = (int)strlen(rtmpUser);
   }

   if (rtmpPass) {
      mRtmp->Link.pubPasswd.av_val = rtmpPass;
      mRtmp->Link.pubPasswd.av_len = (int)strlen(rtmpPass);
   }

   mRtmp->Link.swfUrl.av_len = mRtmp->Link.tcUrl.av_len;
   mRtmp->Link.swfUrl.av_val = mRtmp->Link.tcUrl.av_val;
   mRtmp->Link.flashVer.av_val = "FMLE/3.0 (compatible; FMSc/1.0)";
   mRtmp->Link.flashVer.av_len = (int)strlen(mRtmp->Link.flashVer.av_val);
   mRtmp->m_outChunkSize = 4096;
   mRtmp->m_bSendChunkSizeInfo = TRUE;
   mRtmp->m_bUseNagle = TRUE;

   strBindIP = L"Default";
   if (scmp(strBindIP, TEXT("Default"))) {
      if (schr(strBindIP.Array(), ':'))
         mRtmp->m_bindIP.addr.ss_family = AF_INET6;
      else
         mRtmp->m_bindIP.addr.ss_family = AF_INET;
      mRtmp->m_bindIP.addrLen = sizeof(mRtmp->m_bindIP.addr);

      if (WSAStringToAddress(strBindIP.Array(), mRtmp->m_bindIP.addr.ss_family, NULL, (LPSOCKADDR)&mRtmp->m_bindIP.addr, &mRtmp->m_bindIP.addrLen) == SOCKET_ERROR) {
         goto end;
      }
   }

   if (!RTMP_SetupURL2(mRtmp, mlpAnsiURL, mlpAnsiPlaypath)) {
      goto end;
   }

   RTMP_EnableWrite(mRtmp);
   #endif
   OSLeaveMutex(mRTMPMutex);
   return true;

end:
   rtmpUser != NULL ? Free(rtmpUser) : nop();
   rtmpPass != NULL ? Free(rtmpPass) : nop();
   OSLeaveMutex(mRTMPMutex);
   return false;
}
bool RTMPPublisherReal::RTMPConnect() {
   DEBUGRTMP

   bool ret = false;
   OSEnterMutex(mRTMPMutex);
   if (mRtmp != NULL&&RTMP_Connect(mRtmp, NULL)) {
      if (mRtmp != NULL&&RTMP_ConnectStream(mRtmp, 0)) {
         ret = true;
      }
   }
   if(mRtmp)
   {
      mServiceIP=mRtmp->m_server_ip;
   }
   OSLeaveMutex(mRTMPMutex);
   return ret;
}
void RTMPPublisherReal::RTMPInitMetaData() {

   if (mIsEncoderDataInitialized)
      return;

   mIsEncoderDataInitialized = true;
   metaDataPacketBuffer.resize(2048);

   char *enc = metaDataPacketBuffer.data() + RTMP_MAX_HEADER_SIZE;
   char *pend = metaDataPacketBuffer.data() + metaDataPacketBuffer.size();
   enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
   enc = AMF_EncodeString(enc, pend, &av_onMetaData);
   enc = EncMetaData(enc, pend, false, mAudioEncoder, mVideoEncoder);
   metaDataPacketBuffer.resize(enc - metaDataPacketBuffer.data());
   mAudioEncoder->GetHeaders(mAudioHeaders);
   mVideoEncoder->GetHeaders(mVideoHeaders);

   mCurrentVideoBits = mVideoBits = this->mVideoEncoder->GetBitRate();
}

bool RTMPPublisherReal::RTMPSendMetaData() {
   DEBUGRTMP

      bool ret = false;
   OSEnterMutex(mRTMPMutex);
   RTMPPacket packet;
   packet.m_nChannel = 0x03;     // control channel (invoke)
   packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
   packet.m_packetType = RTMP_PACKET_TYPE_INFO;
   packet.m_nTimeStamp = 0;
   packet.m_nInfoField2 = mRtmp->m_stream_id;
   packet.m_hasAbsTimestamp = TRUE;
   packet.m_body = metaDataPacketBuffer.data() + RTMP_MAX_HEADER_SIZE;

   packet.m_nBodySize = metaDataPacketBuffer.size() - RTMP_MAX_HEADER_SIZE;
   if (!RTMP_SendPacket(mRtmp, &packet, FALSE)) {
      if (mMediaCoreEvent)
         mMediaCoreEvent->OnNetworkEvent(Network_rtmp_header_send_failed);
      ret = false;
   } else {
      ret = true;
   }
   OSLeaveMutex(mRTMPMutex);
   return ret;
}
bool RTMPPublisherReal::RTMPSendVideoHeader() {
   DEBUGRTMP

      bool ret = false;
   OSEnterMutex(mRTMPMutex);
   RTMPPacket packet;
   List<BYTE> packetPadding;
   packet.m_nChannel = 0x04;
   packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
   packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
   
   packet.m_nInfoField2 = mRtmp->m_stream_id;
   packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
   packetPadding.AppendArray(mVideoHeaders.lpPacket, mVideoHeaders.size);
   packet.m_nTimeStamp=0;

   packet.m_body = (char*)packetPadding.Array() + RTMP_MAX_HEADER_SIZE;
   packet.m_nBodySize = mVideoHeaders.size;
   if (!RTMP_SendPacket(mRtmp, &packet, FALSE)) {
      if (mMediaCoreEvent)
         mMediaCoreEvent->OnNetworkEvent(Network_video_header_send_failed);
      ret = false;
   } else {
      ret = true;
   }
   OSLeaveMutex(mRTMPMutex);
   return ret;
}
bool RTMPPublisherReal::RTMPSendAudioHeader() {
   DEBUGRTMP

      bool ret = false;
   OSEnterMutex(mRTMPMutex);
   RTMPPacket packet;
   List<BYTE> packetPadding;
   packet.m_nChannel = 0x05; // source channel
   packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
   packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
   packet.m_nInfoField2 = mRtmp->m_stream_id;
   packet.m_nTimeStamp=0;

   packetPadding.SetSize(RTMP_MAX_HEADER_SIZE);
   packetPadding.AppendArray(mAudioHeaders.lpPacket, mAudioHeaders.size);

   packet.m_body = (char*)packetPadding.Array() + RTMP_MAX_HEADER_SIZE;
   packet.m_nBodySize = mAudioHeaders.size;
   if (!RTMP_SendPacket(mRtmp, &packet, FALSE)) {
      if (mMediaCoreEvent)
         mMediaCoreEvent->OnNetworkEvent(Network_audio_header_send_failed);
      ret = false;
   } else {
      ret = true;
   }
   OSLeaveMutex(mRTMPMutex);
   return ret;
}


bool RTMPPublisherReal::RTMPSendPacket(NetworkPacketReal *networkPacket) {
   if (!networkPacket) {
      return false;
   }
   NETWORKPACKETDEBUG2(networkPacket);

   bool ret = true;
   OSEnterMutex(mRTMPMutex);
   List<BYTE> packetData;
   PacketType type = networkPacket->type;
   DWORD      timestamp = networkPacket->timestamp;
   packetData.TransferFrom(networkPacket->data);
   RTMPPacket packet;
   packet.m_nChannel = (type == PacketType_Audio) ? 0x5 : 0x4;
   packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
   packet.m_packetType = (type == PacketType_Audio) ? RTMP_PACKET_TYPE_AUDIO : RTMP_PACKET_TYPE_VIDEO;
   packet.m_nTimeStamp = timestamp;
   packet.m_nInfoField2 = mRtmp != NULL ? mRtmp->m_stream_id : 0;
   packet.m_hasAbsTimestamp = TRUE;

   packet.m_nBodySize = packetData.Num() - RTMP_MAX_HEADER_SIZE;
   packet.m_body = (char*)packetData.Array() + RTMP_MAX_HEADER_SIZE;
   if (networkPacket->type != PacketType_Audio) {
      mVideoSendSize += packet.m_nBodySize;
   }

   if (!RTMPRecvDiscardBytes()) {
      return false;
   }
   #ifdef RTMPREALDEBUG
   if(networkPacket->type!=PacketType_Audio)
   {
      Log(TEXT("[send][%d]\t[%lu]\t[%d]\t[%lu]"),
         networkPacket->type,
         networkPacket->timestamp,
         mWaitPacketType,
         packet.m_nBodySize
         );
   }
   #endif

   
   if (!RTMP_SendPacket(mRtmp, &packet, FALSE)) {
      if (!RTMP_IsConnected(mRtmp)) {
         if (mMediaCoreEvent)
            mMediaCoreEvent->OnNetworkEvent(Network_server_disconnect);
      }
      ret = false;
   }
   if (networkPacket->type != PacketType_Audio) {
      mVideoSendCount++;
   }
   mBytesSent += packetData.Num();

   if(mAVSendSize==0)
   {
      mAVSendTime=os_gettime_ns() / 1000000;
   }
   
   mAVSendSize += packetData.Num();
   if (mVideoSendSize == 0) {
      mVideoSendTime = os_gettime_ns() / 1000000;
   }



   uint64_t endTime = os_gettime_ns() / 1000000;
   uint64_t diffTime = endTime - mVideoSendTime;
   if (diffTime > 1000) {
      double speed = mVideoSendSize;
      speed /= diffTime;
      speed *= 1000;
      mSpeed = speed;
      mVideoSendSize = 0;
      VideoBitUpper();
   }

   diffTime = endTime - mAVSendTime;
   if(diffTime>500)
   {
      double speed = mAVSendSize;
	  chunk_size = mAVSendSize/1024;
      speed /= diffTime;
      speed *= 1000;
      mAVSpeed= speed;
      mAVSendSize = 0;
	  update_time = endTime;  
   }

   OSLeaveMutex(mRTMPMutex);
   return ret;
}
void RTMPPublisherReal::SetAutoSpeed(bool autoSpeed) {
   return ;
   OSEnterMutex(mAutoSpeedMutex);
   if (!autoSpeed) {
      this->mCurrentVideoBits = mVideoBits;
      if (mVideoEncoder) {
         mVideoEncoder->SetBitRate(mVideoBits, 0);
      }
      mIsAutoSpeed = autoSpeed;
   }
   OSLeaveMutex(mAutoSpeedMutex);
}
int RTMPPublisherReal::GetSpeedLevel() {
#define FONT_COLOR_RED_LIMIT     2/3
#define FONT_COLOR_YELLOW_LIMIT  1/4

   int level=0;
   if(!mNumFramesDumped) {
      level = 0;
   }
   else if(mTotalVideoFrames*FONT_COLOR_RED_LIMIT<=mNumFramesDumped) {
      level = 2;
   }
   else if(mTotalVideoFrames*FONT_COLOR_YELLOW_LIMIT<=mNumFramesDumped) {
      level = 1;
   }
   //Log(TEXT("RTMPPublisherReal::GetSpeedLevel() %d"),level);

   return level;
}

bool RTMPPublisherReal::GetAutoSpeed() {
   bool ret = false;
   OSEnterMutex(mAutoSpeedMutex);
   ret = mIsAutoSpeed;
   OSLeaveMutex(mAutoSpeedMutex);
   return ret;
}
void RTMPPublisherReal::VideoBitUpperCountPlus() {
   OSEnterMutex(mVideoBitUpperCountMutex);
   mVideoBitUpperCount++;
   OSLeaveMutex(mVideoBitUpperCountMutex);
}
void RTMPPublisherReal::VideoBitUpperCountClear() {
   OSEnterMutex(mVideoBitUpperCountMutex);
   mVideoBitUpperCount = 0;
   OSLeaveMutex(mVideoBitUpperCountMutex);
}
bool RTMPPublisherReal::GetIsVideoBitUpper() {
   bool ret = false;
   OSEnterMutex(mVideoBitUpperCountMutex);
   if (mVideoBitUpperCount >= VIDEOBITUPPERSLEEP) {
      ret = true;
   }
   OSLeaveMutex(mVideoBitUpperCountMutex);
   return ret;
}

void RTMPPublisherReal::VideoBitUpper() {
   if (!GetAutoSpeed()) {
      return;
   }
   uint64_t currtimens = os_gettime_ns();
   if (currtimens - mSpeedChangeTime < SPEEDTIMECHANGEINTERVAL * 1000000000) {
      return;
   }

   if (mCurrentVideoBits<mVideoBits) {
      bool upper = true;
      UINT testSpeed = mSpeed * 8 / 1024;
      if (testSpeed>this->mCurrentVideoBits*2.0f) {
         testSpeed = this->mCurrentVideoBits*1.48f;
      } else if (testSpeed>this->mCurrentVideoBits*1.8f) {
         testSpeed = this->mCurrentVideoBits*1.38f;
      } else if (testSpeed > this->mCurrentVideoBits*1.6f) {
         testSpeed = this->mCurrentVideoBits*1.28f;
      } else if (testSpeed > this->mCurrentVideoBits*1.4f) {
         testSpeed = this->mCurrentVideoBits*1.18f;
      } else if (testSpeed > this->mCurrentVideoBits*1.2f) {
         testSpeed = this->mCurrentVideoBits*1.08f;
      } else {
         upper = false;
      }
      if (upper) {
         if (GetIsVideoBitUpper()) {
            this->mCurrentVideoBits = testSpeed;
            if (this->mCurrentVideoBits > mVideoBits) {
               mCurrentVideoBits = mVideoBits;
            }
            this->mVideoEncoder->SetBitRate(mCurrentVideoBits, 0);

            mSpeedChangeTime = os_gettime_ns();
            VHallDebug("UPPER SetSpeed %ld\n", mCurrentVideoBits);
         } else {
            VideoBitUpperCountPlus();
         }
      }
   }

}
void RTMPPublisherReal::VideoBitLower() {
   if (!GetAutoSpeed()) {
      return;
   }

   uint64_t currtimens = os_gettime_ns();
   if (currtimens - mSpeedChangeTime<SPEEDTIMECHANGEINTERVAL * 1000000000) {
      return;
   }

   this->mCurrentVideoBits = mSpeed / 1024 * 8 * 0.7;
   UINT minimumSpeed = this->mVideoBits*0.1;
   this->mCurrentVideoBits = this->mCurrentVideoBits>minimumSpeed ? this->mCurrentVideoBits : minimumSpeed;


   VHallDebug("LOWER SetSpeed %ld\n", mCurrentVideoBits);
   this->mVideoEncoder->SetBitRate(mCurrentVideoBits, 0);
   mSpeedChangeTime = os_gettime_ns();
   VideoBitUpperCountClear();
}
//丢帧
#define MAX_QUEUE_SIZE 250
#define MAX_QUEUE_DURATION 10000
void RTMPPublisherReal::QueueDropFrames2() {
	if (StatusGet() != STATUS_CONNECTED){
		return;
	}
	OSEnterMutex(mDataMutex);
	//如果队列大小为0 则不进行丢帧
	int queueSize = mQueuedPackets.size();
	if (queueSize == 0) {
		OSLeaveMutex(mDataMutex);
		return;
	}
	//第一个迭代器
	std::list<NetworkPacketReal *>::iterator Begin
		= mQueuedPackets.begin();
	//最后一个迭代器
	std::list<NetworkPacketReal *>::iterator End
		= mQueuedPackets.end();
	End--;
	//队列间隔
	DWORD queueDuration = (*End)->timestamp
		- (*Begin)->timestamp;
	bool BFrameDroped = false;
	bool PFrameDroped = false;

	while (queueSize > MAX_QUEUE_SIZE || queueDuration > MAX_QUEUE_DURATION){

		//drop PB frame first
		if (!BFrameDroped){
			bool bSetPriority = true;
			PacketType last_type = PacketType_VideoDisposable;
			std::list<NetworkPacketReal *>::iterator itera_last, itera;
			itera_last = itera = mQueuedPackets.begin();
			bool last_set = false;
			int i = 0;
			for (; itera != mQueuedPackets.end();) {
				if ((*itera)->type < PacketType_VideoHigh){
					last_type = (*itera)->type;
					NetWorkPacketDestory(*itera);
					mNumFramesDumped++;
					itera_last = itera = mQueuedPackets.erase(itera);
					last_set = true;
					i++;
				}
				else {
					itera++;
				}
			}
			//if there is a P frame after last_drop bframe do not bSetPriority
			if (last_set){
				for (; itera_last != mQueuedPackets.end(); itera_last++){
					if ((*itera_last)->type > PacketType_VideoLow){
						bSetPriority = false;
					}
				}
				if (bSetPriority){
					if (mWaitPacketType < PacketType_VideoLow){
						mWaitPacketType = PacketType_VideoLow;
					}
				}
			}
			BFrameDroped = true;
		}
		else if (!PFrameDroped){
			bool bSetPriority = true;
			PacketType last_type = PacketType_VideoDisposable;
			std::list<NetworkPacketReal *>::iterator itera_last, itera;
			itera_last = itera = mQueuedPackets.begin();
			bool last_set = false;
			int i = 0;
			for (; itera != mQueuedPackets.end();) {
				if ((*itera)->type < PacketType_VideoHighest){
					last_type = (*itera)->type;
					NetWorkPacketDestory(*itera);
					mNumFramesDumped++;
					itera_last = itera = mQueuedPackets.erase(itera);
					last_set = true;
					i++;
				}
				else {
					itera++;
				}
			}
			//if there is a I frame after last_drop pframe do not bSetPriority
			if (last_set){
				for (; itera_last != mQueuedPackets.end(); itera_last++){
					if ((*itera_last)->type == PacketType_VideoHighest){
						bSetPriority = false;
					}
				}
				if (bSetPriority){
					mWaitPacketType = PacketType_VideoHighest;
				}
			}
			PFrameDroped = true;
		}
		else {
			//check if can drop a hole gop.	
			std::list<NetworkPacketReal *>::iterator firstI, secondI, itera;
			firstI = secondI = mQueuedPackets.end();
			for (itera = mQueuedPackets.begin(); itera != mQueuedPackets.end(); itera++){
				if ((*itera)->type == PacketType_VideoHighest){
					if (firstI == mQueuedPackets.end()){
						firstI = itera;
					}
					else {
						secondI = itera;
						break;
					}
				}
			}
			if (firstI != mQueuedPackets.end() && secondI != mQueuedPackets.end()){
				DWORD offset = (*secondI)->timestamp - (*firstI)->timestamp;
				mFirstTimestamp += offset;
				for (itera = secondI; itera != mQueuedPackets.end(); itera++) {
					(*itera)->timestamp -= offset;
				}
				NetworkPacketReal * sI = *secondI;
				for (; *firstI != sI; ){
					if ((*firstI)->type != PacketType_Audio) {
						mNumFramesDumped++;
					}
					NetWorkPacketDestory(*firstI);			
					firstI = mQueuedPackets.erase(firstI);
				}		
			}
			else { //only one I frame and others are audio
				break;
			}
		}
		queueSize = mQueuedPackets.size();
		if (queueSize <= 0){
			break;
		}
		End = mQueuedPackets.end();
		End--;
		Begin = mQueuedPackets.begin();
		queueDuration = (*End)->timestamp - (*Begin)->timestamp;
		continue;
	}
	OSLeaveMutex(mDataMutex);
}


void RTMPPublisherReal::QueueDropFrames() {
   //只有连接状态才会丢帧
   if (StatusGet() == STATUS_CONNECTED) {
      
      OSEnterMutex(mDataMutex);
      //如果队列大小为0 则不进行丢帧
      int queueSize = mQueuedPackets.size();
      if (queueSize == 0) {
         OSLeaveMutex(mDataMutex);
         return;
      }




      //第一个迭代器
      std::list<NetworkPacketReal *>::iterator itera 
         = mQueuedPackets.begin();

      //第一个数据包
      NetworkPacketReal *packet = *itera;
      DWORD firstTimeStamp = packet->timestamp;

      //队列长度大于0
      if (queueSize > 0){
          //最小丢帧时间小于第一个时间戳
 //         && mMinFramedropTimestsamp < firstTimeStamp) {

         //第一个迭代器
         std::list<NetworkPacketReal *>::iterator itera 
            = mQueuedPackets.begin();

         for(;itera!=mQueuedPackets.end();itera++) {
            if((*itera)->type!=PacketType_Audio) {
               break;
            }
         }

         //最后一个迭代器
         std::list<NetworkPacketReal *>::iterator iteraEnd 
            = mQueuedPackets.end();
                
         for(;iteraEnd!=mQueuedPackets.begin();) {
            iteraEnd--;
            if((*iteraEnd)->type!=PacketType_Audio) {
               break;
            }
         }

         if (
            itera==iteraEnd
            ||
            (*iteraEnd)->type==PacketType_Audio
            ||
            (*itera)->type==PacketType_Audio
         ) {
            OSLeaveMutex(mDataMutex);
            return;
         }

         

         //队列间隔
         DWORD queueDuration = (*iteraEnd)->timestamp 
            - (*itera)->timestamp;






         
         //当前系统时间
         DWORD curTime = OSGetTime();

         //队列长度大于3000毫秒
         if (queueDuration >= QUEUEDROPPFRAMETIMES) {
            mMinFramedropTimestsamp = 
               (*iteraEnd)->timestamp;
            //不仅仅丢B帧
            while (DoIFrameDelay(false)); 

            mWaitPacketType = PacketType_VideoHighest;
            
            //Log(TEXT("[%s][%d]Set mWaitPacketType %d"),__FUNCTION__,__LINE__,mWaitPacketType);
            VideoBitLower();

         } else if (queueDuration >= QUEUEDROPBFRAMETIMES

                    && curTime - mLastBFrameDropTime >= QUEUEDROPPFRAMETIMES) {

            while (DoIFrameDelay(true));

            mLastBFrameDropTime = curTime;
            if(mWaitPacketType<PacketType_VideoHigh)
            {
               mWaitPacketType = PacketType_VideoHigh;
            }
            
            
            //Log(TEXT("[%d]Set mWaitPacketType %d"),__LINE__,mWaitPacketType);
            VideoBitLower();
         }
         //不丢帧
         else {
            //   VideoBitUpper();
         }
      } else {
         // VideoBitUpper();
      }

      OSLeaveMutex(mDataMutex);
   }
}
bool RTMPPublisherReal::DoIFrameDelay(bool bBFramesOnly) {

   int curWaitType = PacketType_VideoDisposable;
   if (StatusGet() != STATUS_CONNECTED) {
      return false;
   }
   
   while (  
            //不仅仅丢B帧  并且当前期望的类型小于关键帧
            !bBFramesOnly && curWaitType < PacketType_VideoHighest
         ||
            //仅仅是B帧 并且当前期望的类型小于P帧
            bBFramesOnly && curWaitType < PacketType_VideoHigh) {

      DWORD bestPacket = INVALID;
      UINT bestPacketDistance = 0;
      //如果期望是P帧
      if (curWaitType == PacketType_VideoHigh) {
         bool bFoundIFrame = false;
         //从后往前遍历队列
         std::list<NetworkPacketReal *>::iterator itera =
         mQueuedPackets.end();

         for (; itera != mQueuedPackets.begin();) {
            itera--;

            NetworkPacketReal *packet = *itera;

            if (packet->type == PacketType_Audio)
               continue;

            if (packet->type == curWaitType) {
               if (bFoundIFrame) {
                  bestPacket = (*itera)->timestamp;
                  
                  //Log(TEXT("bestPacket0 [%d]"),packet->type);
                  break;
               }
               else if (bestPacket == INVALID)
               {
                  bestPacket = (*itera)->timestamp;
                  //Log(TEXT("bestPacket1 [%d]"),packet->type);
               }
            } else if (packet->type == PacketType_VideoHighest)
               bFoundIFrame = true;
         }
      }
      else {
         //期待类型为B帧

         //从队列头开始遍历队列
         std::list<NetworkPacketReal *>::iterator itera
            = mQueuedPackets.begin();
         for (; itera != mQueuedPackets.end(); itera++) {
            
            NetworkPacketReal *packet = *itera;
            if (packet->type == PacketType_Audio)
            {
               continue;
            }
            //如果是B帧
            if (packet->type <= curWaitType) {
				bestPacket = (*itera)->timestamp;
			/*
              if (packet->distanceFromDroppedFrame > bestPacketDistance) {
                  bestPacket = (*itera)->timestamp;
                  #ifdef RTMPREALDEBUG
                  Log(TEXT("bestPacket2 [%d]"),packet->type);
                  #endif
                  bestPacketDistance = packet->distanceFromDroppedFrame;		  
               }*/
            }
         }
      }


      if (bestPacket != INVALID) {
         DropFrame(bestPacket);
         return true;
      }

      curWaitType++;
   }
   return false;
}
void RTMPPublisherReal::DropFrame(DWORD timeStamp) {
   std::list<NetworkPacketReal *>::iterator itera =mQueuedPackets.begin();

   for(;itera!=mQueuedPackets.end();itera++)
   {
      if((*itera)->timestamp==timeStamp)
      {
         if((*itera)->type!=PacketType_Audio)
         {
            break;
         }
      }
   }
   
   if (itera == mQueuedPackets.end()) {
      return;
   }


   PacketType type = (*itera)->type;

   for(std::list<NetworkPacketReal *>::iterator iteraB=mBufferedPackets.begin();
      iteraB!=mBufferedPackets.end();)
   {
      NetworkPacketReal *packet=*iteraB;
      if(packet->type==PacketType_Audio)
      {
         iteraB++;
         continue;
      }

      if(packet->type==PacketType_VideoHighest)
      {
		  break;  
      }

      else if(packet->type<=type)
      {
         //Log(TEXT("[drop0][%d]\t[%lu]\t[%d]"),packet->type,packet->timestamp,mWaitPacketType);
         NetWorkPacketDestory(packet);
         mBufferedPackets.erase(iteraB++);
      }
      else
      {
         
         //Log(TEXT("[no drop0][%d]\t[%lu]"),packet->type,packet->timestamp);
         iteraB++;
      }
   }
   

   if(type==PacketType_Audio)
   {
      return ;
   }
   
   if (itera != mQueuedPackets.end()) {
      for (; itera != mQueuedPackets.end(); itera++) {
         DWORD distance = ((*itera)->timestamp - timeStamp);
         if ((*itera)->distanceFromDroppedFrame <= distance)
            break;
         (*itera)->distanceFromDroppedFrame = distance;
      }
   }

   for(itera=mQueuedPackets.begin();itera!=mQueuedPackets.end();itera++)
   {
      if((*itera)->timestamp==timeStamp)
      {
         break;
      }
   }
   
   if (itera != mQueuedPackets.end() && itera != mQueuedPackets.begin()) {
      for (; itera != mQueuedPackets.begin();) {
         DWORD distance = timeStamp - (*itera)->timestamp;
         if ((*itera)->distanceFromDroppedFrame <= distance)
            break;
         (*itera)->distanceFromDroppedFrame = distance;
         
         itera--;
      }

   }

   for(itera=mQueuedPackets.begin();itera!=mQueuedPackets.end();itera++)
   {
      if((*itera)->timestamp==timeStamp)
      {
         break;
      }
   }

   
   if (itera != mQueuedPackets.end()) {
      bool bSetPriority = true;
      for (; itera != mQueuedPackets.end();) {

         NetworkPacketReal *packet = *itera;
         if (packet->type < PacketType_Audio) {
            if (type >= PacketType_VideoHigh) {
               if (packet->type < PacketType_VideoHighest) {   
                  mNumFramesDumped++;
                  #ifdef RTMPREALDEBUG
                  Log(TEXT("[drop1][%d]\t[%lu]\t[%d]"),
                     packet->type,packet->timestamp,mWaitPacketType);
                  #endif
                  NetWorkPacketDestory(packet);
                  if (itera != mQueuedPackets.begin()) {
                     mQueuedPackets.erase(itera++);
                  } else {
                     mQueuedPackets.erase(itera);
                     itera = mQueuedPackets.begin();
                  }

               } else {
                  bSetPriority = false;
                  break;
               }
            } else {
               if (packet->type >= type) {
                  bSetPriority = false;
                  break;
               }
               itera++;
            }
         } else {
            itera++;
         }
      }

      if (bSetPriority) {
         
         if (type >= PacketType_VideoHigh)
            mWaitPacketType = PacketType_VideoHighest;
         else {
            mWaitPacketType=PacketType_VideoHigh;
            #if 0
            if (mPacketWaitType < type)
               mPacketWaitType = type;
            #endif
         }
         #ifdef RTMPREALDEBUG
         Log(TEXT("bSetPriority[%d]"),type);
         #endif
      }
      else
      {
         mWaitPacketType = type;
         #ifdef RTMPREALDEBUG
         Log(TEXT("!bSetPriority[%d]"),type);
         #endif
      }
   }

   for(itera=mQueuedPackets.begin();itera!=mQueuedPackets.end();itera++)
   {
      if((*itera)->timestamp==timeStamp)
      {
         break;
      }
   }
   bool saveP=false;
   while(itera != mQueuedPackets.end()) {
      
      NetworkPacketReal *packet= *itera;
      if(packet->type==PacketType_Audio)
      {
         itera++;
         continue;
      }
      else if(packet->type==PacketType_VideoHighest)
      {
         break;
      }
      else
      {
         if(packet->type==PacketType_VideoHigh&&!saveP)
         {
            saveP=true;
            continue;
         }
         
         if (mWaitPacketType <= packet->type) {
            mWaitPacketType = PacketType_VideoHighest;
         }
         mNumFramesDumped++;
         #ifdef RTMPREALDEBUG
         Log(TEXT("[drop2][%d]\t[%lu]\t[%d]"),
            packet->type,
            packet->timestamp,
            mWaitPacketType);
         #endif
         NetWorkPacketDestory(*itera);
         mQueuedPackets.erase(itera++);
      }
   }
}
bool RTMPPublisherReal::RTMPSendInvoke() {
   DEBUGRTMP
   if(!mRtmp) {
      return false;
   }
   if (RTMP_IsConnected(mRtmp)) {
      if (mRtmp) {
         //closesocket(mRtmp->m_sb.sb_socket);
		  RTMP_DisconnectSock(mRtmp);
      }
   }
   return true;
}
enum RTMPPublisherStatus RTMPPublisherReal::StatusGet() {
   enum RTMPPublisherStatus status;
   OSEnterMutex(mStatusMutex);
   status = mStatus;
   OSLeaveMutex(mStatusMutex);
   return status;
}
bool RTMPPublisherReal::StatusSet(enum RTMPPublisherStatus status) {
   bool doMachineSet = false;
   if (StatusGet() != status) {
      doMachineSet = StatusMachineSet(StatusGet(), status);
   }
   return doMachineSet;
}
void RTMPPublisherReal::StatusLock() {
   OSEnterMutex(mStatusMutex);
}
void RTMPPublisherReal::StatusUnlock() {
   OSLeaveMutex(mStatusMutex);
}

bool RTMPPublisherReal::StatusSetInternal(enum RTMPPublisherStatus status) {
   OSEnterMutex(mStatusMutex);
   mStatus = status;
   OSLeaveMutex(mStatusMutex);
   return true;
}

bool RTMPPublisherReal::StatusMachineSet(enum RTMPPublisherStatus expectStatus, enum RTMPPublisherStatus targetStatus) {
   StatusMachine machineFunc = statusMachine[expectStatus][targetStatus];

   if ((this->*machineFunc)()) {
      if (mStatus == STATUS_CONNECTING) {
         ReleaseSemaphore(mConnectSempahore, 1, NULL);
      }
      return true;
   }
   return false;
}
bool RTMPPublisherReal::StatusMachineCallbackDoConnect() {
   bool ret = false;
   switch (StatusGet()) {
   case STATUS_CONNECTING:
      ret = false;
      break;
   case STATUS_STOPPING:
      ret = false;
      break;
   case STATUS_STOPPED:
      StatusMachineDoConnect();
      ret = true;
      break;
   default:
      break;
   }

   return ret;
}
bool RTMPPublisherReal::StatusMachineCallbackDoStop() {
   bool ret = false;
   switch (StatusGet()) {
   case STATUS_CONNECTED:
      StatusMachineDoStop();
      ret = true;
      break;
   case STATUS_STOPPING:
      break;
   default:
      break;
   }

   return ret;
}

bool RTMPPublisherReal::StatusMachineNop() {
   return false;
}

bool RTMPPublisherReal::StatusMachineDoConnect() {

   RTMPThreadCreate();
   return true;
}
bool RTMPPublisherReal::StatusMachineDoStop() {
   StatusSetInternal(STATUS_STOPPING);
   RTMPThreadTerminate();
   return true;
}

void RTMPPublisherReal::SendPacket(BYTE *data, UINT size, DWORD timestamp, PacketType type) {
#ifdef RTMPREALDEBUG
   if (type == PacketType_Audio) {
      Log(TEXT("RTMPPublisherReal::SendPacket AUDIO %lu"),timestamp);
   }
#endif   
   
   if(mExit)
   {
      return ;
   }
   
   QueueDropFrames2();
   
   RTMPInitMetaData();
   if (StatusSet(STATUS_CONNECTING)) {
      mIsStreamStarted = false;

   }
   
   if(StatusGet()!=STATUS_CONNECTED)
   {
      ReleaseSemaphore(mSendSempahore, 1, NULL);

	  if (type != PacketType_Audio){
		  mNumFramesDumpedByReconnected++;
	  }

      return ;
   }
   
   if (BufferNum() == MAX_BUFFERED_PACKETS) {
      NetworkPacketReal *packet = BufferFrontPop();
      QueuePushRtmp(packet);
      if (QueueNum())
         ReleaseSemaphore(mSendSempahore, 1, NULL);
   } 
   else if (BufferNum() == 0) {
      mFirstTimestamp = timestamp;
   }
   BufferPushRtmp(data, size, timestamp, type);
}
bool  RTMPPublisherReal::IsCanRestart() {
   bool ret = StatusGet() == STATUS_STOPPED;

   return ret;
}

void RTMPPublisherReal::Shutdown() {
   OSEnterMutex(mRTMPMutex);
   RTMPSendInvoke();
   OSLeaveMutex(mRTMPMutex);
}
void RTMPPublisherReal::WaitForShutdownComplete() {
   StatusSet(STATUS_STOPPING);
   if(mRtmpThreadHandle)
   {
      DWORD waitResult = WaitForSingleObject(mRtmpThreadHandle, 5000);
      if (waitResult == WAIT_OBJECT_0) {
         Log(TEXT("RTMPPublisherReal::WaitForShutdownComplete  thread exit."));
      } else if (waitResult == WAIT_TIMEOUT) {
         _ASSERT(FALSE);
         Log(TEXT("RTMPPublisherReal::WaitForShutdownComplete shutdown timeout."));
      } else {
         _ASSERT(FALSE);
         Log(TEXT("RTMPPublisherReal::WaitForShutdownComplete thread shutdown failed."));
      }
      CloseHandle(mRtmpThreadHandle);
      mRtmpThreadHandle = NULL;
   }
}
std::string RTMPPublisherReal::GetServerIP()
{
   return mServiceIP;
}
std::string RTMPPublisherReal::GetStreamID()
{
   return mStreamID;
}

void RTMPPublisherReal::SendLoop() {
   mLevel = 0;
   DWORD dwRet = 0;
   //mWaitKeyFrame = false;
   //SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
   while (StatusGet() != STATUS_STOPPING) {
      dwRet = WaitForSingleObject(mSendSempahore, 5000);
      if (dwRet == WAIT_TIMEOUT) {
         continue;
      }
      #if 1
      if (!mIsStreamStarted) {
         if (!RTMPSendMetaData()) {
            return;
         }

         if (!RTMPSendAudioHeader()) {
            return;
         }

         if (!RTMPSendVideoHeader()) {
            return;
         }
         
         strncpy_s(mNetworkConnectParam.serverIP,mRtmp->m_server_ip,20);
         strncpy_s(mNetworkConnectParam.streamID,mStreamID.c_str(),128);
         strncpy_s(mNetworkConnectParam.serverUrl,mServerUrl.c_str(),512);

         mMediaCoreEvent->OnNetworkEvent(Network_publish_success, &mNetworkConnectParam);
         mIsStreamStarted = true;
      }

      while (true) {
         if (StatusGet() == STATUS_STOPPING || QueueNum() == 0) {
            break;
         }
         NetworkPacketReal *networkPacket = QueueFrontPop();
         NETWORKPACKETDEBUG(networkPacket);

         if (!RTMPSendPacket(networkPacket)) {
            
            logWarn(TEXT("RTMPPublisherReal::RTMPSendPacket Failed %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
#if 0
            QueuePushRtmp(networkPacket);
#else
            NetWorkPacketDestory(networkPacket);
#endif
            return;
         }

         NetWorkPacketDestory(networkPacket);
         networkPacket = NULL;
      }
      #endif
      if (StatusGet() == STATUS_STOPPING){
         logInfo(TEXT("RTMPPublisherReal::STATUS_STOPPING %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
         return;
      }
   }
}
void RTMPPublisherReal::RTMPThreadCreate() {
   DEBUGRTMP
   OSEnterMutex(mRtmpThreadHandleMutex);
   if (mRtmpThreadHandle != NULL) {
      DWORD ThreadExitCode=-1;
      bool ret = GetExitCodeThread(mRtmpThreadHandle, &ThreadExitCode);
      if (ThreadExitCode!=0) {
         OSLeaveMutex(mRtmpThreadHandleMutex);
         return;
      }
   }

   StatusSetInternal(STATUS_CONNECTING);

   RTMPInit();
   mRtmpThreadExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
   HANDLE handle = OSCreateThread((XTHREAD)RTMPThread, this);
   mRtmpThreadHandle = handle;
   OSLeaveMutex(mRtmpThreadHandleMutex);
}
void RTMPPublisherReal::RTMPThreadTerminate() {
   DEBUGRTMP

      OSEnterMutex(mRtmpThreadHandleMutex);
   if (mRtmpThreadHandle == NULL) {
      StatusSetInternal(STATUS_STOPPED);
      OSLeaveMutex(mRtmpThreadHandleMutex);
      return;
   }

   RTMPSendInvoke();
   ReleaseSemaphore(mSendSempahore, 1, NULL);
   if (mRtmpThreadExitEvent != NULL) {
      WaitForSingleObject(mRtmpThreadExitEvent, INFINITE);
   }
   OSLeaveMutex(mRtmpThreadHandleMutex);

   RTMPThreadExit();

}
void RTMPPublisherReal::RTMPThreadExit() {
   DEBUGRTMP

   OSEnterMutex(mRtmpThreadHandleMutex);
   if (mRtmpThreadExitEvent != NULL) {
      CloseHandle(mRtmpThreadExitEvent);
      mRtmpThreadExitEvent = NULL;
   }

   RTMPDestory();
   #if 0
   if(mRtmpThreadHandle)
   {
      OSCloseThread(mRtmpThreadHandle);
      mRtmpThreadHandle = NULL;
   }
   #endif
   OSLeaveMutex(mRtmpThreadHandleMutex);

}
DWORD RTMPPublisherReal::RTMPThread(RTMPPublisherReal *publisher) {
   return publisher->RTMPThreadLoop();
}
DWORD RTMPPublisherReal::RTMPThreadLoop() {
   BufferQueueTidy();
   mConnect_count++;
   logInfo(TEXT("RTMPPublisherReal::RTMPThreadLoop %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
   mAVSpeed=0;
   DWORD dwRet = WaitForSingleObject(mConnectSempahore, 50000);
   if (dwRet == WAIT_TIMEOUT) {
      logWarn(TEXT("RTMPPublisherReal::RTMPThreadLoop WaitForSingleObject %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
	  
      return 0;
   }
   bool bSuccess = false;
   logInfo(TEXT("RTMPPublisherReal::RTMPSetup %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());

   if (!RTMPSetup()) {
      logWarn(TEXT("RTMPPublisherReal::RTMPSetup Failed %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
      goto end;
   }

   logInfo(TEXT("RTMPPublisherReal::RTMPConnect %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
   #if 1

   if (!RTMPConnect()) {
      logWarn(TEXT("RTMPPublisherReal::RTMPConnect Failed %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
      goto end;
   }

   bSuccess = true;
   logInfo(TEXT("RTMPPublisherReal::BufferQueueTidy %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
   BufferQueueTidy();
   mSpeed = 0.0f;
   #endif
end:
   
   if (!bSuccess) {
      logWarn(TEXT("RTMPPublisherReal::Thread Failed %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
      StatusSetInternal(STATUS_STOPPING);
      
      if (mMediaCoreEvent) {
         strncpy_s(mNetworkConnectParam.serverIP,mServiceIP.c_str(),20);
         strncpy_s(mNetworkConnectParam.streamID,mStreamID.c_str(),128);
         strncpy_s(mNetworkConnectParam.serverUrl,mServerUrl.c_str(),512);
         mMediaCoreEvent->OnNetworkEvent(Network_connect_failed,&mNetworkConnectParam);
      }

      Sleep(3000);
      SetEvent(mRtmpThreadExitEvent);

      RTMPThreadExit();
      StatusSetInternal(STATUS_STOPPED);

      mSpeed = 0.0f;

	  BufferQueueTidy();
      return 0;
   }
   
   logInfo(TEXT("RTMPPublisherReal::Connect Success %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());

   StatusSetInternal(STATUS_CONNECTED);
   mWaitPacketType = PacketType_VideoDisposable;

   HANDLE handle = OSCreateThread((XTHREAD)RateControlThread, this);

   logInfo(TEXT("RTMPPublisherReal::SendLoop %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
   SendLoop();
   
   StatusSetInternal(STATUS_STOPPING);
   logInfo(TEXT("RTMPPublisherReal::SendLoop End %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());

   Sleep(1000);
   SetEvent(mRtmpThreadExitEvent);

   RTMPThreadExit();
   StatusSetInternal(STATUS_STOPPED);

   mSpeed = 0.0f;
   
   logInfo(TEXT("RTMPPublisherReal::Thread End %s/%s"),mPublishURL.Array(),mPublishPlayPath.Array());
   BufferQueueTidy();
   return 0;
}

DWORD RTMPPublisherReal::RateControlThread(RTMPPublisherReal* publisher)
{
	return publisher->RateControlThreadLoop();
}

DWORD RTMPPublisherReal::RateControlThreadLoop()
{
#if 1
	return 0;
#endif

	while (true)
	{
		Sleep(5000);
		OSEnterMutex(mDataMutex);
		int queueSize = mQueuedPackets.size();
		if (queueSize < 50) {
			OSLeaveMutex(mDataMutex);
			return 0;
		}
		std::list<NetworkPacketReal *>::iterator iter_begin
			= mQueuedPackets.begin();
		std::list<NetworkPacketReal *>::iterator iter_end
			= mQueuedPackets.begin();
		NetworkPacketReal *first_packet = *iter_begin;
		NetworkPacketReal *last_packet = *iter_end;
		DWORD time_interval = last_packet->timestamp - first_packet->timestamp;
		OSLeaveMutex(mDataMutex);
		if (time_interval >= 5000)
		{
			return 1;
		}
	}
}
void RTMPPublisherReal::SetDispath(bool bDispatch,const Dispatch_Param &param){
   m_bDispatch = bDispatch;
   m_dispatchParam = param;
}

