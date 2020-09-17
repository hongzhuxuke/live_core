#ifndef __LIVE_MESSAGE_H__
#define __LIVE_MESSAGE_H__

#include <string>
#include "live_obs.h"
#include "live_define.h"
#include "talk/base/thread.h"
#include "safe_buffer_data.h"

class SafeData;

class StringMessageData : public talk_base::MessageData {
public:
   StringMessageData(const std::string& str)
   : str_(str) {
   }
   ~StringMessageData() {
   }
   const std::string str_;
};

class IntMessageData : public talk_base::MessageData {
public:
   IntMessageData(const int value)
   : mValue(value) {}
   ~IntMessageData() {}
   int mValue;
};

class CodeMessageData : public talk_base::MessageData {
public:
   CodeMessageData(int code, const std::string &content)
   : code_(code),
   content_(content) {
   }
   ~CodeMessageData() {
   }
   int code_;
   std::string content_;
};

class EventMessageData : public talk_base::MessageData {
public:
   EventMessageData(int type, const EventParam &param)
   : type_(type),
   param_(param) {
   }
   ~EventMessageData() {
   }
   int type_;
   EventParam param_;
};

class DataMessageData : public talk_base::MessageData {
public:
   DataMessageData(const int8_t * data, int size)
   : size_(size){
      data_ = (int8_t *)malloc(size);
      memcpy(data_, data, size);
   }
   ~DataMessageData() {
      VHALL_DEL(data_);
   }
   int8_t * data_;
   int size_;
};

class HttpDataMessageData : public talk_base::MessageData {
public:
   HttpDataMessageData(const std::string &url,int key):mUrl(url),key(key){
   }
   ~HttpDataMessageData() {}
   int  key;
   std::string mUrl;
};

class SafeDataMessageData : public talk_base::MessageData {
public:
   SafeDataMessageData(SafeData * safe_data)
   :mSafeData(safe_data),mExtendParam(NULL){}
   ~SafeDataMessageData(){
      VHALL_DEL(mExtendParam);
      if (mSafeData) {
         mSafeData->SelfRelease();
      }
   }
   void SetExtendParam(LiveExtendParam* extendParam){
      if (extendParam) {
         mExtendParam = new LiveExtendParam();
         *mExtendParam = *extendParam;
      }else{
         VHALL_DEL(mExtendParam);
      }
   }
   LiveExtendParam * mExtendParam;
   SafeData *mSafeData;
};

class AudioParamMessageData : public talk_base::MessageData {
public:
   AudioParamMessageData(AudioParam*audioParam) {
      mParam = *audioParam;
   }
   ~AudioParamMessageData() {
      
   }
   AudioParam mParam;
};

class VideoParamMessageData : public talk_base::MessageData {
public:
   VideoParamMessageData(VideoParam* videoParam) {
      mParam = *videoParam;
   }
   ~VideoParamMessageData() {
      
   }
   VideoParam mParam;
};

#endif
