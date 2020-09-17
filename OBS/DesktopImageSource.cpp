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
#include <dwmapi.h>
#include <list>
#include <vector>
#include <mutex>
#include "VHMonitorCapture.h"
#include "VHDesktopCaptureInterface.h"
#include "ImageEnhance.h"
using namespace std;
using namespace vlive_desktopcatpure;

vector <VHD_WindowInfo> gWindowInfo;
unsigned int gDesktopIndex = 0;
static std::mutex gDesktopMutex;

#define NUM_CAPTURE_TEXTURES 2

class DesktopImageSource : public ImageSource,public CaputreCallback {
   Texture *renderTextures[NUM_CAPTURE_TEXTURES];
   Texture *desktopTexture[NUM_CAPTURE_TEXTURES];
   Texture *lastRendered = nullptr;
   Texture *mClientImageTexture = nullptr;
   Shader   *colorKeyShader = nullptr, *alphaIgnoreShader = nullptr;
   int      curCaptureTexture = 0;
   XElement *data = nullptr;

   bool     bUseColorKey = false, bUsePointFiltering = false;
   DWORD    keyColor;
   UINT     keySimilarity, keyBlend;

   UINT     opacity;
   int      gamma;
   VHD_WindowInfo m_windowInfo;
   int m_width;
   int m_height;
   int mScreenId = 0;
   VHDesktopCaptureInterface *mDesktopCapture = nullptr;
   bool mbIsCapture = false; // 控制桌面采集频率，较少采集频率

   std::atomic_bool mIsOpenEnhance = false;
   ImageEnhance* mImageEnhance = nullptr;
   int mEnhanceDesktopWidth = 0;
   int mEnhanceDesktopHeight = 0;
public:
   DesktopImageSource(UINT frameTime, XElement *data) {
      for (UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         renderTextures[i] = NULL;
         desktopTexture[i] = NULL;
      }
      this->data = data;

      CreateDesktopCapture();
      UpdateSettings();
      curCaptureTexture = 0;
      colorKeyShader = CreatePixelShaderFromFile(TEXT("shaders\\ColorKey_RGB.pShader"));
      alphaIgnoreShader = CreatePixelShaderFromFile(TEXT("shaders\\AlphaIgnore.pShader"));

      String clientImage = OBSAPI_GetAppPath();
      clientImage += TEXT("\\screen.png");
      mClientImageTexture =CreateTextureFromFile(clientImage, TRUE);
      OBSApiLog("DesktopImageSource::DesktopImageSource ok\n");
   }

   ~DesktopImageSource() {      
       for (int i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
           SafeDelete(renderTextures[i]);
       }
       for (int i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
          SafeDelete(desktopTexture[i]);
       }
      SafeDelete(alphaIgnoreShader);
      SafeDelete(colorKeyShader);
      SafeDelete(mClientImageTexture);
      std::unique_lock<std::mutex> lock(gDesktopMutex);
      OBSApiLog("DesktopImageSource::CreateDesktopCapture release type %d\n", m_windowInfo.type);
      DestoryHDesktopCaptureInstance(mDesktopCapture);
      mDesktopCapture = nullptr;
      OBSApiLog("~DesktopImageSource::DesktopImageSource ok\n");
   }

   void DesktopImageSource::CreateDesktopCapture() {
      std::unique_lock<std::mutex> lock(gDesktopMutex);
      if(mDesktopCapture == nullptr){
         OBSApiLog("DesktopImageSource::CreateDesktopCapture\n");
         mDesktopCapture = GetVHDesktopCaptureInstance();
         mDesktopCapture->Init();
         mDesktopCapture->Start(this);
         mDesktopCapture->SetScreenSoucce(mScreenId);
         OBSApiLog("DesktopImageSource::CreateDesktopCapture end\n");
      }
   }

   static BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM) {
      VHD_WindowInfo windowInfo;
      memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
      windowInfo.type = VHD_Desktop;
      windowInfo.screen_id = gDesktopIndex++;
      windowInfo.hwnd = (HWND)hMonitor;
      windowInfo.rect = *rect;
      gWindowInfo.push_back(windowInfo);
      return TRUE;
   }

   void SetDesktopImageEnhance(bool open){
      mIsOpenEnhance = open;
   }

   void DesktopImageSource::OnCaptureResult(CaputreResult result, const unsigned char*src_data, int width, int height){
      if(result == CaputreResult::CAPUTRE_SUCCESS){
         Texture *captureTexture = desktopTexture[curCaptureTexture];
         if(captureTexture){
            if (mEnhanceDesktopWidth != width || mEnhanceDesktopHeight != height) {
               mEnhanceDesktopWidth = width;
               mEnhanceDesktopHeight = height;
               VideoFilterParam param;
               param.width = width;
               param.height = height;
               if (mImageEnhance != nullptr) {
                  delete mImageEnhance;
                  mImageEnhance = nullptr;
               }
               mImageEnhance = new ImageEnhance;
               mImageEnhance->Init(&param);
            }
            if (width > 0 && height > 0 && (m_width != width || m_height != height)) {
               OBSApiLog("DesktopImageSource::OnCaptureResult size changed\n");
               gWindowInfo.clear();
               gDesktopIndex = 0;
               EnumDisplayMonitors(NULL, NULL, MonitorInfoEnumProc, NULL);
               for(int i = 0; i < gWindowInfo.size(); i++){
                  VHD_WindowInfo info = gWindowInfo.at(i);
                  if(info.screen_id == mScreenId && data){
                     info.captureType = m_windowInfo.captureType;
                     data->SetHexListPtr(L"winInfo", (unsigned char *)&info, sizeof(VHD_WindowInfo));
                     if(m_windowInfo.rect.bottom != info.rect.bottom || m_windowInfo.rect.left != info.rect.left 
                        || m_windowInfo.rect.right != info.rect.right|| m_windowInfo.rect.top != info.rect.top){
                        DesktopCaptureSizeChanged();
                     }
                     break;
                  }
               }
               return;
            }
            if (mImageEnhance && mIsOpenEnhance) {
               mImageEnhance->EdgeEnhance((uint32_t*)(src_data), (uint32_t*)(src_data), PixelFmt_RGB);
            }

            captureTexture->SetImage((void*)src_data, GS_IMAGEFORMAT_BGRA, width * 4);
            DrawSprite(captureTexture, 0xFFFFFFFF, renderFrameOffset.x, renderFrameOffset.y, renderFrameSize.x, renderFrameSize.y);
         }
      }
   }

   HDC GetCurrentHDC() {
      HDC hDC = NULL;

    Texture *captureTexture = renderTextures[curCaptureTexture];
    if (captureTexture)
         captureTexture->GetDC(hDC);

      return hDC;
   }

   void ReleaseCurrentHDC(HDC hDC) {
      Texture *captureTexture = renderTextures[curCaptureTexture];
      if(captureTexture) {
         captureTexture->ReleaseDC();
      }
   }

   void EndPreprocess() {
      lastRendered = renderTextures[curCaptureTexture];
      if (++curCaptureTexture == NUM_CAPTURE_TEXTURES)
         curCaptureTexture = 0;
   }

   bool Preprocess() {
      //if(m_windowInfo.type == VHD_Desktop) {
      //   float newW = GetSystemMetrics(SM_CXSCREEN);
      //   float newH = GetSystemMetrics(SM_CYSCREEN);
      //   if(m_width != newW || m_height != newH){
      //      UpdateSettings();
      //      EndPreprocess();
      //      return true;
      //   }
      //}

      if (m_windowInfo.type == VHD_Desktop) {
         if (m_windowInfo.captureType == VHD_Capture_DX) {
            std::unique_lock<std::mutex> lock(gDesktopMutex);
            if (mDesktopCapture) {
               mDesktopCapture->CaputreFrame();
            }
            return true;
         }else{
            gWindowInfo.clear();
            gDesktopIndex = 0;
            EnumDisplayMonitors(NULL, NULL, MonitorInfoEnumProc, NULL);
            for (int i = 0; i < gWindowInfo.size(); i++) {
               VHD_WindowInfo info = gWindowInfo.at(i);                    
               int width = m_windowInfo.rect.right - m_windowInfo.rect.left;
               int height = m_windowInfo.rect.bottom - m_windowInfo.rect.top;
               if ((info.screen_id == mScreenId && (m_width != width || m_height != height)) && data) {
                  OBSApiLog("DesktopImageSource bitblt screen size changed\n");
                  data->SetHexListPtr(L"winInfo", (unsigned char *)&info, sizeof(VHD_WindowInfo));
                  UpdateSettings();
                  EndPreprocess();
                  return true;
               }
            }
         }
      }

      HDC hDC;
      if (hDC = GetCurrentHDC()) {
         if (!VHD_Render(m_windowInfo, hDC, m_width, m_height, true, mIsOpenEnhance)) {
            ReleaseCurrentHDC(hDC);
            EndPreprocess();
            return false;
         }

         ReleaseCurrentHDC(hDC);
         if((m_windowInfo.rect.right-m_windowInfo.rect.left!=(int)m_width)||
            (m_windowInfo.rect.bottom - m_windowInfo.rect.top != (int)m_height)) {
            data->SetHexListPtr(L"winInfo", (unsigned char *)& m_windowInfo, sizeof(VHD_WindowInfo));
            UpdateSettings();
            EndPreprocess();
            return true;
         }
      } 
      if(m_windowInfo.type==VHD_Window&&IsIconic(m_windowInfo.hwnd)) {
         return true;
      }
      EndPreprocess();
      return true;
   }

   void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType renderType) {
      SamplerState *sampler = NULL;
      Vect2 ulCoord = Vect2(0.0f, 0.0f),lrCoord = Vect2(1.0f, 1.0f);
      if(SceneRenderType::SceneRenderType_Desktop == renderType) {
         return ;
      }

      if(renderType==SceneRenderType::SceneRenderType_Preview&&m_windowInfo.type==VHD_Desktop){
#ifdef SHOW_IMAGE_MODE
         if (mClientImageTexture != NULL) {
            DrawSpriteEx(mClientImageTexture, 0xFFFFFFFF,
               pos.x,
               pos.y,
               pos.x + size.x,
               pos.y + size.y,
               0,
               0,
               1,
               1);
         }
#else
      if (mClientImageTexture != NULL) {
         DrawSpriteEx(lastRendered, 0xFFFFFFFF,
            pos.x,
            pos.y,
            pos.x + size.x,
            pos.y + size.y,
            0,
            0,
            1,
            1);
      }
#endif // DEBUG
         return ;
      }


      if (lastRendered) {
         float fGamma = float(-(gamma - 100) + 100) * 0.01f;
         Shader *lastPixelShader = GetCurrentPixelShader();
         float fOpacity = float(opacity)*0.01f;
         DWORD opacity255 = DWORD(fOpacity*255.0f);
         if (bUseColorKey) {
            LoadPixelShader(colorKeyShader);
            HANDLE hGamma = colorKeyShader->GetParameterByName(TEXT("gamma"));
            if (hGamma)
               colorKeyShader->SetFloat(hGamma, fGamma);

            float fSimilarity = float(keySimilarity)*0.01f;
            float fBlend = float(keyBlend)*0.01f;

            colorKeyShader->SetColor(colorKeyShader->GetParameter(2), keyColor);
            colorKeyShader->SetFloat(colorKeyShader->GetParameter(3), fSimilarity);
            colorKeyShader->SetFloat(colorKeyShader->GetParameter(4), fBlend);

         } else {
            LoadPixelShader(alphaIgnoreShader);
            HANDLE hGamma = alphaIgnoreShader->GetParameterByName(TEXT("gamma"));
            if (hGamma)
               alphaIgnoreShader->SetFloat(hGamma, fGamma);
         }

         if (bUsePointFiltering) {
            SamplerInfo samplerinfo;
            samplerinfo.filter = GS_FILTER_POINT;
            sampler = CreateSamplerState(samplerinfo);
            LoadSamplerState(sampler, 0);
         }
         DrawSpriteExRotate(lastRendered, (opacity255 << 24) | 0xFFFFFF,
            pos.x, pos.y, pos.x + size.x, pos.y + size.y, 0.0f,
            ulCoord.x, ulCoord.y,
            lrCoord.x, lrCoord.y, 0.0f); 
         if (bUsePointFiltering) 
             delete(sampler); 

         LoadPixelShader(lastPixelShader);
      }
   }

   Vect2 GetSize() const {      
      return Vect2(float(m_width), float(m_height));
   }

   void DesktopCaptureSizeChanged(){
      OutputDebugStringA("################### DesktopImageSource UpdateSettings ###################\n");
      OBSAPI_EnterSceneMutex();
      data->GetHexListPtr(L"winInfo", (unsigned char *)& m_windowInfo, sizeof(VHD_WindowInfo));
      m_width = m_windowInfo.rect.right - m_windowInfo.rect.left;
      m_height = m_windowInfo.rect.bottom - m_windowInfo.rect.top;
      OBSApiLog("DesktopImageSource UpdateSetting CreateGDITexture m_width:%d  m_height:%d\n", m_width, m_height);
      for (UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         if (desktopTexture[i] != NULL) {
            delete desktopTexture[i];
            desktopTexture[i] = NULL;
         }
         Texture *tex = CreateTexture(m_width, m_height, GS_BGRA, nullptr, false, false);
         if (tex == NULL) {
            OBSApiLog("DesktopImageSource UpdateSetting CreateGDITexture Failed! %d %d\n", m_width, m_height);
            continue;
         }
         desktopTexture[i] = tex;
      }

      for (UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         lastRendered = desktopTexture[curCaptureTexture];
         if (++curCaptureTexture == NUM_CAPTURE_TEXTURES)
            curCaptureTexture = 0;
      }
      OBSAPI_LeaveSceneMutex();
   }

   void UpdateSettings() {
      OutputDebugStringA("################### DesktopImageSource UpdateSettings ###################\n");
      OBSAPI_EnterSceneMutex();      
      data->GetHexListPtr(L"winInfo", (unsigned char *)& m_windowInfo, sizeof(VHD_WindowInfo));
      gamma = 100;
      bool bNewUseColorKey = data->GetInt(TEXT("useColorKey"), 0) != 0;
      keyColor = data->GetInt(TEXT("keyColor"), 0xFFFFFFFF);
      keySimilarity = data->GetInt(TEXT("keySimilarity"), 10);
      keyBlend = data->GetInt(TEXT("keyBlend"), 0);
      bUsePointFiltering = data->GetInt(TEXT("usePointFiltering"), 0) != 0;
      bUseColorKey = bNewUseColorKey;
      opacity = data->GetInt(TEXT("opacity"), 100);

      //only first screen
      if(m_windowInfo.type == VHD_Desktop) {
         if (m_windowInfo.screen_id != mScreenId && m_windowInfo.captureType == VHD_Capture_DX) {
            mScreenId = m_windowInfo.screen_id;
            std::unique_lock<std::mutex> lock(gDesktopMutex);
            if (mDesktopCapture) {
               mDesktopCapture->SetScreenSoucce(m_windowInfo.screen_id);
            }
         }
      }   
      
      m_width = m_windowInfo.rect.right - m_windowInfo.rect.left;
      m_height = m_windowInfo.rect.bottom - m_windowInfo.rect.top;
      OBSApiLog("DesktopImageSource UpdateSetting CreateGDITexture m_width:%d  m_height:%d\n", m_width, m_height);
      for (UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         if (desktopTexture[i] != NULL) {
            delete desktopTexture[i];
            desktopTexture[i] = NULL;
         }
         Texture *tex = CreateTexture(m_width, m_height, GS_BGRA, nullptr, false, false);
         if (tex == NULL) {
            OBSApiLog("DesktopImageSource UpdateSetting CreateGDITexture Failed! %d %d\n", m_width, m_height);
            continue;
         }
         desktopTexture[i] = tex;
      }

      for (UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         if(renderTextures[i]!=NULL) {
            delete renderTextures[i];
            renderTextures[i]=NULL;
         }
         Texture *tex = CreateGDITexture(m_width, m_height);
         if(tex == NULL) {            
            OBSApiLog("DesktopImageSource UpdateSetting CreateGDITexture Failed! %d %d\n",m_width, m_height);
            continue;
         }
         renderTextures[i] = tex;
         
         HDC hdc=NULL;
         renderTextures[i]->GetDC(hdc);
         if(hdc) {
            HBRUSH hbrush = CreateSolidBrush(RGB(0,0,0));
            SelectObject(hdc, hbrush);
            Rectangle(hdc,0,0, m_width, m_height);
            DeleteObject(hbrush);
            renderTextures[i]->ReleaseDC();
         }
      }
      
      for(UINT i = 0; i < NUM_CAPTURE_TEXTURES; i++) {
         Preprocess();
         EndPreprocess();
      }
      OBSAPI_LeaveSceneMutex();

      if(VHD_Desktop==m_windowInfo.type) {
         OBSApiLog("DesktopImageSource UpdateSetting [VHD_Desktop] %d %d\n",VHD_Desktop,m_windowInfo.type);
      }
      else if(VHD_Window==m_windowInfo.type){
         OBSApiLog("DesktopImageSource UpdateSetting [VHD_Window] %d %d\n",VHD_Window,m_windowInfo.type);
         OBSApiLog("DesktopImageSource UpdateSetting [VHD_Window]size %d %d %d %d\n",
         m_windowInfo.rect.right ,m_windowInfo.rect.left, m_windowInfo.rect.bottom ,m_windowInfo.rect.top);
      }
      else if(VHD_AREA==m_windowInfo.type){
         OBSApiLog("DesktopImageSource UpdateSetting [VHD_AREA] %d %d\n",VHD_AREA,m_windowInfo.type);
      }
      else {
         OBSApiLog("DesktopImageSource UpdateSetting %d\n",m_windowInfo.type);
      } 
   }

   void SetInt(CTSTR lpName, int iVal) {
      if (scmpi(lpName, TEXT("useColorKey")) == 0) {
         bool bNewVal = iVal != 0;
         bUseColorKey = bNewVal;
      } else if (scmpi(lpName, TEXT("keyColor")) == 0) {
         keyColor = (DWORD)iVal;
      } else if (scmpi(lpName, TEXT("keySimilarity")) == 0) {
         keySimilarity = iVal;
      } else if (scmpi(lpName, TEXT("keyBlend")) == 0) {
         keyBlend = iVal;
      } else if (scmpi(lpName, TEXT("opacity")) == 0) {
         opacity = (UINT)iVal;
      } else if (scmpi(lpName, TEXT("gamma")) == 0) {
         gamma = iVal;
         if (gamma < 50)        gamma = 50;
         else if (gamma > 175)  gamma = 175;
      }
   }
};



ImageSource* STDCALL CreateDesktopSource(XElement *data) {
   if (!data)
      return NULL;

   return new DesktopImageSource(OBSAPI_GetFrameTime(), data);
}

static bool STDCALL ConfigureDesktopSource2(XElement *element, bool bInitialize, int dialogID) {
   return true;
}

bool STDCALL ConfigureDesktopSource(XElement *element, bool bInitialize) {
   return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREDESKTOPSOURCE);
}

bool STDCALL ConfigureWindowCaptureSource(XElement *element, bool bInitialize) {
   return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREWINDOWCAPTURE);
}

bool STDCALL ConfigureMonitorCaptureSource(XElement *element, bool bInitialize) {
   return ConfigureDesktopSource2(element, bInitialize, IDD_CONFIGUREMONITORCAPTURE);
}
