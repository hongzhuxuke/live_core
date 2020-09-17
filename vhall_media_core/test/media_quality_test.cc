#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif // _WIN32
#include "media_qa_interface.h"
#include "media_assess.h"

int main() {
   int framesize = 0;
   char *pRef = nullptr;
   char *pDif = nullptr;
   FILE *fpInRef = nullptr;
   FILE *fpInDif = nullptr;
   //FILE *fpOutRef = nullptr;
   //FILE *fpOutDif = nullptr;
   LivePushParam param;
   MediaQualityInterface *inst;

   param.width = 1280;
   param.height = 720;
   param.frame_rate = 1;
   param.score_frames_period = 50;
   param.is_assess_video_quality = true;
   framesize = param.width * param.height * 1.5;

   pRef = new char[framesize];
   pDif = new char[framesize];

   //fpInRef = fopen("E:/vmaf/python/test/resource/yuv/src01_hrc00_576x324.yuv", "rb");
   //fpInDif = fopen("E:/vmaf/python/test/resource/yuv/src01_hrc01_576x324.yuv", "rb");
   //fpOutRef = fopen("E:/vmaf/python/test/resource/yuv/src01_hrc00_576x324_cp.yuv", "wb");
   //fpOutDif = fopen("E:/vmaf/python/test/resource/yuv/src01_hrc01_576x324_cp.yuv", "wb");
   fpInRef = fopen("E:/TestVideoSet/KristenAndSara_1280x720_60.yuv", "rb");

   if (fpInRef == nullptr /*|| fpInDif == nullptr || fpOutRef == nullptr || fpOutDif == nullptr*/) {
      printf("Files open failed!\n");
      return -1;
   }

   inst = new MediaQualityAssess;
  
   int ret = inst->Init(&param);
   if (ret) {
      printf("MQA init failed!\n");
      return -2;
   }
   else {
      printf("MQA init finished!\n");
   }
   if (!inst->StartVideoAssess()) {
      printf("MQA start failed!\n");
      return -3;
   }

   while (true) {
      if (fread(pRef, 1, framesize, fpInRef) != framesize /*|| fread(pDif, 1, framesize, fpInDif) != framesize*/) {
         inst->InputVideoData(nullptr, nullptr);
         break;
      }
      inst->InputVideoData(pRef, pRef);
#ifdef _WIN32
      Sleep(40);
#else
      usleep(40000);
#include <unistd.h>
#endif // _WIN32
   }

   if (!inst->StopVideoAssess()) {
      printf("MQA start failed!\n");
      return -4;
   }
   if (fpInDif) {
      fclose(fpInDif);
      fpInDif = nullptr;
   }
   if (fpInRef) {
      fclose(fpInRef);
      fpInRef = nullptr;
   }
   /*if (fpOutDif) {
      fclose(fpOutDif);
      fpOutDif = nullptr;
   }
   if (fpOutRef) {
      fclose(fpOutRef);
      fpOutRef = nullptr;
   }*/

   
   return 0;
}