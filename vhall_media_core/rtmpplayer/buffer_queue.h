#ifndef _BUFFER_QUEUE_INCLUDE_
#define _BUFFER_QUEUE_INCLUDE_
#ifdef _WIN32
//llc include windows in winsock2.h 
#include <winsock2.h>
#else
#include <pthread.h>
#endif
#include <stdint.h>

#ifdef _WIN32
typedef CRITICAL_SECTION v_mutex_t;
typedef HANDLE v_cond_t;
#else
typedef pthread_mutex_t v_mutex_t;
typedef pthread_cond_t v_cond_t;
#endif

void v_mtuex_init(v_mutex_t * mutex);
void v_cond_init(v_cond_t * cond);

void v_lock_mutex(v_mutex_t * mutex);
void v_unlock_mutex(v_mutex_t * mutex);
void v_cond_signal(v_cond_t * cond);
void v_cond_wait(v_cond_t * cond, v_mutex_t * mutex);
void v_cond_wait_timeout(v_cond_t * cond, v_mutex_t * mutex, unsigned long timeInMs);

void v_mutex_destroy(v_mutex_t * cond);
void v_cond_destroy(v_cond_t * cond);

typedef struct DataUnit {
   unsigned char* unitBuffer;
   uint64_t unitBufferSize;
   uint64_t dataSize;
   uint64_t timestap;
   bool isKey;
   DataUnit* next;
}DataUnit;

const int DataUnitSize = sizeof(DataUnit);

typedef struct Queue {
   DataUnit* head;
   DataUnit* tail;
   int unitNumber;
   v_mutex_t mutex;
   v_cond_t cond;
   int isExit;
}Queue;

const int QueueSize = sizeof(Queue);
/*
 Producer MallocDataUnit -> fill data unit -> PutDataUnit
 consumer  GetDataUnit-> use data unit -> FreeDataUnit
 */
class BufferQueue {
public:
   BufferQueue(long bufferSize, const int& maxQueueSize);
   virtual ~BufferQueue();
   long GetBufferSize();
   void SetQueueSize(const int& maxQueueSize);
   int GetQueueSize();
public:
   DataUnit* MallocDataUnit(const long& bufferSize, bool block = false);
   void FreeDataUnit(DataUnit* dataUnit);
   
   DataUnit* GetDataUnit(bool block);
   int PutDataUnit(DataUnit*);
   void Flush();
   void Reset();
   int DropUnit(const uint64_t& minTimestamp, const int & unitCnt, uint64_t& lastUnitTimestamp);
   
   void SetPerItemTimestamp(const uint64_t& timestamp);
   
   uint64_t GetHeadTimestamp();
   uint64_t GetTailTimestamp();
   int GetDataUnitCnt();
   int GetFreeUnitCnt();
   uint64_t GetKeyUnitTimestap(); //if no key ,will return 0;
private:
   void init();
   void destory();
   int AppendUnit2Queue(Queue *q, DataUnit *dataUnit);
   int PopUnitfromQueue(Queue *q, DataUnit **dataUnit, bool block);
   int GetQueueSize(Queue *q);
   void FreeQueue(Queue *q);
private:
   int FreeUnit(Queue *dataQueue, const uint64_t& minTimestamp, const int & unitCnt, uint64_t& lastUnitTimestamp);
private:
   //for unused
   Queue mFreeQueue;
   Queue mBufferQueue;
   long  mBufferSize;
   int   mMaxQueueSize;
   int   mDataUnitCnt;
};


#endif
