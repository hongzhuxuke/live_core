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


typedef LPVOID (STDCALL* OBSCREATEPROC)(XElement*); //data
typedef bool (STDCALL* OBSCONFIGPROC)(XElement*, bool); //element, bInitializing

//-------------------------------------------------------------------
// API interface, plugins should not ever use, use C funcs below

class APIInterface
{
    friend class OBS;
public:

    virtual ~APIInterface() {}

    virtual void EnterSceneMutex()=0;
    virtual void LeaveSceneMutex()=0;

    virtual void RegisterSceneClass(CTSTR lpClassName, CTSTR lpDisplayName, OBSCREATEPROC createProc, OBSCONFIGPROC configProc)=0;
    virtual ImageSource* CreateImageSource(CTSTR lpClassName, XElement *data)=0;
    virtual XElement* GetSceneElement()=0;

    virtual Vect2 GetBaseSize() const=0;                //get the base scene size
    virtual Vect2 GetRenderFrameSize() const=0;         //get the render frame size
    virtual Vect2 GetOutputSize() const=0;              //get the stream output size

    virtual void GetBaseSize(UINT &width, UINT &height) const=0;
    virtual void GetRenderFrameSize(UINT &width, UINT &height) const=0;
    virtual void GetOutputSize(UINT &width, UINT &height) const=0;
    virtual UINT GetMaxFPS() const=0;

    virtual CTSTR GetLanguage() const=0;

    virtual CTSTR GetAppDataPath() const=0;
    virtual String GetPluginDataPath() const=0;


    virtual bool UseMultithreadedOptimizations() const=0;

    virtual CTSTR GetAppPath() const=0;

    virtual DWORD GetOBSVersion() const=0;

    virtual Vect2 GetRenderFrameOffset() const=0;       //get the render frame offset inside the control
    virtual Vect2 GetRenderFrameControlSize() const=0;  //get the render frame control size

    virtual void GetRenderFrameOffset(UINT &x, UINT &y) const=0;
    virtual void GetRenderFrameControlSize(UINT &width, UINT &height) const=0;


    // Helpers for GetRenderFrame*() methods
    virtual Vect2 MapWindowToFramePos(Vect2 mousePos) const=0;
    virtual Vect2 MapFrameToWindowPos(Vect2 framePos) const=0;
    virtual Vect2 MapWindowToFrameSize(Vect2 windowSize) const=0;
    virtual Vect2 MapFrameToWindowSize(Vect2 frameSize) const=0;
    virtual Vect2 GetWindowToFrameScale() const=0;
    virtual Vect2 GetFrameToWindowScale() const=0;

    virtual void GetThreadHandles (HANDLE *videoThread)=0;


    virtual UINT GetMonitorsNum()=0;

    virtual UINT GetFrameTime()=0;

    virtual ImageSource* GetGlobalSource(CTSTR lpName)=0;

    virtual XConfig * GetScenesConfig()=0;


    virtual bool GetIsBRunning()=0;
    
    virtual void UIWarning(wchar_t *) = 0;
};
struct OBS_TextParam
{
   unsigned int color;
};

typedef unsigned char **(*Func_OBSAPI_CreateTextPic)(CTSTR str,OBS_TextParam *param,int &w,int &h);

BASE_EXPORT String OBSAPI_GetPluginDataPath();
BASE_EXPORT CTSTR OBSAPI_GetAppPath();
BASE_EXPORT UINT OBSAPI_GetAPIVersion();
BASE_EXPORT void OBSAPI_EnterSceneMutex();
BASE_EXPORT void OBSAPI_LeaveSceneMutex();
BASE_EXPORT XElement* OBSAPI_GetSceneElement();
BASE_EXPORT ImageSource* OBSAPI_CreateImageSource(CTSTR lpClassName, XElement *data);
BASE_EXPORT Vect2 OBSAPI_MapFrameToWindowPos(Vect2 framePos);
BASE_EXPORT Vect2 OBSAPI_GetFrameToWindowScale();
BASE_EXPORT Vect2 OBSAPI_MapFrameToWindowSize(Vect2 frameSize);
BASE_EXPORT String OBSAPI_GetPluginDataPath();
BASE_EXPORT UINT OBSAPI_GetAPIVersion();
BASE_EXPORT Vect2 OBSAPI_GetBaseSize();
BASE_EXPORT void OBSAPI_GetBaseSize(UINT&,UINT&);

BASE_EXPORT bool OBSAPI_UseMultithreadedOptimizations();
BASE_EXPORT UINT OBSAPI_GetMaxFPS();
BASE_EXPORT CTSTR OBSAPI_GetLanguage();
BASE_EXPORT void OBSAPI_InterfaceRegister(APIInterface*, const wchar_t* logPath = NULL);
BASE_EXPORT void OBSAPI_InterfaceUnRegister();

BASE_EXPORT void OBSAPI_GetThreadHandles (HANDLE *videoThread);
BASE_EXPORT UINT OBSAPI_GetMonitorsNum();
BASE_EXPORT UINT OBSAPI_GetFrameTime();
BASE_EXPORT ImageSource* OBSAPI_GetGlobalSource(CTSTR lpName);

BASE_EXPORT XConfig * OBSAPI_GetScenesConfig();
BASE_EXPORT bool OBSAPI_GetIsBRunning();
