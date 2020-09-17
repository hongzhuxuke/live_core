//
//  noise_suppression_packing.cpp
//  VhallLiveApi
//
//  Created by ilong on 2017/6/23.
//  Copyright © 2017年 vhall. All rights reserved.
//

#include "noise_suppression_packing.h"
#include "../common/live_define.h"
#include "noise_suppression.h"
#include "signal_processing_library.h"
#include "../common/vhall_log.h"
#include "../utility/utility.h"

#define FILTER_STATE_COUNT    6

NoiseSuppressionPacking::NoiseSuppressionPacking():
mNSHandle(NULL),
mInBufferH(NULL),
mOutBufferL(NULL),
mInBufferL(NULL),
mOutBufferH(NULL),
mFilterState1(NULL),
mFilterState2(NULL),
mFilterState3(NULL),
mFilterState4(NULL){
   mSampleFormat = VH_AV_SAMPLE_FMT_S16;
}

NoiseSuppressionPacking::~NoiseSuppressionPacking(){
   VHALL_DEL(mInBufferH);
   VHALL_DEL(mInBufferL);
   VHALL_DEL(mOutBufferH);
   VHALL_DEL(mOutBufferL);
   VHALL_DEL(mFilterState1);
   VHALL_DEL(mFilterState2);
   VHALL_DEL(mFilterState3);
   VHALL_DEL(mFilterState4);
   if (mNSHandle) {
      WebRtcNs_Free((NsHandle*)mNSHandle);
      mNSHandle = NULL;
   }
}

int NoiseSuppressionPacking::Init(int sample_rate, int level,VHAVSampleFormat sample_format){
   mSampleFormat = sample_format;
   NsHandle* handle = NULL;
   int ret = -1;
   if (mNSHandle==NULL) {
      ret = WebRtcNs_Create(&handle);
      if (ret<0) {
         LOGE("WebRtcNs_Create Error!");
         return -1;
      }
      mNSHandle = handle;
   }
   ret = WebRtcNs_Init(handle, sample_rate);
   if (ret<0) {
      LOGE("WebRtcNs_Init Error!");
      return -2;
   }
   ret = WebRtcNs_set_policy(handle, level);
   if (ret<0) {
      LOGE("WebRtcNs_set_policy Error!");
      return -3;
   }
   int size = NS_PROCESS_SIZE/2*Utility::GetBitNumWithSampleFormat(mSampleFormat)/8/2;
   if (mInBufferL == NULL) {
      mInBufferL = (int8_t*)calloc(1,size);
   }
   if (mInBufferH == NULL) {
      mInBufferH = (int8_t*)calloc(1,size);
   }
   if (mOutBufferL == NULL) {
      mOutBufferL = (int8_t*)calloc(1,size);
   }
   if (mOutBufferH == NULL) {
      mOutBufferH = (int8_t*)calloc(1,size);
   }
   int byteCount = FILTER_STATE_COUNT*2*Utility::GetBitNumWithSampleFormat(mSampleFormat)/8;
   if (mFilterState1==NULL) {
      mFilterState1 = (int8_t*)calloc(1, byteCount);
   }
   if (mFilterState2==NULL) {
      mFilterState2 = (int8_t*)calloc(1, byteCount);
   }
   if (mFilterState3==NULL) {
      mFilterState3 = (int8_t*)calloc(1, byteCount);
   }
   if (mFilterState4==NULL) {
      mFilterState4 = (int8_t*)calloc(1, byteCount);
   }
   return 0;
}

int NoiseSuppressionPacking::ResetFilterState(){
   int byteCount = FILTER_STATE_COUNT*2*Utility::GetBitNumWithSampleFormat(mSampleFormat)/8;
   memset(mFilterState1, 0, byteCount);
   memset(mFilterState2, 0, byteCount);
   memset(mFilterState3, 0, byteCount);
   memset(mFilterState4, 0, byteCount);
   return 0;
}

int NoiseSuppressionPacking::NoiseSuppressionProcess(const int8_t *input_data, const int size, const int8_t *output_data){
   NsHandle * nsHandle = static_cast<NsHandle *>(mNSHandle);
   if (mSampleFormat == VH_AV_SAMPLE_FMT_S16){
      //splitting the signal
      WebRtcSpl_AnalysisQMF((short*)input_data, (short*)mInBufferL, (short*)mInBufferH,(int32_t*)mFilterState1, (int32_t*)mFilterState2);
      // noise suppress
      int ret = WebRtcNs_Process(nsHandle, (short*)mInBufferL, (short*)mInBufferH, (short*)mOutBufferL, (short*)mOutBufferH);
      if (ret == 0){
         //unite the signal
         WebRtcSpl_SynthesisQMF((short*)mOutBufferL, (short*)mOutBufferH, (short*)output_data, (int32_t*)mFilterState3, (int32_t*)mFilterState4);
         return ret;
      }
   }else if(mSampleFormat == VH_AV_SAMPLE_FMT_FLT){
      VhallSpl_AnalysisQMF_32f((float*)input_data, (float*)mInBufferL, (float*)mInBufferH,(double*)mFilterState1, (double*)mFilterState2);
      // noise suppress
      int ret = VhallNs_Process_32f(nsHandle, (float*)mInBufferL, (float*)mInBufferH, (float*)mOutBufferL, (float*)mOutBufferH);
      if (ret == 0){
         //unite the signal
         VhallSpl_SynthesisQMF_32f((float*)mOutBufferL, (float*)mOutBufferH, (float*)output_data, (double*)mFilterState3, (double*)mFilterState4);
         return ret;
      }
   }
   return -1;
}
