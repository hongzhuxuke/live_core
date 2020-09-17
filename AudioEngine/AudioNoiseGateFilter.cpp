#include "Utility.h"
#include "AudioNoiseGateFilter.h"
#include "IAudioSource.h"
#include "pathManage.h"

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

inline float rmsToDb(float rms) {
   float db = 20.0f * log10(rms);
   if (!_finite(db))
      return VOL_MIN;
   return db;
}

inline float dbToRms(float db) {
   return pow(10.0f, db / 20.0f);
}


AudioNoiseGateFilter::AudioNoiseGateFilter() :
mAttenuation(0.0f),
mLevel(0.0f),
mThresholdTime(0.0f),
mIsOpen(false) {
   LoadDefaults();

	wstring dir = GetAppDataPath();
	WCHAR tmp[512] = {0};
	wcscat_s(tmp, _MAX_PATH, dir.c_str());
	wcscat_s(tmp, _MAX_PATH, L"\\audio.ini");
	mConfig.Open(tmp, true);

   mThresholdMutex = OSCreateMutex();
}


AudioNoiseGateFilter::~AudioNoiseGateFilter() {
   OSCloseMutex(mThresholdMutex);
}

SAudioSegment* AudioNoiseGateFilter::Process(SAudioSegment *segment) {
   OSEnterMutex(mThresholdMutex);
   if (mIsEnable) {
      ApplyNoiseGate(segment->audioData.Array(), segment->audioData.Num());
   } else {
      // Reset state
      mAttenuation = 0.0f;
      mLevel = 0.0f;
      mThresholdTime = 0.0f;
      mIsOpen = false;
   }
   OSLeaveMutex(mThresholdMutex);
   return segment;
}

void AudioNoiseGateFilter::ApplyNoiseGate(float *buffer, int totalFloats) {
   // We assume stereo input

   if (totalFloats % 2)
      return; // Odd number of samples

   const float SAMPLE_RATE_F = float(mSampleRateHz);
   const float dtPerSample = 1.0f / SAMPLE_RATE_F;

   // Convert configuration times into per-sample amounts
   const float attackRate = 1.0f / (mAttackTime * SAMPLE_RATE_F);
   const float releaseRate = 1.0f / (mReleaseTime * SAMPLE_RATE_F);

   // Determine level decay rate. We don't want human voice (75-300Hz) to cross the close
   // threshold if the previous peak crosses the open threshold.
   const float thresholdDiff = mOpenThreshold - mCloseThreshold;
   const float minDecayPeriod = (1.0f / 75.0f) * SAMPLE_RATE_F;
   const float decayRate = thresholdDiff / minDecayPeriod;

   // We can't use SSE as the processing of each sample depends on the processed
   // result of the previous sample.
   for (int i = 0; i < totalFloats; i += 2) {
      // Get current input level
      float curLvl = abs((buffer[i] + buffer[i + 1])) * 0.5f;

      // Test thresholds
      if (curLvl > mOpenThreshold && !mIsOpen)
         mIsOpen = true;
      if (mLevel < mCloseThreshold && mIsOpen) {
         mThresholdTime = 0.0f;
         mIsOpen = false;
      }

      // Decay level slowly so human voice (75-300Hz) doesn't cross the close threshold
      // (Essentially a peak detector with very fast decay)
      mLevel = max(mLevel, curLvl) - decayRate;

      // Apply gate state to attenuation
      if (mIsOpen)
         mAttenuation = min(1.0f, mAttenuation + attackRate);
      else {
         mThresholdTime += dtPerSample;
         if (mThresholdTime > mHoldTime)
            mAttenuation = max(0.0f, mAttenuation - releaseRate);
      }

      // Test if disabled from the config window here so that the above state calculations
      // are still processed when playing around with the configuration
      if (mIsEnable) {
         // Multiple input by gate multiplier (0.0f if fully closed, 1.0f if fully open)
         buffer[i] *= mAttenuation;
         buffer[i + 1] *= mAttenuation;
      }
   }
}
void AudioNoiseGateFilter::LoadDefaults() {
   mIsEnable = false;
   mOpenThreshold = dbToRms(-26.0f);
   mCloseThreshold = dbToRms(-32.0f);
   mAttackTime = 0.025f;
   mHoldTime = 0.2f;
   mReleaseTime = 0.15f;
}

void AudioNoiseGateFilter::LoadSettings() {
   mIsEnable = mConfig.GetInt(TEXT("General"), TEXT("IsEnabled"), mIsEnable ? 1 : 0) ? true : false;
   mOpenThreshold = dbToRms((float)mConfig.GetInt(TEXT("General"), TEXT("OpenThreshold"), (int)rmsToDb(mOpenThreshold)));
   mCloseThreshold = dbToRms((float)mConfig.GetInt(TEXT("General"), TEXT("CloseThreshold"), (int)rmsToDb(mCloseThreshold)));
   mAttackTime = mConfig.GetFloat(TEXT("General"), TEXT("AttackTime"), mAttackTime);
   mHoldTime = mConfig.GetFloat(TEXT("General"), TEXT("HoldTime"), mHoldTime);
   mReleaseTime = mConfig.GetFloat(TEXT("General"), TEXT("ReleaseTime"), mReleaseTime);
}

void AudioNoiseGateFilter::SaveSettings() {
   mConfig.SetInt(TEXT("General"), TEXT("IsEnabled"), mIsEnable ? 1 : 0);
   mConfig.SetInt(TEXT("General"), TEXT("OpenThreshold"), (int)rmsToDb(mOpenThreshold));
   mConfig.SetInt(TEXT("General"), TEXT("CloseThreshold"), (int)rmsToDb(mCloseThreshold));
   mConfig.SetFloat(TEXT("General"), TEXT("AttackTime"), mAttackTime);
   mConfig.SetFloat(TEXT("General"), TEXT("HoldTime"), mHoldTime);
   mConfig.SetFloat(TEXT("General"), TEXT("ReleaseTime"), mReleaseTime);
}

void AudioNoiseGateFilter::SetEnableNoiseGate(bool enable) {
   this->mIsEnable = enable;
}

void AudioNoiseGateFilter::SetThresHoldValue(float closeThreshold, float openThreshold) {
   OSEnterMutex(mThresholdMutex);
   mCloseThreshold = dbToRms(closeThreshold);
   mOpenThreshold = dbToRms(openThreshold);
   OSLeaveMutex(mThresholdMutex);
}

void AudioNoiseGateFilter::Init(unsigned int sampleRate, bool enable, float closeThreshold, float openThreshold) {
   mSampleRateHz = sampleRate;
   SetThresHoldValue(closeThreshold, openThreshold);
   SetEnableNoiseGate(enable);
}
