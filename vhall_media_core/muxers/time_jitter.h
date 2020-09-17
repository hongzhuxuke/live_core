#ifndef __TIME_JITTER_H__
#define __TIME_JITTER_H__

#if defined(_WIN32)
#include <stdint.h>
#endif
#include<stdlib.h>
#include <inttypes.h>

enum JitterFrameType{
	JITTER_FRAME_TYPE_VIDEO = 0,
	JITTER_FRAME_TYPE_AUDIO,
	JITTER_FRAME_TYPE_NONE
};

class TimeJitter{
private:
	int mLastType;
	int64_t mLastTs;
	int64_t mLastRetAudioTs;
	int64_t mLastRetVideoTs;
	int64_t mLastRetTs;
	int mAudioFrameDuration;
	int mVideoFrameDuration;
	int mCorrectThreashold;
   
public:
	TimeJitter(int audio_frame_duration = 23,   //44100
              int video_frame_duration = 67, //frame rate 15
				  int correct_threashhold = 200);

	uint64_t GetCorretTime(JitterFrameType type, uint64_t ts);
	uint64_t GetLastRetTs(){ return mLastRetTs;};
	void Reset();
};

#endif
