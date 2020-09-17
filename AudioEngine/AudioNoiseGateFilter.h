#ifndef __AUDIO_NOISE_GATE_FILTER_INCLUDE_H__
#define __AUDIO_NOISE_GATE_FILTER_INCLUDE_H__
#include "IAudioFilter.h"
#include <atomic>
class AudioNoiseGateFilter :public IAudioFilter {
public:
   AudioNoiseGateFilter();
   ~AudioNoiseGateFilter();

   void Init(unsigned int sampleRate, bool enable, float openThreshold, float closeThreshold);

   void SetSampleRate(unsigned int sampleRate);
   void SetEnableNoiseGate(bool enable);
public:
   virtual SAudioSegment* Process(SAudioSegment *segment);
   virtual void SetThresHoldValue(float closeThreshold, float openThreshold);
private:
   void ApplyNoiseGate(float *buffer, int totalFloats);
   void LoadDefaults();
   void LoadSettings();
   void SaveSettings();
private:
   float   mAttenuation; // Current gate multiplier
   float   mLevel;  // Input level with delayed decay
   float   mThresholdTime; // The amount of time we've held the gate open after it we hit the close threshold
   bool    mIsOpen;
   unsigned int  mSampleRateHz;
   
   // User configuration  
   HANDLE mThresholdMutex;
   float   mOpenThreshold;
   float   mCloseThreshold;
   float   mAttackTime;
   float   mHoldTime;
   float   mReleaseTime;
   std::atomic_bool    mIsEnable;
   ConfigFile mConfig;
};

#endif //__AUDIO_NOISE_GATE_FILTER_INCLUDE_H__

