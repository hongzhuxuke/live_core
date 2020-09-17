#include "http_flv_demuxer.h"
#include "../common/vhall_log.h"
#include "../api/live_interface.h"
#include "../os/vhall_monitor_log.h"
#include "../rtmpplayer/vhall_live_player.h"
#include "talk/base/thread.h"
#include "../common/auto_lock.h"

#define MAX_ENCODED_PKT_NUM     100
#define DEFULT_PUBLISH_TIME_OUT 5000

#define ERROR_SUCCESS 0

HttpFlvDemuxer::HttpFlvDemuxer(VhallPlayerInterface*player):
mWorkerThread(NULL),
mComputeSpeedThread(NULL),
mPlayer(player),
mFlvTagDemuxer(NULL),
mClient(NULL),
mDataStream(NULL),
mParam(NULL){
   mTotalSize = 0;
   mConnectTimeout = 0;
   mStart = false;
   mGotAudioPkt = false;
   mGotVideoKeyFrame = false;
   vhall_lock_init(&mDeatoryRtmpMutex);
   Init();

   mGotHttpHeader = false;
   mGotFlvFileHeader = false;
}

HttpFlvDemuxer::~HttpFlvDemuxer() {
   Stop();
   Destory();
   VHALL_DEL(mFlvTagDemuxer);
   vhall_lock_destroy(&mDeatoryRtmpMutex);
}

void HttpFlvDemuxer::SetParam(LivePlayerParam *param){
   mParam = param;
}

void HttpFlvDemuxer::AddMediaInNotify(IMediaNotify* mediaNotify) {
   mMediaOutputNotify.push_back(mediaNotify);
}

void HttpFlvDemuxer::ClearMediaInNotify(){
   mMediaOutputNotify.clear();
}

void HttpFlvDemuxer::Init(){
   mComputeSpeedThread = new talk_base::Thread();
   if (mComputeSpeedThread == NULL) {
      LOGE("mComputeSpeedThread is NULL");
      return;
   }
   mWorkerThread = new talk_base::Thread();
   if (mWorkerThread == NULL) {
      LOGE("mWorkerThread is NULL");
      return;
   }
   mWorkerThread ->Start();
   mComputeSpeedThread->Start();
}

void HttpFlvDemuxer::Destory(){
   VhallAutolock lock(&mDeatoryRtmpMutex);
   VHALL_THREAD_DEL(mWorkerThread);
   VHALL_THREAD_DEL(mComputeSpeedThread);
}

void HttpFlvDemuxer::DestoryClient()
{
   VhallAutolock lock(&mDeatoryRtmpMutex);
   mGotHttpHeader = false;
   mGotFlvFileHeader = false;

   if (mDataStream){
	   mDataStream->Close();
	   delete mDataStream;
	   mDataStream = NULL;
   }
   VHALL_DEL(mClient);
   //VHALL_DEL(mDataStream);
   mTagBuffer.clear();
}

void HttpFlvDemuxer::Stop() {
   if(mStart == false)
      return ;
   mStart = false;
   mComputeSpeedThread->Clear(this);
   mWorkerThread->Clear(this);
   mWorkerThread->Post(this, MSG_RTMP_Close);
   LOGI("close RTMP connect");
}

void HttpFlvDemuxer::Start(const char *url) {
   mStart = true;
   mConnectTimeout = mParam->watch_timeout>0?mParam->watch_timeout:0;
   if (url)
      mUrl = url;

   mWorkerThread->Post(this, MSG_RTMP_Connect);
   mMaxConnectTryTime = mParam->watch_reconnect_times;
   mConnectTryTime = 0;
   mParam->live_model = LIVE_TYPE_NONE;
}

bool HttpFlvDemuxer::OnConnect(int timeout) {
   
   DestoryClient();

   mGotVideoKeyFrame = false;
   mGotAudioPkt = false;

   for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
	   mMediaOutputNotify[i]->Destory();
   }
   VHALL_DEL(mFlvTagDemuxer);
   mFlvTagDemuxer = new FlvTagDemuxer();

   VHALL_DEL(mClient);
   mClient = new talk_base::HttpClientDefault(NULL,"vhall_demuxer", NULL);
   mClient->prepare_get(mUrl);
   mClient->request().setHeader(talk_base::HH_CONNECTION, "Keep-Alive", false);

   //set redirect action
   mClient->set_redirect_action(talk_base::HttpClient::REDIRECT_DEFAULT);
  
    //set time out
   int time_out = mConnectTimeout <= 0 ? DEFULT_PUBLISH_TIME_OUT : mConnectTimeout;

   //used to check timeout of connection
   mWorkerThread->PostDelayed(time_out, this, MSG_TIMEOUT);

   //set proxy
   if (mParam->is_http_proxy){
	   ProxyDetail pd = mParam->proxy;
	   //TODO talk with fuzhuang make it better
	   talk_base::ProxyInfo prox;
	   prox.autodetect = false;
	   prox.address = talk_base::SocketAddress(pd.host, pd.port);
	   prox.type = talk_base::PROXY_HTTPS;
	   prox.username = pd.username;
	   talk_base::InsecureCryptStringImpl ins_pw;
	   ins_pw.password() = pd.password;
	   std::string a = ins_pw.password();
	   talk_base::CryptString pw(ins_pw);
	   prox.password = pw;
	   mClient->set_proxy(prox);
   }

   mClient->SignalHeaderAvailable.connect(this, &HttpFlvDemuxer::OnHeaderAvailable);
   mClient->SignalHttpClientComplete.connect(this, &HttpFlvDemuxer::OnHttpClientComplete);
   mClient->start();

   return true;
}

void HttpFlvDemuxer::OnHeaderAvailable(talk_base::HttpClient* client, bool isfinal, size_t doc_length){
	if (isfinal){
		EventParam param;
		param.mId = -1;
		param.mDesc = mClient->server().ipaddr().ToString();
		mPlayer->NotifyEvent(SERVER_IP, param);
		LOGI("play stream success,tcurl:%s", mUrl.c_str());
		param.mDesc = "Player http-flv Connect OK";
		mPlayer->NotifyEvent(OK_WATCH_CONNECT, param);

		mConnectTryTime = 0;
		mDataStream = mClient->GetDocumentStream();
		mDataStream->SignalEvent.connect(this, &HttpFlvDemuxer::OnStreamEvent);
		mGotHttpHeader = true;

		//mWorkerThread->Post(this, MSG_RTMP_Recving);
		mComputeSpeedThread->Clear(this, MSG_RTMP_ComputeSpeed);
		mComputeSpeedThread->PostDelayed(5, this, MSG_RTMP_ComputeSpeed);
		return;
	}
}

void HttpFlvDemuxer::Retry(){
	if (!mStart){
		return;
	}
	mConnectTryTime++;
	if (mConnectTryTime> mMaxConnectTryTime){
        //mStart = false;
		EventParam param;
		param.mId = -1;
		param.mDesc = "Player stream failed";
		mPlayer->NotifyEvent(ERROR_WATCH_CONNECT, param);
		LOGE("connect failed.");
		Stop();
	}else{
		LOGW("start reconnect: %d", mConnectTryTime);
		mWorkerThread->PostDelayed(DEFAULT_PLAY_RECONNECT_DELAY_MS, this, MSG_RTMP_Connect);
	}
}

void HttpFlvDemuxer::OnHttpClientComplete(talk_base::HttpClient* client, talk_base::HttpErrorType type)
{
   Retry();
}

void HttpFlvDemuxer::OnStreamEvent(talk_base::StreamInterface* stream, int type, int error)
{
	if (type & talk_base::StreamEvent::SE_READ){
		talk_base::StreamResult result = talk_base::StreamResult::SR_ERROR;
		int error;
		char data_buf[10000];
		size_t nread = 0;
		LOGD("OnStreamEvent read event");
		while (1){
			result = stream->Read(data_buf, 10000, &nread, &error);
			if (result != talk_base::StreamResult::SR_SUCCESS || nread <= 0){
				break;
			}
			mTagBuffer.insert(mTagBuffer.end(), data_buf, data_buf + nread);
			LOGD("*************OnStreamEvent bufsize=%d", mTagBuffer.size());
			int ret = 0;
         while (true){
            ret = RecvOneTag();
            if (ret!=0) {
               break;
            }
         };
			if (ret == -2){ //means file not support or error version
				mStart = false;
				EventParam param;
				param.mId = -1;
				param.mDesc = "Player stream failed";
				mPlayer->NotifyEvent(ERROR_WATCH_CONNECT, param);
				LOGE("connect failed.");
				Stop();
				return;
			}
			if (ret == -1){
				break;
			}
		}
	}

	if (type & talk_base::StreamEvent::SE_CLOSE){
		Retry();
		LOGD("OnStreamEvent close event");
	}
}

int  HttpFlvDemuxer::ReadFull(void* buffer, size_t buffer_len, size_t* read, int* error)
{
	talk_base::StreamResult result = talk_base::StreamResult::SR_ERROR;
	size_t total_read = 0, current_read;

	if (!mDataStream){
		return result;
	}

	while (total_read < buffer_len && mStart) {
		current_read = 0;
		result = mDataStream->Read(static_cast<char*>(buffer)+total_read,
			buffer_len - total_read, &current_read, error);
		if (result == talk_base::StreamResult::SR_ERROR || result == talk_base::StreamResult::SR_EOS) //if SR_BLOCK read again
			break;
		total_read += current_read;
	}
	if (read){
		*read = total_read;
	}
	return result;
}

int HttpFlvDemuxer::RecvOneTag() {
	if (mStart) {
		if (!mDataStream){
			return -1;
		}
		//read file header
		if (!mGotFlvFileHeader){
			uint8_t *file_header;

			if (mTagBuffer.size() > 9){
				file_header = (uint8_t*)&mTagBuffer.at(0);
			}
			else{
				return -1;
			}

			if (file_header[0] != 'F' ||
				file_header[1] != 'L' ||
				file_header[2] != 'V'
				){
				LOGE("file is not flv");
				return -2;
			}
			if (file_header[3] != 0x01){//File version  1 Bytes
				return -2; //TODO check if suport more version
			}
					
			//check stream type
			LiveModel streamType = LIVE_TYPE_NONE;
			if (file_header[4] & 0x04 && file_header[4] && 0x01){
				streamType = LIVE_TYPE_VIDEO_AND_AUDIO;
			}else if (file_header[4] & 0x04){
				streamType = LIVE_TYPE_AUDIO_ONLY;
			}else if (file_header[4] & 0x01){
				streamType = LIVE_TYPE_VIDEO_ONLY;
			}
					
			char streamTypeStr[2] = { 0 };
			snprintf(streamTypeStr, 2, "%d", streamType);
			EventParam param;
			param.mId = -1;
			param.mDesc = streamTypeStr;
			mPlayer->NotifyEvent(RECV_STREAM_TYPE, param);
			
         mParam->live_model = streamType;
         
			mGotFlvFileHeader = true;
			mTagBuffer.erase(mTagBuffer.begin(), mTagBuffer.begin() + 9);
		}
		//start read tag
		int pos = 0;
		//read previous tag size.
		if (mTagBuffer.size() > pos + 4){		
			pos += 4;
		}
		else{
			return -1;
		}

		//read tag header
		uint8_t *tag_header;

		if (mTagBuffer.size() > pos + 11){
			tag_header = (uint8_t*)&mTagBuffer.at(pos);
			pos += 11;
		}
		else{
			return -1;
		}

		uint32_t size = 0;
		char type = 0;
		char* data = NULL;
		uint32_t timestamp = 0;
		type = tag_header[0] & 0x1F; //0x00011111
		size += tag_header[1] << 16;
		size += tag_header[2] << 8;
		size += tag_header[3];

		timestamp += tag_header[7] << 24;
		timestamp += tag_header[4] << 16;
		timestamp += tag_header[5] << 8;
		timestamp += tag_header[6];

		if (mTagBuffer.size() > pos + size){
			data = &mTagBuffer.at(pos);
			pos += size;
		}
		else{
			return -1;
		}

		switch (type){
		case SRS_RTMP_TYPE_AUDIO:
			OnAudio(timestamp, data, size);
			break;
		case SRS_RTMP_TYPE_VIDEO:
			OnVideo(timestamp, data, size);
			break;
		case SRS_RTMP_TYPE_SCRIPT:
			//report from file header not metadata any more
			//OnMetaData(timestamp, data, size); 
			mGotVideoKeyFrame = false;
			mGotAudioPkt = false;
			LOGI("receive metadata, destory decoder.");
			break;
		default:
			LOGW("unknown AMF0/AMF3 data message.");
		}

		mTotalSize += pos;
		mTagBuffer.erase(mTagBuffer.begin(), mTagBuffer.begin() + pos);
	}
	return 0;
}

void HttpFlvDemuxer::OnMessage(talk_base::Message* msg) {
   switch (msg->message_id) {
      case MSG_RTMP_Connect:
         if (true == mStart ) {
			 OnConnect(0);        
         }else{
            LOGW("mStart is false!");
         }
         break;
      case MSG_RTMP_ComputeSpeed:
		  if (true == mStart){
			  OnComputeSpeed();
		  }
         break;
      case MSG_RTMP_Close:
		  DestoryClient();
         break;

	  case MSG_TIMEOUT: //
		  if (!mGotHttpHeader){
				  Retry();
		  }
		  break;

      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

int  HttpFlvDemuxer::OnAudio(uint32_t timestamp, char*data, int size)
{
	int ret = ERROR_SUCCESS;
	//SrsAutoFree(SrsCommonMessage, audio);
	AacAvcCodecSample codecSample;
	if ((ret = mFlvTagDemuxer->audio_aac_demux(data, size, &codecSample)) != ERROR_SUCCESS) {
		LOGE("aac codec demux audio failed. ret=%d", ret);
		return ret;
	}
	if (codecSample.nb_sample_units == 0){
		LOGD("no audio sample unit");
		return ret;
	}
	if (mFlvTagDemuxer->audio_codec_id != FlvSoundFormatAAC) {
		LOGE("only suppot aac codec");
		return ret;
	}
	if (mGotAudioPkt == false) {
		mGotAudioPkt = true;
		AudioParam audioParam;
		GetAudioParam(audioParam, mFlvTagDemuxer, &codecSample);
		LOGI("Get first audio packet, will get audio codec, and notify mediaout(InitAudio)");
		for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
			mMediaOutputNotify[i]->InitAudio(&audioParam);
		}
	}
	for (int i = 0; i < codecSample.nb_sample_units; i++) {
		CodecSampleUnit* sample_unit = &codecSample.sample_units[i];
		ret = -1;

		LOGD("Audio(AAC) pkt timestamp=%lld ,size=%d,will notify %u mediaout",
			timestamp, sample_unit->size, (unsigned int)mMediaOutputNotify.size());

		for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
			DataUnit* newPkt = mMediaOutputNotify[i]->MallocDataUnit(STREAM_TYPE_AUDIO, sample_unit->size, 0);
			if (newPkt) {
				newPkt->dataSize = sample_unit->size;
				memcpy(newPkt->unitBuffer, sample_unit->bytes, (size_t)newPkt->dataSize);
				newPkt->timestap = timestamp;
				newPkt->isKey = false;
				newPkt->next = NULL;
				mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_AUDIO, newPkt);
			}
			else {
				LOGW("Media output can't malloc free dataunit,will discard audio data, something wrong.");
			}
		}//end for mMediaOutputNotify.size()
	}//end nb_sample_units
	return ret;
}

int  HttpFlvDemuxer::OnVideo(uint32_t timestamp, char*data, int size)
{
	int ret = 0;
	bool block = false;
	static u_int8_t aud_nal[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xf0 };
	AacAvcCodecSample codecSample;
	if ((ret = mFlvTagDemuxer->video_avc_demux(data, size, &codecSample)) != ERROR_SUCCESS) {
		LOGE("hls codec demux video failed. ret=%d", ret);
		return ret;
	}
	// ignore info frame,
	// @see https://github.com/simple-rtmp-server/srs/issues/288#issuecomment-69863909
	if (codecSample.frame_type == FlvVideoFrameTypeVideoInfoFrame
		|| FlvVideoAVCPacketTypeNALU != codecSample.avc_packet_type) {
		LOGI("found info frame,ignore it. ");
		return ret;
	}

	if (mFlvTagDemuxer->video_codec_id != FlvVideoCodecIDAVC) {
		LOGE("Only support AVC.");
		return ret;
	}
	if (codecSample.nb_sample_units == 0){
		LOGD("no video sample unit");
		return ret;
	}

	// ignore sequence header
	//first frame must key frame
	if (mGotVideoKeyFrame == false) {
		if (codecSample.frame_type != FlvVideoFrameTypeKeyFrame){
			LOGI("First frame must key frame,ignore this pkt. ");
			return ret;
		}
		LOGI("Got First frame must key frame. ");
		mGotVideoKeyFrame = true;
		VideoParam videoParam;
		GetVideoParam(videoParam, mFlvTagDemuxer, &codecSample);
		for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
			mMediaOutputNotify[i]->InitVideo(&videoParam);
		}
	}
	//if i frame, wait for buffer.
	//block = codecSample.frame_type == SrsCodecVideoAVCFrameKeyFrame;
	block = true;
	for (int i = 0; i < codecSample.nb_sample_units; i++) {
		CodecSampleUnit* sample_unit = &codecSample.sample_units[i];
      AvcNaluType nalType = (AvcNaluType)(sample_unit->bytes[0] & 0x1F);
      if (nalType > AvcNaluTypeIDR && sample_unit->size < 80) {
         LOGD("Not Frame data. ingore it. ");
         continue;
      }
		LOGD("Video(AVC) pkt timestamp=%llu frame_type=%d, avc_packet_type=%d, nal type= %ld ,size=%d, will notify %u mediaout",
			timestamp,
			codecSample.frame_type,
			codecSample.avc_packet_type,
			nalType,
			sample_unit->size,
			(unsigned int)mMediaOutputNotify.size());
		for (size_t i = 0; i < mMediaOutputNotify.size(); i++) {
			long unitSize = sample_unit->size + 4;
			if (codecSample.frame_type == FlvVideoFrameTypeKeyFrame){
				unitSize += mFlvTagDemuxer->pictureParameterSetLength + mFlvTagDemuxer->sequenceParameterSetLength + 8;
			}
			DataUnit* newPkt = mMediaOutputNotify[i]->MallocDataUnit(STREAM_TYPE_VIDEO, unitSize, 0);
			if (newPkt) {
				unsigned char* begin = newPkt->unitBuffer;
				newPkt->dataSize = unitSize;
				if (codecSample.frame_type == FlvVideoFrameTypeKeyFrame){
					memcpy(begin, aud_nal, 4);
					begin += 4;
					memcpy(begin, mFlvTagDemuxer->sequenceParameterSetNALUnit, mFlvTagDemuxer->sequenceParameterSetLength);
					begin += mFlvTagDemuxer->sequenceParameterSetLength;
					memcpy(begin, aud_nal, 4);
					begin += 4;
					memcpy(begin, mFlvTagDemuxer->pictureParameterSetNALUnit, mFlvTagDemuxer->pictureParameterSetLength);
					begin += mFlvTagDemuxer->pictureParameterSetLength;
					newPkt->isKey = true;
				}
				else{
					newPkt->isKey = false;
				}
				memcpy(begin, aud_nal, 4);
				begin += 4;
				memcpy(begin, sample_unit->bytes, sample_unit->size);
				newPkt->timestap = timestamp;
				newPkt->next = NULL;
				mMediaOutputNotify[i]->AppendStreamPacket(STREAM_TYPE_VIDEO, newPkt);
			}
			else {
				if (codecSample.frame_type == FlvVideoFrameTypeKeyFrame){
					LOGD("Key Frame is lost.");
				}
				LOGE("Media output can't malloc free dataunit,will discard video data, something wrong.");
			}
		}
	}
	return ret;
}

int  HttpFlvDemuxer::OnMetaData(uint32_t timestamp, char*data, int size)
{
	int ret = 0;
	if ((ret = mFlvTagDemuxer->metadata_demux2(data, size)) != 0){
        LOGE("Demux Metadata failed");
		return ret;
	}
	LOGI("process onMetaData message success.");
    EventParam param;
    param.mId = -1;
    param.mDesc = "process onMetaData message success.";
    mPlayer->NotifyEvent(DEMUX_METADATA_SUCCESS, param);
	LiveModel streamType = LIVE_TYPE_NONE;
	if (mFlvTagDemuxer->video_codec_id == FlvVideoCodecIDAVC&&mFlvTagDemuxer->audio_codec_id == FlvSoundFormatAAC) {
		streamType = LIVE_TYPE_VIDEO_AND_AUDIO;
	}
	else if (mFlvTagDemuxer->video_codec_id == FlvVideoCodecIDAVC&&mFlvTagDemuxer->audio_codec_id != FlvSoundFormatAAC){
		streamType = LIVE_TYPE_VIDEO_ONLY;
	}
	else if (mFlvTagDemuxer->audio_codec_id == FlvSoundFormatAAC&&mFlvTagDemuxer->video_codec_id != FlvVideoCodecIDAVC){
		streamType = LIVE_TYPE_AUDIO_ONLY;
	}
	if (mParam->live_model != LIVE_TYPE_NONE && streamType != mParam->live_model) {
	}
	else{
		char streamTypeStr[2] = { 0 };
		snprintf(streamTypeStr, 2, "%d", streamType);
		EventParam param;
		param.mId = -1;
		param.mDesc = streamTypeStr;
		mPlayer->NotifyEvent(RECV_STREAM_TYPE, param);
	}
	mParam->live_model = streamType;
	return ret;
}

void HttpFlvDemuxer::OnComputeSpeed()
{
   uint32_t m_speed = 0;
   m_speed = mTotalSize * 8/1024;
   char speed_string[8]={0};
   snprintf(speed_string,8,"%d",m_speed);
   mTotalSize = 0;
   EventParam param;
   param.mId = -1;
   param.mDesc = speed_string;
   mPlayer->NotifyEvent(INFO_SPEED_DOWNLOAD, param);
   mComputeSpeedThread->PostDelayed(1000, this, MSG_RTMP_ComputeSpeed);
}

//http://www.cnblogs.com/musicfans/archive/2012/11/07/2819291.html
void HttpFlvDemuxer::GetAudioParam(AudioParam &audioParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample) {
   audioParam.extra_size = flv_tag_demuxer->aac_extra_size;
   audioParam.extra_data = (char*)malloc(flv_tag_demuxer->aac_extra_size);
   if (audioParam.extra_data)
      memcpy(audioParam.extra_data, flv_tag_demuxer->aac_extra_data, flv_tag_demuxer->aac_extra_size);
   audioParam.numOfChannels = flv_tag_demuxer->aac_channels;
   //audioParam.samplesPerSecond = ;
}

void HttpFlvDemuxer::GetVideoParam(VideoParam &videoParam, FlvTagDemuxer* flv_tag_demuxer, AacAvcCodecSample* codecSample) {
   videoParam.avc_extra_size = flv_tag_demuxer->avc_extra_size;
   videoParam.avc_extra_data = (char*)malloc(flv_tag_demuxer->avc_extra_size);
   if (videoParam.avc_extra_data)
      memcpy(videoParam.avc_extra_data, flv_tag_demuxer->avc_extra_data, flv_tag_demuxer->avc_extra_size);
   videoParam.width = flv_tag_demuxer->width;
   videoParam.height = flv_tag_demuxer->height;
   if (flv_tag_demuxer->frame_rate <=5) {
      videoParam.framesPerSecond = DEFAULT_FPS_SUPPORT;
   }else if(flv_tag_demuxer->frame_rate >= MAX_FPS_SUPPORT){
      videoParam.framesPerSecond = MAX_FPS_SUPPORT;
   }else{
      videoParam.framesPerSecond = flv_tag_demuxer->frame_rate;
   }
}

std::string HttpFlvDemuxer::GetServerIp(){
   VhallAutolock lock(&mDeatoryRtmpMutex);
   if (mClient && mGotHttpHeader){
	   return mClient->server().ipaddr().ToString();
   }
   return "";
}

