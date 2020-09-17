/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once

#include <memory>
#include <atomic>
#include "ImageEnhance.h"
#include "VHBeautifyRender.h"

void PackPlanar(LPBYTE convertBuffer, LPBYTE lpPlanar, UINT renderCX, UINT renderCY, UINT pitch, UINT startY, UINT endY, UINT linePitch, UINT lineShift);


class DeviceSource;

class DeviceSource : public ImageSource
{
    UINT enteredSceneCount;

    //---------------------------------
    std::atomic_bool    m_bIsHasNoData;
    bool            m_bDeviceExist;
    std::atomic_bool m_bIsPreView = false;
    DeviceInfo      deviceInfo;
    IDShowVideoFilterDevice* m_device = NULL;
    VideoDeviceSetting    m_setting;
    UINT            renderCX, renderCY;
    UINT            newCX, newCY;
    UINT            imageCX, imageCY;
    UINT            linePitch, lineShift, lineSize;
    BOOL            fullRange;


    struct {
        int                         type; //DeinterlacingType
        char                        fieldOrder; //DeinterlacingFieldOrder
        char                        processor; //DeinterlacingProcessor
        bool                        curField, bNewFrame;
        bool                        doublesFramerate;
        bool                        needsPreviousFrame;
        bool                        isReady;
        std::unique_ptr<Texture>    texture;
        UINT                        imageCX, imageCY;
        std::unique_ptr<Shader>     vertexShader;
        FuturePixelShader           pixelShader;
    } deinterlacer,beautyDeinterlacer;

    bool            bFirstFrame;
    bool            bUseThreadedConversion;
    std::atomic_bool bReadyToDraw;
    std::atomic_bool bHideBeautyPreviewDraw = false;

    Texture         *m_deviceSourceTexture = nullptr, *previousTexture = nullptr;
    Texture         *m_deviceSourceBeautyTexture = nullptr, *previousBeautyTexture = nullptr;;

    Texture         *tipTexture = nullptr;

    Texture         *m_disconnectTexture = nullptr;
    Texture         *m_failedTexture = nullptr;
    Texture         *m_blackBackgroundTexture = nullptr;
    Texture         *m_deviceNameTexture = nullptr;


    std::shared_ptr<vhall::VHBeautifyRender> mBeautifyFilter = nullptr;
    std::atomic<int> mBeautyLevel = 0;

    std::shared_ptr<unsigned char[]> mCaptureData = nullptr;  //采集数据
    std::shared_ptr<unsigned char[]> mBeautyRgb24 = nullptr;  //采集数据转换为美颜需要的rgb24

    std::shared_ptr<unsigned char[]> mBeautyCallbackData = nullptr;   //美颜数据转换为渲染数据
    std::shared_ptr<unsigned char[]> mRenderData = nullptr;

    ImageEnhance*   m_ImageEnhance = nullptr;
    BYTE*           m_ImageEnhanceData = nullptr;
    int             mLastWidth = 0;
    int             mLastHeight = 0;

    XElement        *mElementData = nullptr;
    UINT            texturePitch;
    UINT            m_imageBufferSize;
    bool            bCapturing = false, bFiltersLoaded = false;
    Shader          *colorConvertShader = nullptr;
    Shader          *drawShader = nullptr;

    HANDLE          hStopSampleEvent;
    HANDLE          hSampleMutex;
    SampleData      *latestVideoSample = nullptr;
    QWORD           m_LastSyncTime;
    QWORD           m_LastTickTime = 0; 
    HWND            m_renderHWND = NULL;   
    HWND            m_privew_renderHwnd = nullptr;
    HWND            m_beauty_privew_renderHwnd = nullptr;
    OBSRenderView   *m_renderView = NULL;
    

    //---------------------------------

    LPBYTE          lpImageBuffer = nullptr;
    ConvertData     *convertData = nullptr;
    HANDLE          *hConvertThreads = nullptr;

    void ChangeSize(bool bSucceeded = true, bool bForce = false);
    void KillThreads();

    void ProcessImageHance(SampleData* sample,int data_format);
    String ChooseShader();
    String ChooseDeinterlacingShader();

    void Convert422To444(LPBYTE convertBuffer, LPBYTE lp422, UINT pitch, bool bLeadingY);
    void SetDeviceExist(bool);

    bool LoadFilters();
    void UnloadFilters();

    void Start();
    void Stop();

   void CreateBeautyFilter();

public:
    bool Init(XElement *data);
    DeviceSource();
    ~DeviceSource();

    void UpdateSettings();

    virtual void SetBeautyPreviewHandle(HWND m_PriviewRenderHwnd, int hide_preview);

    virtual void SetPreviewState(bool is_preivew);
    bool Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType);
    virtual int HasRendData();
    void BeginScene();
    void EndScene();
    void HDCRender(DeviceColorType type, HWND desHwnd, LPBYTE bytes, int cx, int cy);
    void HDBeautyRender(DeviceColorType type, LPBYTE src_bytes,int cx, int cy);
    void OnProcessVideoFrame(SampleData* frame, bool bForceRGB24 = false);
    void GlobalSourceEnterScene();
    void GlobalSourceLeaveScene();
    virtual void SetBeautyLevel(int level);
    Vect2 GetSize() const {
      if(imageCX!=0&&imageCY!=0)
         return Vect2(float(imageCX), float(imageCY));
      else
         return Vect2(float(1280.0f),float(720.0f));
    }
    
    virtual void Reload();
};

