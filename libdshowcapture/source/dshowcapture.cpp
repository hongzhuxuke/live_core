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

#include "../dshowcapture.hpp"
#include "dshow-base.hpp"
#include "dshow-enum.hpp"
#include "device.hpp"
#include "dshow-device-defs.hpp"
#include "log.hpp"

#include <vector>

namespace DShow {

Device::Device(InitGraph initialize) : context(new HDevice)
{
	//if (initialize == InitGraph::True)
		//context->CreateGraph();
   
}

Device::~Device()
{
	delete context;
}

bool Device::Valid() const
{
	return context->Valid();
}
void Device::ShutdownGraph()
{
	delete context;
	context = new HDevice;
}

bool Device::SetVideoConfig(VideoConfig *config)
{
	return context->SetVideoConfig(config);
}

bool Device::SetAudioConfig(AudioConfig *config)
{
	return context->SetAudioConfig(config);
}

bool Device::ConnectFilters()
{
//	return context->ConnectFilters();
   return true;
}

Result Device::Start()
{
	return context->Start();
}

void Device::Stop()
{
	context->Stop();
}
void Device::ReStart()
{
   context->ReStart();
}

bool Device::GetVideoConfig(VideoConfig &config) const
{
	if (context->videoCapture == NULL)
		return false;

	config = context->videoConfig;
	return true;
}

bool Device::GetAudioConfig(AudioConfig &config) const
{
	if (context->audioCapture == NULL)
		return false;

	config = context->audioConfig;
	return true;
}

bool Device::GetVideoDeviceId(DeviceId &id) const
{
	if (context->videoCapture == NULL)
		return false;

	id = context->videoConfig;
	return true;
}

bool Device::GetAudioDeviceId(DeviceId &id) const
{
	if (context->audioCapture == NULL)
		return false;

	id = context->audioConfig;
	return true;
}
const GUID MEDIASUBTYPE_I420  = { 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID MEDIASUBTYPE_HDYC = { 0x43594448, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

VideoOutputType GetVideoOutputType(const AM_MEDIA_TYPE &media_type)
{
    VideoOutputType type = VideoOutputType_None;

    if(media_type.majortype == MEDIATYPE_Video)
    {
        // Packed RGB formats
        if(media_type.subtype == MEDIASUBTYPE_RGB24)
            type = VideoOutputType_RGB24;
        else if(media_type.subtype == MEDIASUBTYPE_RGB32)
            type = VideoOutputType_RGB32;
        else if(media_type.subtype == MEDIASUBTYPE_ARGB32)
            type = VideoOutputType_ARGB32;

        // Planar YUV formats
        else if(media_type.subtype == MEDIASUBTYPE_I420)
            type = VideoOutputType_I420;
       else if(media_type.subtype == MEDIASUBTYPE_HDYC)
            type = VideoOutputType_HDYC;
        else if(media_type.subtype == MEDIASUBTYPE_IYUV)
            type = VideoOutputType_I420;
        else if(media_type.subtype == MEDIASUBTYPE_YV12)
            type = VideoOutputType_YV12;

        else if(media_type.subtype == MEDIASUBTYPE_Y41P)
            type = VideoOutputType_Y41P;
        else if(media_type.subtype == MEDIASUBTYPE_YVU9)
            type = VideoOutputType_YVU9;

        // Packed YUV formats
        else if(media_type.subtype == MEDIASUBTYPE_YVYU)
            type = VideoOutputType_YVYU;
        else if(media_type.subtype == MEDIASUBTYPE_YUY2)
            type = VideoOutputType_YUY2;
        else if(media_type.subtype == MEDIASUBTYPE_UYVY)
            type = VideoOutputType_UYVY;

        else if(media_type.subtype == MEDIASUBTYPE_MPEG2_VIDEO)
            type = VideoOutputType_MPEG2_VIDEO;

        else if(media_type.subtype == MEDIASUBTYPE_H264)
            type = VideoOutputType_H264;

        else if(media_type.subtype == MEDIASUBTYPE_dvsl)
            type = VideoOutputType_dvsl;
        else if(media_type.subtype == MEDIASUBTYPE_dvsd)
            type = VideoOutputType_dvsd;
        else if(media_type.subtype == MEDIASUBTYPE_dvhd)
            type = VideoOutputType_dvhd;

        else if(media_type.subtype == MEDIASUBTYPE_MJPG)
            type = VideoOutputType_MJPG;
        else if (media_type.subtype == MEDIASUBTYPE_NV12)  {
           type = VideoOutputType_I420;
           //type = VideoOutputType_YVU9;
        }
//        else
         //   nop();
        ;
    }

    return type;
}

bool Device::GetVideoMediaType(VideoOutputType *type){
   if(!context||!type){
      return false; 
   }
   MediaType media_type=context->GetVideoMediaType();
   *type= GetVideoOutputType(*media_type.Ptr());
   return true;
}

//获得当前设备的配置和配置列表
bool Device::GetCurrDeviceFrameInfoList(FrameInfoList *frameList,FrameInfo *currFrame)
{
   if(!context)
   {
      return false;
   }

   return context->GetCurrDeviceFrameInfoList(frameList,currFrame);
}
bool Device::GetDeviceFrameInfoList(DeviceInfo info,FrameInfoList *frameList,FrameInfo *currFrame)
{
   return HDevice::GetDeviceFrameInfoList(info,frameList,currFrame);
}
bool Device::SetDeviceDefaultAttribute(int width,int height,int frameInternal,VideoFormat format)
{
   if(!context)
   {
      return false;
   }

   return context->SetDeviceDefaultAttribute(width,height,frameInternal,format);
}
bool Device::SetDeviceDefaultAttribute(DeviceInfo deviceInfo,UINT width,UINT height,int frameInternal,VideoFormat format)
{
   return HDevice::SetDeviceDefaultAttribute(deviceInfo,width,height,frameInternal,format);
}
static void OpenPropertyPages(HWND hwnd, IUnknown *propertyObject)
{
	if (!propertyObject)
		return;

	ComQIPtr<ISpecifyPropertyPages> pages(propertyObject);
	CAUUID cauuid;

	if (pages != NULL) {
		if (SUCCEEDED(pages->GetPages(&cauuid)) && cauuid.cElems) {
			OleCreatePropertyFrame(hwnd, 0, 0, NULL, 1,
					(LPUNKNOWN*)&propertyObject,
					cauuid.cElems, cauuid.pElems,
					0, 0, NULL);
			CoTaskMemFree(cauuid.pElems);
		}
	}
}
static void EnumEncodedVideo(std::vector<VideoDevice> &devices,
		const wchar_t *deviceName, const wchar_t *devicePath,
		const EncodedDevice &info)
{
	VideoDevice device;
	VideoInfo   caps;

	device.name          = deviceName;
	device.path          = devicePath;
	device.audioAttached = true;

	caps.minCX         = caps.maxCX         = info.width;
	caps.minCY         = caps.maxCY         = info.height;
	caps.granularityCX = caps.granularityCY = 1;
	caps.minInterval   = caps.maxInterval   = info.frameInterval;
	caps.format                             = info.videoFormat;

	device.caps.push_back(caps);
	devices.push_back(device);
}

static void EnumExceptionVideoDevice(std::vector<VideoDevice> &devices,
		IBaseFilter *filter,
		const wchar_t *deviceName,
		const wchar_t *devicePath)
{
	ComPtr<IPin> pin;

	if (GetPinByName(filter, PINDIR_OUTPUT, L"656", &pin))
		EnumEncodedVideo(devices, deviceName, devicePath, HD_PVR2);

	else if (GetPinByName(filter, PINDIR_OUTPUT, L"TS Out", &pin))
		EnumEncodedVideo(devices, deviceName, devicePath, Roxio);
}

static bool EnumVideoDevice(std::vector<VideoDevice> &devices,
		IBaseFilter *filter,
		const wchar_t *deviceName,
		const wchar_t *devicePath)
{
	ComPtr<IPin>  pin;
	ComPtr<IPin>  audioPin;
	VideoDevice   info;

	if (wcsstr(deviceName, L"C875") != nullptr) {
		EnumEncodedVideo(devices, deviceName, devicePath, AV_LGP);
		return true;

	} else if (wcsstr(deviceName, L"Hauppauge HD PVR Capture") != nullptr) {
		EnumEncodedVideo(devices, deviceName, devicePath, HD_PVR1);
		return true;
	}

	bool success = GetFilterPin(filter, MEDIATYPE_Video,
			PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &pin);

	/* if this device has no standard capture pin, see if it's an
	 * encoded device, and get its information if so (all encoded devices
	 * are exception devices pretty much) */
	if (!success) {
		EnumExceptionVideoDevice(devices, filter, deviceName,
				devicePath);
		return true;
	}

	if (!EnumVideoCaps(pin, info.caps))
		return true;

	info.audioAttached = GetFilterPin(filter, MEDIATYPE_Audio,
			PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &audioPin);

	info.name = deviceName;
	if (devicePath)
		info.path = devicePath;

	devices.push_back(info);
	return true;
}

bool Device::EnumVideoDevices(std::vector<VideoDevice> &devices)
{
	devices.clear();
	return EnumDevices(CLSID_VideoInputDeviceCategory,
			EnumDeviceCallback(EnumVideoDevice), &devices);
}

static bool EnumAudioDevice(vector<AudioDevice> &devices,
		IBaseFilter *filter,
		const wchar_t *deviceName,
		const wchar_t *devicePath)
{
	ComPtr<IPin>  pin;
	AudioDevice   info;

	bool success = GetFilterPin(filter, MEDIATYPE_Audio,
			PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &pin);
	if (!success)
		return true;

	if (!EnumAudioCaps(pin, info.caps))
		return true;

	info.name = deviceName;
	if (devicePath)
		info.path = devicePath;

	devices.push_back(info);
	return true;
}

bool Device::EnumAudioDevices(vector<AudioDevice> &devices)
{
	devices.clear();
	return EnumDevices(CLSID_AudioInputDeviceCategory,
			EnumDeviceCallback(EnumAudioDevice), &devices);
}

}; /* namespace DShow */
