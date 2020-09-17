
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
#include <time.h>
#include <Avrt.h>
void OBS::ToggleCapturing() {
   if (!bRunning)
      Start();
   else
      Stop(true);
}

void OBS::Start() {
   if (bRunning) return;
   OSEnterMutex(hStartupShutdownMutex);


   scenesConfig.SaveTo(String() << lpAppDataPath << "\\scenes.xconfig");
   scenesConfig.Save();

   //-------------------------------------------------------------

   fps = AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);
   frameTime = 1000 / fps;

   OSCheckForBuggyDLLs();

   //-------------------------------------------------------------
retryHookTest:
   bool alreadyWarnedAboutModules = false;
   if (OSIncompatibleModulesLoaded()) {
      //Log(TEXT("Incompatible modules (pre-D3D) detected."));
      int ret = OBSMessageBox(mMainWidgetHwnd, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
      if (ret == IDABORT) {
         OSLeaveMutex(hStartupShutdownMutex);
         return;
      } else if (ret == IDRETRY) {
         goto retryHookTest;
      }

      alreadyWarnedAboutModules = true;
   }

   String strPatchesError;
   if (OSIncompatiblePatchesLoaded(strPatchesError)) {
      OSLeaveMutex(hStartupShutdownMutex);
      OBSMessageBox(mMainWidgetHwnd, strPatchesError.Array(), NULL, MB_ICONERROR);
      //Log(TEXT("Incompatible patches detected."));
      return;
   }

   //check the user isn't trying to stream or record with no sources which is typically
   //a configuration error

   bool foundSource = false;
   XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
   if (scenes) {
      UINT numScenes = scenes->NumElements();

      for (UINT i = 0; i < numScenes; i++) {
         XElement *sceneElement = scenes->GetElementByID(i);
         XElement *sources = sceneElement->GetElement(TEXT("sources"));
         if (sources && sources->NumElements()) {
            foundSource = true;
            break;
         }
      }
   }
   //-------------------------------------------------------------

   String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
   if (!scmp(processPriority, TEXT("Idle")))
      SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
   else if (!scmp(processPriority, TEXT("Above Normal")))
      SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
   else if (!scmp(processPriority, TEXT("High")))
      SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

   //Log(TEXT("=====Stream Start: %s==============================================="), CurrentDateTimeString().Array());

   //-------------------------------------------------------------

   int monitorID = AppConfig->GetInt(TEXT("Video"), TEXT("Monitor"));
   if (monitorID >= (int)monitors.Num())
      monitorID = 0;

   RECT &screenRect = monitors[monitorID].rect;
   int defCX = screenRect.right - screenRect.left;
   int defCY = screenRect.bottom - screenRect.top;

   downscaleType = AppConfig->GetInt(TEXT("Video"), TEXT("Filter"), 0);
   baseCX = AppConfig->GetInt(TEXT("Video"), TEXT("BaseWidth"), defCX);
   baseCY = AppConfig->GetInt(TEXT("Video"), TEXT("BaseHeight"), defCY);

   baseCX = MIN(MAX(baseCX, 128), 4096);
   baseCY = MIN(MAX(baseCY, 128), 4096);

   scaleCX = AppConfig->GetInt(TEXT("Video"), TEXT("OutputCX"), baseCX);
   scaleCY = AppConfig->GetInt(TEXT("Video"), TEXT("OutputCY"), baseCY);


   int LastSizeX=AppConfig->GetInt(TEXT("LastSize"), TEXT("x"),0);
   int LastSizeY=AppConfig->GetInt(TEXT("LastSize"), TEXT("y"),0);

   //align width to 128bit for fast SSE YUV4:2:0 conversion
   outputCX = scaleCX & 0xFFFFFFFC;
   outputCY = scaleCY & 0xFFFFFFFE;

   bUseMultithreadedOptimizations = AppConfig->GetInt(TEXT("General"), TEXT("UseMultithreadedOptimizations"), TRUE) != 0;
   //Log(TEXT("  Multithreaded optimizations: %s"), (CTSTR)(bUseMultithreadedOptimizations ? TEXT("On") : TEXT("Off")));
   if (!OBSGraphics_Load(mRenderWidgetHwnd, renderFrameWidth, renderFrameHeight, GlobalConfig)) {
		Log(TEXT("Unable to Load Graphics Module"));
      CrashError(TEXT("Unable to Load Graphics Module"));

      return;
   }
   Log(TEXT("OBSGraphics_Init"));
   OBSGraphics_Init();
   Log(TEXT("OBSGraphics_Init end"));
   //Thanks to ASUS OSD hooking the goddamn user mode driver framework (!!!!), we have to re-check for dangerous
   //hooks after initializing D3D.
retryHookTestV2:
   if (!alreadyWarnedAboutModules) {
      if (OSIncompatibleModulesLoaded()) {
         //Log(TEXT("Incompatible modules (post-D3D) detected."));
         int ret = OBSMessageBox(mMainWidgetHwnd, Str("IncompatibleModules"), NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE);
         if (ret == IDABORT) {
            //FIXME: really need a better way to abort startup than this...
            //network.reset();
            OBSGraphics_Unload();
            OSLeaveMutex(hStartupShutdownMutex);
            return;
         } else if (ret == IDRETRY) {
            goto retryHookTestV2;
         }
      }
   }

   //-------------------------------------------------------------

   mainVertexShader = CreateVertexShaderFromFile(TEXT("shaders/DrawTexture.vShader"));
   mainPixelShader = CreatePixelShaderFromFile(TEXT("shaders/DrawTexture.pShader"));

   solidVertexShader = CreateVertexShaderFromFile(TEXT("shaders/DrawSolid.vShader"));
   solidPixelShader = CreatePixelShaderFromFile(TEXT("shaders/DrawSolid.pShader"));

   transitionPixelShader = CreatePixelShaderFromFile(TEXT("shaders/SceneTransition.pShader"));

   if (!mainVertexShader || !mainPixelShader)
      CrashError(TEXT("Unable to load DrawTexture shaders"));

   if (!solidVertexShader || !solidPixelShader)
      CrashError(TEXT("Unable to load DrawSolid shaders"));

   if (!transitionPixelShader)
      CrashError(TEXT("Unable to load SceneTransition shader"));

   //------------------------------------------------------------------

   CTSTR lpShader;

   switch (mScaleType) {
   case 0:
   default:
      lpShader = TEXT("shaders/DrawYUVTexture.pShader");
      break;
   case 1:
      lpShader = TEXT("shaders/DownscaleBilinear1YUV.pShader");
      break;
   case 2:
      lpShader = TEXT("shaders/DownscaleBicubicYUV.pShader");
      break;
   case 3:
      lpShader = TEXT("shaders/DownscaleLanczos6tapYUV.pShader");
      break;
   case 4:
      lpShader = TEXT("shaders/DownscaleBilinear9YUV.pShader");
      break;
   }

   yuvScalePixelShader = CreatePixelShaderFromFile(lpShader);
   if (!yuvScalePixelShader)
      CrashError(TEXT("Unable to create shader from file %s"), lpShader);

   //-------------------------------------------------------------

   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      mainRenderTextures[i] = CreateRenderTarget(baseCX, baseCY, GS_BGRA, FALSE);
      yuvRenderTextures[i] = CreateRenderTarget(outputCX, outputCY, GS_BGRA, FALSE);
   }

   int frameCount = VIDEO_FRAME_LIST_SIZE;
   for(int i=0;i<frameCount;i++) {
      OBSYUVData data;
      data.pts = 0;
      data.yuvData = new unsigned char [outputCX*outputCY*4];
      if (data.yuvData) {
          memset(data.yuvData, 0, outputCX*outputCY * 4);
      }
      mFrameList.push_back(data);
   }
      
   currentFrameYUV[0]=NULL;
   currentFrameYUV[1]=NULL;
   previewTexture = CreateRenderTarget(baseCX, baseCY, GS_BGRA, FALSE);
   transitionTexture = CreateRenderTarget(baseCX, baseCY, GS_BGRA, FALSE);

   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      copyTextures[i] = CreateReadTexture(outputCX, outputCY, GS_BGRA);
   }

   bRunning = true;
   EnterSceneMutex();
   if (sceneElement) {
      scene = CreateScene(sceneElement->GetString(TEXT("class")), sceneElement->GetElement(TEXT("data")));
      XElement *sources = sceneElement->GetElement(TEXT("sources"));
      if (sources) {
         UINT numSources = sources->NumElements();
         for (UINT i = 0; i < numSources; i++) {
            XElement *source = sources->GetElementByID(i);
            if (source) {
               XElement *data = source->GetElement(TEXT("data"));
               if (data) {
                  SceneItem *item = scene->AddImageSource(source);
                  float x = data->GetFloat(L"x", -1);
                  float y = data->GetFloat(L"y", -1);
                  float w = data->GetFloat(L"cx", -1);
                  float h = data->GetFloat(L"cy", -1);
                  if (!item) {
                     _ASSERT(FALSE);
                     LeaveSceneMutex();
                     return;
                  }
                  if(item->GetSourceType()==SRC_WINDOWS) {
                     int type=data->GetInt(L"sourceType", SRC_WINDOWS);
                     item->SetSourceType(type);
                  }

                  bool isFullScreen = data->GetInt(L"isFullscreen", 0) != 0;
                  if(isFullScreen){
                     FitItemsToScreen(item);
                     continue;
                  }

                  float k_this=baseCX;
                  k_this/=baseCY;
                  
                  float k_last=0.0f;
                  if(LastSizeX==0||LastSizeY==0)
                  {
                     k_last=k_this;  
                  }
                  else
                  {
                     k_last=LastSizeX;
                     k_last/=LastSizeY;
                  }
                  
                  //比例相等，等比例换算
                  if(k_this-k_last>-0.01f&&k_this-k_last<0.01f)
                  {
                     if (x > 0 && y > 0) {
                        item->pos.x = x*baseCX;
                        item->pos.y = y*baseCY;
                     }
                     if (w > 0 && h > 0) {
                        item->size.x = w*baseCX;
                        item->size.y = h*baseCY;
                     }

                  }
                  //比例不等
                  else
                  {
                     if (x > 0 && y > 0) {
                        item->pos.x = x*baseCX;
                        item->pos.y = y*baseCY;
                     }
                     if (w > 0 && h > 0) {
                        item->size.x = w*baseCX;
                        item->size.y = w*baseCX/item->source->GetSize().x*item->source->GetSize().y;                        
                     }
                  }
                  
                  //两张背景图不自适应
                  int persistent = item->GetElement()->GetInt(L"persistent", 0);
                  if (persistent == 1) {
                     //居中
                     if(x<0.5f&&x+w>0.5f&&y<0.5f&&y+h>0.5f)
                     {
                        item->size.x=w*baseCX;
                        item->size.y=item->size.x/item->source->GetSize().x*item->source->GetSize().y;
                        item->pos.x=(baseCX-item->size.x)/2;
                        item->pos.y=(baseCY-item->size.y)/2;
                     }
                     //右上角
                     else if(x>0.5f&&y<0.5f)
                     {
                        item->size.x=w*baseCX;
                        item->size.y=item->size.x/item->source->GetSize().x*item->source->GetSize().y;
                        item->pos.x=baseCX*(x);
                        item->pos.y=y*baseCY;
                     }
                  } 
               }
            }
         }
      }
      scene->BeginScene();
   }
   LeaveSceneMutex();
   //-------------------------------------------------------------

   int maxBitRate = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
   int bufferSize = maxBitRate;
   if (AppConfig->GetInt(L"Video Encoding", L"UseBufferSize", 0) != 0)
      bufferSize = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
   int quality = AppConfig->GetInt(TEXT("Video Encoding"), TEXT("Quality"), 8);
   String preset = AppConfig->GetString(TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));
   colorDesc.fullRange = AppConfig->GetInt(L"Video", L"FullRange") != 0;
   colorDesc.primaries = ColorPrimaries_BT709;
   colorDesc.transfer = ColorTransfer_IEC6196621;
   colorDesc.matrix = outputCX >= 1280 || outputCY > 576 ? ColorMatrix_BT709 : ColorMatrix_SMPTE170M;

   ResizeRenderFrame(true);
   bShutdownVideoThread = false;
   hVideoThread = OSCreateThread((XTHREAD)OBS::MainCaptureThread, this);

   SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, 0, 0);
   SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED);

   OSLeaveMutex(hStartupShutdownMutex);


}

void OBS::Stop(bool isRestart) {
   if (!bRunning)
      return;

   OSEnterMutex(hStartupShutdownMutex);
   //we only want the capture thread to stop first, so we can ensure all packets are flushed
   bShutdownVideoThread = true;
   SetEvent(hVideoEvent);
   if (hVideoThread) {
      OSTerminateThread(hVideoThread, 30000);
      hVideoThread = NULL;
   }

   bRunning = false;

   for (UINT i = 0; i < globalSources.Num(); i++)
      globalSources[i].source->EndScene();
   
   EnterSceneMutex();
   if (scene)
      scene->EndScene();

   OBSGraphics_UnloadAllData();

   //-------------------------------------------------------------
   if (scene) {
      scene->SetIsRestart(isRestart);
      delete scene;
      scene = NULL;
   }
   LeaveSceneMutex();
   for (UINT i = 0; i < globalSources.Num(); i++)
      globalSources[i].FreeData();
   globalSources.Clear();

   while(mFrameList.size()) {
      OBSYUVData data=mFrameList.front();
      mFrameList.pop_front();
      delete data.yuvData;
   }
   
   currentFrameYUV[0]=NULL;
   currentFrameYUV[1]=NULL;

   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      OBS_DEL(mainRenderTextures[i]);
      OBS_DEL(yuvRenderTextures[i]);
   }
   
   OBS_DEL(previewTexture);

   for (UINT i = 0; i < NUM_RENDER_BUFFERS; i++) {
      OBS_DEL(copyTextures[i]);
   }
   OBS_DEL(transitionTexture);
   OBS_DEL(lastRenderTexture);
   OBS_DEL(mainVertexShader);
   OBS_DEL(mainPixelShader);      
   OBS_DEL(yuvScalePixelShader);
   OBS_DEL(transitionPixelShader);
   OBS_DEL(solidVertexShader);
   OBS_DEL(solidPixelShader)


   //-------------------------------------------------------------

   OBSGraphics_Unload();

   //-------------------------------------------------------------

   ResizeRenderFrame(false);
   //-------------------------------------------------------------

   DumpProfileData();
   FreeProfileData();
   //Log(TEXT("=====Stream End: %s================================================="), CurrentDateTimeString().Array());

   //update notification icon to reflect current status

   bEditMode = false;
   SendMessage(GetDlgItem(mMainWidgetHwnd, ID_SCENEEDITOR), BM_SETCHECK, BST_UNCHECKED, 0);
   EnableWindow(GetDlgItem(mMainWidgetHwnd, ID_SCENEEDITOR), FALSE);

   SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1, 0, 0);
   SetThreadExecutionState(ES_CONTINUOUS);

   String processPriority = AppConfig->GetString(TEXT("General"), TEXT("Priority"), TEXT("Normal"));
   if (scmp(processPriority, TEXT("Normal")))
      SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

   OSLeaveMutex(hStartupShutdownMutex);
}
