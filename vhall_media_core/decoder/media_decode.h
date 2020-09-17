#ifndef _AUDIO_DEC_INCLUDE_H_
#define _AUDIO_DEC_INCLUDE_H_

#include "talk/base/messagequeue.h"
#include "../rtmpplayer/buffer_queue.h"
#include "../rtmpplayer/media_notify.h"
#include "../common/live_define.h"
#include <atomic>

//前向引用
namespace talk_base {
    class Thread;
}
class AACDecoder;
class Decoder;
class VhallPlayer;
class VhallPlayerInterface;
namespace talk_base {
   class Thread;
}

class MediaDecode : public talk_base::MessageHandler, public IMediaNotify {
public:
   MediaDecode(VhallPlayerInterface*player,
               uint64_t maxBufferTimeInMs = DEFAULT_BUFFER_TIME);
public:
   ~MediaDecode();
   void AddMediaInNotify(IMediaNotify* mediaNotify);
   void ClearMediaInNotify();
   void SetMaxBufferTimeInMs(const uint64_t& maxBufferTimeInMs);
   virtual bool InitLiveModel(LiveModel liveModel);
   virtual bool InitAudio(AudioParam* audioParam);
   virtual bool InitVideo(VideoParam* videoParam);
   virtual void Destory();
   virtual DataUnit* MallocDataUnit(const STREAM_TYPE& streamType, const long& bufferSize, const int& dropCnt);
   virtual bool AppendStreamPacket(const STREAM_TYPE& streamType, DataUnit* dataUnit);

private:
   enum {
      MSG_AUDIO_DECODE_INIT,
      MSG_VIDEO_DECODE_INIT,
      MSG_AUDIO_DECODE,
      MSG_VIDEO_DECODE,
      MSG_AUDIO_DECODE_DESTORY,
      MSG_VIDEO_DECODE_DESTORY
   };
   virtual void OnMessage(talk_base::Message* msg);
   void OnInitAudio(AudioParam* audioParam);
   void OnInitVideo(VideoParam* videoParam);
   void OnDecodeAudio();
   void OnDecodeVideo();
   void OnAudioDestory();
   void OnVideoDestory();
   void ProcessBuffer();
   void BufferMonitor();
   //int  BufferMonitor(const STREAM_TYPE& streamType);
   int  CalcAudioBufferSize(AudioParam* audioParam);
   int  CalcVideoBufferSize(VideoParam* videoParam);
   int DropDataUnits(BufferQueue* bufferQueue, const uint64_t& timestap);
private:
   talk_base::Thread* mAudioWorkerThread;
   talk_base::Thread* mVideoWorkerThread;
   AACDecoder *mAacDecoder;
   Decoder* mVideoDecoder;
   LiveModel mLiveModel;
   BufferQueue* mEncodedAudioQueue;
   BufferQueue* mEncodedVideoQueue;
   std::vector<IMediaNotify*> mMediaOutputNotify;
   
   std::atomic_bool mAudioStart;
   std::atomic_bool mVideoStart;
   std::atomic_bool mHasAudio;
   std::atomic_bool mHasVideo;

   volatile unsigned long long mLastAudioDecodeTimestamp;
   volatile unsigned long long mLastVideoDecodeTimestamp;
   
   unsigned char* mAudioFrameBuffer;
   unsigned long mAudioFrameBufferSize;
   unsigned int mAudioFrameDataSize;
   
   unsigned char* mVideoFrameBuffer;
   unsigned long mVideoFrameBufferSize;
   
   //calculate timestamp
   std::atomic_bool mFirstAudioDecoded;
   std::atomic_bool mFirstVideoDecoded;
   unsigned long long mLastDropFrameTimeStamp;
   unsigned long long mRawVideoFrameCnt;
   VideoParam mVideoParam;
   VhallPlayerInterface * mPlayer;
   v_mutex_t mBufferManagerMutex;
   int mMaxBufferTimeInMs;
   std::atomic_bool mIsInBuffering;
   std::atomic_bool mIsInBufferingFlags;
   volatile uint64_t mBufferStartTime;
};
#endif
