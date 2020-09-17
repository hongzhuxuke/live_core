//
//  timer.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/8/4.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "timer.h"
#include "../common/live_define.h"
#include "talk/base/messagehandler.h"
#include "talk/base/thread.h"

class Timer::TimerThread:public talk_base::MessageHandler {
   
public:
   TimerThread(int time_interval):
   mWorkThread(nullptr),
   mTimeInterval(time_interval),
   mIsRuning(false){
      mWorkThread = new(std::nothrow) talk_base::Thread();
   }
   ~TimerThread(){
      VHALL_THREAD_DEL(mWorkThread);
   }
   void SetTimeInterval(int time_interval){
      mTimeInterval = time_interval;
   }
   void SetSelectorMethod(const SelectorDelegate &selector){
      mSelector = selector;
   }
   void Start(){
      mIsRuning = true;
      if (!mWorkThread->started()) {
         mWorkThread->Start();
      }
      mWorkThread->Restart();
      mWorkThread->Post(this,MSG_RUN);
   }
   void Stop(){
      mIsRuning = false;
      mWorkThread->Clear(this);
      mWorkThread->Stop();
   }
private:
   enum {
      MSG_RUN
   };
   void OnMessage(talk_base::Message* msg){
      switch (msg->message_id) {
         case MSG_RUN:{
            if (mSelector) {
               mSelector();
            }
            if (mIsRuning) {
               mWorkThread->PostDelayed(mTimeInterval,this,MSG_RUN);
            }
         }
            break;
         default:
            break;
      }
      VHALL_DEL(msg->pdata);
   }
   int  mTimeInterval;
   bool mIsRuning;
   SelectorDelegate mSelector;
   talk_base::Thread *mWorkThread;
};

Timer::Timer(int time_interval):
mTimerThread(NULL){
   mTimerThread = new(std::nothrow) TimerThread(time_interval);
}

Timer::~Timer(){
   VHALL_DEL(mTimerThread);
}

void Timer::Start(){
   if (mTimerThread) {
      mTimerThread->Start();
   }
}

void Timer::Stop(){
   if (mTimerThread) {
      mTimerThread->Stop();
   }
}

void Timer::SetTimeInterval(int time_interval){
   if (mTimerThread) {
      mTimerThread->SetTimeInterval(time_interval);
   }
}

void Timer::SetSelectorMethod(const SelectorDelegate& selector){
   if (mTimerThread) {
      mTimerThread->SetSelectorMethod(selector);
   }
}

