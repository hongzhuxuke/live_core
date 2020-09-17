#include <stdio.h>
#include <chrono>
#include <thread>
#include "encode_interface.h"
#include "media_encode.h"
#include "media_qa_interface.h"
#include "media_assess.h"
//#include "media_data_send.h"
#if defined WIN32
#pragma comment(lib, "winmm.lib ")
#pragma comment(lib, "Ws2_32.lib")
#endif

int main() {
   int ret;
   int framesize = 0;
   char *pRef = nullptr;
   char *pDif = nullptr;
   FILE *fpInRef = nullptr;
  
   LivePushParam param;
   LiveExtendParam exParam;
   MediaQualityInterface *MQAInst;
   EncodeInterface *EncInst;
   
   param.width = 1920;
   param.height = 1080;
   param.frame_rate = 5;
   param.bit_rate = 1800000;
   param.ch_num = 2;
   param.encode_type = ENCODE_TYPE_SOFTWARE;
   param.src_sample_fmt = VH_AV_SAMPLE_FMT_FLT;
   param.encode_pix_fmt = ENCODE_PIX_FMT_YUV420SP_NV12;
   param.live_model = LIVE_TYPE_VIDEO_ONLY;
   param.is_assess_video_quality = true;
   param.is_adjust_bitrate = true;// true;
   param.is_quality_limited = true;
   param.high_codec_open = 0;
   param.score_frames_period = param.frame_rate * 4;
   param.is_encoder_debug = true;
   param.video_process_filters |= VIDEO_PROCESS_DIFFCHECK;

   //param.fast_assess_mode = 0;
   exParam.scene_type = SceneType_Artificial;
   exParam.same_last = 0;
   exParam.frame_diff_mb = nullptr;
   
   framesize = param.width * param.height * 1.5;

   EncInst = new MediaEncode;
   MQAInst = new MediaQualityAssess;

   if (EncInst == nullptr || MQAInst == nullptr ) {
      printf("Instance create failed!\n");
      return -1;
   }
   if (param.is_assess_video_quality) {
      EncInst->SetMediaQAListener((MQADataInteract *)MQAInst);
   }

   ret = EncInst->LiveSetParam(&param);
   if (ret) {
      printf("Encoder init failed!\n");
      return -1;
   }

   ret = MQAInst->Init(&param);
   if (ret) {
      printf("MAQ init failed!\n");
      return -1;
   }
   printf("init finished!\n");

   if (!EncInst->isInit()) {
      EncInst->Start();
   }

   //fpInRef = fopen("E:/vmaf/python/test/resource/yuv/src01_hrc00_576x324.yuv", "rb");
   fpInRef = fopen("F:/TestVideoSet/NV12/rollingScreenIDE.1080p.yuv", "rb");

   pRef = new char[framesize];
   pDif = new char[framesize];

   if (fpInRef == nullptr) {
      printf("Files open failed!\n");
      return -1;
   }
   
   /*if (MQAInst->StartVideoAssess() < 0) {
      printf("MQA start failed!\n");
      return -1;
   }*/
   MQAInst->StartVideoAssess();
   
   uint64_t ts = 0;
   while (true) {
      if (fread(pRef, 1, framesize, fpInRef) != framesize ) {
         break;
      }
      EncInst->EncodeVideo(pRef, framesize, ts, &exParam);
      //printf("encode frame %d\n", ts);
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
      ts++;
   }

   if (param.is_assess_video_quality) {
      if (!MQAInst->StopVideoAssess()) {
         printf("MQA start failed!\n");
         return -1;
      }
   }
   

   EncInst->Stop();

   if (fpInRef) {
      fclose(fpInRef);
      fpInRef = nullptr;
   }
   return 0;
}