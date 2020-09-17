#include "x264_encoder.h"
#include "../common/vhall_log.h"
#include "../utility/utility.h"
#include <string.h>
#include <stdlib.h>
#include "../common/live_sys.h"
#include "../3rdparty/json/json.h"

#define  MAX_ABR_BITRATE_RATIO   1.5
#define  HIGH_CODEC_MAX          9
#define  VIDEO_QUALITY_LEVEL_NUM 3
#define  RESLOUTION_LEVEL_NUM    10

#define  PUSH_BUFFER_FILL_HIGH   0.8
#define  PUSH_BUFFER_FILL_LOW    0.1
#define  BUFFER_DELAY_HIGH       1.5
#define  BUFFER_DELAY_LOW        0.2

//#define OLDVERSION


enum ColorPrimaries {
   ColorPrimaries_BT709 = 1,
   ColorPrimaries_Unspecified,
   ColorPrimaries_BT470M = 4,
   ColorPrimaries_BT470BG,
   ColorPrimaries_SMPTE170M,
   ColorPrimaries_SMPTE240M,
   ColorPrimaries_Film,
   ColorPrimaries_BT2020
};

enum ColorTransfer {
   ColorTransfer_BT709 = 1,
   ColorTransfer_Unspecified,
   ColorTransfer_BT470M = 4,
   ColorTransfer_BT470BG,
   ColorTransfer_SMPTE170M,
   ColorTransfer_SMPTE240M,
   ColorTransfer_Linear,
   ColorTransfer_Log100,
   ColorTransfer_Log316,
   ColorTransfer_IEC6196624,
   ColorTransfer_BT1361,
   ColorTransfer_IEC6196621,
   ColorTransfer_BT202010,
   ColorTransfer_BT202012
};

enum ColorMatrix {
   ColorMatrix_GBR = 0,
   ColorMatrix_BT709,
   ColorMatrix_Unspecified,
   ColorMatrix_BT470M = 4,
   ColorMatrix_BT470BG,
   ColorMatrix_SMPTE170M,
   ColorMatrix_SMPTE240M,
   ColorMatrix_YCgCo,
   ColorMatrix_BT2020NCL,
   ColorMatrix_BT2020CL
};

enum VideoResolution {
   VideoResolution_Error = 0,
   VideoResolution_120p = 120,
   VideoResolution_180p = 180,
   VideoResolution_240p = 240,
   VideoResolution_360p = 360,
   VideoResolution_480p = 480,
   VideoResolution_540p = 540,
   VideoResolution_720p = 720,
   VideoResolution_768p = 768,
   VideoResolution_1080p = 1080,
   VideoResolution_2160p = 2160
};

int DefaultVideoBitrate[RESLOUTION_LEVEL_NUM] = {
   200, 290, 350, 500, 850, 1000, 1500, 1500, 2000, 4000
};

double VideoQualityLevelTable[HIGH_CODEC_MAX + 1][VIDEO_QUALITY_LEVEL_NUM] ={
   {65.0, 75.0, 85.0},{65.0, 75.0, 85.0},{66.0, 76.0, 86.0},
   {66.0, 76.0, 86.0},{67.0, 77.0, 87.0},{67.0, 77.0, 87.0},
   {68.0, 78.0, 88.0},{68.0, 78.0, 88.0},{69.0, 79.0, 89.0},
   {70.0, 80.0, 90.0}
};

void get_x264_log(void *param, int i_level, const char *psz, va_list argptr);

NaluUnit* MallocNalu(const int& naluSize){
   NaluUnit* newNaluUnit = NULL;
   newNaluUnit = (NaluUnit*)calloc(1, sizeof(NaluUnit));
   if (newNaluUnit == NULL){
      LOGE("MallocNalu malloc newNaluUnit failed. ");
      return NULL;
   }
   newNaluUnit->size = naluSize;
   newNaluUnit->data = (unsigned char*)malloc(naluSize);
   if (newNaluUnit->data == NULL){
      LOGE("MallocNalu malloc newNaluUnit data failed. ");
      free(newNaluUnit);
      return NULL;
   }
   return newNaluUnit;
}

X264Encoder::X264Encoder():
mX264Encoder(nullptr),
mIsPadCBR(false),
mIsUseCFR(true),
mKeyframeInterval(2.0),
mIsUseCBR(false),
mReconFrame(nullptr),
mRefFrame(nullptr),
mDiffFrame(nullptr),
mFileH264(nullptr),
mFileEncodeYUV(nullptr),
mFileFrameInfo(nullptr),
mFileLog(nullptr),
//mYuvBuffer(NULL),
//mPicInBuffer(NULL),
mIsInited(false),
mReconfigType(Reconfig_KeepSame),
mSceneType(VIDEO_TYPE_NORMAL)
{
   mFrameCount = 0;
   mFrameFaildCount = 0;
   memset(&mSEINaluUnit, 0, sizeof(NaluUnit));
   memset(&mHeaderUnit, 0, sizeof(NaluUnit));
}

X264Encoder::~X264Encoder() {
   this->Destroy();
}

void X264Encoder::Destroy() {
   mIsInited = false;
   LOGI("X264Encoder::destroy.");
   if(mX264Encoder){
      x264_encoder_close(mX264Encoder);
      mX264Encoder = NULL;
      x264_picture_clean(&mX264PicIn);
   }
   //VHALL_DEL(mYuvBuffer);
   //VHALL_DEL(mPicInBuffer);
   VHALL_FILE_DEL(mFileH264);
   VHALL_FILE_DEL(mFileEncodeYUV);
   VHALL_FILE_DEL(mFileFrameInfo);
   VHALL_FILE_DEL(mFileLog);
   VHALL_DEL_ARRAY(mReconFrame);
   VHALL_DEL_ARRAY(mRefFrame);
   VHALL_DEL_ARRAY(mDiffFrame);
}

bool X264Encoder::Init(VideoEncParam * param) {
    mIsInited = false;
    mEncParam = param;
    int ret = -1;
    this->Destroy();
    LOGI("X264Encoder::Init.");
    //mBitrate = param->bit_rate/1000;
    mOriginWidth = param->width;
    mOriginHeight = param->height;
    mFrameRate = param->frame_rate;
    mWidthMb = (param->width + 15) / 16;
    mHeightMb = (param->height + 15) / 16;
    mFrameNum = 0;
    mCrfDelta = 0;

    if (param->gop_interval > 0) {
        mKeyframeInterval = param->gop_interval;
    }
    else {
       mKeyframeInterval =  2;//default gop is 2s
    }

    mSceneType = VIDEO_TYPE_NORMAL;//default scene type is natural

    mHighCodecLevel = param->high_codec_open;
    mIsQualityLimited = param->is_quality_limited;
    mIsQualityProtect = param->is_quality_protect;
    mIsAdjustBitrate = param->is_adjust_bitrate;
    mIsRequestKeyframe = false;

    if (mHighCodecLevel < 0 || mHighCodecLevel > 9) {
       mHighCodecLevel = 0;
       LOGW("x264_encoder:high codec open value is invalid, set 0");
    }

    //Invalid situation: 1. Height or/and Width is not a even number; 2. Height or/and Width is negative
    if (mOriginHeight & 1 || mOriginWidth & 1 || mOriginHeight < 0 || mOriginWidth < 0) {
        mResolutionLevel = VideoResolution_Error;
        LOGE("x264_encoder : Encoder get an invalid resolution parameter.");
        return false;
    }
    mOriginBitrate = param->bit_rate / 1000;

    //bitrate setting
    if (!ResClassify()) {
        LOGE("x264_encoder : Frame size is too large!");
        return false;
    }
    if (!BitrateNormalize()) {
        LOGE("x264_encoder : Input bitrate or high codec value invalid.");
        return false;
    }
    
    mX264ParamData.rc.i_qp_max = 38 - mHighCodecLevel / 4; //36 - 38
    mX264ParamData.rc.i_qp_min = 18 - mHighCodecLevel / 4; //16 - 18

    memset(&mX264ParamData, 0, sizeof(mX264ParamData));
    mProfile = "high";
    mPreset = "superfast";
    //const char*tune = x264_tune_names[7];
    const char*tune = "zerolatency";
    if (x264_param_default_preset(&mX264ParamData, mPreset.c_str(), tune)) {
        LOGE("Failed to set mX264Encoder defaults: %s/%s", mPreset.c_str(), " ");
        return false;
    }
    //vba
    mX264ParamData.b_repeat_headers = 0; //I 有pps sps
    //http://blog.csdn.net/lzx995583945/article/details/43446259
    //mX264ParamData.b_deterministic = false;
    //mX264ParamData.rc.i_vbv_max_bitrate = (int)(mBitrate * MAX_ABR_BITRATE_RATIO); //vbv-maxrate
    // mX264ParamData.rc.i_vbv_buffer_size = mBitrate ; //vbv-bufsize

    LOGI("X264Encoder::Init. bitrate=%d", (int)mBitrate);

    if (mIsUseCBR) {
        if (mIsPadCBR)
            mX264ParamData.rc.b_filler = 1;
        mX264ParamData.rc.i_bitrate = mBitrate;
        mX264ParamData.rc.i_rc_method = X264_RC_ABR;
        mX264ParamData.rc.f_rf_constant = 0.0f;
        mX264ParamData.rc.f_rate_tolerance = MAX_ABR_BITRATE_RATIO;
    }
    else {
        mReconfigType = Reconfig_Init;
        mX264ParamData.rc.i_rc_method = X264_RC_CRF;

        if (!RateControlConfig2()) {
            LOGE("Failed to initialize the x264 encoder rate control parameter.");
            return false;
        }
    }

    mX264ParamData.b_vfr_input = !mIsUseCFR;
    mX264ParamData.vui.b_fullrange = 0;
    mX264ParamData.vui.i_colorprim = ColorPrimaries_BT709;
    mX264ParamData.vui.i_transfer = ColorTransfer_IEC6196621;
    mX264ParamData.vui.i_colmatrix =
        mOriginWidth >= 1280 || mOriginHeight > 576 ? ColorMatrix_BT709 : ColorMatrix_SMPTE170M;
    mX264ParamData.i_bframe = 0;//no b-frame
    mX264ParamData.rc.i_lookahead = 0;// no lookahead
    mX264ParamData.i_keyint_max = mFrameRate * mKeyframeInterval;
    mX264ParamData.i_keyint_min = mFrameRate * mKeyframeInterval;
    mX264ParamData.i_fps_num = mFrameRate;
    mX264ParamData.i_fps_den = 1;
    //mX264ParamData.i_timebase_num = 1;
    //mX264ParamData.i_timebase_den = 1000;

    /* Log */
    mX264ParamData.pf_log = get_x264_log;
    VHALL_FILE_DEL(mFileLog);
    if (param->is_encoder_debug) {

        mFileLog = fopen("x264_1pass.log", "wb");
        mX264ParamData.i_log_level = X264_LOG_INFO;
        mX264ParamData.analyse.b_psnr = 1;
        mX264ParamData.b_full_recon = 1;
    }
    else {
        mX264ParamData.i_log_level = X264_LOG_ERROR;
    }
    mX264ParamData.p_log_private = mFileLog;

    mX264ParamData.b_cabac = 1;
    mX264ParamData.i_scenecut_threshold = 0;
    mX264ParamData.rc.i_aq_mode = 0;
    mX264ParamData.b_sliced_threads = 0;

    /* video process */
    mCurrentProcessFlag = param->video_process_filters;
    if (mCurrentProcessFlag & VIDEO_PROCESS_DIFFCHECK) {
        mX264ParamData.b_motion_ref = 1;
        mX264ParamData.i_frame_reference = 1;
        mX264ParamData.i_threads = 1;
    }
    else {
        mX264ParamData.b_motion_ref = 0;
    }
    //TODO: other video process function


    if (strcmp(mProfile.c_str(), "main") == 0)
        mX264ParamData.i_level_idc = 41; // to ensure compatibility with portable devices

    if (param->pixel_format == PIX_FMT_YUV420SP_NV21) {
        mX264ParamData.i_csp = X264_CSP_NV21;
    }
    else if (param->pixel_format == PIX_FMT_YUV420SP_NV12) {
        mX264ParamData.i_csp = X264_CSP_NV12;
    }
    else if (param->pixel_format == PIX_FMT_YUV420P_I420) {
        mX264ParamData.i_csp = X264_CSP_I420;
    }
    else if (param->pixel_format == PIX_FMT_YUV420P_YV12) {
        mX264ParamData.i_csp = X264_CSP_YV12;
    }
    else {
        mX264ParamData.i_csp = X264_CSP_NV21;
    }

    x264_picture_init(&mX264PicIn);
    mX264ParamData.i_width = mOriginWidth;
    mX264ParamData.i_height = mOriginHeight;

    if (x264_param_apply_profile(&mX264ParamData, mProfile.c_str())) {
        LOGE("Failed to x264_param_apply_profile profile=%s", mProfile.c_str());
        return false;
    }

    ret = x264_picture_alloc(&mX264PicIn, mX264ParamData.i_csp, mX264ParamData.i_width, mX264ParamData.i_height);
    if (ret < 0) {
        LOGE("Failed to x264_picture_alloc %dx%d", mOriginWidth, mOriginHeight);
        return false;
    }
    if (mX264ParamData.b_motion_ref) {
        VHALL_DEL(mX264PicIn.motion_status);
        mX264PicIn.motion_status = new uint8_t[mWidthMb * mHeightMb];
        memset(mX264PicIn.motion_status, 0xff, mWidthMb * mHeightMb);
    }

    mX264Encoder = x264_encoder_open(&mX264ParamData);
    if (!mX264Encoder) {
        LOGE("Failed to open X264Encoder profile=%s", mProfile.c_str());
        return false;
    }

    //   mYuvBuffer = (unsigned char*)calloc(mOriginWidth * mOriginHeight * 3 / 2, 1);
    //   mPicInBuffer = (unsigned char*)calloc(mOriginWidth * mOriginHeight * 3 / 2, 1);
    //   if(mYuvBuffer == NULL /*|| mPicInBuffer == NULL*/){
    //      LOGE("calloc yuv/picin buffer failed.");
    //      return false;
    //   }

    VHALL_DEL_ARRAY(mReconFrame);
    mReconFrame = new char[mOriginWidth * mOriginHeight * 3 / 2];
    VHALL_DEL_ARRAY(mRefFrame);
    mRefFrame = new char[mOriginWidth * mOriginHeight * 3 / 2];
    VHALL_DEL_ARRAY(mDiffFrame);
    mDiffFrame = new char[mOriginWidth * mOriginHeight];
    mFrameCount = 0;
    mFrameFaildCount = 0;

    //for rate control
    mQualityState = QualityState_Normal;
    mNetworkState = NetworkState_Normal;

    //for debug
    if (param->is_encoder_debug && mFileH264 == NULL) {
        mFileH264 = fopen("H264Stream.264", "wb");
    }
    if (param->is_saving_data_debug) {
        if (mFileEncodeYUV == NULL) {
            mFileEncodeYUV = fopen("EncodeData.yuv", "wb");
        }
        if (mFileFrameInfo == NULL) {
            mFileFrameInfo = fopen("FrameInfo.txt", "wb");
            fprintf(mFileFrameInfo, "Scenetype:0 - Unknown; 1 - Natural; 2 - Artificial\n");
        }
    }
    mIsInited = true;
    return true;
}

//NV21->I420
int X264Encoder::Encode(const char * indata,
                        int insize,
                        char *outdata,
                        int *p_out_size,
                        int *p_frame_type,
                        uint64_t in_ts,
                        uint64_t *out_ts,
                        VideoType type)
{
   int frameSize = 0;
   x264_nal_t *nalOut;
   int nalOutNum = 0;
   *p_frame_type = VIDEO_P_FRAME;
   if(mX264Encoder == NULL){
      LOGW("X264Encoder::Encode x264encoder not init.");
      return -1;
   }
   memcpy(mX264PicIn.img.plane[0], indata, insize);
   
   VideoType newSceneType = VIDEO_TYPE_NORMAL;
   VideoType oldSceneType = VIDEO_TYPE_NORMAL;
   int luma_size = mOriginWidth * mOriginHeight;
   int chroma_size = luma_size / 4;
   /* video process */
   if (mX264ParamData.b_motion_ref) {
      DiffMbCheck(&mX264PicIn);
   }


   /*if (extendParam == NULL){
      mX264PicIn.i_scene_type = SceneType_Natural;
      mX264PicIn.b_is_same_last = 0;
   }
   else{
      mX264PicIn.i_scene_type = extendParam->scene_type;
      mX264PicIn.b_is_same_last = extendParam->same_last;
   }
   int ret = vhall_video_preprocess_process(mX264Encoder, &mX264PicIn);
   if (ret<0){
      LOGE("video preprocess failed! %d",ret);
   }*/
   /* debug */
   if (mEncParam->is_saving_data_debug){
      fwrite(mX264PicIn.img.plane[0], luma_size + chroma_size * 2, 1, mFileEncodeYUV);
      /*fprintf(mFileFrameInfo, "Scenetype of frame [%d] is [%d]\n", mFrameNum, mX264PicIn.i_scene_type);
      if (mX264PicIn.b_is_same_last == 1){
         fprintf(mFileFrameInfo, "and it is SAME AS LAST FRAME\n");
      }
      mFrameNum++;*/
   }
   //check if need to reconfigure
   if (type != mSceneType) {
      mReconfigType = Reconfig_SceneCut;
      oldSceneType = mSceneType;
      mSceneType = type;
   }
   /*if (mX264PicIn.i_scene_type == SceneType_Unknown){
      newSceneType = SceneType_Natural;
   }
   else{
      newSceneType = mX264PicIn.i_scene_type;
   }
   
   if (newSceneType != mSceneType){
      mReconfigType = Reconfig_SceneCut;
      oldSceneType = mSceneType;
      mSceneType = newSceneType;
   }
   if (mCurrentProcessFlag != mLiveParam->video_process_filters){
      mReconfigType = Reconfig_Process;
      mCurrentProcessFlag = mLiveParam->video_process_filters;
   }*/

   if (mReconfigType){
      if (!RateControlConfig2()){
         LOGE("x264 reconfig failed. ");
         if (mReconfigType == Reconfig_SceneCut){
            mSceneType = oldSceneType;
         }
      }
      int Br = mBitrate;
      LOGD("[X264Encoder]bit rate is reconfigured to %d", Br);
   }
 
   //check if have a key-frame request
   if (mIsRequestKeyframe){
      mX264PicIn.i_type = X264_TYPE_IDR;
      mIsRequestKeyframe = false;
   }
   else{
      mX264PicIn.i_type = X264_TYPE_AUTO;
   }
   mX264PicIn.i_pts = in_ts;
   frameSize = x264_encoder_encode(mX264Encoder, &nalOut, &nalOutNum, &mX264PicIn, &mX264PicOut );

   if (frameSize < 0) {
      LOGE("x264_encoder_encode failed. ");
      mFrameFaildCount++;
      return -1;
   }
   
   if (mEncParam->is_encoder_debug && frameSize > 0){
      fwrite(nalOut->p_payload, frameSize, 1, mFileH264);
   }
   //编码成功，时间戳入堆栈
   mFrameTimestampQueue.push_back(in_ts);
   if(nalOutNum < 0){
      LOGE("no frame, this frame is cached. ");
      return 0;
   }
   //if frame duplication is being used, the shift will be insignificant, so just don't bother adjusting audio
   if(frameSize > 0 &&mFrameTimestampQueue.size()>0){
      if (mX264PicOut.i_type == X264_TYPE_KEYFRAME||mX264PicOut.i_type == X264_TYPE_I||mX264PicOut.i_type == X264_TYPE_IDR) {
         *p_frame_type = VIDEO_I_FRAME;
      }else if(mX264PicOut.i_type == X264_TYPE_P||mX264PicOut.i_type == X264_TYPE_BREF){
         *p_frame_type = VIDEO_P_FRAME;
      }else{
         *p_frame_type = VIDEO_B_FRAME;
      }
      memcpy(outdata, nalOut->p_payload, frameSize);
      *p_out_size = frameSize;
      *out_ts = *mFrameTimestampQueue.begin();
      mFrameTimestampQueue.pop_front();
      //printf("frmae %d is encoded\n",mFrameCount);
      mFrameCount++;
      return frameSize;
   }else {
       LOGW("no frame,this frame is cached");
      return 0;
   }
}

bool X264Encoder::LiveGetRealTimeStatus(VHJson::Value &value){
   value["Name"] = VHJson::Value("X264Encoder");
   
   //TODO may need to make it thread safe.
   value["width"] = VHJson::Value(mOriginWidth);
   value["height"] = VHJson::Value(mOriginHeight);
   value["frame_rate"] = VHJson::Value(mFrameRate);
   value["bitrate"] = VHJson::Value(mBitrate);
   value["gop_size"] = VHJson::Value(mKeyframeInterval);
   value["profile"] = VHJson::Value(mProfile);
   value["preset"] = VHJson::Value(mPreset);
   
   value["frame_success_count"] = VHJson::Value(mFrameCount);
   value["frame_faild_count"] = VHJson::Value(mFrameFaildCount);
   
   return true;
}

bool X264Encoder::SetVideoQuality(double grade)
{
   double *scoreLevel;
   scoreLevel = VideoQualityLevelTable[mHighCodecLevel];

   if (grade < -2) {
      mQualityState = QualityState_Reset;
   }
   else if (grade < 0.0 || grade > 100.0) {
      mQualityState = QualityState_Normal;
   }
   else if (grade - scoreLevel[2] > 5) {
      mQualityState = QualityState_Excellent;
      LOGW("[X264Enc]Vidao quality is overcapacity!");
   }
   else if (grade > scoreLevel[2]) {
      mQualityState = QualityState_Good;
      LOGW("[X264Enc]Vidao quality is very good!");
   }
   else if (grade < scoreLevel[0]) {
      mQualityState = QualityState_Terrible;
      LOGW("[X264Enc]Vidao quality is terrible!");
   }
   else if (grade < scoreLevel[1]) {
      mQualityState = QualityState_Bad;
      LOGW("[X264Enc]Vidao quality is bad!");
   }
   else {
      mQualityState = QualityState_Normal;
      LOGI("[X264Enc]Vidao quality is normal!");
   }
   RateControlAdjustCore();
   return true;
}

bool X264Encoder::RateControlAdjust(NetworkState state)
{
   if (mIsInited == false) {
      return false;
   }
   mNetworkState = state;
   int ret = 0;
   if ((ret = RateControlAdjustCore()) > 0) {
      LOGI("RateControl bitrate changed to %d", ret);
      return true;
   }
   return false;
}

int X264Encoder::RateControlAdjustCore()
{
   if (mIsInited == false) {
      return -1;
   }
   int bitrate = mBitrate;
   //reset decide
   if (mNetworkState == NetworkState_Reset) {
      SetBitrate(mOriginBitrate);
      mNetworkState = NetworkState_Normal;
      mCrfDelta = 0;
      return mBitrate;
   }
   if (mQualityState == QualityState_Reset && (mNetworkState == NetworkState_Block || mNetworkState == NetworkState_Overuse) && mOriginBitrate >= mBitrate) {
      mQualityState = QualityState_Normal;
   }
   else if (mQualityState == QualityState_Reset) {
      SetBitrate(mOriginBitrate);
      mQualityState = QualityState_Normal;
      mCrfDelta = 0;
      return mBitrate;
   }

   //adjust decide
   switch (mNetworkState) {
   case NetworkState_Block:
      bitrate *= 0.7;
      mCrfDelta += 2;
      LOGW("[X264Enc]Network is blocked, bitrate must be lower!");
      break;
   case NetworkState_Overuse:
      bitrate *= 0.8;
      mCrfDelta += 1;
      LOGW("[X264Enc]Network is overused, bit rate must be lower!");
      break;
   default:
      switch (mQualityState) {
      case QualityState_Terrible:
         bitrate *= 1.5;
         mCrfDelta -= 3;
         LOGW("[X264Enc]more bit rate needed for video qualtiy!!");
         break;
      case QualityState_Bad:
         bitrate *= 1.3;
         mCrfDelta -= 1;
         LOGW("[X264Enc]more bit rate needed for video qualtiy!");
         break;
      case QualityState_Good:
         bitrate *= 0.9;
         mCrfDelta += 0.5;
         LOGI("[X264Enc]video quality is good enough, bit rate could be lower!");
         break;
      case QualityState_Excellent:
         bitrate *= 0.8;
         mCrfDelta += 1;
         LOGI("[X264Enc]video quality is good enough, bit rate could be lower!!");
         break;
      default:
         if (mNetworkState == NetworkState_Underuse) {
            if (bitrate <= mOriginBitrate * 0.9) {
               bitrate *= 1.1;
               mCrfDelta -= 0.3;
            }
            else {
               bitrate = mOriginBitrate;
               mCrfDelta = 0;
            }
            LOGI("[X264Enc]Network is underused, bit rate could be higher!");
         }
      }
   }
   
   //keep crf value in a reasonable interval
   if (mCrfDelta < (-4 - mHighCodecLevel / 3)) {
      mCrfDelta = -4 - mHighCodecLevel / 3;
   }
   else if (mCrfDelta > 6) {
      mCrfDelta = 6;
   }

   //set new bitrate and wait for reconfig
   if (SetBitrate(bitrate)) {
      return mBitrate;
   }
   return -2;
}

bool X264Encoder::GetSpsPps(char*data,int *size){
   if (data!=NULL&&mX264Encoder!=NULL) {
      x264_nal_t *nalOut;
      int lenght = 0;
      int nalOutNum = 0;
      int ret = x264_encoder_headers(mX264Encoder, &nalOut, &nalOutNum);
      if (ret>0) {
         for (int i = 0; i < nalOutNum; ++i)
         {
            switch (nalOut[i].i_type)
            {
               case NAL_SPS:
                  memcpy(data, nalOut[i].p_payload, nalOut[i].i_payload);
                  lenght += nalOut[i].i_payload;
                  break;
               case NAL_PPS:
                  memcpy(data+lenght, nalOut[i].p_payload, nalOut[i].i_payload);
                  lenght += nalOut[i].i_payload;
                  break;
               default:
                  break;
            }
         }
         *size = lenght;
         if (mFileH264 != NULL){
            fwrite(data, *size, 1, mFileH264);
         }
      }
      return true;
   }
   return false;
}

void get_x264_log(void *param, int i_level, const char *psz, va_list argptr) {
   char buffer[4096];
   char *psz_prefix;
   switch( i_level )
   {
      case X264_LOG_ERROR:
         psz_prefix = (char *)"error";
         break;
      case X264_LOG_WARNING:
         psz_prefix = (char *)"warning";
         break;
      case X264_LOG_INFO:
         psz_prefix = (char *)"info";
         break;
      case X264_LOG_DEBUG:
         psz_prefix = (char *)"debug";
         break;
      default:
         psz_prefix = (char *)"unknown";
         break;
   }
   fprintf( stderr, "x264 [%s]: ", psz_prefix );
   vfprintf( stderr, psz, argptr );
   vsprintf( buffer, psz, argptr );
   LOGW("x264 [%s]: %s", psz_prefix,  buffer);
   if (param) {
      fprintf((FILE *)param, "x264 [%s]: ", psz_prefix);
      vfprintf((FILE *)param, psz, argptr);
   }
}

//functions for rate control based on network
int X264Encoder::GetResolution(){
   
   if (!mIsInited ){
      return VideoResolution_Error;
   }
   return mResolutionLevel;
   
}

int X264Encoder::GetBitrate(){
   //Invalid situation : 1.have not initialized yet;2. bitrate isn't positive
   if (!mIsInited || mBitrate <= 0){
      return 0;
   }
   return mBitrate;
}

//bool X264Encoder::SetBitrate(int bitrate){
//   if (mReconfigType && !mIsInited){
//      LOGW("x264_encoder : Encoder is reconfiguring or not initialized! Bitrate set failed!");
//      return false;
//   }
//   if (!mIsAdjustBitrate){
//      LOGW("x264_encoder : Bitrate adjestment is turn off! Bitrate set failed!");
//      return false;
//   }
//   if (bitrate <= 0){
//      LOGE("x264_encoder : Can't set a negative bitrate!");
//      return false;
//   }
//   if (bitrate == mBitrate){
//      return true;
//   }
//   //If input value is unreliable, classify it.
//   BitrateClassify(bitrate);
//   mReconfigType = Reconfig_SetBitrate;
//   return true;
//}
//
//bool X264Encoder::RateControlConfig(){
//   if (!mReconfigType){
//      return true;
//   }
//   if (mSceneType == SceneType_Natural){
//      switch (mResolutionLevel){
//         case VideoResolution_360p:
//            switch ((int)mBitrate){
//               case 100:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 150:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 250:
//                  mX264ParamData.rc.f_rf_constant = 26;
//                  break;
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 24;
//                  break;
//               case 425:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 22;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 26;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate ;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.2;
//            break;
//         case VideoResolution_480p:
//            switch ((int)mBitrate){
//               case 150:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 300:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 400:
//                  mX264ParamData.rc.f_rf_constant = 26;
//                  break;
//               case 525:
//                  mX264ParamData.rc.f_rf_constant = 24;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 22;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 26;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.2;
//            break;
//         case VideoResolution_540p:
//            switch ((int)mBitrate){
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 300:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 400:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 26;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 24;
//                  break;
//               case 850:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 1100:
//                  mX264ParamData.rc.f_rf_constant = 22;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 26;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.2;
//            break;
//         case VideoResolution_720p:
//            switch ((int)mBitrate){
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 27;
//                  break;
//               case 1000:
//                  mX264ParamData.rc.f_rf_constant = 25;
//                  break;
//               case 1400:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 2000:
//                  mX264ParamData.rc.f_rf_constant = 21;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 27;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.3;
//            break;
//         case VideoResolution_768p:
//            switch ((int)mBitrate){
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 27;
//                  break;
//               case 1100:
//                  mX264ParamData.rc.f_rf_constant = 25;
//                  break;
//               case 1500:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 2200:
//                  mX264ParamData.rc.f_rf_constant = 21;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 27;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.3;
//            break;
//         case VideoResolution_1080p:
//            switch ((int)mBitrate){
//               case 700:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 1000:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  break;
//               case 1300:
//                  mX264ParamData.rc.f_rf_constant = 28;
//                  break;
//               case 1600:
//                  mX264ParamData.rc.f_rf_constant = 27;
//                  break;
//               case 2000:
//                  mX264ParamData.rc.f_rf_constant = 25;
//                  break;
//               case 2700:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 3800:
//                  mX264ParamData.rc.f_rf_constant = 22;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 27;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.3;
//            break;
//         case VideoResolution_2160p:
//            switch ((int)mBitrate){
//               case 2500:
//                  mX264ParamData.rc.f_rf_constant = 33;
//                  break;
//               case 3500:
//                  mX264ParamData.rc.f_rf_constant = 31;
//                  break;
//               case 4800:
//                  mX264ParamData.rc.f_rf_constant = 29;
//                  break;
//               case 6000:
//                  mX264ParamData.rc.f_rf_constant = 27;
//                  break;
//               case 7500:
//                  mX264ParamData.rc.f_rf_constant = 25;
//                  break;
//               case 10000:
//                  mX264ParamData.rc.f_rf_constant = 23;
//                  break;
//               case 15000:
//                  mX264ParamData.rc.f_rf_constant = 22;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 27;
//            }
//            mX264ParamData.rc.i_vbv_max_bitrate = mBitrate;
//            mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate * 1.4;
//            break;
//         default:
//            return false;
//      }
//   }
//   //In artificial scene, we using a big vbv buffer to ensure the quality of I frame
//   else if (mSceneType == SceneType_Artificial){
//      switch (mResolutionLevel){
//         case VideoResolution_360p:
//            switch ((int)mBitrate){
//               case 100:
//                  mX264ParamData.rc.f_rf_constant = 36;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 150 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 450 * 0.8;
//                  break;
//               case 150:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 200 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 450 * 0.8;
//                  break;
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 200 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 600 * 0.8;
//                  break;
//               case 250:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 250 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 600 * 0.8;
//                  break;
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 250 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 750 * 0.9;
//                  break;
//               case 425:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 350 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 750 * 0.9;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 350;
//                  mX264ParamData.rc.i_vbv_buffer_size = 1050;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 250 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 600 * 0.8;
//            }
//            break;
//         case VideoResolution_480p:
//            switch ((int)mBitrate){
//               case 150:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 200 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 600 * 0.8;
//                  break;
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 300 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 600 * 0.8;
//                  break;
//               case 300:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 300 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 900 * 0.8;
//                  break;
//               case 400:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 400 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 900 * 0.8;
//                  break;
//               case 525:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 400 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1200 * 0.9;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 525 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1200 * 0.9;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 525;
//                  mX264ParamData.rc.i_vbv_buffer_size = 1575;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 400 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 900 * 0.8;
//            }
//            break;
//         case VideoResolution_540p:
//            switch ((int)mBitrate){
//               case 200:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 300 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 900 * 0.8;
//                  break;
//               case 300:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 400 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 900 * 0.8;
//                  break;
//               case 400:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 400 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1200 * 0.8;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 500 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1200 * 0.8;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 500 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.9;
//                  break;
//               case 850:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 700 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.9;
//                  break;
//               case 1100:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 700;
//                  mX264ParamData.rc.i_vbv_buffer_size = 2100;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 500 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1200 * 0.8;
//            }
//            break;
//         case VideoResolution_720p:
//            switch ((int)mBitrate){
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 500 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.8;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 650 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.8;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 650 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//                  break;
//               case 1000:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 2400 * 0.9;
//                  break;
//               case 1400:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1000 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 2400 * 0.9;
//                  break;
//               case 2000:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 1000;
//                  mX264ParamData.rc.i_vbv_buffer_size = 3000;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//            }
//            break;
//         case VideoResolution_768p:
//            switch ((int)mBitrate){
//               case 350:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 500 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.8;
//                  break;
//               case 500:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 650 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1500 * 0.8;
//                  break;
//               case 650:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 650 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//                  break;
//               case 800:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//                  break;
//               case 1100:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 2400 * 0.9;
//                  break;
//               case 1500:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1000 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 2400 * 0.9;
//                  break;
//               case 2200:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 1000;
//                  mX264ParamData.rc.i_vbv_buffer_size = 3000;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 1950 * 0.8;
//            }
//            break;
//         case VideoResolution_1080p:
//            switch ((int)mBitrate){
//               case 700:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1000 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 3000 * 0.8;
//                  break;
//               case 1000:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1300 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 3000 * 0.8;
//                  break;
//               case 1300:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1300 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 3900 * 0.8;
//                  break;
//               case 1600:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1600 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 3900 * 0.8;
//                  break;
//               case 2000:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1600 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 4800 * 0.9;
//                  break;
//               case 2700:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 2000 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 4800 * 0.9;
//                  break;
//               case 3800:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 2000;
//                  mX264ParamData.rc.i_vbv_buffer_size = 6000;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 1600 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 3900 * 0.8;
//            }
//            break;
//         case VideoResolution_2160p:
//            switch ((int)mBitrate){
//               case 2500:
//                  mX264ParamData.rc.f_rf_constant = 36;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 3500 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 10500 * 0.8;
//                  break;
//               case 3500:
//                  mX264ParamData.rc.f_rf_constant = 35;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 4800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 10500 * 0.8;
//                  break;
//               case 4800:
//                  mX264ParamData.rc.f_rf_constant = 34;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 4800 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 14400 * 0.8;
//                  break;
//               case 6000:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 6000 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 14400 * 0.8;
//                  break;
//               case 7500:
//                  mX264ParamData.rc.f_rf_constant = 32;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 6000 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 18000 * 0.9;
//                  break;
//               case 10000:
//                  mX264ParamData.rc.f_rf_constant = 31;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 7500 * 0.9;
//				  mX264ParamData.rc.i_vbv_buffer_size = 18000 * 0.9;
//                  break;
//               case 15000:
//                  mX264ParamData.rc.f_rf_constant = 30;
//                  mX264ParamData.rc.i_vbv_max_bitrate = 7500;
//                  mX264ParamData.rc.i_vbv_buffer_size = 22500;
//                  break;
//               default:
//                  mX264ParamData.rc.f_rf_constant = 33;
//				  mX264ParamData.rc.i_vbv_max_bitrate = 6000 * 0.8;
//				  mX264ParamData.rc.i_vbv_buffer_size = 14400 * 0.8;
//            }
//            break;
//         default:
//            return false;
//      }
//      //mX264ParamData.rc.i_vbv_buffer_size = mX264ParamData.rc.i_vbv_max_bitrate*1.3;
//   }
//   if (mHighCodecLevel == 9){
//      mX264ParamData.rc.i_qp_max = 37;
//      mX264ParamData.rc.i_qp_min = 19;
//      mX264ParamData.rc.i_vbv_buffer_size = 0;
//      mX264ParamData.rc.i_vbv_max_bitrate = 0;
//      mQPLimitedLevel = 7;
//      if (mSceneType == SceneType_Artificial){
//         mX264ParamData.rc.f_rf_constant = 33;
//      }
//      else{
//         mX264ParamData.rc.f_rf_constant = 25;
//      }
//   }
//   
//   if (mReconfigType == Reconfig_SetBitrate || mReconfigType == Reconfig_SceneCut){
//      int ret;
//      //TODO:create a new function to replace it, for x264_encoder_reconfig doing too many things we don't care
//      ret = x264_encoder_reconfig(mX264Encoder, &mX264ParamData);
//      if (ret < 0){
//         LOGE("x264_encoder : x264_encoder_reconfig failed for parameter validation error!");
//         return false;
//      }
//   }
//   LOGD("x264_encoder : x264 encoder config/reconfig complete!");
//   mReconfigType = Reconfig_KeepSame;
//   return true;
//}

bool X264Encoder::RateControlConfig2()
{
   if (!mReconfigType) {
      return true;
   }
   if (mHighCodecLevel == 9) {
      mX264ParamData.rc.i_vbv_buffer_size = 0;
      mX264ParamData.rc.i_vbv_max_bitrate = 0;
      if (mSceneType == VIDEO_TYPE_DESKTOP) {
         mX264ParamData.rc.f_rf_constant = 36 + mCrfDelta;
      }
      else {
         mX264ParamData.rc.f_rf_constant = 26 + mCrfDelta;
      }
   }
   else {
      if (mSceneType == VIDEO_TYPE_DESKTOP) {
         mX264ParamData.rc.f_rf_constant = 37 + mCrfDelta;
         mX264ParamData.rc.i_vbv_buffer_size = mBitrate * 3;
         mX264ParamData.rc.i_vbv_max_bitrate = mBitrate * 6;
      }
      else {
         mX264ParamData.rc.f_rf_constant = 27 + mCrfDelta;
         mX264ParamData.rc.i_vbv_buffer_size = mBitrate;
         mX264ParamData.rc.i_vbv_max_bitrate = mBitrate * 1.5;
      }
   }
   
   if (mReconfigType == Reconfig_SetBitrate || mReconfigType == Reconfig_SceneCut) {
      int ret;
      //TODO:create a new function to replace it, for x264_encoder_reconfig doing too many things we don't care
      ret = x264_encoder_reconfig(mX264Encoder, &mX264ParamData);
      if (ret < 0) {
         LOGE("x264_encoder : x264_encoder_reconfig failed for parameter validation error!");
         return false;
      }
   }
   if ((mReconfigType == Reconfig_SceneCut || mReconfigType == Reconfig_Init) && mSceneType == VIDEO_TYPE_DESKTOP) {
      LOGI("[x264 Encoder]Now is in screen share mode!");
   }
   if (mReconfigType == Reconfig_SetBitrate) {
      int newBitrate = mBitrate;
      LOGI("[X264Enc]bitrate is change, new value is %d", newBitrate);
   }
   LOGD("x264_encoder : x264 encoder config/reconfig complete!");
   mReconfigType = Reconfig_KeepSame;
   return true;
}

bool X264Encoder::BitrateClassify(int &bitrate){   
   if (bitrate <= 0){
      LOGE("x264_encoder : Bitrate must be positive!");
      return false;
   }
   if (bitrate > mOriginBitrate){
      bitrate = mOriginBitrate;
   }
   //In current version, mQPLimitedLevel mustn't be lower than 5
   if (mQPLimitedLevel < 5){
      mQPLimitedLevel = 5;
   }
   //SceneType_Natural is the standard
   switch (mResolutionLevel)
   {
      case VideoResolution_360p:
         if (bitrate <= 125 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 100;
         }
         else if (bitrate <= 175 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 150;
         }
         else if (bitrate <= 225 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 200;
         }
         else if (bitrate <= 300 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 250;
         }
         else if (bitrate <= 375 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 350;
         }
         else if (bitrate <= 450 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 425;
         }
         else {
            mBitrate = 500;
         }
         break;
      case VideoResolution_480p:
         if (bitrate <= 175 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 150;
         }
         else if (bitrate <= 250 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 200;
         }
         else if (bitrate <= 350 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 300;
         }
         else if (bitrate <= 440 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 400;
         }
         else if (bitrate <= 575 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 525;
         }
         else if (bitrate <= 725 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 650;
         }
         else {
            mBitrate = 800;
         }
         break;
      case VideoResolution_540p:
         if (bitrate <= 250 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 200;
         }
         else if (bitrate <= 350 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 300;
         }
         else if (bitrate <= 450 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 400;
         }
         else if (bitrate <= 575 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 500;
         }
         else if (bitrate <= 750 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 650;
         }
         else if (bitrate <= 975 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 850;
         }
         else {
            mBitrate = 1100;
         }
         break;
      case VideoResolution_720p:
         if (bitrate <= 425 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 350;
         }
         else if (bitrate <= 575 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 500;
         }
         else if (bitrate <= 725 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 650;
         }
         else if (bitrate <= 900 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 800;
         }
         else if (bitrate <= 1200 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 1000;
         }
         else if (bitrate <= 1700 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 1400;
         }
         else {
            mBitrate = 2000;
         }
         break;
      case VideoResolution_768p:
         if (bitrate <= 425 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 350;
         }
         else if (bitrate <= 575 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 500;
         }
         else if (bitrate <= 725 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 650;
         }
         else if (bitrate <= 950 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 800;
         }
         else if (bitrate <= 1300 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 1100;
         }
         else if (bitrate <= 1800 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 1500;
         }
         else {
            mBitrate = 2200;
         }
         break;
      case VideoResolution_1080p:
         if (bitrate < 850 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 700;
         }
         else if (bitrate <= 1150 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 1000;
         }
         else if (bitrate <= 1450 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 1300;
         }
         else if (bitrate <= 1800 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 1600;
         }
         else if (bitrate <= 2350 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 2000;
         }
         else if (bitrate <= 3250 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 2700;
         }
         else {
            mBitrate = 3800;
         }
         break;
      case VideoResolution_2160p:
         if (bitrate < 3000 || (mIsQualityLimited && mQPLimitedLevel == 1)){
            mBitrate = 2500;
         }
         else if (bitrate <= 4150 || (mIsQualityLimited && mQPLimitedLevel == 2)){
            mBitrate = 3500;
         }
         else if (bitrate <= 5400 || (mIsQualityLimited && mQPLimitedLevel == 3)){
            mBitrate = 4800;
         }
         else if (bitrate <= 6750 || (mIsQualityLimited && mQPLimitedLevel == 4)){
            mBitrate = 6000;
         }
         else if (bitrate <= 8750 || (mIsQualityLimited && mQPLimitedLevel == 5)){
            mBitrate = 7500;
         }
         else if (bitrate <= 12500 || (mIsQualityLimited && mQPLimitedLevel == 6)){
            mBitrate = 10000;
         }
         else {
            mBitrate = 15000;
         }
         break;
      default:
         LOGE("x264_encoder : Resolution level is invalid! Classification failed!");
         return false;
   }
   return true;
}

bool X264Encoder::ResClassify()
{
   if (mOriginWidth * mOriginHeight <= 224*120) {
      mResolutionLevel = VideoResolution_120p;
   }
   else if (mOriginWidth * mOriginHeight <= 320*180) {
      mResolutionLevel = VideoResolution_180p;
   }
   else if (mOriginWidth * mOriginHeight <= 432 * 240) {
      mResolutionLevel = VideoResolution_240p;
   }
   else if (mOriginWidth * mOriginHeight <= 640 * 360) {
      mResolutionLevel = VideoResolution_360p;
   }
   else if (mOriginWidth * mOriginHeight <= 864 * 480) {
      mResolutionLevel = VideoResolution_480p;
   }
   else if (mOriginWidth * mOriginHeight <= 690 * 540) {
      mResolutionLevel = VideoResolution_540p;
   }
   else if (mOriginWidth * mOriginHeight <= 1366 * 768) {
      mResolutionLevel = VideoResolution_720p;
   }
   else if (mOriginWidth * mOriginHeight <= 1920 * 1080) {
      mResolutionLevel = VideoResolution_1080p;
   }
   //we offer a level set for 4K-Resolution, but performance may be poor
   else if (mOriginWidth * mOriginHeight <= 4069 * 2160) {
      mResolutionLevel = VideoResolution_2160p;
   }
   //Don't support resolution higher than 4096x2160, considering it invalid
   else {
      mResolutionLevel = VideoResolution_Error;
      return false;
   }
   return true;
}

bool X264Encoder::BitrateNormalize()
{
   float rate = mHighCodecLevel / 10;
   float fpsRatio = 1;
   if (mFrameRate <= 5) {
      fpsRatio = 0.8;
   }
   else if (mFrameRate <= 18) {
      fpsRatio = 1;
   }
   else if (mFrameRate <= 30) {
      fpsRatio = 1.5;
   }
   else {
      fpsRatio = 2.5;
   }


   //this function clip the input bitrate parameter
   switch (mResolutionLevel)
   {
   case VideoResolution_120p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[0] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[0] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[0] * 0.4* fpsRatio) {
         mBitrate = DefaultVideoBitrate[0] * 0.4* fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[0] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[0] * 0.5* fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[0] * 0.5* fpsRatio;
      }
      break;
   case VideoResolution_180p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[1] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[1] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[1] * 0.4* fpsRatio) {
         mBitrate = DefaultVideoBitrate[1] * 0.4* fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[1] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[1] * 0.5* fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[1] * 0.5* fpsRatio;
      }
      break;
   case VideoResolution_240p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[2] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[2] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[2] * 0.4* fpsRatio) {
         mBitrate = DefaultVideoBitrate[2] * 0.4* fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[2] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[2] * 0.5* fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[2] * 0.5 * fpsRatio;
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      break;
   case VideoResolution_360p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[3] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[3] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[3] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[3] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[3] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[3] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[3] * 0.5 * fpsRatio;
      }
      break;
   case VideoResolution_480p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[4] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[4] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[4] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[4] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[4] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[4] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[4] * 0.5 * fpsRatio;
      }
      break;
   case VideoResolution_540p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[5] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[5] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[5] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[5] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[5] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[5] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[5] * 0.5 * fpsRatio;
      }
      break;
   case VideoResolution_720p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[6] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[6] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[6] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[6] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[6] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[6] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[6] * 0.5 * fpsRatio;
      }
      break;
   case VideoResolution_1080p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[8] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[8] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[8] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[8] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[8] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[8] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[8] * 0.5 * fpsRatio;
      }
      break;
   case VideoResolution_2160p:
      if (mIsQualityLimited && mOriginBitrate > DefaultVideoBitrate[9] * 2 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[9] * 2 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is higher than limit!");
      }
      else if (mIsQualityProtect && mOriginBitrate > 0 && mOriginBitrate < DefaultVideoBitrate[9] * 0.4 * fpsRatio) {
         mBitrate = DefaultVideoBitrate[9] * 0.4 * fpsRatio;
         LOGW("x264_encoder : Input bitrate is lower than limit!");
      }
      else if (mOriginBitrate <= 0) {
         mBitrate = DefaultVideoBitrate[9] * fpsRatio;
         LOGE("x264_encoder : Input bitrate is not positive!");
      }
      else {
         mBitrate = mOriginBitrate * fpsRatio;
      }
      mBitrateMax = mBitrate * (1.2 + 0.8 * (0.1 + rate)); // 1.28 - 2, but could not lower than half of default bitrate
      if (mBitrateMax < DefaultVideoBitrate[9] * 0.5 * fpsRatio) {
         mBitrateMax = DefaultVideoBitrate[9] * 0.5 * fpsRatio;
      }
      break;
   default:
      mBitrate = 0;
      mBitrateMin = 0;
      mBitrateMax = 0;
      LOGE("x264_encoder : Resolution level is invalid! Bitrate setting failed!");
      return false;
   }
   //mBitrate = mBitrate * fpsRatio;
   //mBitrateMax *= fpsRatio;
   mBitrateMin = mBitrate * 0.5; // 0.5
   return true;
}

bool X264Encoder::SetBitrate(int bitrate)
{
   if (mReconfigType && !mIsInited) {
      LOGW("x264_encoder : Encoder is reconfiguring or not initialized! Bitrate set failed!");
      return false;
   }
   if (!mIsAdjustBitrate) {
      LOGW("x264_encoder : Bitrate adjestment is turn off! Bitrate set failed!");
      return false;
   }
   if (bitrate <= 0) {
      LOGE("x264_encoder : Can't set a negative bitrate!");
      return false;
   }
   if (bitrate == mBitrate) {
      return true;
   }

   if (bitrate > mBitrateMax) {
      bitrate = mBitrateMax;
   }
   if (bitrate < mBitrateMin) {
      bitrate = mBitrateMin;
   }
   mBitrate = bitrate;
   mReconfigType = Reconfig_SetBitrate;
   return true;
}

bool X264Encoder::DiffMbCheck(x264_picture_t *pic)
{
   
   int iPadRight = 0;
   int iPadBottom = 0;
   int iRow = 0;
   int iCol = 0;
   int iX = 0;
   int iY = 0;
   int iMBCnt = 0;
   int iRStep = 0;
   int iCStep = 0;

   uint8_t *pCurFrame = nullptr;
   uint8_t *pRefFrame = nullptr;
   uint8_t *pDifFrame = nullptr;
   uint8_t *pResult = nullptr;
   uint8_t *pMBScan = nullptr;

   if (!pic->motion_status) {
      LOGE("[X264Encoder]no memory allocated for frame difference check result!");
      return false;
   }

   /*memset(pic->motion_status, 0xff, mWidthMb * mHeightMb);
   return true;*/

   if (!pic || !mRefFrame) {
      memset(pic->motion_status, 0xff, mWidthMb * mHeightMb);
      LOGE("[X264Encoder]frame difference check input error!");
      return false;
   }

   //according to past experience, fast skip mode in every frame might cause a reference error accidentally
   //TODO: find the bug and only do this at first frame
   /*if (mFrameCount % mLiveParam->frame_rate == (mLiveParam->frame_rate >> 1)) {
      memset(pic->motion_status, 0xff, mWidthMb * mHeightMb);
      return true;
   }*/

   memset(pic->motion_status, 0, mWidthMb * mHeightMb);
   memset(mDiffFrame, 0, mOriginWidth * mOriginHeight);

   iPadRight = mOriginWidth % 16;
   iPadBottom = mOriginHeight % 16;
   
   for (iRow = 0; iRow < mOriginHeight; iRow++) {
      pCurFrame = pic->img.plane[0] + iRow * mOriginWidth;
      pRefFrame = (uint8_t *)mRefFrame + iRow * mOriginWidth;
      pDifFrame = (uint8_t *)mDiffFrame + iRow * mOriginWidth;
      for (iCol = 0; iCol < mOriginWidth; iCol++) {
         if (pCurFrame[iCol] != pRefFrame[iCol]) {
            pDifFrame[iCol] = 0xff;
         }
      }
   }
   for (iRow = 0; iRow < mHeightMb; iRow++) {
      pResult = pic->motion_status + iRow * mWidthMb;
      for (iCol = 0; iCol < mWidthMb; iCol++) {
         bool changed = false;
         pDifFrame = (uint8_t *)mDiffFrame + iRow * 16 * mOriginWidth + iCol * 16;
         if (iPadBottom && iRow == mHeightMb - 1) {
            iRStep = iPadBottom;
         }
         else {
            iRStep = 16;
         }
         if (iPadRight && iCol == mWidthMb - 1) {
            iCStep = iPadRight;
         }
         else {
            iCStep = 16;
         }
         for (iY = 0; iY < iRStep; iY++) {
            pMBScan = pDifFrame + iY * mOriginWidth;
            for (iX = 0; iX < iCStep; iX++) {
               if (pMBScan[iX]) {
                  changed = true;
                  break;
               }
            }
            if (changed) {
               break;
            }
         }
         pResult[iCol] = changed ? 0xFF : 0x00;
      }
   }
   memcpy(mRefFrame, pic->img.plane[0], mOriginHeight * mOriginWidth * 3 / 2);
   return true;
}

bool X264Encoder::SetQualityLimited(bool onoff){
   //this parameter could be set only after init
   if (mIsInited){
      mIsQualityLimited = onoff;
      return true;
   }
   return false;
}

bool X264Encoder::SetAdjustBitrate(bool onoff){
   //this parameter could be set only after init
   if (mIsInited){
      mIsAdjustBitrate = onoff;
      return true;
   }
   return false;
}

bool X264Encoder::RequestKeyframe(){
   if (mIsRequestKeyframe){
      LOGW("x264_encoder : Keyframe requests are too frequent!");
      return false;
   }
   mIsRequestKeyframe = true;
   return true;
}

bool X264Encoder::GetRecentlyDecodeYUVData(char** data, int* size) {
	for (int y = 0; y < mX264ParamData.i_height; y++) {
	   memcpy(mReconFrame + y * mX264ParamData.i_width, mX264PicOut.img.plane[0] + y * mX264PicOut.img.i_stride[0], mX264ParamData.i_width);
	}
	*data = mReconFrame;
	*size = mX264ParamData.i_width*mX264ParamData.i_height*1.5;
	return true;
}
