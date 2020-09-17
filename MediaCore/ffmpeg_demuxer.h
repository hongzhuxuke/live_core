/*
 * ffmpeg_demuxer.h
 *
 *  Created on: 2018年3月30日
 *      Author: yangsl
 */

#ifndef MODULES_MEDIA_DEMUXERS_FFMPEG_DEMUXER_H_
#define MODULES_MEDIA_DEMUXERS_FFMPEG_DEMUXER_H_
//#include "media_common.h"
#ifdef  __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef  __cplusplus
}
#endif

#include <vector>
#include <mutex>

#define MEDIA_AUDIO_CHANNEL       2
#define MEDIA_AUDIO_SAMPLE_RATE   44100
#define MEDIA_AUDIO_SAMPLE_FMT    AV_SAMPLE_FMT_S32
#define MEDIA_TIME_BASE           1000

//enum  STREAM_TYPE {
//	STREAM_TYPE_VIDEO,
//	STREAM_TYPE_AUDIO
//};
//

typedef struct {
	int codecId;
	int format;
	int bitRate;
	int width;
	int height;
	int framesPerSecond;
	int avc_extra_size;
	char* avc_extra_data;
} VideoPar;
typedef struct {
	int codecId;
	int format;
	int bitRate;
	int samplesPerSecond;
	int bitsPerSample;
	int numOfChannels;
	int frame_size;
	int extra_size;
	char* extra_data;
} AudioPar;

namespace vhall {
   class FFmpegDemuxer {
   public:
      FFmpegDemuxer();
      virtual ~FFmpegDemuxer();
   public:
      int Init(const char *url);
      int ReadPacket(AVPacket* packet, enum AVMediaType & mType);
      int GetAudioStreamCount();
      /* get audio index from index vector */
      int GetAudioStreamIndex(int streamIndex);
      /* current video index */
      int GetVideoStreamIndex();
      /* total duration of media file, unit: msec */
      int64_t GetDurtion();
      bool Seek(int64_t seekTime, int streamIndex);
      AudioPar* GetAudioPar(int streamIndex);
      VideoPar* GetVideoPar();
   private:
      int                       hasAudio;
      int                       hasVideo;
      AVFormatContext*          mIfmtCtx;
      AudioPar*          mAudioPar;
      VideoPar*          mVideoPar;
      std::vector<AudioPar*>    mAudioParInfo;
      std::vector<int>          mAudioStreamIndex;
      int                       mVideoStreamIndex;
      std::mutex                mReadMtx;
   };
}
#endif /* MODULES_MEDIA_DEMUXERS_FFMPEG_DEMUXER_H_ */
