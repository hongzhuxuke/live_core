#include "rate_control.h"
#include "../common/vhall_log.h"
#include "talk/base/thread.h"
#include "live_define.h"
#include "../common/auto_lock.h"


#define  PUSH_BUFFER_FILL_HIGH   0.5
#define  PUSH_BUFFER_FILL_LOW    0.05
#define  BUFFER_DELAY_HIGH       1000
#define  BUFFER_DELAY_LOW        200
#define  CHANGE_THRESHOLD        0.1

RateControl::RateControl()
{
   mRCEncoder = nullptr;
   mBufferState = nullptr;
   mLastBufferSize = 0;
   mLastIFrameNum = 0;
   mFallingCounter = 0;
   mFrozenCounter = 0;
   mThreadRc = new (std::nothrow)talk_base::Thread();
   vhall_lock_init(&mMutex);
}

RateControl::~RateControl()
{
   vhall_lock_destroy(&mMutex);
   mThreadRc->Clear(this);
   VHALL_THREAD_DEL(mThreadRc);
}

void RateControl::SetEncoder(EnocoderRateController* encoder)
{
   if (!encoder) {
      LOGW("[RateControl]input rc encoder is null.");
      return;
   }
   if (mRCEncoder) {
      LOGW("[RateControl]already have a rc encoder, it will be covered by a new one.");
   }
   mRCEncoder = encoder;
}

void RateControl::SetBufferState(BufferState *state){
   VhallAutolock _l(&mMutex);
   if (mBufferState) {
      LOGW("[RateControl]already have a buffer state, it will be covered by a new one.");
   }
   mBufferState = state;
}

void RateControl::CleanBufferState()
{
   VhallAutolock _l(&mMutex);
   if (mIsWorking) {
      LOGE("[RateControl]Clean buffer state when it is working!!!!.");
      Stop();
      mIsWorking = false;
   }
   mBufferState = nullptr;
}

bool RateControl::SetPushState(PushBufferInfo* bufferState)
{
   NetworkState state = NetworkState_Normal;
   float fillingRatio = (float)bufferState->queueSize / (bufferState->maxSize + 1);
   int bufferChange = 0; // negative/zero/positive mean falling/staying/rising
   int bufferDelayMs = bufferState->delayMsInBuffer;

   if (bufferState->maxSize <= 0 || fillingRatio > 1.0 || fillingRatio < 0.0 ) { //not a normal case, we do not change anything
      state = NetworkState_Normal;
      LOGE("[RateControl]Push buffer size is not positive or filling ratio is out of range!");
      return false;
   }

   //bufferChange status decide
   if (bufferState->bytesInBuffer > mLastBufferSize&& mLastBufferSize > bufferState->maxSize* (1 - CHANGE_THRESHOLD)) {
      bufferChange = 2;
   }
   else if (bufferState->bytesInBuffer > mLastBufferSize * CHANGE_THRESHOLD&& bufferState->keyFrameInBuffer <= mLastIFrameNum) {
      bufferChange = 1;
   }
   else if (bufferState->bytesInBuffer < mLastBufferSize * CHANGE_THRESHOLD && bufferState->keyFrameInBuffer >= mLastIFrameNum) {
      bufferChange = -1;
   }
   else {
      bufferChange = 0;
   }
   mLastBufferSize = bufferState->bytesInBuffer;
   mLastIFrameNum = bufferState->keyFrameInBuffer;

   //network state decide
   if (fillingRatio > PUSH_BUFFER_FILL_HIGH) { //too much data in buffer
      if (bufferChange < 0) {
         state = NetworkState_Normal;
      }
      else {
         state = NetworkState_Block;
      }
   }
   else if (fillingRatio > PUSH_BUFFER_FILL_LOW) {  //normal range
      if (bufferDelayMs > BUFFER_DELAY_HIGH) {
         state = NetworkState_Block;
      }
      else if (bufferChange < 0 && bufferDelayMs > BUFFER_DELAY_LOW) {
         state = NetworkState_Overuse;
      }
      else {
         state = NetworkState_Normal;
      }
   }
   else { // only few data in buffer
      if (bufferDelayMs > BUFFER_DELAY_HIGH) {
         state = NetworkState_Block;
      }
      else if (bufferDelayMs < BUFFER_DELAY_LOW) {
         state = NetworkState_Underuse;
      }
      else {
         state = NetworkState_Normal;
      }
   }

   if (!mRCEncoder) {
      LOGE("[RateControl]mRCEncoder is nullprt!");
      return false;
   }
   //avoid a tidal adjustment
   if (state == NetworkState_Block || state == NetworkState_Overuse) {
      mFallingCounter++;
      if (mFallingCounter >= 2) {
         mFrozenCounter = mFrozenCounter > 4? mFrozenCounter : 4;
      }
      else if (mFallingCounter >= 4) {
         mFrozenCounter = 10;
      }
   }
   else if (state == NetworkState_Underuse) {
      if (mFallingCounter > 0) {
         mFallingCounter--;
      }
      
      if (mFrozenCounter > 0) {
         mFrozenCounter--;
         state = NetworkState_Normal;
      }
   }
   else {
      if (mFallingCounter > 0) {
         mFallingCounter--;
      }
      if (mFrozenCounter > 0) {
         mFrozenCounter--;
      }
   }
   if (mFallingCounter > 10) {
      mFallingCounter = 10;
   }

   //adjust bitrate
   if (!mRCEncoder->RateControlAdjust(state)) {
      LOGE("[RateControl]bitrate adjust failed");
      return false;
   }
   return true;
}

bool RateControl::ResetBitrate()
{
   if (mRCEncoder == nullptr || !mRCEncoder->RateControlAdjust(NetworkState_Reset)) {
      LOGE("[RateControl]bitrate adjust failed");
      return false;
   }
   return true;
}

bool RateControl::Start(){
   if (!mThreadRc->started()) {
      mThreadRc->Start();
      mThreadRc->Restart();
   }
   mIsWorking = true;
   mThreadRc->Post(this, MSG_RC);
   return true;
}

void RateControl::Stop(){
   mIsWorking = false;
   if(mThreadRc->started()){
      mThreadRc->Quit();
   }
}

void RateControl::OnMessage(talk_base::Message* msg){
   switch (msg->message_id) {
      case MSG_START:
         break;
      case MSG_RC:
         if (mIsWorking) {
            RateControlLoop();
            mThreadRc->PostDelayed(1000, this, MSG_RC);
         }
         break;
      case MSG_STOP:
         break;
      default:
         break;
   }
   VHALL_DEL(msg->pdata);
}

void RateControl::RateControlLoop(){
   VhallAutolock _l(&mMutex);
   if(mBufferState && mIsWorking){
      int maxSize = mBufferState->GetMaxNum();
      int bytesInBuffer = mBufferState->GetQueueDataSize();
      int delayMsInBuffer = mBufferState->GetQueueDataDuration();
      int keyFrameInBuffer = mBufferState->GetKeyFrameCount();
      int queueSize = mBufferState->GetQueueSize();
      LOGI("maxSize: %d bytesInBuffer: %d delayMsInBuffer: %d keyFrameInBuffer: %d queueDataSize: %d",
           maxSize, bytesInBuffer, delayMsInBuffer, keyFrameInBuffer, queueSize);
      PushBufferInfo bufferInfo(maxSize, bytesInBuffer, delayMsInBuffer, keyFrameInBuffer, queueSize);
      SetPushState(&bufferInfo);
      if(mRCEncoder){
         mRCEncoder->OnPushBufferInfo(bufferInfo);
      }
   }
}

