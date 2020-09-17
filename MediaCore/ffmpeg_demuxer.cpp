/*
 * ffmpeg_demuxer.cpp
 *
 *  Created on: 2018年3月30日
 *      Author: yangsl
 */
#include "ffmpeg_demuxer.h"
//#include "utility.h"

namespace vhall {
   FFmpegDemuxer::FFmpegDemuxer() {
      hasAudio = 0;
      hasVideo = 0;
      mIfmtCtx = NULL;
      mAudioPar = NULL;
      mVideoPar = NULL;
      mVideoStreamIndex = -1;
   }
   FFmpegDemuxer::~FFmpegDemuxer() {
      if (mIfmtCtx) {
         avformat_close_input(&mIfmtCtx);
      }
      if (mAudioPar && mAudioPar->extra_data) {
         free(mAudioPar->extra_data);
         mAudioPar->extra_data = nullptr;
         mAudioPar->extra_size = 0;
      }
      //VHALL_DEL(mAudioPar);
		if (mAudioPar) {
			delete mAudioPar;
			mAudioPar = NULL;
		}
      if (mVideoPar && mVideoPar->avc_extra_data) {
         free(mVideoPar->avc_extra_data);
         mVideoPar->avc_extra_data = nullptr;
         mVideoPar->avc_extra_size = 0;
      }
      //VHALL_DEL(mVideoPar);
		if (mVideoPar) {
			delete mVideoPar;
			mVideoPar = NULL;
		}
   }
   int FFmpegDemuxer::Init(const char *url) {
      int ret = 0;
      //av_register_all();
      avformat_network_init();

      if ((ret = avformat_open_input(&mIfmtCtx, url, NULL, NULL)) < 0) {
         //LOGE("Cannot open input file\n");
         return ret;
      }
      if ((ret = avformat_find_stream_info(mIfmtCtx, NULL)) < 0) {
        // LOGE("Cannot find stream information\n");
         return ret;
      }
      for (unsigned int i = 0; i < mIfmtCtx->nb_streams; i++) {
         AVStream *stream;
         AVCodecParameters *codecpar;
         stream = mIfmtCtx->streams[i];
		 if (stream == nullptr) {
			 break;
		 }
         codecpar = stream->codecpar;
         /* analyze Audio/Video foramt/Codec info */
         if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoPar = (VideoPar*)calloc(1, sizeof(VideoPar));
            if (NULL == mVideoPar) {
               //LOGE("new mVideoParam fail!\n");
               return -1;
            }
            hasVideo = true;
            mVideoStreamIndex = i;
            mVideoPar->codecId = codecpar->codec_id;
            mVideoPar->framesPerSecond = av_q2d(stream->avg_frame_rate);
            mVideoPar->bitRate = codecpar->bit_rate;
            mVideoPar->format = codecpar->format;
            mVideoPar->height = codecpar->height;
            mVideoPar->width = codecpar->width;
            if (codecpar->extradata_size > 0) {
               mVideoPar->avc_extra_data = (char*)calloc(1, codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
               if (NULL != mVideoPar->avc_extra_data) {
                  memcpy(mVideoPar->avc_extra_data, codecpar->extradata, codecpar->extradata_size);
                  mVideoPar->avc_extra_size = codecpar->extradata_size;
               }
            }
         }
         else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            mAudioPar = (AudioPar*) calloc(1, sizeof(AudioPar));
            if (NULL == mAudioPar) {
              // LOGE("new mAudioPar fail!\n");
               return -1;
            }
            hasAudio = true;
            mAudioParInfo.push_back(mAudioPar);
            mAudioStreamIndex.push_back(i);
            mAudioPar->codecId = codecpar->codec_id;
            mAudioPar->bitRate = codecpar->bit_rate;
            mAudioPar->format = codecpar->format;
            mAudioPar->numOfChannels = codecpar->channels;
            mAudioPar->samplesPerSecond = codecpar->sample_rate;
            mAudioPar->frame_size = codecpar->frame_size;
            //mAudioPar->bitsPerSample = Utility::GetBitNumWithSampleFormat(codecpar->format);
            if (codecpar->extradata_size > 0) {
               mAudioPar->extra_data = (char*)calloc(1, codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
               if (NULL != mAudioPar->extra_data) {
                  memcpy(mAudioPar->extra_data, codecpar->extradata, codecpar->extradata_size);
                  mAudioPar->extra_size = codecpar->extradata_size;
               }
            }
            mAudioPar->extra_size = codecpar->extradata_size;
         }
      }
      av_dump_format(mIfmtCtx, 0, url, 0);
      return 0;
   }
   int FFmpegDemuxer::ReadPacket(AVPacket* packet, enum AVMediaType & mType) {
      int ret = 0;
      if (NULL == packet) {
         return -1;
      }
      av_init_packet(packet);
      std::unique_lock<std::mutex> lock(mReadMtx);
      if ((ret = av_read_frame(mIfmtCtx, packet)) < 0) {
        // LOGE("av_read_frame fail\n");
         return -2;
      }
      lock.unlock();
      mType = mIfmtCtx->streams[packet->stream_index]->codecpar->codec_type;
      AVRational dstRational = { 1, MEDIA_TIME_BASE };
      packet->pts = av_rescale_q_rnd(packet->pts, mIfmtCtx->streams[packet->stream_index]->time_base, dstRational, AV_ROUND_NEAR_INF);
//      LOGI("Demuxer gave frame of stream_index %u\n", packet->stream_index);
      return 0;
   }
   int FFmpegDemuxer::GetAudioStreamCount() {
      return mAudioParInfo.size();
   }
   int FFmpegDemuxer::GetVideoStreamIndex() {
      return mVideoStreamIndex;
   }
   int FFmpegDemuxer::GetAudioStreamIndex(int streamIndex) {
      if (streamIndex < mAudioStreamIndex.size()) {
         return mAudioStreamIndex[streamIndex];
      }
      return -1;
   }
   int64_t FFmpegDemuxer::GetDurtion() {
      if (mIfmtCtx) {
         return mIfmtCtx->duration / 1000;
      }
      return 0;
   }
   bool FFmpegDemuxer::Seek(int64_t seekTime, int streamIndex) {
      int ret = 0;
      if (nullptr == mIfmtCtx) {
       //  LOGE("FFmpegDemuxer seek fail: not inited");
         return false;
      }
      if (streamIndex < 0) {
       //  LOGE("streamIndex err");
         return false;
      }
      if (seekTime > GetDurtion() || seekTime < 0) {
        // LOGE("error seekTime");
         return false;
      }
      AVRational srcRational = { 1, MEDIA_TIME_BASE };
      auto seekTime_ = av_rescale_q_rnd(seekTime, srcRational, mIfmtCtx->streams[streamIndex]->time_base, AV_ROUND_NEAR_INF);
      auto newPts = mIfmtCtx->streams[streamIndex]->start_time + seekTime_;
      if (newPts > mIfmtCtx->streams[streamIndex]->cur_dts) {
         std::unique_lock<std::mutex> lock(mReadMtx);
         ret = av_seek_frame(mIfmtCtx, streamIndex, newPts, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
      } else {
         std::unique_lock<std::mutex> lock(mReadMtx);
         ret = av_seek_frame(mIfmtCtx, streamIndex, newPts, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
      }
      if (ret < 0) {
         return false;
      }
      return true;
   }
   AudioPar* FFmpegDemuxer::GetAudioPar(int streamIndex) {
      if (streamIndex < mAudioParInfo.size()) {
         return mAudioParInfo[streamIndex];
      }
      return nullptr;
   }
   VideoPar* FFmpegDemuxer::GetVideoPar() {
      if (mVideoPar) {
         return mVideoPar;
      }
      return nullptr;
   }
}

