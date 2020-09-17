//
//  Utility.hpp
//  VinnyLive
//
//  Created by liwenlong on 16/7/4.
//  Copyright © 2016年 vhall. All rights reserved.
//

#ifndef Utility_hpp
#define Utility_hpp

#include <stdio.h>
#include <string>
#include <stdint.h>
#include <sstream>
#include "live_open_define.h"

struct NaluUnit;

class Utility {
private:
   Utility(){};
   ~Utility(){};
   static inline void WriteYUV(const int x, const int y, const int width,
                               const int r8, const int g8, const int b8,
                               uint8_t* const py, uint8_t* const puv);
public:
   static void Planar2SemiPlanar(unsigned char* input, unsigned char *output, int width, int height);
   static void Nv212Planar(unsigned char* input, unsigned char *output, int width, int height);
   static void NV212SemiPlanar(unsigned char* input, unsigned char *output, int width, int height);
   static void YV122YuvPlanar(unsigned char* input, unsigned char *output, int width, int height);
   static void YV122SemiPlanar(unsigned char* input, unsigned char *output, int width, int height);
   static void SemiPlanar2Planar(unsigned char* input, unsigned char *output, int width, int height);
   static void SemiPlanarRotate90(unsigned char* input, unsigned char *output, int width, int height, CameraType camera_type);
   //以下的YUV420sp是NV21
   static void ConvertRGB565ToYUV420SP(unsigned char* input, unsigned char* output,
                                       const int width, const int height);
   static void ConvertARGB8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                         int width, int height);
   static void ConvertRGBA8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                         int width, int height);
   static void ConvertABGR8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                         int width, int height);
   static void ConvertABGRPlaneToData(unsigned char* input, unsigned char* output,
                                         int width, int height, int pixel_stride, int row_padding);
   static uint64_t GetTimestampMs();
   static int GetNumFromSamplingRate(int sampleRate);
   static int GetBitNumWithSampleFormat(int avSampleFormat);
   static int GetNalu(int type, unsigned char *data, int size, NaluUnit*outNalu);
   //used for debug
   static int PrintMem(unsigned char *data, int size, int line_count = 16);
   
   template<typename T>
   static std::string ToString(T arg){
      std::stringstream ss;
      ss << arg;
      return ss.str();
   }
   
   static void ToLower(std::string& s);

   static void ToUpper(std::string& s);
   
   /**
    Token 转换算法

    @param s 要转换的字符串
    @return 转换完的字符串
    */
   static std::string TokenTransition(const std::string& s);
};

#endif /* Utility_hpp */
