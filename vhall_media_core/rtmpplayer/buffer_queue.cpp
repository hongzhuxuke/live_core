#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "buffer_queue.h"
#include "../common/vhall_log.h"
#include <stdint.h>

void v_mtuex_init(v_mutex_t * mutex) {
#ifdef _WIN32
   InitializeCriticalSection(mutex);
#else
   pthread_mutex_init(mutex,NULL);
#endif
}

void v_cond_init(v_cond_t * cond) {
#ifdef _WIN32
   *cond = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
   pthread_cond_init(cond,NULL);
#endif
}

void v_lock_mutex(v_mutex_t * mutex) {
#ifdef _WIN32
   EnterCriticalSection(mutex);
#else
   pthread_mutex_lock(mutex);
#endif
}

void v_unlock_mutex(v_mutex_t * mutex) {
#ifdef _WIN32
   LeaveCriticalSection(mutex);
#else
   pthread_mutex_unlock(mutex);
#endif
}

void v_cond_signal(v_cond_t * cond) {
#ifdef _WIN32
   SetEvent(*cond);
#else
   pthread_cond_signal(cond);
#endif
}
void v_cond_wait(v_cond_t * cond, v_mutex_t * mutex) {
#ifdef _WIN32
   LeaveCriticalSection(mutex);
   WaitForSingleObject(*cond, -1);
   EnterCriticalSection(mutex);
#else
   pthread_cond_wait(cond,mutex);
#endif
}
void v_cond_wait_timeout(v_cond_t * cond, v_mutex_t * mutex, unsigned long timeInMs) {
#ifdef _WIN32
   LeaveCriticalSection(mutex);
   WaitForSingleObject(*cond, timeInMs);
   EnterCriticalSection(mutex);
#else
   /*struct timeval now;
    gettimeofday(&now, NULL);
    outtime.tv_sec = now.tv_sec + 5;
    outtime.tv_nsec = now.tv_usec * 1000;
    pthread_cond_timedwait(cond, mutex, outtime);*/
#endif
}

void v_mutex_destroy(v_mutex_t * mutex) {
#ifdef _WIN32
   DeleteCriticalSection(mutex);
#else
   pthread_mutex_destroy(mutex);
#endif
}

void v_cond_destroy(v_cond_t * cond) {
#ifdef _WIN32
   CloseHandle(*cond);
#else
   pthread_cond_destroy(cond);
#endif
}

BufferQueue::BufferQueue(long bufferSize, const int& maxQueueSize) :
mBufferSize(bufferSize),
mMaxQueueSize(maxQueueSize),
mDataUnitCnt(0) {
   init();
}

BufferQueue::~BufferQueue() {
   destory();
}

long BufferQueue::GetBufferSize() {
   return mBufferSize;
}

void BufferQueue::SetQueueSize(const int& maxQueueSize){
   mMaxQueueSize = maxQueueSize;
   if(maxQueueSize < mDataUnitCnt)
      LOGW(" will strict queue");
}

int BufferQueue::GetQueueSize(){
   return mMaxQueueSize;
}

DataUnit* BufferQueue::MallocDataUnit(const long& bufferSize, bool block) {
   DataUnit* dataUnit = NULL;
   if (PopUnitfromQueue(&mFreeQueue, &dataUnit, false) != 0){
      if(mMaxQueueSize > mDataUnitCnt){
         dataUnit = (DataUnit*)malloc(DataUnitSize);
         if (dataUnit) {
            dataUnit->unitBuffer = (unsigned char*)malloc(bufferSize);
            dataUnit->unitBufferSize = bufferSize;
            dataUnit->timestap = 0;
            dataUnit->isKey = false;
            dataUnit->next = NULL;
            mDataUnitCnt++;
         }
      }
   }
   
   if(dataUnit == NULL && block){
      LOGW("MallocDataUnit is block");
      PopUnitfromQueue(&mFreeQueue, &dataUnit, block);
   }
   if (dataUnit){
      if (dataUnit->unitBufferSize < bufferSize) {
         if (dataUnit->unitBuffer)
            free(dataUnit->unitBuffer);
         dataUnit->unitBuffer = (unsigned char*)malloc(bufferSize);
         dataUnit->unitBufferSize = bufferSize;
      }
   }
   return dataUnit;
}

void BufferQueue::FreeDataUnit(DataUnit* dataUnit) {
   AppendUnit2Queue(&mFreeQueue, dataUnit);
}

DataUnit* BufferQueue::GetDataUnit(bool block) {
   DataUnit* dataUnit = NULL;
   PopUnitfromQueue(&mBufferQueue, &dataUnit, block);
   //assert
   return dataUnit;
}

int BufferQueue::PutDataUnit(DataUnit* dataUnit) {
   return AppendUnit2Queue(&mBufferQueue, dataUnit);
}

void BufferQueue::Flush() {
   v_lock_mutex(&mBufferQueue.mutex);
   mBufferQueue.isExit = 1;
   v_cond_signal(&mBufferQueue.cond);
   v_unlock_mutex(&mBufferQueue.mutex);
   
   v_lock_mutex(&mFreeQueue.mutex);
   mFreeQueue.isExit = 1;
   v_cond_signal(&mFreeQueue.cond);
   v_unlock_mutex(&mFreeQueue.mutex);
}

void BufferQueue::Reset(){
   v_lock_mutex(&mBufferQueue.mutex);
   mBufferQueue.isExit = 0;
   v_unlock_mutex(&mBufferQueue.mutex);
   
   v_lock_mutex(&mFreeQueue.mutex);
   mFreeQueue.isExit = 0;
   v_unlock_mutex(&mFreeQueue.mutex);
   DataUnit* dataUnit = NULL;
   while (PopUnitfromQueue(&mBufferQueue, &dataUnit, false) == 0) {
      AppendUnit2Queue(&mFreeQueue, dataUnit);
   }
}

int BufferQueue::DropUnit(const uint64_t& minTimestamp, const int & unitCnt, uint64_t& lastUnitTimestamp){
   return FreeUnit(&mBufferQueue, minTimestamp, unitCnt, lastUnitTimestamp);
}

void BufferQueue::SetPerItemTimestamp(const uint64_t& timestamp){
   Queue *q= &mBufferQueue;
   v_cond_signal(&q->cond);
   v_lock_mutex(&q->mutex);
   DataUnit*dataUnit = q->head;
   while (dataUnit&&q->isExit == 0) {
      dataUnit->timestap = timestamp;
      dataUnit= dataUnit->next;
   }
   v_unlock_mutex(&q->mutex);
}

uint64_t BufferQueue::GetHeadTimestamp() {
   uint64_t timestamp = 0;
   if (mBufferQueue.head)
      timestamp = mBufferQueue.head->timestap;
   return timestamp;
}

uint64_t BufferQueue::GetTailTimestamp() {
   uint64_t timestamp = 0;
   if (mBufferQueue.tail)
      timestamp = mBufferQueue.tail->timestap;
   return timestamp;
}

int BufferQueue::GetDataUnitCnt(){
   return GetQueueSize(&mBufferQueue);
}

int BufferQueue::GetFreeUnitCnt(){
   return GetQueueSize(&mFreeQueue);
}

uint64_t BufferQueue::GetKeyUnitTimestap(){
   uint64_t timestap = 0;
   Queue *q= &mBufferQueue;
   v_cond_signal(&q->cond);
   v_lock_mutex(&q->mutex);
   DataUnit*dataUnit = q->head;
   while (q->isExit == 0) {
      if (dataUnit != NULL) {
         if(dataUnit->isKey == true){
            timestap = dataUnit->timestap;
            break;
         }
         dataUnit= dataUnit->next;
      }else{
         break;
      }
   }
   v_unlock_mutex(&q->mutex);
   return timestap;
}

void BufferQueue::init() {
   memset(&mFreeQueue, 0, QueueSize);
   memset(&mBufferQueue, 0, QueueSize);
   v_mtuex_init(&mFreeQueue.mutex);
   v_cond_init(&mFreeQueue.cond);
   v_mtuex_init(&mBufferQueue.mutex);
   v_cond_init(&mBufferQueue.cond);
}

void BufferQueue::destory() {
   //free memory
   FreeQueue(&mFreeQueue);
   FreeQueue(&mBufferQueue);
   v_mutex_destroy(&mFreeQueue.mutex);
   v_cond_destroy(&mFreeQueue.cond);
   v_mutex_destroy(&mBufferQueue.mutex);
   v_cond_destroy(&mBufferQueue.cond);
}

int BufferQueue::AppendUnit2Queue(Queue *q, DataUnit *dataUnit) {
   dataUnit->next = NULL;
   v_lock_mutex(&q->mutex);
   if (q->head == NULL) {
      q->head = dataUnit;
   } else {
      q->tail->next = dataUnit;
   }
   q->tail = dataUnit;
   q->unitNumber++;
 //  LOGD("%ld/%ld AppendUnit2Queue %d", (long)this, (long)q, q->unitNumber );
   v_cond_signal(&q->cond);
   v_unlock_mutex(&q->mutex);
   return 1;
}

int BufferQueue::PopUnitfromQueue(Queue *q, DataUnit **dataUnit, bool block) {
   int ret = 1;
   *dataUnit = NULL;
   v_lock_mutex(&q->mutex);
   while (q->isExit == 0) {
      *dataUnit = q->head;
      if (q->head != NULL) {
         q->head = q->head->next;
         if (q->head == NULL)
            q->tail = NULL;
         q->unitNumber--;
         if(q->unitNumber < 0)
            LOGD(" error %ld/%ld", (long)this, (long)q);
 //        LOGD("%ld/%ld PopUnitfromQueue %d", (long)this, (long)q, q->unitNumber );
         ret = 0;
         break;
      } else if (block) {
         v_cond_wait(&q->cond, &q->mutex);
      } else {
         ret = -1;
         break;
      }
   }
   v_unlock_mutex(&q->mutex);
   return ret;
}

int BufferQueue::GetQueueSize(Queue *q) {
   int ret = 0;
   v_lock_mutex(&q->mutex);
   ret = q->unitNumber;
   v_unlock_mutex(&q->mutex);
   return ret;
}

void BufferQueue::FreeQueue(Queue *q) {
   q->isExit = 1;
   v_cond_signal(&q->cond);
   v_lock_mutex(&q->mutex);
   while (true) {
      DataUnit*dataUnit = q->head;
      if (q->head != NULL) {
         q->head = q->head->next;
         if (q->head == NULL)
            q->tail = NULL;
         q->unitNumber--;
         //LOGD("FreeQueue %d",q->unitNumber );
         if (dataUnit->unitBufferSize)
            free(dataUnit->unitBuffer);
         free(dataUnit);
      } else {
         break;
      }
   }
   v_unlock_mutex(&q->mutex);
}

int BufferQueue::FreeUnit(
                          Queue *dataQueue,
                          const uint64_t& minTimestamp,
                          const int & unitCnt,
                          uint64_t& lastUnitTimestamp){
   return 0;
   v_lock_mutex(&dataQueue->mutex);
   DataUnit *dataUnit = dataQueue->head;
   DataUnit *pre = dataUnit;
   int actualFreeUnitCnt = 0;
   if(dataUnit){
      //int delt = dataQueue->unitNumber / unitCnt;
      dataUnit = dataUnit->next; // first data don't drop
      int deltaIndex = 0;
      while (dataUnit && dataQueue->isExit == 0) {
         if(deltaIndex == 0 &&
            dataUnit->timestap > lastUnitTimestamp
            && dataUnit->isKey == false ){
            //drop data from data queue
            
            pre->next = dataUnit->next;
            dataQueue->unitNumber--;
            LOGD("%ld/%ld FreeUnit %d", (long)this, (long)dataQueue, dataQueue->unitNumber );
            lastUnitTimestamp = dataUnit->timestap;
            LOGD("Drop frame timestamp=%ld, keyframe=%d, size=%ld",
                 (long)dataUnit->timestap, dataUnit->isKey?1:0, (long)dataUnit->dataSize);
            FreeDataUnit(dataUnit);
            dataUnit = pre->next;
            deltaIndex = dataQueue->unitNumber / (unitCnt - actualFreeUnitCnt);
            actualFreeUnitCnt ++;
            continue;
         }else{
            if(deltaIndex> 0)
               deltaIndex --;
         }
         pre = dataUnit;
         dataUnit = dataUnit->next;
      }//end for while
      
   }
   v_unlock_mutex(&dataQueue->mutex);
   return actualFreeUnitCnt;
}
