#ifndef __VINNY_LIVE_DEFINE_H__
#define __VINNY_LIVE_DEFINE_H__

#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <functional>
#include "live_open_define.h"

//用于控制创建的对象
typedef enum{
	LIVE_TYPE_PUBLISH = 0, //发起
	LIVE_TYPE_PLAY = 1   //观看
}LiveCreateType;

struct AudioParam {
   AudioParam() {
      enabled = 0;
      bitRate = 0;
      samplesPerSecond = 0;
      bitsPerSample = 0;
      numOfChannels = 0;
      unitLengthInMs = 0; /* Remove here. */
      tempoChangeBeforeDrop = 0;
      extra_size = 0;
      extra_data = 0;
   };
   virtual ~AudioParam() {
   };
	bool enabled;
	int bitRate;
	int samplesPerSecond;
	int bitsPerSample;
	int numOfChannels;
	int unitLengthInMs;  /* Remove here. */
	//int frame_size;
	bool tempoChangeBeforeDrop;
	int  extra_size;
	char* extra_data;
};

struct VideoParam {
   VideoParam() {
      enabled = 0;
      bitRate = 0;
      width = 0;
      height = 0;
      framesPerSecond = 0;
      //int deviceId;
      catchUpFrameMethod = 0; /* 0 - fill with blank data, 1 - fill with prev frame data. */
      avc_extra_size = 0;
      avc_extra_data = NULL;
   };
   virtual ~VideoParam() {
   };
	bool enabled;
	int bitRate;
	int width;
	int height;
	int framesPerSecond;
	//int deviceId;
	int catchUpFrameMethod; /* 0 - fill with blank data, 1 - fill with prev frame data. */
	int   avc_extra_size;
	char* avc_extra_data;
};

enum  STREAM_TYPE {
	STREAM_TYPE_VIDEO,
	STREAM_TYPE_AUDIO,
   STREAM_TYPE_ONCUEPONIT_MSG
};

typedef struct
{
	uint64_t first_time_stamp;
	unsigned int video_count;
} video_frame_limit_st;

class MuxerEventParam{
public:
	//set by MuxerInterface
	int mId;
	std::string mTag;
	//set by subclasses, may be "";
	std::string mDesc;
	MuxerEventParam() :mId(-1), mTag(""), mDesc(""){};
};

class EventParam : public MuxerEventParam{};

#define VIDEO_DEC_MODE_SOFT 1
#define VIDEO_DEC_MODE_HW   2
#define VIDEO_DEC_MODE_AUTO 3

#define SDK_VERSION             "v3.8.1"

#define SEND_ERROR_CHANCE       3
#define RECV_ERROR_CHANCE       3

#define AUDIO_SAMPLE_RATE_BASE  1000

#define BUFFER_MAX_TIME         10
#define BUFFER_MIN_TIME         1

#define AUDIO_DEBUG  1

#define VHALL_DEL(x) if (x) {  \
delete x;                      \
x = NULL;                      \
}
#define VHALL_DEL_ARRAY(x) if (x) {  \
delete [] x;                      \
x = NULL;                      \
}
#define VHALL_FILE_DEL(x) if (x) {  \
fclose(x);                      \
x = NULL;                      \
}

#define VHALL_THREAD_DEL(x) if (x) {  \
x->Stop();                            \
delete x;                             \
x = NULL;                             \
}

typedef std::function<void(const int8_t* audio_data,const int size)> OutputDataDelegate;

/** @def VH_DISALLOW_COPY_AND_ASSIGN(TypeName)
 * A macro to disallow the copy constructor and operator= functions.
 * This should be used in the private: declarations for a class
 */
#if defined(__GNUC__) && ((__GNUC__ >= 5) || ((__GNUG__ == 4) && (__GNUC_MINOR__ >= 4))) \
|| (defined(__clang__) && (__clang_major__ >= 3)) || (_MSC_VER >= 1800)
#define VH_DISALLOW_COPY_AND_ASSIGN(TypeName) \
TypeName(const TypeName &) = delete; \
TypeName &operator =(const TypeName &) = delete;
#else
#define VH_DISALLOW_COPY_AND_ASSIGN(TypeName) \
TypeName(const TypeName &); \
TypeName &operator =(const TypeName &);
#endif

#define VH_CALLBACK_0(__selector__,__target__, ...) std::bind(&__selector__,__target__, ##__VA_ARGS__)
#define VH_CALLBACK_1(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, ##__VA_ARGS__)
#define VH_CALLBACK_2(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, ##__VA_ARGS__)
#define VH_CALLBACK_3(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, ##__VA_ARGS__)

//play consts
#define DEFAULT_FPS_SUPPORT             15
#define MAX_FPS_SUPPORT                 60
#define DEFAULT_AUDIO_PLAY_UNIT_SIZE_MS 50
#define DEFAULT_BUFFER_TIME 2000
#define MIN_BUFFER_TIME 1000   //must > INIT_PLAY_BUFFER_TIME
#define MAX_BUFFER_TIME 20000
#define INIT_PLAY_BUFFER_TIME 1000
#define MIN_AUDIO_BUFFER_SIZE 50
#define MIN_VIDEO_BUFFER_SIZE 20

#define RENDER_BUFFER_TIME 300
#define MIN_AUDIO_RENDER_BUFFER_SIZE 10
#define MIN_VIDEO_RENDER_BUFFER_SIZE 5

#define PCM_UNIT_SIZE         4096
#define PCM_FRAME_SIZE        1024

#define PLAY_BUFFER_RATIO     1.0
#define STRICT_BUFFER_RATIO   4.0
#define MAX_FRAME_SIZE        3840*2160*3/2

#define DEFAULT_GOP_TIME      4 //单位是S

#ifndef MIN
#define MIN(x,y) (x>y?y:x)
#endif

#ifndef MAX
#define MAX(x,y) (x>y?x:y)
#endif

#define VIDEO_ENCODE_X264

#define DEFAULT_PLAY_RECONNECT_DELAY_MS 1000
#define DEFAULT_PUSH_RECONNECT_DELAY_MS 1000

//音频去噪支持的最大采样率
#define SUPPORT_MAX_NOISE_SUPPRESSION_SAMPLING_RATE    32000

typedef enum {
	SERVER_IP = 1000,
   UPDATE_URL = 1001,
   NEW_KEY_FRAME = 1002,
}LiveInsideStatusCode;

typedef struct NaluUnit
{
	int type;
	int size;
	unsigned char *data;
} NaluUnit;

typedef struct _RTMPMetadata
{
	// video, must be h264 type
	bool	         bHasVideo;
	unsigned int	nWidth;
	unsigned int	nHeight;
	unsigned int	nFrameRate;		   // fps
	unsigned int	nVideoDataRate;	// bps
	unsigned int	nSpsLen;
	uint8_t	      Sps[1024];
	unsigned int	nPpsLen;
	uint8_t	      Pps[1024];

	// audio, must be aac type
	bool	         bHasAudio;
	unsigned int	nAudioSampleRate;
	unsigned int	nAudioSampleSize;
	unsigned int	nAudioChannels;
	char		      pAudioSpecCfg;
	unsigned int	nAudioSpecCfgLen;

	//file info
	int64_t    nFileSize;
	int64_t    nDuration;

}RTMPMetadata, *LPRTMPMetadata;

enum
{
	FLV_CODECID_H264 = 7,
	FLV_CODECID_AAC = 10
};
#endif
