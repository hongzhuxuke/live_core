#ifndef __RATECONTROL_H__
#define __RATECONTROL_H__
//#include <limits.h>
#include "rate_control_interface.h"
//#include "../encoder/encoder_info.h"
#include "talk/base/messagehandler.h"
//#include "../common/live_define.h"
//#include <list>
//#include <vector>
#include "../common/live_sys.h"

namespace talk_base {
   class Thread;
}

class RateControl : public talk_base::MessageHandler{
public:
   RateControl();
   ~RateControl();
   void SetEncoder(EnocoderRateController* encoder);
   void SetBufferState(BufferState *state);
   void CleanBufferState();
   bool ResetBitrate();
   bool Start();
   void Stop();
private:
   bool SetPushState(PushBufferInfo* bufferState);
   virtual void OnMessage(talk_base::Message* msg);
   void RateControlLoop();
private:
   enum MSG{
      MSG_START = 0,
      MSG_STOP,
      MSG_RC
   };
   bool mIsWorking;
   int mLastBufferSize;
   int mLastIFrameNum;
   vhall_lock_t mMutex;
   uint16_t mFallingCounter;
   uint16_t mFrozenCounter;
   EnocoderRateController* mRCEncoder;
   talk_base::Thread   *mThreadRc;
   BufferState *mBufferState;
};

#endif
