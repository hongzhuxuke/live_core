#include "media_render.h"
#include "../api/live_interface.h"
#include "../common/vhall_log.h"
#include "../common/live_message.h"
#include "../os/vhall_monitor_log.h"
#include "../utility/utility.h"
#include "../3rdparty/json/json.h"
#include "vhall_live_player.h"
#include "talk/base/thread.h"

#define  AMF_MAG_QUEUE_MAX_COUNT       500
#define  AMF_MAG_BUFFER_MAX_SIZE       40 //byte

MediaRender::MediaRender(VhallPlayerInterface*vinnyLive,
                         uint64_t maxBufferTimeInMs) :
mAudioRenderThread(NULL),
mVideoRenderThread(NULL),
mAmfMsgRenderThread(NULL),
mAudioRawDataQueue(0),
mVideoRawDataQueue(0),
mAmfMsgQueue(NULL),
mStoping(false),
mAudioAlreadInit(false),
mVideoAlreadInit(false),
mAudioMediaRenderTime(0),
mVideoMediaRenderTime(0),
//mSleepTimeInMs(1),
mVideoFrameDurationInMs(0),
mAudioPlayDurationInMs(0),
mAudioUnitTimeInMs(0),
mVideoUnitTimeInMs(0),
mVinnyLive(vinnyLive)
{
   mMaxBufferTimeInMs = maxBufferTimeInMs;
   mVideoRenderThread = new talk_base::Thread();
   if (mVideoRenderThread == NULL) {
      LOGE("mVideoRenderThread is NULL");
   }
   mVideoRenderThread->Start();
   
   mAudioRenderThread = new(std::nothrow) talk_base::Thread();
   if (mAudioRenderThread == NULL) {
      LOGE("mAudioRenderThread is NULL");
   }
   mAudioRenderThread->Start();
   
   mAmfMsgRenderThread = new(std::nothrow) talk_base::Thread();
   if (mAmfMsgRenderThread == NULL) {
      LOGE("mAmfMsgRenderThread is NULL");
   }
   mAmfMsgRenderThread->Start();
   
   mAmfMsgQueue = new(std::nothrow) BufferQueue(AMF_MAG_BUFFER_MAX_SIZE,AMF_MAG_QUEUE_MAX_COUNT);
   if (mAmfMsgQueue==NULL) {
      LOGE("mAmfMsgQueue is NULL");
   }
}

MediaRender::~MediaRender() {
   if(mStoping == false)
      Destory();
   
   VHALL_THREAD_DEL(mVideoRenderThread);
   VHALL_THREAD_DEL(mAudioRenderThread);
   VHALL_THREAD_DEL(mAmfMsgRenderThread);
   
   VHALL_DEL(mAudioRawDataQueue);
   VHALL_DEL(mVideoRawDataQueue);
   VHALL_DEL(mAmfMsgQueue);
}

bool MediaRender::InitLiveModel(LiveModel liveModel){
   mLiveModel = liveModel;
   return true;
}

bool MediaRender::InitAudio(AudioParam* audioParam) {
   LOGI("Init audio render, will post init audio message.");
   long bufferSize = PCM_UNIT_SIZE;
   long maxQueueSize =    mMaxBufferTimeInMs  * audioParam->numOfChannels*audioParam->bitsPerSample
   *audioParam->samplesPerSecond / 8 / bufferSize / 1000 ;
   
   maxQueueSize = MAX(maxQueueSize, MIN_AUDIO_RENDER_BUFFER_SIZE);
   if(mAudioRawDataQueue == NULL)
      mAudioRawDataQueue = new BufferQueue(bufferSize, (int)maxQueueSize);
   if(mAudioRawDataQueue == NULL){
      LOGE("malloc raw  audio queue failed");
      return false;
   }
   mAudioRawDataQueue->SetQueueSize((int)maxQueueSize);
   LOGD("%ld MediaRender::InitAudio", (long)mAudioRawDataQueue );
   
   mAudioAlreadInit = false;
   mStoping = false;
   mAudioRawDataQueue->Flush();
   msleep(1);
   mAudioRawDataQueue->Reset();
   LOGD("MediaRender::init %s debug   buffered(device)/free=%d/%d",
        "audio",
        mAudioRawDataQueue->GetDataUnitCnt(),
        mAudioRawDataQueue->GetFreeUnitCnt());
   mAudioUnitTimeInMs = PCM_UNIT_SIZE*1000/(audioParam->numOfChannels*audioParam->bitsPerSample/2*audioParam->samplesPerSecond);
   mAudioUnitTimeInMs = MIN(5, mAudioUnitTimeInMs);
   if(!mAudioRenderThread->started()){
      mAudioRenderThread->Start();
   }
   mAudioRenderThread->Restart();
   mAudioRenderThread->Post(this, MSG_AUDIO_MediaRender_INIT, new AudioParamMessageData(audioParam));
   return true;
}

bool MediaRender::InitVideo(VideoParam* videoParam) {
   LOGI("Init video render");
   long bufferSize = videoParam->width* videoParam->height * 3;
   long maxQueueSize = (long)(videoParam->framesPerSecond* mMaxBufferTimeInMs / 1000);
   
   maxQueueSize = MAX(maxQueueSize, MIN_VIDEO_RENDER_BUFFER_SIZE);
   if(mVideoRawDataQueue == NULL)
      mVideoRawDataQueue = new BufferQueue(bufferSize, (int)maxQueueSize);
   if(mVideoRawDataQueue == NULL){
      LOGE("malloc raw  video queue failed");
      return false;
   }
   mVideoRawDataQueue->SetQueueSize((int)maxQueueSize);
   if( mVideoAlreadInit == true){
      mVideoAlreadInit = false;
      mVideoRenderThread->Clear(this, MSG_VIDEO_MediaRender_INIT);
      mVideoRenderThread->Clear(this, MSG_VIDEO_MediaRender);
      mVideoRawDataQueue->Flush();
      msleep(1);
   }
   mVideoRawDataQueue->Reset();
   mVideoAlreadInit = false;
   mStoping = false;
   mVideoUnitTimeInMs = 50;
   if(videoParam->framesPerSecond > 0 && videoParam->framesPerSecond < 60)
      mVideoUnitTimeInMs = 1000/videoParam->framesPerSecond;
   
   LOGI("Init video render, reset video queue.");
   if(!mVideoRenderThread->started()){
      mVideoRenderThread->Start();
   }
   mVideoRenderThread->Restart();
   mVideoRenderThread->Post(this, MSG_VIDEO_MediaRender_INIT, new VideoParamMessageData(videoParam));
   return true;
}

void MediaRender::Destory(){
   LOGI("Destory render, will clear message.");
   if(mStoping == false){
      mStoping = true;
      mVideoRenderThread->Clear(this, MSG_VIDEO_MediaRender_INIT);
      mVideoRenderThread->Clear(this, MSG_VIDEO_MediaRender);
      mVideoRenderThread->Post(this, MSG_VIDEO_MediaRender_DESTORY);
      
      mAudioRenderThread->Clear(this, MSG_AUDIO_MediaRender_INIT);
      mAudioRenderThread->Clear(this, MSG_AUDIO_MediaRender);
      mAudioRenderThread->Clear(this, MSG_BUFFERR_NOTIFY);
      mAudioRenderThread->Post(this, MSG_AUDIO_MediaRender_DESTORY);
      
      if(mVideoRawDataQueue)
         mVideoRawDataQueue->Flush();
      if(mAudioRawDataQueue)
         mAudioRawDataQueue->Flush();
      if (mAmfMsgQueue) {
         mAmfMsgQueue->SetPerItemTimestamp(0);
      }
      //mVideoRenderThread->Stop();
      //mAudioRenderThread->Stop();
      mAudioMediaRenderTime = 0;
      mVideoMediaRenderTime = 0;
   }
}

DataUnit* MediaRender::MallocDataUnit(const STREAM_TYPE& streamType, const long& bufferSize, const int& dropCnt) {
   DataUnit* newPkt = NULL;
   if(mStoping == false){
      BufferQueue* bufferQueue = NULL;
      if (STREAM_TYPE_VIDEO == streamType) {
         ASSERT(mVideoRawDataQueue != NULL);
         if (mVideoAlreadInit){
            bufferQueue = mVideoRawDataQueue;
         }
      } else if (STREAM_TYPE_AUDIO == streamType) {
         ASSERT(mAudioRawDataQueue != NULL);
         if (mAudioAlreadInit){
            bufferQueue = mAudioRawDataQueue;
         }
      }else if(STREAM_TYPE_ONCUEPONIT_MSG == streamType){
         ASSERT(mAmfMsgQueue != NULL);
         bufferQueue = mAmfMsgQueue;
         LOGD("amf msg play, buffered max/cur/free=%d/%d/%d",
              mAmfMsgQueue->GetQueueSize(),
              mAmfMsgQueue->GetDataUnitCnt(),
              mAmfMsgQueue->GetFreeUnitCnt());
      }
      if(bufferQueue){
         int actualDropCnt = dropCnt;
         while (actualDropCnt > 0  ) {
            newPkt = bufferQueue->GetDataUnit(false);
            if(newPkt)
               bufferQueue->PutDataUnit(newPkt);
            else
               break;
            actualDropCnt--;
         }
         if(STREAM_TYPE_ONCUEPONIT_MSG == streamType){
            newPkt = bufferQueue->MallocDataUnit(bufferSize, false);
         }else{
            newPkt = bufferQueue->MallocDataUnit(bufferSize, true);
         }
#ifdef AUDIO_DEBUG
         if(STREAM_TYPE_AUDIO == streamType)
#else
         if(STREAM_TYPE_VIDEO == streamType)
#endif
      LOGD("MediaRender::MallocDataUnit %s play,  buffered(device)/free=%d/%d",
                    STREAM_TYPE_AUDIO == streamType?"audio":"video",
                    mAudioRawDataQueue->GetDataUnitCnt(),
                    mAudioRawDataQueue->GetFreeUnitCnt());
         
      }
   }
   return newPkt;
}

bool MediaRender::AppendStreamPacket(const STREAM_TYPE& streamType, DataUnit* dataUnit) {
   if (STREAM_TYPE_VIDEO == streamType) {
      if (mVideoAlreadInit)
         mVideoRawDataQueue->PutDataUnit(dataUnit);
      if(dataUnit->timestap < mVideoMediaRenderTime || mVideoMediaRenderTime == 0){
         mVideoMediaRenderTime = dataUnit->timestap;
      }
   } else if (STREAM_TYPE_AUDIO == streamType) {
      if (mAudioAlreadInit)
         mAudioRawDataQueue->PutDataUnit(dataUnit);
   } else if (STREAM_TYPE_ONCUEPONIT_MSG == streamType){
      mAmfMsgQueue->PutDataUnit(dataUnit);
   }
#ifdef AUDIO_DEBUG
   if(STREAM_TYPE_AUDIO == streamType)
#else
      if(video_st == streamType)
#endif
      LOGD("MediaRender::AppendStreamPacket %s   %llu",
              STREAM_TYPE_VIDEO == streamType?"video":"audio", dataUnit->timestap);
   
   return true;
}

void MediaRender::OnMessage(talk_base::Message* msg) {
   switch (msg->message_id) {
      case MSG_AUDIO_MediaRender_INIT:{
         AudioParamMessageData * apmd = static_cast<AudioParamMessageData*>(msg->pdata);
         OnInitAudio(&apmd->mParam);
      }
         break;
      case MSG_VIDEO_MediaRender_INIT:{
         VideoParamMessageData * vpmd = static_cast<VideoParamMessageData*>(msg->pdata);
         OnInitVideo(&vpmd->mParam);
      }
         break;
      case MSG_AUDIO_MediaRender:
         if(mStoping == false)
            AudioRenderLoop();
         break;
      case MSG_VIDEO_MediaRender:
         if(mStoping == false)
            VideoRenderLoop();
         break;
      case MSG_AMF_MSG_MediaRender:
         if(mStoping == false)
            AmfMsgRenderLoop();
         break;
      case MSG_AUDIO_MediaRender_DESTORY:
         mVinnyLive->NotifyJNIDetachAudioThread();
         mAudioAlreadInit = false;
         break;
      case MSG_VIDEO_MediaRender_DESTORY:
         mVinnyLive->NotifyJNIDetachVideoThread();
         mVideoAlreadInit = false;
         break;
      case MSG_BUFFERR_NOTIFY:
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void MediaRender::OnInitAudio(AudioParam* audioParam) {
   VHJson::FastWriter root;
   VHJson::Value item;
   item["numOfChannels"] = VHJson::Value(audioParam->numOfChannels);
   item["samplesPerSecond"] = VHJson::Value(audioParam->samplesPerSecond);
   item["bitsPerSample"] = VHJson::Value(audioParam->bitsPerSample);
   EventParam param;
   param.mId = -1;
   param.mDesc = root.write(item);
   mVinnyLive->NotifyEvent(INFO_DECODED_AUDIO, param);
   mAudioAlreadInit = true;
   mAudioParam = *audioParam;
   mAudioPlayDurationInMs = 0;
   LOGI("Init audio play success, will post play audio message.");
   mAudioRenderThread->PostDelayed(RENDER_BUFFER_TIME, this, MSG_AUDIO_MediaRender, 0);
   //mWorkerThread->PostDelayed(INIT_PLAY_BUFFER_TIME, this, MSG_BUFFERR_NOTIFY, 0);
}

void MediaRender::OnInitVideo(VideoParam* videoParam) {
   if(mVideoAlreadInit == false
      || videoParam->width != mVideoParam.width
      || videoParam->height != mVideoParam.height){
      VHJson::FastWriter root;
      VHJson::Value item;
      item["width"] = VHJson::Value(videoParam->width);
      item["height"] = VHJson::Value(videoParam->height);
      EventParam param;
      param.mId = -1;
      param.mDesc = root.write(item);
      mVinnyLive->NotifyEvent(INFO_DECODED_VIDEO, param);
      mVideoAlreadInit = true;
      mVideoParam = *videoParam;
      mVideoFrameSize = mVideoParam.width* mVideoParam.height*3/2;
      mVideoFrameDurationInMs = 0;
      LOGI("Init video render success, will post render video message.");
      mVideoRenderThread->PostDelayed(RENDER_BUFFER_TIME, this, MSG_VIDEO_MediaRender, 0);
      mVideoNextFrameRenderTimestamp = Utility::GetTimestampMs();
   }
}

void MediaRender::AudioRenderLoop() {
   int sleepTimeInMs = (int)mAudioUnitTimeInMs/2;
   if (mAudioAlreadInit) {
      uint64_t audioBufferTime = Utility::GetTimestampMs() - mLastAudioMediaRenderStartTime;
      //当只有audio或audio播放时间小于或等于video渲染时间时，才播放audio
      if((audioBufferTime + RENDER_BUFFER_TIME) > mAudioPlayDurationInMs){
         //play audio
         sleepTimeInMs = MediaRenderAudio();
      }
   }
   if(mAudioAlreadInit && mStoping == false){
      //LOGD("MediaRender::AudioRenderLoop, will delay %d ms play audio.", sleepTimeInMs);
      mAudioRenderThread->PostDelayed(sleepTimeInMs, this, MSG_AUDIO_MediaRender, 0);
   }
   if (mAmfMsgQueue->GetDataUnitCnt()>0&&mAudioMediaRenderTime>mAmfMsgQueue->GetHeadTimestamp()) {
      mAmfMsgRenderThread->Post(this,MSG_AMF_MSG_MediaRender);
   }
}

void MediaRender::AmfMsgRenderLoop(){
   while (true) {
      if (mAmfMsgQueue->GetDataUnitCnt()>0&&mAudioMediaRenderTime>mAmfMsgQueue->GetHeadTimestamp()) {
         DataUnit* amfUnit = mAmfMsgQueue->GetDataUnit(false);
         if (amfUnit) {
            EventParam event;
            event.mId = 0;
            std::string s;
            s.append((char*)amfUnit->unitBuffer, (int)(amfUnit->dataSize));
            event.mDesc = s;
            mVinnyLive->NotifyEvent(ONCUEPOINT_AMF_MSG, event);
            mAmfMsgQueue->FreeDataUnit(amfUnit);
         }
         LOGI("amf mag data queue count:%d free queue count:%d",mAmfMsgQueue->GetDataUnitCnt(),mAmfMsgQueue->GetFreeUnitCnt());
      }else{
         break;
      }
   }
}

void MediaRender::VideoRenderLoop() {
   int sleepTimeInMs = (int)mVideoUnitTimeInMs;
   if(mVideoAlreadInit){
      bool needRendVideo = false;
      //int64_t deltaTime = 0;
      //当只有video或audio播放时间大于video渲染时间时，才渲染video
      if(false == mAudioAlreadInit){
         if( Utility::GetTimestampMs() >= mVideoNextFrameRenderTimestamp ){
            needRendVideo = true;
         }else{
            sleepTimeInMs = (int)(mVideoNextFrameRenderTimestamp - Utility::GetTimestampMs());
         }
      }else{
         uint64_t audioBufferTime = Utility::GetTimestampMs() - mLastAudioMediaRenderStartTime;
         uint64_t audioRenderTime = mAudioMediaRenderTime + audioBufferTime;
         
         if(audioRenderTime >= ( mVideoMediaRenderTime )){
            needRendVideo = true;
            if((audioRenderTime - mVideoMediaRenderTime) > RENDER_BUFFER_TIME ){
               sleepTimeInMs = 0;
            }
         }
      }
      
      if(true == needRendVideo){
         mVideoNextFrameRenderTimestamp = Utility::GetTimestampMs();
         MediaRenderVideo();
         mVideoNextFrameRenderTimestamp = mVideoNextFrameRenderTimestamp + mVideoFrameDurationInMs;
         
         if(sleepTimeInMs != 0 ){
            uint64_t nowInms = Utility::GetTimestampMs();
            sleepTimeInMs = mVideoNextFrameRenderTimestamp > nowInms?(int)(mVideoNextFrameRenderTimestamp - nowInms):0 ;
         }
      }
   }
   
   if(mVideoAlreadInit && mStoping == false){
      LOGD("MediaRender::VideoRenderLoop, will delay %d ms render video.", sleepTimeInMs);
      if(sleepTimeInMs > mVideoUnitTimeInMs){
         sleepTimeInMs = MIN(sleepTimeInMs, (int)mVideoUnitTimeInMs);
      }
      mVideoRenderThread->PostDelayed(sleepTimeInMs, this, MSG_VIDEO_MediaRender);
   }
}

int MediaRender::MediaRenderAudio(){
   uint64_t sleepTimeInMs = mAudioUnitTimeInMs/2;
   if (mAudioAlreadInit ) {
      uint64_t audioBufferTime = Utility::GetTimestampMs() - mLastAudioMediaRenderStartTime;
      if( (audioBufferTime + RENDER_BUFFER_TIME) < mAudioPlayDurationInMs){
         sleepTimeInMs = mAudioPlayDurationInMs - audioBufferTime;
         LOGD("MediaRender::MediaRenderAudio, left buffer  %llu.", audioBufferTime);
      }else{
         int playBufferCnt = 0;
         uint64_t unitTimeInMs = 0;
         DataUnit* nextAudioUnit = mAudioRawDataQueue->GetDataUnit(mVideoAlreadInit == false ); //if only audio will block
         if ( nextAudioUnit) {
            if(mStoping == true){
               mAudioRawDataQueue->FreeDataUnit(nextAudioUnit);
               return 0;
            }
            //put audio to play list
            playBufferCnt = mVinnyLive->NotifyAudioData((const char * )nextAudioUnit->unitBuffer, nextAudioUnit->dataSize) -1;
            
            playBufferCnt = MAX(0, playBufferCnt);
            mLastAudioMediaRenderStartTime = Utility::GetTimestampMs();
            
            mAudioPlayDurationInMs = nextAudioUnit->dataSize*playBufferCnt*1000/
            (mAudioParam.numOfChannels*mAudioParam.bitsPerSample/2*mAudioParam.samplesPerSecond);
            
            unitTimeInMs = nextAudioUnit->dataSize*1000/
            (mAudioParam.numOfChannels*mAudioParam.bitsPerSample/2*mAudioParam.samplesPerSecond);
            audioBufferTime = 0;
#ifdef AUDIO_DEBUG
            LOGD("MediaRender::MediaRenderAudio play audio, timestamp %llu. buffered(device)/free=%d(%d)/%d",
                 nextAudioUnit->timestap,
                 mAudioRawDataQueue->GetDataUnitCnt(),
                 playBufferCnt,
                 mAudioRawDataQueue->GetFreeUnitCnt());
#endif
            //ASSERT(mNextAudioUnit->timestap > mAudioPlayBufferInMs);
            if(nextAudioUnit->timestap < mAudioPlayDurationInMs){
               mAudioMediaRenderTime = 1;
            }else{
               mAudioMediaRenderTime = nextAudioUnit->timestap - mAudioPlayDurationInMs;
            }
            mAudioRawDataQueue->FreeDataUnit(nextAudioUnit);
            if(mAudioPlayDurationInMs> unitTimeInMs)
               mAudioPlayDurationInMs -=unitTimeInMs;
            sleepTimeInMs = mAudioPlayDurationInMs/2;
         }else{ // for none audio unit
            //
            //mAudioMediaRenderTime = 0;
            //mSleepTimeInMs = 0;
            mAudioPlayDurationInMs =1;
         }
      }
      
      if(mVideoAlreadInit){
         //有video,优先fill device buffer
         //mSleepTimeInMs = MIN(mAudioPlayDurationInMs - audioBufferTime, mVideoFrameDurationInMs);
      }
   }
   return (int)sleepTimeInMs;
}

int MediaRender::MediaRenderVideo() {
   uint64_t sleepTimeInMs = mVideoUnitTimeInMs/2;
   if (mVideoAlreadInit) {
      DataUnit *videoUnit = mVideoRawDataQueue->GetDataUnit(true);
      if(videoUnit){
         if(mStoping == true){
            mVideoRawDataQueue->FreeDataUnit(videoUnit);
            return 0;
         }
         //MediaRender video
         if(mVideoFrameSize == videoUnit->dataSize ){
            mVinnyLive->NotifyVideoData((const char*)videoUnit->unitBuffer, (int)videoUnit->dataSize, mVideoParam.width, mVideoParam.height);
         }else{
            LOGW("MediaRenderVideo data size is invalid, so ingnore it. %llu  %llu.",
                 mVideoFrameSize, videoUnit->dataSize);
         }
         
         mVideoFrameDurationInMs = videoUnit->timestap - mVideoMediaRenderTime;
         LOGD("%llu MediaRender::MediaRenderVideo render video timestamp %llu. FrameDurationInMs %llu. buffered/free=%d/%d",
              Utility::GetTimestampMs(),
              videoUnit->timestap,
              mVideoFrameDurationInMs,
              mVideoRawDataQueue->GetDataUnitCnt(),
              mVideoRawDataQueue->GetFreeUnitCnt());
         if (mVideoFrameDurationInMs > 1000) {
            LOGW("mVideoFrameDurationInMs is exception value:%llu", mVideoFrameDurationInMs);
            mVideoFrameDurationInMs = 100;
         }
         mVideoMediaRenderTime = videoUnit->timestap;
         mVideoRawDataQueue->FreeDataUnit(videoUnit);
      }else{ //end videoUnit
         //mSleepTimeInMs = 0;
         sleepTimeInMs = 5;
      }
   } else {
      LOGW("video is not init render, but call render video. so bad");
   }
   return (int)sleepTimeInMs;
}
