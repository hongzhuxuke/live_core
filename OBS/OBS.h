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

#include <functional>
#include <list>
#include <atomic>

#pragma once

class Scene;
#define NUM_RENDER_BUFFERS 2

static const int minClientWidth  = 640;
static const int minClientHeight = 275;

static const int defaultClientWidth  = 896;
static const int defaultClientHeight = 504;
struct EncoderPicture;

enum edges {
    edgeLeft = 0x01,
    edgeRight = 0x02,
    edgeTop = 0x04,
    edgeBottom = 0x08,
};

struct MonitorInfo
{
    inline MonitorInfo() {zero(this, sizeof(MonitorInfo));}

    inline MonitorInfo(HMONITOR hMonitor, RECT *lpRect)
    {
        this->hMonitor = hMonitor;
        mcpy(&this->rect, lpRect, sizeof(rect));
    }

    HMONITOR hMonitor;
    RECT rect;
    float rotationDegrees;
};

struct ClassInfo
{
    String strClass;
    String strName;
    OBSCREATEPROC createProc;
    OBSCONFIGPROC configProc;
    bool bDeprecated;

    inline void FreeData() {strClass.Clear(); strName.Clear();}
};

//----------------------------

/* Event callback signiture definitions */
typedef void (*OBS_CALLBACK)();
typedef void (*OBS_REPLAY_BUFFER_SAVED_CALLBACK)(CTSTR filename, UINT recordingLengthMS);
typedef void (*OBS_STATUS_CALLBACK)(bool /*running*/, bool /*streaming*/, bool /*recording*/,
                                    bool /*previewing*/, bool /*reconnecting*/);
typedef void (*OBS_STREAM_STATUS_CALLBACK)(bool /*streaming*/, bool /*previewOnly*/,
                                           UINT /*bytesPerSec*/, double /*strain*/, 
                                           UINT /*totalStreamtime*/, UINT /*numTotalFrames*/, 
                                           UINT /*numDroppedFrames*/, UINT /*fps*/);
typedef void (*OBS_SCENE_SWITCH_CALLBACK)(CTSTR);
typedef void (*OBS_SOURCE_CHANGED_CALLBACK)(CTSTR /*sourceName*/, XElement* /*source*/); 
typedef void (*OBS_VOLUME_CHANGED_CALLBACK)(float /*level*/, bool /*muted*/, bool /*finalFalue*/);
typedef void (*OBS_LOG_UPDATE_CALLBACK)(CTSTR /*delta*/, UINT /*length*/);



//----------------------------

struct GlobalSourceInfo
{
    String strName;
    XElement *element;
    ImageSource *source;

    inline void FreeData() {strName.Clear(); delete source; source = NULL;}
};

//----------------------------

enum
{
    ID_SETTINGS=5000,
    ID_TOGGLERECORDINGREPLAYBUFFER,
    ID_TOGGLERECORDING,
    ID_STARTSTOP,
    ID_EXIT,
    ID_SCENEEDITOR,
    ID_DESKTOPVOLUME,
    ID_MICVOLUME,
    ID_DESKTOPVOLUMEMETER,
    ID_MICVOLUMEMETER,
    ID_STATUS,
    ID_SCENES,
    ID_SCENES_TEXT,
    ID_SOURCES,
    ID_SOURCES_TEXT,
    ID_TESTSTREAM,
    ID_GLOBALSOURCES,
    ID_PLUGINS,
    ID_DASHBOARD,
    ID_MINIMIZERESTORE,

    ID_SWITCHPROFILE,
    ID_SWITCHPROFILE_END = ID_SWITCHPROFILE+1000,
    ID_UPLOAD_LOG,
    ID_UPLOAD_LOG_END = ID_UPLOAD_LOG+1000,
    ID_VIEW_LOG,
    ID_VIEW_LOG_END = ID_VIEW_LOG+1000,
    ID_REFRESH_LOGS,
    ID_UPLOAD_ANALYZE_LOG,
    ID_UPLOAD_ANALYZE_LOG_END = ID_UPLOAD_ANALYZE_LOG+1000,
    ID_LOG_WINDOW,
    ID_SWITCHSCENECOLLECTION,
    ID_SWITCHSCENECOLLECTION_END = ID_SWITCHSCENECOLLECTION+1000,
};

enum
{
    OBS_REQUESTSTOP=WM_USER+1,
    OBS_CALLHOTKEY,
    OBS_RECONNECT,
    OBS_SETSCENE,
    OBS_SETSOURCEORDER,
    OBS_SETSOURCERENDER,
    OBS_NOTIFICATIONAREA,
    OBS_NETWORK_FAILED,
    OBS_CONFIGURE_STREAM_BUTTONS,
};

//----------------------------

enum ColorPrimaries
{
    ColorPrimaries_BT709 = 1,
    ColorPrimaries_Unspecified,
    ColorPrimaries_BT470M = 4,
    ColorPrimaries_BT470BG,
    ColorPrimaries_SMPTE170M,
    ColorPrimaries_SMPTE240M,
    ColorPrimaries_Film,
    ColorPrimaries_BT2020
};

enum ColorTransfer
{
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

enum ColorMatrix
{
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

struct ColorDescription
{
    int fullRange;
    int primaries;
    int transfer;
    int matrix;
};

//----------------------------

enum ItemModifyType
{
    ItemModifyType_None,
    ItemModifyType_Move,
    ItemModifyType_ScaleBottomLeft,
    ItemModifyType_CropBottomLeft,
    ItemModifyType_ScaleLeft,
    ItemModifyType_CropLeft,
    ItemModifyType_ScaleTopLeft,
    ItemModifyType_CropTopLeft,
    ItemModifyType_ScaleTop,
    ItemModifyType_CropTop,
    ItemModifyType_ScaleTopRight,
    ItemModifyType_CropTopRight,
    ItemModifyType_ScaleRight,
    ItemModifyType_CropRight,
    ItemModifyType_ScaleBottomRight,
    ItemModifyType_CropBottomRight,
    ItemModifyType_ScaleBottom,
    ItemModifyType_CropBottom
};

//----------------------------

enum PreviewDrawType {
    Preview_Standard,
    Preview_Fullscreen,
    Preview_Projector
};

typedef struct OBSYUVData_st{
   unsigned long long pts;
   unsigned char *yuvData;
}OBSYUVData;

#define OBS_DEL(x) if (x) {  \
delete x;                      \
x = NULL;                      \
}

class IDataReceiver;
//todo: this class has become way too big, it's horrible, and I should be ashamed of myself
class OBS
{
    friend class Scene;
    friend class SceneItem;
    friend class D3D10System;
    friend class D3D11System;
    friend class OBSAPIInterface;
    friend class GlobalSource;
    friend class TextOutputSource;
    friend class CGraphics;

    //---------------------------------------------------
    // graphics stuff

    Texture *copyTextures[NUM_RENDER_BUFFERS] = {nullptr};
    
    Texture         *mainRenderTextures[NUM_RENDER_BUFFERS] = { nullptr };
    Texture         *yuvRenderTextures[NUM_RENDER_BUFFERS] = { nullptr };
    Texture         *previewTexture = nullptr;
    Texture         *desktopTexture = nullptr;
    
    Texture *lastRenderTexture = nullptr;
    Texture *transitionTexture = nullptr;

    bool    performTransition;
    bool    bTransitioning = false;
    float   transitionAlpha;

    Shader  *mainVertexShader = nullptr, *mainPixelShader = nullptr, *yuvScalePixelShader = nullptr, *transitionPixelShader = nullptr;
    Shader  *solidVertexShader = nullptr, *solidPixelShader = nullptr;

    Scene                   *scene = nullptr;
    List<MonitorInfo>       monitors;
    XConfig                 scenesConfig;
    XConfig                 globalSourcesImportConfig;
    XElement                *sceneElement = nullptr;
public:
   static OBS *mThis;
protected:   
   static DWORD STDCALL ThreadOBSReset(OBS *obs) ;
private:
    bool mIsAutoMode;
    HANDLE  mSetSceneVisibleEvent;
    std::atomic_bool    mBIsSetSceneVisible;
   
    HWND    mRenderWidgetHwnd;
    HWND    mMainWidgetHwnd;
    String  strLanguage;
    std::atomic_bool    bUseMultithreadedOptimizations;
    std::atomic_bool    bRunning = false;
    std::atomic_bool bShutdownVideoThread;
    int     renderFrameWidth = 0, renderFrameHeight = 0; // The size of the preview only
    int     renderFrameX = 0, renderFrameY = 0; // The offset of the preview inside the preview control
    int     renderFrameCtrlWidth = 0, renderFrameCtrlHeight = 0; // The size of the entire preview control
    int     oldRenderFrameCtrlWidth = 0, oldRenderFrameCtrlHeight = 0; // The size of the entire preview control before the user began to resize the window
    HWND    hwndRenderMessage; // The text in the middle of the main window
    std::atomic_bool    renderFrameIn1To1Mode;
    int     borderXSize = 0, borderYSize = 0;
    int     clientWidth = 0, clientHeight = 0;

    std::atomic_bool    bDragResize;
    std::atomic_bool    bSizeChanging;
    std::atomic_bool    bResizeRenderView = false;

    std::atomic_bool    bEditMode = false;
    std::atomic_bool    bRenderViewEnabled = false;
    std::atomic_bool    bShowFPS;
    std::atomic_bool    bMouseMoved = false;
    std::atomic_bool    bMouseDown = false;
    std::atomic_bool    bRMouseDown = false;
    std::atomic_bool    bItemWasSelected = false;
    
    Vect2   startMousePos, lastMousePos;
    ItemModifyType modifyType = ItemModifyType_None;
    SceneItem *scaleItem = NULL;

    int     cpuInfo[4];

    // resolution/fps/downscale/etc settings

    int     lastRenderTarget = 0;
    UINT    baseCX = 0,   baseCY = 0;
    UINT    scaleCX = 0,  scaleCY = 0;
    UINT    outputCX = 0, outputCY = 0;
    //float   downscale;
    int     downscaleType;
    UINT    frameTime, fps;
    ColorDescription colorDesc;

    int mScaleType;

    //---------------------------------------------------
    // stats

    double curStrain;

    //---------------------------------------------------
    // main capture loop stuff


    HANDLE  hVideoThread = nullptr;
    HANDLE  hSceneMutex = nullptr;
    HANDLE  mDataMutex = nullptr;

    IDataReceiver *mDataReceiver = NULL;
    unsigned char *volatile currentFrameYUV[2] = { nullptr };
    std::list<OBSYUVData> mFrameList;
    
    QWORD currentFrameSyncTime = 0;
    IPOSTCRMessageFunc       mfuncPostCRMsg = NULL;

    //±àÂëÆ÷È¡Í¼Ïñ½Ó¿Ú
    //EncoderPicture * volatile curFramePic;
    HANDLE hVideoEvent;
    static DWORD STDCALL MainCaptureThread(LPVOID lpUnused);
    void MainCaptureLoop();

    void DrawPreview(const Vect2 &renderFrameSize, const Vect2 &renderFrameOffset, const Vect2 &renderFrameCtrlSize, int curRenderTarget, PreviewDrawType type);
    void SetSyncWithEncoder(const bool & isWillsync);
    unsigned char** LockCurrentFramePic(QWORD &syncTime,unsigned long long currentTime);
    void UnlockCurrentFramePic() ;

    List<ClassInfo> sceneClasses;
    List<ClassInfo> imageSourceClasses;

    List<GlobalSourceInfo> globalSources;

    HANDLE hStartupShutdownMutex;

    ImageSource* AddGlobalSourceToScene(CTSTR lpName);

    inline ImageSource* GetGlobalSource(CTSTR lpName)
    {
        for(UINT i=0; i<globalSources.Num(); i++)
        {
            if(globalSources[i].strName.CompareI(lpName))
                return globalSources[i].source;
        }

        return AddGlobalSourceToScene(lpName);
    }

    inline ClassInfo* GetSceneClass(CTSTR lpClass) const
    {
        for(UINT i=0; i<sceneClasses.Num(); i++)
        {
            if(sceneClasses[i].strClass.CompareI(lpClass))
                return sceneClasses+i;
        }

        return NULL;
    }

    inline ClassInfo* GetImageSourceClass(CTSTR lpClass) const
    {
        for(UINT i=0; i<imageSourceClasses.Num(); i++)
        {
            if(imageSourceClasses[i].strClass.CompareI(lpClass))
                return imageSourceClasses+i;
        }

        return NULL;
    }

    void SetAutoMode(bool isAuto){this->mIsAutoMode=isAuto;}
    void Start();
    void Stop(bool isRestart);
    void Reset();

    // Helpers for converting between window and actual coordinates for the preview
    Vect2 MapWindowToFramePos(Vect2 mousePos);
    Vect2 MapFrameToWindowPos(Vect2 framePos);
    Vect2 MapWindowToFrameSize(Vect2 windowSize);
    Vect2 MapFrameToWindowSize(Vect2 frameSize);
    Vect2 GetWindowToFrameScale();
    Vect2 GetFrameToWindowScale();

    void FitItemsToScreen(SceneItem *item);

    // helper to valid crops as you scale items
    bool EnsureCropValid(SceneItem *&scaleItem, Vect2 &minSize, Vect2 &snapSize, bool bControlDown, int cropEdges, bool cropSymmetric);
    void ResizeRenderFrame(bool bRedrawRenderFrame);

    void ToggleCapturing();
 
    Scene* CreateScene(CTSTR lpClassName, XElement *data);
    void ConfigureScene(XElement *element);
    void ConfigureImageSource(XElement *element);

    static bool STDCALL ConfigGlobalSource(XElement *element, bool bCreating);

public:
   OBS(HWND frameWnd, HWND renderWnd,int scaleType, const wchar_t *logPath = NULL);
    virtual ~OBS();
    void SetDataReceiver(IDataReceiver *r){mDataReceiver=r;}
    bool SetSourceVisible(wchar_t *sourceName,bool isVisible,bool isWait);   
    bool WaitSetSourceVisible();
    inline HWND GetRenderHWND(){return mRenderWidgetHwnd;}

    void GetBaseSize(UINT &width, UINT &height) const;

    inline void GetRenderFrameSize(UINT &width, UINT &height) const         {width = renderFrameWidth; height = renderFrameHeight;}
    inline void GetRenderFrameOffset(UINT &x, UINT &y) const                {x = renderFrameX; y = renderFrameY;}
    inline void GetRenderFrameControlSize(UINT &width, UINT &height) const  {width = renderFrameCtrlWidth; height = renderFrameCtrlHeight;}
    inline void GetOutputSize(UINT &width, UINT &height) const              {width = outputCX;         height = outputCY;}

    inline Vect2 GetBaseSize() const
    {
        UINT width, height;
        GetBaseSize(width, height);
        return Vect2(float(width), float(height));
    }

    inline Vect2 GetOutputSize()      const         {return Vect2(float(outputCX), float(outputCY));}
    inline Vect2 GetRenderFrameSize() const         {return Vect2(float(renderFrameWidth), float(renderFrameHeight));}
    inline Vect2 GetRenderFrameOffset() const       {return Vect2(float(renderFrameX), float(renderFrameY));}
    inline Vect2 GetRenderFrameControlSize() const  {return Vect2(float(renderFrameCtrlWidth), float(renderFrameCtrlHeight));}
    //inline void EnterSceneMutex();
    //inline void LeaveSceneMutex();

    inline void EnterSceneMutex() {OSEnterMutex(hSceneMutex);}
    inline void LeaveSceneMutex() {OSLeaveMutex(hSceneMutex);}

    inline UINT GetFrameTime() const {return frameTime;}

    inline UINT NumMonitors()  const {return monitors.Num();}
    inline const MonitorInfo& GetMonitor(UINT id) const {if(id < monitors.Num()) return monitors[id]; else return monitors[0];}

    inline XElement* GetSceneElement() const {return sceneElement;}
    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated);
    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated);
    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data);
    virtual bool SetScene(CTSTR lpScene); 
    void GetThreadHandles (HANDLE *videoThread);
    void UIWarning(wchar_t *d);
    void SetPostMsgHook(IPOSTCRMessageFunc funcPostMsg);
    void DoHideLogo(bool bHide);
    void EnterStartUpShutDownMutex();
    void LeaveStartUpShutDownMutex();

};
const MonitorInfo &OBSGetMonitor(UINT);
LONG CALLBACK OBSExceptionHandler (PEXCEPTION_POINTERS exceptionInfo);
