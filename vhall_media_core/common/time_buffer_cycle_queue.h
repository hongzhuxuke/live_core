//
//  TimeBufferCycleQueue.hpp
//  VinnyLive
//
//  Created by liwenlong on 15/12/18.
//  Copyright © 2015年 vhall. All rights reserved.
//

#ifndef TimeBufferCycleQueue_hpp
#define TimeBufferCycleQueue_hpp

#include <string>
#include <stdlib.h>
#include <live_sys.h>
#include <stdint.h>
typedef struct TimeFrame_t
{
   char    *data;
   int     size;
   int     buffer_size;
   uint32_t  ts;
   TimeFrame_t  *next;
public:
   void ClearData()
   {
      size = 0;
      ts = 0;
   }
   
} TimeFrame_t;

class TimeBufferCycleQueue
{
   
public:
   
   TimeBufferCycleQueue(int max_num);
   
   ~TimeBufferCycleQueue(){
      TimeFrame_t *tmp_ptr = mReadPtr->next;
      mReadPtr->next = NULL;
      while (tmp_ptr != NULL){
         TimeFrame_t * frame_t = tmp_ptr;
         tmp_ptr = (TimeFrame_t *)tmp_ptr->next;
         if (frame_t->data){
            free(frame_t->data);
         }
         if (frame_t){
            free(frame_t);
            frame_t = NULL;
         }
      }
	  vhall_lock_destroy(&mMutex);
	  vhall_cond_destroy(&mCondC);
   };
   bool Write(const char * data, const int size, const uint32_t ts);
   
   bool Read(TimeFrame_t &timeframe);
   
   void Clear();
   
   bool IsFull(){
      return (mReadPtr == mWritePtr&&mReadPtr->size>0);
   }
   
   bool IsEmpty(){
      return (mReadPtr == mWritePtr&&mReadPtr->size<=0);
   }
   
private:
   TimeBufferCycleQueue(){};
   
   int Size() { return mSize; }
   
   volatile int mMaxNum;
   volatile int mSize;
   
   vhall_lock_t mMutex;
   vhall_cond_t  mCondC;
   
   TimeFrame_t * mReadPtr;
   TimeFrame_t * mWritePtr;
};


#endif /* TimeBufferCycleQueue_hpp */
