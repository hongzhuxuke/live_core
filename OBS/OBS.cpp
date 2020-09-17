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
#include "Main.h"
#include "Msg_OBSControl.h"

#include <intrin.h>
#include "DeckLinkVideoSource.h"
#include "MediaVideoSource.h"
#include "IDShowPlugin.h"

//primarily main window stuff an initialization/destruction code

typedef bool(*LOADPLUGINPROC)();
typedef bool(*LOADPLUGINEXPROC)(UINT);
typedef void(*UNLOADPLUGINPROC)();

ImageSource* STDCALL CreateDesktopSource(XElement *data);
bool STDCALL ConfigureDesktopSource(XElement *data, bool bCreating);
bool STDCALL ConfigureWindowCaptureSource(XElement *data, bool bCreating);
bool STDCALL ConfigureMonitorCaptureSource(XElement *data, bool bCreating);

ImageSource* STDCALL CreateBitmapSource(XElement *data);
bool STDCALL ConfigureBitmapSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateBitmapTransitionSource(XElement *data);
bool STDCALL ConfigureBitmapTransitionSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateTextSource(XElement *data);
bool STDCALL ConfigureTextSource(XElement *element, bool bCreating);

ImageSource* STDCALL CreateGlobalSource(XElement *data);

void CreateOBSApiInterface(OBS *, const wchar_t* logPath = NULL);
void DestoryOBSApiInterface();
const float defaultBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

//----------------------------

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, List<MonitorInfo> &monitors) {
   monitors << MonitorInfo(hMonitor, lprcMonitor);
   return TRUE;
}

const int controlPadding = 3;

const int totalControlAreaWidth = minClientWidth - 10;
const int miscAreaWidth = 290;
const int totalControlAreaHeight = 171;//170;//
const int listAreaWidth = totalControlAreaWidth - miscAreaWidth;
const int controlWidth = miscAreaWidth / 2;
const int controlHeight = 22;
const int volControlHeight = 32;
const int volMeterHeight = 10;
const int textControlHeight = 16;
const int listControlWidth = listAreaWidth / 2;

Scene* STDCALL CreateNormalScene(XElement *data) {
   return new Scene;
}

BOOL IsWebrootLoaded() {
   BOOL ret = FALSE;
   StringList moduleList;

   OSGetLoadedModuleList(GetCurrentProcess(), moduleList);

   HMODULE msIMG = GetModuleHandle(TEXT("MSIMG32"));
   if (msIMG) {
      FARPROC alphaBlend = GetProcAddress(msIMG, "AlphaBlend");
      if (alphaBlend) {
         if (!IsBadReadPtr(alphaBlend, 5)) {
            BYTE opCode = *(BYTE *)alphaBlend;

            if (opCode == 0xE9) {
               if (moduleList.HasValue(TEXT("wrusr.dll")))
                  ret = TRUE;
            }
         }
      }
   }

   return ret;
}


OBS *OBS::mThis=NULL;
ImageSource * STDCALL CreateDShowVideoFilterVideoSource(XElement *data);

//---------------------------------------------------------------------------
OBS::OBS(HWND frameWnd, HWND renderWnd,int scaleType, const wchar_t *logPath) {
   bRunning = false;
   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      mainRenderTextures[i] = NULL;
      yuvRenderTextures[i] = NULL;
   }
   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      copyTextures[i] = nullptr;
   }

   mIsAutoMode=false;
   mThis=this;
   mScaleType=scaleType;
  
   performTransition = true;        //Default to true and don't set the conf. 
   //We don't want to let plugins disable standard behavior permanently.
   hSceneMutex = OSCreateMutex();
   hVideoEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

   monitors.Clear();
   EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorInfoEnumProc, (LPARAM)&monitors);

   INITCOMMONCONTROLSEX ecce;
   ecce.dwSize = sizeof(ecce);
   ecce.dwICC = ICC_STANDARD_CLASSES;
   if (!InitCommonControlsEx(&ecce))
      CrashError(TEXT("Could not initalize common shell controls"));
   //-----------------------------------------------------
   // load locale

   if (!XTGetLocale()->LoadStringFile(TEXT("locale/en.txt")))
      AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("locale/en.txt"));

   strLanguage = GlobalConfig->GetString(TEXT("General"), TEXT("Language"), TEXT("en"));
   if (!strLanguage.CompareI(TEXT("en"))) {
      String langFile;
      langFile << TEXT("locale/") << strLanguage << TEXT(".txt");

      if (!XTGetLocale()->LoadStringFile(langFile))
         AppWarning(TEXT("Could not open locale string file '%s'"), langFile.Array());
   }

   //-----------------------------------------------------
   // load classes
   RegisterSceneClass(TEXT("Scene"), Str("Scene"), (OBSCREATEPROC)CreateNormalScene, NULL, false);
   RegisterImageSourceClass(TEXT("DesktopImageSource"), Str("Sources.SoftwareCaptureSource"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureDesktopSource, true);
   RegisterImageSourceClass(TEXT("WindowCaptureSource"), Str("Sources.SoftwareCaptureSource.WindowCapture"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureWindowCaptureSource, false);
   RegisterImageSourceClass(TEXT("MonitorCaptureSource"), Str("Sources.SoftwareCaptureSource.MonitorCapture"), (OBSCREATEPROC)CreateDesktopSource, (OBSCONFIGPROC)ConfigureMonitorCaptureSource, false);
   RegisterImageSourceClass(TEXT("BitmapImageSource"), Str("Sources.BitmapSource"), (OBSCREATEPROC)CreateBitmapSource, (OBSCONFIGPROC)ConfigureBitmapSource, false);
   RegisterImageSourceClass(TEXT("GlobalSource"), Str("Sources.GlobalSource"), (OBSCREATEPROC)CreateGlobalSource, (OBSCONFIGPROC)OBS::ConfigGlobalSource, false);
   RegisterImageSourceClass(TEXT("TextSource"), Str("Sources.TextSource"), (OBSCREATEPROC)CreateTextSource, (OBSCONFIGPROC)ConfigureTextSource, false);
   RegisterImageSourceClass(DECKLINK_CLASSNAME, Str("Sources.DeckLinkCapture"), (OBSCREATEPROC)CreateDeckLinkVideoSource, NULL, false);
   RegisterImageSourceClass(MEDIA_OUTPUT_CLASSNAME, Str("Sources.MediaOutSource"), (OBSCREATEPROC)CreateMediaVideoSource, NULL, false);
   RegisterImageSourceClass(DSHOW_CLASSNAME, Str("Sources.DeviceCapture"), (OBSCREATEPROC)CreateDShowVideoFilterVideoSource, NULL, false);

   // create main window

   int fullscreenX = GetSystemMetrics(SM_CXFULLSCREEN);
   int fullscreenY = GetSystemMetrics(SM_CYFULLSCREEN);

   borderXSize = borderYSize = 0;

   borderXSize += GetSystemMetrics(SM_CXSIZEFRAME) * 2;
   borderYSize += GetSystemMetrics(SM_CYSIZEFRAME) * 2;
   borderYSize += GetSystemMetrics(SM_CYMENU);
   borderYSize += GetSystemMetrics(SM_CYCAPTION);

   clientWidth = GlobalConfig->GetInt(TEXT("General"), TEXT("Width"), defaultClientWidth);
   clientHeight = GlobalConfig->GetInt(TEXT("General"), TEXT("Height"), defaultClientHeight);

   if (clientWidth < minClientWidth)
      clientWidth = minClientWidth;
   if (clientHeight < minClientHeight)
      clientHeight = minClientHeight;

   int maxCX = fullscreenX - borderXSize;
   int maxCY = fullscreenY - borderYSize;

   if (clientWidth > maxCX)
      clientWidth = maxCX;
   if (clientHeight > maxCY)
      clientHeight = maxCY;

   int cx = clientWidth + borderXSize;
   int cy = clientHeight + borderYSize;

   int x = (fullscreenX / 2) - (cx / 2);
   int y = (fullscreenY / 2) - (cy / 2);

   int posX = GlobalConfig->GetInt(TEXT("General"), TEXT("PosX"), -9999);
   int posY = GlobalConfig->GetInt(TEXT("General"), TEXT("PosY"), -9999);

   bool bInsideMonitors = false;
   for (UINT i = 0; i < monitors.Num(); i++) {
      if (posX >= monitors[i].rect.left && posX < monitors[i].rect.right  &&
          posY >= monitors[i].rect.top  && posY < monitors[i].rect.bottom) {
         bInsideMonitors = true;
         break;
      }
   }

   if (bInsideMonitors) {
      x = posX;
      y = posY;
   }

   x = 0;
   y = 0;

   
   RECT crc;
   GetWindowRect(frameWnd, &crc);
   cx = crc.right - crc.left;
   cy = crc.bottom - crc.top;
   mMainWidgetHwnd=frameWnd;

   mRenderWidgetHwnd=renderWnd;

   hStartupShutdownMutex = OSCreateMutex();
   //-----------------------------------------------------
   CreateOBSApiInterface(this, logPath);
   
   //  TODO: Should these be stored in the config file?
   bRenderViewEnabled = GlobalConfig->GetInt(TEXT("General"), TEXT("PreviewEnabled"), 1) != 0;
   renderFrameIn1To1Mode = false;

   if (GlobalConfig->GetInt(TEXT("General"), TEXT("ShowWebrootWarning"), TRUE) && IsWebrootLoaded())
      OBSMessageBox(mMainWidgetHwnd, TEXT("Webroot Secureanywhere appears to be active.  This product will cause problems with OBS as the security features block OBS from accessing Windows GDI functions.  It is highly recommended that you disable Secureanywhere and restart OBS.\r\n\r\nOf course you can always just ignore this message if you want, but it may prevent you from being able to stream certain things. Please do not report any bugs you may encounter if you leave Secureanywhere enabled."), TEXT("Just a slight issue you might want to be aware of"), MB_OK);
 
   renderFrameIn1To1Mode = false;
   mDataMutex = OSCreateMutex();
   mSetSceneVisibleEvent=CreateEvent(NULL, TRUE, FALSE, NULL);
   
}


OBS::~OBS() {
   mThis=NULL;
   Stop(false);

   scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
   scenesConfig.Close(true);

   for (UINT i = 0; i < sceneClasses.Num(); i++)
      sceneClasses[i].FreeData();
   for (UINT i = 0; i < imageSourceClasses.Num(); i++)
      imageSourceClasses[i].FreeData();

   DestoryOBSApiInterface();

   if(hStartupShutdownMutex)
   {
      OSCloseMutex(hStartupShutdownMutex);
      hStartupShutdownMutex=NULL;
   }
   if (hVideoEvent)
   {
      CloseHandle(hVideoEvent);
      hVideoEvent=NULL;
   }
   if (hSceneMutex)
   {   
      OSCloseMutex(hSceneMutex);
      hSceneMutex=NULL;
   }
   if(mDataMutex)
   {
      OSCloseMutex(mDataMutex);
      mDataMutex=NULL;
   }
   if(mSetSceneVisibleEvent)
   {
      CloseHandle(mSetSceneVisibleEvent);

      mSetSceneVisibleEvent=NULL;   
   }
}

//void OBS::EnterSceneMutex() {
//   OSEnterMutex(hSceneMutex); 
//}
//
//void OBS::LeaveSceneMutex(){ 
//   OSLeaveMutex(hSceneMutex); 
//}

void OBS::ResizeRenderFrame(bool bRedrawRenderFrame) {
   // Get output steam size and aspect ratio
   int curCX, curCY;
   float mainAspect;
   if (bRunning) {
      curCX = outputCX;
      curCY = outputCY;
      mainAspect = float(curCX) / float(curCY);
   } else {
      // Default to the monitor's resolution if the base size is undefined
      int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
      if (monitorID >= (int)monitors.Num())
         monitorID = 0;

      if(monitors.Num()<=monitorID)
      {
         return ;
      }
      
      RECT &screenRect = monitors[monitorID].rect;
      int defCX = screenRect.right - screenRect.left;
      int defCY = screenRect.bottom - screenRect.top;

      // Calculate output size using the same algorithm that's in OBS::Start()
      float scale = AppConfig->GetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
      curCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"), defCX);
      curCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
      curCX = MIN(MAX(curCX, 128), 4096);
      curCY = MIN(MAX(curCY, 128), 4096);
      curCX = UINT(double(curCX) / double(scale));
      curCY = UINT(double(curCY) / double(scale));
      curCX = curCX & 0xFFFFFFFC; // Align width to 128bit for fast SSE YUV4:2:0 conversion
      curCY = curCY & 0xFFFFFFFE;

      mainAspect = float(curCX) / float(curCY);
   }

   // Get area to render in
   int x, y;
   UINT controlWidth = clientWidth;
   UINT controlHeight = clientHeight;


   UINT newRenderFrameWidth, newRenderFrameHeight;
   if (renderFrameIn1To1Mode) {
      newRenderFrameWidth = (UINT)curCX;
      newRenderFrameHeight = (UINT)curCY;
      x = (int)controlWidth / 2 - curCX / 2;
      y = (int)controlHeight / 2 - curCY / 2;
   } else { // Scale to fit
      Vect2 renderSize = Vect2(float(controlWidth), float(controlHeight));
      float renderAspect = renderSize.x / renderSize.y;

      if (renderAspect > mainAspect) {
         renderSize.x = renderSize.y*mainAspect;
         x = int((float(controlWidth) - renderSize.x)*0.5f);
         y = 0;
      } else {
         renderSize.y = renderSize.x / mainAspect;
         x = 0;
         y = int((float(controlHeight) - renderSize.y)*0.5f);
      }

      // Round and ensure even size
      newRenderFrameWidth = int(renderSize.x + 0.5f) & 0xFFFFFFFE;
      newRenderFrameHeight = int(renderSize.y + 0.5f) & 0xFFFFFFFE;
   }
   
   renderFrameX = x;
   renderFrameY = y;
   renderFrameWidth = newRenderFrameWidth;
   renderFrameHeight = newRenderFrameHeight;
   renderFrameCtrlWidth = controlWidth;
   renderFrameCtrlHeight = controlHeight;
   if (!bRunning) {
      oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
      oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
      InvalidateRect(hwndRenderMessage, NULL, true); // Repaint text
   } else if (bRunning && bRedrawRenderFrame) {
      oldRenderFrameCtrlWidth = renderFrameCtrlWidth;
      oldRenderFrameCtrlHeight = renderFrameCtrlHeight;
      bResizeRenderView = true;
   }
}
void OBS::GetBaseSize(UINT &width, UINT &height) const {
   if (bRunning) {
      width = baseCX;
      height = baseCY;
   } else {
      int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
      if (monitorID >= (int)monitors.Num())
         monitorID = 0;

      RECT &screenRect = monitors[monitorID].rect;
      int defCX = screenRect.right - screenRect.left;
      int defCY = screenRect.bottom - screenRect.top;

      width = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"), defCX);
      height = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);
   }
}

void OBS::GetThreadHandles(HANDLE *videoThread) {
   if (hVideoThread)
      *videoThread = hVideoThread;
}
const MonitorInfo &OBSGetMonitor(UINT id)
{
   return OBS::mThis->GetMonitor(id);
}
bool OBS::SetSourceVisible(wchar_t *sourceName,bool isVisible,bool isWait)
{
   bool ret=false;
   EnterSceneMutex();
   ret=scene->SetSourceVisible(sourceName,isVisible);
   mBIsSetSceneVisible=isWait;
   LeaveSceneMutex();
   return ret;
}
bool OBS::WaitSetSourceVisible()
{
   bool ret= WaitForSingleObject(mSetSceneVisibleEvent, 3000)!=WAIT_TIMEOUT;
   Log(TEXT("OBS::WaitSetSourceVisible() end"));
   ResetEvent(mSetSceneVisibleEvent);
   return ret;
}

void OBS::FitItemsToScreen(SceneItem *item) {
   Vect2 baseSize = GetBaseSize();
   double baseAspect = double(baseSize.x) / double(baseSize.y);
   if (item->source) {
      Vect2 itemSize = item->source->GetSize();
      itemSize.x -= (item->crop.x + item->crop.w);
      itemSize.y -= (item->crop.y + item->crop.z);

      Vect2 size = baseSize;
      double sourceAspect = double(itemSize.x) / double(itemSize.y);
      if (!CloseDouble(baseAspect, sourceAspect)) {
         if (baseAspect < sourceAspect)
            size.y = float(double(size.x) / sourceAspect);
         else
            size.x = float(double(size.y) * sourceAspect);

         size.x = (float)round(size.x);
         size.y = (float)round(size.y);
      }

      Vect2 scale = itemSize / size;
      size.x += (item->crop.x + item->crop.w) / scale.x;
      size.y += (item->crop.y + item->crop.z) / scale.y;
      item->size = size;

      Vect2 pos;
      pos.x = (baseSize.x*0.5f) - ((item->size.x + item->GetCrop().x - item->GetCrop().w)*0.5f);
      pos.y = (baseSize.y*0.5f) - ((item->size.y + item->GetCrop().y - item->GetCrop().z)*0.5f);
      pos.x = (float)round(pos.x);
      pos.y = (float)round(pos.y);
      item->pos = pos;

      XElement *itemElement = item->GetElement();
      if (itemElement) {
         XElement *data = itemElement->GetElement(L"data");
         if (!data) {
            data = itemElement->CreateElement(TEXT("data"));
         }

         float x = pos.x;
         float y = pos.y;
         float cx = size.x;
         float cy = size.y;

         x /= baseSize.x;
         y /= baseSize.y;
         cx /= baseSize.x;
         cy /= baseSize.y;

         data->SetFloat(TEXT("x"), x);
         data->SetFloat(TEXT("y"), y);
         data->SetFloat(TEXT("cx"), cx);
         data->SetFloat(TEXT("cy"), cy);
      }
   }
}
void OBS::UIWarning(wchar_t *d){
   if(this->mfuncPostCRMsg) {
      this->mfuncPostCRMsg(MSG_OBSCONTROL_STREAM_NOTIFY,d,(wcslen(d)+1)*sizeof(wchar_t));
   }
}
void OBS::SetPostMsgHook(IPOSTCRMessageFunc funcPostMsg){
   this->mfuncPostCRMsg=funcPostMsg;
}
void OBS::DoHideLogo(bool bHide) {
   EnterSceneMutex();
   if (scene) {
      scene->SceneHideSource(SRC_SOURCE_PERSISTENT, bHide);
   }
   LeaveSceneMutex();
}

void OBS::EnterStartUpShutDownMutex() {
   if (hStartupShutdownMutex) {
      OSEnterMutex(hStartupShutdownMutex);
   }
}

void OBS::LeaveStartUpShutDownMutex() {
   if (hStartupShutdownMutex) {
      OSLeaveMutex(hStartupShutdownMutex);
   }
}

