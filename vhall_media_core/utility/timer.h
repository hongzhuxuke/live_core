//
//  timer.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/8/4.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef timer_h
#define timer_h

#include <functional>
#include <stdio.h>

//this class is a timer

typedef std::function<void()> SelectorDelegate;

class Timer{
public:
   Timer(int time_interval); //ms
   ~Timer();
   void SetTimeInterval(int time_interval);//ms
   void SetSelectorMethod(const SelectorDelegate &selector);
   void Start();
   void Stop();
private:
   class TimerThread;
   TimerThread *mTimerThread;
};

#endif /* timer_hpp */
