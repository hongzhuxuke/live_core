#include "srs_flv_recorder.h"
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
#include <sys/stat.h>

#define DEFULT_BUFFER_LENGTH   600
#define DEFULT_PUBLISH_TIME_OUT 5000
#define DEFULT_SPEED_COUNT_SIZE 1000

SrsFlvRecorder::SrsFlvRecorder(MuxerListener *listener, std::string tag, std::string path, LivePushParam *param)
	:MuxerInterface(listener, tag),
	mPath(path),
	mFlv(NULL),
	mHasSendFileHeaders(false),
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
	mCurentSpeed = 0;
	mState = MUXER_STATE_STOPED;
	mAsyncStoped = false;
	mSendFrameCount = 0;
	mHasInQueueVideoHeader = false;
	mHasInQueueAudioHeader = false;
	mMetaDataOffset = -1;
   
	//≥ı ºªØ mFrameData
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
	mThread->SetName("SrsFlvRecorder->mThread", this);
	mThread->Start();
	mBufferQueue = new SafeDataQueue(this, 0.1, 0.9, DEFULT_BUFFER_LENGTH);
	mBufferQueue->SetFrameDropType(mParam->drop_frame_type);
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

SrsFlvRecorder::~SrsFlvRecorder()
{
	Stop();
   VHALL_THREAD_DEL(mThread);
   VHALL_DEL(mBufferQueue);
	VHALL_DEL(mTimeJitter);
	VHALL_DEL(mFrameData);
	vhall_lock_destroy(&mMutex);
}

bool SrsFlvRecorder::Start(){
	mAsyncStoped = false;
	mThread->Post(this, SELF_MSG_START);
	return true;
}

std::list<SafeData*> SrsFlvRecorder::Stop(){
   std::list<SafeData*> list;
   list.clear();
	mAsyncStoped = true;
	mThread->Clear(this);
   mBufferQueue->ClearAllQueue();
	mThread->Post(this, SELF_MSG_STOP);
   mState = MUXER_STATE_STOPED;
   return list;
}

AVHeaderGetStatues SrsFlvRecorder::GetAVHeaderStatus(){
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

bool SrsFlvRecorder::PushData(SafeData *data){
	if (data->mType == VIDEO_HEADER){
		mHasInQueueVideoHeader = true;
	}
	if (data->mType == AUDIO_HEADER){
		mHasInQueueAudioHeader = true;
	}
	return mBufferQueue->PushQueue(data, BUFFER_FULL_ACTION::ACTION_DROP_FAILD_LARGE);
}

std::string SrsFlvRecorder::GetDest(){

	//VhallAutolock _l(&mMutex);
	return mPath;
}

int SrsFlvRecorder::GetState()
{
	return mState;
}

int SrsFlvRecorder::GetDumpSpeed()
{
	return (int)mCurentSpeed;
}

const VHMuxerType SrsFlvRecorder::GetMuxerType(){
   return FILE_FLV_MUXER;
}

void SrsFlvRecorder::OnMessage(talk_base::Message* msg){
	switch (msg->message_id) {
	case SELF_MSG_START:
	{
		if (!Init()) {
			mMuxerEvent.mDesc = ""; //TODO°°get error desc
			ReportMuxerEvent(MUXER_MSG_START_FAILD, &mMuxerEvent);
		}
		else {
			mMuxerEvent.mDesc = "";
			ReportMuxerEvent(MUXER_MSG_START_SUCCESS, &mMuxerEvent);
			mState = MUXER_STATE_STARTED;
			mThread->Post(this, SELF_MSG_SEND);
		}
	}
		break;
	case SELF_MSG_SEND:
		if (!Sending()){
			mMuxerEvent.mDesc = "";
			ReportMuxerEvent(MUXER_MSG_DUMP_FAILD, &mMuxerEvent);
		}
		if (!mAsyncStoped){ //if not stop keep sending
			mThread->Post(this, SELF_MSG_SEND);
		}
		break;
	case SELF_MSG_STOP:
		RepairMetaData();
		Reset();
		mState = MUXER_STATE_STOPED;
		break;
	default:
		break;
	}
	delete msg->pdata;
	msg->pdata = NULL;
}

bool SrsFlvRecorder::Sending(){
	SafeData *frame = mBufferQueue->ReadQueue(true);
	if (frame == NULL) {
		return false;
	}
	bool ret = Publish(frame);
	frame->SelfRelease();
	return ret;
}

bool SrsFlvRecorder::LiveGetRealTimeStatus(VHJson::Value & value){

	value["Name"] = VHJson::Value("SrsFlvRecorder");
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

void SrsFlvRecorder::OnSafeDataQueueChange(SafeDataQueueState state, std::string tag){
	//TODO report or not 
	if (state == SAFE_DATA_QUEUE_STATE_FULL){
		mMuxerEvent.mDesc = "buffer full";
		ReportMuxerEvent(MUXER_MSG_BUFFER_FULL, &mMuxerEvent);
	}
	else if (state == SAFE_DATA_QUEUE_STATE_EMPTY){
		mMuxerEvent.mDesc = "buffer empty";
		ReportMuxerEvent(MUXER_MSG_BUFFER_EMPTY, &mMuxerEvent);
	}
	else if (state == SAFE_DATA_QUEUE_STATE_NORMAL){
		mMuxerEvent.mDesc = "buffer normal";
		ReportMuxerEvent(MUXER_MSG_BUFFER_NORMAL, &mMuxerEvent);
	}
}

int SrsFlvRecorder::ReportMuxerEvent(int type, MuxerEventParam *param){
	if (mAsyncStoped/* &&(type == MUXER_MSG_START_FAILD || type == MUXER_MSG_DUMP_FAILD)*/){
		return 0;
	}
	return MuxerInterface::ReportMuxerEvent(type, param);
}
/*here need to be awared:
we only lock mMutex at Init and Destory, which will create and delete mFlv.
because all things in mFlv is will not change except Send_bytes which have
changed to  atomic_uint64_t°£
so the mMutex is only to protect the mFlv point not it content.

self thread will writ mFlv in init and Destory°£
and all other private thread are called by self thread°£

outside thread will read mFlv in Stop and Getip, outside thread
will not wri te mFlv

so we only put 4 lock in Init Destroy Getip and Stop°£
*/
bool SrsFlvRecorder::Init()
{
	VhallAutolock _l(&mMutex);
	mStartTime = mLastSpeedUpdateTime = srs_utils_time_ms();
	mLastSendBytes = 0;
	mCurentSendBytes = 0;
	mCurentSpeed = 0;
	mSendFrameCount = 0;
	Destroy();
   if (PathExists(mPath.c_str())) {
      mHasSendFileHeaders = true;
      mFlv = srs_flv_open_append_write(mPath.c_str());
   }else{
      mFlv = srs_flv_open_write(mPath.c_str());
   }
	if (mFlv == NULL) {
		LOGE("srs_flv_open_write failed.");
		return false;
	}
	return true;
}

bool SrsFlvRecorder::Reset()
{
	Destroy();
	mStartTime = 0;
	mLastSpeedUpdateTime = 0;
	mLastSendBytes = 0;
	mCurentSendBytes = 0;
	mCurentSpeed = 0;

	mAsyncStoped = false;
	mSendFrameCount = 0;
	mHasSendFileHeaders = false;
	mHasSendHeaders = false;
	mHasSendKeyFrame = false;

	mHasInQueueVideoHeader = false;
	mHasInQueueAudioHeader = false;

	if (mVideoHeader){
		mVideoHeader->SelfRelease();
		mVideoHeader = NULL;
	}
	if (mAudioHeader){
		mAudioHeader->SelfRelease();
		mAudioHeader = NULL;
	}
	//reset time jitter.
	//mTimeJitter->Reset();
	memset(&mMetaData, 0, sizeof(RTMPMetadata));
	mBufferQueue->Reset();
   LOGW("SrsFlvRecorder::Reset()");
	return true;
}

void SrsFlvRecorder::Destroy(){
	VhallAutolock _l(&mMutex);
	if (mFlv){
		srs_flv_close(mFlv);
		mFlv = NULL;
	}
}

bool SrsFlvRecorder::Publish(SafeData *frame){
	const char * data = frame->mData;
	int size = frame->mSize;
	int type = frame->mType;
	uint64_t timestamp = frame->mTs;
	LivePushParam * live_param = mParam;

   if (type == SCRIPT_FRAME) {
      return true;
   }

	if (!mHasSendHeaders){
		if (live_param->live_model == LIVE_TYPE_AUDIO_ONLY){
			if (type == AUDIO_HEADER){
				if (mAudioHeader){
					mAudioHeader->SelfRelease();
				}
				mAudioHeader = frame->SelfCopy();
				return WriteHeaders();
			}
			LOGW("Audio Only first frame is not audio header!");
		}else if (live_param->live_model == LIVE_TYPE_VIDEO_ONLY){
			if (type == VIDEO_HEADER){

				if (mVideoHeader){
					mVideoHeader->SelfRelease();
				}
				mVideoHeader = frame->SelfCopy();

				return WriteHeaders();
			}
			LOGW("Video Only first frame is not video header!");
		}
		else //live_param->live_publish_model == LIVE_PUBLISH_TYPE_VIDEO_AND_AUDIO
		{
			if (type == AUDIO_HEADER || type == VIDEO_HEADER) {
				if (type == AUDIO_HEADER){
					if (mAudioHeader){
						mAudioHeader->SelfRelease();
					}
					mAudioHeader = frame->SelfCopy();
				}else{
					if (mVideoHeader){
						mVideoHeader->SelfRelease();
					}
					mVideoHeader = frame->SelfCopy();
				}
			}else{
				LOGW("Audio and Video first two frame is not audio header or video header!");
			}
			if (mAudioHeader && mVideoHeader){
				return WriteHeaders();
			}
		}
		return true;
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
      return WriteHeaders();
   }
   
	if (!mHasSendKeyFrame && live_param->live_model != LIVE_TYPE_AUDIO_ONLY){
		if (type > VIDEO_I_FRAME || type == AUDIO_FRAME) { //wait to send key frame.
         LOGW("wait to send key frame.");
			return true;
		}
	}

	JitterFrameType jitter_type;
	if (type == AUDIO_FRAME){
		jitter_type = JITTER_FRAME_TYPE_AUDIO;
	}
	else {
		jitter_type = JITTER_FRAME_TYPE_VIDEO;
	}

	uint64_t ts = mTimeJitter->GetCorretTime(jitter_type, timestamp);

	if (type == AUDIO_FRAME){
		if (false == WriteAudioPacket(mFlv, const_cast<char *>(data), size, ts)) {
			LOGE("Write AUDIO Frame error");
			//pthread_mutex_unlock(&mMutex);
			return false;
		}
		LOGI("AUDIO_A_FRAME timestamp:%llu MS", ts);
   }else{
		bool is_key = false;
		int  nalu_title = 0;

		if (size > 3 && data[0] == 0 && data[1] == 0 && data[2] == 1){
			nalu_title = 3;
		}else if (size > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1){
			nalu_title = 4;
		}
		else{
			nalu_title = 0;
		}
		if (type == VIDEO_I_FRAME){
			is_key = true;
		}

		if (false == WriteH264Packet(mFlv, const_cast<char *>(data + nalu_title), size - nalu_title, is_key, ts)) {
			LOGE("Write H264 Frame error");
			return false;
		}
		if (!mHasSendKeyFrame && is_key){
			mHasSendKeyFrame = true;
		}
      LOGI("Flv Recorder VIDEO_%s_FRAME size:%d timestamp:%llu MS", type==VIDEO_I_FRAME?"I":"P", size, ts);
	}

	return true;
}

bool SrsFlvRecorder::WriteHeaders()
{
	memset(&mMetaData, 0, sizeof(RTMPMetadata));

	//write file header
	if (!mHasSendFileHeaders) {
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
		}else{
			flv_file_header[4] = 0x05; //00000101 video and audio
		}
		flv_file_header[8] = 0x09; //The Length of this header 4 Bytes flv_file_header[5~8]
		if (srs_flv_write_header(mFlv, flv_file_header) != 0){
			LOGE("flv file header write fail!");
			return false;
		}
		mHasSendFileHeaders = true;
	}

	//repire this after when stop.
	mMetaData.nFileSize = 0;
	mMetaData.nDuration = 0;

	if (mParam->live_model == LIVE_TYPE_AUDIO_ONLY){
		mMetaData.bHasVideo = false;
		mMetaData.nAudioSampleRate = mParam->dst_sample_rate;
		mMetaData.nAudioSampleSize = mParam->audio_bitrate;
		mMetaData.nAudioChannels = mParam->ch_num;

		if (false == WriteMetadata(mFlv, &mMetaData, 0)) {
			LOGE("flv file Meta data write fail!");
			return false;
		}
		if (false == WriteAudioInfoData()) {
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
		}else {
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

		if (false == WriteMetadata(mFlv, &mMetaData, 0)){
			LOGE("flv file WriteMetadata data write fail!");
			return false;
		}
		if (mParam->live_model != LIVE_TYPE_VIDEO_ONLY){
			if (false == WriteAudioInfoData()) {
				LOGE("flv file AudioInfo data write fail!");
				return false;
			}
		}

		if (false == WritePpsAndSpsData(mFlv, &mMetaData, 0)) {
			LOGE("flv file PpsAndSps data write fail!");
			return false;
		}
	}
	mHasSendHeaders = true;
	return true;
}

bool SrsFlvRecorder::WriteMetadata(srs_flv_t pFlv, LPRTMPMetadata lpMetaData, uint64_t timestamp)
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
		//TODO°° must change to real duration and filesize
		srs_amf0_t num1Amf0 = srs_amf0_create_number(lpMetaData->nDuration);
		srs_amf0_object_property_set(objectAmf0, "duration", num1Amf0);

		srs_amf0_t num2Amf0 = srs_amf0_create_number(lpMetaData->nFileSize);
		srs_amf0_object_property_set(objectAmf0, "filesize", num2Amf0);

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
		for (auto it = mParam->extra_metadata.begin(); it != mParam->extra_metadata.end(); it++){
			srs_amf0_t item = srs_amf0_create_string(it->second.c_str());
			srs_amf0_object_property_set(objectAmf0, it->first.c_str(), item);
		}

		srs_amf0_t str3Amf0 = srs_amf0_create_string("vhall");
		srs_amf0_object_property_set(objectAmf0, "copyright", str3Amf0);

		obSize = srs_amf0_size(objectAmf0);
		srs_amf0_serialize(objectAmf0, data, obSize);
		srs_amf0_free(objectAmf0);
	}
	mMetaDataOffset = srs_flv_tellg(pFlv);
	return WritePacket(pFlv, SRS_RTMP_TYPE_SCRIPT, timestamp, medateData, obSize + mdSize + dfSize);
}

bool SrsFlvRecorder::WriteAudioInfoData()
{
	assert(mAudioHeader != NULL && mFlv != NULL);
	LivePushParam * param = mParam;
	//int sampleNum = Utility::GetNumFromSamplingRate(param->dst_sample_rate);
	int chnnelNum = param->ch_num == 1 ? 0 : 1;
	//
	char audioHeader[128] = { 0 };
	audioHeader[0] = 0xAC | (1 << 1) | (chnnelNum << 0);//
	audioHeader[1] = 0x00;

	memcpy(audioHeader + 2, mAudioHeader->mData, mAudioHeader->mSize);

	return WritePacket(mFlv, SRS_RTMP_TYPE_AUDIO, 0, audioHeader, sizeof(audioHeader));
}

bool SrsFlvRecorder::WritePpsAndSpsData(srs_flv_t pFlv, LPRTMPMetadata lpMetaData, uint64_t timestamp)
{
	char data[1024] = { 0 };
	int dataSize = 0;
	SetPpsAndSpsData(lpMetaData, data, &dataSize);
	return WritePacket(pFlv, SRS_RTMP_TYPE_VIDEO, timestamp, data, dataSize);
}

bool SrsFlvRecorder::SetPpsAndSpsData(LPRTMPMetadata lpMetaData, char*spsppsData, int * dataSize){

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

void SrsFlvRecorder::UpdataSpeed(){
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
		mLastSendBytes = mCurentSendBytes*1;
	}
}

void SrsFlvRecorder::RepairMetaData(){
	if (mFlv){
		uint64_t duration = mTimeJitter->GetLastRetTs() / 1000;
		uint64_t file_size = srs_flv_tellg(mFlv);
		mMetaData.nFileSize = file_size;
		mMetaData.nDuration = duration;
		//seek back to mMetaData. 13 byte header
		srs_flv_lseek(mFlv, 13);
		WriteMetadata(mFlv, &mMetaData, 0);
      srs_flv_lseek(mFlv, file_size);
	}
}

bool SrsFlvRecorder::PathExists(const std::string& path){
   struct stat st;
   // stat current dir, if exists, return error.
   if (stat(path.c_str(), &st) == 0) {
      return true;
   }
   return false;
}

bool SrsFlvRecorder::WritePacket(srs_flv_t pFlv, char type, uint64_t timestamp, char* data, int size)
{
	bool ret = false;

	if (NULL == data || NULL == pFlv) {
		LOGE("!pFlv");
		return ret;
	}
	//char * pushData = (char*)calloc(1, size);
	//memcpy(pushData, data, size);

	if (srs_flv_write_tag(pFlv, type, (uint32_t)timestamp, data, size) == 0){
		ret = true;
	}
	else{
		ret = false;
	}
	mCurentSendBytes = srs_flv_tellg(pFlv);
	//UpdataSpeed();

	return ret;
}

bool SrsFlvRecorder::WriteH264Packet(srs_flv_t pFlv, char *data, long size, bool bIsKeyFrame, uint64_t nTimeStamp)
{
	if (NULL == data || NULL == pFlv) {
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
	return WritePacket(pFlv, SRS_RTMP_TYPE_VIDEO, nTimeStamp, body, (unsigned int)(size + i));
}

bool SrsFlvRecorder::WriteAudioPacket(srs_flv_t pFlv, char *data, int size, uint64_t nTimeStamp)
{
	if (NULL == data || NULL == pFlv)
	{
		return false;
	}
	char *body = mFrameData;
	int i = 0;
	body[i++] = 0xAF;
	body[i++] = 0x01;
	// NALU data
	memcpy(&body[i], data, size);

	return WritePacket(pFlv, SRS_RTMP_TYPE_AUDIO, nTimeStamp, body, size + i);
}
