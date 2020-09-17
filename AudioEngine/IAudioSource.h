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

#define DECKLINKAUDIODEBUG_
#ifndef __AUDIO_SOURCE_INCLUDE__
#define __AUDIO_SOURCE_INCLUDE__
#include "VH_ConstDeff.h"

#define PLAY_DIFF_TIME 170
#define PLAY_RESET_TIME 20000

enum {
   NoAudioAvailable,
   AudioAvailable,
};

enum AudioSourceType {
   eAudioSource_None = 0,
   eAudioSource_Mic = 1,
   eAudioSource_Player = 2,
   eAudioSource_Media = 3,
   eAudioSource_DeckLink = 4,
   eAudioSource_DShowAudio = 5,
};

struct SAudioSegment {
   List<float> audioData;
   QWORD timestamp;

   inline SAudioSegment(float *data, UINT numFloats, QWORD timestamp) : timestamp(timestamp) {
      audioData.CopyArray(data, numFloats);
   }

   inline void ClearData() {
      audioData.Clear();
   }
};

class IAudioFilter;

class  __declspec(dllexport)IAudioSource {
   bool bResample;
   LPVOID resampler;
   double resampleRatio;

   //-----------------------------------------

   List<IAudioFilter*> audioFilters;

   //-----------------------------------------

   List<SAudioSegment*> audioSegments; /* 混音所需要的音频数据 */
   List<SAudioSegment*> originalMicAudioSegments;

   QWORD lastUsedTimestamp;
   QWORD lastSentTimestamp;
   int timeOffset;

   //-----------------------------------------

   List<float> storageBuffer;

   //-----------------------------------------

   List<float> outputBuffer;
   List<float> micOutputBuffer;
   List<float> convertBuffer;
   List<float> tempBuffer;
   List<float> tempResampleBuffer;

   //-----------------------------------------

   bool  bFloat;
   UINT  inputChannels;
   UINT  inputSamplesPerSec;
   UINT  inputBitsPerSample;
   UINT  inputBlockSize;
   DWORD inputChannelMask;


   QWORD mLastGetBufferTime;
   QWORD mLastMediaStreamResetTime;
   QWORD mSumjumpCount;
   int mDiff;

   //-----------------------------------------

   float sourceVolume;
   AudioSourceType mSourceType = eAudioSource_None;
   //-----------------------------------------
   DeviceInfo mDeviceInfo;
protected:
   UINT mDstSampleRateHz;
   void AddAudioSegment(SAudioSegment *segment, float curVolume, bool bSaveMicAudio);

protected:

   void InitAudioData(bool bFloat, UINT channels, UINT samplesPerSec, UINT bitsPerSample, UINT blockSize, DWORD channelMask);

   //-----------------------------------------

   virtual CTSTR GetDeviceName() const = 0;

   //-----------------------------------------

   virtual bool GetNextBuffer(void **buffer, UINT *numFrames, QWORD *timestamp) = 0;
   virtual void ReleaseBuffer() = 0;
   virtual void ClearAudioBuffer() {};


public:

   //-----------------------------------------
   
   IAudioSource(AudioSourceType type);
   virtual ~IAudioSource();
   virtual bool isDecklinkDevice(){return false;}
	virtual void SetDstSampleRateHz(const int& iDstSampleRateHz);
   virtual UINT GetSampleRateHz(); // for dst sample rate
   virtual UINT QueryAudio(float curVolume);
   virtual bool GetEarliestTimestamp(QWORD &timestamp);
   virtual bool GetBuffer(float **buffer, QWORD targetTimestamp, QWORD &curTimeStamp);
   virtual void ClearMicOrignalBuffer();
   virtual void ClearBuffer();
   virtual bool GetMicOrignalBuffer(float **buffer, QWORD targetTimestamp);
   virtual bool Initialize() { return true; }
   virtual bool GetNewestFrame(float **buffer);
   virtual void SetCurrentBaseTime(QWORD baseTime){};
   virtual QWORD GetBufferedTime();
   virtual void StartCapture();
   virtual void StopCapture();
   DeviceInfo GetDeviceInfo()
   {
      return mDeviceInfo;
   }
   void SetDeviceInfo(DeviceInfo deviceInfo)
   {
      mDeviceInfo=deviceInfo;
   }
   UINT GetChannelCount() const;
   UINT GetSamplesPerSec() const;

   int  GetTimeOffset() const;
   void SetTimeOffset(int newOffset);

   void SetVolume(float fVal);
   float GetVolume() const;

   UINT NumAudioFilters() const;
   IAudioFilter* GetAudioFilter(UINT id);

   void AddAudioFilter(IAudioFilter *filter);
   void InsertAudioFilter(UINT pos, IAudioFilter *filter);
   void RemoveAudioFilter(IAudioFilter *filter);
   void RemoveAudioFilter(UINT id);

   virtual bool GetLatestTimestamp(QWORD &timestamp);
   virtual bool GetLatestMicAudioTimestamp(QWORD &timestamp);

   void SortAudio(QWORD timestamp);
   void SortMicOrignaleAudio(QWORD timestamp);
   UINT QueryAudio2(float curVolume, bool bCanBurst = false, bool bSaveOrignalAudio = false, QWORD duration = 0);
   bool GetNextOriBuffer(unsigned char *&buffer, UINT &numFrames, QWORD &timestamp, bool &floatFormat, UINT& bitPerSample, UINT &channels);
   CTSTR GetDeviceName2() const { return GetDeviceName(); }

};



#endif

