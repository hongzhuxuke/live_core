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

#include <XInput.h>
#define XINPUT_GAMEPAD_LEFT_TRIGGER    0x0400
#define XINPUT_GAMEPAD_RIGHT_TRIGGER   0x0800


void OBS::RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: No create procedure specified"));
        return;
    }

    if(GetSceneClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterSceneClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo   = sceneClasses.CreateNew();
    classInfo->strClass    = lpClassName;
    classInfo->strName     = lpDisplayName;
    classInfo->createProc  = createProc;
    classInfo->configProc  = configProc;
    classInfo->bDeprecated = bDeprecated;
}

void OBS::RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc, bool bDeprecated)
{
    if(!lpClassName || !*lpClassName)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No class name specified"));
        return;
    }

    if(!createProc)
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: No create procedure specified"));
        return;
    }

    if(GetImageSourceClass(lpClassName))
    {
        AppWarning(TEXT("OBS::RegisterImageSourceClass: Tried to register '%s', but it already exists"), lpClassName);
        return;
    }

    ClassInfo *classInfo   = imageSourceClasses.CreateNew();
    classInfo->strClass    = lpClassName;
    classInfo->strName     = lpDisplayName;
    classInfo->createProc  = createProc;
    classInfo->configProc  = configProc;
    classInfo->bDeprecated = bDeprecated;
}

Scene* OBS::CreateScene(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<sceneClasses.Num(); i++)
    {
        if(sceneClasses[i].strClass.CompareI(lpClassName))
            return (Scene*)sceneClasses[i].createProc(data);
    }

    AppWarning(TEXT("OBS::CreateScene: Could not find scene class '%s'"), lpClassName);
    return NULL;
}

ImageSource* OBS::CreateImageSource(CTSTR lpClassName, XElement *data)
{
    for(UINT i=0; i<imageSourceClasses.Num(); i++)
    {
       if (imageSourceClasses[i].strClass.CompareI(lpClassName))  {
          if (imageSourceClasses[i].createProc) {
             if (wcscmp(lpClassName, L"DeviceCapture") == 0) {
                printf("");

             }

             return (ImageSource*)imageSourceClasses[i].createProc(data);
          }
       }
    }

    AppWarning(TEXT("OBS::CreateImageSource: Could not find image source class '%s'"), lpClassName);
    return NULL;
}

void OBS::ConfigureScene(XElement *element)
{
    if(!element)
    {
        AppWarning(TEXT("OBS::ConfigureScene: NULL element specified"));
        return;
    }

    CTSTR lpClassName = element->GetString(TEXT("class"));
    if(!lpClassName)
    {
        AppWarning(TEXT("OBS::ConfigureScene: No class specified for scene '%s'"), element->GetName());
        return;
    }

    for(UINT i=0; i<sceneClasses.Num(); i++)
    {
        if(sceneClasses[i].strClass.CompareI(lpClassName))
        {
            if(sceneClasses[i].configProc)
                sceneClasses[i].configProc(element, false);
            return;
        }
    }

    AppWarning(TEXT("OBS::ConfigureScene: Could not find scene class '%s'"), lpClassName);
}

void OBS::ConfigureImageSource(XElement *element)
{
    if(!element)
    {
        AppWarning(TEXT("OBS::ConfigureImageSource: NULL element specified"));
        return;
    }

    CTSTR lpClassName = element->GetString(TEXT("class"));
    if(!lpClassName)
    {
        AppWarning(TEXT("OBS::ConfigureImageSource: No class specified for image source '%s'"), element->GetName());
        return;
    }

    for(UINT i=0; i<imageSourceClasses.Num(); i++)
    {
        if(imageSourceClasses[i].strClass.CompareI(lpClassName))
        {
            if(imageSourceClasses[i].configProc)
                imageSourceClasses[i].configProc(element, false);
            return;
        }
    }

    AppWarning(TEXT("OBS::ConfigureImageSource: Could not find scene class '%s'"), lpClassName);
}
bool OBS::SetScene(CTSTR lpScene)
{
    XElement *scenes = scenesConfig.GetElement(TEXT("scenes"));
    XElement *newSceneElement = scenes->GetElement(lpScene);
    if(!newSceneElement)
        return false;

    if(sceneElement == newSceneElement)
        return true;

    sceneElement = newSceneElement;
    
    
    return true;
}

class OBSAPIInterface : public APIInterface
{
    friend class OBS;
public:
    OBSAPIInterface(OBS *obs):APIInterface()
    {
      mObs=obs;
    }
    virtual void EnterSceneMutex() {mObs->EnterSceneMutex();}
    virtual void LeaveSceneMutex() {mObs->LeaveSceneMutex();}

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        mObs->RegisterSceneClass(lpClassName, lpDisplayName, createProc, configProc, false);
    }

    virtual void RegisterImageSourceClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)
    {
        mObs->RegisterImageSourceClass(lpClassName, lpDisplayName, createProc, configProc, false);
    }

    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)
    {
        return mObs->CreateImageSource(lpClassName, data);
    }
    virtual XElement* GetSceneElement()         {return mObs->GetSceneElement();}
    virtual Vect2 GetBaseSize() const               {return Vect2(float(mObs->baseCX), float(mObs->baseCY));}
    virtual Vect2 GetRenderFrameSize() const        {return Vect2(float(mObs->renderFrameWidth), float(mObs->renderFrameHeight));}
    virtual Vect2 GetRenderFrameOffset() const      {return Vect2(float(mObs->renderFrameX), float(mObs->renderFrameY));}
    virtual Vect2 GetRenderFrameControlSize() const {return Vect2(float(mObs->renderFrameCtrlWidth), float(mObs->renderFrameCtrlHeight));}
    virtual Vect2 GetOutputSize() const             {return Vect2(float(mObs->outputCX), float(mObs->outputCY));}

    virtual void GetBaseSize(UINT &width, UINT &height) const               {mObs->GetBaseSize(width, height);}
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const        {mObs->GetRenderFrameSize(width, height);}
    virtual void GetRenderFrameOffset(UINT &x, UINT &y) const               {mObs->GetRenderFrameOffset(x, y);}
    virtual void GetRenderFrameControlSize(UINT &width, UINT &height) const {mObs->GetRenderFrameControlSize(width, height);}
    virtual void GetOutputSize(UINT &width, UINT &height) const             {mObs->GetOutputSize(width, height);}

    virtual Vect2 MapWindowToFramePos(Vect2 mousePos) const     {return mObs->MapWindowToFramePos(mousePos);}
    virtual Vect2 MapFrameToWindowPos(Vect2 framePos) const     {return mObs->MapFrameToWindowPos(framePos);}
    virtual Vect2 MapWindowToFrameSize(Vect2 windowSize) const  {return mObs->MapWindowToFrameSize(windowSize);}
    virtual Vect2 MapFrameToWindowSize(Vect2 frameSize) const   {return mObs->MapFrameToWindowSize(frameSize);}
    virtual Vect2 GetWindowToFrameScale() const                 {return mObs->GetWindowToFrameScale();}
    virtual Vect2 GetFrameToWindowScale() const                 {return mObs->GetFrameToWindowScale();}

    virtual UINT GetMaxFPS() const                  {return mObs->bRunning ? mObs->fps : AppConfig->GetInt(TEXT("Video"), TEXT("FPS"), 30);}

    virtual CTSTR GetLanguage() const           {return mObs->strLanguage;}

    virtual CTSTR GetAppDataPath() const        {return lpAppDataPath;}
    virtual String GetPluginDataPath() const    {return String() << lpAppDataPath << TEXT("\\pluginData");}

    virtual bool UseMultithreadedOptimizations() const {return mObs->bUseMultithreadedOptimizations;}

    virtual CTSTR GetAppPath() const            {return lpAppPath;}

    virtual DWORD GetOBSVersion() const {return OBS_VERSION;}   


virtual void GetThreadHandles (HANDLE *videoThread)
{
   mObs->GetThreadHandles(videoThread);
}

virtual const MonitorInfo& GetMonitor(UINT id)
{
   return mObs->GetMonitor(id);
}

virtual UINT GetMonitorsNum()
{
   return mObs->monitors.Num();
}

virtual UINT GetFrameTime()
{
   return mObs->frameTime;
}

virtual ImageSource* GetGlobalSource(CTSTR lpName)
{
   return mObs->GetGlobalSource(lpName);
}
virtual List<GlobalSourceInfo>& GetGlobalSourceList()
{
   return mObs->globalSources;
}

virtual XConfig * GetScenesConfig()
{
   return &mObs->scenesConfig;
}

virtual ClassInfo *GetImageSourceClass(CTSTR lpClass)
{
   return mObs->GetImageSourceClass(lpClass);
}

virtual bool GetIsBRunning()
{
   return mObs->bRunning;
}
virtual void UIWarning(wchar_t *d){
   mObs->UIWarning(d);
}

   
private:
   OBS *mObs;
};
void CreateOBSApiInterface(OBS *obs,const wchar_t* logPath)
{
   APIInterface *API = new OBSAPIInterface(obs);
   OBSAPI_InterfaceRegister(API, logPath);
}
void DestoryOBSApiInterface()
{
   OBSAPI_InterfaceUnRegister();
   OBSGraphics_Unload();
}
