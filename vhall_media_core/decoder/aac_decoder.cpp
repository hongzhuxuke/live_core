#include "aac_decoder.h"
#include "../common/vhall_log.h"

#define SUPPORT_AUDIO_CHANNEL       2
#define SUPPORT_AUDIO_SAMPLE_RATE   44100
#define SUPPORT_AUDIO_SAMPLE_FMT    AV_SAMPLE_FMT_S16

AACDecoder::AACDecoder(AudioParam *audioParam){
   mDecodedFrame = NULL;
   mCodecCtx = NULL;
   mCodec = NULL;
   memset((void*)&mAudioParam, 0, sizeof(AudioParam));
   mAudioParam = *audioParam;
   
   if(audioParam&& audioParam->extra_size > 0){
      mAudioParam.extra_size = audioParam->extra_size;
      mAudioParam.extra_data = (char*)calloc(1,audioParam->extra_size);
      if (mAudioParam.extra_data)
         memcpy(mAudioParam.extra_data, audioParam->extra_data, audioParam->extra_size);
   }
   mSwrCtx = NULL;
   mDstAudioFmt = SUPPORT_AUDIO_SAMPLE_FMT;
}


AACDecoder::~AACDecoder() {
   
   VHALL_DEL(mAudioParam.extra_data);
   if (mCodecCtx) {
      if (mCodecCtx->extradata) {
         av_free(mCodecCtx->extradata);
         mCodecCtx->extradata = NULL;
      }
      avcodec_close(mCodecCtx);
      av_free(mCodecCtx);
      mCodecCtx = NULL;
   }
   if (mDecodedFrame) {
      av_freep(&mDecodedFrame);
      mDecodedFrame = NULL;
   }
   VHALL_DEL(mAudioBuff);
   if(mAudiofifo){
      av_fifo_free(mAudiofifo);
      mAudiofifo = NULL;
   }
   if(mSwrCtx){
      swr_free(&mSwrCtx);
   }
}

bool AACDecoder::Init(void) {
   mCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
   if (!mCodec) {
      return false;
   }
   
   mCodecCtx = avcodec_alloc_context3(mCodec);
   if (!mCodecCtx) {
      return false;
   }
   if(mAudioParam.extra_data){
      mCodecCtx->extradata = (uint8_t*)av_mallocz(mAudioParam.extra_size);
      if(mCodecCtx->extradata)
         memcpy(mCodecCtx->extradata, mAudioParam.extra_data, mAudioParam.extra_size);
      mCodecCtx->extradata_size = mAudioParam.extra_size;
   }else{
      mCodecCtx->channels = mChannels;
      mCodecCtx->sample_rate = mSampleRate;
   }
   if (avcodec_open2(mCodecCtx, mCodec, NULL) < 0) {
      return false;
   }
   mDecodedFrame = av_frame_alloc();
   if (!mDecodedFrame) {
      return false;
   }
   mSwrCtx = NULL;
   VHALL_DEL(mAudioParam.extra_data);
   memset((void*)&mAudioParam, 0, sizeof(AudioParam));
   AVDictionary *opts      = NULL;
   if (!mCodec ||    avcodec_open2(mCodecCtx, mCodec, &opts) < 0)
      return -1;
   mAudioBuffSize =  19200*4;
   mAudioBuff = (unsigned char*)malloc(mAudioBuffSize);
   mAudiofifo = av_fifo_alloc(1000);
   
   return true;
}

int AACDecoder::Decode( unsigned char *data, int size) {
   int resampledDataSize      = 0;
   AVPacket pkt;
   av_init_packet(&pkt);
   pkt.data = data;
   pkt.size = size;
   int align = 1;
   int len1 =0;
   int len2                   = 0;
   uint8_t *audioBuf;
   int fifoDataSize = 0;
   int decodedDataSize      = 0;
   int got_frame = 0;
   while(pkt.size > 0 && pkt.data ){
      len1 = avcodec_decode_audio4(mCodecCtx,mDecodedFrame, &got_frame, &pkt);
      if(len1<0){
         //error, get the next packet
         LOGE("avcodec_decode_audio4 failed.");
         break;
      }
      pkt.data += len1;
      pkt.size -= len1;
      if(!got_frame)
         continue;
#if defined(ANDROID)
      align = 1;
#endif
      decodedDataSize = av_samples_get_buffer_size(NULL, mDecodedFrame->channels,
                                                   mDecodedFrame->nb_samples,
                                                   (AVSampleFormat)mDecodedFrame->format, align);
      if (decodedDataSize <= 0) {
         LOGE("av_samples_get_buffer_size failed");
         break;
      }
      //LOGD("AAC decoder  %d data .", decodedDataSize);
      resampledDataSize = decodedDataSize;
      
      if(mSwrCtx == NULL){
         if(mCodecCtx->sample_fmt != SUPPORT_AUDIO_SAMPLE_FMT
            || mCodecCtx->sample_rate != SUPPORT_AUDIO_SAMPLE_RATE
            || mCodecCtx->channels != SUPPORT_AUDIO_CHANNEL){
            int64_t decChannelLayout   = 0;
            decChannelLayout =   (mCodecCtx->channel_layout
                                  && mCodecCtx->channels == av_get_channel_layout_nb_channels(mCodecCtx->channel_layout))
            ? mCodecCtx->channel_layout : av_get_default_channel_layout(mCodecCtx->channels);
            
            int64_t audioDstChannelLayout = av_get_default_channel_layout(SUPPORT_AUDIO_CHANNEL);
            if (mCodecCtx->sample_fmt != mDstAudioFmt
                || mCodecCtx->sample_rate != mSampleRate
                || audioDstChannelLayout != decChannelLayout){
               //resample the audio data
               mSwrCtx = swr_alloc_set_opts(NULL,
                                            audioDstChannelLayout,
                                            mDstAudioFmt,
                                            SUPPORT_AUDIO_SAMPLE_RATE,
                                            decChannelLayout,
                                            mCodecCtx->sample_fmt,
                                            mCodecCtx->sample_rate,
                                            0,
                                            NULL);
               if (mSwrCtx == NULL || swr_init(mSwrCtx) < 0) {
                  LOGE("AACDecoder::Decode  swr_init() failed");
                  return -1;
               }
               LOGD("AACDecoder::Decode  swr_init() success.");
            }
         }
      }
      if (mSwrCtx) {
         const uint8_t **in = (const uint8_t **)mDecodedFrame->extended_data;
         uint8_t *out[] = {mAudiobuf2};
         len2 = swr_convert(
                            mSwrCtx,
                            out,
                            sizeof(mAudiobuf2) /SUPPORT_AUDIO_CHANNEL / av_get_bytes_per_sample(mDstAudioFmt),
                            in,
                            mDecodedFrame->nb_samples);
         if (len2 < 0) {
            LOGE("AACDecoder::Decode  audio_resample() failed");
            return -1;
         }
         if (len2 == sizeof(mAudiobuf2) / SUPPORT_AUDIO_CHANNEL / av_get_bytes_per_sample(mDstAudioFmt)) {
            LOGW("AACDecoder::Decode audio buffer is probably too small");
            swr_init(mSwrCtx);
         }
         audioBuf = mAudiobuf2;
         resampledDataSize = len2 * SUPPORT_AUDIO_CHANNEL * av_get_bytes_per_sample(mDstAudioFmt);
      } else {
         audioBuf = mDecodedFrame->data[0];
         resampledDataSize = decodedDataSize;
      }
      fifoDataSize = av_fifo_size(mAudiofifo);
      if (av_fifo_realloc2(mAudiofifo, fifoDataSize + resampledDataSize) < 0) {
         LOGE("AACDecoder::Decode  av_fifo_realloc2() failed\n");
         return 0;
      }
      //LOGD("AAC decoder append %d data to list.", resampledDataSize);
      av_fifo_generic_write(mAudiofifo, audioBuf, resampledDataSize, NULL);
   }
   //write the data to out bufer
   fifoDataSize = av_fifo_size(mAudiofifo);
   mAudioParam.bitsPerSample = 16;
   mAudioParam.numOfChannels = SUPPORT_AUDIO_CHANNEL;
   mAudioParam.samplesPerSecond = SUPPORT_AUDIO_SAMPLE_RATE;
   //mAudioParam.frame_size = mCodecCtx->frame_size;
   return fifoDataSize;
}

bool AACDecoder::GetDecodecData(unsigned char * decoded_data, int & decode_size){
   int fifoDataSize = av_fifo_size(mAudiofifo);
   if(fifoDataSize< decode_size){
      return false;
   }else{
      av_fifo_generic_read(mAudiofifo, decoded_data, decode_size, NULL);
      decode_size = av_fifo_size(mAudiofifo);
      return true;
   }
}

AudioParam AACDecoder::GetAudioParam() const{
   return mAudioParam;
}

