#ifndef __RTMP_FLV_UTILITY_H__
#define __RTMP_FLV_UTILITY_H__

#include "byte_stream.h"
/**********************************************************************
  *sound keys  ref video_file_format_spec_v10_1.pdf E.4.2 Audio Tags
*/
enum FlvSoundFormat
{
	// set to the max value to reserved, for array map.
	FlvSoundFormatReserved1 = 16,

	// for user to disable audio, for example, use pure video hls.
	FlvSoundFormatDisabled = 17,
	FlvSoundFormatLinearPCMPlatformEndian = 0,
	FlvSoundFormatADPCM = 1,
	FlvSoundFormatMP3 = 2,
	FlvSoundFormatLinearPCMLittleEndian = 3,
	FlvSoundFormatNellymoser16kHzMono = 4,
	FlvSoundFormatNellymoser8kHzMono = 5,
	FlvSoundFormatNellymoser = 6,
	FlvSoundFormatReservedG711AlawLogarithmicPCM = 7,
	FlvSoundFormatReservedG711MuLawLogarithmicPCM = 8,
	FlvSoundFormatReserved = 9,
	FlvSoundFormatAAC = 10,
	FlvSoundFormatSpeex = 11,
	FlvSoundFormatReservedMP3_8kHz = 14,
	FlvSoundFormatReservedDeviceSpecificSound = 15,
};

enum FlvSoundSampleRate
{
	// set to the max value to reserved, for array map.
	FlvSoundSampleRateReserved = 4,

	FlvSoundSampleRate5512 = 0,
	FlvSoundSampleRate11025 = 1,
	FlvSoundSampleRate22050 = 2,
	FlvSoundSampleRate44100 = 3,
};

enum FlvSoundSampleSize
{
	// set to the max value to reserved, for array map.
	FlvSoundSampleSizeReserved = 2,

	FlvSoundSampleSize8bit = 0,
	FlvSoundSampleSize16bit = 1,
};

enum FlvSoundType
{
	// set to the max value to reserved, for array map.
	FlvSoundTypeReserved = 2,

	FlvSoundTypeMono = 0,
	FlvSoundTypeStereo = 1,
};

enum FlvAACPacketType
{
	// set to the max value to reserved, for array map.
	FlvAACPacketTypeReserved = 2,

	FlvAACPacketTypeSequenceHeader = 0,
	FlvAACPacketTypeRawData = 1,
};

/**********************************************************************
* video keys  ref video_file_format_spec_v10_1.pdf E.4.3 Video Tags
*/
enum FlvVideoFrameType
{
	// set to the zero to reserved, for array map.
	FlvVideoFrameTypeReserved = 0,
	FlvVideoFrameTypeReserved1 = 6,

	FlvVideoFrameTypeKeyFrame = 1,
	FlvVideoFrameTypeInterFrame = 2,
	FlvVideoFrameTypeDisposableInterFrame = 3,
	FlvVideoFrameTypeGeneratedKeyFrame = 4,
	FlvVideoFrameTypeVideoInfoFrame = 5,
};

enum FlvVideoCodecID
{
	// set to the zero to reserved, for array map.
	FlvVideoCodecIDReserved = 0,
	FlvVideoCodecIDReserved1 = 1,
	FlvVideoCodecIDReserved2 = 9,

	// for user to disable video, for example, use pure audio hls.
	FlvVideoCodecIDDisabled = 8,

	FlvVideoCodecIDSorensonH263 = 2,
	FlvVideoCodecIDScreenVideo = 3,
	FlvVideoCodecIDOn2VP6 = 4,
	FlvVideoCodecIDOn2VP6WithAlphaChannel = 5,
	FlvVideoCodecIDScreenVideoVersion2 = 6,
	FlvVideoCodecIDAVC = 7,
};


enum FlvVideoAVCPacketType
{
	// set to the max value to reserved, for array map.
	FlvVideoAVCPacketTypeReserved = 3,

	FlvVideoAVCPacketTypeSequenceHeader = 0,
	FlvVideoAVCPacketTypeNALU = 1,
	FlvVideoAVCPacketTypeSequenceHeaderEOF = 2,
};


/**
* ref video_file_format_spec_v10_1.pdf E.4.1 FLV Tag, page 75
*/
enum FlvTagType
{
	// set to the zero to reserved, for array map.
	FlvTagTypeReserved = 0,

	// 8 = audio
	FlvTagTypeAudio = 8,
	// 9 = video
	FlvTagTypeVideo = 9,
	// 18 = script data
	FlvTagTypeScript = 18,
};


/**
* the profile for avc/h.264.
* @see Annex A Profiles and levels, H.264-AVC-ISO_IEC_14496-10.pdf, page 205.
*/
enum AvcProfile
{
	AvcProfileReserved = 0,

	// @see ffmpeg, libavcodec/avcodec.h:2713
	AvcProfileBaseline = 66,
	// FF_PROFILE_H264_CONSTRAINED  (1<<9)  // 8+1; constraint_set1_flag
	// FF_PROFILE_H264_CONSTRAINED_BASELINE (66|FF_PROFILE_H264_CONSTRAINED)
	AvcProfileConstrainedBaseline = 578,
	AvcProfileMain = 77,
	AvcProfileExtended = 88,
	AvcProfileHigh = 100,
	AvcProfileHigh10 = 110,
	AvcProfileHigh10Intra = 2158,
	AvcProfileHigh422 = 122,
	AvcProfileHigh422Intra = 2170,
	AvcProfileHigh444 = 144,
	AvcProfileHigh444Predictive = 244,
	AvcProfileHigh444Intra = 2192,
};


/**
* the level for avc/h.264.
* @see Annex A Profiles and levels, H.264-AVC-ISO_IEC_14496-10.pdf, page 207.
*/
enum AvcLevel
{
	AvcLevelReserved = 0,

	AvcLevel_1 = 10,
	AvcLevel_11 = 11,
	AvcLevel_12 = 12,
	AvcLevel_13 = 13,
	AvcLevel_2 = 20,
	AvcLevel_21 = 21,
	AvcLevel_22 = 22,
	AvcLevel_3 = 30,
	AvcLevel_31 = 31,
	AvcLevel_32 = 32,
	AvcLevel_4 = 40,
	AvcLevel_41 = 41,
	AvcLevel_5 = 50,
	AvcLevel_51 = 51,
};

enum AvcPayloadFormat
{
	AvcPayloadFormatGuess = 0,
	AvcPayloadFormatAnnexb,
	AvcPayloadFormatIbmf,
};

enum AvcNaluType
{
	// Unspecified
	AvcNaluTypeReserved = 0,

	// Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
	AvcNaluTypeNonIDR = 1,
	// Coded slice data partition A slice_data_partition_a_layer_rbsp( )
	AvcNaluTypeDataPartitionA = 2,
	// Coded slice data partition B slice_data_partition_b_layer_rbsp( )
	AvcNaluTypeDataPartitionB = 3,
	// Coded slice data partition C slice_data_partition_c_layer_rbsp( )
	AvcNaluTypeDataPartitionC = 4,
	// Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
	AvcNaluTypeIDR = 5,
	// Supplemental enhancement information (SEI) sei_rbsp( )
	AvcNaluTypeSEI = 6,
	// Sequence parameter set seq_parameter_set_rbsp( )
	AvcNaluTypeSPS = 7,
	// Picture parameter set pic_parameter_set_rbsp( )
	AvcNaluTypePPS = 8,
	// Access unit delimiter access_unit_delimiter_rbsp( )
	AvcNaluTypeAccessUnitDelimiter = 9,
	// End of sequence end_of_seq_rbsp( )
	AvcNaluTypeEOSequence = 10,
	// End of stream end_of_stream_rbsp( )
	AvcNaluTypeEOStream = 11,
	// Filler data filler_data_rbsp( )
	AvcNaluTypeFilterData = 12,
	// Sequence parameter set extension seq_parameter_set_extension_rbsp( )
	AvcNaluTypeSPSExt = 13,
	// Prefix NAL unit prefix_nal_unit_rbsp( )
	AvcNaluTypePrefixNALU = 14,
	// Subset sequence parameter set subset_seq_parameter_set_rbsp( )
	AvcNaluTypeSubsetSPS = 15,
	// Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
	AvcNaluTypeLayerWithoutPartition = 19,
	// Coded slice extension slice_layer_extension_rbsp( )
	AvcNaluTypeCodedSliceExt = 20,
};

/**
* the aac object type, for RTMP sequence header
* for AudioSpecificConfig, @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 33
* for audioObjectType, @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
*/
enum AacObjectType
{
	AacObjectTypeReserved = 0,

	// Table 1.1 - Audio Object Type definition
	// @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	AacObjectTypeAacMain = 1,
	AacObjectTypeAacLC = 2,
	AacObjectTypeAacSSR = 3,

	// AAC HE = LC+SBR
	AacObjectTypeAacHE = 5,
	// AAC HEv2 = LC+SBR+PS
	AacObjectTypeAacHEV2 = 29,
};

class CodecSampleUnit
{
public:
	/**
	* the sample bytes is directly ptr to packet bytes,
	* user should never use it when packet destroyed.
	*/
	int size;
	char* bytes;
public:
	CodecSampleUnit();
	virtual ~CodecSampleUnit();
};

#define AAC_AVC_MAX_CODEC_SAMPLE  128
#define AAC_SAMPLE_RATE_UNSET 15

class AacAvcCodecSample
{
public:
	/**
	* each audio/video raw data packet will dumps to one or multiple buffers,
	* the buffers will write to hls and clear to reset.
	* generally, aac audio packet corresponding to one buffer,
	* where avc/h264 video packet may contains multiple buffer.
	*/
	int nb_sample_units;
	CodecSampleUnit sample_units[AAC_AVC_MAX_CODEC_SAMPLE];
public:
	/**
	* whether the sample is video sample which demux from video packet.
	*/
	bool is_video;
	/**
	* CompositionTime, video_file_format_spec_v10_1.pdf, page 78.
	* cts = pts - dts, where dts = flvheader->timestamp.
	*/
	int32_t cts;
public:
	// video specified
	FlvVideoFrameType frame_type;
	FlvVideoAVCPacketType avc_packet_type;
	// whether sample_units contains IDR frame.
	bool has_idr;
	AvcNaluType first_nalu_type;
public:
	// audio specified
	FlvSoundFormat acodec;
	// audio aac specified.
	FlvSoundSampleRate sound_rate;
	FlvSoundSampleSize sound_size;
	FlvSoundType sound_type;
	FlvAACPacketType aac_packet_type;
public:
	AacAvcCodecSample();
	virtual ~AacAvcCodecSample();
public:
	/**
	* clear all samples.
	* the sample units never copy the bytes, it directly use the ptr,
	* so when video/audio packet is destroyed, the sample must be clear.
	* in a word, user must clear sample before demux it.
	* @remark demux sample use SrsAvcAacCodec.audio_aac_demux or video_avc_demux.
	*/
	void clear();
	/**
	* add the a sample unit, it's a h.264 NALU or aac raw data.
	* the sample unit directly use the ptr of packet bytes,
	* so user must never use sample unit when packet is destroyed.
	* in a word, user must clear sample before demux it.
	*/
	int add_sample_unit(char* bytes, int size);
};

enum MetadataType{
   MetadataTypeOnMetaDataNone = -1,
   MetadataTypeOnMetaData = 0,
   MetadataTypeOnCuePoint = 1
};

#define kOnCuePointAmfIdMsg           "id"
#define kOnCuePointAmfTypeMsg         "type"
#define kOnCuePointAmfContentMsg      "content"

class CuePointAmfMsg{
public:
   CuePointAmfMsg();
   ~CuePointAmfMsg();
   std::string ToJsonStr();
   void Reset();
   std::string mType;
   std::string mContent;
   std::string mId;
};

class FlvTagDemuxer{
public:
private:
	ByteStream* stream;
public:
	/**
	* metadata specified
	*/
	int             duration;
	int             width;
	int             height;
	int             frame_rate;
   int             audio_samplerate;
   int             audio_channels;
   int             audio_sample_size;
	// @see: SrsCodecVideo
	int             video_codec_id;
	int             video_data_rate; // in bps
	// @see: SrsCodecAudioType
	int             audio_codec_id;
	int             audio_data_rate; // in bps
   CuePointAmfMsg  cuepoint_amf_msg;
   enum MetadataType metadata_type;
public:
	/**
	* video specified
	*/
	// profile_idc, H.264-AVC-ISO_IEC_14496-10.pdf, page 45.
	AvcProfile   avc_profile;
	// level_idc, H.264-AVC-ISO_IEC_14496-10.pdf, page 45.
	AvcLevel     avc_level;
	// lengthSizeMinusOne, H.264-AVC-ISO_IEC_14496-15.pdf, page 16
	int8_t          NAL_unit_length;
	uint16_t        sequenceParameterSetLength;
	char*           sequenceParameterSetNALUnit;
	uint16_t       pictureParameterSetLength;
	char*           pictureParameterSetNALUnit;
private:
	// the avc payload format.
	AvcPayloadFormat payload_format;
public:
	/**
	* audio specified
	* audioObjectType, in 1.6.2.1 AudioSpecificConfig, page 33,
	* 1.5.1.1 Audio object type definition, page 23,
	*           in aac-mp4a-format-ISO_IEC_14496-3+2001.pdf.
	*/
	AacObjectType    aac_object;
	/**
	* samplingFrequencyIndex
	*/
	uint8_t        aac_sample_rate;
	/**
	* channelConfiguration
	*/
	uint8_t        aac_channels;
public:
	/**
	* the avc extra data, the AVC sequence header,
	* without the flv codec header,
	* @see: ffmpeg, AVCodecContext::extradata
	*/
	int             avc_extra_size;
	char*           avc_extra_data;
	/**
	* the aac extra data, the AAC sequence header,
	* without the flv codec header,
	* @see: ffmpeg, AVCodecContext::extradata
	*/
	int             aac_extra_size;
	char*           aac_extra_data;
public:
	// for sequence header, whether parse the h.264 sps.
	bool            avc_parse_sps;
public:
	FlvTagDemuxer();
	virtual ~FlvTagDemuxer();
public:
	/**
	* demux the metadata, to to get the stream info,
	* for instance, the width/height, sample rate.
	* @param metadata, the metadata amf0 object. assert not NULL.
	*/
	//for rtmp
	virtual int metadata_demux(char *data, int size);

	//for flv script
	virtual int metadata_demux2(char *data, int size);

	// whether avc or aac codec sequence header or extra data is decoded ok.
	virtual bool is_avc_codec_ok();
	virtual bool is_aac_codec_ok();
	// the following function used for hls to build the sample and codec.
public:
	/**
	* demux the audio packet in aac codec.
	* the packet mux in FLV/RTMP format defined in flv specification.
	* demux the audio speicified data(sound_format, sound_size, ...) to sample.
	* demux the aac specified data(aac_profile, ...) to codec from sequence header.
	* demux the aac raw to sample units.
	*/
	virtual int audio_aac_demux(char* data, int size, AacAvcCodecSample* sample);
	virtual int audio_mp3_demux(char* data, int size, AacAvcCodecSample* sample);
	/**
	* demux the video packet in h.264 codec.
	* the packet mux in FLV/RTMP format defined in flv specification.
	* demux the video specified data(frame_type, codec_id, ...) to sample.
	* demux the h.264 sepcified data(avc_profile, ...) to codec from sequence header.
	* demux the h.264 NALUs to sampe units.
	*/
	virtual int video_avc_demux(char* data, int size, AacAvcCodecSample* sample);
public:
	/**
	* directly demux the sequence header, without RTMP packet header.
	*/
	virtual int audio_aac_sequence_header_demux(char* data, int size);
private:
	/**
	* when avc packet type is SrsCodecVideoAVCTypeSequenceHeader,
	* decode the sps and pps.
	*/
	virtual int avc_demux_sps_pps(ByteStream* stream);
	/**
	* decode the sps rbsp stream.
	*/
	virtual int avc_demux_sps();
	//virtual int avc_demux_sps_rbsp(char* rbsp, int nb_rbsp);
	/**
	* demux the avc NALU in "AnnexB"
	* from H.264-AVC-ISO_IEC_14496-10.pdf, page 211.
	*/
	virtual int avc_demux_annexb_format(ByteStream* stream, AacAvcCodecSample* sample);
	/**
	* demux the avc NALU in "ISO Base Media File Format"
	* from H.264-AVC-ISO_IEC_14496-15.pdf, page 20
	*/
	virtual int avc_demux_ibmf_format(ByteStream* stream, AacAvcCodecSample* sample);

};

#endif //__RTMP_FLV_UTILITY_H__
