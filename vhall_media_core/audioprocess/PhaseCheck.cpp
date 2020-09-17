#include "PhaseCheck.h"

#define  CHECK_STARTUP_NUM (mSampleRate * 0.5)
#define  SILENCE_THRESHOLD_SHORT    492
#define  SILENCE_THRESHOLD_FLOAT    0.016
#define  PHASE_TH                   0.7
int PhaseCheck::Init(int sampleRate, bool isPlanar)
{
   mIsInited = false;
   if (sampleRate < 8000 || sampleRate > 48000) {
      return -1;
   }
   mSampleRate = sampleRate;
   mIsPlanar = isPlanar;
   mTotalSamples = 0;
   mInPhaseSamples = 0;
   mOpposedPhaseSamples = 0;
   mSilenceSamples = 0;
   mInPhaseRatio = 0;
   mOpposedPhaseRatio = 0;
   mOpposedPhaseValidRatio = 0;
   mSilenceRatio = 0;
   mIsInited = true;
   return 0;
}

int PhaseCheck::Check(int16_t * data, int num)
{
   if (!mIsInited || num%2) {
      return -1;
   }
   short l, r;
   short *buffer = data;
   int half = num / 2;
   for (int i = 0; i < half; i++) {
      if (mIsPlanar) {
         l = buffer[0];
         r = buffer[half];
         if (mOpposedPhaseValidRatio > PHASE_TH) {
            buffer[0] = -buffer[0];
         }
         buffer++;
      }
      else {
         l = buffer[0];
         r = buffer[1];
         if (mOpposedPhaseValidRatio > PHASE_TH) {
            buffer[0] = -buffer[0];
         }
         buffer += 2;
      }
      //weak signal
      if (l < SILENCE_THRESHOLD_SHORT && l > -SILENCE_THRESHOLD_SHORT && r < SILENCE_THRESHOLD_SHORT && r > -SILENCE_THRESHOLD_SHORT) { 
         mSilenceSamples++;
      }
      // in phase
      else if ((l > 0 && r > 0) || (l < 0 && r < 0)) {
         mInPhaseSamples++;
      } 
      //opposed phase
      else {
         mOpposedPhaseSamples++;
      }
      mTotalSamples++;
   }
   mSilenceRatio = (float)mSilenceSamples / mTotalSamples;
   mInPhaseRatio = (float)mInPhaseSamples / mTotalSamples;
   mOpposedPhaseRatio = (float)mOpposedPhaseSamples / mTotalSamples;
   if (mInPhaseSamples + mOpposedPhaseSamples > CHECK_STARTUP_NUM) {
      mOpposedPhaseValidRatio = mOpposedPhaseRatio / (mOpposedPhaseRatio + mInPhaseRatio);
   }
   else {
      mOpposedPhaseValidRatio = 0;
   }
   return 0;
}

int PhaseCheck::Check(float * data, int num)
{
   if (!mIsInited || num % 2) {
      return -1;
   }
   float l, r;
   float *buffer = data;
   int half = num / 2;
   for (int i = 0; i < half; i++) {
      if (mIsPlanar) {
         l = buffer[0];
         r = buffer[half];
         if (mOpposedPhaseValidRatio > PHASE_TH) {
            buffer[0] = -buffer[0];
         }
         buffer++;
      }
      else {
         l = buffer[0];
         r = buffer[1];
         if (mOpposedPhaseValidRatio > PHASE_TH) {
            buffer[0] = -buffer[0];
         }
         buffer += 2;
      }
      //weak signal
      if (l < SILENCE_THRESHOLD_FLOAT && l > -SILENCE_THRESHOLD_FLOAT && r < SILENCE_THRESHOLD_FLOAT && r > -SILENCE_THRESHOLD_FLOAT) {
         mSilenceSamples++;
      }
      // in phase
      else if ((l > 0 && r > 0) || (l < 0 && r < 0)) {
         mInPhaseSamples++;
      }
      //opposed phase
      else {
         mOpposedPhaseSamples++;
      }
      mTotalSamples++;
   }
   mSilenceRatio = (float)mSilenceSamples / mTotalSamples;
   mInPhaseRatio = (float)mInPhaseSamples / mTotalSamples;
   mOpposedPhaseRatio = (float)mOpposedPhaseSamples / mTotalSamples;
   if (mInPhaseSamples + mOpposedPhaseSamples > CHECK_STARTUP_NUM) {
      mOpposedPhaseValidRatio = mOpposedPhaseRatio / (mOpposedPhaseRatio + mInPhaseRatio);
   }
   else {
      mOpposedPhaseValidRatio = 0;
   }
   return 0;
}

float PhaseCheck::GetInPhaseRatio()
{
   return mInPhaseRatio;
}

float PhaseCheck::GetOpposedPhaseRatio()
{
   return mOpposedPhaseRatio;
}

float PhaseCheck::GetSilenceRatio()
{
   return mSilenceRatio;
}

bool PhaseCheck::ResetResult()
{
   mTotalSamples = 0;
   mInPhaseSamples = 0;
   mOpposedPhaseSamples = 0;
   mSilenceSamples = 0;
   return true;
}
