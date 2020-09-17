#ifndef __MEDIA_MUTEX_INCLUDE_H__
#define __MEDIA_MUTEX_INCLUDE_H__

#include "IEncoder.h"
#include "MediaDefs.h"
#include <vector>

struct FrameAudio {
   List<BYTE> audioData;
   QWORD timestamp;
};

struct VideoPacketData {
   List<BYTE> data;
   PacketType type;

   inline void Clear() { data.Clear(); }
};

struct VideoSegment {
   List<VideoPacketData> packets;
   DWORD timestamp;
   DWORD pts;

   inline VideoSegment() : timestamp(0), pts(0) {}
   inline ~VideoSegment() { Clear(); }
   inline void Clear() {
      for (UINT i = 0; i < packets.Num(); i++)
         packets[i].Clear();
      packets.Clear();
   }
};
struct StopInfo {
   DWORD time = (DWORD)-1;
   bool timeSeen = false;
   std::function<void()> func;
};

struct AudioStats;
struct VideoStats;
class MediaMutex {
public:
   MediaMutex(IMediaCoreEvent* mediaCoreEvent);
   ~MediaMutex();
public:
   bool InitEncoder(const VideoInfo &videoInfo, const AudioInfo &audioInfo);
   bool AddRtmpPublisher(const RtmpInfo &rtmpInfo);
   bool AddFileStream(String &recordFile);
   void DeleteFileStream();
   void EncodeAudioSegment(float *buffer, UINT numFrames, QWORD timestamp);
   bool EncodeVideo(LPVOID *inputPic[], QWORD Timestamp, SceneType sceneType, int maxBitrate);
   bool BufferVideoData(const List<DataPacket> &inputPackets,
                        const List<PacketType> &inputTypes,
                        DWORD timestamp,
                        DWORD out_pts,
                        QWORD firstFrameTime,
                        VideoSegment &segmentOut);
   void SendFrame(VideoSegment &curSegment, QWORD firstFrameTime);
   void SendVideoFrames();
   void Shutdown();
   void Destory();
   void RestartNetwork();
   void UpdateStats(AudioStats *audioStats, VideoStats *videoStats);
   void UpdateStreamStats(std::vector<StreamStats> *);
   int GetSumSpeed();
   UINT64 GetSendVideoFrameCount();
   int GetSpeedLevel();
   void SetAutoSpeed(bool);
   bool ReInitRtmp();
   
   bool ResetPublishInfo(const char *currentUrl,const char *nextUrl);
   void ClearRtmpInfo();
private:
   bool ReInitRtmpPublisher();
   bool HandleStreamStopInfo(StopInfo &, PacketType, const VideoSegment&);
private:
   VideoInfo mVideoInfo;
   AudioInfo mAudioInfo;
   //RtmpInfo  mRtmpInfo;


   String   mRecordFile;
   VideoEncoder* mVideoEncoder;
   AudioEncoder* mAudioEncoder;
   List<VideoSegment> mBufferedVideo;
   CircularList<DWORD> mBufferedTimes;
   //无用，为0
   QWORD mFirstFrameTime;

   HANDLE  mSoundThread;
   HANDLE mSoundDataMutex;//, hRequestAudioEvent;
   List<FrameAudio> mPendingAudioFrames;
   QWORD mLastAudioTimestamp;
   //是否发送头############################
   bool mIsSentHeaders;

   //only support one network stream and one file stream
   List<NetworkStream *> mNetwork;
   
   std::unique_ptr<VideoFileStream> mFileStream;
   std::unique_ptr<VideoFileStream> mReplayBufferStream;
   StopInfo mNetworkStop;
   StopInfo mFileStreamStop;
   StopInfo mReplayBufferStop;

   
   //rtmp info
   std::vector<std::string> mRtmpURL;
   //String mStreamname;
   std::vector<std::string> mRtmpPlaypath;
   std::vector<std::string> mUserName;
   std::vector<std::string> mUserPass;
   std::vector<std::string> mToken;
   std::vector<int> mMultiConnNum;
   std::vector<int> mMultiConnBufSize;

   int mRtmpCount;

   IMediaCoreEvent* mMediaCoreEvent;
   
   bool m_bDispatch = false;
   struct Dispatch_Param m_dispatchParam;
};

#endif 
