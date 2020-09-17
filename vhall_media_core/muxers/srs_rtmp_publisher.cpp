#include "srs_rtmp_publisher.h"
#include "../common/vhall_log.h"
#include "../utility/utility.h"
#include <live_sys.h>
#include "../common/live_define.h"
#include "muxer_interface.h"
#include "../utility/rtmp_flv_utility.h"
#include "../utility/sps_pps.h"
#include "../common/auto_lock.h"
#include "../3rdparty/json/json.h"
#include <assert.h>

#define DEFAULT_BUFFER_LENGTH    600
#define DEFAULT_PUBLISH_TIME_OUT 5000
#define DEFAULT_SPEED_COUNT_SIZE 1000

SrsRtmpPublisher::SrsRtmpPublisher(MuxerListener *listener, std::string tag, std::string url, LivePushParam *param)
:MuxerInterface(listener, tag),
mUrl(url),
mRtmp(NULL),
mHasSendHeaders(false),
mHasSendKeyFrame(false),
mFrameData(NULL),
mParam(param),
mVideoHeader(NULL),
mAudioHeader(NULL)
{
   mStartTime = 0;
   mLastSpeedUpdateTime = 0;
   mLastSendBytes = 0;
   mCurentSendBytes = 0;
   mAsyncStoped = false;
   mSendFrameCount = 0;
   mCurentSpeed = 0;
   mState = MUXER_STATE_STOPED;
   mHasEverStarted = false;
   mReConnectCount = 0;
   mHasInQueueVideoHeader = false;
   mHasInQueueAudioHeader = false;
   mRemoteIp = "";
   
   mIsMultiTcp = false;
   if (mUrl[0] == 'a' || mUrl[0] == 'A'){
      mIsMultiTcp = true;
      mUrl.replace(0, 5, "rtmp");
   }
   
   //初始化 mRtmpData
   LivePushParam * liveParam = mParam;
   int data_size = 0;
   if (liveParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      data_size = PCM_FRAME_SIZE*liveParam->ch_num*Utility::GetBitNumWithSampleFormat(liveParam->encode_sample_fmt)/8;
   }else{
      data_size = liveParam->width*liveParam->height * 3 / 2;
   }
   mFrameData = (char*)calloc(1, data_size);
   if (mFrameData == NULL) {
      LOGE("mRtmpData new error!");
   }
   memset(&mMetaData, 0, sizeof(RTMPMetadata));
   
   vhall_lock_init(&mMutex);
   mThread = new talk_base::Thread();
   mThread->SetName("SrsRtmpPublisher mThread", this);
   mThread->Start();
   mBufferQueue = new SafeDataQueue(this, 0.1, 0.5, DEFAULT_BUFFER_LENGTH);
   mBufferQueue->SetFrameDropType(mParam->drop_frame_type);
   mBufferQueue->SetTag("SrsRtmpPublisher_Data_Queue");
   //init timejitter
   int video_frame_duration = 0;
   int audio_frame_duration = 0;
   if (mParam->frame_rate > 0){
      video_frame_duration = 1000 / mParam->frame_rate;
      if (video_frame_duration <= 0){
         video_frame_duration = 1;
      }
   }
   else {
      video_frame_duration = 66;
   }
   
   //TODO compulte audio_frame_duration atuo
   if (mParam->dst_sample_rate > 0 && mParam->ch_num > 0){
      audio_frame_duration = 2048 * 1000 * 8 / mParam->dst_sample_rate / 16;
      if (audio_frame_duration <= 0){
         audio_frame_duration = 1;
      }
   }
   else {
      audio_frame_duration = 23;
   }
   
   mTimeJitter = new TimeJitter(audio_frame_duration, video_frame_duration, 200);
}

SrsRtmpPublisher::~SrsRtmpPublisher()
{
   Stop();
   VHALL_THREAD_DEL(mThread);
   //Destroy();
   VHALL_DEL(mBufferQueue);
   VHALL_DEL(mFrameData);
   VHALL_DEL(mTimeJitter);
   vhall_lock_destroy(&mMutex);
}

bool SrsRtmpPublisher::Start(){
   mAsyncStoped = false;
   if (!mThread->started()) {
      mThread->Start();
   }
   mThread->Post(this, SELF_MSG_START);
   LOGI("SrsRtmpPublisher start with id:%d",GetMuxerId());
   return true;
}

std::list<SafeData*> SrsRtmpPublisher::Stop(){
   std::list<SafeData*> list;
   list.clear();
   if (!mThread->started()){
      return list;
   }
   mAsyncStoped = true;
   {
      VhallAutolock _l(&mMutex);
      if (mRtmp) {
         srs_rtmp_async_close(mRtmp);
         LOGI("srs rtmp async close finish.");
      }
   }
   mThread->Clear(this);
   //save the unsent amf0 data
   list = mBufferQueue->GetListFromQueue(SCRIPT_FRAME);
   mBufferQueue->ClearAllQueue();
   mThread->Post(this, SELF_MSG_STOP);
   //mThread->Stop(); //if put this in lock(mMutex) may cause dead lock.
   mState = MUXER_STATE_STOPED;
   LOGI("SrsRtmpPublisher stop with id:%d",GetMuxerId());
   return list;
}

AVHeaderGetStatues SrsRtmpPublisher::GetAVHeaderStatus()
{
   if (mHasInQueueVideoHeader && mHasInQueueAudioHeader){
      return AV_HEADER_ALL;
   }
   else if (mHasInQueueVideoHeader){
      return AV_HEADER_VIDEO;
   }
   else if (mHasInQueueAudioHeader){
      return AV_HEADER_AUDIO;
   }
   else{
      return AV_HEADER_NONE;
   }
}

bool SrsRtmpPublisher::PushData(SafeData *data){
   if (data->mType == VIDEO_HEADER){
      mHasInQueueVideoHeader = true;
   }
   if (data->mType == AUDIO_HEADER){
      mHasInQueueAudioHeader = true;
   }
   return mBufferQueue->PushQueue(data, BUFFER_FULL_ACTION::ACTION_DROP_FAILD_LARGE);
}

std::string SrsRtmpPublisher::GetDest(){
   return mRemoteIp;
}

int SrsRtmpPublisher::GetState()
{
   return mState;
}

const VHMuxerType SrsRtmpPublisher::GetMuxerType(){
   return RTMP_MUXER;
}

int  SrsRtmpPublisher::GetDumpSpeed(){
   UpdataSpeed();
   return (int)mCurentSpeed;
}

static int re_count = 0;

void SrsRtmpPublisher::OnMessage(talk_base::Message* msg){
   
   switch (msg->message_id) {
      case SELF_MSG_START:{
         int ret = Init();
         if (ret !=0) {
            if (mParam->publish_reconnect_times <= 0||ret == MUXER_MSG_PUSH_ERROR_ARREARAGE){
               mMuxerEvent.mDesc = Utility::ToString(ret);
               ReportMuxerEvent(MUXER_MSG_START_FAILD, &mMuxerEvent);
            }
            else {
               if (!mAsyncStoped){
                  mState = MUXER_STATE_RECONNECTING;
                  mThread->PostDelayed(DEFAULT_PUSH_RECONNECT_DELAY_MS, this, SELF_MSG_RC);
               }
            }
         }else {
            mMuxerEvent.mDesc = "SrsRtmpPublisher init success";
            mState = MUXER_STATE_STARTED;
            mHasEverStarted = true;
            ReportMuxerEvent(MUXER_MSG_START_SUCCESS, &mMuxerEvent);
            LOGI("rtmp push connect ok!");
            if (!mAsyncStoped){
               mThread->Post(this, SELF_MSG_SEND);
            }
         }
      }
      break;
      case SELF_MSG_SEND:{
         int ret = Sending();
         if (ret == -1){
            if (mParam->publish_reconnect_times <= 0){
               mMuxerEvent.mDesc = "SrsRtmpPublisher send faild";
               ReportMuxerEvent(MUXER_MSG_DUMP_FAILD, &mMuxerEvent);
            } else {
               if (!mAsyncStoped){
                  mState = MUXER_STATE_RECONNECTING;
                  mThread->Post(this, SELF_MSG_RC);
               }
            }
         }else if (ret == 0){
            if (!mAsyncStoped){ //if not stop keep sending
               mThread->Post(this, SELF_MSG_SEND);
            }
         }else if(ret == -2){
            mMuxerEvent.mDesc = "SrsRtmpPublisher send faild";
            ReportMuxerEvent(MUXER_MSG_DUMP_FAILD, &mMuxerEvent);
         }
      }
      break;
      case SELF_MSG_RC:{
         mReConnectCount++;
         mMuxerEvent.mDesc = "SrsRtmpPublisher reconnecting times=";
         mMuxerEvent.mDesc += Utility::ToString(mReConnectCount);
         ReportMuxerEvent(MUXER_MSG_RECONNECTING, &mMuxerEvent);
         mState = MUXER_STATE_RECONNECTING;
         LOGE("SrsRtmpPublisher reconnecting mReConnectCount=%d", mReConnectCount);
         int ret = Init();
         if (ret!=0){
            if (mReConnectCount >= mParam->publish_reconnect_times||ret == MUXER_MSG_PUSH_ERROR_ARREARAGE){
            //if (mReConnectCount >= mParam->publish_reconnect_times){
               //last time reconnect failed, report faild
               if (!mHasEverStarted){ //never started
                  mMuxerEvent.mDesc = Utility::ToString(ret);
                  ReportMuxerEvent(MUXER_MSG_START_FAILD, &mMuxerEvent);
               }else {
                  mMuxerEvent.mDesc = "SrsRtmpPublisher send faild";
                  ReportMuxerEvent(MUXER_MSG_DUMP_FAILD, &mMuxerEvent);
               }
               LOGE("SrsRtmpPublisher reconnecting fiaild have tried=%d", mReConnectCount);
            }
            else {
               if (!mAsyncStoped){
                  mThread->PostDelayed(DEFAULT_PUSH_RECONNECT_DELAY_MS, this, SELF_MSG_RC);   //wait 1 sec, try again
               }
            }
            LOGE("SrsRtmpPublisher reconnecting try=%d fiaild", mReConnectCount);
         }else{
            mState = MUXER_STATE_STARTED;
            mReConnectCount = 0;  //reset reconnect count.
            re_count++;
            if (!mHasEverStarted){
               mHasEverStarted = true;
               mMuxerEvent.mDesc = "SrsRtmpPublisher init success";
               ReportMuxerEvent(MUXER_MSG_START_SUCCESS, &mMuxerEvent);
            }
            else{
               mMuxerEvent.mDesc = "SrsRtmpPublisher need new key frame";
               ReportMuxerEvent(MUXER_MSG_NEW_KEY_FRAME, &mMuxerEvent);
            }
            if (!mAsyncStoped){
               mThread->Post(this, SELF_MSG_SEND);
            }
            LOGE("SrsRtmpPublisher reconnect  success");
         }
      }
      break;
      case SELF_MSG_STOP:
         Reset();
         mState = MUXER_STATE_STOPED;
         break;
         
      default:
         break;
   }
   VHALL_DEL(msg->pdata)
}

int SrsRtmpPublisher::Sending(){
   SafeData *frame = mBufferQueue->ReadQueue(true);
   if (frame == NULL) {
      LOGW("read send queue 15s timeout!");
      return -2;
   }
   bool ret = Publish(frame);
   frame->SelfRelease();
   if (ret==false) {
      return -1;
   }
   return 0;
}

bool SrsRtmpPublisher::LiveGetRealTimeStatus(VHJson::Value & value){
   
   value["Name"] = VHJson::Value("SrsRtmpPublisher");
   value["id"] = VHJson::Value(MuxerInterface::GetMuxerId());
   value["tag"] = VHJson::Value(MuxerInterface::GetTag());
   value["dest"] = VHJson::Value(GetDest());
   value["speed"] = VHJson::Value(GetDumpSpeed());
   value["send_buffer_size"] = VHJson::Value(mBufferQueue->GetQueueSize());
   value["drop_type"] = VHJson::Value(DropFrameTypeStr[(int)mBufferQueue->GetFrameDropType()]);
   value["drop_frames_count"] = VHJson::Value(mBufferQueue->GetFrameDropCount());
   unsigned int send_frame_count = (unsigned int)mSendFrameCount;
   value["send_frames_count"] = VHJson::Value(send_frame_count);
   value["start_duration"] = VHJson::Value((int)(srs_utils_time_ms() - mStartTime));
   value["send_bytes"] = VHJson::Value((int)(mCurentSendBytes));
   if (mState == MUXER_STATE_STOPED){
      value["status"] = VHJson::Value("stoped");
   }
   else if (mState == MUXER_STATE_STARTED){
      value["status"] = VHJson::Value("started");
   }
   else {
      value["status"] = VHJson::Value("undefined");
   }
   return true;
}

int SrsRtmpPublisher::GetKeyFrameCount(){
   return mBufferQueue->GetKeyFrameCount();
}

int SrsRtmpPublisher::GetQueueDataSize()
{
   return mBufferQueue->GetQueueDataSize();
}

uint32_t SrsRtmpPublisher::GetQueueDataDuration()
{
   return mBufferQueue->GetQueueDataDuration();
}

int SrsRtmpPublisher::GetQueueSize(){
   return mBufferQueue->GetQueueSize();
}

int SrsRtmpPublisher::GetMaxNum(){
   return mBufferQueue->GetMaxNum();
}

void SrsRtmpPublisher::OnSafeDataQueueChange(SafeDataQueueState state, std::string tag){
   //TODO report or not
   if (state == SAFE_DATA_QUEUE_STATE_FULL){
      mMuxerEvent.mDesc = "buffer full";
      ReportMuxerEvent(MUXER_MSG_BUFFER_FULL, &mMuxerEvent);
   }else if (state == SAFE_DATA_QUEUE_STATE_EMPTY){
      mMuxerEvent.mDesc = "buffer empty";
      ReportMuxerEvent(MUXER_MSG_BUFFER_EMPTY, &mMuxerEvent);
   }else if (state == SAFE_DATA_QUEUE_STATE_NORMAL){
      mMuxerEvent.mDesc = "buffer normal";
      ReportMuxerEvent(MUXER_MSG_BUFFER_NORMAL, &mMuxerEvent);
   }else if (state == SAFE_DATA_QUEUE_STATE_MIN_WARN){
      mMuxerEvent.mDesc = "buffer min warning";
      ReportMuxerEvent(MUXER_MSG_BUFFER_MIN_WARN, &mMuxerEvent);
   }else if(state == SAFE_DATA_QUEUE_STATE_MAX_WARN){
      mMuxerEvent.mDesc = "buffer max warning";
      ReportMuxerEvent(MUXER_MSG_BUFFER_MAX_WARN, &mMuxerEvent);
   }
}

int SrsRtmpPublisher::ReportMuxerEvent(int type, MuxerEventParam *param){
   if (mAsyncStoped/* &&(type == MUXER_MSG_START_FAILD || type == MUXER_MSG_DUMP_FAILD)*/){
      return 0;
   }
   return MuxerInterface::ReportMuxerEvent(type, param);
}

/*here need to be awared:
	we only lock mMutex at Init and Destory, which will create and delete mRtmp.
	because all things in mRtmp is will not change except Send_bytes which have
	changed to  atomic_uint64_t。
	so the mMutex is only to protect the mRtmp point not it content.
 
	self thread will writ mRtmp in init and Destory。
	and all other private thread are called by self thread。
 
	outside thread will read mRtmp in Stop and Getip, outside thread
	will not wri te mRtmp
 
	so we only put 4 lock in Init Destroy Getip and Stop。
	*/
int SrsRtmpPublisher::Init()
{
   Reset(false); //this mast put befor lock .or this thread may got two lock at the same time.
   if (mStartTime == 0){ //first time init.
      mStartTime = srs_utils_time_ms();
   }
   mLastSpeedUpdateTime = srs_utils_time_ms();
   
   VhallAutolock _l(&mMutex);
   
   if (mIsMultiTcp){
      mRtmp = srs_rtmp_create_msock(mUrl.c_str());
   }
   else{
      mRtmp = srs_rtmp_create(mUrl.c_str());
   }
   //TODO　fix other error code return.
   if (mRtmp == NULL) {
      LOGE("srs_rtmp_create failed.");
      return -1;
   }
   int time_out = mParam->publish_timeout;
   if (time_out <= 0){
      time_out = DEFAULT_PUBLISH_TIME_OUT;
   }
   int ret = srs_rtmp_set_timeout(mRtmp, time_out, time_out);
   if (ret==0) {
      LOGI("set timeout success.");
   }
   LOGI("start simple handshake.");
   if (srs_rtmp_handshake(mRtmp) != 0) {
      LOGE("simple handshake failed.");
      Destroy();
      return MUXER_MSG_SIMPLE_HANDSHAKE_FAILED;
   }
   LOGI("simple handshake success");
   if (srs_rtmp_connect_app(mRtmp) != 0) {
      LOGE("connect vhost/app failed.");
      Destroy();
      return MUXER_MSG_CONNECT_VHOST_AND_APP_FAILED;
   }
   LOGI("connect vhost/app success url:%s", mUrl.c_str());
   int error_code = 0;
   MuxerMSG msg = MUXER_MSG_START_FAILD;
   std::string msgString;
   if ((error_code = srs_rtmp_publish_stream(mRtmp)) != 0) {
      switch (error_code){
         case ERRORPublishBadname:
            msgString = "NetStream.Publish.BadName";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_ALREADY_EXISTS;
            break;
         case ERRORPublishAlreadyPublished:
            msgString = "NetStream.Publish.AlreadyPublished";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_ALREADY_EXISTS;
            break;
         case ERRORPublishTokenEmpty:
            msgString = "NetStream.Publish.TokenEmpty";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN;
            break;
         case ERRORPublishBlackList:
            msgString = "NetStream.Publish.BlackList";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_IN_BLACKLIST;
            break;
         case ERRORPublishNotWhiteList:
            msgString = "NetStream.Publish.NotWhiteList";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_NOT_IN_WHITELIST;
            break;
         case ERRORPublishKickOut:
            msgString = "NetStream.Publish.KickOut";
            msg = MUXER_MSG_START_FAILD;
            //ReportMuxerEvent(MUXER_MSG_START_FAILD, &mMuxerEvent);
            break;
         case ERRORPublishAuthFailed:
            msgString = "NetStream.Publish.AuthFailed";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN;
            break;
         case ERRORPublishArrearage:
            msgString = "NetStream.Publish.Arrearage";
            msg = MUXER_MSG_PUSH_ERROR_ARREARAGE;
            break;
         default:
            msgString = "NetStream.Publish.AuthFailed";
            msg = MUXER_MSG_PUSH_ERROR_REJECTED_INVALID_TOKEN;
           break;
      }
      LOGE("publish stream failed. %s",msgString.c_str());
      Destroy();
      return (int)msg;
   }
   
   //set mRemoteip;
   char ip_buf[64];
   if (srs_rtmp_get_remote_ip(mRtmp, ip_buf, 64) > 0){
      mRemoteIp = ip_buf;
   }
   return 0;
}

bool SrsRtmpPublisher::Reset(bool isStop)
{
   Destroy();
   mAsyncStoped = false;
   mCurentSpeed = 0;
   mHeaderWaitCount = 0;
   
   mHasSendHeaders = false;
   mHasSendKeyFrame = false;
   
   mHasInQueueVideoHeader = false;
   mHasInQueueAudioHeader = false;
   
   if (isStop){
      mStartTime = 0;
      mLastSendBytes = 0;
      mCurentSendBytes = 0;
      mSendFrameCount = 0;
      
      if (mVideoHeader){
         mVideoHeader->SelfRelease();
         mVideoHeader = NULL;
      }
      if (mAudioHeader){
         mAudioHeader->SelfRelease();
         mAudioHeader = NULL;
      }
   }
   //reset time jitter.
   mTimeJitter->Reset();
   memset(&mMetaData, 0, sizeof(RTMPMetadata));
   mBufferQueue->Reset(isStop);
   return true;
}

void SrsRtmpPublisher::Destroy(){
   VhallAutolock _l(&mMutex);
   if (mRtmp){
      //llc srs2.0 do not have this function any more
      LOGI("srs librtmp close finish.");
      srs_rtmp_async_close(mRtmp);
      srs_rtmp_destroy(mRtmp);
      mRtmp = NULL;
   }
}

bool SrsRtmpPublisher::SendHeaders() {
   //RTMPMetadata        metaData;//meta数据
   memset(&mMetaData, 0, sizeof(RTMPMetadata));
   //LiveParam * liveParam = mParam;
   
   if (mParam->live_model == LIVE_TYPE_AUDIO_ONLY){
      mMetaData.bHasVideo = false;
      mMetaData.bHasAudio = true;
      mMetaData.nAudioSampleRate = mParam->dst_sample_rate;
      mMetaData.nAudioSampleSize = mParam->audio_bitrate;
      mMetaData.nAudioChannels = mParam->ch_num;
      if (false == SendMetadata(mRtmp, &mMetaData, 0)) {
         LOGE("Meta data send fail!");
         return false;
      }
      if (false == SendAudioInfoData()) {
         LOGE("AudioInfo data send fail!");
         return false;
      }
   }
   
   if (mParam->live_model == LIVE_TYPE_VIDEO_AND_AUDIO ||
       mParam->live_model == LIVE_TYPE_VIDEO_ONLY)
   {
      int nSize = mVideoHeader->mSize;
      char *data = mVideoHeader->mData;
      
      mMetaData.nFrameRate = mParam->frame_rate;
      mMetaData.nVideoDataRate = mParam->bit_rate;
      mMetaData.bHasVideo = true;
      if (mParam->live_model == LIVE_TYPE_VIDEO_ONLY){
         mMetaData.bHasAudio = false;
      }
      else{
         mMetaData.bHasAudio = true;
         mMetaData.nAudioSampleRate = mParam->dst_sample_rate;
         mMetaData.nAudioSampleSize = mParam->audio_bitrate;
         mMetaData.nAudioChannels = mParam->ch_num;
      }
      
      NaluUnit nalu;
      //get sps nalu
      if (Utility::GetNalu(7, (unsigned char*)data, nSize, &nalu) != 0){
         LOGE("Do not find sps Nalu in Video Header data !!!!!");
      }
      mMetaData.nSpsLen = nalu.size;
      memcpy(mMetaData.Sps, nalu.data, nalu.size);
      
      //get pps nalu
      if (Utility::GetNalu(8, (unsigned char*)data, nSize, &nalu) != 0){
         LOGE("Do not find pps Nalu in Video Header data !!!!!");
      }
      mMetaData.nPpsLen = nalu.size;
      memcpy(mMetaData.Pps, nalu.data, nalu.size);
      {
         get_bit_context con_buf;
         SPS             my_sps;
         memset(&con_buf, 0, sizeof(con_buf));
         memset(&my_sps, 0, sizeof(my_sps));
         con_buf.buf = mMetaData.Sps + 1;
         con_buf.buf_size = mMetaData.nSpsLen - 1;
         if (h264dec_seq_parameter_set(&con_buf, &my_sps) != 0){
            LOGE("hls codec demux video failed. ret=%d", -1);
            return false;
         }
         mMetaData.nWidth = h264_get_width(&my_sps);
         mMetaData.nHeight = h264_get_height(&my_sps);
      }
      LOGI("in rtmppublisher, metaData.nWidth = %d, metaData.nHeight = %d, metaData.nFrameRate=%d",
           mMetaData.nWidth, mMetaData.nHeight, mMetaData.nFrameRate);
      
      if (false == SendMetadata(mRtmp, &mMetaData, 0)){
         LOGE("SendMetadata data send fail!");
         return false;
      }
      
      if (mParam->live_model != LIVE_TYPE_VIDEO_ONLY){
         if (false == SendAudioInfoData()) {
            LOGE("AudioInfo data send fail!");
            return false;
         }
         LOGI("send audioInfo finish!");
      }
      
      if (false == SendPpsAndSpsData(mRtmp, &mMetaData, 0)) {
         LOGE("PpsAndSps data send fail!");
         return false;
      }
      LOGI("send PpsAndSps finish!");
   }
   mHasSendHeaders = true;
   return true;
}

bool SrsRtmpPublisher::Publish(SafeData *frame){
   const char * data = frame->mData;
   int size = frame->mSize;
   int type = frame->mType;
   uint64_t timestamp = frame->mTs;
   LivePushParam * live_param = mParam;
   
   if (!mHasSendHeaders){
      bool ret = true;
      bool is_header = false;
      if (type == AUDIO_HEADER){
         if (mAudioHeader){
            mAudioHeader->SelfRelease();
         }
         mAudioHeader = frame->SelfCopy();
         is_header = true;
      }
      
      if (type == VIDEO_HEADER){
         if (mVideoHeader){
            mVideoHeader->SelfRelease();
         }
         mVideoHeader = frame->SelfCopy();
         is_header = true;
      }
      if (type != VIDEO_HEADER && type != AUDIO_HEADER) {
         LOGW("first item is not VIDEO_HEADER or AUDIO_HEADER!");
      }
      if ((live_param->live_model == LIVE_TYPE_AUDIO_ONLY && mAudioHeader) ||
          (live_param->live_model == LIVE_TYPE_VIDEO_ONLY && mVideoHeader) ||
          (live_param->live_model == LIVE_TYPE_VIDEO_AND_AUDIO &&
           mAudioHeader && mVideoHeader)){
             ret = SendHeaders();
      }
      if (is_header){ //type is header.
         return ret;
      }else {
         mHeaderWaitCount++;
         //type is not header if header have sent header (reconnect header have got before)
         //we need to send this frame
         LOGW("Audio and Video first two frame is not audio header or video header!");
         if (!ret){ //send header faild
            return false;
         }else if (ret && !mHasSendHeaders){ //wait for header
            if (mHeaderWaitCount>20) {
               LOGE("audio header or video header wait count over 20");
               return false;
            }else{
               return true;
            }
         } //else header have send we will send this fame bellow.
      }
   }else{
      mHeaderWaitCount = 0;
   }
   //new header come
   if (type == AUDIO_HEADER || type == VIDEO_HEADER){
      if (type == AUDIO_HEADER) {
         if (mAudioHeader){
            mAudioHeader->SelfRelease();
         }
         mAudioHeader = frame->SelfCopy();
      }
      if (type == VIDEO_HEADER){
         if (mVideoHeader){
            mVideoHeader->SelfRelease();
         }
         mVideoHeader = frame->SelfCopy();
      }
      return SendHeaders();
   }
   
   if (!mHasSendKeyFrame && live_param->live_model != LIVE_TYPE_AUDIO_ONLY){
      if (type > VIDEO_I_FRAME) { //wait to send key frame.
         LOGW("wait to send key frame.");
         return true;
      }
   }
   
   JitterFrameType jitter_type;
   if (type == AUDIO_FRAME){
      jitter_type = JITTER_FRAME_TYPE_AUDIO;
   }else {
      jitter_type = JITTER_FRAME_TYPE_VIDEO;
   }
   uint64_t ts = 0;
   if (type != SCRIPT_FRAME) {
      ts = mTimeJitter->GetCorretTime(jitter_type, timestamp);
   }
   if (type == AUDIO_FRAME){
      if (false == SendAudioPacket(mRtmp, const_cast<char *>(data), size, ts)) {
         LOGE("Send AUDIO Frame error");
         //pthread_mutex_unlock(&mMutex);
         return false;
      }
      LOGD("A frame size:%d ts:%llu",size,ts);
   }else if(type == SCRIPT_FRAME){
      bool ret = SendPacket(mRtmp, SRS_RTMP_TYPE_SCRIPT, timestamp, const_cast<char *>(data), size);
      if (false == ret ) {
         LOGE("Send Amf0 msg error!");
         //pthread_mutex_unlock(&mMutex);
        return ret;
      }
      LOGD("send Amf0 msg size:%d ts:%llu",size,timestamp);
   }else {
      bool is_key = false;
      int  nalu_title = 0;
      
      if (size > 3 && data[0] == 0 && data[1] == 0 && data[2] == 1){
         nalu_title = 3;
      }else if (size > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1){
         nalu_title = 4;
      }else{
         nalu_title = 0;
         if (mParam->encode_type==ENCODE_TYPE_SOFTWARE) {
            LOGE("video header is not 001 or 0001!");
            return false;
         }
      }
      
      if (type == VIDEO_I_FRAME){
         is_key = true;
      }
      
      if (false == SendH264Packet(mRtmp, const_cast<char *>(data + nalu_title), size - nalu_title, is_key, ts)) {
         LOGE("Send P Frame error");
         //pthread_mutex_unlock(&mMutex);
         return false;
      }
      if (!mHasSendKeyFrame && is_key){
         mHasSendKeyFrame = true;
      }
      LOGD("%s frame size:%d ts:%llu", type==VIDEO_I_FRAME?"I":"P",size - nalu_title, ts);
   }
   return true;
}

bool SrsRtmpPublisher::SendMetadata(srs_rtmp_t pRtmp, LPRTMPMetadata lpMetaData, uint64_t timestamp)
{
   if (NULL == lpMetaData) {
      return false;
   }
   char medateData[1024] = { 0 };
   char *data = medateData;
   int dfSize, mdSize, obSize;
   {
      srs_amf0_t str1Amf0 = srs_amf0_create_string("@setDataFrame");
      dfSize = srs_amf0_size(str1Amf0);
      srs_amf0_serialize(str1Amf0, data, dfSize);
      data += dfSize;
      srs_amf0_free(str1Amf0);
      
      srs_amf0_t str2Amf0 = srs_amf0_create_string("onMetaData");
      mdSize = srs_amf0_size(str2Amf0);
      srs_amf0_serialize(str2Amf0, data, mdSize);
      data += mdSize;
      srs_amf0_free(str2Amf0);
      
      
      srs_amf0_t objectAmf0 = srs_amf0_create_object();
      
      //live did not need to write this
      //srs_amf0_t num1Amf0 = srs_amf0_create_number(0.0);
      //srs_amf0_object_property_set(objectAmf0, "duration", num1Amf0);
      
      //srs_amf0_t num2Amf0 = srs_amf0_create_number(0.0);
      //srs_amf0_object_property_set(objectAmf0, "filesize", num2Amf0);
      
      if (lpMetaData->bHasVideo) {
         srs_amf0_t num3Amf0 = srs_amf0_create_number(lpMetaData->nWidth);
         srs_amf0_object_property_set(objectAmf0, "width", num3Amf0);
         
         srs_amf0_t num4Amf0 = srs_amf0_create_number(lpMetaData->nHeight);
         srs_amf0_object_property_set(objectAmf0, "height", num4Amf0);
         
         srs_amf0_t num5Amf0 = srs_amf0_create_number(lpMetaData->nFrameRate);
         srs_amf0_object_property_set(objectAmf0, "framerate", num5Amf0);
         
         srs_amf0_t num6Amf0 = srs_amf0_create_number(lpMetaData->nVideoDataRate);
         srs_amf0_object_property_set(objectAmf0, "videodatarate", num6Amf0);
         
         srs_amf0_t num7Amf0 = srs_amf0_create_number(FlvVideoCodecIDAVC);
         srs_amf0_object_property_set(objectAmf0, "videocodecid", num7Amf0);
      }
      if (lpMetaData->bHasAudio){
         srs_amf0_t num8Amf0 = srs_amf0_create_number(lpMetaData->nAudioSampleRate);
         srs_amf0_object_property_set(objectAmf0, "audiosamplerate", num8Amf0);
         
         srs_amf0_t num9Amf0 = srs_amf0_create_number(lpMetaData->nAudioSampleSize);
         srs_amf0_object_property_set(objectAmf0, "audiosamplesize", num9Amf0);
         
         srs_amf0_t num10Amf0 = srs_amf0_create_number(FlvSoundFormatAAC);
         srs_amf0_object_property_set(objectAmf0, "audiocodecid", num10Amf0);
         
         srs_amf0_t num11Amf0 = srs_amf0_create_number(lpMetaData->nAudioChannels);
         srs_amf0_object_property_set(objectAmf0, "audiochannels", num11Amf0);
      }
      
      //add extra metadata.
      std::map<std::string, std::string>::iterator it;
      for (it = mParam->extra_metadata.begin(); it != mParam->extra_metadata.end(); it++){
         srs_amf0_t item = srs_amf0_create_string(it->second.c_str());
         srs_amf0_object_property_set(objectAmf0, it->first.c_str(), item);
      }
      
      srs_amf0_t str3Amf0 = srs_amf0_create_string("vhall");
      srs_amf0_object_property_set(objectAmf0, "copyright", str3Amf0);
      
      obSize = srs_amf0_size(objectAmf0);
      srs_amf0_serialize(objectAmf0, data, obSize);
      srs_amf0_free(objectAmf0);
   }
   return SendPacket(pRtmp, SRS_RTMP_TYPE_SCRIPT, timestamp, medateData, obSize + mdSize + dfSize);
}

bool SrsRtmpPublisher::SendAudioInfoData()
{
   assert(mAudioHeader != NULL && mRtmp != NULL);
   LivePushParam * param = mParam;
   //int sampleNum = Utility::GetNumFromSamplingRate(param->dst_sample_rate);
   int chnnelNum = param->ch_num == 1 ? 0 : 1;
   // AUDIO info 数据
   char audioHeader[128] = { 0 };
   audioHeader[0] = 0xAC | (1 << 1) | (chnnelNum << 0);//第一个字节A是10代表的意思是AAC
   audioHeader[1] = 0x00;
   
   memcpy(audioHeader + 2, mAudioHeader->mData, mAudioHeader->mSize);
   
   //audioHeader[2] = 0x10|(sampleNum>>1);
   //audioHeader[3] = ((sampleNum&0x1)<<7)|((param->ch_num & 0xF) << 3);
   
   return SendPacket(mRtmp, SRS_RTMP_TYPE_AUDIO, 0, audioHeader, mAudioHeader->mSize + 2);
}

bool SrsRtmpPublisher::SendPpsAndSpsData(srs_rtmp_t pRtmp, LPRTMPMetadata lpMetaData, uint64_t timestamp)
{
   char data[1024] = { 0 };
   int dataSize = 0;
   SetPpsAndSpsData(lpMetaData, data, &dataSize);
   return SendPacket(pRtmp, SRS_RTMP_TYPE_VIDEO, timestamp, data, dataSize);
}

bool SrsRtmpPublisher::SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData, int * dataSize){
   
   char * body = spsppsData;
   int i = 0;
   body[i++] = 0x17; // 1:keyframe  7:AVC
   body[i++] = 0x00; // AVC sequence header
   body[i++] = 0x00;
   body[i++] = 0x00;
   body[i++] = 0x00; // fill in 0;
   // AVCDecoderConfigurationRecord.
   body[i++] = 0x01; // configurationVersion
   body[i++] = lpMetaData->Sps[1]; // AVCProfileIndication
   body[i++] = lpMetaData->Sps[2]; // profile_compatibility
   body[i++] = lpMetaData->Sps[3]; // AVCLevelIndication
   body[i++] = 0xff; // lengthSizeMinusOne
   
   // sps nums
   body[i++] = 0xE1; //&0x1f
   // sps data length
   body[i++] = lpMetaData->nSpsLen >> 8;
   body[i++] = lpMetaData->nSpsLen & 0xff;
   // sps data
   memcpy(&body[i], lpMetaData->Sps, lpMetaData->nSpsLen);
   i = i + lpMetaData->nSpsLen;
   // pps nums
   body[i++] = 0x01; //&0x1f
   // pps data length
   body[i++] = lpMetaData->nPpsLen >> 8;
   body[i++] = lpMetaData->nPpsLen & 0xff;
   // sps data
   memcpy(&body[i], lpMetaData->Pps, lpMetaData->nPpsLen);
   i = i + lpMetaData->nPpsLen;
   *dataSize = i;
   return true;
}

void SrsRtmpPublisher::UpdataSpeed(){
   if (mLastSpeedUpdateTime == 0){ //not start
      return;
   }
   
   int64_t duration = 0;
   uint64_t last_speed_time = mLastSpeedUpdateTime;
   uint64_t curent_time = srs_utils_time_ms();
   duration = curent_time - last_speed_time;
   if (duration > DEFAULT_SPEED_COUNT_SIZE){
      mLastSpeedUpdateTime = curent_time;
      mCurentSpeed = (mCurentSendBytes - mLastSendBytes) * 8 / duration;
      mLastSendBytes = (long long)mCurentSendBytes;
   }
}

bool SrsRtmpPublisher::SendPacket(srs_rtmp_t pRtmp, char type, uint64_t timestamp, char* data, int size)
{
   bool ret = false;
   if (NULL == data || NULL == pRtmp) {
      LOGE("!pRtmp");
      return ret;
   }
   char * pushData = (char*)calloc(1, size);
   memcpy(pushData, data, size);
   
   if (srs_rtmp_write_packet(pRtmp, type, (uint32_t)timestamp, pushData, size) == 0){
      ret = true;
   }else{
      ret = false;
   }
   
   mCurentSendBytes += size;
   
   return ret;
}

bool SrsRtmpPublisher::SendH264Packet(srs_rtmp_t pRtmp, char *data, long size, bool bIsKeyFrame, uint64_t nTimeStamp){
   if (NULL == data || NULL == pRtmp) {
      return false;
   }
   char *body = mFrameData;
   int i = 0;
   if (bIsKeyFrame){
      body[i++] = 0x17;
   }else{
      body[i++] = 0x27;
   }
   body[i++] = 0x01;// AVC NALU
   body[i++] = 0x00;
   body[i++] = 0x00;
   body[i++] = 0x00;
   
   //TODO there may be more than one NALUs, if need to check.
   // NALU size
   body[i++] = size >> 24;
   body[i++] = size >> 16;
   body[i++] = size >> 8;
   body[i++] = size & 0xff;
   // NALU data
   memcpy(&body[i], data, size);
   mSendFrameCount++;
   return SendPacket(pRtmp, SRS_RTMP_TYPE_VIDEO, nTimeStamp, body, (unsigned int)(size + i));
}

bool SrsRtmpPublisher::SendAudioPacket(srs_rtmp_t pRtmp, char *data, int size, uint64_t nTimeStamp){
   if (NULL == data || NULL == pRtmp){
      return false;
   }
   
   LivePushParam * param = mParam;
   int chnnelNum = param->ch_num == 1 ? 0 : 1;
   char *body = mFrameData;
   int i = 0;
   body[i++] = 0xAC | (1 << 1) | (chnnelNum << 0);//第一个字节A是10代表的意思是AAC
   body[i++] = 0x01;
   // NALU data
   memcpy(&body[i], data, size);
   mSendFrameCount++;
   return SendPacket(pRtmp, SRS_RTMP_TYPE_AUDIO, nTimeStamp, body, size + i);
}
