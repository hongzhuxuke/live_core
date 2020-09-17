#include<string.h>
#include "rtmp_flv_utility.h"
#include "live_sys.h"
#include "vhall_log.h"
#include "byte_stream.h"
#include "assert.h"
#include "sps_pps.h"
#include "rtmp_amf0.h"
#include "../3rdparty/json/json.h"
//#include "talk/base/base64.h"


#define ERROR_NOT_ANNEXB       -2
#define ERROR_ADD_UNIT         -1

#define ERROR_SUCCESS           0 

bool avc_startswith_annexb(ByteStream* stream, int* pnb_start_code)
{
   char* bytes = stream->data() + stream->pos();
   char* p = bytes;
   
   for (;;) {
      if (!stream->require(p - bytes + 3)) {
         return false;
      }
      
      // not match
      if (p[0] != (char)0x00 || p[1] != (char)0x00) {
         return false;
      }
      
      // match N[00] 00 00 01, where N>=0
      if (p[2] == (char)0x01) {
         if (pnb_start_code) {
            *pnb_start_code = (int)(p - bytes) + 3;
         }
         return true;
      }
      
      p++;
   }
   
   return false;
}

CodecSampleUnit::CodecSampleUnit(){
   size = 0;
   bytes = NULL;
}

CodecSampleUnit::~CodecSampleUnit(){
   
}

AacAvcCodecSample::AacAvcCodecSample(){
   clear();
}

AacAvcCodecSample::~AacAvcCodecSample(){
   
}

void AacAvcCodecSample::clear(){
   is_video = false;
   nb_sample_units = 0;
   
   cts = 0;
   frame_type = FlvVideoFrameTypeReserved;
   avc_packet_type = FlvVideoAVCPacketTypeReserved;
   has_idr = false;
   first_nalu_type = AvcNaluTypeReserved;
   
   acodec = FlvSoundFormatReserved1;
   sound_rate = FlvSoundSampleRateReserved;
   sound_size = FlvSoundSampleSizeReserved;
   sound_type = FlvSoundTypeReserved;
   aac_packet_type = FlvAACPacketTypeReserved;
}


int AacAvcCodecSample::add_sample_unit(char* bytes, int size){
   int ret = 0;
   
   if (nb_sample_units >= AAC_AVC_MAX_CODEC_SAMPLE) {
      ret = -1;
      LOGE("hls decode samples error, "
           "exceed the max count: %d, ret=%d", AAC_AVC_MAX_CODEC_SAMPLE, ret);
      return ret;
   }
   
   CodecSampleUnit* sample_unit = &sample_units[nb_sample_units++];
   sample_unit->bytes = bytes;
   sample_unit->size = size;
   
   // for video, parse the nalu type, set the IDR flag.
   if (is_video) {
      AvcNaluType nal_unit_type = (AvcNaluType)(bytes[0] & 0x1f);
      
      if (nal_unit_type == AvcNaluTypeIDR) {
         has_idr = true;
      }
      
      if (first_nalu_type == AvcNaluTypeReserved) {
         first_nalu_type = nal_unit_type;
      }
   }
   
   return ret;
}


CuePointAmfMsg::CuePointAmfMsg(){
   Reset();
}

CuePointAmfMsg::~CuePointAmfMsg(){
   
}

std::string CuePointAmfMsg::ToJsonStr(){
   LOGD("id:%s type:%s content:%s",mId.c_str(),mType.c_str(),mContent.c_str());
   VHJson::Value value(VHJson::objectValue);
   value[kOnCuePointAmfIdMsg] = VHJson::Value(mId);
   value[kOnCuePointAmfTypeMsg] = VHJson::Value(mType);
   value[kOnCuePointAmfContentMsg] = VHJson::Value(mContent);
   VHJson::FastWriter rootWriter;
   return rootWriter.write(value);
}

void CuePointAmfMsg::Reset(){
   mId = "";
   mType = "";
   mContent = "";
}

FlvTagDemuxer::FlvTagDemuxer()
{
   avc_parse_sps = true;
   metadata_type = MetadataTypeOnMetaDataNone;
   width = 0;
   height = 0;
   duration = 0;
   NAL_unit_length = 0;
   frame_rate = 0;
   audio_samplerate = 0;
   audio_channels = 0;
   audio_sample_size = 0;
   
   video_data_rate = 0;
   video_codec_id = 0;
   
   audio_data_rate = 0;
   audio_codec_id = 0;
   
   avc_profile = AvcProfileReserved;
   avc_level = AvcLevelReserved;
   aac_object = AacObjectTypeReserved;
   aac_sample_rate = AAC_SAMPLE_RATE_UNSET; // sample rate ignored
   aac_channels = 0;
   avc_extra_size = 0;
   avc_extra_data = NULL;
   aac_extra_size = 0;
   aac_extra_data = NULL;
   
   sequenceParameterSetLength = 0;
   sequenceParameterSetNALUnit = NULL;
   pictureParameterSetLength = 0;
   pictureParameterSetNALUnit = NULL;
   
   payload_format = AvcPayloadFormatGuess;
   stream = new ByteStream();
}

FlvTagDemuxer::~FlvTagDemuxer()
{
   if (avc_extra_data){
      delete[] avc_extra_data;
      avc_extra_data = NULL;
   }
   if (aac_extra_data){
      delete[] aac_extra_data;
      aac_extra_data = NULL;
   }
   
   if (stream){
      delete stream;
   }
   
   if (sequenceParameterSetNALUnit){
      delete[] sequenceParameterSetNALUnit;
      sequenceParameterSetNALUnit = NULL;
   }
   
   if (pictureParameterSetNALUnit){
      delete[] pictureParameterSetNALUnit;
      pictureParameterSetNALUnit = NULL;
   }
}

int FlvTagDemuxer::metadata_demux2(char *data, int size)
{
   ByteStream stream;
   if (stream.initialize(data, size) != 0){
      return -1;
   }
   VhallAmf0Any *name = VhallAmf0Any::str();
   VhallAmf0EcmaArray *ecmarray = VhallAmf0Any::ecma_array();
   name->read(&stream);
   ecmarray->read(&stream);
   
   int i = 0;
   
   while (ecmarray->count() > i){
      if (ecmarray->key_at(i) == "duration" && ecmarray->value_at(i)->is_number()){
         duration = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "width" && ecmarray->value_at(i)->is_number()){
         width = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "height" && ecmarray->value_at(i)->is_number()){
         height = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "framerate" && ecmarray->value_at(i)->is_number()){
         frame_rate = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "videocodecid" && ecmarray->value_at(i)->is_number()){
         video_codec_id = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "videodatarate" && ecmarray->value_at(i)->is_number()){
         video_data_rate = (int)(ecmarray->value_at(i)->to_number()*1000);
      }
      else if (ecmarray->key_at(i) == "audiocodecid" && ecmarray->value_at(i)->is_number()){
         audio_codec_id = ecmarray->value_at(i)->to_number();
      }
      else if (ecmarray->key_at(i) == "audiodatarate" && ecmarray->value_at(i)->is_number()){
         audio_data_rate = (int)(ecmarray->value_at(i)->to_number()*1000);
      }
      else {
         LOGE("unknown metadata item%s\n", ecmarray->key_at(i).c_str());
      }
      LOGD(" metadata item %s\n", ecmarray->key_at(i).c_str());
      i++;
   }
   return 0;
}

int FlvTagDemuxer::metadata_demux(char *data, int size)
{
   metadata_type = MetadataTypeOnMetaDataNone;
   ByteStream stream;
   if (stream.initialize(data, size) != ERROR_SUCCESS){
      return -1;
   }
   int ret = 0;
   std::string name;
   if ((ret = vhall_amf0_read_string(&stream, name)) != ERROR_SUCCESS) {
      LOGE("decode metadata name failed. ret=%d", ret);
      return ret;
   }
   // ignore the @setDataFrame
   if (name == VHALL_CONSTS_RTMP_SET_DATAFRAME) {
      if ((ret = vhall_amf0_read_string(&stream, name)) != ERROR_SUCCESS) {
         LOGE("decode metadata name failed. ret=%d", ret);
         return ret;
      }
      if (name!=VHALL_CONSTS_RTMP_ON_METADATA) {
         LOGW("name is %s is not %s",name.c_str(),VHALL_CONSTS_RTMP_ON_METADATA);
         return -1;
      }
   }else if(name == VHALL_CONSTS_RTMP_ON_CUEPOINT) {
      // the metadata maybe object or ecma array
      VhallAmf0Any* any = NULL;
      if ((ret = vhall_amf0_read_any(&stream, &any)) != ERROR_SUCCESS) {
         LOGE("decode metadata metadata failed. ret=%d", ret);
         return ret;
      }
      VhallAmf0Object *obj = NULL;
      if (any!=NULL&&any->is_object()) {
         obj = any->to_object();
         metadata_type = MetadataTypeOnCuePoint;
         cuepoint_amf_msg.Reset();
         LOGI("decode metadata onCuePoint object success");
         VhallAmf0Any* item;
         if ((item = obj->get_property(kOnCuePointAmfIdMsg)) != NULL && item->is_string()){
            cuepoint_amf_msg.mId = item->to_str();
         }
         if ((item = obj->get_property(kOnCuePointAmfTypeMsg)) != NULL && item->is_string()){
            cuepoint_amf_msg.mType = item->to_str();
         }
         if ((item = obj->get_property(kOnCuePointAmfContentMsg)) != NULL && item->is_string()){
            cuepoint_amf_msg.mContent = item->to_str();
         }
         vhall_freep(any);
      }else{
         vhall_freep(any);
         return -1;
      }
      return ERROR_SUCCESS;
   }
   LOGD("decode metadata name success. name=%s", name.c_str());
   // the metadata maybe object or ecma array
   VhallAmf0Any* any = NULL;
   if ((ret = vhall_amf0_read_any(&stream, &any)) != ERROR_SUCCESS) {
      LOGE("decode metadata metadata failed. ret=%d", ret);
      return ret;
   }
   VhallAmf0Object *obj = NULL;
   if (any!=NULL&&any->is_object()) {
      obj = any->to_object();
      metadata_type = MetadataTypeOnMetaData;
      LOGI("decode metadata object success");
      VhallAmf0Any* item;
      if ((item = obj->get_property("duration")) != NULL && item->is_number()){
         duration = (int)item->to_number();
      }
      if ((item = obj->get_property("width")) != NULL && item->is_number()){
         width = (int)item->to_number();
      }
      if ((item = obj->get_property("height")) != NULL && item->is_number()){
         height = item->to_number();
      }
      if ((item = obj->get_property("framerate")) != NULL && item->is_number()){
         frame_rate = (int)item->to_number();
      }
      if ((item = obj->get_property("videocodecid")) != NULL && item->is_number()){
         video_codec_id = (int)item->to_number();
      }
      if ((item = obj->get_property("videodatarate")) != NULL && item->is_number()){
         video_data_rate = (int)(item->to_number() * 1000);
      }
      if ((item = obj->get_property("audiocodecid")) != NULL && item->is_number()){
         audio_codec_id = (int)item->to_number();
      }
      if ((item = obj->get_property("audiodatarate")) != NULL && item->is_number()){
         audio_data_rate = (int)(item->to_number() * 1000);
      }
      if ((item = obj->get_property("audiosamplerate")) != NULL && item->is_number()){
         audio_samplerate = (int)(item->to_number());
      }
      if ((item = obj->get_property("audiosamplesize")) != NULL && item->is_number()){
         audio_sample_size = (int)(item->to_number());
      }
      if ((item = obj->get_property("audiochannels")) != NULL && item->is_number()){
         audio_channels = (int)(item->to_number());
      }
      
      vhall_freep(any);
   }else{
      vhall_freep(any);
      return -1;
   }
   return ERROR_SUCCESS;
}

bool FlvTagDemuxer::is_avc_codec_ok()
{
   return avc_extra_size > 0 && avc_extra_data;
}

bool FlvTagDemuxer::is_aac_codec_ok()
{
   return aac_extra_size > 0 && aac_extra_data;
}

int FlvTagDemuxer::audio_aac_demux(char* data, int size, AacAvcCodecSample* sample)
{
   int ret = ERROR_SUCCESS;
   
   sample->is_video = false;
   
   if (!data || size <= 0) {
      LOGE("no audio present, ignore it.");
      return ret;
   }
   
   if ((ret = stream->initialize(data, size)) != ERROR_SUCCESS) {
      return ret;
   }
   
   // audio decode
   if (!stream->require(1)) {
      ret = -1;
      LOGE("aac decode sound_format failed. ret=%d", ret);
      return ret;
   }
   
   // @see: E.4.2 Audio Tags, video_file_format_spec_v10_1.pdf, page 76
   int8_t sound_format = stream->read_1bytes();
   
   int8_t sound_type = sound_format & 0x01;
   int8_t sound_size = (sound_format >> 1) & 0x01;
   int8_t sound_rate = (sound_format >> 2) & 0x03;
   sound_format = (sound_format >> 4) & 0x0f;
   
   audio_codec_id = sound_format;
   sample->acodec = (FlvSoundFormat)audio_codec_id;
   
   sample->sound_type = (FlvSoundType)sound_type;
   sample->sound_rate = (FlvSoundSampleRate)sound_rate;
   sample->sound_size = (FlvSoundSampleSize)sound_size;
   
   // we support h.264+mp3 for hls.
   if (audio_codec_id == FlvSoundFormatMP3) {
      return -1;
   }
   
   // only support aac
   if (audio_codec_id != FlvSoundFormatAAC) {
      ret = -1;
      LOGE("aac only support mp3/aac codec. actual=%d, ret=%d", audio_codec_id, ret);
      return ret;
   }
   
   if (!stream->require(1)) {
      ret = -1;
      LOGE("aac decode aac_packet_type failed. ret=%d", ret);
      return ret;
   }
   
   int8_t aac_packet_type = stream->read_1bytes();
   sample->aac_packet_type = (FlvAACPacketType)aac_packet_type;
   
   if (aac_packet_type == FlvAACPacketTypeSequenceHeader) {
      // AudioSpecificConfig
      // 1.6.2.1 AudioSpecificConfig, in aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 33.
      aac_extra_size = stream->size() - stream->pos();
      if (aac_extra_size > 0) {
         delete [] aac_extra_data;
         aac_extra_data = new char[aac_extra_size];
         memcpy(aac_extra_data, stream->data() + stream->pos(), aac_extra_size);
         
         // demux the sequence header.
         if ((ret = audio_aac_sequence_header_demux(aac_extra_data, aac_extra_size)) != ERROR_SUCCESS) {
            return ret;
         }
      }
   }
   else if (aac_packet_type == FlvAACPacketTypeRawData) {
      // ensure the sequence header demuxed
      if (!is_aac_codec_ok()) {
         LOGW("aac ignore type=%d for no sequence header. ret=%d", aac_packet_type, ret);
         return ret;
      }
      
      // Raw AAC frame data in UI8 []
      // 6.3 Raw Data, aac-iso-13818-7.pdf, page 28
      if ((ret = sample->add_sample_unit(stream->data() + stream->pos(), stream->size() - stream->pos())) != 0) {
         LOGE("aac add sample failed. ret=%d", ret);
         return ret;
      }
   }
   else {
      // ignored.
   }
   
   // reset the sample rate by sequence header
   if (aac_sample_rate != AAC_SAMPLE_RATE_UNSET) {
      static int aac_sample_rates[] = {
         96000, 88200, 64000, 48000,
         44100, 32000, 24000, 22050,
         16000, 12000, 11025, 8000,
         7350, 0, 0, 0
      };
      switch (aac_sample_rates[aac_sample_rate]) {
         case 11025:
            sample->sound_rate = FlvSoundSampleRate11025;
            break;
         case 22050:
            sample->sound_rate = FlvSoundSampleRate22050;
            break;
         case 44100:
            sample->sound_rate = FlvSoundSampleRate44100;
            break;
         default:
            break;
      };
   }
   LOGD("aac decoded, type=%d, codec=%d, asize=%d, rate=%d, format=%d, size=%d",
        sound_type, audio_codec_id, sound_size, sound_rate, sound_format, size);
   
   return ret;
   
}

int FlvTagDemuxer::audio_mp3_demux(char* data, int size, AacAvcCodecSample* sample)
{
   int ret = 0;
   
   // we always decode aac then mp3.
   assert(sample->acodec == FlvSoundFormatMP3);
   
   // @see: E.4.2 Audio Tags, video_file_format_spec_v10_1.pdf, page 76
   if (!data || size <= 1) {
      LOGI("no mp3 audio present, ignore it.");
      return ret;
   }
   
   // mp3 payload.
   if ((ret = sample->add_sample_unit(data + 1, size - 1)) != 0) {
      LOGE("audio codec add mp3 sample failed. ret=%d", ret);
      return ret;
   }
   
   LOGI("audio decoded, type=%d, codec=%d, asize=%d, rate=%d, format=%d, size=%d",
        sample->sound_type, audio_codec_id, sample->sound_size, sample->sound_rate, sample->acodec, size);
   
   return ret;
}

int FlvTagDemuxer::video_avc_demux(char* data, int size, AacAvcCodecSample* sample)
{
   int ret = 0;
   
   sample->is_video = true;
   
   if (!data || size <= 0) {
      LOGE("no video present, ignore it.");
      return ret;
   }
   
   if ((ret = stream->initialize(data, size)) != ERROR_SUCCESS) {
      return ret;
   }
   
   // video decode
   if (!stream->require(1)) {
      ret = -1;
      LOGE("avc decode frame_type failed. ret=%d", ret);
      return ret;
   }
   
   // @see: E.4.3 Video Tags, video_file_format_spec_v10_1.pdf, page 78
   int8_t frame_type = stream->read_1bytes();
   int8_t codec_id = frame_type & 0x0f;
   frame_type = (frame_type >> 4) & 0x0f;
   
   sample->frame_type = (FlvVideoFrameType)frame_type;
   
   // ignore info frame without error,
   // @see https://github.com/ossrs/srs/issues/288#issuecomment-69863909
   if (sample->frame_type == FlvVideoFrameTypeVideoInfoFrame) {
      LOGW("avc igone the info frame, ret=%d", ret);
      return ret;
   }
   
   // only support h.264/avc
   if (codec_id != FlvVideoCodecIDAVC) {
      ret = -1;
      LOGE("avc only support video h.264/avc codec. actual=%d, ret=%d", codec_id, ret);
      return ret;
   }
   video_codec_id = codec_id;
   
   if (!stream->require(4)) {
      ret = -1;
      LOGE("avc decode avc_packet_type failed. ret=%d", ret);
      return ret;
   }
   int8_t avc_packet_type = stream->read_1bytes();
   int32_t composition_time = stream->read_3bytes();
   
   // pts = dts + cts.
   sample->cts = composition_time;
   sample->avc_packet_type = (FlvVideoAVCPacketType)avc_packet_type;
   
   if (avc_packet_type == FlvVideoAVCPacketTypeSequenceHeader) {
      if ((ret = avc_demux_sps_pps(stream)) != ERROR_SUCCESS) {
         return ret;
      }
   }
   else if (avc_packet_type == FlvVideoAVCPacketTypeNALU){
      // ensure the sequence header demuxed
      if (!is_avc_codec_ok()) {
         LOGW("avc ignore type=%d for no sequence header. ret=%d", avc_packet_type, ret);
         return ret;
      }
      
      // guess for the first time.
      if (payload_format == AvcPayloadFormatGuess) {
         // One or more NALUs (Full frames are required)
         // try  "AnnexB" from H.264-AVC-ISO_IEC_14496-10.pdf, page 211.
         if ((ret = avc_demux_annexb_format(stream, sample)) != 0) {
            // stop try when system error.
            if (ret != ERROR_NOT_ANNEXB) {
               LOGE("avc demux for annexb failed. ret=%d", ret);
               return ret;
            }
            
            // try "ISO Base Media File Format" from H.264-AVC-ISO_IEC_14496-15.pdf, page 20
            if ((ret = avc_demux_ibmf_format(stream, sample)) != ERROR_SUCCESS) {
               return ret;
            }
            else {
               payload_format = AvcPayloadFormatIbmf;
               LOGI("hls guess avc payload is ibmf format.");
            }
         }
         else {
            payload_format = AvcPayloadFormatAnnexb;
            LOGI("hls guess avc payload is annexb format.");
         }
      }
      else if (payload_format == AvcPayloadFormatIbmf) {
         // try "ISO Base Media File Format" from H.264-AVC-ISO_IEC_14496-15.pdf, page 20
         if ((ret = avc_demux_ibmf_format(stream, sample)) != ERROR_SUCCESS) {
            return ret;
         }
         LOGI("hls decode avc payload in ibmf format.");
      }
      else {
         // One or more NALUs (Full frames are required)
         // try  "AnnexB" from H.264-AVC-ISO_IEC_14496-10.pdf, page 211.
         if ((ret = avc_demux_annexb_format(stream, sample)) != ERROR_SUCCESS) {
            // ok, we guess out the payload is annexb, but maybe changed to ibmf.
            if (ret != ERROR_NOT_ANNEXB) {
               LOGE("avc demux for annexb failed. ret=%d", ret);
               return ret;
            }
            
            // try "ISO Base Media File Format" from H.264-AVC-ISO_IEC_14496-15.pdf, page 20
            if ((ret = avc_demux_ibmf_format(stream, sample)) != ERROR_SUCCESS) {
               return ret;
            }
            else {
               payload_format = AvcPayloadFormatIbmf;
               LOGW("hls avc payload change from annexb to ibmf format.");
            }
         }
         LOGI("hls decode avc payload in annexb format.");
      }
   }
   else {
      // ignored.
   }
   LOGD("avc decoded, type=%d, codec=%d, avc=%d, cts=%d, size=%d",
        frame_type, video_codec_id, avc_packet_type, composition_time, size);
   
   return ret;
}

int FlvTagDemuxer::audio_aac_sequence_header_demux(char* data, int size)
{
   int ret = 0;
   
   if ((ret = stream->initialize(data, size)) != ERROR_SUCCESS) {
      return ret;
   }
   
   // only need to decode the first 2bytes:
   //      audioObjectType, aac_profile, 5bits.
   //      samplingFrequencyIndex, aac_sample_rate, 4bits.
   //      channelConfiguration, aac_channels, 4bits
   if (!stream->require(2)) {
      ret = -1;
      LOGE("audio codec decode aac sequence header failed. ret=%d", ret);
      return ret;
   }
   uint8_t profile_ObjectType = stream->read_1bytes();
   uint8_t samplingFrequencyIndex = stream->read_1bytes();
   
   aac_channels = (samplingFrequencyIndex >> 3) & 0x0f;
   samplingFrequencyIndex = ((profile_ObjectType << 1) & 0x0e) | ((samplingFrequencyIndex >> 7) & 0x01);
   profile_ObjectType = (profile_ObjectType >> 3) & 0x1f;
   
   // set the aac sample rate.
   aac_sample_rate = samplingFrequencyIndex;
   
   // convert the object type in sequence header to aac profile of ADTS.
   aac_object = (AacObjectType)profile_ObjectType;
   if (aac_object == AacObjectTypeReserved) {
      ret = -1;
      LOGE("audio codec decode aac sequence header failed, "
           "adts object=%d invalid. ret=%d", profile_ObjectType, ret);
      return ret;
   }
   
   // TODO: FIXME: to support aac he/he-v2, see: ngx_rtmp_codec_parse_aac_header
   // @see: https://github.com/winlinvip/nginx-rtmp-module/commit/3a5f9eea78fc8d11e8be922aea9ac349b9dcbfc2
   //
   // donot force to LC, @see: https://github.com/ossrs/srs/issues/81
   // the source will print the sequence header info.
   //if (aac_profile > 3) {
   // Mark all extended profiles as LC
   // to make Android as happy as possible.
   // @see: ngx_rtmp_hls_parse_aac_header
   //aac_profile = 1;
   //}
   
   return ret;
}

int FlvTagDemuxer::avc_demux_sps_pps(ByteStream* stream)
{
   int ret = ERROR_SUCCESS;
   
   // AVCDecoderConfigurationRecord
   // 5.2.4.1.1 Syntax, H.264-AVC-ISO_IEC_14496-15.pdf, page 16
   avc_extra_size = stream->size() - stream->pos();
   if (avc_extra_size > 0) {
      delete[] avc_extra_data;
      avc_extra_data = new char[avc_extra_size];
      memcpy(avc_extra_data, stream->data() + stream->pos(), avc_extra_size);
   }
   
   if (!stream->require(6)) {
      ret = -1;
      LOGE("avc decode sequenc header failed. ret=%d", ret);
      return ret;
   }
   //int8_t configurationVersion = stream->read_1bytes();
   stream->read_1bytes();
   //int8_t AVCProfileIndication = stream->read_1bytes();
   avc_profile = (AvcProfile)stream->read_1bytes();
   //int8_t profile_compatibility = stream->read_1bytes();
   stream->read_1bytes();
   //int8_t AVCLevelIndication = stream->read_1bytes();
   avc_level = (AvcLevel)stream->read_1bytes();
   
   // parse the NALU size.
   int8_t lengthSizeMinusOne = stream->read_1bytes();
   lengthSizeMinusOne &= 0x03;
   NAL_unit_length = lengthSizeMinusOne;
   
   // 5.3.4.2.1 Syntax, H.264-AVC-ISO_IEC_14496-15.pdf, page 16
   // 5.2.4.1 AVC decoder configuration record
   // 5.2.4.1.2 Semantics
   // The value of this field shall be one of 0, 1, or 3 corresponding to a
   // length encoded with 1, 2, or 4 bytes, respectively.
   if (NAL_unit_length == 2) {
      ret = -1;
      LOGE("sps lengthSizeMinusOne should never be 2. ret=%d", ret);
      return ret;
   }
   
   // 1 sps, 7.3.2.1 Sequence parameter set RBSP syntax
   // H.264-AVC-ISO_IEC_14496-10.pdf, page 45.
   if (!stream->require(1)) {
      ret = -1;
      LOGE("avc decode sequenc header sps failed. ret=%d", ret);
      return ret;
   }
   int8_t numOfSequenceParameterSets = stream->read_1bytes();
   numOfSequenceParameterSets &= 0x1f;
   if (numOfSequenceParameterSets != 1) {
      ret = -1;
      LOGE("avc decode sequenc header sps failed. ret=%d", ret);
      return ret;
   }
   if (!stream->require(2)) {
      ret = -1;
      LOGE("avc decode sequenc header sps size failed. ret=%d", ret);
      return ret;
   }
   sequenceParameterSetLength = stream->read_2bytes();
   if (!stream->require(sequenceParameterSetLength)) {
      ret = -1;
      LOGE("avc decode sequenc header sps data failed. ret=%d", ret);
      return ret;
   }
   if (sequenceParameterSetLength > 0) {
      delete [] sequenceParameterSetNALUnit;
      sequenceParameterSetNALUnit = new char[sequenceParameterSetLength];
      stream->read_bytes(sequenceParameterSetNALUnit, sequenceParameterSetLength);
   }
   // 1 pps
   if (!stream->require(1)) {
      ret = -1;
      LOGE("avc decode sequenc header pps failed. ret=%d", ret);
      return ret;
   }
   int8_t numOfPictureParameterSets = stream->read_1bytes();
   numOfPictureParameterSets &= 0x1f;
   if (numOfPictureParameterSets != 1) {
      ret = -1;
      LOGE("avc decode sequenc header pps failed. ret=%d", ret);
      return ret;
   }
   if (!stream->require(2)) {
      ret = -1;
      LOGE("avc decode sequenc header pps size failed. ret=%d", ret);
      return ret;
   }
   pictureParameterSetLength = stream->read_2bytes();
   if (!stream->require(pictureParameterSetLength)) {
      ret = -1;
      LOGE("avc decode sequenc header pps data failed. ret=%d", ret);
      return ret;
   }
   if (pictureParameterSetLength > 0) {
      delete [] pictureParameterSetNALUnit;
      pictureParameterSetNALUnit = new char[pictureParameterSetLength];
      stream->read_bytes(pictureParameterSetNALUnit, pictureParameterSetLength);
   }
   
   return avc_demux_sps();
}

int FlvTagDemuxer::avc_demux_sps()
{
   int ret = ERROR_SUCCESS;
   
   if (sequenceParameterSetLength < 2) {
      LOGE("secuence header need at lest 1 byte");
      return -1;
   }
   
   int8_t nutv = sequenceParameterSetNALUnit[0];
   
   // forbidden_zero_bit shall be equal to 0.
   int8_t forbidden_zero_bit = (nutv >> 7) & 0x01;
   if (forbidden_zero_bit) {
      ret = -1;
      LOGE("forbidden_zero_bit shall be equal to 0. ret=%d", ret);
      return ret;
   }
   
   // nal_ref_idc not equal to 0 specifies that the content of the NAL unit contains a sequence parameter set or a picture
   // parameter set or a slice of a reference picture or a slice data partition of a reference picture.
   int8_t nal_ref_idc = (nutv >> 5) & 0x03;
   if (!nal_ref_idc) {
      ret = -1;
      LOGE("for sps, nal_ref_idc shall be not be equal to 0. ret=%d", ret);
      return ret;
   }
   
   // 7.4.1 NAL unit semantics
   // H.264-AVC-ISO_IEC_14496-10-2012.pdf, page 61.
   // nal_unit_type specifies the type of RBSP data structure contained in the NAL unit as specified in Table 7-1.
   AvcNaluType nal_unit_type = (AvcNaluType)(nutv & 0x1f);
   if (nal_unit_type != AvcNaluTypeSPS) {
      ret = -1;
      LOGE("for sps, nal_unit_type shall be equal to 7. ret=%d", ret);
      return ret;
   }
   
   get_bit_context con_buf;
   SPS             my_sps;
   memset(&con_buf, 0, sizeof(con_buf));
   memset(&my_sps, 0, sizeof(my_sps));
   con_buf.buf = (uint8_t*)(sequenceParameterSetNALUnit + 1);
   con_buf.buf_size = sequenceParameterSetLength - 1;
   if (h264dec_seq_parameter_set(&con_buf, &my_sps) != 0){
      LOGE("hls codec demux video failed. ret=%d", -1);
      return -1;
   }
   width = h264_get_width(&my_sps);
   height = h264_get_height(&my_sps);
   return ret;
}

int FlvTagDemuxer::avc_demux_annexb_format(ByteStream* stream, AacAvcCodecSample* sample)
{
   int ret = 0;
   
   // not annexb, try others
   if (!avc_startswith_annexb(stream, NULL)) {
      return ERROR_NOT_ANNEXB;
   }
   
   // AnnexB
   // B.1.1 Byte stream NAL unit syntax,
   // H.264-AVC-ISO_IEC_14496-10.pdf, page 211.
   while (!stream->empty()) {
      // find start code
      int nb_start_code = 0;
      if (!avc_startswith_annexb(stream, &nb_start_code)) {
         return ret;
      }
      
      // skip the start code.
      if (nb_start_code > 0) {
         stream->skip(nb_start_code);
      }
      
      // the NALU start bytes.
      char* p = stream->data() + stream->pos();
      
      // get the last matched NALU
      while (!stream->empty()) {
         if (avc_startswith_annexb(stream, NULL)) {
            break;
         }
         
         stream->skip(1);
      }
      
      char* pp = stream->data() + stream->pos();
      
      // skip the empty.
      if (pp - p <= 0) {
         continue;
      }
      
      // got the NALU.
      if ((ret = sample->add_sample_unit(p, pp - p)) != 0) {
         LOGE("annexb add video sample failed. ret=%d", ret);
         return ret;
      }
   }
   
   return ret;
}

int FlvTagDemuxer::avc_demux_ibmf_format(ByteStream* stream, AacAvcCodecSample* sample)
{
   int ret = ERROR_SUCCESS;
   
   int PictureLength = stream->size() - stream->pos();
   
   // 5.3.4.2.1 Syntax, H.264-AVC-ISO_IEC_14496-15.pdf, page 16
   // 5.2.4.1 AVC decoder configuration record
   // 5.2.4.1.2 Semantics
   // The value of this field shall be one of 0, 1, or 3 corresponding to a
   // length encoded with 1, 2, or 4 bytes, respectively.
   assert(NAL_unit_length != 2);
   
   // 5.3.4.2.1 Syntax, H.264-AVC-ISO_IEC_14496-15.pdf, page 20
   for (int i = 0; i < PictureLength;) {
      // unsigned int((NAL_unit_length+1)*8) NALUnitLength;
      if (!stream->require(NAL_unit_length + 1)) {
         ret = -1;
         LOGE("avc decode NALU size failed. ret=%d", ret);
         return ret;
      }
      int32_t NALUnitLength = 0;
      if (NAL_unit_length == 3) {
         NALUnitLength = stream->read_4bytes();
      }else if (NAL_unit_length == 1) {
         NALUnitLength = stream->read_2bytes();
      }
      else {
         NALUnitLength = stream->read_1bytes();
      }
      
      // maybe stream is invalid format.
      // see: https://github.com/ossrs/srs/issues/183
      if (NALUnitLength < 0) {
         ret = -1;
         LOGE("maybe stream is AnnexB format. ret=%d", ret);
         return ret;
      }
      
      // NALUnit
      if (!stream->require(NALUnitLength)) {
         ret = -1;
         LOGE("avc decode NALU data failed. ret=%d", ret);
         return ret;
      }
      // 7.3.1 NAL unit syntax, H.264-AVC-ISO_IEC_14496-10.pdf, page 44.
      if ((ret = sample->add_sample_unit(stream->data() + stream->pos(), NALUnitLength)) != ERROR_SUCCESS) {
         LOGE("avc add video sample failed. ret=%d", ret);
         return ret;
      }
      stream->skip(NALUnitLength);
      
      i += NAL_unit_length + 1 + NALUnitLength;
   }
   return ret;
}

