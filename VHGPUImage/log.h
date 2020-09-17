#pragma once
#include <iostream>
#include <string>
#include <stdarg.h>
//#include <common/vhall_log.h>

static inline void Print(std::string header, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  { // stdout
    fprintf(stdout, "%s:", header.c_str());
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout); 
  }
  va_end(ap);

  return;
}

/* 如果未定义log默认标准输出 */
#ifndef LOGI
#define LOGI(msg, ...)   { Print(std::string("stdout:") + __func__ + ":" + std::to_string(__LINE__), msg, ##__VA_ARGS__);}
#endif
#ifndef LOGD
#define LOGD(msg, ...)   { Print(std::string("stdout:") + __func__ + ":" + std::to_string(__LINE__), msg, ##__VA_ARGS__);}
#endif
#ifndef LOGW
#define LOGW(msg, ...)   { Print(std::string("stdout:") + __func__ + ":" + std::to_string(__LINE__), msg, ##__VA_ARGS__);}
#endif
#ifndef LOGE
#define LOGE(msg, ...)   { Print(std::string("stdout:") + __func__ + ":" + std::to_string(__LINE__), msg, ##__VA_ARGS__);}
#endif