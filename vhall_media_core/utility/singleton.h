//
//  Singleton.hpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/28.
//  Copyright © 2017年 vhall. All rights reserved.
//

#ifndef Singleton_h
#define Singleton_h

#include <stdio.h>
#include <mutex>
#include <memory>

template <class T>
class Singleton {
   
public:
   static T* instance(){
      std::call_once(mOnce, [&](){
         mInstance.reset(new T());
      });
      return mInstance.get();
   }
   
   Singleton(const Singleton&) = delete;//禁用copy方法
   const Singleton& operator=( const Singleton&) = delete;//禁用赋值方法

private:
   Singleton(){};
   ~Singleton(){};
   
private:
   static std::once_flag mOnce;
   static std::unique_ptr<T> mInstance;
};


//Instructions for use
//Foo * foo = Singleton<Foo>::Instance();

#endif /* Singleton_hpp */
