#include "media_decode.h"
#include "aac_decoder.h"
#include "h264_decoder.h"
#include "hw_decoder.h"
#include "../common/vhall_log.h"
#include "../common/live_message.h"
#include "../rtmpplayer/media_render.h"
#include "../api/live_interface.h"
#include <live_sys.h>
#include "talk/base/thread.h"
#include "../rtmpplayer/vhall_live_player.h"

/*
 1. max encoded data buffer is MAX_BUFFER_TIME*2, it encoded data buffer time > MAX_BUFFER_TIME, will accelerate video render
 2. when init player, if encoded data buffer time > INIT_PLAY_BUFFER_TIME, will decode it immediately.
 3. while encoder data empty, will buffer INIT_PLAY_BUFFER_TIME.
 */

MediaDecode::MediaDecode(VhallPlayerInterface*player,
                         uint64_t maxBufferTimeInMs):
mAudioWorkerThread(NULL),
mVideoWorkerThread(NULL),
mAacDecoder(0),
mVideoDecoder(0),
mEncodedAudioQueue(0),
mEncodedVideoQueue(0),
mLastAudioDecodeTimestamp(0),
mLastVideoDecodeTimestamp(0),
mLastDropFrameTimeStamp(0),
mAudioFrameBuffer(0),
mAudioFrameBufferSize(0),
mVideoFrameBuffer(0),
mVideoFrameBufferSize(0),
mMaxBufferTimeInMs(maxBufferTimeInMs),
mBufferStartTime(0),
mPlayer(player)
{
   avcodec_register_all();
   av_log_set_level(AV_LOG_ERROR);
   av_log_set_callback(av_log_default_callback);
   mFirstAudioDecoded = false;
   mAudioStart = false;
   mVideoStart = false;
   mHasAudio = false;
   mHasVideo = false;
   mFirstVideoDecoded = false;
   mIsInBuffering = true;
   mIsInBufferingFlags = true;
   v_mtuex_init(&mBufferManagerMutex);
   memset((void*)&mVideoParam, 0, sizeof(VideoParam));
   mMaxBufferTimeInMs = MAX(mMaxBufferTimeInMs, DEFAULT_BUFFER_TIME);
   mVideoWorkerThread = new talk_base::Thread();
   if (mVideoWorkerThread==NULL) {
      LOGE("mVideoWorkerThread is NULL");
   }
   mVideoWorkerThread->Start();
   
   mAudioWorkerThread = new talk_base::Thread();
   if (mAudioWorkerThread == NULL) {
      LOGE("mAudioWorkerThread is NULL");
   }
   mAudioWorkerThread->Start();
}

MediaDecode::~MediaDecode() {
   Destory();
   VHALL_THREAD_DEL(mVideoWorkerThread);
   VHALL_THREAD_DEL(mAudioWorkerThread);
   VHALL_DEL(mEncodedVideoQueue);
   VHALL_DEL(mEncodedAudioQueue);
   VHALL_DEL(mAacDecoder);
   VHALL_DEL(mVideoDecoder);
   VHALL_DEL(mAudioFrameBuffer);
   VHALL_DEL(mVideoFrameBuffer);
   v_mutex_destroy(&mBufferManagerMutex);
}

void MediaDecode::AddMediaInNotify(IMediaNotify* mediaNotify) {
   mMediaOutputNotify.push_back(mediaNotify);
}

void MediaDecode::ClearMediaInNotify(){
   mMediaOutputNotify.clear();
}

void MediaDecode::SetMaxBufferTimeInMs(const uint64_t& maxBufferTimeInMs){
   mMaxBufferTimeInMs = MAX(maxBufferTimeInMs, DEFAULT_BUFFER_TIME);
}

bool MediaDecode::InitLiveModel(LiveModel liveModel){
   mLiveModel = liveModel;
   for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
      mMediaOutputNotify[i]->InitLiveModel(liveModel);
   }
   return true;
}

bool MediaDecode::InitAudio(AudioParam* audioParam) {
   LOGI("Init audio decode, will post init audio message.");
   if(mAudioFrameBuffer == NULL){
      mAudioFrameBufferSize =  19200*4;
      mAudioFrameBuffer = (unsigned char*)malloc(mAudioFrameBufferSize);
   }
   int maxQueueSize = CalcAudioBufferSize(audioParam);
   if (mEncodedAudioQueue == NULL) {
      mEncodedAudioQueue = new BufferQueue(0, maxQueueSize);
   }
   if (mEncodedAudioQueue == NULL) {
      LOGE("malloc new encoded audio queue failed");
      return false;
   }

   mEncodedAudioQueue->SetQueueSize(maxQueueSize);
   v_lock_mutex(&mBufferManagerMutex);
   mAudioStart = false;
   mHasAudio = true;
   v_unlock_mutex(&mBufferManagerMutex);
   mEncodedAudioQueue->Flush();
   msleep(1);
   mEncodedAudioQueue->Reset();
   LOGI("Set audio Queue buffer size=%d, queue size=%d, buffered/free=%d/%d."
        ,0, maxQueueSize, mEncodedAudioQueue->GetDataUnitCnt(), mEncodedAudioQueue->GetFreeUnitCnt());
   
   mAudioWorkerThread->Post(this, MSG_AUDIO_DECODE_INIT, new AudioParamMessageData(audioParam));
   return true;
}

bool MediaDecode::InitVideo(VideoParam* videoParam) {
   LOGI("Init video decode, will post init video message.");
   int maxQueueSize = CalcVideoBufferSize(videoParam);
   if (mEncodedVideoQueue == NULL) {
      mEncodedVideoQueue = new BufferQueue(0, maxQueueSize);
   }
   if(mVideoFrameBuffer == NULL){
      mVideoFrameBufferSize = MAX_FRAME_SIZE;
      mVideoFrameBuffer = (unsigned char*)malloc(mVideoFrameBufferSize);
   }
   if (mVideoFrameBuffer == NULL) {
      LOGE("malloc new encoded video queue failed");
      return false;
   }
   if (mEncodedVideoQueue == NULL) {
      LOGE("malloc new encoded video queue failed");
      return false;
   }
   v_lock_mutex(&mBufferManagerMutex);
   mVideoStart = false;
   mHasVideo = true;
   mVideoParam = *videoParam;
   v_unlock_mutex(&mBufferManagerMutex);
   mEncodedVideoQueue->SetQueueSize(maxQueueSize);
   mEncodedVideoQueue->Flush();
   msleep(1);
   mEncodedVideoQueue->Reset();
   LOGI("Init video Queue buffer size=%d, queue size=%d, buffered/free=%d/%d."
        ,0, maxQueueSize, mEncodedVideoQueue->GetDataUnitCnt(), mEncodedVideoQueue->GetFreeUnitCnt());
   
   mVideoWorkerThread->Post(this, MSG_VIDEO_DECODE_INIT, new VideoParamMessageData(videoParam));
   return true;
}

void MediaDecode::Destory() {
   LOGI("Destory media decode proc.");
   if(mVideoStart){
      mVideoStart = false;
      mVideoWorkerThread->Clear(this);
      if(mEncodedVideoQueue)
         mEncodedVideoQueue->Flush();
      mVideoWorkerThread->Post(this, MSG_VIDEO_DECODE_DESTORY);
   }
   if(mAudioStart){
      mAudioStart = false;
      mAudioWorkerThread->Clear(this);
      if(mEncodedAudioQueue)
         mEncodedAudioQueue->Flush();
      mAudioWorkerThread->Post(this, MSG_AUDIO_DECODE_DESTORY);
   }
   for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
      mMediaOutputNotify[i]->Destory();
   }
   v_lock_mutex(&mBufferManagerMutex);
   mHasAudio = false;
   mHasVideo = false;
   mBufferStartTime = 0;
   mIsInBufferingFlags = true;
   mIsInBuffering = true;
   v_unlock_mutex(&mBufferManagerMutex);
}

DataUnit* MediaDecode::MallocDataUnit(const STREAM_TYPE& streamType,
                                      const long& bufferSize,
                                      const int& dropCnt) {
   DataUnit* newPkt = NULL;
   if (STREAM_TYPE_VIDEO == streamType) {
      ASSERT(mEncodedVideoQueue != NULL);
      newPkt = mEncodedVideoQueue->MallocDataUnit(bufferSize, dropCnt == 0);
   } else if (STREAM_TYPE_AUDIO == streamType) {
      ASSERT(mEncodedAudioQueue != NULL);
      newPkt = mEncodedAudioQueue->MallocDataUnit(bufferSize);
   }else if(STREAM_TYPE_ONCUEPONIT_MSG == streamType){
      if (mMediaOutputNotify.size()>0) {
         newPkt = mMediaOutputNotify[0]->MallocDataUnit(streamType, bufferSize, dropCnt);
      }
   }
   return newPkt;
}

bool MediaDecode::AppendStreamPacket(const STREAM_TYPE& streamType, DataUnit* dataUnit) {
   if (STREAM_TYPE_VIDEO == streamType) {
      mEncodedVideoQueue->PutDataUnit(dataUnit);
   } else if (STREAM_TYPE_AUDIO == streamType) {
      mEncodedAudioQueue->PutDataUnit(dataUnit);
   } else if (STREAM_TYPE_ONCUEPONIT_MSG == streamType){
      for (auto item:mMediaOutputNotify) {
         item->AppendStreamPacket(streamType, dataUnit);
      }
   }
   ProcessBuffer();
   return true;
}

void MediaDecode::OnMessage(talk_base::Message* msg) {
   AudioParamMessageData* apmd = NULL;
   VideoParamMessageData * vpmd = NULL;
   switch (msg->message_id) {
      case MSG_AUDIO_DECODE_INIT:
         apmd = static_cast<AudioParamMessageData*>(msg->pdata);
         OnInitAudio(&apmd->mParam);
         break;
      case MSG_VIDEO_DECODE_INIT:
         vpmd = static_cast<VideoParamMessageData*>(msg->pdata);
         OnInitVideo(&vpmd->mParam);
         break;
      case MSG_AUDIO_DECODE:
         OnDecodeAudio();
         break;
      case MSG_VIDEO_DECODE:
         OnDecodeVideo();
         break;
      case MSG_AUDIO_DECODE_DESTORY:
         OnAudioDestory();
         break;
      case MSG_VIDEO_DECODE_DESTORY:
         OnVideoDestory();
         break;
      default:
         break;
   }
   delete msg->pdata;
   msg->pdata = NULL;
}

void MediaDecode::OnInitAudio(AudioParam* audioParam) {
   bool ret = false;
   if (mAacDecoder) {
      LOGI("Delete last audio decoder.");
      delete mAacDecoder;
   }
   mAacDecoder = new AACDecoder(audioParam);
   if (mAacDecoder == NULL) {
      LOGE("Create audio decoder failed. aac_extra_size=%d.", audioParam->extra_size);
      return;
   }
   ret = mAacDecoder->Init();
   if(false == ret){
      VHALL_DEL(mAacDecoder);
      LOGE("Init audio decoder failed. aac_extra_size=%d.", audioParam->extra_size);
      return;
   }
   v_lock_mutex(&mBufferManagerMutex);
   mAudioStart = true;
   v_unlock_mutex(&mBufferManagerMutex);
   VHALL_DEL(audioParam->extra_data);
   mFirstAudioDecoded = false;
   LOGI("Init audio decoder success.");
}

void MediaDecode::OnInitVideo(VideoParam* videoParam) {
   if (mVideoDecoder) {
      LOGI("Delete last video decoder.");
      delete mVideoDecoder;
   }
   if(mPlayer->GetParam()->video_decoder_mode == 2){
      mVideoDecoder = new HWVideoDecoder(mPlayer);
   }else{
      mVideoDecoder = new H264Decoder(videoParam->avc_extra_data, videoParam->avc_extra_size);
   }
   if (!mVideoDecoder->Init(videoParam->width, videoParam->height)) {
      LOGE("Video decoder init ERROR");
      VHALL_DEL(mVideoDecoder);
   } else {
      LOGI("Video decoder init OK");
   }
   v_lock_mutex(&mBufferManagerMutex);
   mVideoStart = true;
   v_unlock_mutex(&mBufferManagerMutex);
   
   mFirstVideoDecoded = false;
   mLastDropFrameTimeStamp = 0;
   VHALL_DEL(videoParam->avc_extra_data);
   LOGI("Init video decoder success.");
}

void MediaDecode::OnDecodeAudio() {
   ASSERT(mAacDecoder != NULL);
   ASSERT(mEncodedAudioQueue != NULL);
   
   if (mAudioStart == false || mAacDecoder == NULL) {
      LOGE("aac decoder not initialize.");
      return;
   }
   int decoded_size = 0;
   int willDropCnt = 0;//BufferMonitor(audio_st);
   BufferMonitor();
   //get audio from queue
   DataUnit* newAACUnit = mEncodedAudioQueue->GetDataUnit(false);
   if (newAACUnit) {
      mLastAudioDecodeTimestamp = newAACUnit->timestap;
      if(mAudioStart == false){
         mEncodedAudioQueue->FreeDataUnit(newAACUnit);
         return;
      }
      int pcmSize = mAacDecoder->Decode(
                                        newAACUnit->unitBuffer,
                                        (int)newAACUnit->dataSize);
      if (pcmSize<= 0) {
         LOGE("AAC decode failed,timestamp=%llu", newAACUnit->timestap);
      } else {
         if(false == mFirstAudioDecoded){
            mFirstAudioDecoded = true;
            AudioParam audioParam = mAacDecoder->GetAudioParam();
            for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
               mMediaOutputNotify[i]->InitAudio(&audioParam);
            }
            mAudioFrameBufferSize = audioParam.bitsPerSample/8
            *audioParam.numOfChannels
            *audioParam.samplesPerSecond
            *audioParam.unitLengthInMs/1000;
            int maxQueueSize = CalcAudioBufferSize(&audioParam);
            mEncodedAudioQueue->SetQueueSize(maxQueueSize);
            LOGI("Reset audio Queue buffer size=%d, queue size=%d.",0, maxQueueSize);
            msleep(50);
         }
         //decoded_size = pcmSize;
#ifdef AUDIO_DEBUG
         LOGD("AAC decode success,timestamp=%llu will notify mediaoutput[size=%u]. decoded size=%d, buffered/free = %d/%d",
              newAACUnit->timestap,
              (unsigned int)mMediaOutputNotify.size(),
              pcmSize,
              mEncodedAudioQueue->GetDataUnitCnt(),
              mEncodedAudioQueue->GetFreeUnitCnt());
#endif
         decoded_size = PCM_UNIT_SIZE;
         int unitCnt = 0;
         while (mAudioStart == true && mAacDecoder->GetDecodecData(mAudioFrameBuffer, decoded_size)) {
            decoded_size = PCM_UNIT_SIZE;
            pcmSize -= decoded_size;
            uint64_t dataTime = unitCnt*decoded_size*1000.0/
            (mAacDecoder->GetAudioParam().numOfChannels
             *mAacDecoder->GetAudioParam().bitsPerSample/2
             *mAacDecoder->GetAudioParam().samplesPerSecond) ;
            
            if(willDropCnt > 0){
               LOGW("Will discard %d audio data", willDropCnt);
            }else{
               
               v_lock_mutex(&mBufferManagerMutex);
               if(mIsInBuffering == false && mIsInBufferingFlags == true){
                  mIsInBufferingFlags = false;
                  EventParam param;
                  param.mId = -1;
                  param.mDesc = "Stop buffer decode packet.";
                  mPlayer->NotifyEvent(STOP_BUFFERING, param);
                  LOGI("buffer fill, so stop buffer.....");            }
               v_unlock_mutex(&mBufferManagerMutex);
               
               for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
                  long pcmDataSize  = decoded_size;
                  DataUnit* newDataUnit = mMediaOutputNotify[i]->MallocDataUnit(STREAM_TYPE_AUDIO, pcmDataSize, willDropCnt > 0);
                  if (newDataUnit) {
                     memcpy(newDataUnit->unitBuffer, mAudioFrameBuffer, decoded_size);
                     newDataUnit->dataSize = decoded_size;
                     
                     newDataUnit->timestap = newAACUnit->timestap>dataTime? (newAACUnit->timestap + dataTime):0;
                     mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_AUDIO, newDataUnit);
#ifdef AUDIO_DEBUG
                     LOGD("Append pcm data success,timestamp=%llu, decoded size=%d",
                          newDataUnit->timestap, decoded_size);
#endif
                  }else{
                     LOGW("Media output can't malloc free data unit. will discard pcm data");
                  }
               }//for
            }
            unitCnt ++;
         } //end while
      }
      //newAACUnit->timestap = 0;
      mEncodedAudioQueue->FreeDataUnit(newAACUnit);
   }else{
      v_lock_mutex(&mBufferManagerMutex);
      if(mIsInBuffering == false){
         LOGI("audio buffer empty, so start buffer.....");
         mIsInBuffering = true;
         mBufferStartTime = mLastAudioDecodeTimestamp;
         if(mIsInBufferingFlags == false){
            EventParam param;
            param.mId = -1;
            param.mDesc = "audio buffer empty.";
            mPlayer->NotifyEvent(START_BUFFERING, param);
            mIsInBufferingFlags = true;
         }
      }
      v_unlock_mutex(&mBufferManagerMutex);
   }
   v_lock_mutex(&mBufferManagerMutex);
   if(mAudioStart == true
      &&(mIsInBuffering == false ||
         (mIsInBuffering == true && mLastAudioDecodeTimestamp < mBufferStartTime))){
         mAudioWorkerThread->Clear(this, MSG_AUDIO_DECODE);
         mAudioWorkerThread->Post(this, MSG_AUDIO_DECODE);
      }else{
         LOGI("Will exit audio loop.");
      }
   v_unlock_mutex(&mBufferManagerMutex);
}

void MediaDecode::OnDecodeVideo() {
   //ASSERT(mH264Decoder != NULL);
   ASSERT(mEncodedVideoQueue != NULL);
   bool ret = false;
   if (mVideoStart == false || mVideoDecoder == NULL) {
      LOGE("video decoder not initialize.");
      return;
   }
   int willDropCnt = 0;//BufferMonitor(video_st);
   BufferMonitor();
   int videoFrameSize = 0;
   DataUnit* newH264Unit = mEncodedVideoQueue->GetDataUnit(false);
   if (newH264Unit) {
      mLastVideoDecodeTimestamp = newH264Unit->timestap;
      if(mVideoStart == false){
         mEncodedVideoQueue->FreeDataUnit(newH264Unit);
         return;
      }
      //int64_t  startTime = VinnyLive::GetTimestampMs();
      ret = mVideoDecoder->Decode((const char*)newH264Unit->unitBuffer, (int)newH264Unit->dataSize, videoFrameSize, newH264Unit->timestap);
      
      //LOG_OPEN("time %llu ", (uint64_t)(VinnyLive::GetTimestampMs() - startTime));
      if (ret == false) {
         LOGE("H264Decoder decode error,timestamp=%llu. size=%llu.",
              newH264Unit->timestap, newH264Unit->dataSize);
      }else{
         LOGD("H264Decoder decode success,timestamp=%llu, size=%llu."
              " will notify mediaoutput[size=%u]. decoded size=%d, buffered/free=%d/%d",
              newH264Unit->timestap,
              newH264Unit->dataSize,
              (unsigned int)mMediaOutputNotify.size(),
              videoFrameSize,
              mEncodedVideoQueue->GetDataUnitCnt(),
              mEncodedVideoQueue->GetFreeUnitCnt());
         VideoParam videoParam;
         videoParam = mVideoDecoder->GetVideoParam();
         if(videoParam.width != mVideoParam.width
            || videoParam.height != mVideoParam.height){
            mFirstVideoDecoded = false;
            LOGI("MediaDecode::OnDecodeVideo video param changed %dx%d", videoParam.width, videoParam.height);
         }
         videoFrameSize = videoParam.width* videoParam.height*3/2;
         videoParam.framesPerSecond = mVideoParam.framesPerSecond;
         if(false == mFirstVideoDecoded && videoParam.width != 0 && videoParam.height != 0){
            mFirstVideoDecoded = true;
            for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
               mMediaOutputNotify[i]->InitVideo(&videoParam);
            }
            int maxQueueSize = CalcVideoBufferSize(&videoParam);
            mEncodedVideoQueue->SetQueueSize(maxQueueSize);
            LOGI("Reset video Queue buffer size=%d, queue size=%d.",0, maxQueueSize);
            mVideoParam = videoParam;
            msleep(50);
         }
         if(willDropCnt > 0){
            LOGI("will drop video %d frame.", willDropCnt);
         }else{
            v_lock_mutex(&mBufferManagerMutex);
            if(mIsInBuffering == false && mIsInBufferingFlags == true){
               mIsInBufferingFlags = false;
               EventParam param;
               param.mId = -1;
               param.mDesc = "Stop buffer decode packet.";
               mPlayer->NotifyEvent(STOP_BUFFERING, param);
               LOGI("buffer fill, so stop buffer.....");
            }
            v_unlock_mutex(&mBufferManagerMutex);
            //calculate time
            if(mMediaOutputNotify.size() > 0){
               ASSERT(mVideoFrameBufferSize>videoFrameSize);
               uint64_t timestap = 0;
               ret = mVideoDecoder->GetDecodecData(mVideoFrameBuffer, videoFrameSize, timestap);
               while (ret ) {
                  for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
                     DataUnit* newYUVPkt = mMediaOutputNotify[i]->MallocDataUnit(
                                                                                 STREAM_TYPE_VIDEO, videoFrameSize, willDropCnt > 0);
                     if(newYUVPkt){
                        memcpy(newYUVPkt->unitBuffer, mVideoFrameBuffer, videoFrameSize);
                        newYUVPkt->dataSize = videoFrameSize;
                        newYUVPkt->timestap = timestap; //!note, there are some frame in the decoder.
                        mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_VIDEO, newYUVPkt);
                     }else{
                        LOGW("Media output can't malloc free data unit. will discard yuv data");
                     }
                  }
                  ret = mVideoDecoder->GetDecodecData(mVideoFrameBuffer, videoFrameSize, timestap);
               }
            }
         }
      }//end if ret == false
      mEncodedVideoQueue->FreeDataUnit(newH264Unit);
      
   }else{//end if newH264Unit is null
      v_lock_mutex(&mBufferManagerMutex);
      if(mIsInBuffering == false){
         LOGI("video buffer empty, so start buffer.....");
         mIsInBuffering = true;
         mBufferStartTime = mLastVideoDecodeTimestamp;
         if(mIsInBufferingFlags == false){
            EventParam param;
            param.mId = -1;
            param.mDesc = "video buffer empty.";
            mPlayer->NotifyEvent(START_BUFFERING, param);
            mIsInBufferingFlags = true;
         }
      }
      v_unlock_mutex(&mBufferManagerMutex);
   }
   v_lock_mutex(&mBufferManagerMutex);
   if(mVideoStart == true &&
      (mIsInBuffering == false ||
       (mIsInBuffering == true && mLastVideoDecodeTimestamp < mBufferStartTime))  ){
         mVideoWorkerThread->Clear(this, MSG_VIDEO_DECODE);
         mVideoWorkerThread->Post(this, MSG_VIDEO_DECODE, 0);
      }else{
         LOGI("Will exit video loop.");
      }
   v_unlock_mutex(&mBufferManagerMutex);
}

void MediaDecode::OnAudioDestory(){
   LOGI("Audio decode destory.");
   mAudioWorkerThread->Clear(this, MSG_AUDIO_DECODE);
   if(mEncodedAudioQueue){
      mEncodedAudioQueue->Flush();
   }
   VHALL_DEL(mAacDecoder);
   //freep(mEncodedAudioQueue);
   mFirstAudioDecoded = false;
}
void MediaDecode::OnVideoDestory(){
   LOGI("video decode destory.");
   mVideoWorkerThread->Clear(this, MSG_VIDEO_DECODE);
   if(mEncodedVideoQueue){
      mEncodedVideoQueue->Flush();
   }
   VHALL_DEL(mVideoDecoder);
   mFirstVideoDecoded = false;
   //freep(mEncodedVideoQueue);
}

void MediaDecode::ProcessBuffer(){
   //buffer>INIT_PLAY_BUFFER_TIME
   //第一次或正在buffer时
   //空闲buffer<5
   v_lock_mutex(&mBufferManagerMutex);
   if(mIsInBuffering == true){
      uint64_t bufferTime = -1;
      if (mHasVideo) {
         bufferTime = mEncodedVideoQueue->GetTailTimestamp() - mEncodedVideoQueue->GetHeadTimestamp();
      }
      
      if (mHasAudio) {
         bufferTime = MIN(mEncodedAudioQueue->GetTailTimestamp() - mEncodedAudioQueue->GetHeadTimestamp(), bufferTime);
      }
      
      if (mHasAudio == true && mAudioStart == true){
         int audioFreeCnt = mEncodedAudioQueue->GetQueueSize() - mEncodedAudioQueue->GetDataUnitCnt();
         if ((mFirstAudioDecoded == false && bufferTime >= INIT_PLAY_BUFFER_TIME)
             || bufferTime >= mMaxBufferTimeInMs
             || audioFreeCnt <= 5) {
            mIsInBuffering = false;
            mBufferStartTime = 0;
            
            LOGD("audio MediaDecode::ProcessBuffer, bufferTime=%llu,"
                 " freeCnt=%d, buffered/free=%d/%d",
                 bufferTime,
                 audioFreeCnt,
                 mEncodedAudioQueue->GetDataUnitCnt(),
                 mEncodedAudioQueue->GetFreeUnitCnt());
         }
      }
      
      if ( mHasVideo == true && mVideoStart == true){
         int videoFreeCnt = mEncodedVideoQueue->GetQueueSize() - mEncodedVideoQueue->GetDataUnitCnt();
         if ((mFirstVideoDecoded == false && bufferTime >= INIT_PLAY_BUFFER_TIME)
             || bufferTime >= mMaxBufferTimeInMs
             || videoFreeCnt <= 0){
            mIsInBuffering = false;
            mBufferStartTime = 0;
            LOGD("video MediaDecode::ProcessBuffer, bufferTime=%llu,"
                 " freeCnt=%d, buffered/free=%d/%d",
                 bufferTime,
                 videoFreeCnt,
                 mEncodedVideoQueue->GetDataUnitCnt(),
                 mEncodedVideoQueue->GetFreeUnitCnt());
         }
      }
   }
   
   if(mIsInBuffering == false){
      if(mHasAudio == true && mAudioStart == true)
         mAudioWorkerThread->Post(this, MSG_AUDIO_DECODE);
      if(mHasVideo == true && mVideoStart == true)
         mVideoWorkerThread->Post(this, MSG_VIDEO_DECODE);
   }
   v_unlock_mutex(&mBufferManagerMutex);
}

void MediaDecode::BufferMonitor(){
   uint64_t audioDeltaTime = 0;
   int audioFreeCnt = 0;
   bool isAudioNeedDiscard = false;
   
   uint64_t videoDeltaTime = 0;
   int videoFreeCnt = 0;
   bool isVideoNeedDiscard = false;
   uint64_t keyFrameTimestap = 0;
   
   if(mHasAudio){
      audioDeltaTime = mEncodedAudioQueue->GetTailTimestamp() - mEncodedAudioQueue->GetHeadTimestamp();
      audioFreeCnt = mEncodedAudioQueue->GetQueueSize() > mEncodedAudioQueue->GetDataUnitCnt()? mEncodedAudioQueue->GetQueueSize() - mEncodedAudioQueue->GetDataUnitCnt() : 0 ;
      
      if(audioFreeCnt < mEncodedAudioQueue->GetQueueSize()/STRICT_BUFFER_RATIO
         || audioDeltaTime > mMaxBufferTimeInMs*PLAY_BUFFER_RATIO*(STRICT_BUFFER_RATIO - 1)/STRICT_BUFFER_RATIO){
         isAudioNeedDiscard = true;
      }
      if(mHasVideo == false && isAudioNeedDiscard == true && mLiveModel != LIVE_TYPE_AUDIO_ONLY){
         //discard audio
         keyFrameTimestap = mEncodedAudioQueue->GetHeadTimestamp() + audioDeltaTime -  mMaxBufferTimeInMs;
         DropDataUnits(mEncodedAudioQueue, keyFrameTimestap);
         LOGW("MediaDecode::BufferMonitor drop audio(only)");
      }
   }
   if(mHasVideo){
      keyFrameTimestap = mEncodedVideoQueue->GetKeyUnitTimestap();
      videoDeltaTime = mEncodedVideoQueue->GetTailTimestamp() - mEncodedVideoQueue->GetHeadTimestamp();
      videoFreeCnt = mEncodedVideoQueue->GetQueueSize() > mEncodedVideoQueue->GetDataUnitCnt()? mEncodedVideoQueue->GetQueueSize() - mEncodedVideoQueue->GetDataUnitCnt() : 0 ;
      if(videoFreeCnt < mEncodedVideoQueue->GetQueueSize()/STRICT_BUFFER_RATIO
         ||videoDeltaTime > mMaxBufferTimeInMs*PLAY_BUFFER_RATIO*(STRICT_BUFFER_RATIO - 1)/STRICT_BUFFER_RATIO){
         isVideoNeedDiscard = true;
      }
      if((mHasAudio == false && isVideoNeedDiscard == true) && mLiveModel != LIVE_TYPE_VIDEO_ONLY){
         //discard video
         DropDataUnits(mEncodedVideoQueue, keyFrameTimestap);
         LOGW("MediaDecode::BufferMonitor drop video(only)");
      }
   }
   
   mPlayer->SetPlayerBufferTime(MAX(videoDeltaTime,audioDeltaTime));
   
   if(mHasVideo && mHasAudio){
      //discard video
      if((isVideoNeedDiscard || isAudioNeedDiscard) && keyFrameTimestap != 0){
         audioDeltaTime = keyFrameTimestap-mEncodedAudioQueue->GetHeadTimestamp();
         videoDeltaTime = keyFrameTimestap-mEncodedVideoQueue->GetHeadTimestamp();
         uint64_t deltaTime = MIN(audioDeltaTime, videoDeltaTime);
         if (deltaTime<=mMaxBufferTimeInMs*1.0/3) {
            int videoDropCut = DropDataUnits(mEncodedVideoQueue, keyFrameTimestap);
            int audioDropCut = DropDataUnits(mEncodedAudioQueue, keyFrameTimestap);
            LOGW("MediaDecode::BufferMonitor drop video count:%d and audio count:%d deltaTime:%llu",videoDropCut,audioDropCut,deltaTime);
         }
      }else{
         uint64_t deltaTime = 0;
         if( audioDeltaTime > videoDeltaTime ){
            deltaTime = audioDeltaTime - videoDeltaTime;
            if(deltaTime > mMaxBufferTimeInMs){
               LOGW("MediaDecode::BufferMonitor a/v buffer time is wrong. (more audio) ");
            }
         }else{
            deltaTime = videoDeltaTime - audioDeltaTime;
            if(deltaTime > mMaxBufferTimeInMs){
               LOGW("MediaDecode::BufferMonitor a/v buffer time is wrong. (more video) ");
            }
         }
      }
   }
}

//int MediaDecode::BufferMonitor(const STREAM_TYPE& streamType){
//   BufferQueue* bufferQueue = NULL;
//   uint64_t dropCnt = 0;
//   if (STREAM_TYPE_VIDEO == streamType) {
//      bufferQueue = mEncodedVideoQueue;
//   } else if (STREAM_TYPE_AUDIO == streamType) {
//      bufferQueue = mEncodedAudioQueue;
//   }
//   if(bufferQueue){
//      uint64_t deltaTime = bufferQueue->GetTailTimestamp() - bufferQueue->GetHeadTimestamp();
//      int freeCnt = bufferQueue->GetQueueSize() > bufferQueue->GetDataUnitCnt()? bufferQueue->GetQueueSize() - bufferQueue->GetDataUnitCnt() : 0 ;
//
//      /*LOGD("%s MediaDecode::BufferMonitor  success, head timestamp=%llu, tail timestamp=%llu."
//       " freeCnt=%d, buffered/free=%d/%d",
//       video_st == streamType? "video":"audio",
//       bufferQueue->GetHeadTimestap(),
//       bufferQueue->GetTailTimestap(),
//       freeCnt,
//       bufferQueue->GetDataUnitCnt(),
//       bufferQueue->GetFreeUnitCnt());
//       */
//      int dataCnt = bufferQueue->GetDataUnitCnt();
//      if( freeCnt < bufferQueue->GetQueueSize()/4){
//         dropCnt = bufferQueue->GetQueueSize()/2 - freeCnt;
//         LOGW("Drop %d %s frame for strict buffer size.", (int)dropCnt, STREAM_TYPE_AUDIO == streamType? "audio":"video" );
//      }else if(deltaTime > mMaxBufferTimeInMs*2 ){
//         dropCnt = bufferQueue->GetDataUnitCnt()*(deltaTime - mMaxBufferTimeInMs)/mMaxBufferTimeInMs;
//         dropCnt = MAX(dropCnt, 1);
//         LOGW("Drop %d %s frame for strict buffer time. buffered/free=%d/%d",
//              (int)dropCnt, STREAM_TYPE_AUDIO == streamType? "audio":"video", dataCnt, freeCnt);
//      }
//   }
//   return (int )dropCnt;
//}

int MediaDecode::CalcAudioBufferSize(AudioParam* audioParam){
   int bufferSize = PCM_UNIT_SIZE;
   int maxQueueSize = audioParam->numOfChannels*audioParam->bitsPerSample
   *audioParam->samplesPerSecond / 8 / bufferSize;
   maxQueueSize = maxQueueSize*PLAY_BUFFER_RATIO* mMaxBufferTimeInMs/1000;
   maxQueueSize = MAX(maxQueueSize, MIN_AUDIO_BUFFER_SIZE);
   LOGI("Audio Decode Queue Size:%d ch:%d samplesPerSecond:%d",maxQueueSize,audioParam->numOfChannels,audioParam->samplesPerSecond);
   return (int)maxQueueSize;
}

int MediaDecode::CalcVideoBufferSize(VideoParam* videoParam){
   int maxQueueSize = mMaxBufferTimeInMs*PLAY_BUFFER_RATIO / 1000 * videoParam->framesPerSecond;
   maxQueueSize = MAX(maxQueueSize, MIN_VIDEO_BUFFER_SIZE);
   LOGI("Video Decode Queue Size:%d Buffer Times:%d fps=%d",maxQueueSize,
        mMaxBufferTimeInMs, videoParam->framesPerSecond);
   return (int)maxQueueSize;
}

int MediaDecode::DropDataUnits(BufferQueue* bufferQueue, const uint64_t& timestap){
   LOGW("MediaDecode::DropDataUnits will drop dataunit until %llu", timestap);
   int dropCut = 0;
   while (bufferQueue->GetHeadTimestamp() < timestap  ) {
      DataUnit*dataUnit = bufferQueue->GetDataUnit(false);
      if(dataUnit){
         bufferQueue->FreeDataUnit(dataUnit);
      }else{
         LOGE("MediaDecode::DropDataUnits unknow exception,  drop  not enough dataunit ");
         break;
      }
      dropCut++;
   }
   return dropCut;
}
