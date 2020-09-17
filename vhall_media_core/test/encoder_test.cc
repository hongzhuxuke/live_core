/*
*本代码用于测试x264编码器效果
*当前版本下需要将libx264.dll和libgcc_s_dw2-1.dll拷贝到生成的encoder_test.exe目录下（默认$USER_BUILD\bin\Debug\）
*仅支持读入YUV数据进行编码的测试，测试前请填写正确的图像宽高、预计码率(bps)以及输入输出文件的路径
*/

#include "x264_encoder.h"
//#include "media_preprocess.h"
#include <stdio.h>
#include <string.h>
#include <Live_sys.h>
#include <vhall_log.h>

//#define PREPROCESS_RECORD
#define FRAMEWIDTH 1280
#define FRAMEHEIGHT 720
#define FRAMEBITRATE 800000
int main(){
   LivePushParam param;
   memset(&param, 0, sizeof(&param));
   param.frame_rate = 25;
   param.width = FRAMEWIDTH;
   param.height = FRAMEHEIGHT;
   param.bit_rate = FRAMEBITRATE;
   param.gop_interval = 4;
   param.encode_pix_fmt = ENCODE_PIX_FMT_YUV420SP_NV12;

   param.video_process_filters = 0;// VIDEO_PROCESS_SCENETYPE | VIDEO_PROCESS_DIFFCHECK;//VIDEO_PROCESS_SCENETYPE | VIDEO_PROCESS_DENOISE | VIDEO_PROCESS_DIFFCHECK

   LiveExtendParam extand_param;
   memset(&extand_param, 0, sizeof(extand_param));
   extand_param.scene_type = SceneType_Natural;// SceneType_Artificial;
   //extand_param.frame_diff_mb = NULL;
   extand_param.same_last = 0;

   int luma_size = FRAMEWIDTH*FRAMEHEIGHT;
   int frame_size = luma_size * 3 / 2;

   FILE *file_in = fopen("F:\\files\\YUV420\\Stock.1280x720.25fps.yuv", "rb");
   FILE *file_out = fopen("F:\\files\\H264\\Stock.1280x720.25fps.shutdown.h264", "wb");
   //FILE *file_middle = fopen("I:\\files\\YUV420\\process\\Stock.1920x1080.5fps.process.yuv", "wb");
   //FILE *file_info = fopen("I:\\files\\YUV420\\process\\Stock.1280x720.25fps.process.txt", "wb");

   uint8_t *framedata; 
   uint8_t *stream;
   uint8_t *encodedata;
   int stream_size;
   int frame_type;
   uint32_t in_ts;
   uint32_t out_ts;

   X264Encoder encoder;
   //VideoPreprocess preprocessor;

   if (!encoder.Init(&param)){
      printf("Encoder initialization failed...\n");
      system("pause");
      return -1;
   }
    /*if (!preprocessor.Init(&param)){
   printf("Pre-processor initialization failed...\n");
   system("pause");
   return -1;
   }*/

   encodedata = NULL;
   framedata = (uint8_t *)malloc(frame_size);
   stream = (uint8_t *)malloc(frame_size);
   in_ts = 0;
   printf("Going to encode frames...\n");


   encoder.GetSpsPps((char *)stream,&stream_size);
   if (stream_size > 0){
      fwrite(stream, stream_size, 1, file_out);
   }
   while (1){
      if (fread(framedata, 1, frame_size, file_in) != frame_size){
         break;
      }
      encodedata = framedata;
      /*preprocessor.Process((char *)framedata, (char **)&encodedata, &extand_param);
      assert(encodedata != NULL);*/

#ifdef PREPROCESS_RECORD
      fwrite(encodedata, frame_size, 1, file_middle);
      fprintf(file_info,"[%d]------scenetype is %d\n",in_ts,extand_param.scene_type);
#endif // PREPROCESS_RECORD

      in_ts++;
      encoder.Encode((char *)encodedata, frame_size, (char *)stream, &stream_size, &frame_type, in_ts, &out_ts, &extand_param);
      if (stream_size > 0){
         fwrite(stream, stream_size, 1, file_out);
      }   
   }
   printf("%d frames were encoded...\n",in_ts);
   free(framedata);
   framedata = NULL;
   free(stream);
   stream = NULL;
   fclose(file_in);
   file_in = NULL;
   fclose(file_out);
   file_out = NULL;
  /* fclose(file_middle);
   file_middle = NULL;
   fclose(file_info);
   file_info = NULL;*/
   system("pause");
   return 0;
}

