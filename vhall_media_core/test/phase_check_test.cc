#include <stdio.h>
#include "../PhaseCheck.h"

int main() {
   int sampleBits, sampleRate, channels, bufSize;
   FILE *fpIn, *fpOut;
   char *buffer;
   float *bufFLT;
   int16_t *bufS16;
   errno_t err;

   err = fopen_s(&fpIn, "E:/TestAudioSet/wav/InPhase_stereo_44100_s16.pcm","rb");//input file path
   if (err) {
      printf("input file open fail!\n");
      return -1;
   }
   err = fopen_s(&fpOut, "E:/TestAudioSet/wav/InPhase_stereo_44100_s16_modified.pcm","wb");//output file path
   if (err) {
      printf("output file open fail!\n");
      return -1;
   }

   sampleBits = 32;//short - 16, float - 32
   sampleRate = 44100;//8000 - 48000
   channels = 2;//must be 2, only stereo audio is supported

   PhaseCheck *inst = new PhaseCheck;
   inst->Init(sampleRate, false);
   bufSize = sampleRate / 50 * channels * sizeof(float);
   buffer = new char[bufSize]; //20ms data here, you can change it but over 10ms recommended

   while (bufSize == fread(buffer, 1, bufSize, fpIn)) {
      //float type
      if (sampleBits = 32) {
         inst->Check((float *)buffer, bufSize);
      }
      //short type
      else {
         inst->Check((int16_t *)buffer, bufSize);
      }
      fwrite(buffer, 1, bufSize, fpOut);
   }
   if (fpIn) {
      fclose(fpIn);
      fpIn = nullptr;
   }
   if (fpOut) {
      fclose(fpOut);
      fpOut = nullptr;
   }
   return 0;
}