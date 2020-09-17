#pragma once

#include <vector>
#include <string>
#include <functional>

namespace DShow {
   struct VideoConfig;
   struct AudioConfig;
   typedef std::function<void(const VideoConfig &config, unsigned char *data,
      size_t size, long long startTime, long long stopTime,
      long rotation)>
      VideoProc;

   typedef std::function<void(const AudioConfig &config, unsigned char *data,
      size_t size, long long startTime, long long stopTime)>
      AudioProc;

   enum class DeviceInputType {
      Audio_Input,
      Video_Input,
   };

   
   enum class InitGraph {
      False,
      True,
   };

   /** DirectShow configuration dialog type */
   enum class DialogType {
      ConfigVideo,
      ConfigAudio,
      ConfigCrossbar,
      ConfigCrossbar2,
   };

   enum class VideoFormat {
      Any,
      Unknown,

      /* raw formats */
      ARGB = 100,
      XRGB,

      /* planar YUV formats */
      I420 = 200,
      NV12,
      YV12,
      Y800,

      /* packed YUV formats */
      YVYU = 300,
      YUY2,
      UYVY,
      HDYC,

      /* encoded formats */
      MJPEG = 400,
      H264,
   };

   enum class AudioFormat {
      Any,
      Unknown,

      /* raw formats */
      Wave16bit = 100,
      WaveFloat,

      /* encoded formats */
      AAC = 200,
      AC3,
      MPGA, /* MPEG 1 */
   };

   enum class AudioMode {
      Capture,
      DirectSound,
      WaveOut,
   };

   enum class Result {
      Success,
      InUse,
      Error,
   };

   struct VideoInfo {
      int minCX, minCY;
      int maxCX, maxCY;
      int granularityCX, granularityCY;
      long long minInterval, maxInterval;
      VideoFormat format;
   };

   struct AudioInfo {
      int minChannels, maxChannels;
      int channelsGranularity;
      int minSampleRate, maxSampleRate;
      int sampleRateGranularity;
      AudioFormat format;
   };

   struct DeviceId {
      std::wstring name;
      std::wstring path;
   };

   struct VideoDevice : DeviceId {
      bool audioAttached = false;
      bool separateAudioFilter = false;
      std::vector<VideoInfo> caps;
   };

   struct AudioDevice : DeviceId {
      std::vector<AudioInfo> caps;
   };

   struct Config : DeviceId {
      /** Use the device's desired default config */
      bool useDefaultConfig = true;
   };


   struct VideoConfig : Config {
      VideoProc callback;
      
      VideoProc recvVideoCallback;

      /** Desired width/height of video. */
      int cx = 0, cy_abs = 0;

      /** Whether or not cy was negative. */
      bool cy_flip = false;

      /** Desired frame interval (in 100-nanosecond units) */
      long long frameInterval = 0;

      /** Internal video format. */
      VideoFormat internalFormat = VideoFormat::Any;

      /** Desired video format. */
      VideoFormat format = VideoFormat::Any;

   };

   struct AudioConfig : Config {
      AudioProc callback;

      /**
          * Use the audio attached to the video device
          *
          * (name/path memeber variables will be ignored)
          */
      bool useVideoDevice = false;

      /** Use separate filter for audio */
      bool useSeparateAudioFilter = false;

      /** Desired sample rate */
      int sampleRate = 0;

      /** Desired channels */
      int channels = 0;

      /** Desired audio format */
      AudioFormat format = AudioFormat::Any;

      /** Audio playback mode */
      AudioMode mode = AudioMode::Capture;
   };
}
