//
//  TimeBufferQueue.hpp
//  VinnyLive
//
//  Created by liwenlong on 16/1/28.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef TimeBufferQueue_hpp
#define TimeBufferQueue_hpp

#include <string>
#include <stdlib.h>
#include <live_sys.h>
#include <stdint.h>

typedef struct BufferItem
{
   int          index;
   char         *data;
   int          size;
   int          buffer_size;
   uint32_t     ts;
   int          type; //数据类型
   BufferItem   *next; //下一个
   BufferItem   *previous; //前一个
   int          key_index;
public:
   
   void ClearItem()
   {
      ClearData();
      next = NULL;
      previous = NULL;
   }
   
   void ClearData()
   {
      size = 0;
      ts = 0;
      type = 0;
      key_index = 0;
      if(data){
        // memset(data, 0, buffer_size);
      }
   }
   
//   BufferItem& operator= (const BufferItem item){
//      
//      if(this==&item) return *this;
//      
//      data = item.data;
//      size = item.size;
//      buffer_size = item.buffer_size;
//      ts = item.ts;
//      type = item.type;
//      next = item.next;
//      
//      return *this;
//   }
   
} BufferItem;


enum BufferState{
   BUFFER_STATE_NONE = 0,
   BUFFER_STATE_FULL,
   BUFFER_STATE_EMPTY
};

class BufferCallback {
   
public:
   BufferCallback(){};
   virtual ~BufferCallback(){};
   virtual void OnBufferStateChange(BufferState state) = 0;
};

class TimeBufferQueue {
   
public:
   TimeBufferQueue(int max_num = 0);
   TimeBufferQueue(const BufferCallback & callback ,int max_num = 0);
   ~TimeBufferQueue();
   void SetTag(std::string tag);
   bool WriteQueue(const char* data,const int size,const int type,const uint32_t ts,const bool block = true);
   bool ReadQueue(BufferItem &rItem);
   int GetQueueSize();
   bool IsFull();
   bool IsEmpty();
   void ClearAllQueue();
private:
   
   void OnInit(int max_num);
   void DiscardGop();
   volatile int m_max_num;
   volatile int m_size;
   volatile int m_count;
   std::string  m_tag;
   
   vhall_lock_t mutex;
   vhall_cond_t  m_cond_c;
   vhall_cond_t  m_cond_p;
   
   BufferItem * m_read_ptr;
   BufferItem * m_write_ptr;
   BufferItem * m_end_ptr;
   
   BufferCallback * m_callback;
   BufferState      m_buffer_state;
   int              m_key_index;
};

#endif /* TimeBufferQueue_hpp */
