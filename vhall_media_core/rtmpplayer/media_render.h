#ifndef _VIDEO_DEC_INCLUDE_H_
#define _VIDEO_DEC_INCLUDE_H_

#include "talk/base/messagequeue.h"
#include "buffer_queue.h"
#include "media_notify.h"
#include "../common/live_define.h"

/*
 render
 根据timestamp渲染YUV和播放pcm
 */
class VhallPlayerInterface;
namespace talk_base {
   class Thread;
}

class MediaRender : public talk_base::MessageHandler, public IMediaNotify {
public:
   MediaRender(VhallPlayerInterface*vinnyLive,
               uint64_t maxBufferTimeInMs = RENDER_BUFFER_TIME);
public:
   
   ~MediaRender();
   virtual bool InitLiveModel(LiveModel liveModel);
   virtual bool InitAudio(AudioParam* audioParam);
   virtual bool InitVideo(VideoParam* videoParam);
   virtual void Destory();
   virtual DataUnit* MallocDataUnit(const STREAM_TYPE& streamType, const long& bufferSize, const int& dropCnt);
   virtual bool AppendStreamPacket(const STREAM_TYPE& streamType, DataUnit* dataUnit);
   
private:
   enum {
      MSG_AUDIO_MediaRender_INIT,
      MSG_VIDEO_MediaRender_INIT,
      MSG_AUDIO_MediaRender,
      MSG_VIDEO_MediaRender,
      MSG_AMF_MSG_MediaRender,
      MSG_AUDIO_MediaRender_DESTORY,
      MSG_VIDEO_MediaRender_DESTORY,
      MSG_BUFFERR_NOTIFY
   };
   void OnMessage(talk_base::Message* msg);
   void OnInitAudio(AudioParam* audioParam);
   void OnInitVideo(VideoParam* videoParam);
   
   void AudioRenderLoop();
   void VideoRenderLoop();
   void AmfMsgRenderLoop();
   
   int MediaRenderAudio();
   int MediaRenderVideo();
private:
   talk_base::Thread* mAudioRenderThread;
   talk_base::Thread* mVideoRenderThread;
   talk_base::Thread* mAmfMsgRenderThread;
   
   BufferQueue* mAudioRawDataQueue;
   BufferQueue* mVideoRawDataQueue;
   BufferQueue* mAmfMsgQueue;
   
   LiveModel mLiveModel;
   
   bool mStoping;
   bool mAudioAlreadInit;
   bool mVideoAlreadInit;
   uint64_t  mLastAudioMediaRenderStartTime;
   uint64_t  mLastVideoMediaRenderStartTime;
   uint64_t  mAudioMediaRenderTime;
   uint64_t  mVideoMediaRenderTime;
   //uint64_t  mSleepTimeInMs;
   uint64_t  mMaxBufferTimeInMs;
   uint64_t  mAudioPlayDurationInMs;
   uint64_t  mAudioUnitTimeInMs;
   uint64_t  mVideoUnitTimeInMs;
   uint64_t  mVideoFrameDurationInMs;
   uint64_t  mVideoNextFrameRenderTimestamp;
   uint64_t  mVideoFrameSize;
   AudioParam mAudioParam;
   VideoParam mVideoParam;
   VhallPlayerInterface* mVinnyLive;
};
#endif
