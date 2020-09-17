#ifndef __RATECONTROLINTERFACE_H__
#define __RATECONTROLINTERFACE_H__

#include <stdlib.h>
#include <string>
#include <sstream>

enum NetworkState {
   NetworkState_Normal = 0,
   NetworkState_Underuse,
   NetworkState_Overuse,
   NetworkState_Block,
   NetworkState_Reset
};

class PushBufferInfo {
public:
   int maxSize;            // Buffer size(item)
   int bytesInBuffer;      // data size(byte) in buffer
   int delayMsInBuffer;      // buffering data duration in ms
   int keyFrameInBuffer;   // I-frame number in buffer
   int queueSize;      // curent buffer size(item)
   PushBufferInfo(int maxSize,
                  int bytesInBuffer,
                  int delayMsInBuffer,
                  int keyFrameInBuffer,
                  int queueSize) {
      this->maxSize = maxSize;
      this->bytesInBuffer = bytesInBuffer;
      this->delayMsInBuffer = delayMsInBuffer;
      this->keyFrameInBuffer = keyFrameInBuffer;
      this->queueSize = queueSize;
   }
   std::string print(){
      std::stringstream ss;
      ss << " maxSize:" << this->maxSize;
      ss << " bytesInBuffer:" << this->bytesInBuffer;
      ss << " delayMsInBuffer:" << this->delayMsInBuffer;
      ss << " keyFrameInBuffer:" << this->keyFrameInBuffer;
      ss << " queueSize:" << this->queueSize;
      return ss.str();
   }
};

class EnocoderRateController
{
public:
   EnocoderRateController() {};
   virtual ~EnocoderRateController() {};

   virtual bool RateControlAdjust(NetworkState state) = 0;
   
   // for test
   virtual void OnPushBufferInfo(PushBufferInfo info) {};
};

class BufferState {
   
public:
   BufferState(){};
   virtual ~BufferState(){};
   
   virtual int GetQueueSize() = 0;
   virtual int GetMaxNum() = 0;
   virtual uint32_t GetQueueDataDuration() = 0;
   virtual int GetQueueDataSize() = 0;
   virtual int GetKeyFrameCount() = 0;
};


#endif
