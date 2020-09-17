/*
 *  Copyright (C) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#pragma once

#include <vector>
#include <string>
#include <functional>
#include "VH_ConstDeff.h"

#ifdef DSHOWCAPTURE_EXPORTS
	#define DSHOWCAPTURE_EXPORT __declspec(dllexport)
#else
	#define DSHOWCAPTURE_EXPORT
#endif

#define DSHOWCAPTURE_VERSION_MAJOR 0
#define DSHOWCAPTURE_VERSION_MINOR 5
#define DSHOWCAPTURE_VERSION_PATCH 3

#define MAKE_DSHOWCAPTURE_VERSION(major, minor, patch) \
		( (major << 24) | \
		  (minor << 16) | \
		  (patch)       )

#define DSHOWCAPTURE_VERSION MAKE_DSHOWCAPTURE_VERSION( \
		DSHOWCAPTURE_VERSION_MAJOR, \
		DSHOWCAPTURE_VERSION_MINOR, \
		DSHOWCAPTURE_VERSION_PATCH)

#define DSHOW_MAX_PLANES 8

namespace DShow {
   class MediaType;
	/* internal forward */
	struct HDevice;
	struct HVideoEncoder;
	struct VideoConfig;
	struct AudioConfig;

	typedef std::function<
		void (const VideoConfig &config,
			unsigned char *data, size_t size,
			long long startTime, long long stopTime)
		> VideoProc;

	typedef std::function<
		void (const AudioConfig &config,
			unsigned char *data, size_t size,
			long long startTime, long long stopTime)
		> AudioProc;

	enum class InitGraph {
		False,
		True
	};

	/** DirectShow configuration dialog type */
	enum class DialogType {
		ConfigVideo,
		ConfigAudio,
		ConfigCrossbar,
		ConfigCrossbar2
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
		MPGA /* MPEG 1 */
	};

	enum class AudioMode {
		Capture,
		DirectSound,
		WaveOut
	};

	enum class Result {
		Success,
		InUse,
		Error
	};

	struct VideoInfo {
		int         minCX, minCY;
		int         maxCX, maxCY;
		int         granularityCX, granularityCY;
		long long   minInterval, maxInterval;
		VideoFormat format;
	};

	struct AudioInfo {
		int         minChannels, maxChannels;
		int         channelsGranularity;
		int         minSampleRate, maxSampleRate;
		int         sampleRateGranularity;
		AudioFormat format;
	};

	struct DeviceId {
		std::wstring name;
		std::wstring path;
	};

	struct VideoDevice : DeviceId {
		bool audioAttached = false;
		std::vector<VideoInfo> caps;
	};

	struct AudioDevice : DeviceId {
		std::vector<AudioInfo> caps;
	};

	struct Config : DeviceId {
		/** Use the device's desired default config */
		bool        useDefaultConfig = true;
      void*       context = NULL;
	};

	struct VideoConfig : Config {
		VideoProc   callback = nullptr;

		/** Desired width/height of video.  */
		int         cx = 0, cy = 0;

		/** Desired frame interval (in 100-nanosecond units) */
		long long   frameInterval = 0;

		/** Internal video format. */
		VideoFormat internalFormat = VideoFormat::Any;

		/** Desired video format. */
		VideoFormat format = VideoFormat::Any;

      bool        isReset;
	};

	struct AudioConfig : Config {
		AudioProc   callback = nullptr;

		/**
		 * Use the audio attached to the video device
		 *
		 * (name/path memeber variables will be ignored)
		 */
		DShowDeviceType  deviceType;
        DShowDevicePinType pinType;

		/** Desired sample rate */
		int         sampleRate = 0;

		/** Desired channels */
		int         channels = 0;

		/** Desired audio format */
		AudioFormat format = AudioFormat::Any;

		/** Audio playback mode */
		AudioMode   mode = AudioMode::Capture;

      bool        isReset;

	};

	class DSHOWCAPTURE_EXPORT Device {
		HDevice *context = nullptr;

	public:
		Device(InitGraph initialize = InitGraph::False);
		~Device();

		bool        Valid() const;
		void        ShutdownGraph();
        //只能改变PIN，不能改变Filter
		bool        SetVideoConfig(VideoConfig *config);
		bool        SetAudioConfig(AudioConfig *config);

		/**
		 * Connects all the configured filters together.
		 *
		 * Call SetVideoConfig and/or SetAudioConfig before using.
		 */
		bool        ConnectFilters();

		Result      Start();
		void        Stop();
        void        ReStart();

		bool        GetVideoConfig(VideoConfig &config) const;
		bool        GetAudioConfig(AudioConfig &config) const;
		bool        GetVideoDeviceId(DeviceId &id) const;
		bool        GetAudioDeviceId(DeviceId &id) const;

		/**
		 * Opens a DirectShow dialog associated with this device
		 *
		 * @param  type  The dialog type
		 */

		static bool EnumVideoDevices(std::vector<VideoDevice> &devices);
		static bool EnumAudioDevices(std::vector<AudioDevice> &devices);

      bool GetVideoMediaType(VideoOutputType *type);
      //获得当前设备的配置信息
      bool GetCurrDeviceFrameInfoList(FrameInfoList *,FrameInfo *);
      //获得设备配置信息列表
      static bool GetDeviceFrameInfoList(DeviceInfo,FrameInfoList *frameList,FrameInfo *currFrame);
      //设置设备默认属性      
      bool SetDeviceDefaultAttribute(int width,int height,int frameInternal,VideoFormat format);
      //设置未激活的设备属性
      static bool SetDeviceDefaultAttribute(DeviceInfo deviceInfo,UINT width,UINT height,int frameInternal,VideoFormat format);
	};

	struct VideoEncoderConfig : DeviceId {
		int fpsNumerator;
		int fpsDenominator;
		int bitrate;
		int keyframeInterval;
		int cx;
		int cy;
	};

	struct EncoderPacket {
		unsigned char  *data = nullptr;
		size_t         size = 0;
		long long      pts = 0;
		long long      dts = 0;
	};

	class VideoEncoder {
		HVideoEncoder *context = nullptr;

	public:
		VideoEncoder();
		~VideoEncoder();

		bool Valid() const;
		bool Active() const;


		bool SetConfig(VideoEncoderConfig &config);
		bool GetConfig(VideoEncoderConfig &config) const;

		bool Encode(unsigned char *data[DSHOW_MAX_PLANES],
				size_t linesize[DSHOW_MAX_PLANES],
				long long timestampStart,
				long long timestampEnd,
				EncoderPacket &packet,
				bool &new_packet);

		static bool EnumEncoders(std::vector<DeviceId> &encoders);
	};

	enum class LogType {
		Error,
		Warning,
		Info,
		Debug
	};

	typedef void (*LogCallback)(LogType type, const wchar_t *msg,
			void *param);

	DSHOWCAPTURE_EXPORT void SetLogCallback(LogCallback callback,void *param);
};
DSHOWCAPTURE_EXPORT void InitDShowCapture();
DSHOWCAPTURE_EXPORT void UnInitDShowCapture();


typedef enum DShowLogType{
   DShowLogType_Level0_debug,//调试
   DShowLogType_Level1_Interface,//DShow接口
   DShowLogType_Level1_Receive,//DShow接收数据过程
   DShowLogType_Level1_AudioPretreatment,//音频预处理
   DShowLogType_Level1_AudioMixAndEncode,//音频混音和编码
   DShowLogType_Level1_USBHot,//USB插拔记录
   DShowLogType_Level2_ALL,//系统CPU使用率、内存使用率、当前混音后音量值
   DShowLogType_Level3_AudioSource,//DShow音频数据源的一般日志
   DShowLogType_Level3_DShowDevice,//设备的一般日志
   DShowLogType_Level3_DShowGraphic,//DShowGraphic的一般日志
   
}DShowLogType;

typedef enum DShowLogLevel{
   DShowLogLevel_Debug,
   DShowLogLevel_Info,
   DShowLogLevel_Warning,
   DShowLogLevel_Error
}DShowLogLevel;

DSHOWCAPTURE_EXPORT void DShowLogInit();
DSHOWCAPTURE_EXPORT void DShowLogUnInit();
DSHOWCAPTURE_EXPORT void DShowLog(DShowLogType type,DShowLogLevel level,wchar_t *fmt,...);
#define DSHOWLogAudioExec(X,Y) if(MEDIATYPE_Audio == X) {Y;}

