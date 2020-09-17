#include <stdio.h>
#include <string.h>
#include <chrono>
#include <iostream>
#include "ImageEnhance.h"

//Please modify the parameters below
#define WIDTH 1920	//width of the frame
#define HEIGHT 1080	//height of the frame


#define SCREEN
//#define CAMERA

#define ARGB
//#define YUV420

int main(){
	FILE *fpInputFile;
	FILE *fpOutputFile;
	
#ifdef YUV420
   pixel *inputFrame;
   pixel *outputFrame;
#endif // YUV420
#ifdef ARGB
   uint32_t *inputFrame;
   uint32_t *outputFrame;
#endif // ARGB
	int iFrame;
	int iFrameSize;
	ImageEnhance filter;
   VideoFilterParam cfg;
	int filterType;

   cfg.width = WIDTH;
   cfg.height = HEIGHT;
#ifdef YUV420
   fpInputFile = fopen("F:/TestVideoSet/NV12/NaturalScene.1920x1080.25fps.yuv", "rb");
   fpOutputFile = fopen("F:/TestVideoSet/NV12/NaturalScene.1920x1080.25fps_sharpen.yuv", "wb");
#endif // YUV420
#ifdef ARGB
   fpInputFile = fopen("F:/TestVideoSet/mPlayerAudioFile_1920x1080.rgb", "rb");
   fpOutputFile = fopen("F:/TestVideoSet/mPlayerAudioFile_1920x1080_sharpen.rgb", "wb");
#endif // ARGB

   
	

	iFrame = 0;
#ifdef YUV420
   iFrameSize = HEIGHT * WIDTH * 3 / 2;
   inputFrame = new pixel[iFrameSize];
   outputFrame = new pixel[iFrameSize];
#endif // YUV420
#ifdef ARGB
   iFrameSize = HEIGHT * WIDTH * 4;
   inputFrame = new uint32_t[iFrameSize/4];
   outputFrame = new uint32_t[iFrameSize/4];
#endif // ARGB
	filterType = 0;
	filter.Init(&cfg);
   auto start = std::chrono::steady_clock::now();
	for (;; iFrame++){
		if (fread(inputFrame, 1, iFrameSize, fpInputFile) != iFrameSize){
			break;
		}

#ifdef SCREEN
#ifdef YUV420
      filter.EdgeEnhance(inputFrame, outputFrame, PixelFmt_I420);
      fwrite(outputFrame, 1, iFrameSize, fpOutputFile);
#endif // YUV420
#ifdef ARGB
      filter.EdgeEnhance(inputFrame, outputFrame, PixelFmt_RGB);
      fwrite(outputFrame, 1, iFrameSize, fpOutputFile);
#endif // ARGB
#endif // SCREEN

#ifdef CAMERA
#ifdef YUV420
      filter.Denoise(inputFrame, outputFrame, PixelFmt_YV12);
      filter.Brighter(outputFrame, inputFrame, PixelFmt_YV12); //we just exchange input and output buffer for simplify code
      fwrite(inputFrame, 1, iFrameSize, fpOutputFile);
#endif // YUV420
#endif // CAMERA

	}
   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double, std::milli> elapsed = end - start;
   std::cout << iFrame <<  "frames take " << elapsed.count() << "ms(" << elapsed.count()/iFrame <<" ms/f)"<<std::endl;
   
	if (inputFrame){
		delete[] inputFrame;
		inputFrame = nullptr;
	}
	if (outputFrame){
		delete[] outputFrame;
		outputFrame = nullptr;
	}
	fclose(fpInputFile);
	fclose(fpOutputFile);

	return 0;
}