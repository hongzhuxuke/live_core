#include "srs_http_flv_streamer.h"
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
#include "talk/base/stream.h"
#include "talk/base/httpcommon.h"
#include "srs_http_handler.h"

#define DEFULT_BUFFER_LENGTH    600
#define DEFULT_PUBLISH_TIME_OUT 5000
#define DEFULT_SPEED_COUNT_SIZE 1000

#define ERROR_SUCCESS           0L

/**
 * E.4.1 FLV Tag, page 75
 */
enum SrsCodecFlvTag
{
   // set to the zero to reserved, for array map.
   SrsCodecFlvTagReserved = 0,
   
   // 8 = audio
   SrsCodecFlvTagAudio = 8,
   // 9 = video
   SrsCodecFlvTagVideo = 9,
   // 18 = script data
   SrsCodecFlvTagScript = 18,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class SrsHttpFlvEncoder
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SrsHttpFlvEncoder::SrsHttpFlvEncoder()
{
   writer = NULL;
   tag_stream = new SrsDataStream();
}

SrsHttpFlvEncoder::~SrsHttpFlvEncoder()
{
   VHALL_DEL(tag_stream);
}

int SrsHttpFlvEncoder::initialize(SrsAsyncHttpRequest* wr){
   int ret = ERROR_SUCCESS;
   assert(wr);
   writer = wr;

   return ret;
}

int SrsHttpFlvEncoder::write_header(){
   int ret = ERROR_SUCCESS;

   // 9bytes header and 4bytes first previous-tag-size
   char flv_header[] = {
      'F', 'L', 'V', // Signatures "FLV"
      (char)0x01, // File version (for example, 0x01 for FLV version 1)
      (char)0x05, // 4, audio; 1, video; 5 audio+video.
      (char)0x00, (char)0x00, (char)0x00, (char)0x09 // DataOffset UI32 The length of this header in bytes
   };

   // write 9bytes header.
   if ((ret = write_header(flv_header)) != ERROR_SUCCESS) {
      LOGE("write flv header failed. ret=%d", ret);
      return ret;
   }

   return ret;
}
int SrsHttpFlvEncoder::write_header(char flv_header[9]){
   int ret = ERROR_SUCCESS;

   // write data.
   if ((ret = this->writer->write(flv_header, 9, NULL)) != ERROR_SUCCESS) {
      LOGE("write flv header failed. ret=%d", ret);
      return ret;
   }

   // previous tag size.
   char pts[] = { (char)0x00, (char)0x00, (char)0x00, (char)0x00 };
   if ((ret = this->writer->write(pts, 4, NULL)) != ERROR_SUCCESS) {
      LOGE("write flv pre tag size failed. ret=%d", ret);
      return ret;
   }

   return ret;
}
int SrsHttpFlvEncoder::write_metadata(char type, char* data, int size){
   int ret = ERROR_SUCCESS;

   assert(data);

   if ((ret = this->write_metadata_to_cache(type, data, size, tag_header)) != ERROR_SUCCESS) {
      free(data);
      return ret;
   }

   if ((ret = write_tag(tag_header, sizeof(tag_header), data, size)) != ERROR_SUCCESS) {
      free(data);
      LOGE("write flv data tag failed. ret=%d", ret);
      return ret;
   }
   free(data);
   return ret;
}
int SrsHttpFlvEncoder::write_audio(int64_t timestamp, char* data, int size){
   int ret = ERROR_SUCCESS;

   assert(data);

   if ((ret = write_audio_to_cache(timestamp, data, size, tag_header)) != ERROR_SUCCESS) {
      free(data);
      return ret;
   }

   if ((ret = write_tag(tag_header, sizeof(tag_header), data, size)) != ERROR_SUCCESS) {
      free(data);
      LOGE("write flv audio tag failed. ret=%d", ret);
      return ret;
   }
   free(data);
   return ret;
}
int SrsHttpFlvEncoder::write_video(int64_t timestamp, char* data, int size){
   int ret = ERROR_SUCCESS;

   assert(data);

   if ((ret = write_video_to_cache(timestamp, data, size, tag_header)) != ERROR_SUCCESS) {
      free(data);
      return ret;
   }

   if ((ret = write_tag(tag_header, sizeof(tag_header), data, size)) != ERROR_SUCCESS) {
      free(data);
      LOGE("write flv video tag failed. ret=%d", ret);
      return ret;
   }
   free(data);
   return ret;
}
int SrsHttpFlvEncoder::write_metadata_to_cache(char type, char* data, int size, char* cache){
   int ret = ERROR_SUCCESS;

   assert(data);

   // 11 bytes tag header
   /*char tag_header[] = {
   (char)type, // TagType UB [5], 18 = script data
   (char)0x00, (char)0x00, (char)0x00, // DataSize UI24 Length of the message.
   (char)0x00, (char)0x00, (char)0x00, // Timestamp UI24 Time in milliseconds at which the data in this tag applies.
   (char)0x00, // TimestampExtended UI8
   (char)0x00, (char)0x00, (char)0x00, // StreamID UI24 Always 0.
   };*/

   // write data size.
   if ((ret = tag_stream->initialize(cache, 11)) != ERROR_SUCCESS) {
      return ret;
   }
   tag_stream->write_1bytes(type);
   tag_stream->write_3bytes(size);
   tag_stream->write_3bytes(0x00);
   tag_stream->write_1bytes(0x00);
   tag_stream->write_3bytes(0x00);

   return ret;
}
int SrsHttpFlvEncoder::write_audio_to_cache(int64_t timestamp, char* data, int size, char* cache){
   int ret = ERROR_SUCCESS;

   assert(data);

   timestamp &= 0x7fffffff;

   // 11bytes tag header
   /*char tag_header[] = {
   (char)SrsCodecFlvTagAudio, // TagType UB [5], 8 = audio
   (char)0x00, (char)0x00, (char)0x00, // DataSize UI24 Length of the message.
   (char)0x00, (char)0x00, (char)0x00, // Timestamp UI24 Time in milliseconds at which the data in this tag applies.
   (char)0x00, // TimestampExtended UI8
   (char)0x00, (char)0x00, (char)0x00, // StreamID UI24 Always 0.
   };*/

   // write data size.
   if ((ret = tag_stream->initialize(cache, 11)) != ERROR_SUCCESS) {
      return ret;
   }
   tag_stream->write_1bytes(SrsCodecFlvTagAudio);
   tag_stream->write_3bytes(size);
   tag_stream->write_3bytes((int32_t)timestamp);
   // default to little-endian
   tag_stream->write_1bytes((timestamp >> 24) & 0xFF);
   tag_stream->write_3bytes(0x00);

   return ret;
}
int SrsHttpFlvEncoder::write_video_to_cache(int64_t timestamp, char* data, int size, char* cache){
   int ret = ERROR_SUCCESS;

   assert(data);

   timestamp &= 0x7fffffff;

   // 11bytes tag header
   /*char tag_header[] = {
   (char)SrsCodecFlvTagVideo, // TagType UB [5], 9 = video
   (char)0x00, (char)0x00, (char)0x00, // DataSize UI24 Length of the message.
   (char)0x00, (char)0x00, (char)0x00, // Timestamp UI24 Time in milliseconds at which the data in this tag applies.
   (char)0x00, // TimestampExtended UI8
   (char)0x00, (char)0x00, (char)0x00, // StreamID UI24 Always 0.
   };*/

   // write data size.
   if ((ret = tag_stream->initialize(cache, 11)) != ERROR_SUCCESS) {
      return ret;
   }
   tag_stream->write_1bytes(SrsCodecFlvTagVideo);
   tag_stream->write_3bytes(size);
   tag_stream->write_3bytes((int32_t)timestamp);
   // default to little-endian
   tag_stream->write_1bytes((timestamp >> 24) & 0xFF);
   tag_stream->write_3bytes(0x00);

   return ret;
}

int SrsHttpFlvEncoder::write_pts_to_cache(int size, char* cache){
   int ret = ERROR_SUCCESS;

   if ((ret = tag_stream->initialize(cache, SRS_FLV_PREVIOUS_TAG_SIZE)) != ERROR_SUCCESS) {
      return ret;
   }
   tag_stream->write_4bytes(size);

   return ret;
}

int SrsHttpFlvEncoder::write_tag(char* header, int header_size, char* tag, int tag_size){
   int ret = ERROR_SUCCESS;

   // PreviousTagSizeN UI32 Size of last tag, including its header, in bytes.
   char pre_size[SRS_FLV_PREVIOUS_TAG_SIZE];
   if ((ret = write_pts_to_cache(tag_size + header_size, pre_size)) != ERROR_SUCCESS) {
      return ret;
   }

   iovec iovs[3];
   iovs[0].iov_base = header;
   iovs[0].iov_len = header_size;
   iovs[1].iov_base = tag;
   iovs[1].iov_len = tag_size;
   iovs[2].iov_base = pre_size;
   iovs[2].iov_len = SRS_FLV_PREVIOUS_TAG_SIZE;

   if ((ret = this->writer->writev(iovs, 3, NULL)) != ERROR_SUCCESS) {
      return ret;
   }

   return ret;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SrsHttpFlvMuxer::SrsHttpFlvMuxer(MuxerListener *listener, std::string tag, std::string url, LivePushParam *param)
:MuxerInterface(listener, tag),
mUrl(url),
mHasSendHeaders(false),
mHasSendKeyFrame(false),
mHasSendFileHeader(false),
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
   mStopedByCommand = false;
   mHasEverStarted = false;
   mReConnectCount = 0;
   mHasInQueueVideoHeader = false;
   mHasInQueueAudioHeader = false;
   mRemoteIp = "";
   
   LivePushParam * liveParam = mParam;
   int data_size = 0;
   if (liveParam->live_model==LIVE_TYPE_AUDIO_ONLY) {
      data_size = PCM_FRAME_SIZE*liveParam->ch_num*Utility::GetBitNumWithSampleFormat(liveParam->encode_sample_fmt)/8;
   }else{
      data_size = liveParam->width*liveParam->height * 3 / 2;
   }
   mFrameData = (char*)calloc(1, data_size);
   if (mFrameData == NULL) {
      LOGE("mFrameData new error!");
   }

   memset(&mMetaData, 0, sizeof(RTMPMetadata));

   vhall_lock_init(&mMutex);

   mThread = new talk_base::Thread();
   mThread->SetName("SrsHttpFlvMuxer mThread", this);
   mThread->Start();

   mBufferQueue = new SafeDataQueue(this, 0.1, 0.5, DEFULT_BUFFER_LENGTH);
   mBufferQueue->SetFrameDropType(mParam->drop_frame_type);
   mBufferQueue->SetTag("SrsHttpFlvMuxer_Data_Queue");
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
   if (mParam->sample_rate > 0 && mParam->ch_num > 0){
	   audio_frame_duration = 2048 * 1000 * 8 / mParam->sample_rate / 16;
      if (audio_frame_duration <= 0){
         audio_frame_duration = 1;
      }
   }
   else {
      audio_frame_duration = 23;
   }

   mTimeJitter = new TimeJitter(audio_frame_duration, video_frame_duration, 200);

   mEncoder = new SrsHttpFlvEncoder();
   mWriter = NULL;
}

SrsHttpFlvMuxer::~SrsHttpFlvMuxer()
{
   Stop();
   mThread->Clear(this);
   VHALL_THREAD_DEL(mThread);
   VHALL_DEL(mFrameData);
   VHALL_DEL(mBufferQueue);
   VHALL_DEL(mEncoder);
   delete mTimeJitter;
   vhall_lock_destroy(&mMutex);
}

bool SrsHttpFlvMuxer::Init()
{
   Reset(false);
   VhallAutolock _l(&mMutex);
   if (mStartTime == 0){ //first time init.
	   mStartTime = srs_utils_time_ms();
   }

   mLastSpeedUpdateTime = srs_utils_time_ms();

   HttpFlvOpenWrite(mUrl.c_str());

   return true;
}

void SrsHttpFlvMuxer::HttpFlvOpenWrite(const char* url){
   int ret = ERROR_SUCCESS;
   assert(mWriter == NULL);
   mWriter = new SrsAsyncHttpRequest(url, this, mThread);
   mWriter->init(url);
   if (mParam->is_http_proxy){
      ProxyDetail pd = mParam->proxy;
      mWriter->set_proxy(pd.host, pd.port, pd.type, pd.username, pd.password);
   }
   //mWriter->set_proxy("172.20.1.30", 8080, 2, "fuzhuang", "vhall.123");

   mWriter->doConnect();
   mEncoder->initialize(mWriter);

   return;
}
void SrsHttpFlvMuxer::HttpFlvClose(){
   mWriter->stop();
}

bool SrsHttpFlvMuxer::Reset(bool isStop)
{
   Destroy();

   mAsyncStoped = false;
   mCurentSpeed = 0;
   mHeaderWaitCount = 0;

   mHasSendHeaders = false;
   mHasSendKeyFrame = false;

   mStopedByCommand = false;

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

void SrsHttpFlvMuxer::Destroy(){
   VhallAutolock _l(&mMutex);
   if (mWriter){
      //delete(mWriter);
      mWriter->Destroy(true);
      mWriter = NULL;
   }
}

bool SrsHttpFlvMuxer::Start(){
   mStopedByCommand = false;

   // mUrl check
   if (mUrl.empty() || mUrl.length() == 0){
      LOGE("empty http url!");
      return false;
   }
   if (mUrl.find("http://") != 0){
      LOGE("flv url must starts with http://, actual is %s", mUrl.c_str());
      return false;
   }
   if (mUrl.rfind(".flv") != (mUrl.length() - 4)){
      LOGE("flv url must ends with .flv, actual is %s", mUrl.c_str());
      return false;
   }
   mAsyncStoped = false;
   mThread->Post(this, SELF_MSG_START);
   return true;
}

std::list<SafeData*> SrsHttpFlvMuxer::Stop(){
   std::list<SafeData*> list;
   list.clear();
   if (!mThread->started() || (mState == MUXER_STATE_STOPED && mStopedByCommand)){
      return list;
   }
   mState = MUXER_STATE_STOPED;
   mStopedByCommand = true;
   mAsyncStoped = true;
   vhall_lock(&mMutex);
   if (mWriter){
      HttpFlvClose();
   }
   vhall_unlock(&mMutex);
   mThread->Clear(this);
   //save the unsent amf0 data
   list = mBufferQueue->GetListFromQueue(SCRIPT_FRAME);
   mBufferQueue->ClearAllQueue();
   mThread->Post(this, SELF_MSG_STOP);
   return list;
}

void SrsHttpFlvMuxer::Restart(){
   //if (!mThread->started()){
   //   return;
   //}
   //mAsyncStoped = true;
   //vhall_lock(&mMutex);
   //if (mWriter){
   //   HttpFlvClose();
   //}
   //vhall_unlock(&mMutex);

   mThread->Clear(this);
   mBufferQueue->ClearAllQueue();

   //mAsyncStoped = false;
   //if (!mThread->started()) {
   //   mThread->Start();
   //}
   msleep(1000);
   //mThread->Post(this, SELF_MSG_START);
   Init();
}

void SrsHttpFlvMuxer::OnMessage(talk_base::Message* msg){
   switch (msg->message_id) {
   case SELF_MSG_START:
      Init();
      break;
   case SELF_MSG_CONN_SUCC:
      mHasEverStarted = true;
      mState = MUXER_STATE_STARTED;
      // send file header firstly
      SendFlvFileHeaders();

      if (mState == MUXER_STATE_RECONNECTING){
         mReConnectCount = 0;  //reset reconnect count.
         if (!mHasEverStarted){
            mMuxerEvent.mDesc = "SrsHttpFlvMuxer init success";
            ReportMuxerEvent(MUXER_MSG_START_SUCCESS, &mMuxerEvent);
         }
         else{
            mMuxerEvent.mDesc = "SrsHttpFlvMuxer need new key frame";
            ReportMuxerEvent(MUXER_MSG_NEW_KEY_FRAME, &mMuxerEvent);
         }
         LOGE("SrsHttpFlvMuxer reconnect  success");
      }
      else{
         mMuxerEvent.mDesc = "SrsHttpFlvMuxer init success";
         ReportMuxerEvent(MUXER_MSG_START_SUCCESS, &mMuxerEvent);
      }

      if (!mAsyncStoped){ //if not stop keep sending
         mThread->Post(this, SELF_MSG_SEND);
      }
      break;

   case SELF_MSG_CONN_RC:
      if (mState == MUXER_STATE_STOPED && mStopedByCommand)
         break;
      mReConnectCount++;
      mMuxerEvent.mDesc = "SrsHttpFlvMuxer reconnecting times=";
      mMuxerEvent.mDesc += int2str(mReConnectCount);
      ReportMuxerEvent(MUXER_MSG_RECONNECTING, &mMuxerEvent);
      mState = MUXER_STATE_RECONNECTING;
      LOGE("SrsHttpFlvMuxer reconnecting mReConnectCount=%d", mReConnectCount);
      
      if (mReConnectCount >= mParam->publish_reconnect_times){
         //last time reconnect failed, report faild
         if (!mHasEverStarted){ //never started
            mMuxerEvent.mDesc = "SrsHttpFlvMuxer init falid";
            ReportMuxerEvent(MUXER_MSG_START_FAILD, &mMuxerEvent);
         }
         else {
            mMuxerEvent.mDesc = "SrsHttpFlvMuxer send or connect faild";
            ReportMuxerEvent(MUXER_MSG_DUMP_FAILD, &mMuxerEvent);
         }
         LOGE("SrsHttpFlvMuxer reconnecting fiaild have tried=%d", mReConnectCount);
      }
      else {
         Restart();
      }
      
      break;
   case SELF_MSG_SEND:
      if (!mAsyncStoped){
         if (Sending())
            mThread->Post(this, SELF_MSG_SEND);
         else
            mThread->Post(this, SELF_MSG_CONN_RC);
      }
      break;
   case SELF_MSG_STOP:
      Reset();
      mState = MUXER_STATE_STOPED;
      mStopedByCommand = true;
      break;
   default:
      break;
   }
   delete msg->pdata;
   msg->pdata = NULL;
}

int SrsHttpFlvMuxer::HttpFlvWriteHeader(char header[9]){
   int ret = ERROR_SUCCESS;

   if (!mWriter->is_open()) {
      return -1;
   }

   if ((ret = mEncoder->write_header(header)) != ERROR_SUCCESS) {
      return ret;
   }

   return ret;
}
int SrsHttpFlvMuxer::HttpFlvWriteTag(char type, int32_t time, char* data, int size){
   int ret = ERROR_SUCCESS;

   if (!mWriter->is_open()){
      free(data);
      return -1;
   }

   if (type == SRS_RTMP_TYPE_AUDIO) {
      return mEncoder->write_audio(time, data, size);
   }
   else if (type == SRS_RTMP_TYPE_VIDEO) {
      return mEncoder->write_video(time, data, size);
   }
   else {
      return mEncoder->write_metadata(type, data, size);
   }

   return ret;
}

int SrsHttpFlvMuxer::GetKeyFrameCount(){
   return mBufferQueue->GetKeyFrameCount();
}

int SrsHttpFlvMuxer::GetQueueDataSize()
{
   return mBufferQueue->GetQueueDataSize();
}
uint32_t SrsHttpFlvMuxer::GetQueueDataDuration()
{
   return mBufferQueue->GetQueueDataDuration();
}

int SrsHttpFlvMuxer::GetQueueSize(){
   return mBufferQueue->GetQueueSize();
}

int SrsHttpFlvMuxer::GetMaxNum(){
   return mBufferQueue->GetMaxNum();
}


bool SrsHttpFlvMuxer::Sending(){
   bool ret = true;

   SafeData *frame = mBufferQueue->ReadQueue(true);
   if (frame == NULL) {
      return false;
   }
   ret = Publish(frame);
   frame->SelfRelease();
   return ret;
}

bool SrsHttpFlvMuxer::Publish(SafeData *frame)
{
   const char * data = frame->mData;
   int size = frame->mSize;
   int type = frame->mType;
   uint32_t timestamp = frame->mTs;
   LivePushParam * live_param = mParam;

   // send A/V sequence header

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
      if (type != VIDEO_HEADER&&type != AUDIO_HEADER) {
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
	   }
	   else {
         mHeaderWaitCount++;
		   //type is not header if header have sent header (reconnect header have got before) 
		   //we need to send this frame
         LOGW("Audio and Video first two frame is not audio header or video header!");
		   if (!ret){ //send header faild
			   return false;
		   }
		   else if (ret && !mHasSendHeaders){ //wait for header
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
         mAudioHeader->SelfRelease();
         mAudioHeader = frame->SelfCopy();
      }
      if (type == VIDEO_HEADER){
         mVideoHeader->SelfRelease();
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
   
   uint32_t ts = 0;
   if (type != SCRIPT_FRAME) {
      ts = mTimeJitter->GetCorretTime(jitter_type, timestamp);
   }

   if (type == AUDIO_FRAME){
      if (false == SendAudioPacket(const_cast<char *>(data), size, ts)) {
         LOGE("Write AUDIO Frame error");
         //pthread_mutex_unlock(&mMutex);
         return false;
      }
      LOGI("AUDIO_A_FRAME timestamp:%d MS", ts);
   }else if(type == SCRIPT_FRAME){
      bool ret = SendPacket(SRS_RTMP_TYPE_SCRIPT, timestamp, const_cast<char *>(data), size);
      if (false == ret ) {
         LOGE("Send Amf0 msg error!");
         //pthread_mutex_unlock(&mMutex);
         return ret;
      }
      LOGD("send Amf0 msg size:%d ts:%d",size,timestamp);
   }else{
      bool is_key = false;
      int  nalu_title = 0;

      if (size > 3 && data[0] == 0 && data[1] == 0 && data[2] == 1){
         nalu_title = 3;
      }
      else if (size > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1){
         nalu_title = 4;
      }
      else{
         nalu_title = 0;
      }
      if (type == VIDEO_I_FRAME){
         is_key = true;
      }

      if (false == SendH264Packet(const_cast<char *>(data + nalu_title), size - nalu_title, is_key, ts)) {
         LOGE("Write H264 Frame error");
         return false;
      }
      if (!mHasSendKeyFrame && is_key){
         mHasSendKeyFrame = true;
      }
      LOGI("VIDEO_%s_FRAME type=%d timestamp:%llu MS", type==VIDEO_I_FRAME?"I":"P", ts);
   }
   return true;
}

bool SrsHttpFlvMuxer::SendFlvFileHeaders(){
   char flv_file_header[9] = { 0 };
   flv_file_header[0] = 'F';
   flv_file_header[1] = 'L';
   flv_file_header[2] = 'V';
   flv_file_header[3] = 0x01;  //File version  1 Bytes
   if (mParam->live_model == LIVE_TYPE_AUDIO_ONLY){
      flv_file_header[4] = 0x04; //00000100 only audio
   }
   else if (mParam->live_model == LIVE_TYPE_VIDEO_ONLY){
      flv_file_header[4] = 0x01; //00000001 only video
   }
   else{
      flv_file_header[4] = 0x05; //00000101 video and audio
   }
   flv_file_header[8] = 0x09; //The Length of this header 4 Bytes flv_file_header[5~8]
   if (this->HttpFlvWriteHeader(flv_file_header) != 0){
      LOGE("flv file header write fail!");
      return false;
   }
   mHasSendFileHeader = true;

   return true;
}

bool SrsHttpFlvMuxer::SendHeaders()
{
   //RTMPMetadata        metaData;//meta¬†Àù√¶‚Ä?
   //memset(&metaData, 0, sizeof(RTMPMetadata));
   //LiveParam * liveParam = mParam;

   //repire this after when stop.
   mMetaData.nFileSize = 0;
   mMetaData.nDuration = 0;

   if (mParam->live_model == LIVE_TYPE_AUDIO_ONLY){
      mMetaData.bHasVideo = false;
      mMetaData.bHasAudio = true;
      mMetaData.nAudioSampleRate = mParam->sample_rate;
      mMetaData.nAudioSampleSize = mParam->audio_bitrate;
      mMetaData.nAudioChannels = mParam->ch_num;

      if (false == SendMetadata(&mMetaData, 0)) {
         LOGE("flv file Meta data write fail!");
         return false;
      }
      if (false == SendAudioInfoData()) {
         LOGE("flv file AudioInfo data write fail!");
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
      else {
         mMetaData.bHasAudio = true;
         mMetaData.nAudioSampleRate = mParam->sample_rate;
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
//      {
//         int ret = 0;
//         SrsCodecSample codecSample;
//         SrsAvcAacCodec avcAacCodec;
//         char body[1024] = {0};
//         int dataSize = 0;
//         SetPpsAndSpsData(&mMetaData, body, &dataSize);
//         if ((ret = avcAacCodec.video_avc_demux(body, dataSize, &codecSample)) != 0) {
//            LOGE("hls codec demux video failed. ret=%d", ret);
//            //delete avcAacCodec;
//            return false;
//         }
//         mMetaData.nWidth = avcAacCodec.width;
//         mMetaData.nHeight = avcAacCodec.height;
//      }
      LOGI("in rtmppublisher, metaData.nWidth = %d, metaData.nHeight = %d, metaData.nFrameRate=%d",
         mMetaData.nWidth, mMetaData.nHeight, mMetaData.nFrameRate);

      if (false == SendMetadata(&mMetaData, 0)){
         LOGE("flv file SendMetadata data write fail!");
         return false;
      }
      if (mParam->live_model != LIVE_TYPE_VIDEO_ONLY){
         if (false == SendAudioInfoData()) {
            LOGE("flv file AudioInfo data write fail!");
            return false;
         }
      }

      if (false == SendPpsAndSpsData(&mMetaData, 0)) {
         LOGE("flv file PpsAndSps data write fail!");
         return false;
      }
   }
   mHasSendHeaders = true;
   return true;
}

bool SrsHttpFlvMuxer::SendMetadata(LPRTMPMetadata lpMetaData, uint32_t timestamp)
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

      srs_amf0_t str3Amf0 = srs_amf0_create_string("vhall");
      srs_amf0_object_property_set(objectAmf0, "copyright", str3Amf0);

      obSize = srs_amf0_size(objectAmf0);
      srs_amf0_serialize(objectAmf0, data, obSize);
      srs_amf0_free(objectAmf0);
   }
   //mMetaDataOffset = srs_flv_tellg(pFlv);
   return SendPacket(SRS_RTMP_TYPE_SCRIPT, timestamp, medateData, obSize + mdSize + dfSize);
}

bool SrsHttpFlvMuxer::SendAudioInfoData()
{
   assert(mAudioHeader != NULL);
   LivePushParam * param = mParam;
   int sampleNum = Utility::GetNumFromSamplingRate(param->sample_rate);
   int chnnelNum = param->ch_num == 1 ? 0 : 1;
   //
   char audioHeader[128] = { 0 };
   audioHeader[0] = 0xAC | (1 << 1) | (chnnelNum << 0);//
   audioHeader[1] = 0x00;

   memcpy(audioHeader + 2, mAudioHeader->mData, mAudioHeader->mSize);

   return SendPacket(SRS_RTMP_TYPE_AUDIO, 0, audioHeader, sizeof(audioHeader));
}

bool SrsHttpFlvMuxer::SendPpsAndSpsData(LPRTMPMetadata lpMetaData, uint32_t timestamp)
{
   char data[1024] = { 0 };
   int dataSize = 0;
   SetPpsAndSpsData(lpMetaData, data, &dataSize);
   return SendPacket(SRS_RTMP_TYPE_VIDEO, timestamp, data, dataSize);
}

bool SrsHttpFlvMuxer::SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData, int * dataSize){

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

void SrsHttpFlvMuxer::UpdataSpeed(){
   if (mLastSpeedUpdateTime == 0){ //not start
      return;
   }
   int64_t duration = 0;
   uint64_t last_speed_time = mLastSpeedUpdateTime;
   uint64_t curent_time = srs_utils_time_ms();
   duration = curent_time - last_speed_time;
   if (duration > DEFULT_SPEED_COUNT_SIZE){
      mLastSpeedUpdateTime = curent_time;
      mCurentSpeed = (mCurentSendBytes - mLastSendBytes) * 8 / duration;
      mLastSendBytes = (long long)mCurentSendBytes;
   }
}

bool SrsHttpFlvMuxer::SendH264Packet(char *data, long size, bool bIsKeyFrame, unsigned int nTimeStamp)
{
   if (NULL == data || NULL == mEncoder || NULL == mWriter) {
      LOGE("!bad data");
      return false;
   }
   char *body = mFrameData;
   int i = 0;
   if (bIsKeyFrame)
   {
      body[i++] = 0x17;
   }
   else
   {
      body[i++] = 0x27;
   }
   body[i++] = 0x01;// AVC NALU
   body[i++] = 0x00;
   body[i++] = 0x00;
   body[i++] = 0x00;
   // NALU size
   body[i++] = size >> 24;
   body[i++] = size >> 16;
   body[i++] = size >> 8;
   body[i++] = size & 0xff;
   // NALU data
   memcpy(&body[i], data, size);

   mSendFrameCount++;

   return SendPacket(SRS_RTMP_TYPE_VIDEO, nTimeStamp, body, (unsigned int)(size + i));
}

bool SrsHttpFlvMuxer::SendAudioPacket(char *data, int size, int nTimeStamp)
{
   if (NULL == data || NULL == mEncoder || NULL == mWriter) {
      LOGE("!bad data");
      return false;
   }
   char *body = mFrameData;
   int i = 0;
   body[i++] = 0xAF;
   body[i++] = 0x01;
   // NALU data
   memcpy(&body[i], data, size);

   return SendPacket(SRS_RTMP_TYPE_AUDIO, nTimeStamp, body, size + i);
}

bool SrsHttpFlvMuxer::SendPacket(char type, uint32_t timestamp, char* data, int size)
{
   bool ret = false;

   if (NULL == data || NULL == mEncoder || NULL == mWriter) {
      LOGE("!bad data");
      return ret;
   }
   char * pushData = (char*)calloc(1, size);
   memcpy(pushData, data, size);

   if (this->HttpFlvWriteTag(type, timestamp, pushData, size) == 0){
      ret = true;
   }
   else{
      ret = false;
   }

   mCurentSendBytes += size;

   return ret;
}


AVHeaderGetStatues SrsHttpFlvMuxer::GetAVHeaderStatus(){

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

bool SrsHttpFlvMuxer::PushData(SafeData *data){
   if (data->mType == VIDEO_HEADER){
      mHasInQueueVideoHeader = true;
   }
   if (data->mType == AUDIO_HEADER){
      mHasInQueueAudioHeader = true;
   }
   return mBufferQueue->PushQueue(data, BUFFER_FULL_ACTION::ACTION_DROP_FAILD_LARGE);
}

std::string SrsHttpFlvMuxer::GetDest(){
   //VhallAutolock _l(&mMutex);
   return "127.0.0.1";
}

int SrsHttpFlvMuxer::GetState()
{
   return mState;
}

int SrsHttpFlvMuxer::GetDumpSpeed(){
   UpdataSpeed();
   return (int)mCurentSpeed;
}

const VHMuxerType SrsHttpFlvMuxer::GetMuxerType(){
   return HTTP_FLV_MUXER;
}

bool SrsHttpFlvMuxer::LiveGetRealTimeStatus(VHJson::Value & value){
   value["Name"] = VHJson::Value("SrsHttpFlvMuxer");
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

void SrsHttpFlvMuxer::OnSafeDataQueueChange(SafeDataQueueState state, std::string tag){
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

int SrsHttpFlvMuxer::ReportMuxerEvent(int type, MuxerEventParam *param){
   if (mAsyncStoped/* &&(type == MUXER_MSG_START_FAILD || type == MUXER_MSG_DUMP_FAILD)*/){
      return 0;
   }
   return MuxerInterface::ReportMuxerEvent(type, param);
}
