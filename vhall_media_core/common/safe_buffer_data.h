#ifndef __SAFE_BUFFER_DATA__
#define __SAFE_BUFFER_DATA__
/*
athor：liulichuan
email：lichuan.liu@vhall.com
all class here is thread safe.
*/
#include <atomic>
#include <stdint.h>
#include <live_sys.h>
#include <list>
#include <live_define.h>
#include <live_open_define.h>

class SafeDataPool;
class SafeData;

#define SINGLE_SAFE_DATA_DEBUG  0

typedef  void(*SafeDataFree)(void *user_data, SafeData *data);

class SafeData
{
	//self data
	friend SafeDataPool;
public: //outside use only can read.
	char * mData;
	int    mSize;
	int    mType;
	uint64_t  mTs;
	void  * mExtraData;
	void SelfRelease();
	SafeData * SelfCopy();
   int GetRefCount();
#if SINGLE_SAFE_DATA_DEBUG
	void SetDebug(std::string tag){ mDebug = true; mTag = tag;}
#endif
private:
	int    mBufferSize;
	volatile std::atomic_uint mRef;
	//free func
	void *mUserData;
	SafeDataFree mFreeFunc;
	SafeData *next;
#if SINGLE_SAFE_DATA_DEBUG
	std::string mTag;
	bool mDebug;
#endif
private:
	SafeData();
	~SafeData();
	int GetBufferSize();
	int Reset(int buffer_size);
	int WriteData(char * data, int size, int type, uint64_t ts);
	void SetFree(SafeDataFree ffunc, void *mUserData);
};

class SafeDataPool{
private:
	SafeData * mArraySafeDatas;

	SafeData * mFreeDatas;
	int mAutoLarge;
	std::list<SafeData*> mLargeList;

	int        mFreeCount;

	int        mMaxSize;

	vhall_lock_t mLock;
public:
	//set size=0 means use defult.
	//auto_large when pool is dry large it if set.
	SafeDataPool(int size = 10000, int auto_large = 1);
	~SafeDataPool();
	SafeData *GetSafeData(char *data, int size, int type, uint64_t ts);
	int GetFreeDateSize();
	int GetMaxDataSize();
private:

	SafeData *GetSafeData2(char *data, int size, int type, uint64_t ts);
	void  CheckPool();
	int LargeSize(int large_size);

	void RecycleData(SafeData * data);

	static void DataFree(void *user_data, SafeData * data);
};

enum SafeDataQueueState {
	SAFE_DATA_QUEUE_STATE_EMPTY,
	SAFE_DATA_QUEUE_STATE_NORMAL,
	SAFE_DATA_QUEUE_STATE_FULL,
   SAFE_DATA_QUEUE_STATE_MIN_WARN,
   SAFE_DATA_QUEUE_STATE_MAX_WARN,
};

class SafeDataQueueStateListener {
public:
	SafeDataQueueStateListener(){};
	virtual ~SafeDataQueueStateListener(){};
	virtual void OnSafeDataQueueChange(SafeDataQueueState state, std::string tag) = 0;
};

extern char *DropFrameTypeStr[];

typedef enum BUFFER_FULL_ACTION{
	ACTION_BLOCK = 0,
	ACTION_DROP_FAILD_RETURN,
	ACTION_DROP_FAILD_LARGE,
}BUFFER_FULL_ACTION;

class SafeDataQueue{
private:

	vhall_lock_t  mMutex;

	vhall_cond_t  mCondNotEmpty;
	vhall_cond_t  mCondNotFull;

	volatile uint32_t mMaxNum;
	volatile uint32_t mMinWarning;
	volatile uint32_t mMAXWarning;

	SafeDataQueueStateListener * mStateCallback;
	SafeDataQueueState       mState;
	std::string              mTag;
	DropFrameType            mDropType;
	int                      mDropCount;

	bool mWaitFlag;
	VHFrameType mWaitFrameType;

	bool mClearFlag;

	std::list<SafeData*> mQueue;

public:
	SafeDataQueue(SafeDataQueueStateListener *callback = NULL, float min_warning = 0.1, float max_warning = 0.9, int max_num = 0);
	//bool WriteQueue(const char* data, const int size, const int type, const uint32_t ts, const bool block = true);
	~SafeDataQueue();
	bool PushQueue(SafeData* data, BUFFER_FULL_ACTION action = ACTION_BLOCK);
	SafeData * ReadQueue(bool block = true);
   // get sameone type list
   std::list<SafeData*> GetListFromQueue(VHFrameType frame_type);
   // push front list to queue
   void PushFrontList2Queue(std::list<SafeData*> * list);
   // read first item ts
   uint64_t ReadQueueItemTS();
   void SetMaxNum(uint32_t max_num);
	int GetQueueSize();
	uint32_t GetMaxNum();
   bool IsFull();
   // set all item ts value
   void SetAllQueueItemTS(const uint64_t ts);
	void ClearAllQueue();
	void Reset(bool isResetDropCount = true);
	void SetTag(std::string tag);
	std::string GetTag();
	void SetFrameDropType(DropFrameType type);
	DropFrameType GetFrameDropType();
	int GetFrameDropCount();
	int GetQueueDataSize();
   int GetKeyFrameCount();
	uint32_t GetQueueDataDuration();
private:
	void UpdataState();
	int  DropFrame();
	int  DropGops();
	int  DropAll(int type);
	int  DropOne(int type);
};

#endif
