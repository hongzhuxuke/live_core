#include "time_jitter.h"
#include <assert.h>

TimeJitter::TimeJitter(int audio_frame_duration,   //44100
	int video_frame_duration, //frame rate 15
	int correct_threashhold)
	:mLastType(JITTER_FRAME_TYPE_NONE),
	mLastTs(0),
	mLastRetAudioTs(0),
	mLastRetVideoTs(0),
	mLastRetTs(0),
	mAudioFrameDuration(audio_frame_duration),
	mVideoFrameDuration(video_frame_duration),
	mCorrectThreashold(correct_threashhold)
{
	assert(mAudioFrameDuration > 0);
	assert(mVideoFrameDuration > 0);
	assert(mCorrectThreashold > 0);
}

uint64_t TimeJitter::GetCorretTime(JitterFrameType type, uint64_t ts)
{
	assert(type == JITTER_FRAME_TYPE_VIDEO || type == JITTER_FRAME_TYPE_AUDIO);
	uint64_t correct_ts = 0;
	if (mLastType == JITTER_FRAME_TYPE_NONE){ //first frame
		if (type == JITTER_FRAME_TYPE_VIDEO){
			mLastType = JITTER_FRAME_TYPE_VIDEO;
		}
		else {
			mLastType = JITTER_FRAME_TYPE_AUDIO;	
		}
		mLastTs = ts;
		mLastRetVideoTs = mLastRetAudioTs = mLastRetTs = 0;
		return mLastRetTs;
	}

	if (type == JITTER_FRAME_TYPE_VIDEO){
		int64_t diff = (int64_t)(ts - mLastTs);
		if (diff >= mCorrectThreashold || diff < 0){
			correct_ts = mLastRetVideoTs + mVideoFrameDuration;
			if (correct_ts < mLastRetTs) { //check if little than lastts(if true lasttype should be audio)
				diff = 0;
			}
			else{
				diff = correct_ts - mLastRetTs;
			}
		}
		mLastTs = ts;
		mLastRetVideoTs = mLastRetTs = mLastRetTs+diff;
		mLastType = JITTER_FRAME_TYPE_VIDEO;
		return mLastRetTs;
	}
	else {
		uint64_t diff = ts - mLastTs;
		if (diff >= mCorrectThreashold || diff < 0){
			correct_ts = mLastRetAudioTs + mAudioFrameDuration;
			if (correct_ts < mLastRetTs) { //check if little than lastts(if true lasttype should be audio)
				diff = 0;
			}
			else {
				diff = correct_ts - mLastRetTs;
			}
		}

		mLastTs = ts;
		mLastRetAudioTs = mLastRetTs = mLastRetTs + diff;
		mLastType = JITTER_FRAME_TYPE_AUDIO;
		return mLastRetTs;
	}
}

void TimeJitter::Reset()
{
	mLastType = JITTER_FRAME_TYPE_NONE;
	mLastTs = 0;
	mLastRetAudioTs = 0;
	mLastRetVideoTs = 0;
	mLastRetTs = 0;
}
