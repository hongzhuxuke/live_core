#include "media_muxer.h"
#include "../utility/utility.h"
#include "../common/vhall_log.h"
#include "../common/live_message.h"
#include "../common/live_status_listener.h"
#include "muxer_interface.h"
#include "../3rdparty/json/json.h"
#include "srs_rtmp_publisher.h"
#include "srs_flv_recorder.h"
#include "srs_http_flv_streamer.h"
#include "talk/base/stringdigest.h"
#include "../ratecontrol/rate_control.h"
#include "../common/auto_lock.h"

#define QUEUE_TIME_SIZE    5 //2s

NS_VH_BEGIN

MediaMuxer::MediaMuxer()
:mVideoQueue(NULL),
mAudioQueue(NULL),
mBufferItem(NULL),
mAudioItem(NULL),
mVideoItem(NULL),
mDataPool(NULL),
mParam(NULL),
mStatueListener(NULL),
mAudioHeader(NULL),
mVideoHeader(NULL),
mAmf0MsgQueue(NULL),
mRateControl(NULL){
   mIsStart = false;
   vhall_lock_init(&mMutex);
   mSyncThread = new talk_base::Thread();
   mSyncThread->SetName("mSyncThread", this);
   OnInit();
}

void MediaMuxer::OnInit(){
   mDataPool = new SafeDataPool(30);
   if (mDataPool==NULL) {
      LOGE("mDataPool is NULL!");
   }
}

MediaMuxer::~MediaMuxer(){
   OnDestory();
   vhall_lock_destroy(&mMutex);
}

void MediaMuxer::OnDestory(){
   VHALL_THREAD_DEL(mSyncThread);
   VHALL_DEL(mAudioQueue);
   VHALL_DEL(mVideoQueue);
   VHALL_DEL(mAmf0MsgQueue);
   VHALL_DEL(mDataPool);
   RemoveAllMuxer();
}

int MediaMuxer::LiveSetParam(LivePushParam *param){
   int startCount = GetMuxerStartCount();
   if (param!=NULL&&startCount<=0) {
      mParam = param;
      VHALL_DEL(mAudioQueue);
      
      mAudioQueue = new(std::nothrow) SafeDataQueue(this,0.1,0.9,(int)(mParam->dst_sample_rate/AUDIO_SAMPLE_RATE_BASE)*QUEUE_TIME_SIZE);
      if (mAudioQueue==NULL) {
         LOGE("mAudioQueue new fail!");
      }else{
         mAudioQueue->SetTag("audio");
      }
      
      VHALL_DEL(mVideoQueue);
      mVideoQueue = new(std::nothrow) SafeDataQueue(this,0.1,0.9,mParam->frame_rate*QUEUE_TIME_SIZE);
      if (mVideoQueue==NULL) {
         LOGE("mVideoQueue new fail!");
      }else{
         mVideoQueue->SetTag("video");
      }
      
      if (mAmf0MsgQueue==NULL) {
         mAmf0MsgQueue = new(std::nothrow) SafeDataQueue(this,0.1,0.9,10);
         mAmf0MsgQueue->SetTag("Amf0Msg");
      }
      return 0;
   }
   return -1;
}

void MediaMuxer::SetStatusListener(LiveStatusListener * listener){
   mStatueListener = listener;
}

void MediaMuxer::SetRateControl(RateControl *rateControl){
   mRateControl = rateControl;
}

int MediaMuxer::AddMuxer(VHMuxerType type,void * param){
   VhallAutolock _l(&mMutex);
   MuxerInterface  *m_rtmp_publisher = NULL;
   std::string md5Tag = talk_base::MD5(std::string((char*)param));
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++ ){
      if (md5Tag == iter->second->GetTag()) {
         LOGW("muxer id: alrealy existing.",iter->first);
         return iter->first;
      }
   }
   if (type == RTMP_MUXER){
      auto tmp = new SrsRtmpPublisher(this, md5Tag, std::string((char*)param), mParam);
      m_rtmp_publisher = tmp;
      if (mRateControl) {
         mRateControl->SetBufferState(tmp);
      }
   }else if (type == FILE_FLV_MUXER){
      m_rtmp_publisher = new SrsFlvRecorder(this, md5Tag, std::string((char*)param), mParam);
   }else if (type == HTTP_FLV_MUXER){
      auto tmp = new SrsHttpFlvMuxer(this, md5Tag, std::string((char*)param), mParam);
      m_rtmp_publisher = tmp;
      if (mRateControl) {
         mRateControl->SetBufferState(tmp);
      }
   }
   int muxerId = m_rtmp_publisher->GetMuxerId();
   mMuxers.insert(std::pair<int,MuxerInterface*>(muxerId,m_rtmp_publisher));
   return muxerId;
}

void MediaMuxer::RemoveMuxer(int muxer_id){
   VhallAutolock _l(&mMutex);
   int startCount =  GetMuxerStartCount();
   if (startCount==0) {
      auto iter=mMuxers.find(muxer_id);
      if(iter==mMuxers.end()){
         LOGW("we do not find muxer:%d",muxer_id);
      }
      else
      {
         VHMuxerType muxerType = iter->second->GetMuxerType();
         if ((muxerType == RTMP_MUXER || muxerType == HTTP_FLV_MUXER)) {
            mRateControl->CleanBufferState();
         }
         VHALL_DEL(iter->second);
         mMuxers.erase(iter);  //delete muxerId;
      }
   }else{
      mSyncThread->Post(this,MSG_RTMP_REMOVE_MUXER,new IntMessageData(muxer_id));
   }
}

void MediaMuxer::StartMuxer(int muxer_id){
   VhallAutolock _l(&mMutex);
   auto iter=mMuxers.find(muxer_id);
   if(iter==mMuxers.end()){
      LOGW("we do not find muxer:%d",muxer_id);
   }
   else
   {
      if (mIsStart==false) {
         mIsStart = true;
         if (!mSyncThread->started()) {
            mSyncThread->Start();
         }
         mSyncThread->Restart();
         mSyncThread->Post(this,MSG_RTMP_SYNC_DATA);
      }
      if (iter->second->GetState() == MUXER_STATE_STOPED) {
         iter->second->Start();
      }
   }
}

void MediaMuxer::StopAllMuxer(){
   VhallAutolock _l(&mMutex);
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++ ){
      StopMuxer(iter->first);
   }
}

void MediaMuxer::StopMuxer(int muxer_id){
   VhallAutolock _l(&mMutex);
   auto iter=mMuxers.find(muxer_id);
   if(iter==mMuxers.end()){
      LOGW("we do not find muxer:%d",muxer_id);
   }else{
      if (iter->second->GetState() != MUXER_STATE_STOPED){
         VHMuxerType muxerType = iter->second->GetMuxerType();
         if (muxerType == RTMP_MUXER||muxerType == HTTP_FLV_MUXER) {
            mAmf0MsgUnsentlist.clear();
            mAmf0MsgUnsentlist = iter->second->Stop();
         }else{
            iter->second->Stop();
         }
      }
   }
   int startCount = GetPushMuxerStartCount();
   if (startCount == 0 && mRateControl) {
      mRateControl->Stop();
   }

   startCount = GetMuxerStartCount();
   if (startCount==0) {
      mIsStart = false;
      mSyncThread->Clear(this);
      if (mAudioQueue) {
         mAudioQueue->ClearAllQueue();
      }
      if (mVideoQueue) {
         mVideoQueue->ClearAllQueue();
      }
      if (mAmf0MsgQueue) {
         mAmf0MsgQueue->PushFrontList2Queue(&mAmf0MsgUnsentlist);
         mAmf0MsgQueue->SetAllQueueItemTS(0);
         mAmf0MsgUnsentlist.clear();
      }
      mSyncThread->Post(this,MSG_RTMP_CLEAR_SYNC_DATA);
      mSyncThread->Stop();
   }
}

int MediaMuxer::GetMuxerStatus(int muxer_id){
   VhallAutolock _l(&mMutex);
   auto iter = mMuxers.find(muxer_id);
   if(iter==mMuxers.end()){
      LOGW("we do not find muxer:%d",muxer_id);
   }else{
      //TODO get muxer status
      return iter->second->GetState();
   }
   return MUXER_STATE_UNDEFINED;
}

const VHMuxerType MediaMuxer::GetMuxerType(int muxer_id){
   VhallAutolock _l(&mMutex);
   auto iter = mMuxers.find(muxer_id);
   if(iter==mMuxers.end()){
      LOGW("we do not find muxer:%d",muxer_id);
   }else{
      //TODO get muxer status
      return iter->second->GetMuxerType();
   }
   return MUXER_NONE;
}

int MediaMuxer::GetDumpSpeed(int muxer_id){
   VhallAutolock _l(&mMutex);
   auto iter=mMuxers.find(muxer_id);
   if(iter==mMuxers.end()){
      LOGW("we do not find muxer:%d",muxer_id);
   }else{
      //TODO get muxer
      return iter->second->GetDumpSpeed();
   }
   return 0;
}

int MediaMuxer::GetMuxerStartCount(){
   VhallAutolock _l(&mMutex);
   int startCount = 0;
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++ ){
      if (iter->second->GetState() == MUXER_STATE_STARTED) {
         startCount++;
      }
   }
   return startCount;
}

int MediaMuxer::GetPushMuxerStartCount() {
   VhallAutolock _l(&mMutex);
   int startCount = 0;
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++) {
      if (iter->second->GetState() == MUXER_STATE_STARTED ) {
         VHMuxerType muxerType = iter->second->GetMuxerType();
         if (muxerType == RTMP_MUXER || muxerType == HTTP_FLV_MUXER) {
            startCount++;
         }
      }
   }
   return startCount;
}

int MediaMuxer::GetMuxerCount(){
   return (int)mMuxers.size();
}

void MediaMuxer::RemoveAllMuxer(){
   VhallAutolock _l(&mMutex);
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++ ){
      if (iter->second->GetState() != MUXER_STATE_STOPED){
         iter->second->Stop();
      }
      VHALL_DEL(iter->second);
   }
   mMuxers.clear();
}

int MediaMuxer::OnMuxerEvent(int type, MuxerEventParam* param){
   switch (type) {
      case MUXER_MSG_START_SUCCESS:
      {
         if (mStatueListener) {
            //TODO 获取Ip
            auto iter=mMuxers.find(param->mId);
            if(iter==mMuxers.end()){
               LOGW("we do not find muxer:%d", param->mId);
            }else{
               //TODO get muxer status
               std::string ip = iter->second->GetDest();
               EventParam ipParam;
               ipParam.mId = param->mId;
               ipParam.mDesc = ip;
               mStatueListener->NotifyEvent(SERVER_IP, ipParam);
            }
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(OK_PUBLISH_CONNECT, eventParam);
            int startCount = GetMuxerStartCount();
            if (mParam->live_model != LIVE_TYPE_AUDIO_ONLY
                &&startCount == 1
                &&mRateControl) {
               mRateControl->Start();
            }
         }
      }
         break;
      case MUXER_MSG_START_FAILD:
      {
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(ERROR_PUBLISH_CONNECT, eventParam);
         }
      }
         break;
      case MUXER_MSG_DUMP_FAILD:
      {
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(ERROR_SEND, eventParam);
         }
      }
         break;
      case MUXER_MSG_BUFFER_MIN_WARN:
      {
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(UPLOAD_NETWORK_EXCEPTION, eventParam);
         }
      }
         break;
      case MUXER_MSG_BUFFER_EMPTY:
      {
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(UPLOAD_NETWORK_OK, eventParam);
         }
      }
         break;
      case MUXER_MSG_RECONNECTING:{
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(RECONNECTING, eventParam);
         }
      }
         break;
      case MUXER_MSG_NEW_KEY_FRAME:{
         if (mStatueListener) {
            EventParam &eventParam = *(static_cast<EventParam*>(param));
            mStatueListener->NotifyEvent(NEW_KEY_FRAME, eventParam);
         }
      }
         break;
      default:
         break;
   }
   return 0;
}

void MediaMuxer::OnSendVideoData(const char * data, int size, int type, uint64_t timestamp){
   if (mVideoQueue==NULL) {
      LOGE("mVideoQueue is NULL");
      return;
   }
   //LOGD("mVideoQueue->GetQueueSize():%d pts:%llu",mVideoQueue->GetQueueSize(), timestamp);
   SafeData * safeData = mDataPool->GetSafeData((char*)data, size, type, timestamp);
   if (safeData) {
      bool ret = mVideoQueue->PushQueue(safeData);
      if (!ret) {
         LOGE("video data write error");
      }
   }else{
      LOGE("mVideoQueue SafeData is null.");
   }
}

void MediaMuxer::OnSendAudioData(const char * data, int size,int type ,uint64_t timestamp){
   if (mAudioQueue==NULL) {
      LOGE("mAudioQueue is NULL");
      return;
   }
   //LOGD("mAudioQueue->GetQueueSize():%d pts:%llu",mAudioQueue->GetQueueSize(), timestamp);
   SafeData * safeData = mDataPool->GetSafeData((char*)data, size, type, timestamp);
   if (safeData) {
      bool ret = mAudioQueue->PushQueue(safeData);
      if (!ret) {
         LOGE("audio data write error");
      }
   }else{
      LOGE("mAudioQueue SafeData is null.");
   }
}

void MediaMuxer::OnSendAmf0Msg(const char * data, int size, int type, uint64_t timestamp){
   if (mAmf0MsgQueue==NULL) {
      LOGE("mAudioQueue is NULL");
      return;
   }
   LOGD("Amf0 msg size:%d ts:%llu",size,timestamp);
   SafeData * safeData = mDataPool->GetSafeData((char*)data, size, type, timestamp);
   if (safeData) {
      bool ret = mAmf0MsgQueue->PushQueue(safeData,ACTION_DROP_FAILD_LARGE);
      if (!ret) {
         LOGE("amf0 msg write error");
      }
   }
}

void MediaMuxer::OnMessage(talk_base::Message* msg){
   switch(msg->message_id) {
      case MSG_RTMP_SYNC_DATA:{
         OnSyncData();
      }
         break;
      case MSG_RTMP_CLEAR_SYNC_DATA:{
         if (mVideoHeader) {
            mVideoHeader->SelfRelease();
            mVideoHeader = NULL;
         }
         if (mAudioHeader) {
            mAudioHeader->SelfRelease();
            mAudioHeader = NULL;
         }
         if (mAudioItem) {
            mAudioItem->SelfRelease();
            mAudioItem = NULL;
         }
         if (mVideoItem) {
            mVideoItem->SelfRelease();
            mVideoItem = NULL;
         }
         if (mBufferItem) {
            mBufferItem->SelfRelease();
            mBufferItem = NULL;
         }
      }
         break;
      case MSG_RTMP_REMOVE_MUXER:{
         IntMessageData * intData = static_cast<IntMessageData*>(msg->pdata);
         int muxerId = intData->mValue;
         auto iter=mMuxers.find(muxerId);
         if(iter==mMuxers.end()){
            LOGW("we do not find muxer:%d",muxerId);
         }
         else
         {
            VHALL_DEL(iter->second);
            mMuxers.erase(iter);  //delete muxerId;
         }
      }
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void MediaMuxer::OnSyncData(){
   LivePushParam * param = mParam;
   if (param->live_model == LIVE_TYPE_AUDIO_ONLY) {
      // only audio
      OnSendOnlyAudio();
   }else if(param->live_model == LIVE_TYPE_VIDEO_ONLY){
      //only video
      OnSendOnlyVideo();
   }else{
      // all
      OnSendAll();
   }
}

void MediaMuxer::SetMediaHeader(SafeData**header,SafeData**item){
   if (*header) {
      (*header)->SelfRelease();
      *header = NULL;
   }
   *header = *item;
}

// the method is not thread safe
void MediaMuxer::PushData2Muxer(SafeData *header,SafeData *item){
   for (auto iter = mMuxers.begin(); iter != mMuxers.end(); iter++){
      AVHeaderGetStatues headerStatus = iter->second->GetAVHeaderStatus();
      if (header&&((header->mType==VIDEO_HEADER&&(headerStatus!=AV_HEADER_VIDEO&&headerStatus!=AV_HEADER_ALL))||(header->mType==AUDIO_HEADER&&(headerStatus!=AV_HEADER_AUDIO&&headerStatus!=AV_HEADER_ALL)))) {
         header->SelfCopy();
         iter->second->PushData(header);
      }
      if (iter->second->GetState()==MUXER_STATE_STARTED&&item){
         item->SelfCopy();
         iter->second->PushData(item);
      }
   }
}

void MediaMuxer::OnSendOnlyAudio(){
   if (mIsStart&&mAudioQueue) {
      if (mAudioQueue) {
         mAudioItem = mAudioQueue->ReadQueue();
      }
      if (mAudioItem) {
         if (mAudioItem->mType == AUDIO_HEADER) {
            SetMediaHeader(&mAudioHeader, &mAudioItem);
            mAudioItem = NULL;
         }
         // push Amf0 msg to muxer
         while (mAmf0MsgQueue->GetQueueSize()>0&&mAudioItem) {
            uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
            if (mAudioItem->mTs>=amfTS) {
               SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
               PushData2Muxer(mAudioHeader, amfMsg);
               amfMsg->SelfRelease();
            }else{
               break;
            }
         }
         //TODO send audio data to muxer
         PushData2Muxer(mAudioHeader, mAudioItem);
         if (mAudioItem) {
            mAudioItem->SelfRelease();
            mAudioItem = nullptr;
         }
      }
      mSyncThread->Post(this,MSG_RTMP_SYNC_DATA);
   }
}

void MediaMuxer::OnSendOnlyVideo(){
   if (mIsStart&&mAudioQueue) {
      if (mVideoQueue) {
         mVideoItem = mVideoQueue->ReadQueue();
      }
      if (mVideoItem) {
         if (mVideoItem->mType == VIDEO_HEADER) {
            SetMediaHeader(&mVideoHeader, &mVideoItem);
            mVideoItem = NULL;
         }
         // push Amf0 msg to muxer
         while (mAmf0MsgQueue->GetQueueSize()>0&&mVideoItem) {
            uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
            if (mVideoItem->mTs>=amfTS) {
               SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
               PushData2Muxer(mVideoHeader, amfMsg);
               amfMsg->SelfRelease();
            }else{
               break;
            }
         }
         //TODO send audio data to muxer
         PushData2Muxer(mVideoHeader, mVideoItem);
         if (mVideoItem) {
            mVideoItem->SelfRelease();
            mVideoItem = nullptr;
         }
      }
      mSyncThread->Post(this,MSG_RTMP_SYNC_DATA);
   }
}

void MediaMuxer::OnSendAll(){
   // audio and video
   if (mIsStart&&mVideoQueue&&mAudioQueue) {
      if (mBufferItem==NULL) {
         //先从视频队列取出一帧数据，赋值给mBufferItem；
         if (mVideoQueue) {
            mBufferItem = mVideoQueue->ReadQueue();
         }
      }else if(mBufferItem&&(mBufferItem->mType == AUDIO_FRAME||mBufferItem->mType == AUDIO_HEADER)){
         //如果mBufferItem的类型是AUDIO_FRAME，那么需要再从队列取出一个临时视频数据；
         if (mVideoQueue) {
            mVideoItem = mVideoQueue->ReadQueue();
         }
         if (mVideoItem&&mBufferItem) {
            //比较音视频数据的时间戳，谁小发送谁；
            if (mVideoItem->mTs<=mBufferItem->mTs) {
               if (mVideoItem->mType == VIDEO_HEADER) {
                  SetMediaHeader(&mVideoHeader, &mVideoItem);
                  mVideoItem = NULL;
               }
               // push Amf0 msg to muxer
               while (mAmf0MsgQueue->GetQueueSize()>0&&mVideoItem) {
                  uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
                  if (mVideoItem->mTs>=amfTS) {
                     SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
                     PushData2Muxer(mVideoHeader, amfMsg);
                     amfMsg->SelfRelease();
                  }else{
                     break;
                  }
               }
               //TODO send video to muxer
               PushData2Muxer(mVideoHeader, mVideoItem);
               if (mVideoItem) {
                  mVideoItem->SelfRelease();
                  mVideoItem = nullptr;
               }
            }else{
               if (mBufferItem->mType == AUDIO_HEADER) {
                  SetMediaHeader(&mAudioHeader, &mBufferItem);
                  mBufferItem = NULL;
               }
               // push Amf0 msg to muxer
               while (mAmf0MsgQueue->GetQueueSize()>0&&mBufferItem) {
                  uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
                  if (mBufferItem->mTs>=amfTS) {
                     SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
                     PushData2Muxer(mAudioHeader, amfMsg);
                     amfMsg->SelfRelease();
                  }else{
                     break;
                  }
               }
               // TODO sned buffer data to muxer
               PushData2Muxer(mAudioHeader, mBufferItem);
               if (mBufferItem) {
                  mBufferItem->SelfRelease();
                  mBufferItem = nullptr;
               }
               mBufferItem = mVideoItem;
               mVideoItem = nullptr;
            }
         }
      }else if(mBufferItem&&(mBufferItem->mType == VIDEO_I_FRAME||
                             mBufferItem->mType == VIDEO_P_FRAME||
                             mBufferItem->mType == VIDEO_B_FRAME||
                             mBufferItem->mType == VIDEO_HEADER)){
         //如果mBufferItem的类型是VIDEO_FRAME，那么需要再从队列取出一个临时音频数据；
         if (mAudioQueue) {
            mAudioItem = mAudioQueue->ReadQueue();
         }
         if (mAudioItem&&mBufferItem) {
            //比较音视频数据的时间戳，谁小发送谁；
            if (mAudioItem->mTs<mBufferItem->mTs || (mAudioItem->mTs==mBufferItem->mTs&&mAudioItem->mType == AUDIO_HEADER)) {
               if (mAudioItem->mType == AUDIO_HEADER) {
                  SetMediaHeader(&mAudioHeader, &mAudioItem);
                  mAudioItem = NULL;
               }
               // push Amf0 msg to muxer
               while (mAmf0MsgQueue->GetQueueSize()>0&&mAudioItem) {
                  uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
                  if (mAudioItem->mTs>=amfTS) {
                     SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
                     PushData2Muxer(mAudioHeader, amfMsg);
                     amfMsg->SelfRelease();
                  }else{
                     break;
                  }
               }
               //TODO send audio data to muxer
               PushData2Muxer(mAudioHeader, mAudioItem);
               if (mAudioItem) {
                  mAudioItem->SelfRelease();
                  mAudioItem = nullptr;
               }
            }else{
               if (mBufferItem->mType == VIDEO_HEADER) {
                  SetMediaHeader(&mVideoHeader, &mBufferItem);
                  mBufferItem = NULL;
               }
               // push Amf0 msg to muxer
               while (mAmf0MsgQueue->GetQueueSize()>0&&mBufferItem) {
                  uint64_t amfTS = mAmf0MsgQueue->ReadQueueItemTS();
                  if (mBufferItem->mTs>=amfTS) {
                     SafeData *amfMsg = mAmf0MsgQueue->ReadQueue();
                     PushData2Muxer(mVideoHeader, amfMsg);
                     amfMsg->SelfRelease();
                  }else{
                     break;
                  }
               }
               //TODO send buffer data to muxer
               PushData2Muxer(mVideoHeader, mBufferItem);
               if (mBufferItem) {
                  mBufferItem->SelfRelease();
                  mBufferItem = nullptr;
               }
               mBufferItem = mAudioItem;
               mAudioItem = nullptr;
            }
         }
      }
      mSyncThread->Post(this,MSG_RTMP_SYNC_DATA);
   }
}

void MediaMuxer::OnSafeDataQueueChange(SafeDataQueueState state, std::string tag){
   if(state == SAFE_DATA_QUEUE_STATE_FULL){
      EventParam param;
      param.mId = -1;
      if (tag == "audio") {
         param.mDesc = "audio queue full!";
         mStatueListener->NotifyEvent(AUDIO_QUEUE_FULL, param);
      }else if(tag == "video"){
         param.mDesc = "video queue full";
         mStatueListener->NotifyEvent(VIDEO_QUEUE_FULL, param);
      }
   }
}

bool MediaMuxer::LiveGetRealTimeStatus(VHJson::Value &value){
   value["Name"] = VHJson::Value("MediaMuxer");
   value["data_pool_size"] = VHJson::Value(mDataPool->GetMaxDataSize());
   value["data_pool_free_size"] = VHJson::Value(mDataPool->GetFreeDateSize());
   value["audio_queue_size"] = VHJson::Value(mAudioQueue->GetQueueSize());
   value["video_queue_size"] = VHJson::Value(mVideoQueue->GetQueueSize());
   value["amf0_queue_size"] = VHJson::Value(mAmf0MsgQueue->GetQueueSize());
   value["muxers_size"] = VHJson::Value(int(mMuxers.size()));
   //TODO and other items in this level
   
   VHJson::Value minxers(VHJson::arrayValue);
   std::map <int, MuxerInterface*>::iterator iter;
   for (iter = mMuxers.begin(); iter != mMuxers.end(); iter++ ){
      VHJson::Value minxer;
      bool ret = iter->second->LiveGetRealTimeStatus(minxer);
      if (!ret){
         LOGE("Get encoder realtime status failed!");
      }else{
         minxers.append(minxer);
      }
   }
   value["Muxers"] = minxers;
   return true;
}

NS_VH_END
