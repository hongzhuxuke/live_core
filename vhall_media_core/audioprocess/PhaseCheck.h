/**************************************************************************/
/* VHALL Audio Phase Check                                                */
/* In a stereo audio data, left and right channel should keep their phase */
/* difference appropriate. A opposed phase stereo audio signal may cause  */
/* problem when played at a single speaker.                               */
/* This module could check the audio phase status. If mixing left and     */
/* right channel will cause a bad play, it will flip phase of one channel */
/* Created BY Neil Xia, 2019-05-30                                        */
/* Version 1.0.0                                                          */
/**************************************************************************/

#ifndef PHASE_CHECK_H
#define PHASE_CHECK_H

#include <stdint.h>

class PhaseCheck {
public:
   PhaseCheck() { mIsInited = false;  };
   ~PhaseCheck() {};
   /**************************************************************************/
   //Init()
   //Input:
   //    - sampleRate      : sample rate of input audio data
   //    - isPlannar       : 1 - LLLLLRRRRR, 0 - LRLRLRLRLR
   //Return                : 0 - Ok, -1 - error input parameter
   /**************************************************************************/
   int   Init(int sampleRate, bool isPlanar);

   /**************************************************************************/
   //Check()
   //Input:
   //    - data            : input data
   //    - num             : number of input samples / channels
   //Output:
   //    - data            : output data
   //Return                : 0 - Ok, -1 - error
   /**************************************************************************/
   int   Check(int16_t *data, int num);
   int   Check(float *data, int num);

   /**************************************************************************/
   //GetInPhaseRate()
   //Return                : In phase signal ratio
   /**************************************************************************/
   float GetInPhaseRatio();

   /**************************************************************************/
   //GetOpposedPhaseRate()
   //Return                : Opposed phase signal ratio
   /**************************************************************************/
   float GetOpposedPhaseRatio();

   /**************************************************************************/
   //GetSilenceRatio()
   //Return                : Weak Signal ratio
   /**************************************************************************/
   float GetSilenceRatio();

   /**************************************************************************/
   //ResetResult()
   //Return                : true - success reset all result, false - error
   /**************************************************************************/
   bool  ResetResult();
private:
   bool     mIsInited;
   bool     mIsPlanar;
   int      mBitsPerSample;
   int      mSampleRate;
   float    mInPhaseRatio;
   float    mOpposedPhaseRatio;
   float    mOpposedPhaseValidRatio;
   float    mSilenceRatio;
   uint64_t mTotalSamples;
   uint64_t mInPhaseSamples;
   uint64_t mOpposedPhaseSamples;
   uint64_t mSilenceSamples;

};

#endif // !PHASE_CHECK_H

