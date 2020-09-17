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


#include "Utility.h"
#include <Audioclient.h>
#include "libsamplerate/samplerate.h"
#include "IAudioFilter.h"
#include "IAudioSource.h"
#include "Logging.h"
#include <math.h>

#define KSAUDIO_SPEAKER_4POINT1     (KSAUDIO_SPEAKER_QUAD|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_3POINT1     (KSAUDIO_SPEAKER_STEREO|SPEAKER_FRONT_CENTER|SPEAKER_LOW_FREQUENCY)
#define KSAUDIO_SPEAKER_2POINT1     (KSAUDIO_SPEAKER_STEREO|SPEAKER_LOW_FREQUENCY)

#define RUNONCE static bool bRunOnce = false; if(!bRunOnce && (bRunOnce = true)) 

QWORD GetQWDif(QWORD val1, QWORD val2) {
   return (val1 > val2) ? (val1 - val2) : (val2 - val1);
}

void MultiplyAudioBuffer(float *buffer, int totalFloats, float mulVal) {
   float sum = 0.0f;
   int totalFloatsStore = totalFloats;

   if ((UPARAM(buffer) & 0xF) == 0) {
      UINT alignedFloats = totalFloats & 0xFFFFFFFC;
      __m128 sseMulVal = _mm_set_ps1(mulVal);

      for (UINT i = 0; i < alignedFloats; i += 4) {
         __m128 sseScaledVals = _mm_mul_ps(_mm_load_ps(buffer + i), sseMulVal);
         _mm_store_ps(buffer + i, sseScaledVals);
      }

      buffer += alignedFloats;
      totalFloats -= alignedFloats;
   }

   for (int i = 0; i < totalFloats; i++)
      buffer[i] *= mulVal;
}

/* astoundingly disgusting hack to get more variables into the class without breaking API */
struct NotAResampler {
   SRC_STATE *resampler = nullptr;
   //帧调整间隔
   QWORD     jumpRange;

};

#define MoreVariables static_cast<NotAResampler*>(resampler)

IAudioSource::IAudioSource(AudioSourceType type) {
   mSourceType = type;
   sourceVolume = 1.0f;
   resampler = (void*)new NotAResampler;
   MoreVariables->jumpRange = PLAY_DIFF_TIME;
   mSumjumpCount=0;
   mDiff=0;
   mLastMediaStreamResetTime = 0;
}

IAudioSource::~IAudioSource() {
   if (bResample)
      src_delete(MoreVariables->resampler);

   for (UINT i = 0; i < audioSegments.Num(); i++)
      delete audioSegments[i];

   for (UINT i = 0; i < originalMicAudioSegments.Num(); i++)
      delete originalMicAudioSegments[i];

   delete (NotAResampler*)resampler;
   for (UINT i = 0; i < audioFilters.Num(); i++) {
      delete audioFilters[i];
   }
   audioFilters.Clear();
}


union TripleToLong {
   LONG val;
   struct {
      WORD wVal;
      BYTE tripleVal;
      BYTE lastByte;
   };
};

void IAudioSource::InitAudioData(bool bFloat, UINT channels, UINT samplesPerSec, UINT bitsPerSample, UINT blockSize, DWORD channelMask) {
   this->bFloat = bFloat;
   inputChannels = channels;
   inputSamplesPerSec = samplesPerSec;
   inputBitsPerSample = bitsPerSample;
   inputBlockSize = blockSize;
   inputChannelMask = channelMask;

   //-----------------------------

   UINT sampleRateHz = GetSampleRateHz();

   //if (inputSamplesPerSec != sampleRateHz)
	{
      int errVal;

      int converterType = SRC_SINC_FASTEST;
		if (NULL!=MoreVariables->resampler)
		{
			src_delete(MoreVariables->resampler);
		}
     
		MoreVariables->resampler = src_new(converterType, 2, &errVal);
		if (!MoreVariables->resampler)
			CrashError(TEXT("IAudioSource::InitAudioData: Could not initiate resampler"));

      resampleRatio = double(sampleRateHz) / double(inputSamplesPerSec);
      bResample = true;

      //----------------------------------------------------
      // hack to get rid of that weird first quirky resampled packet size

      SRC_DATA data;
      data.src_ratio = resampleRatio;

      List<float> blankBuffer;
      blankBuffer.SetSize(inputSamplesPerSec / 100 * 2);

      data.data_in = blankBuffer.Array();
      data.input_frames = inputSamplesPerSec / 100;

      UINT frameAdjust = UINT((double(data.input_frames) * resampleRatio) + 1.0);
      UINT newFrameSize = frameAdjust * 2;

      tempResampleBuffer.SetSize(newFrameSize);

      data.data_out = tempResampleBuffer.Array();
      data.output_frames = frameAdjust;

      data.end_of_input = 0;

      int err = src_process(MoreVariables->resampler, &data);

      nop();
   }

   //-------------------------------------------------------------------------

   if (inputChannels > 2) {
      switch (inputChannelMask) {
      case KSAUDIO_SPEAKER_QUAD:              Log(TEXT("Using quad speaker setup"));                          break; //ocd anyone?
      case KSAUDIO_SPEAKER_2POINT1:           Log(TEXT("Using 2.1 speaker setup"));                           break;
      case KSAUDIO_SPEAKER_3POINT1:           Log(TEXT("Using 3.1 speaker setup"));                           break;
      case KSAUDIO_SPEAKER_4POINT1:           Log(TEXT("Using 4.1 speaker setup"));                           break;
      case KSAUDIO_SPEAKER_SURROUND:          Log(TEXT("Using basic surround speaker setup"));                break;
      case KSAUDIO_SPEAKER_5POINT1:           Log(TEXT("Using 5.1 speaker setup"));                           break;
      case KSAUDIO_SPEAKER_5POINT1_SURROUND:  Log(TEXT("Using 5.1 surround speaker setup"));                  break;
      case KSAUDIO_SPEAKER_7POINT1:           Log(TEXT("Using 7.1 speaker setup"));                           break;
      case KSAUDIO_SPEAKER_7POINT1_SURROUND:  Log(TEXT("Using 7.1 surround speaker setup"));                  break;
      default:
         Log(TEXT("Using unknown speaker setup: 0x%lX, %d channels"), inputChannels, inputChannelMask);
         inputChannelMask = 0;
         break;
      }

      if (inputChannelMask == 0) {
         switch (inputChannels) {
         case 3: inputChannelMask = KSAUDIO_SPEAKER_2POINT1; break;
         case 4: inputChannelMask = KSAUDIO_SPEAKER_QUAD;    break;
         case 5: inputChannelMask = KSAUDIO_SPEAKER_4POINT1; break;
         case 6: inputChannelMask = KSAUDIO_SPEAKER_5POINT1; break;
         case 8: inputChannelMask = KSAUDIO_SPEAKER_7POINT1; break;
         default:
            CrashError(TEXT("Unknown speaker setup, no downmixer available."));
         }
      }
   }
}


const float dbMinus3 = 0.7071067811865476f;
const float dbMinus6 = 0.5f;
const float dbMinus9 = 0.3535533905932738f;

//not entirely sure if these are the correct coefficients for downmixing,
//I'm fairly new to the whole multi speaker thing
const float surroundMix = dbMinus3;
const float centerMix = dbMinus6;
const float lowFreqMix = dbMinus3;

const float surroundMix4 = dbMinus6;

const float attn5dot1 = 1.0f / (1.0f + centerMix + surroundMix);
const float attn4dotX = 1.0f / (1.0f + surroundMix4);
/*
   将音频数据加入到音频队列
*/
void IAudioSource::AddAudioSegment(SAudioSegment *newSegment, float curVolume, bool bSaveMicAudio) {

   //调整音量
   if (newSegment)
      MultiplyAudioBuffer(newSegment->audioData.Array(), newSegment->audioData.Num(), curVolume*sourceVolume);

   //保存原始音频数据，用于计算原始音量
   if (newSegment && bSaveMicAudio) {
      SAudioSegment* originalSegment = new SAudioSegment(newSegment->audioData.Array(), newSegment->audioData.Num(), newSegment->timestamp);
      if (originalSegment) {
         originalMicAudioSegments << originalSegment;
      }
   }

   for (UINT i = 0; i < audioFilters.Num(); i++) {
      if (newSegment)
         newSegment = audioFilters[i]->Process(newSegment);
   }

   if (newSegment)
      audioSegments << newSegment;

}
/*
   数据源时间戳整理

   1.输入参数 timestamp 为队列尾部的时间戳
   2.时间戳从后向前，依次递减10毫秒
*/
void IAudioSource::SortAudio(QWORD timestamp) {
   QWORD jumpAmount = 0;

   if (audioSegments.Num() <= 1)
      return;

   lastUsedTimestamp = lastSentTimestamp = audioSegments.Last()->timestamp = timestamp;

   for (UINT i = audioSegments.Num() - 1; i > 0; i--) {
      SAudioSegment *segment = audioSegments[i - 1];
      UINT frames = segment->audioData.Num() / 2;
      double totalTime = double(frames) / double(GetSampleRateHz())*1000.0;
      QWORD newTime = timestamp - QWORD(totalTime);
      if (newTime < segment->timestamp) {
         QWORD newAmount = (segment->timestamp - newTime);
         if (newAmount > jumpAmount) {
            jumpAmount = newAmount;
         }
         segment->timestamp = newTime;
      }
      timestamp = segment->timestamp;
   }

   if (jumpAmount > MoreVariables->jumpRange) {
      MoreVariables->jumpRange = jumpAmount;
   }
}

void IAudioSource::SortMicOrignaleAudio(QWORD timestamp) {
   QWORD jumpMicAudioAmount = 0;

   if (originalMicAudioSegments.Num() <= 1)
      return;

   originalMicAudioSegments.Last()->timestamp = timestamp;

   for (UINT i = originalMicAudioSegments.Num() - 1; i > 0; i--) {
      SAudioSegment *segment = originalMicAudioSegments[i - 1];
      UINT frames = segment->audioData.Num() / 2;
      double totalTime = double(frames) / double(GetSampleRateHz())*1000.0;
      QWORD newTime = timestamp - QWORD(totalTime);

      if (newTime < segment->timestamp) {
         QWORD newAmount = (segment->timestamp - newTime);
         if (newAmount > jumpMicAudioAmount)
            jumpMicAudioAmount = newAmount;

         segment->timestamp = newTime;
      }

      timestamp = segment->timestamp;
   }

   if (jumpMicAudioAmount > MoreVariables->jumpRange) {
      MoreVariables->jumpRange = jumpMicAudioAmount;
   }
}


UINT IAudioSource::GetSampleRateHz() {
   return mDstSampleRateHz;
}
UINT IAudioSource::QueryAudio(float curVolume) {
   return 0;
}

//获得音频数据源的音频数据
/*
   从数据源中GetNextBuffer获得音频数据
   1.转换为浮点型
   2.声道转换
   3.重采样
   4.数据插入队列
*/
UINT IAudioSource::QueryAudio2(float curVolume/*当前音量*/,bool bCanBurstHack/*是否可填满*/, bool bSaveMicAudio,QWORD duration /*= 0*/) {

   LPVOID buffer;
   UINT numAudioFrames=0;
   uint64_t newTimestamp;

   if (GetNextBuffer((void**)&buffer, &numAudioFrames, &newTimestamp)) {

      float *captureBuffer;
      //-------------------------浮点转换-------------------------------------
      if (!bFloat) {
         UINT totalSamples = numAudioFrames*inputChannels;
         if (convertBuffer.Num() < totalSamples)
            convertBuffer.SetSize(totalSamples);

         if (inputBitsPerSample == 8) {
            float *tempConvert = convertBuffer.Array();
            char *tempSByte = (char*)buffer;

            while (totalSamples--) {
               *(tempConvert++) = float(*(tempSByte++)) / 127.0f;
            }
         } else if (inputBitsPerSample == 16) {
            float *tempConvert = convertBuffer.Array();
            short *tempShort = (short*)buffer;

            while (totalSamples--) {
               *(tempConvert++) = float(*(tempShort++)) / 32767.0f;
            }
         } else if (inputBitsPerSample == 24) {
            float *tempConvert = convertBuffer.Array();
            BYTE *tempTriple = (BYTE*)buffer;
            TripleToLong valOut;

            while (totalSamples--) {
               TripleToLong &valIn = (TripleToLong&)tempTriple;

               valOut.wVal = valIn.wVal;
               valOut.tripleVal = valIn.tripleVal;
               if (valOut.tripleVal > 0x7F)
                  valOut.lastByte = 0xFF;

               *(tempConvert++) = float(double(valOut.val) / 8388607.0);
               tempTriple += 3;
            }
         } else if (inputBitsPerSample == 32) {
            float *tempConvert = convertBuffer.Array();
            long *tempShort = (long*)buffer;

            while (totalSamples--) {
               *(tempConvert++) = float(double(*(tempShort++)) / 2147483647.0);
            }
         }

         captureBuffer = convertBuffer.Array();
      }
      else
         captureBuffer = (float*)buffer;

      if (tempBuffer.Num() < numAudioFrames * 2)
         tempBuffer.SetSize(numAudioFrames * 2);

      float *dataOutputBuffer = tempBuffer.Array();
      float *tempOut = dataOutputBuffer;

      //-------------------------双声道转换-------------------------------------
      if (inputChannels == 1) {
         UINT  numFloats = numAudioFrames;
         float *inputTemp = (float*)captureBuffer;
         float *outputTemp = dataOutputBuffer;

         if ((UPARAM(inputTemp) & 0xF) == 0 && (UPARAM(outputTemp) & 0xF) == 0) {
            UINT alignedFloats = numFloats & 0xFFFFFFFC;
            for (UINT i = 0; i < alignedFloats; i += 4) {
               __m128 inVal = _mm_load_ps(inputTemp + i);

               __m128 outVal1 = _mm_unpacklo_ps(inVal, inVal);
               __m128 outVal2 = _mm_unpackhi_ps(inVal, inVal);

               _mm_store_ps(outputTemp + (i * 2), outVal1);
               _mm_store_ps(outputTemp + (i * 2) + 4, outVal2);
            }

            numFloats -= alignedFloats;
            inputTemp += alignedFloats;
            outputTemp += alignedFloats * 2;
         }

         while (numFloats--) {
            float inputVal = *inputTemp;
            *(outputTemp++) = inputVal;
            *(outputTemp++) = inputVal;

            inputTemp++;
         }
      }
      else if (inputChannels == 2) {
         memcpy(dataOutputBuffer, captureBuffer, numAudioFrames * 2 * sizeof(float));
      }
      else {
         //混音算法
         //todo: downmix optimization, also support for other speaker configurations than ones I can merely "think" of.  ugh.
         float *inputTemp = (float*)captureBuffer;
         float *outputTemp = dataOutputBuffer;

         if (inputChannelMask == KSAUDIO_SPEAKER_QUAD) {
            UINT numFloats = numAudioFrames * 4;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];
               float rearLeft = inputTemp[2] * surroundMix4;
               float rearRight = inputTemp[3] * surroundMix4;

               // When in doubt, use only left and right .... and rear left and rear right :) 
               // Same idea as with 5.1 downmix

               *(outputTemp++) = (left + rearLeft)  * attn4dotX;
               *(outputTemp++) = (right + rearRight) * attn4dotX;

               inputTemp += 4;
            }
         } else if (inputChannelMask == KSAUDIO_SPEAKER_2POINT1) {
            UINT numFloats = numAudioFrames * 3;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];

               // Drop LFE since we don't need it
               //float lfe       = inputTemp[2]*lowFreqMix;

               *(outputTemp++) = left;
               *(outputTemp++) = right;

               inputTemp += 3;
            }
         } else if (inputChannelMask == KSAUDIO_SPEAKER_3POINT1) {
            UINT numFloats = numAudioFrames * 4;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];
               float frontCenter = inputTemp[2];
               float lowFreq = inputTemp[3];

               *(outputTemp++) = left;
               *(outputTemp++) = right;

               inputTemp += 4;
            }
         } else if (inputChannelMask == KSAUDIO_SPEAKER_4POINT1) {
            UINT numFloats = numAudioFrames * 5;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];

               // Skip LFE , we don't really need it.
               //float lfe       = inputTemp[2];

               float rearLeft = inputTemp[3] * surroundMix4;
               float rearRight = inputTemp[4] * surroundMix4;

               // Same idea as with 5.1 downmix

               *(outputTemp++) = (left + rearLeft)  * attn4dotX;
               *(outputTemp++) = (right + rearRight) * attn4dotX;

               inputTemp += 5;
            }
         } else if (inputChannelMask == KSAUDIO_SPEAKER_SURROUND) {
            UINT numFloats = numAudioFrames * 4;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];
               float frontCenter = inputTemp[2];
               float rearCenter = inputTemp[3];
               *(outputTemp++) = left;
               *(outputTemp++) = right;

               inputTemp += 4;
            }
         }
         else if (inputChannelMask == KSAUDIO_SPEAKER_5POINT1 || inputChannelMask == KSAUDIO_SPEAKER_5POINT1_SURROUND) {
            UINT numFloats = numAudioFrames * 6;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];
               float center = inputTemp[2] * centerMix;

               float rearLeft = inputTemp[4] * surroundMix;
               float rearRight = inputTemp[5] * surroundMix;

               *(outputTemp++) = (left + center + rearLeft) * attn5dot1;
               *(outputTemp++) = (right + center + rearRight) * attn5dot1;

               inputTemp += 6;
            }
         }
         else if (inputChannelMask == KSAUDIO_SPEAKER_7POINT1) {
            UINT numFloats = numAudioFrames * 8;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];

               float center = inputTemp[2] * centerMix;

               float rearLeft = inputTemp[4] * surroundMix;
               float rearRight = inputTemp[5] * surroundMix;

               *(outputTemp++) = (left + center + rearLeft)  * attn5dot1;
               *(outputTemp++) = (right + center + rearRight) * attn5dot1;

               inputTemp += 8;
            }
         }

         else if (inputChannelMask == KSAUDIO_SPEAKER_7POINT1_SURROUND) {
            UINT numFloats = numAudioFrames * 8;
            float *endTemp = inputTemp + numFloats;

            while (inputTemp < endTemp) {
               float left = inputTemp[0];
               float right = inputTemp[1];
               float center = inputTemp[2] * centerMix;
               float rearLeft = inputTemp[4];
               float rearRight = inputTemp[5];
               float sideLeft = inputTemp[6];
               float sideRight = inputTemp[7];

               // combine the rear/side channels first , baaam! 5.1
               rearLeft = (rearLeft + sideLeft)  * 0.5f;
               rearRight = (rearRight + sideRight) * 0.5f;

               // downmix to stereo as in 5.1 case
               *(outputTemp++) = (left + center + rearLeft  * surroundMix) * attn5dot1;
               *(outputTemp++) = (right + center + rearRight * surroundMix) * attn5dot1;

               inputTemp += 8;
            }
         }
      }

      ReleaseBuffer();

      //----------------------------重采样-----------------------------------------
      if (bResample) {
         //帧调整
         UINT frameAdjust = UINT((double(numAudioFrames) * resampleRatio) + 1.0);
         UINT newFrameSize = frameAdjust * 2;

         if (tempResampleBuffer.Num() < newFrameSize)
            tempResampleBuffer.SetSize(newFrameSize);

         SRC_DATA data;
         data.src_ratio = resampleRatio;

         data.data_in = tempBuffer.Array();
         data.input_frames = numAudioFrames;

         data.data_out = tempResampleBuffer.Array();
         data.output_frames = frameAdjust;

         data.end_of_input = 0;
         //重采样
         int err = src_process(MoreVariables->resampler, &data);
         if (err) {
            RUNONCE AppWarning(TEXT("IAudioSource::QueryAudio: Was unable to resample audio for device '%s'"), GetDeviceName());
            return NoAudioAvailable;
         }

         if (data.input_frames_used != numAudioFrames) {
            RUNONCE AppWarning(TEXT("IAudioSource::QueryAudio: Failed to downsample buffer completely, which shouldn't actually happen because it should be using 10ms of samples"));
            return NoAudioAvailable;
         }

         numAudioFrames = data.output_frames_gen;
      }
      //-------------------------音频数据插入队列------------------------------------
      if (!lastUsedTimestamp)
         lastUsedTimestamp = newTimestamp;
      else
         lastUsedTimestamp += 10;

      QWORD difVal = GetQWDif(newTimestamp, lastUsedTimestamp);
      QWORD jumpRange = MoreVariables->jumpRange;
      if (difVal > jumpRange) { 
        // gLogger->logInfo("%s mSourceType:%d  ** diff:%llu", __FUNCTION__, mSourceType, difVal);
         /*if (mSourceType == eAudioSource_DShowAudio || mSourceType == eAudioSource_Media) {
            int num = audioSegments.Num();
            audioSegments.Clear();
            originalMicAudioSegments.Clear();
            ClearAudioBuffer();
         }*/
         lastUsedTimestamp = newTimestamp;
      }
      float *newBuffer = (bResample) ? tempResampleBuffer.Array() : tempBuffer.Array();
      SAudioSegment *newSegment = new SAudioSegment(newBuffer, numAudioFrames * 2, newTimestamp);
      AddAudioSegment(newSegment, curVolume*sourceVolume, bSaveMicAudio);
      lastSentTimestamp = lastUsedTimestamp;
      return AudioAvailable;
   }
   //没有获得下一个数据包
   //gLogger->logInfo("IAudioSource::QueryAudio2  NoAudioAvailable\n");
   //OBSApiUIWarning(L"声音采集异常，请检查或重连音频设备");
   return NoAudioAvailable;
}

bool IAudioSource::GetNextOriBuffer(unsigned char *&buffer, UINT &numFrames, QWORD &timestamp, bool &floatFormat, UINT& bitPerSample, UINT &channels) {
  if (GetNextBuffer((void**)&buffer, &numFrames, &timestamp)) {
    floatFormat = bFloat;
    bitPerSample = inputBitsPerSample;
    channels = inputChannels;
    return true;
  }
  return false;
}
bool IAudioSource::GetEarliestTimestamp(QWORD &timestamp) {
   if (audioSegments.Num()) {
      timestamp = audioSegments[0]->timestamp;
      return true;
   }

   return false;
}


bool IAudioSource::GetLatestTimestamp(QWORD &timestamp) {
   if (audioSegments.Num()) {
      timestamp = audioSegments.Last()->timestamp;     
      static int logIndex=0;
      if(logIndex%1000==0)
      {
         QWORD osTime=GetQPCTimeMS();
         //Log(TEXT("GetLatestTimestamp timestamp %llu,osTime %llu"),timestamp,osTime);
         logIndex=0;
      }

      logIndex++;
      
      return true;
   }

   return false;
}

bool IAudioSource::GetLatestMicAudioTimestamp(QWORD &timestamp) {
   if (originalMicAudioSegments.Num()) {
      timestamp = originalMicAudioSegments.Last()->timestamp;
      static int logIndex = 0;
      if (logIndex % 1000 == 0) {
         QWORD osTime = GetQPCTimeMS();
         //Log(TEXT("GetLatestTimestamp timestamp %llu,osTime %llu"),timestamp,osTime);
         logIndex = 0;
      }

      logIndex++;

      return true;
   }

   return false;
}

/*
   1.audioSegments为数据源的音频队列，删除小于目标时间的音频数据
   2.获得大于targetTimestamp且与targetTimestamp之差在11毫秒的音频数据
*/
bool IAudioSource::GetBuffer
   (float **buffer,/*输出参数，音频缓存*/
   QWORD targetTimestamp, QWORD &curTimeStamp) /*输入参数，基准时间戳*/{

   bool bSuccess = false;
   bool bDeleted = false;

   outputBuffer.Clear();
   if (mSourceType == eAudioSource_Media) {
	   //while (audioSegments.Num()) {
		  // QWORD audioSegmentTimestamp = audioSegments[0]->timestamp;
		  // if (audioSegmentTimestamp < targetTimestamp - 120) {
			 //  SAudioSegment* firstAudioSegment = audioSegments[0];
			 //  QWORD diff = targetTimestamp - firstAudioSegment->timestamp;
			 //  delete audioSegments[0];
			 //  audioSegments.Remove(0);
			 //  //g_pLogger->logInfo("eAudioSource_Media audioSegmentTimestamp drop:%lld diff value:%d", audioSegmentTimestamp, diff);
			 //  bDeleted = true;
		  // }
		  // else {
			 //  break;
		  // }
	   //}

	   //if (audioSegments.Num()) {
		  // bool bUseSegment = false;
		  // SAudioSegment *segment = audioSegments[0];
		  // int difference = (segment->timestamp - targetTimestamp);
		  // if (bDeleted || abs(difference) <= 121) {
			 //  outputBuffer.TransferFrom(segment->audioData);
			 //  delete segment;
			 //  audioSegments.Remove(0);
			 //  bSuccess = true;
		  // }
		  // else {
			 //  //g_pLogger->logInfo("eAudioSource_Media null segment->timestamp - targetTimestamp:%d", difference);
		  // }
	   //}

      if (audioSegments.Num()) {
         bool bUseSegment = false;
         SAudioSegment *segment = audioSegments[0];
         curTimeStamp = segment->timestamp;
         outputBuffer.TransferFrom(segment->audioData);
         delete segment;
         audioSegments.Remove(0);
         bSuccess = true;
      }
   }else{
	   while (audioSegments.Num() && mSourceType != eAudioSource_Mic && mSourceType != eAudioSource_Player && targetTimestamp > 0) {
		   SAudioSegment *segment = audioSegments[0];
		   if (segment) {
			   QWORD osTime = GetQPCTimeMS();
			   QWORD audioSegmentTimestamp = segment->timestamp;
			   if (audioSegmentTimestamp + PLAY_DIFF_TIME < targetTimestamp) {
				   if (mSourceType == eAudioSource_Media) {
					   QWORD diff = targetTimestamp - audioSegmentTimestamp;
					   //gLogger->logInfo("IAudioSource::QueryAudio2 1 NoAudioAvailable diff:%lld", diff);
					   if (osTime - mLastMediaStreamResetTime > PLAY_RESET_TIME || mLastMediaStreamResetTime == 0) {
						   //gLogger->logInfo("IAudioSource::QueryAudio2 1 ClearAudioBuffer");
						   ClearAudioBuffer();
						   mLastMediaStreamResetTime = osTime;
					   }
				   }
				   else {
					   segment->ClearData();
					   delete segment;
					   audioSegments.Remove(0);
					   for (int i = 0; i < originalMicAudioSegments.Num(); i++) {
						   SAudioSegment *audioSegment = originalMicAudioSegments[0];
						   if (audioSegment) {
							   audioSegment->ClearData();
							   delete audioSegment;
							   originalMicAudioSegments.Remove(0);
						   }
					   }
					   ClearAudioBuffer();
				   }
				   break;
			   }
			   else if (targetTimestamp + PLAY_DIFF_TIME < audioSegmentTimestamp) {
				   if (mSourceType == eAudioSource_Media) {
					   QWORD diff = audioSegmentTimestamp - targetTimestamp;
					   //gLogger->logInfo("IAudioSource::QueryAudio2 ********* NoAudioAvailable diff:%lld", diff);
					   if (osTime - mLastMediaStreamResetTime > PLAY_RESET_TIME || mLastMediaStreamResetTime == 0) {
						   //gLogger->logInfo("IAudioSource::QueryAudio2 ********* ClearAudioBuffer");
						   ClearAudioBuffer();
						   mLastMediaStreamResetTime = osTime;
					   }
				   }
				   break;
			   }
			   else {
				   break;
			   }
		   }
	   }

	   if (audioSegments.Num()) {
		   SAudioSegment *segment = audioSegments[0];
		   if (segment != nullptr) {
            curTimeStamp = segment->timestamp;
			   outputBuffer.TransferFrom(segment->audioData);
			   segment->ClearData();
			   delete segment;
			   audioSegments.Remove(0);
			   bSuccess = true;
		   }
	   }
   }

   outputBuffer.SetSize(GetSampleRateHz() / 100 * 2);
   
   *buffer = outputBuffer.Array();

   return bSuccess;
}

bool  IAudioSource::GetMicOrignalBuffer(float **buffer, QWORD targetTimestamp) {
   bool bSuccess = false;
   bool bDeleted = false;

   micOutputBuffer.Clear();

   //while (originalMicAudioSegments.Num()) {
   //   QWORD audioSegmentTimestamp = originalMicAudioSegments[0]->timestamp;
   //   if (audioSegmentTimestamp + PLAY_DIFF_TIME < targetTimestamp) {
   //      SAudioSegment* firstAudioSegment = originalMicAudioSegments[0];
   //      QWORD diff = targetTimestamp - firstAudioSegment->timestamp;
   //      delete originalMicAudioSegments[0];
   //      originalMicAudioSegments.Remove(0);
   //      bDeleted = true;
   //   } else {
   //      break;
   //   }
   //}

   if (originalMicAudioSegments.Num()) {
      bool bUseSegment = false;
      SAudioSegment *segment = originalMicAudioSegments[0];
      //QWORD difference = (segment->timestamp - targetTimestamp);
      if (true) {
         micOutputBuffer.TransferFrom(segment->audioData);
         delete segment;
         originalMicAudioSegments.Remove(0);
         bSuccess = true;
      }
   }

   micOutputBuffer.SetSize(GetSampleRateHz() / 100 * 2);
   *buffer = micOutputBuffer.Array();
   return bSuccess;
}

void IAudioSource::ClearBuffer() {
   audioSegments.Clear();
   originalMicAudioSegments.Clear();
}

void IAudioSource::ClearMicOrignalBuffer() {
   micOutputBuffer.Clear();
   for (int i = 0; i < originalMicAudioSegments.Num(); i++) {
      delete originalMicAudioSegments[i];
   }
   originalMicAudioSegments.Clear();
}

bool IAudioSource::GetNewestFrame(float **buffer) {
   if (buffer) {
      if (audioSegments.Num()) {
         List<float> &data = audioSegments.Last()->audioData;
         *buffer = data.Array();
         return true;
      }
   }

   return false;
}

QWORD IAudioSource::GetBufferedTime() {
   if (audioSegments.Num())
      return audioSegments.Last()->timestamp - audioSegments[0]->timestamp;

   return 0;
}


void IAudioSource::StartCapture() {}
void IAudioSource::StopCapture() {}


UINT IAudioSource::GetChannelCount() const { return inputChannels; }
UINT IAudioSource::GetSamplesPerSec() const { return inputSamplesPerSec; }

int  IAudioSource::GetTimeOffset() const { return timeOffset; }
void IAudioSource::SetTimeOffset(int newOffset) { timeOffset = newOffset; }

void IAudioSource::SetVolume(float fVal) { 
   sourceVolume = fabsf(fVal);
   if (gLogger) {
      gLogger->logInfo("IAudioSource::SetVolume sourceVolume:%f", sourceVolume);
   }
}
float IAudioSource::GetVolume() const { return sourceVolume; }

UINT IAudioSource::NumAudioFilters() const { return audioFilters.Num(); }
IAudioFilter* IAudioSource::GetAudioFilter(UINT id) { if (audioFilters.Num() > id) return audioFilters[id]; return NULL; }

void IAudioSource::AddAudioFilter(IAudioFilter *filter) { audioFilters << filter; }
void IAudioSource::InsertAudioFilter(UINT pos, IAudioFilter *filter) { audioFilters.Insert(pos, filter); }
void IAudioSource::RemoveAudioFilter(IAudioFilter *filter) { audioFilters.RemoveItem(filter); }
void IAudioSource::RemoveAudioFilter(UINT id) { if (audioFilters.Num() > id) audioFilters.Remove(id); }

void IAudioSource::SetDstSampleRateHz(const int& iDstSampleRateHz)
{
	mDstSampleRateHz = iDstSampleRateHz;
   audioSegments.Clear();
}


