#include "hw_decoder.h"
#include "../api/live_interface.h"
#include "../common/vhall_log.h"
#include "../json/json.h"
#include "../common/live_obs.h"
#include "../utility/utility.h"
#include <string.h>
#include "../rtmpplayer/vhall_live_player.h"

HWVideoDecoder::HWVideoDecoder(VhallPlayerInterface*vinnyLive):mVinnyLive(vinnyLive)
{
   
}

HWVideoDecoder::~HWVideoDecoder() {
   destroy();
}

void HWVideoDecoder::destroy() {
   LOGW("HWVideoDecoder::destroy.");
   EventParam param;
   param.mId = -1;
   mVinnyLive->NotifyEvent(VIDEO_HWDECODER_DESTORY, param);
}

bool HWVideoDecoder::Init(int w, int h) {
   LOGW("HWVideoDecoder::Init %dx%d", w, h);
   mVideoParam.width = w;
   mVideoParam.height = h;
   VHJson::StyledWriter root;
   VHJson::Value item;
   item["width"] = VHJson::Value(mVideoParam.width);
   item["height"] = VHJson::Value(mVideoParam.height);
   std::string json_str = root.write(item);
   EventParam param;
   param.mId = -1;
   param.mDesc = json_str;
   mVinnyLive->NotifyEvent(VIDEO_HWDECODER_INIT, param);
   
   return true;
}

bool HWVideoDecoder::Decode(const char * data, int size, int & decode_size, uint64_t pts) {
   LOGW("HWDecodeVideo begin++++++++, %llu", pts);
   int decFrameCnt =  mVinnyLive->OnHWDecodeVideo(
                                                    data,
                                                    size,
                                                    mVideoParam.width,
                                                    mVideoParam.height,
                                                    pts);
   LOGW("HWDecodeVideo end---------");
   return  decFrameCnt > 0;
}

bool HWVideoDecoder::GetDecodecData(unsigned char * decoded_data, int & decode_size, uint64_t& pts){
   DecodedVideoInfo* decodedVideoFrame = mVinnyLive->GetHWDecodeVideo();
   if(decodedVideoFrame != NULL){
      if( VHALL_COLOR_FormatYUV420SemiPlanar == decodedVideoFrame->mMediaFormat
         || VHALL_COLOR_FormatYUV420PackedSemiPlanar32m == decodedVideoFrame->mMediaFormat){
         LOGW("will VHALL_COLOR_FormatYUV420SemiPlanar %d", decodedVideoFrame->mMediaFormat);
         Utility::SemiPlanar2Planar((unsigned char*)decodedVideoFrame->mData, decoded_data, mVideoParam.width, mVideoParam.height);
      }else if( VHALL_COLOR_FormatYUV420Planar == decodedVideoFrame->mMediaFormat){
         LOGW("no VHALL_COLOR_FormatYUV420SemiPlanar %d", decodedVideoFrame->mMediaFormat);
         memcpy(decoded_data, decodedVideoFrame->mData, decodedVideoFrame->mSize);
      }
      pts = decodedVideoFrame->mTS;
      LOGW("HWVideoDecoder success,timestamp=%llu", pts);
      return true;
   }
   return false;
}


