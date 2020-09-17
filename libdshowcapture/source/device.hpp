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
#include "VH_ConstDeff.h"
#include "../dshowcapture.hpp"
#include "capture-filter.hpp"
#include "dshow-graph.hpp"

#include <string>
#include <vector>
using namespace std;

namespace DShow {

struct EncodedData {
	long long                      lastStartTime = 0;
	long long                      lastStopTime  = 0;
	vector<unsigned char>          bytes;
};

struct EncodedDevice {
	VideoFormat videoFormat;
	ULONG       videoPacketID;
	long        width = 0;
	long        height = 0;
	long long   frameInterval = 0;

	AudioFormat audioFormat;
	ULONG       audioPacketID;
	DWORD       samplesPerSec;
};



struct HDevice {
   DShowDeviceType GetFilterType()const{return filterType;}
   bool HasFilter(IBaseFilter *);

   DShowDeviceType                filterType;
   ComPtr<IBaseFilter>            deviceFilter = nullptr;
   DeviceInfo                     deviceInfo;
   
	ComPtr<CaptureFilter>          videoCapture = nullptr;
	ComPtr<CaptureFilter>          audioCapture = nullptr;
   
	ComPtr<IBaseFilter>            rocketEncoder = nullptr;

    MediaType                      videoMediaType;
	MediaType                      audioMediaType;

    HANDLE                         mConfigMutex;
	VideoConfig                    videoConfig;
	AudioConfig                    audioConfig;
    bool                           isResetVideo = false;
    bool                           isResetAudio = false;
	bool                           encodedDevice = false;
   

	EncodedData                    encodedVideo;
	EncodedData                    encodedAudio;


   
	HDevice();
	~HDevice();
   MediaType GetVideoMediaType(){return videoMediaType;}

   void ReleaseFilter();
	void ConvertVideoSettings();
	void ConvertAudioSettings();

	bool EnsureInitialized(const wchar_t *func);
	bool EnsureInactive(const wchar_t *func);
   bool Valid();

	inline void SendToCallback(bool video,
			unsigned char *data, size_t size,
			long long startTime, long long stopTime);

	void Receive(bool video, IMediaSample *sample);

	bool SetupEncodedVideoCapture(IBaseFilter *filter,
				VideoConfig &config,
				const EncodedDevice &info,
				IGraphBuilder *graph);

	bool SetupExceptionVideoCapture(IBaseFilter *filter,
			VideoConfig &config,IGraphBuilder *graph);

	bool SetupExceptionAudioCapture(IPin *pin);

	bool SetupVideoCapture(IBaseFilter *filter, VideoConfig &config,IGraphBuilder *graph);
	bool SetupAudioCapture(IBaseFilter *filter, AudioConfig &config,IGraphBuilder *graph);
   
   //只能选择PIN，不能重置Filter
	bool SetVideoConfig(VideoConfig *config);
	bool SetAudioConfig(AudioConfig *config);
   bool InitDeviceFilter(std::wstring &name,std::wstring &path,DShowDeviceType filterType);

	bool FindCrossbar(IBaseFilter *filter, IBaseFilter **crossbar,ICaptureGraphBuilder2 *builder);
	bool ConnectPins(const GUID &category, const GUID &type,IBaseFilter *filter, IBaseFilter *capture,ICaptureGraphBuilder2 *builder,
		IGraphBuilder*          graph);

	bool RenderFilters(const GUID &category, const GUID &type,
	IBaseFilter *filter, IBaseFilter *capture,ICaptureGraphBuilder2 *builder);

	bool ConnectFiltersInternal();

	Result Start();
	void Stop();
   void ReStart();

   //获得当前设备的配置信息
   bool GetCurrDeviceFrameInfoList(FrameInfoList *frameList,FrameInfo *currFrame);
   static bool GetDeviceFrameInfoList(DeviceInfo,FrameInfoList *frameList,FrameInfo *currFrame);
   bool SetDeviceDefaultAttribute(int width,int height,int frameInternal,VideoFormat format);   
   static bool SetDeviceDefaultAttribute(DeviceInfo deviceInfo,UINT width,UINT height,int frameInternal,VideoFormat format);
};

}; /* namespace DShow */
