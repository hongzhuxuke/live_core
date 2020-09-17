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
#include "IAudioCapture.h"
#include <inttypes.h>

extern "C"
{
//#include "x264.h"
}

#include <memory>
void OBSImageProcessDestory();
void OBSImageProcess(LPBYTE input, int width, int inPitch, int outPitch, int height, int startY, int endY, LPBYTE *output);
void OBSImageProcessInit(int outputCX,int outputCY);

DWORD STDCALL OBS::MainCaptureThread(LPVOID lpUnused) {
   OBS *obs=(OBS *)lpUnused;
   obs->MainCaptureLoop();
   return 0;
}

#define NUM_OUT_BUFFERS 3

bool STDCALL SleepToNS(QWORD qwNSTime) {
   QWORD t = GetQPCTimeNS();

   if (t >= qwNSTime)
      return false;

   unsigned int milliseconds = (unsigned int)((qwNSTime - t) / 1000000);
   if (milliseconds > 1) //also accounts for windows 8 sleep problem
   {
      //trap suspicious sleeps that should never happen
      if (milliseconds > 10000) {
         Log(TEXT("Tried to sleep for %u seconds, that can't be right! Triggering breakpoint."), milliseconds);
         DebugBreak();
      }
      OSSleep(milliseconds);
   }

   for (;;) {
      t = GetQPCTimeNS();
      if (t >= qwNSTime)
         return true;
      Sleep(1);
   }
}

bool STDCALL SleepTo100NS(QWORD qw100NSTime) {
   QWORD t = GetQPCTime100NS();

   if (t >= qw100NSTime)
      return false;

   unsigned int milliseconds = (unsigned int)((qw100NSTime - t) / 10000);
   if (milliseconds > 1) //also accounts for windows 8 sleep problem
      OSSleep(milliseconds);

   for (;;) {
      t = GetQPCTime100NS();
      if (t >= qw100NSTime)
         return true;
      Sleep(1);
   }
}

#define LOGLONGFRAMESDEFAULT 0

DWORD OBS::ThreadOBSReset(OBS *obs) 
{
   OBSApiLog("ThreadOBSReset OBSGraphics_Lock");
   OBSGraphics_Lock();
   
   OBSApiLog("ThreadOBSReset::OBSGraphics_Lock");
   if(obs)
   {
      QWORD t;
      OSSleep(500);
      obs->LockCurrentFramePic(t,0);
      OBSApiLog("ThreadOBSReset::obs->Stop()");
      obs->Stop(true);
      OBSApiLog("ThreadOBSReset::obs->Start()");
      obs->Start();
      OBSApiLog("ThreadOBSReset::UnlockCurrentFramePic");
      obs->UnlockCurrentFramePic();
      OBSApiLog("ThreadOBSReset::UnlockCurrentFramePic end");
   }
   OBSApiLog("ThreadOBSReset::OBSGraphics_UnLock");
   OBSGraphics_UnLock();
   
   OBSApiLog("ThreadOBSReset::end");
   return 0;
}

void OBS::Reset(){
   if (bShutdownVideoThread)
      return;
   OSCreateThread((XTHREAD)ThreadOBSReset,this);
}
extern HWND gTest;

void OBS::DrawPreview(const Vect2 &renderFrameSize, const Vect2 &renderFrameOffset, const Vect2 &renderFrameCtrlSize, int curRenderTarget, PreviewDrawType type) {
   //OBSApiLog("OBS::DrawPreview");
   if (mainVertexShader) {
      LoadVertexShader(mainVertexShader);
      //OBSApiLog("OBS::DrawPreview LoadVertexShader mainVertexShader");
   }
   if (mainPixelShader) {
      LoadPixelShader(mainPixelShader);
      //OBSApiLog("OBS::DrawPreview LoadVertexShader mainPixelShader");
   }

   Ortho(0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y, 0.0f, -100.0f, 100.0f);
   if (type != Preview_Projector
       && (renderFrameCtrlSize.x != oldRenderFrameCtrlWidth
       || renderFrameCtrlSize.y != oldRenderFrameCtrlHeight)) {
      // User is drag resizing the window. We don't recreate the swap chains so our coordinates are wrong
      SetViewport(0.0f, 0.0f, (float)oldRenderFrameCtrlWidth, (float)oldRenderFrameCtrlHeight);
   } else
      SetViewport(0.0f, 0.0f, renderFrameCtrlSize.x, renderFrameCtrlSize.y);

   // Draw background (Black if fullscreen/projector, window colour otherwise)
   if (type == Preview_Fullscreen || type == Preview_Projector)
      ClearColorBuffer(0x000000);
   else
      //ClearColorBuffer(GetSysColor(COLOR_BTNFACE));
      ClearColorBuffer(0x000000);

   //OBSApiLog("OBS::DrawPreview DrawSprite");
   if (previewTexture) {
      DrawSprite(
         previewTexture,
         0xFFFFFFFF,
         renderFrameOffset.x,
         renderFrameOffset.y,
         renderFrameOffset.x + renderFrameSize.x,
         renderFrameOffset.y + renderFrameSize.y);
   }
   //OBSApiLog("OBS::DrawPreview DrawSprite end");
}

const float yuvFullMat[][16] = {
   { 0.000000f, 1.000000f, 0.000000f, 0.000000f,
   0.000000f, 0.000000f, 1.000000f, 0.000000f,
   1.000000f, 0.000000f, 0.000000f, 0.000000f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.250000f, 0.500000f, 0.250000f, 0.000000f,
   -0.249020f, 0.498039f, -0.249020f, 0.501961f,
   0.498039f, 0.000000f, -0.498039f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.262700f, 0.678000f, 0.059300f, 0.000000f,
   -0.139082f, -0.358957f, 0.498039f, 0.501961f,
   0.498039f, -0.457983f, -0.040057f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.212600f, 0.715200f, 0.072200f, 0.000000f,
   -0.114123f, -0.383916f, 0.498039f, 0.501961f,
   0.498039f, -0.452372f, -0.045667f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.212200f, 0.701300f, 0.086500f, 0.000000f,
   -0.115691f, -0.382348f, 0.498039f, 0.501961f,
   0.498039f, -0.443355f, -0.054684f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.299000f, 0.587000f, 0.114000f, 0.000000f,
   -0.168074f, -0.329965f, 0.498039f, 0.501961f,
   0.498039f, -0.417046f, -0.080994f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },
};

const float yuvMat[][16] = {
   { 0.000000f, 0.858824f, 0.000000f, 0.062745f,
   0.000000f, 0.000000f, 0.858824f, 0.062745f,
   0.858824f, 0.000000f, 0.000000f, 0.062745f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.214706f, 0.429412f, 0.214706f, 0.062745f,
   -0.219608f, 0.439216f, -0.219608f, 0.501961f,
   0.439216f, 0.000000f, -0.439216f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.225613f, 0.582282f, 0.050928f, 0.062745f,
   -0.122655f, -0.316560f, 0.439216f, 0.501961f,
   0.439216f, -0.403890f, -0.035325f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.182586f, 0.614231f, 0.062007f, 0.062745f,
   -0.100644f, -0.338572f, 0.439216f, 0.501961f,
   0.439216f, -0.398942f, -0.040274f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.182242f, 0.602293f, 0.074288f, 0.062745f,
   -0.102027f, -0.337189f, 0.439216f, 0.501961f,
   0.439216f, -0.390990f, -0.048226f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },

   { 0.256788f, 0.504129f, 0.097906f, 0.062745f,
   -0.148223f, -0.290993f, 0.439216f, 0.501961f,
   0.439216f, -0.367788f, -0.071427f, 0.501961f,
   0.000000f, 0.000000f, 0.000000f, 1.000000f },
};


//todo: this function is an abomination, this is just disgusting.  fix it.
//...seriously, this is really, really horrible.  I mean this is amazingly bad.
void OBS::MainCaptureLoop() {
   OBSApiLog("OBS::MainCaptureLoop Start");
   int curRenderTarget = 0, curYUVTexture = 0, curCopyTexture = 0;
   int copyWait = NUM_RENDER_BUFFERS - 1;
   //FILE*    mMixRgb = fopen("E:/1/aaa.yuv", "wb");
   bEditMode = true;
   bSizeChanging = false;
   bool bLogLongFramesProfile = GlobalConfig->GetInt(TEXT("General"), TEXT("LogLongFramesProfile"), LOGLONGFRAMESDEFAULT) != 0;
   float logLongFramesProfilePercentage = GlobalConfig->GetFloat(TEXT("General"), TEXT("LogLongFramesProfilePercentage"), 10.f);

   Vect2 baseSize = Vect2(float(baseCX), float(baseCY));
   Vect2 outputSize = Vect2(float(outputCX), float(outputCY));
   Vect2 scaleSize = Vect2(float(scaleCX), float(scaleCY));

   HANDLE hMatrix = yuvScalePixelShader->GetParameterByName(TEXT("yuvMat"));
   HANDLE hScaleVal = yuvScalePixelShader->GetParameterByName(TEXT("baseDimensionI"));

   HANDLE hTransitionTime = transitionPixelShader->GetParameterByName(TEXT("transitionTime"));

   //----------------------------------------
   // x264 input buffers

   int curOutBuffer = 0;


   QWORD lastAdjustmentTime = 0;
   UINT adjustmentStreamId = 0;

   bool bWasLaggedFrame = false;


   DWORD fpsCounter = 0;

   int numLongFrames = 0;
   int numTotalFrames = 0;

   curStrain = 0.0;

   QWORD lastBytesSent[3] = { 0, 0, 0 };
   double bpsTime = 0.0;

   double lastStrain = 0.0f;
   DWORD numSecondsWaited = 0;

   bool bEncode;
   bool bFirstFrame = true;
   bool bFirstImage = true;
   bool bFirstEncode = true;
   OBSImageProcessInit(outputCX,outputCY);
   //----------------------------------------
   //FILE* mMixRGB = fopen("E:\\1\\aa.rgb", "wb");
   QWORD streamTimeStart = GetQPCTimeNS();
   QWORD lastStreamTime = 0;
   QWORD firstFrameTimeMS = streamTimeStart / 1000000;
   QWORD frameLengthNS = 1000000000 / fps;
   OBSApiLog("OBS::MainCaptureLoop start capture");

   while (WaitForSingleObject(hVideoEvent, INFINITE) == WAIT_OBJECT_0) {
      if (bShutdownVideoThread)
         break;
      QWORD lastSendDataTime = GetQPCTimeMS();

      SetEvent(hVideoEvent);
      
      QWORD renderStartTime = GetQPCTimeNS();
      bool bRenderView = !IsIconic(mMainWidgetHwnd) && bRenderViewEnabled;
      QWORD renderStartTimeMS = renderStartTime / 1000000;
      QWORD curStreamTime = 0;
      if (!lastStreamTime)
         lastStreamTime = curStreamTime - frameLengthNS;

      QWORD frameDelta = curStreamTime - lastStreamTime;
      double fSeconds = double(frameDelta)*0.000000001;
      OSEnterMutex(hSceneMutex);

      if (bResizeRenderView) {
         OBSGraphics_ResizeView();
         bResizeRenderView = false;
      }

      if (scene) {
         Vect2 rect = GetOutputSize();
         scene->Preprocess(rect, GetRenderFrameOffset());
         for (UINT i = 0; i < globalSources.Num(); i++) {
            globalSources[i].source->Preprocess();
         }
         scene->Tick(float(fSeconds));
         for (UINT i = 0; i < globalSources.Num(); i++) {
            globalSources[i].source->Tick(float(fSeconds));
         }
      }

      QWORD curBytesSent = 0;
      if (numSecondsWaited) {
         //reset stats if the network disappears
         bpsTime = 0;
         numSecondsWaited = 0;
         curBytesSent = 0;
         zero(lastBytesSent, sizeof(lastBytesSent));
      }

      bpsTime += fSeconds;
      if (bpsTime > 1.0f) {
         if (numSecondsWaited < 3) {
            ++numSecondsWaited;
         }

         if (bpsTime > 2.0) {
            bpsTime = 0.0f;
         }
         else {
            bpsTime -= 1.0;
         }

         if (numSecondsWaited == 3) {
            lastBytesSent[0] = lastBytesSent[1];
            lastBytesSent[1] = lastBytesSent[2];
            lastBytesSent[2] = curBytesSent;
         }
         else {
            lastBytesSent[numSecondsWaited] = curBytesSent;
         }
         fpsCounter = 0;
      }
      fpsCounter++;

      //向桌面共享纹理渲染。
      EnableBlending(FALSE);
      LoadVertexShader(mainVertexShader);
      LoadPixelShader(mainPixelShader);
      SetRenderTarget(desktopTexture);
      scene->Render(SceneRenderType::SceneRenderType_Desktop);
      EnableBlending(TRUE);
      BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
      //------------------------------------
      // 推流纹理渲染。
      LoadVertexShader(mainVertexShader);
      LoadPixelShader(mainPixelShader);
      //设置向主窗口渲染
      SetRenderTarget(mainRenderTextures[curRenderTarget]);
      Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);
      SetViewport(0, 0, baseSize.x, baseSize.y);
      if (scene)
         scene->Render(SceneRenderType::SceneRenderType_Stream);
      //------------------------------------

      if (bTransitioning) {
         if (!lastRenderTexture) {
            lastRenderTexture = CreateTexture(baseCX, baseCY, GS_BGRA, NULL, FALSE, TRUE);
            if (lastRenderTexture) {
               OBSGraphics_CopyTexture(lastRenderTexture,mainRenderTextures[lastRenderTarget]);
            }
            else {
               bTransitioning = false;
            }
         } 
         else if (transitionAlpha >= 1.0f) {
            delete lastRenderTexture;
            lastRenderTexture = NULL;
            bTransitioning = false;
         }
      }

      if (bTransitioning) {
         transitionAlpha += float(fSeconds) * 5.0f;
         if (transitionAlpha > 1.0f)
            transitionAlpha = 1.0f;
         //设置变换文理
         SetRenderTarget(transitionTexture);

         Shader *oldPixelShader = GetCurrentPixelShader();
         LoadPixelShader(transitionPixelShader);

         transitionPixelShader->SetFloat(hTransitionTime, transitionAlpha);
         LoadTexture(mainRenderTextures[curRenderTarget], 1U);

         DrawSpriteEx(lastRenderTexture, 0xFFFFFFFF, 0, 0, baseSize.x, baseSize.y, 0.0f, 0.0f, 1.0f, 1.0f);
         LoadTexture(nullptr, 1U);
         LoadPixelShader(oldPixelShader);
      }

      EnableBlending(FALSE);

      if (bRenderView) {
         // Cache
         const Vect2 renderFrameSize = GetRenderFrameSize();
         const Vect2 renderFrameOffset = GetRenderFrameOffset();
         const Vect2 renderFrameCtrlSize = GetRenderFrameControlSize();
         
         EnableBlending(TRUE);
         
         //设置向主窗口渲染
         SetRenderTarget(previewTexture);
         
         Ortho(0.0f, baseSize.x, baseSize.y, 0.0f, -100.0f, 100.0f);
         SetViewport(0, 0, baseSize.x, baseSize.y);

         ClearColorBuffer(0x000000);
         
         scene->Render(SceneRenderType::SceneRenderType_Preview);
         //scene->Render(SceneRenderType::SceneRenderType_Stream);
 
         EnableBlending(FALSE);
         //设置为空，向显示窗口渲染
         SetRenderTarget(NULL);
         DrawPreview(renderFrameSize, renderFrameOffset, renderFrameCtrlSize, curRenderTarget,Preview_Standard);

         //draw selections if in edit mode
         if (bEditMode/* && !bSizeChanging*/) {
            if (scene) {
               LoadVertexShader(solidVertexShader);
               LoadPixelShader(solidPixelShader);
               solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFFFF);
               scene->RenderSelections(solidPixelShader);
            }
         }
      } 

      //------------------------------------
      // actual stream output

      LoadVertexShader(mainVertexShader);
      LoadPixelShader(yuvScalePixelShader);

      Texture *yuvRenderTexture = yuvRenderTextures[curRenderTarget];
      //设置向YUV渲染
      SetRenderTarget(yuvRenderTexture);

      switch (colorDesc.matrix) {
      case ColorMatrix_GBR:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[0] : (float*)yuvMat[0]);
         break;
      case ColorMatrix_YCgCo:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[1] : (float*)yuvMat[1]);
         break;
      case ColorMatrix_BT2020NCL:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[2] : (float*)yuvMat[2]);
         break;
      case ColorMatrix_BT709:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[3] : (float*)yuvMat[3]);
         break;
      case ColorMatrix_SMPTE240M:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[4] : (float*)yuvMat[4]);
         break;
      default:
         yuvScalePixelShader->SetMatrix(hMatrix, colorDesc.fullRange ? (float*)yuvFullMat[5] : (float*)yuvMat[5]);
      }

      //待测试
      yuvScalePixelShader->SetVector2(hScaleVal, 1.0f / baseSize);
      
      Ortho(0.0f, outputSize.x, outputSize.y, 0.0f, -100.0f, 100.0f);
      SetViewport(0.0f, 0.0f, outputSize.x, outputSize.y);

      //why am I using scaleSize instead of outputSize for the texture?
      //because outputSize can be trimmed by up to three pixels due to 128-bit alignment.
      //using the scale function with outputSize can cause slightly inaccurate scaled images
      if (bTransitioning)
         DrawSpriteEx(transitionTexture, 0xFFFFFFFF, 0.0f, 0.0f, scaleSize.x, scaleSize.y, 0.0f, 0.0f, 1.0f, 1.0f);
      else
         DrawSpriteEx(mainRenderTextures[curRenderTarget], 0xFFFFFFFF, 0.0f, 0.0f, outputSize.x, outputSize.y, 0.0f, 0.0f, 1.0f, 1.0f);

      if (bRenderView && !copyWait)
      {
         //刷新
         OBSGraphics_Present();
      }

      OSLeaveMutex(hSceneMutex);
      bEncode = true;

      if (copyWait) {
         copyWait--;
         bEncode = false;
      } else {
         if (!bEncode) {
            if (curYUVTexture == (NUM_RENDER_BUFFERS - 1))
               curYUVTexture = 0;
            else
               curYUVTexture++;
         }
      }

      lastStreamTime = curStreamTime;
      if (bEncode) {
         UINT prevCopyTexture = (curCopyTexture == 0) ? NUM_RENDER_BUFFERS - 1 : curCopyTexture - 1;
         
         Texture *copyTexture = copyTextures[curCopyTexture];
         if (!bFirstEncode && copyTexture) {
            copyTexture->Unmap();
         }
         
         Texture *d3dYUV = yuvRenderTextures[curYUVTexture];
         if (copyTexture && d3dYUV) {
             OBSGraphics_CopyTexture(copyTexture, d3dYUV);
         }

         Texture *prevTexture = copyTextures[prevCopyTexture];
         if (bFirstImage) //ignore the first frame
            bFirstImage = false;
         else if(prevTexture){
            BYTE *lpData=NULL;
            UINT pitch=0;
            
            if (prevTexture->Map(lpData,pitch)) {
               int prevOutBuffer = (curOutBuffer == 0) ? NUM_OUT_BUFFERS - 1 : curOutBuffer - 1;
               int nextOutBuffer = (curOutBuffer == NUM_OUT_BUFFERS - 1) ? 0 : curOutBuffer + 1;
               OSEnterMutex(mDataMutex);
               BYTE* p = lpData;
               OBSYUVData data=mFrameList.front();
               mFrameList.pop_front();
               currentFrameYUV[0] = data.yuvData;
               currentFrameYUV[1] = data.yuvData + outputCX * outputCY;
               OBSImageProcess(lpData,outputCX, pitch, outputCX,outputCY,0,outputCY, (LPBYTE *)currentFrameYUV);
               data.pts = GetQPCTimeMS();
               mFrameList.push_back(data);
               this->currentFrameSyncTime=mFrameList.front().pts;
               OSLeaveMutex(mDataMutex);

              if (bFirstEncode)
                 bFirstEncode = bEncode = false;

               if (bEncode) {
                  //将指针与输出指针交换
 
                  static int index=0;
                  if(mBIsSetSceneVisible)
                  {
                     if(index>NUM_OUT_BUFFERS)
                     {
                        Log(TEXT("RESET mSetSceneVisibleEvent"));
                        mBIsSetSceneVisible=false;
                        SetEvent(mSetSceneVisibleEvent);
                        index=0;
                     }
                     else
                     {
                        index++;
                     }
                  }
               }

               curOutBuffer = nextOutBuffer;

               prevTexture->Unmap();
            } 
            else
            {
               //异常处理
               Reset();
               break;
            }
         }

         if (curCopyTexture == (NUM_RENDER_BUFFERS - 1))
            curCopyTexture = 0;
         else
            curCopyTexture++;

         if (curYUVTexture == (NUM_RENDER_BUFFERS - 1))
            curYUVTexture = 0;
         else
            curYUVTexture++;

      }
      lastRenderTarget = curRenderTarget;

      if (curRenderTarget == (NUM_RENDER_BUFFERS - 1))
         curRenderTarget = 0;
      else
         curRenderTarget++;

      OBSGraphics_Flush();
      
      if (bWasLaggedFrame = (frameDelta > frameLengthNS)) {
         numLongFrames++;
         if (bLogLongFramesProfile && (numLongFrames / float(max(1, numTotalFrames)) * 100.) > logLongFramesProfilePercentage)
            DumpLastProfileData();
      }
      numTotalFrames++;


      QWORD endTimestamp = GetQPCTimeMS();
      int processTime = endTimestamp - lastSendDataTime;
      if (processTime > 0 && processTime < 40) {
         int sleepTime = 40 - processTime - 3;
         //int sleepTime = diff >= 2 ? diff / 2 : 1;
         if (sleepTime > 0) {
            OSSleep(sleepTime);
         }
      }
   }
  
   if (!bFirstEncode && copyTextures[curCopyTexture]) {
      copyTextures[curCopyTexture]->Unmap();
   }

   Log(TEXT("Total frames rendered: %d, number of late frames: %d (%0.2f%%) (it's okay for some frames to be late)"), numTotalFrames, numLongFrames, (numTotalFrames > 0) ? (double(numLongFrames) / double(numTotalFrames))*100.0 : 0.0f);
   OBSImageProcessDestory();
}

void OBS::SetSyncWithEncoder( const bool & isWillsync)
{
   SetEvent(hVideoEvent);
}
unsigned char** OBS::LockCurrentFramePic(QWORD &syncTime,unsigned long long currentTime) {
   OSEnterMutex(mDataMutex);
   SetEvent(hVideoEvent);
   syncTime=this->currentFrameSyncTime;
   if (mFrameList.size())
   {
      currentFrameYUV[0]=mFrameList.front().yuvData;
      currentFrameYUV[1]=mFrameList.front().yuvData+outputCX*outputCY;
      
      if(currentTime!=0){
         currentFrameYUV[0]=mFrameList.back().yuvData;
         currentFrameYUV[1]=mFrameList.back().yuvData+outputCX*outputCY;
         syncTime=mFrameList.back().pts;
         int index=0;
         for(auto itor = mFrameList.begin();itor!=mFrameList.end();itor++) {
            if(currentTime /*+ AUDIOENGINE_VIDEO_DIFF*/ <= itor->pts) {
               currentFrameYUV[0]=itor->yuvData;
               currentFrameYUV[1]=itor->yuvData+outputCX*outputCY;
               syncTime=itor->pts;
               break;
            }
            index++;
         }
      }
      
      return  (unsigned char**)currentFrameYUV;
   }
   else
      return NULL;
}
void OBS::UnlockCurrentFramePic() {
   OSLeaveMutex(mDataMutex);
}




