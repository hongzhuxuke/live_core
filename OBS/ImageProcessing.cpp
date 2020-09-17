/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 ********************************************************************************/

#include "Main.h"
#include <inttypes.h>
#include <memory>
struct Convert444Data {
   LPBYTE input;
   LPBYTE output[3];
   bool bNV12;
   bool bKillThread;
   HANDLE hSignalConvert, hSignalComplete;
   int width, height, inPitch, outPitch, startY, endY;
   DWORD numThreads;
};
static int G_NumberThread = 1;
static List<HANDLE> G_CompleteEvents;
static HANDLE *G_h420Threads = NULL;
static Convert444Data *G_ConvertInfo = NULL;
void Convert444toNV12(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output);

DWORD STDCALL Convert444Thread(Convert444Data *data) {
   do {
      WaitForSingleObject(data->hSignalConvert, INFINITE);
      if (data->bKillThread) break;
      profileParallelSegment("Convert444Thread", "Convert444Threads", data->numThreads);
      if (data->bNV12)
         Convert444toNV12(data->input, data->width, data->inPitch, data->outPitch, data->height, data->startY, data->endY, data->output);
      else
         Convert444toNV12(data->input, data->width, data->inPitch, data->width, data->height, data->startY, data->endY, data->output);

      SetEvent(data->hSignalComplete);
   } while (!data->bKillThread);

   return 0;
}
void OBSImageProcessInit(int outputCX, int outputCY) {
   G_NumberThread = MAX(OSGetTotalCores() - 2, 1);
   G_h420Threads = (HANDLE*)Allocate(sizeof(HANDLE)*G_NumberThread);
   //YUV 444?YUV 420
   G_ConvertInfo = (Convert444Data*)Allocate(sizeof(Convert444Data)*G_NumberThread);

   zero(G_h420Threads, sizeof(HANDLE)*G_NumberThread);
   zero(G_ConvertInfo, sizeof(Convert444Data)*G_NumberThread);

   for (int i = 0; i < G_NumberThread; i++) {
      G_ConvertInfo[i].width = outputCX;
      G_ConvertInfo[i].height = outputCY;
      G_ConvertInfo[i].hSignalConvert = CreateEvent(NULL, FALSE, FALSE, NULL);
      G_ConvertInfo[i].hSignalComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
      G_ConvertInfo[i].numThreads = G_NumberThread;

      if (i == 0)
         G_ConvertInfo[i].startY = 0;
      else
         G_ConvertInfo[i].startY = G_ConvertInfo[i - 1].endY;

      if (i == (G_NumberThread - 1))
         G_ConvertInfo[i].endY = outputCY;
      else
         G_ConvertInfo[i].endY = ((outputCY / G_NumberThread)*(i + 1)) & 0xFFFFFFFE;
   }

   for (int i = 0; i < G_NumberThread; i++) {
      G_h420Threads[i] = OSCreateThread((XTHREAD)Convert444Thread, G_ConvertInfo + i);
      G_CompleteEvents << G_ConvertInfo[i].hSignalComplete;
   }

}
void OBSImageProcessDestory() {
   for (int i = 0; i < G_NumberThread; i++) {
      if (G_h420Threads[i]) {
         G_ConvertInfo[i].bKillThread = true;
         SetEvent(G_ConvertInfo[i].hSignalConvert);

         OSTerminateThread(G_h420Threads[i], 10000);
         G_h420Threads[i] = NULL;
      }

      if (G_ConvertInfo[i].hSignalConvert) {
         CloseHandle(G_ConvertInfo[i].hSignalConvert);
         G_ConvertInfo[i].hSignalConvert = NULL;
      }

      if (G_ConvertInfo[i].hSignalComplete) {
         CloseHandle(G_ConvertInfo[i].hSignalComplete);
         G_ConvertInfo[i].hSignalComplete = NULL;
      }
   }
   G_CompleteEvents.Clear();
   Free(G_h420Threads);
   Free(G_ConvertInfo);
}
void OBSImageProcess(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output) {
   for (int i = 0; i < G_NumberThread; i++) {
      G_ConvertInfo[i].input = input;
      G_ConvertInfo[i].inPitch = inPitch;

      G_ConvertInfo[i].output[0] = output[0];
      G_ConvertInfo[i].output[1] = output[1];
      G_ConvertInfo[i].output[2] = output[2];

      SetEvent(G_ConvertInfo[i].hSignalConvert);
   }
   WaitForMultipleObjects(G_CompleteEvents.Num(), G_CompleteEvents.Array(), TRUE, INFINITE);
}
void Convert444toI420(LPBYTE input, int width, int pitch, int height, int startY, int endY, LPBYTE *output) {
   profileSegment("Convert444toI420");
   LPBYTE lumPlane = output[0];
   LPBYTE uPlane = output[1];
   LPBYTE vPlane = output[2];
   int  chrPitch = width >> 1;

   __m128i lumMask = _mm_set1_epi32(0x0000FF00);
   __m128i uvMask = _mm_set1_epi16(0x00FF);

   for (int y = startY; y < endY; y += 2) {
      int yPos = y*pitch;
      int chrYPos = ((y >> 1)*chrPitch);
      int lumYPos = y*width;

      for (int x = 0; x < width; x += 4) {
         LPBYTE lpImagePos = input + yPos + (x * 4);
         int chrPos = chrYPos + (x >> 1);
         int lumPos0 = lumYPos + x;
         int lumPos1 = lumPos0 + width;

         __m128i line1 = _mm_load_si128((__m128i*)lpImagePos);
         __m128i line2 = _mm_load_si128((__m128i*)(lpImagePos + pitch));

         //pack lum vals
         {
            __m128i packVal = _mm_packs_epi32(_mm_srli_si128(_mm_and_si128(line1, lumMask), 1), _mm_srli_si128(_mm_and_si128(line2, lumMask), 1));
            packVal = _mm_packus_epi16(packVal, packVal);

            *(LPUINT)(lumPlane + lumPos0) = packVal.m128i_u32[0];
            *(LPUINT)(lumPlane + lumPos1) = packVal.m128i_u32[1];
         }

         //do average, pack UV vals
         {
            __m128i addVal = _mm_add_epi64(_mm_and_si128(line1, uvMask), _mm_and_si128(line2, uvMask));
            __m128i avgVal = _mm_srai_epi16(_mm_add_epi64(addVal, _mm_shuffle_epi32(addVal, _MM_SHUFFLE(2, 3, 0, 1))), 2);
            avgVal = _mm_shuffle_epi32(avgVal, _MM_SHUFFLE(3, 1, 2, 0));
            avgVal = _mm_shufflelo_epi16(avgVal, _MM_SHUFFLE(3, 1, 2, 0));
            avgVal = _mm_packus_epi16(avgVal, avgVal);

            DWORD packedVals = avgVal.m128i_u32[0];

            *(LPWORD)(uPlane + chrPos) = WORD(packedVals);
            *(LPWORD)(vPlane + chrPos) = WORD(packedVals >> 16);
         }
      }
   }
}
void Convert444toNV12(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output) {
   profileSegment("Convert444toNV12");
   LPBYTE lumPlane = output[0];
   LPBYTE uvPlane = output[1];

   __m128i lumMask = _mm_set1_epi32(0x0000FF00);
   __m128i uvMask = _mm_set1_epi16(0x00FF);

   for (int y = startY; y < endY; y += 2) {
      int yPos = y*inPitch;
      int uvYPos = (y >> 1)*outPitch;
      int lumYPos = y*outPitch;

      for (int x = 0; x < width; x += 4) {
         LPBYTE lpImagePos = input + yPos + (x * 4);
         int uvPos = uvYPos + x;
         int lumPos0 = lumYPos + x;
         int lumPos1 = lumPos0 + outPitch;

         __m128i line1 = _mm_load_si128((__m128i*)lpImagePos);
         __m128i line2 = _mm_load_si128((__m128i*)(lpImagePos + inPitch));

         //pack lum vals
         {
            __m128i packVal = _mm_packs_epi32(_mm_srli_si128(_mm_and_si128(line1, lumMask), 1), _mm_srli_si128(_mm_and_si128(line2, lumMask), 1));
            packVal = _mm_packus_epi16(packVal, packVal);

            *(LPUINT)(lumPlane + lumPos0) = packVal.m128i_u32[0];
            *(LPUINT)(lumPlane + lumPos1) = packVal.m128i_u32[1];
         }

         //do average, pack UV vals
         {
            __m128i addVal = _mm_add_epi64(_mm_and_si128(line1, uvMask), _mm_and_si128(line2, uvMask));
            __m128i avgVal = _mm_srai_epi16(_mm_add_epi64(addVal, _mm_shuffle_epi32(addVal, _MM_SHUFFLE(2, 3, 0, 1))), 2);
            avgVal = _mm_shuffle_epi32(avgVal, _MM_SHUFFLE(3, 1, 2, 0));

            *(LPUINT)(uvPlane + uvPos) = _mm_packus_epi16(avgVal, avgVal).m128i_u32[0];
         }
      }
   }
}

void Convert444toNV12_bak2(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output) {
   LPBYTE lumPlane = output[0];
   LPBYTE uvPlane = output[1];
   //4个int型，均赋值  0x0000FF00
   __m128i lumMask = _mm_set1_epi32(0x0000FF00);
   //8个short型赋值0x00FF
   __m128i uvMask = _mm_set1_epi16(0x00FF);

   for (int y = startY; y < endY; y += 2) {
      int yPos = y*inPitch;
      int uvYPos = (y >> 1)*outPitch;
      int lumYPos = y*outPitch;
      unsigned char byteLine1[16] = { 0 };
      unsigned char byteLine2[16] = { 0 };

      for (int x = 0; x < width; x += 4) {
         LPBYTE lpImagePos = input + yPos + (x * 4);
         int uvPos = uvYPos + x;
         int lumPos0 = lumYPos + x;
         int lumPos1 = lumPos0 + outPitch;

         __m128i sort8;
         sort8.m128i_i8[0] = 0x02;
         sort8.m128i_i8[1] = 0x01;
         sort8.m128i_i8[2] = 0x00;
         sort8.m128i_i8[3] = 0x03;

         sort8.m128i_i8[4] = 0x06;
         sort8.m128i_i8[5] = 0x05;
         sort8.m128i_i8[6] = 0x04;
         sort8.m128i_i8[7] = 0x07;

         sort8.m128i_i8[8] = 0x0a;
         sort8.m128i_i8[9] = 0x09;
         sort8.m128i_i8[10] = 0x08;
         sort8.m128i_i8[11] = 0x0b;

         sort8.m128i_i8[12] = 0x0e;
         sort8.m128i_i8[13] = 0x0d;
         sort8.m128i_i8[14] = 0x0c;
         sort8.m128i_i8[15] = 0x0f;






         //加载两个128位数据
         __m128i line1 = _mm_load_si128((__m128i*)lpImagePos);

         line1 = _mm_shuffle_epi8(line1, sort8);

         __m128i line2 = _mm_load_si128((__m128i*)(lpImagePos + inPitch));
         line2 = _mm_shuffle_epi8(line2, sort8);




         //pack lum vals
         {
            __m128i first_param = _mm_and_si128(line1, lumMask);
            __m128i second_param = _mm_and_si128(line2, lumMask);



            __m128i first = _mm_srli_si128(first_param, 1);
            __m128i second = _mm_srli_si128(second_param, 1);
            __m128i packVal = _mm_packs_epi32(first, second);

            packVal = _mm_packus_epi16(packVal, packVal);

            *(LPUINT)(lumPlane + lumPos0) = packVal.m128i_u32[0];
            *(LPUINT)(lumPlane + lumPos1) = packVal.m128i_u32[1];

         }

         //
         //do average, pack UV vals
         {
            //逻辑与操作
            __m128i addValFirst = _mm_and_si128(line1, uvMask);
            __m128i addValSecond = _mm_and_si128(line2, uvMask);
            //算术加操作
            __m128i addVal = _mm_add_epi64(addValFirst, addValSecond);

            // 按照  _MM_SHUFFLE(2, 3, 0, 1)  重新排序
            /*
                1 2 3 4 | 5 6 7 8 | 9 10 11 12 | 13 14 15 16

                5 6 7 8 | 1 2 3 4 | 13 14 15 16 | 9 10 11 12
                */

            //_mm_shuffle_epi8

            __m128i avgValParam_second = _mm_shuffle_epi32(addVal, _MM_SHUFFLE(2, 3, 0, 1));//10 11 00 01  //177 //B1   

            __m128i avgValParam = _mm_add_epi64(addVal, avgValParam_second);

            __m128i avgVal = _mm_srai_epi16(avgValParam, 2);

            avgVal = _mm_shuffle_epi32(avgVal, _MM_SHUFFLE(3, 1, 2, 0));  //11 01 10 00 //   216 // D8

            *(LPUINT)(uvPlane+uvPos) = _mm_packus_epi16(avgVal, avgVal).m128i_u32[0];


         }
      }
   }
}
void Convert444toNV12_bak(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output)
{
   LPBYTE lumPlane     = output[0];
   LPBYTE uvPlane		= output[1];

   __m128i lumMask = _mm_set1_epi32(0x0000FF00);
   __m128i uvMask = _mm_set1_epi16(0x00FF);

   for (int y = startY; y < endY; y += 2) {
      int yPos = y*inPitch;
      int uvYPos = (y >> 1)*outPitch;
      int lumYPos = y*outPitch;
      unsigned char byteLine1[16] = { 0 };
      unsigned char byteLine2[16] = { 0 };

      for (int x = 0; x < width; x += 4) {
         LPBYTE lpImagePos = input + yPos + (x * 4);
         int uvPos = uvYPos + x;
         int lumPos0 = lumYPos + x;
         int lumPos1 = lumPos0 + outPitch;

         __m128i line1 = _mm_load_si128((__m128i*)lpImagePos);
         __m128i line2 = _mm_load_si128((__m128i*)(lpImagePos+inPitch));

         //pack lum vals
         {
            __m128i packVal = _mm_packs_epi32(_mm_srli_si128(_mm_and_si128(line1, lumMask), 1), _mm_srli_si128(_mm_and_si128(line2, lumMask), 1));
            packVal = _mm_packus_epi16(packVal, packVal);
            *(LPUINT)(lumPlane+lumPos0) = packVal.m128i_u32[0];
            *(LPUINT)(lumPlane+lumPos1) = packVal.m128i_u32[1];
         }

         //do average, pack UV vals
         {
            __m128i addVal = _mm_add_epi64(_mm_and_si128(line1, uvMask), _mm_and_si128(line2, uvMask));
            __m128i avgVal = _mm_srai_epi16(_mm_add_epi64(addVal, _mm_shuffle_epi32(addVal, _MM_SHUFFLE(2, 3, 0, 1))), 2);
            avgVal = _mm_shuffle_epi32(avgVal, _MM_SHUFFLE(3, 1, 2, 0));
            *(LPUINT)(uvPlane + uvPos) = _mm_packus_epi16(avgVal, avgVal).m128i_u32[0];
         }
      }
   }
}

