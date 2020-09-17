//
//  TimeBufferQueue.cpp
//  VinnyLive
//
//  Created by liwenlong on 16/1/28.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "time_buffer_queue.h"
#include "../common/vhall_log.h"
#include "../common/live_define.h"
#include "live_sys.h"
#include <string.h>

#define Default_Count       3  //初始化默认的item个数
#define Buf_Warning_Size    30 //缓冲预警的buf个数

TimeBufferQueue::TimeBufferQueue(int max_num):m_max_num(max_num),m_read_ptr(NULL),m_write_ptr(NULL),m_end_ptr(NULL),m_callback(NULL){
   OnInit(max_num);
}

TimeBufferQueue::TimeBufferQueue(const BufferCallback & callback ,int max_num):m_max_num(max_num),m_read_ptr(NULL),m_write_ptr(NULL),m_end_ptr(NULL),m_callback((BufferCallback*)&callback)
{
   OnInit(max_num);
}

void TimeBufferQueue::OnInit(int max_num)
{
   m_buffer_state = BUFFER_STATE_NONE;
   BufferItem *tmp_ptr = NULL;
   vhall_cond_init(&m_cond_c);
   vhall_cond_init(&m_cond_p);
   vhall_lock_init(&mutex);
   m_size = 0;
   m_count = Default_Count;
   m_key_index = 0;
   m_tag = "";
   for (int i = 0; i<Default_Count; i++) {
      BufferItem *item = (BufferItem*)calloc(1,sizeof(BufferItem));
      if (item == NULL) {
         LOGE("BufferItem calloc fail!");
         continue;
      }
      item->index = i;
      if (m_read_ptr == NULL) {
         m_read_ptr = m_write_ptr = tmp_ptr=item;
         continue;
      }
      tmp_ptr->next = item;
      item->previous = tmp_ptr;
      tmp_ptr = item;
   }
   m_end_ptr = tmp_ptr;
}

void TimeBufferQueue::SetTag(std::string tag)
{
   m_tag = tag;
}

bool TimeBufferQueue::ReadQueue(BufferItem &rItem){
   
	vhall_lock(&mutex);

   if (IsEmpty()) {
      if (m_buffer_state == BUFFER_STATE_FULL&&m_callback) {
		  m_buffer_state = BUFFER_STATE_EMPTY;
         m_callback->OnBufferStateChange(m_buffer_state);
         LOGW("上行网络卡顿恢复!");
      }
	  vhall_cond_wait(&m_cond_c, &mutex);
   }
   
   if (m_read_ptr->data==NULL||m_read_ptr->size<=0) {
      LOGE("%s读队列失败！",m_tag.c_str());
	  vhall_unlock(&mutex);
      return false;
   }
   
   BufferItem *item = m_read_ptr;
   m_read_ptr = item->next;
   m_read_ptr->previous = NULL;
   //
   if (rItem.data==NULL) {
      rItem.data = (char*)calloc(1,item->size);
      if (rItem.data) {
         rItem.buffer_size = item->size;
      }else{
         LOGE("item->data calloc error!");
      }
   }else if(item->size>rItem.buffer_size){
      rItem.data = (char*)realloc(rItem.data,item->size);
      if (rItem.data) {
         rItem.buffer_size = item->size;
      }else{
         LOGE("item->data calloc error!");
      }
   }
   
   //对数据进行复制
   memcpy(rItem.data ,item->data, item->size);
   rItem.size = item->size;
   rItem.ts = item->ts;
   rItem.type = item->type;
   //清空item的数据
   item->ClearItem();
   
   m_end_ptr->next = item;
   item->previous = m_end_ptr;
   m_end_ptr = item;
   
   if (m_write_ptr == NULL) {
      m_write_ptr = item;
   }
   m_size--;
   vhall_cond_signal(&m_cond_p);
   vhall_unlock(&mutex);
   return true;
}

bool TimeBufferQueue::WriteQueue(const char* data,const int size,const int type,const uint32_t ts ,const bool block)
{
	vhall_lock(&mutex);
   if (m_buffer_state != BUFFER_STATE_FULL&&m_callback&&m_size>Buf_Warning_Size) {
      m_buffer_state = BUFFER_STATE_FULL;
      m_callback->OnBufferStateChange(m_buffer_state);
      LOGW("上行网络卡顿!");
   }
   if (IsFull()) {
      if (m_count<m_max_num) {
         BufferItem *item = (BufferItem*)calloc(1,sizeof(BufferItem));
         item->previous = m_end_ptr;
         m_end_ptr->next = item;
         m_end_ptr = item;
         m_write_ptr = item;
         m_count++;
      }else{
         if (block) {
            LOGW("%s wirte block!",m_tag.c_str());
			vhall_cond_wait(&m_cond_p, &mutex);
         }else{
            DiscardGop();
         }
      }
   }
   BufferItem *item = m_write_ptr;
   if (item==NULL) {
      LOGE("队列写指针为空！");
	  vhall_unlock(&mutex);
      return false;
   }

   if (item->data == NULL) {
      item->data = (char*)calloc(1,size);
      if (item->data) {
         item->buffer_size = size;
      }else{
         LOGE("item->data calloc error!");
      }
   }else if(size>item->buffer_size){
      item->data = (char*)realloc(item->data,size);
      if (item->data) {
         item->buffer_size = size;
      }else{
         LOGE("item->data calloc error!");
      }
   }
   if (item->data) {
       memcpy(item->data, data, size);
   }
   item->size = size;
   item->ts = ts;
   item->type = type;
   if (m_size<m_max_num) {
      m_size++;
   }
   m_write_ptr = m_write_ptr->next;
   vhall_cond_signal(&m_cond_c);
   vhall_unlock(&mutex);
   return true;
}

void TimeBufferQueue::DiscardGop()
{
   //丢整个的Gop策略
   BufferItem *first_item = NULL;
   BufferItem *second_item = NULL;
   BufferItem *p_read = m_end_ptr;
   
   while (p_read!=NULL) {
      if (first_item==NULL&&p_read->type == VIDEO_I_FRAME) {
         first_item = p_read;
         p_read = p_read->previous;
      }
      if (first_item&&p_read->type == VIDEO_I_FRAME) {
         second_item = p_read;
         break;
      }
      p_read = p_read->previous;
   }
   
   if (first_item&&second_item&&first_item!=second_item) {
      
      BufferItem * first_p_item = first_item->previous;
      BufferItem * second_p_item = second_item->previous;
      
      first_item->previous = second_p_item;
      if (second_p_item) {
         second_p_item->next = first_item;
      }else{
         m_read_ptr = first_item;
      }
      
      second_item->previous = m_end_ptr;
      m_end_ptr->next = second_item;
      m_write_ptr = second_item;
      
      m_end_ptr = first_p_item;
      first_p_item->next = NULL;
      BufferItem * tmp_item = m_write_ptr;
      
      while (tmp_item!=NULL) {
         m_size--;
         tmp_item->ClearData();
         tmp_item = tmp_item->next;
      }
      
   }else if(first_item!=NULL&&second_item==NULL){
      m_write_ptr = first_item->next;
      BufferItem * tmp_item = m_write_ptr;
      while (tmp_item!=NULL) {
         m_size--;
         tmp_item->ClearData();
         tmp_item = tmp_item->next;
      }
      LOGW("Only one I frame in queue.");
   }else{
      LOGW("队列中没有一个完整的GOP");
      m_write_ptr = m_read_ptr;
      m_size = 0;
   }
}

void TimeBufferQueue::ClearAllQueue()
{
	vhall_cond_signal(&m_cond_c);
	vhall_cond_signal(&m_cond_p);
   vhall_unlock(&mutex);
}

int TimeBufferQueue::GetQueueSize(){
   return m_size;
}

bool TimeBufferQueue::IsFull(){
   return (m_write_ptr==NULL);
}

bool TimeBufferQueue::IsEmpty(){
   return (m_write_ptr==m_read_ptr&&m_write_ptr->previous==NULL);
}

TimeBufferQueue::~TimeBufferQueue(){
	vhall_lock(&mutex);
   BufferItem * tmp_ptr = m_read_ptr;
   while (tmp_ptr!=NULL) {
      BufferItem * item = tmp_ptr;
      tmp_ptr = tmp_ptr->next;
      if (item) {
         if (item->data) {
            free(item->data);
            item->data = NULL;
         }
         free(item);
         item = NULL;
      }
   }
   vhall_unlock(&mutex);
   
   vhall_cond_destroy(&m_cond_c);
   vhall_cond_destroy(&m_cond_p);
   vhall_lock_destroy(&mutex);
}
