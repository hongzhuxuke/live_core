#include "safe_buffer_data.h"
#include "vhall_log.h"
#include <stdlib.h>
#include <assert.h> 
#include <auto_lock.h>
#include <string.h>
#include "live_message.h"

#define DEFULT_POOL_SIZE 10000
#define DEFULT_POOL_LARGE_SIZE 100

#define COND_WAIT_TIME      15000 //15s

char *DropFrameTypeStr[] = {
	(char*)"DROP_NONE",
	(char*)"DROP_GOPS",
	(char*)"DROP_ALL_VIDEO",
	(char*)"DROP_ALL",
	(char*)"DROP_ALL_B",
	(char*)"DROP_ALL_B_P",
	(char*)"DROP_ALL_B_P_I",
	(char*)"DROP_ALL_B_P_I_A",
	(char*)"DROP_ONE_B",
	(char*)"DROP_ONE_B_P",
	(char*)"DROP_ONE_B_P_I",
	(char*)"DROP_ONE_B_P_I_A"
};

SafeData::SafeData()
: mSize(0),
  mBufferSize(0),
  mData(NULL),
  mType(-1),
  mTs(0),
  mUserData(NULL),
  mFreeFunc(NULL),
  mExtraData(NULL),
  next(NULL)
{
	mRef = 0;
#if SINGLE_SAFE_DATA_DEBUG
	mDebug = false;
#endif
};

SafeData::~SafeData(){
	//assert(mRef == 0);
	if (mData){
		free(mData);
      mData = NULL;
	}
	if (mExtraData){
		delete (char*)mExtraData;
      mExtraData = NULL;
	}
}

void SafeData::SelfRelease(){
	assert(mRef > 0);
	unsigned int v = --mRef;

#if SINGLE_SAFE_DATA_DEBUG
	if (mDebug){
		int ref = mRef;
		LOGD("%s mRef=%d SelfRelease\n", mTag.c_str(), ref);
	}
#endif

	if (v == 0){
		if (mFreeFunc) {
#if SINGLE_SAFE_DATA_DEBUG
			if (mDebug){
				int ref = mRef;
				LOGD("%s mRef=%d Free\n", mTag.c_str(), ref);
			}
         mDebug = false;
#endif
			mSize = 0;
			mFreeFunc(mUserData, this);
		}
	}
}

int SafeData::GetBufferSize() {
	return mBufferSize;
}

SafeData * SafeData::SelfCopy(){
	assert(mRef > 0);
	mRef++;
#if SINGLE_SAFE_DATA_DEBUG
	if (mDebug){
		int ref = mRef;
		LOGD("%s mRef=%d  SelfCopy\n", mTag.c_str(), ref);
	}
#endif
	return this;
}

int SafeData::GetRefCount(){
   return mRef;
}

int SafeData::Reset(int buffer_size){
	if (mBufferSize == 0){
		mData = (char *)calloc(buffer_size, 1);
		if (!mData){
			return -1;
		}
	}

	if (mBufferSize < buffer_size){
		char *temp = (char *)realloc(mData, buffer_size);
		if (!temp){
			return -1;
		}		
		mData = temp;
	}

	mBufferSize = buffer_size;
	return 0;
}

int SafeData::WriteData(char * data, int size, int type, uint64_t ts){
	assert(mRef == 0);
	if (mBufferSize < size && Reset(size) < 0){
		return -1;
	}
#if SINGLE_SAFE_DATA_DEBUG
	if (mDebug){
		int ref = mRef;
		LOGD("%s mRef=%d WriteData\n", mTag.c_str(), ref);
	}
#endif
	memcpy(mData, data, size);
	mSize = size;
	mType = type;
	mTs = ts;
	mRef = 1;
	return 0;
}

void SafeData::SetFree(SafeDataFree ffunc, void *mUserData){
	mFreeFunc = ffunc;
	this->mUserData = mUserData;
}

SafeDataPool::SafeDataPool(int max_size, int auto_larg)
	:mMaxSize(max_size),
   mAutoLarge(auto_larg){
	vhall_lock_init(&mLock);
	mArraySafeDatas = new SafeData[max_size];
	SafeData *pre = &mArraySafeDatas[0];

	mLargeList.clear();

	mFreeDatas = pre;

	for (int i = 1; i < max_size; i++){
		pre->SetFree(DataFree, this);
		pre->next = &mArraySafeDatas[i];	
		pre = pre->next;
	}
	pre->SetFree(DataFree, this);
	pre->next = NULL;

	mFreeCount = max_size;

	CheckPool();

}

/**befor call this Destrocter 
*  make sure all SafeDate that from this pool never used any more.
*  or there will be a unknow error.
*/
SafeDataPool::~SafeDataPool(){
	delete[] mArraySafeDatas;
	for (auto it = mLargeList.begin(); it != mLargeList.end(); it++){
		delete (*it);
	}
	mLargeList.clear();

	vhall_lock_destroy(&mLock);
}

SafeData *SafeDataPool::GetSafeData(char *data, int size, int type, uint64_t ts)
{
	VhallAutolock _Lock(&mLock);
	CheckPool();
	SafeData *ret = NULL;
	if (mFreeCount > 0){
		ret = mFreeDatas;

		if (ret->WriteData(data, size, type, ts) < 0) {
			LOGW("WriteData faild!!!!!!!!!!!!!!!!!!!!!");
			ret = GetSafeData2(data, size, type, ts);
		}else {
			mFreeDatas = mFreeDatas->next;
			mFreeCount--;
		}
	}
	else if(mAutoLarge){
		if (LargeSize(DEFULT_POOL_LARGE_SIZE) > 0){
			return GetSafeData(data, size, type, ts);
		}
	}
	//TODO set a flag if allow larger than max_size
	CheckPool();
   if (ret==NULL) {
      LOGE("new SafeData is error!!!!");
   }
	return ret;
}

int SafeDataPool::GetFreeDateSize(){
	int ret;
	VhallAutolock _Lock(&mLock);
	ret = mFreeCount;
	return ret;
}

int SafeDataPool::GetMaxDataSize(){
	int ret;
	VhallAutolock _Lock(&mLock);
	ret = mMaxSize;
	return ret;
}

int SafeDataPool::LargeSize(int large_size){
	CheckPool();
	assert(mFreeDatas == NULL);
	int ret = 0;
	SafeData * last = NULL;
	for (int i = 0; i < large_size; i++){
		SafeData *item = new SafeData();
		if (!item){
			break;
		}
		item->SetFree(DataFree, this);
		item->next = last;
		last = item;
		mLargeList.push_back(item);
		ret++;
	}
	mFreeDatas = last;
	mMaxSize += ret;
	mFreeCount += ret;
	CheckPool();
	return ret;
}

void SafeDataPool::CheckPool()
{
	int i = 0;
	SafeData *item = mFreeDatas;
	while (item){
		i++;
		assert(item->mRef == 0);
		item = item->next;
	}
	assert(mFreeCount == i);
}

//find a SafeData which buffersize > size.
SafeData *SafeDataPool::GetSafeData2(char *data, int size, int type, uint64_t ts)
{
	CheckPool();
	SafeData *pre = mFreeDatas;
	SafeData *next = pre->next;
	//no need to test first
	while (next){
		if (next->GetBufferSize() >= size){
			next->WriteData(data, size, type, ts);
			pre->next = next->next;
			mFreeCount--;
			return next;
		}
		pre = next;
		next = next->next;
	}
	CheckPool();
	return NULL;
}

void SafeDataPool::RecycleData(SafeData * data){
	VhallAutolock _Lock(&mLock);
   CheckPool();
	data->next = mFreeDatas;
	mFreeDatas = data;
	mFreeCount++;
   CheckPool();
}

void SafeDataPool::DataFree(void *user_data, SafeData * data){	
	SafeDataPool *pool = (SafeDataPool*)user_data;
	pool->RecycleData(data);	
}

SafeDataQueue::SafeDataQueue(SafeDataQueueStateListener *callback, float min_warning , float max_warning, int max_num)
	:mMaxNum(max_num),
	mDropCount(0),
	mStateCallback(callback)
{
	mMinWarning = (uint32_t)(mMaxNum * min_warning);
	mMAXWarning = (uint32_t)(mMaxNum * max_warning);
	mState = SAFE_DATA_QUEUE_STATE_EMPTY;
	mDropType = DROP_NONE;

	vhall_cond_init(&mCondNotEmpty);
	vhall_cond_init(&mCondNotFull);
	vhall_lock_init(&mMutex);

	mTag = "";
	mQueue.clear();

	mWaitFlag = false;
	mWaitFrameType = VIDEO_B_FRAME;

	mClearFlag = false;

	LOGD("%p %s SafeDataQueue constroctor", this, mTag.c_str());
}

SafeDataQueue::~SafeDataQueue(){
	ClearAllQueue();
	vhall_cond_destroy(&mCondNotEmpty);
	vhall_cond_destroy(&mCondNotFull);
	vhall_lock_destroy(&mMutex);
	LOGD("%p  %s SafeDataQueue destroctor", this, mTag.c_str() );
}

bool SafeDataQueue::PushQueue(SafeData* data, BUFFER_FULL_ACTION action){

	VhallAutolock _Lock(&mMutex);

	UpdataState();

	//check if wait frame.
	if (mWaitFlag&&data->mType>=AUDIO_FRAME){
		//not match what we wait
		if (data->mType > mWaitFrameType||data->mType == AUDIO_FRAME){
			data->SelfRelease();
			LOGD("%s wait frame not match wait_type=%d now_type=%d", mTag.c_str(), mWaitFrameType, data->mType);
			return true;
		}
		mWaitFlag = false;
		mWaitFrameType = VIDEO_B_FRAME;
	}

	if (mQueue.size() >= mMaxNum && DropFrame() <= 0){
		if (action == ACTION_BLOCK){
			do{
				vhall_cond_wait(&mCondNotFull, &mMutex);
			} while (mQueue.size() >= mMaxNum && mClearFlag == false);//TODO　if need to remove while
		}
		else if (action == ACTION_DROP_FAILD_RETURN){
			LOGW("%s ReadQueue faild1！ ACTION_DROP_FAILD_RETURN!", mTag.c_str());
			return false;
		}
		else { //action == ACTION_DROP_FAILD_LARGE, so we large the queue
			mMaxNum++;
		}	
	}

	if (mQueue.size() >= mMaxNum){
		LOGW("%s PushQueue failed2 mQueue.size()=%d mMaxNum=%d", mTag.c_str(), mQueue.size(), mMaxNum);
		return false;
	}

	mQueue.push_back(data);

	vhall_cond_signal(&mCondNotEmpty);

	return true;
}

SafeData * SafeDataQueue::ReadQueue(bool block)
{
	VhallAutolock _Lock(&mMutex);

	UpdataState();

	if (mQueue.size() == 0) {
		if (block) {
			do{
				//vhall_cond_wait(&mCondNotEmpty, &mMutex);
            vhall_cond_wait_time(&mCondNotEmpty, &mMutex,COND_WAIT_TIME); //15s
			} while (false);
		}else{
			LOGE("%s ReadQueue faild1", mTag.c_str());
			return NULL;
		}
	}

	if (mQueue.size() == 0){
		LOGE("%s ReadQueue faild2", mTag.c_str());
		return NULL;
	}

	SafeData *ret = mQueue.front();

	mQueue.pop_front();

	vhall_cond_signal(&mCondNotFull);

	return ret;
}

std::list<SafeData*> SafeDataQueue::GetListFromQueue(VHFrameType frame_type){
   VhallAutolock _Lock(&mMutex);
   std::list<SafeData*> list;
   list.clear();
   for (auto iter = mQueue.begin(); iter!=mQueue.end(); iter++) {
      if ((*iter)->mType == frame_type) {
         (*iter)->SelfCopy();
         list.push_back(*iter);
      }
   }
   return list;
}

void SafeDataQueue::PushFrontList2Queue(std::list<SafeData*> *list){
   VhallAutolock _Lock(&mMutex);
   for (auto iter:*list) {
      mQueue.push_back(iter);
   }
}

uint64_t SafeDataQueue::ReadQueueItemTS(){
   VhallAutolock _Lock(&mMutex);
   if (mQueue.size() == 0){
      return -1;
   }
   SafeData *ret = mQueue.front();
   return ret->mTs;
}

int SafeDataQueue::GetQueueSize(){
	VhallAutolock _Lock(&mMutex);
	int size = 0;
	size = mQueue.size();
	return size;
}

void SafeDataQueue::SetMaxNum(uint32_t max_num){
   VhallAutolock _Lock(&mMutex);
   mMaxNum = max_num;
   mMinWarning = (uint32_t)(mMaxNum * mMinWarning);
   mMAXWarning = (uint32_t)(mMaxNum * mMAXWarning);
}

uint32_t SafeDataQueue::GetMaxNum()
{
	return mMaxNum;
}

bool SafeDataQueue::IsFull(){
   int size = GetQueueSize();
   if (size>=mMaxNum) {
      return true;
   }
   return false;
}

void SafeDataQueue::Reset(bool isResetDropCount){

	VhallAutolock _Lock(&mMutex);
	while (mQueue.size() > 0){
		SafeData *data = mQueue.front();
		mQueue.pop_front();
		data->SelfRelease();
	}
	if (isResetDropCount){
		mDropCount = 0;
	}
	mState = SAFE_DATA_QUEUE_STATE_EMPTY;
	mWaitFlag = false;
	mWaitFrameType = VIDEO_B_FRAME;
	mClearFlag = false;
	LOGD("%p Reset %s", this, mTag.c_str());
}

void SafeDataQueue::SetAllQueueItemTS(const uint64_t ts){
   VhallAutolock _Lock(&mMutex);
   for (auto iter = mQueue.begin(); iter != mQueue.end(); iter++ ) {
      (*iter)->mTs = ts;
   }
   LOGD("%s SetAllQueueItemTS:%llu", mTag.c_str(),ts);
}

//this will wake up thread reading this queue.
//if and after this buffer can not push and read.
//if you want to use this buffer again,call reset.
void SafeDataQueue::ClearAllQueue()
{
	VhallAutolock _Lock(&mMutex);
	while (mQueue.size() > 0){
		SafeData *data = mQueue.front();
      if (data->mType == SCRIPT_FRAME) {
         LOGW("amf0 msg release!");
      }
		mQueue.pop_front();
		data->SelfRelease();
	}

	mClearFlag = true;

	vhall_cond_signal(&mCondNotEmpty);
	
	vhall_cond_signal(&mCondNotFull);
	
	LOGD("%s ClearAllQueue", mTag.c_str());
}

void SafeDataQueue::SetTag(std::string tag)
{
	VhallAutolock _Lock(&mMutex);
	mTag = tag;
}

std::string SafeDataQueue::GetTag(){
	VhallAutolock _Lock(&mMutex);
	return mTag;
}

void SafeDataQueue::SetFrameDropType(DropFrameType type)
{
	VhallAutolock _Lock(&mMutex);
	if (type >= DROP_NONE && type <= DROP_ONE_B_P_I_A){
		mDropType = type;
	}
}

DropFrameType SafeDataQueue::GetFrameDropType(){
	VhallAutolock _Lock(&mMutex);
	DropFrameType type;
	type = mDropType;
	return mDropType;
}

int SafeDataQueue::GetFrameDropCount(){
	VhallAutolock _Lock(&mMutex);
	int count;
	count = mDropCount;
	return count;
}

int SafeDataQueue::GetQueueDataSize(){
	VhallAutolock _Lock(&mMutex);
	int ret = 0;
	for (auto it = mQueue.begin(); it != mQueue.end(); it++){
		ret += (*it)->mSize;
	}
	return ret;
}

int SafeDataQueue::GetKeyFrameCount(){
   VhallAutolock _Lock(&mMutex);
   int ret = 0;
   for (auto it = mQueue.begin(); it != mQueue.end(); it++){
      if((*it)->mType == VIDEO_I_FRAME){
         ret += 1;
      }
   }
   return ret;
}

uint32_t SafeDataQueue::GetQueueDataDuration()
{
	VhallAutolock _Lock(&mMutex);
	uint32_t ret;
	int duration;
	if (mQueue.size() <= 1){
		return 0;
	}
	duration = (uint32_t)(mQueue.back()->mTs - mQueue.front()->mTs);
	ret = duration > 0 ? duration : 0;
	return ret;
}

void SafeDataQueue::UpdataState(){
	int nQueueSize = (int)mQueue.size();
	SafeDataQueueState now_state = mState;
	if (nQueueSize == 0){
		now_state = SAFE_DATA_QUEUE_STATE_EMPTY;
	}
	if (nQueueSize >= mMinWarning){
		now_state = SAFE_DATA_QUEUE_STATE_MIN_WARN;
	}
   if (nQueueSize >= mMAXWarning) {
      now_state = SAFE_DATA_QUEUE_STATE_MAX_WARN;
   }
	if (nQueueSize == mMaxNum){
		now_state = SAFE_DATA_QUEUE_STATE_FULL;
	}
	if (mStateCallback&&now_state != mState){
		mStateCallback->OnSafeDataQueueChange(now_state, mTag);
		mState = now_state;
	}
}

int  SafeDataQueue::DropFrame(){
	int ret = 0;
	switch (mDropType){
	case DROP_NONE:
		ret = 0;
		break;

	case DROP_GOPS:
		ret = DropGops();
		break;

	case DROP_ALL_VIDEO:
		ret = DropAll(VIDEO_I_FRAME);
		break;

	case DROP_ALL:
		ret = DropAll(AUDIO_FRAME);
		break;

	case DROP_ALL_B:  //drop gop and audio bettow it. 
		ret = DropAll(VIDEO_B_FRAME);
		break;

	case DROP_ALL_B_P:  //first drop B then P then I, do not drop audio, if fall and only audio in it, return failed
		if ((ret = DropAll(VIDEO_B_FRAME)) <= 0){
			ret = DropAll(VIDEO_P_FRAME);
		}
		break;

	case DROP_ALL_B_P_I:
		if ((ret = DropAll(VIDEO_B_FRAME)) <= 0 &&
			(ret = DropAll(VIDEO_P_FRAME)) <= 0){
			ret = DropAll(VIDEO_I_FRAME);
		}
		break;

	case DROP_ALL_B_P_I_A:  //first drop B then P then I then audio, never failed
		if ((ret = DropAll(VIDEO_B_FRAME)) <= 0 &&
			(ret = DropAll(VIDEO_P_FRAME)) <= 0 &&
			(ret = DropAll(VIDEO_I_FRAME)) <= 0){
			ret = DropAll(AUDIO_FRAME);
		}
		break;

	case DROP_ONE_B:  //drop gop and audio bettow it. 
		ret= DropOne(VIDEO_B_FRAME);
		break;

	case DROP_ONE_B_P:  //first drop B then P then I, do not drop audio, if fall and only audio in it, return failed
		if ((ret = DropOne(VIDEO_B_FRAME)) <= 0){
			ret = DropOne(VIDEO_P_FRAME);
		}
		break;

	case DROP_ONE_B_P_I:
		if ((ret = DropOne(VIDEO_B_FRAME)) <= 0 &&
			(ret = DropOne(VIDEO_P_FRAME)) <= 0){
			ret = DropOne(VIDEO_I_FRAME);
		}
		break;

	case DROP_ONE_B_P_I_A:  //first drop B then P then I then audio, never failed
		if ((ret = DropOne(VIDEO_B_FRAME)) <= 0 &&
			(ret = DropOne(VIDEO_P_FRAME)) <= 0 &&
			(ret = DropOne(VIDEO_I_FRAME)) <= 0){
			ret = DropOne(AUDIO_FRAME);
		}
		break;

	default:
		LOGE("mDropType is invalid");
		break;
	}
	mDropCount += ret;
	return ret;
}

int  SafeDataQueue::DropGops(){
	LOGD("%s DropGops in" ,mTag.c_str());
	int ret = 0;
	auto it = mQueue.end();
	//find last I frame.
	while (it != mQueue.begin()){
		it--;
		if ((*it)->mType == VIDEO_I_FRAME){
			LOGD("%s DropGops find Key Frame", mTag.c_str());
			break;
		}	
	}

	int highest_drop_video_type = VIDEO_B_FRAME + 1;

	//no I frame or I frame is the first in list, clear all and wait for new I
	if (it == mQueue.begin()) {
		LOGD("%s DropGops it == mQueue.begin() type=%d ts=%llu",
			mTag.c_str(),(*it)->mType, (*it)->mTs);
		while (it != mQueue.end()) {
			//do not drop header.
			if ((*it)->mType == AUDIO_HEADER || (*it)->mType == VIDEO_HEADER || (*it)->mType == SCRIPT_FRAME){
				it++;
				continue;
			}

			if ((*it)->mType > AUDIO_FRAME && (*it)->mType < highest_drop_video_type) {
				highest_drop_video_type = (*it)->mType;
			}

			(*it)->SelfRelease();
			it = mQueue.erase(it);
			ret++;
		}
	}else { //find last I frame remove all before it
		LOGD("%s DropGops find last I frame remove all before it", mTag.c_str());
		auto temp = mQueue.begin();
		while (temp != it){
			//do not drop header.
			if ((*temp)->mType == AUDIO_HEADER || (*temp)->mType == VIDEO_HEADER || (*temp)->mType == SCRIPT_FRAME){
				temp++;
				continue;
			}

			if ((*temp)->mType > AUDIO_FRAME && (*temp)->mType < highest_drop_video_type) {
				highest_drop_video_type = (*temp)->mType;
			}

			(*temp)->SelfRelease();
			temp = mQueue.erase(temp);
			ret++;
		}	
	}

	//check if have drop video frame
	if (highest_drop_video_type <= VIDEO_B_FRAME)
	{
		mWaitFlag = true;
		//have drop P or I frame, next should be higher than P
		if (highest_drop_video_type <= VIDEO_P_FRAME){
			mWaitFrameType = VIDEO_I_FRAME;
		}
		else { //only drop B frame, next should be higher than B
			mWaitFrameType = VIDEO_P_FRAME;
		}
	}
	LOGD("%s DropGops out mWaitFlag=%d mWaitFrameType=%d", mTag.c_str(), mWaitFlag, mWaitFrameType);
	return ret;
}

int SafeDataQueue::DropAll(int type){
	//make sure audio header and video header is never be droped
	assert(type >= AUDIO_FRAME);

	int ret = 0;
	int highest_drop_video_type = VIDEO_B_FRAME + 1;

	auto it = mQueue.begin();
	while (it != mQueue.end()) {
		if ((*it)->mType >= type) {
			if ((*it)->mType > AUDIO_FRAME && (*it)->mType < highest_drop_video_type) {
				highest_drop_video_type = (*it)->mType;
			}
			(*it)->SelfRelease();
			it = mQueue.erase(it);
			ret++;
			continue;
		}
		it++;
	}

	if (highest_drop_video_type <= VIDEO_B_FRAME)
	{
		mWaitFlag = true;
		//have drop P or I frame, next should be higher than P
		if (highest_drop_video_type <= VIDEO_P_FRAME){
			mWaitFrameType = VIDEO_I_FRAME;
		}
		else { //only drop B frame, next should be higher than B
			mWaitFrameType = VIDEO_P_FRAME;
		}
	}
	return ret;
}
int  SafeDataQueue::DropOne(int type){

	//make sure audio header and video header is never be droped
	assert(type >= AUDIO_FRAME);

	int ret = 0;
	int highest_drop_video_type = VIDEO_B_FRAME + 1;

	auto it = mQueue.begin();
	while (it != mQueue.end()) {
		if ((*it)->mType >= type) {
			if ((*it)->mType > AUDIO_FRAME && (*it)->mType < highest_drop_video_type) {
				highest_drop_video_type = (*it)->mType;
			}
			(*it)->SelfRelease();
			it = mQueue.erase(it);
			ret++;
			break;
		}
		it++;
	}
	
	//check if need to drop more and set mWaitFlag

	//drop a P or I frame, all P/B frame after this sould be droped
	if (highest_drop_video_type <= VIDEO_P_FRAME) { 

		bool is_find_i = false;
		while (it != mQueue.end()) {
			if ((*it)->mType >= VIDEO_P_FRAME) {
				(*it)->SelfRelease();
				it = mQueue.erase(it);
				ret++;
				continue;
			}
			//if meet I frame stoped
			else if ((*it)->mType == VIDEO_I_FRAME){
				is_find_i = true;
				break;
			}
			it++;
		}
		if (!is_find_i){
			mWaitFlag = true;
			mWaitFrameType = VIDEO_I_FRAME;
		}
	}else if (highest_drop_video_type <= VIDEO_B_FRAME){ //just drop a B frame
		bool is_find_p = false;
		while (it != mQueue.end()) {
			if ((*it)->mType >= VIDEO_B_FRAME) {
				(*it)->SelfRelease();
				it = mQueue.erase(it);
				ret++;
				continue;
			}
			//if meet higher than B frame stoped
			else if ((*it)->mType == VIDEO_I_FRAME ||
					 (*it)->mType == VIDEO_P_FRAME){
				is_find_p = true;
				break;
			}
			it++;
		}
		if (!is_find_p){
			mWaitFlag = true;
			mWaitFrameType = VIDEO_P_FRAME;
		}	
	}

	return ret;
}
