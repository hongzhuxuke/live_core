#ifndef __VINNY_X264_ENCODER_H__
#define __VINNY_X264_ENCODER_H__

#include <iostream>
#include <list>
#include <x264.h>
#include <atomic>
#include "avc_encode_interface.h"
#include "../common/live_define.h"
#include "encoder_info.h"
#include "../ratecontrol/rate_control_interface.h"

struct LivePushParam;
namespace VHJson {
   class Value;
}

enum ReconfigureType {
   Reconfig_KeepSame = 0,
   Reconfig_Init,
   Reconfig_SetBitrate,
   Reconfig_SceneCut,
   Reconfig_Process
};

enum QualityState {
   QualityState_Normal = 0,
   QualityState_Excellent,
   QualityState_Good,
   QualityState_Bad,
   QualityState_Terrible,
   QualityState_Reset
};

class X264Encoder :public AVCEncodeInterface, public EncoderInfo {
public:
   X264Encoder();
   ~X264Encoder();
private:
   void Destroy();
public:
   //virtual bool Init(LivePushParam * param);
   virtual bool Init(VideoEncParam * param); //independent of live push service
   virtual int Encode(const char * indata,
               int insize,
               char * outdata,
               int * p_out_size,
               int * p_frame_type,
               uint64_t in_ts,
               uint64_t *out_ts, VideoType type = VIDEO_TYPE_NORMAL);
   virtual bool GetSpsPps(char*data,int *size);
   virtual bool LiveGetRealTimeStatus(VHJson::Value &value);
   virtual bool SetVideoQuality(double grade);
   virtual bool RateControlAdjust(NetworkState state)override;
   /* 
   *  SetQualityLimited
   *  @description:
   *    turn on/off the encode quality limit. If quality is limited, just output a standard quality encoding frame when
   *    user or configuration files offer a big expected bitrate.
   *  @param:
   *    onoff [In] - turn on or off quality limit, true for on.
   *  @return:
   *    true - success
   *    false - failure
   */
   bool SetQualityLimited(bool onoff);

   /*
   *  SetAdjustBitrate
   *  @description:
   *    Set the bitrate variable mode. Target bitrate can be changed after encoder initialized in variable mode.
   *  @param:
   *    onoff [In] - turn on or off variable mode, true for on.
   *  @return:
   *    true - success
   *    false - failure
   */
   bool SetAdjustBitrate(bool onoff);

   /*
   *  GetResolution
   *  @description:
   *    Get the value of mResolutionLevel. NOTICE that mResolutionLevel may NOT be the same with true resolution.
   *  @param:
   *    void
   *  @return:
   *    the value of mResolutionLevel, 0 - error, 360 - 640x360, 480 - 640x480, 540 - 960x540, 720 - 1280x720, 
   *    768 - 1364x768, 1080 - 1920x1080, 2160 - 3840x2160
   */
   int GetResolution();

   /*
   *  GetBitrate
   *  @description:
   *    Get the value of mBitrate.
   *  @param:
   *    void
   *  @return:
   *    positive number - value of mBitrate, other - error
   */
   int GetBitrate();

   /*
   *  SetBitrate
   *  @description:
   *    Set the value of mBitrate. Bitrate variable mode should be turn on, otherwise the setting will not work.
   *    If setting success, new parameter will be used in next frame
   *  @param:
   *    bitrate [In] - new expected bitrate.
   *  @return:
   *    true - success
   *    false - failure
   */
   bool SetBitrate(int bitrate);

   /*
   *  RequestKeyframe
   *  @description:
   *    Request a new keyframe.
   *  @param:
   *    void
   *  @return:
   *    true - success
   *    false - failure
   */
   virtual bool RequestKeyframe();
   virtual bool GetRecentlyDecodeYUVData(char **data, int* size);

private:
   int RateControlAdjustCore();
   //bool RateControlConfig();
   bool RateControlConfig2();
   bool BitrateClassify(int &bitrate);
   bool ResClassify();
   bool BitrateNormalize();
   //bool SetBitrate2(int bitrate);
   bool DiffMbCheck(x264_picture_t *pic);

private:
   //x264
   x264_param_t mX264ParamData;
   x264_t *mX264Encoder;
   x264_picture_t mX264PicOut;
   x264_picture_t mX264PicIn;
   bool mIsPadCBR;
   bool mIsUseCFR;
   bool mIsUseCBR;
   
   NaluUnit mSEINaluUnit;
   NaluUnit mHeaderUnit;
   LivePushParam        *mLiveParam;
   VideoEncParam        *mEncParam;

   std::list<uint64_t>   mFrameTimestampQueue;
   
   //unsigned char *mYuvBuffer;
   //unsigned char *mPicInBuffer;
   int mOriginWidth;
   int mOriginHeight;
   int mFrameRate;
   int mWidthMb;
   int mHeightMb;
   
   int mFrameCount;
   int mFrameFaildCount;
   uint32_t mFrameNum;

   std::atomic_int mBitrate;
   int mBitrateMax;
   int mBitrateMin;
   int mOriginBitrate;
   float mKeyframeInterval;
   std::string mProfile;
   std::string mPreset;
   std::list<NaluUnit*>  mNalUnits;

   bool mIsInited;
   bool mIsAdjustBitrate;//Rate Control based on Network Feedback
   bool mIsQualityLimited;//High quality encode permission, high bit rate value will be ceiled if true
   bool mIsQualityProtect;//Low bit rate encode permission, low bit rate value will be floored if true
   int  mQPLimitedLevel;//Flag of the max and min of QP limit
   int  mHighCodecLevel;
   bool mIsRequestKeyframe;
   ReconfigureType mReconfigType;
   VideoType mSceneType;
   int mResolutionLevel;
   int mCurrentProcessFlag;
   float mCrfDelta;

   char *mReconFrame;
   char *mRefFrame;
   char *mDiffFrame;

   QualityState mQualityState;
   NetworkState mNetworkState;

   //for debug
   FILE *mFileH264;
   FILE *mFileEncodeYUV;
   FILE *mFileFrameInfo;
   FILE *mFileLog;

   //for ratecontrol

};

#endif
