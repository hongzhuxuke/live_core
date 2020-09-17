//
//  Singleton.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/9/28.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "singleton.h"


template <class T>
std::once_flag Singleton<T>::mOnce;

template <class T>
std::unique_ptr<T> Singleton<T>::mInstance;
