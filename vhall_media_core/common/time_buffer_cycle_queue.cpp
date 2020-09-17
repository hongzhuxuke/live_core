//
//  TimeBufferCycleQueue.cpp
//  VinnyLive
//
//  Created by liwenlong on 15/12/18.
//  Copyright © 2015年 vhall. All rights reserved.
//

#include "time_buffer_cycle_queue.h"
#include <string.h>
#include "../common/vhall_log.h"
#include <iostream>

TimeBufferCycleQueue::TimeBufferCycleQueue(int max_num):mMaxNum(max_num),mReadPtr(NULL),mWritePtr(NULL)
{
   mSize = 0;
   TimeFrame_t *tmp_ptr = NULL;
   vhall_cond_init(&mCondC);
   vhall_lock_init(&mMutex);
   for (int i =0; i<mMaxNum; i++){
      TimeFrame_t *frame_t = (TimeFrame_t*) calloc(1,sizeof(TimeFrame_t));
      if (frame_t == NULL) {
         LOGE("frame_t create failure!");
      }
      if (mWritePtr == NULL||mReadPtr == NULL){
         mReadPtr =  mWritePtr = tmp_ptr = frame_t;
         continue;
      }
      tmp_ptr->next = frame_t;
      tmp_ptr = frame_t;
   }
   tmp_ptr->next = mWritePtr;
   if (mWritePtr == NULL) {
      LOGE("TimeBufferCycleQueue init failed!");
   }
}

void TimeBufferCycleQueue::Clear(){
   LOGW("TimeBufferCycleQueue clear()调用！Size:%d",mSize);
   vhall_cond_signal(&mCondC);
   vhall_unlock(&mMutex);
}

bool TimeBufferCycleQueue::Write(const char *data, const int size, const uint32_t ts)
{
	vhall_lock(&mMutex);
   
   if (IsFull()) {
      LOGD("TimeBufferCycleQueue is full!");
      mReadPtr->ClearData();
      mReadPtr = mReadPtr->next;
   }
   
   if (mWritePtr->data == NULL){
      mWritePtr->data = (char *)calloc(1,sizeof(char)*size);
      if (mWritePtr->data == NULL) {
         LOGE("mWritePtr->data is NULL");
      }else{
         mWritePtr->buffer_size = size;
      }
   }else if(mWritePtr->buffer_size<size){
      mWritePtr->data = (char*)realloc(mWritePtr->data,sizeof(char)*size);
      if (mWritePtr->data) {
         mWritePtr->buffer_size = size;
      }else{
         LOGE("timeframe.data realloc error!");
      }
   }
   
   mWritePtr->size = size;
   mWritePtr->ts = ts;
   memcpy(mWritePtr->data, data, size);
   
   mWritePtr = mWritePtr->next;
   if (mSize<mMaxNum) {
      mSize++;
   }
   vhall_cond_signal(&mCondC);
   vhall_unlock(&mMutex);
   
   return true;
}

bool TimeBufferCycleQueue::Read(TimeFrame_t &timeframe)
{
	vhall_lock(&mMutex);
   if (IsEmpty()){
      LOGE("TimeBufferCycleQueue is Empty");
	  vhall_cond_wait(&mCondC, &mMutex);
   }
   
   TimeFrame_t * read_buffer = mReadPtr;
   if (read_buffer->data == NULL){
	   vhall_unlock(&mMutex);
      LOGE("read_buffer->data is NULL");
      return false;
   }
   
   if (timeframe.data==NULL) {
      timeframe.data = (char*)calloc(1, sizeof(char)*read_buffer->size);
      if (timeframe.data) {
         timeframe.buffer_size = read_buffer->size;
      }else{
          LOGE("timeframe.data calloc error!");
      }
   }else if(timeframe.buffer_size<read_buffer->size){
      timeframe.data = (char*)realloc(timeframe.data,sizeof(char)*read_buffer->size);
      if (timeframe.data) {
         timeframe.buffer_size = read_buffer->size;
      }else{
          LOGE("timeframe.data realloc error!");
      }
   }
   
   timeframe.size = read_buffer->size;
   timeframe.ts = read_buffer->ts;
   memcpy(timeframe.data, read_buffer->data, read_buffer->size);
   
   if (read_buffer->next!=mWritePtr) {
      //清理数据
      read_buffer->ClearData();
      mReadPtr = mReadPtr->next;
      if (mSize>0) {
         mSize--;
      }
   }else{
      LOGD("TimeBufferCycleQueue has only data!");
   }

   vhall_unlock(&mMutex);
   return true;
}
