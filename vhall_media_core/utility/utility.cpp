//
//  Utility.cpp
//  VinnyLive
//
//  Created by liwenlong on 16/7/4.
//  Copyright © 2016年 vhall. All rights reserved.
//

#include "utility.h"
#include <string.h>
#include <stdlib.h>
#include "live_define.h"
#include <live_sys.h>
#include <stdint.h>
#include <strstream>
#include <algorithm>
#include "../common/vhall_log.h"
#include "talk/base/stringencode.h"
#include "crc32.h"

/*
 * ImageFormat.YV12 yyyyyyyy vvvv uuuu
 * ImageFormat.NV21 yyyyyyyy vuvu vuvu
 *
 * COLOR_FormatYUV420Planar yyyyyyyy uuuu vvvv
 * COLOR_FormatYUV420SemiPlanar yyyyyyyy uvuv uvuv
 */

#ifdef WIN32

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static uint64_t beginTime = 0;

static inline uint64_t get_clockfreq(void)
{
   if (!have_clockfreq)
      QueryPerformanceFrequency(&clock_freq);
   return clock_freq.QuadPart;
}

static uint64_t os_gettime_ns(void)
{
   LARGE_INTEGER current_time;
   double time_val;
   
   QueryPerformanceCounter(&current_time);
   time_val = (double)current_time.QuadPart;
   time_val *= 1000000000.0;
   time_val /= (double)get_clockfreq();
   
   return (uint64_t)time_val;
}
#endif

static char* yuv420p = NULL;
int yuvPlanar = 0;

void Utility::Planar2SemiPlanar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   memcpy(output, input, planar);
   for (int i = 0; i < planar / 4; i++) {
      // copy u
      output[planar + i * 2] = input[planar + i];
      // swap V
      output[planar + i * 2 + 1] = input[planar + planar / 4 + i];
   }
}

void Utility::Nv212Planar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   memcpy(output, input, planar);
   for (int i = 0; i < planar / 4; i++) {
      // swap U
      output[planar + i] = input[planar + i * 2 + 1];
      // copy V
      output[planar + planar / 4 + i] = input[planar + i * 2];
   }
}

void Utility::NV212SemiPlanar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   if (yuvPlanar != planar || yuv420p == 0) {
      if (yuv420p)
         free(yuv420p);
      yuv420p = (char*) malloc(planar / 2 * 3);
      yuvPlanar = planar;
   }
   if (yuv420p != 0) {
      Nv212Planar(input, (unsigned char*)yuv420p, width, height);
   }
   Planar2SemiPlanar((unsigned char*)yuv420p, output, width, height);
}

void Utility::YV122YuvPlanar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   memcpy(output, input, planar);
   memcpy(output + planar, input + planar + planar / 4, planar / 4);
   memcpy(output + planar + planar / 4, input + planar, planar / 4);
}

void Utility::YV122SemiPlanar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   if (yuvPlanar != planar || yuv420p == 0) {
      if (yuv420p)
         free(yuv420p);
      yuv420p = (char*) malloc(planar / 2 * 3);
      yuvPlanar = planar;
   }
   if (yuv420p != 0) {
      memcpy(yuv420p, input, planar);
      memcpy(yuv420p + planar, input + planar + planar / 4, planar / 4);
      memcpy(yuv420p + planar + planar / 4, input + planar, planar / 4);
   }
   Planar2SemiPlanar((unsigned char*)yuv420p, output, width, height);
}

void Utility::SemiPlanar2Planar(unsigned char* input, unsigned char *output, int width, int height) {
   int planar = width * height;
   memcpy(output, input, planar);
   for (int i = 0; i < planar / 4; i++) {
      // swap U
      output[planar + planar / 4  + i] = input[planar + i * 2 + 1];
      // copy V
      output[planar + i] = input[planar + i * 2];
   }
}

void Utility::SemiPlanarRotate90(unsigned char* input, unsigned char *output, int width, int height, CameraType camera_type)
{
   int i, j;
   int framesize = width * height;
   if (camera_type == CAMERA_BACK) { // CAMERA_BACK==1
      for (i = 0; i < width; i++) {
         for (j = 0; j < height; j++) {
            *output++ = input[(height - 1 - j) * width + i];
         }
      }
      for (i = 0; i < width; i+=2) {
         for (j = 0; j < (height >> 1); j++) {
            *output++ = input[framesize + (height / 2 - 1 - j) * width + i];
            *output++ = input[framesize + (height / 2 - 1 - j) * width + i + 1];
         }
      }
   } else if(camera_type == CAMERA_FRONT) {// CAMERA_FRONT==0
      for (i = 0; i < width; i++) {
         for (j = 0; j < height; j++) {
            *output++ = input[(j+1) * width - i-1];
         }
      }
      
      for (i = 0; i < width; i+=2) {
         for (j = 0; j < (height >> 1); j++) {
            *output++ = input[framesize + (j+1) * width - i-2];
            *output++ = input[framesize + (j+1) * width - i-1];
         }
      }
   }
}

void Utility::ConvertRGBA8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                        int width, int height){
   uint8_t* pY = (uint8_t*)output;
   uint8_t* pUV = (uint8_t*)(output + (width * height));
   const uint32_t* in = (const uint32_t*)input;
   
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         const uint32_t rgb = *in++;
         const int nR = (rgb >> 24) & 0xFF;
         const int nG = (rgb >> 16) & 0xFF;
         const int nB = (rgb >> 8) & 0xFF;
         Utility::WriteYUV(x, y, width, nR, nG, nB, pY++, pUV);
      }
   }
}

void Utility::ConvertARGB8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                        int width, int height) {
   uint8_t* pY = (uint8_t*)output;
   uint8_t* pUV = (uint8_t*)(output + (width * height));
   const uint32_t* in = (const uint32_t*)input;
   
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         const uint32_t rgb = *in++;
         const int nR = (rgb >> 16) & 0xFF;
         const int nG = (rgb >> 8) & 0xFF;
         const int nB = rgb & 0xFF;
         Utility::WriteYUV(x, y, width, nR, nG, nB, pY++, pUV);
      }
   }
}

void Utility::ConvertABGR8888ToYUV420SP(unsigned char* input, unsigned char* output,
                                        int width, int height) {
   uint8_t* pY = (uint8_t*)output;
   uint8_t* pUV = (uint8_t*)(output + (width * height));
   const uint32_t* in = (const uint32_t*)input;
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         const uint32_t rgb = *in++;
         const int nB = (rgb >> 16) & 0xFF;
         const int nG = (rgb >>  8) & 0xFF;
         const int nR = rgb & 0xFF;
         Utility::WriteYUV(x, y, width, nR, nG, nB, pY++, pUV);
      }
   }
}

void Utility::ConvertRGB565ToYUV420SP(unsigned char* input, unsigned char* output,
                                      const int width, const int height) {
   uint8_t* pY = (uint8_t*)output;
   uint8_t* pUV = (uint8_t*)(output + (width * height));
   const uint16_t* in = (uint16_t*)input;
   
   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
         const uint32_t rgb = *in++;
         
         const int r5 = ((rgb >> 11) & 0x1F);
         const int g6 = ((rgb >> 5) & 0x3F);
         const int b5 = (rgb & 0x1F);
         
         // Shift left, then fill in the empty low bits with a copy of the high
         // bits so we can stretch across the entire 0 - 255 range.
         const int r8 = r5 << 3 | r5 >> 2;
         const int g8 = g6 << 2 | g6 >> 4;
         const int b8 = b5 << 3 | b5 >> 2;
         
         Utility::WriteYUV(x, y, width, r8, g8, b8, pY++, pUV);
      }
   }
}

void Utility::ConvertABGRPlaneToData(unsigned char* input, unsigned char* output,
                                     int width, int height, int pixel_stride, int row_padding){
   char*in = (char*)input;
   char*out = (char*)output;
   for (int i = 0; i < height; ++i) {
      memcpy(out,in,width*pixel_stride);
      out += width*pixel_stride;
      in += width*pixel_stride+row_padding;
   }
}

inline void Utility::WriteYUV(const int x, const int y, const int width,
                              const int r8, const int g8, const int b8,
                              uint8_t* const py, uint8_t* const puv) {
   // Using formulas from http://msdn.microsoft.com/en-us/library/ms893078
   *py = ((66 * r8 + 129 * g8 + 25 * b8 + 128) >> 8) + 16;
   
   // Odd widths get rounded up so that UV blocks on the side don't get cut off.
   const int blocks_per_row = (width + 1) / 2;
   
   // 2 bytes per UV block
   const int offset = 2 * (((y / 2) * blocks_per_row + (x / 2)));
   
   // U and V are the average values of all 4 pixels in the block.
   if (!(x & 1) && !(y & 1)) {
      // Explicitly clear the block if this is the first pixel in it.
      puv[offset] = 0;
      puv[offset + 1] = 0;
   }
   
   const int u_offset = 1;
   const int v_offset = 0;
   
   // V (with divide by 4 factored in)
   puv[offset + v_offset] += ((112 * r8 - 94 * g8 - 18 * b8 + 128) >> 10) + 32;
   
   // U (with divide by 4 factored in)
   puv[offset + u_offset] += ((-38 * r8 - 74 * g8 + 112 * b8 + 128) >> 10) + 32;
}

uint64_t Utility::GetTimestampMs() {
#ifdef WIN32
   return os_gettime_ns() / 1000000;
#else
   struct timeval tv;
   gettimeofday(&tv, NULL);
   uint64_t curr = (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec/1000);
   return curr;
#endif
}

int Utility::GetNumFromSamplingRate(int sampleRate){
   switch (sampleRate) {
      case 96000:
         return 0;
         
      case 88200:
         return 1;
         
      case 64000:
         return 2;
         
      case 48000:
         return 3;
         
      case 44100:
         return 4;
         
      case 32000:
         return 5;
         
      case 24000:
         return 6;
         
      case 22050:
         return 7;
         
      case 16000:
         return 8;
         
      case 12000:
         return 9;
         
      case 11025:
         return 10;
         
      case 8000:
         return 11;
         
      case 7350:
         return 12;
         
      default:
         break;
   }
   return 8;
}

int Utility::GetBitNumWithSampleFormat(int avSampleFormat){
   if (avSampleFormat==VH_AV_SAMPLE_FMT_U8||avSampleFormat==VH_AV_SAMPLE_FMT_U8P) {
      return 8;
   }else if(avSampleFormat==VH_AV_SAMPLE_FMT_S16||avSampleFormat==VH_AV_SAMPLE_FMT_S16P){
      return 16;
   }else if(avSampleFormat==VH_AV_SAMPLE_FMT_S32||avSampleFormat==VH_AV_SAMPLE_FMT_S32P||avSampleFormat==VH_AV_SAMPLE_FMT_FLT||avSampleFormat==VH_AV_SAMPLE_FMT_FLTP){
      return 32;
   }else if(avSampleFormat==VH_AV_SAMPLE_FMT_DBL||avSampleFormat==VH_AV_SAMPLE_FMT_DBLP){
      return 64;
   }
   return 0;
}

int Utility::GetNalu(int type, unsigned char *data, int size, NaluUnit*outNalu)
{
   int i = 0;
   unsigned char *pos1, *pos2;
   pos1 = pos2 = NULL;
   while (i + 3 < size){
      if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 && (data[i + 3] & 0x1f) == type){
         pos1 = &data[i + 3];
         i += 4;
         break;
      }
      else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1 && i + 4<size && (data[i + 4] & 0x1f) == type){
         //find the nalu type we need
         pos1 = &data[i + 4];
         i += 5;
         break;
      }
      i++;
   }
   
   if (pos1){
      while (i + 2 < size){
         if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1){
            pos2 = &data[i];
            break;
         }
         else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && i + 3< size && data[i + 3] == 1) {
            pos2 = &data[i];
            break;
         }
         i++;
      }
      
      if (pos2){
         outNalu->type = type;
         outNalu->size = (int)(pos2 - pos1);
         outNalu->data = pos1;
      }
      else {
         outNalu->type = type;
         outNalu->size = (int)((data + size) - pos1);
         outNalu->data = pos1;
      }
      return 0;
   }
   else{
      return -1;
   }
}

void Utility::ToLower(std::string& s)
{
   for (auto& c : s) {
      c = char(tolower(c));
   }
}

void Utility::ToUpper(std::string& s){
   for (auto& c : s) {
      c = char(toupper(c));
   }
}

std::string Utility::TokenTransition(const std::string& s){
   std::vector<std::string> fields;
   std::string result;
   size_t num = talk_base::split(s, '_', &fields);
   for (int i = 0; i < num; i++) {
      if (i==0) {
         result = fields[i];
         reverse(result.begin(),result.end());
         result = HexCRC32(result);
      }else{
         result += "_";
         result += fields[i];
      }
   }
   return result;
}

int Utility::PrintMem(unsigned char *data, int size, int line_count){
   int temp = 0;
   int count = 0;
   printf("PrintMem: %x \n",(int8_t)*data);
   while (temp < size){
      printf("%02x ", data[temp]);
      temp++;
      count++;
      if (count % line_count == 0){
         printf("\n");
      }
   }
   printf("\n");
   return 0;
}
