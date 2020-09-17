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
#include "VH_ConstDeff.h"

//-------------------------------------------------------------------

class BASE_EXPORT ImageSource {
public:
   
   virtual ~ImageSource() {interactiveResize=true;}
   virtual void Hide(bool bHide) {m_bHide = bHide;}
   virtual int HasRendData(){return 1;};
   virtual void SetDeviceExist(bool){};
   virtual void SetDesktopImageEnhance(bool open){};
   virtual void SetPreviewState(bool is_preivew){};
   virtual bool Preprocess() {return true;}
   virtual void Tick(float fSeconds) { fSeconds; }
   virtual void Render(const Vect2 &pos, const Vect2 &size,SceneRenderType type) = 0;
   virtual void SetInteractiveResize(bool ok){interactiveResize=ok;}
   virtual bool GetInteractiveResize(){return interactiveResize;}
   virtual Vect2 GetSize() const = 0;
   virtual void Reload(){}
   virtual void UpdateSettings() {}
   virtual void SetBeautyPreviewHandle(HWND m_PriviewRenderHwnd,int hide_preview){}

   virtual void BeginScene() {}
   virtual void EndScene() {}

   virtual void SetBeautyLevel(int level){}

   virtual void GlobalSourceLeaveScene() {}
   virtual void GlobalSourceEnterScene() {}

   virtual void SetFloat(CTSTR lpName, float fValue) { lpName; fValue; }
   virtual void SetInt(CTSTR lpName, int iValue) { lpName; iValue; }
   virtual void SetString(CTSTR lpName, CTSTR lpVal) { lpName; lpVal; }
   virtual void SetVector(CTSTR lpName, const Vect &value) { lpName; value; }
   virtual void SetVector2(CTSTR lpName, const Vect2 &value) { lpName; value; }
   virtual void SetVector4(CTSTR lpName, const Vect4 &value) { lpName; value; }
   virtual void SetMatrix(CTSTR lpName, const Matrix &mat) { lpName; mat; }

   inline  void SetColor(CTSTR lpName, const Color4 &value) { SetVector4(lpName, value); }
   inline  void SetColor(CTSTR lpName, float fR, float fB, float fG, float fA = 1.0f) { SetVector4(lpName, Color4(fR, fB, fG, fA)); }
   inline  void SetColor(CTSTR lpName, DWORD color) { SetVector4(lpName, RGBA_to_Vect4(color)); }

   inline  void SetColor3(CTSTR lpName, const Color3 &value) { SetVector(lpName, value); }
   inline  void SetColor3(CTSTR lpName, float fR, float fB, float fG) { SetVector(lpName, Color3(fR, fB, fG)); }
   inline  void SetColor3(CTSTR lpName, DWORD color) { SetVector(lpName, RGB_to_Vect(color)); }
   inline  void SetVector4(CTSTR lpName, float fX, float fY, float fZ, float fW) { SetVector4(lpName, Vect4(fX, fY, fZ, fW)); }
   inline  void SetVector(CTSTR lpName, float fX, float fY, float fZ) { SetVector(lpName, Vect(fX, fY, fZ)); }

   //-------------------------------------------------------------

   virtual bool GetFloat(CTSTR lpName, float &fValue)   const { lpName; fValue; return false; }
   virtual bool GetInt(CTSTR lpName, int &iValue)       const { lpName; iValue; return false; }
   virtual bool GetString(CTSTR lpName, String &strVal) const { lpName; strVal; return false; }
   virtual bool GetVector(CTSTR lpName, Vect &value)    const { lpName; value; return false; }
   virtual bool GetVector2(CTSTR lpName, Vect2 &value)  const { lpName; value; return false; }
   virtual bool GetVector4(CTSTR lpName, Vect4 &value)  const { lpName; value; return false; }
   virtual bool GetMatrix(CTSTR lpName, Matrix &mat)    const { lpName; mat; return false; }
   virtual bool GetIsHide() {return m_bHide;}

   virtual void SetRendFrameSize(const Vect2& vect){ renderFrameSize = vect;}
   virtual void SetRenderFrameOffset(const Vect2& vect) { renderFrameOffset = vect; }

public:
   Vect2 renderFrameSize;
   Vect2 renderFrameOffset;
private:
   bool interactiveResize;
   bool m_bHide = false;


   
};


//====================================================================================

class BASE_EXPORT SceneItem {
   friend class Scene;
   friend class OBS;
   friend class CGraphics;

   Scene *parent = nullptr;

   ImageSource *source = nullptr;
   XElement *mpElement = nullptr;

   Vect2 startPos, startSize;
   Vect2 pos, size;
   Vect4 crop;

   bool bSelected = false;
   bool bRender = false;
   int mSourceType;

public:
   inline SceneItem() :mSourceType(-1) {}
   ~SceneItem();
   inline int GetSourceType() ;
   inline void SetSourceType(int sourceType) ;
   inline CTSTR GetName() const;
   inline ImageSource* GetSource() const ;
   inline XElement* GetElement() const;

   inline Vect2 GetPos() const ;
   inline Vect2 GetSize() const ;
   inline Vect2 GetScale() const ;

   inline bool IsCropped() const;

   inline void SetPos(const Vect2 &newPos) ;
   inline void SetSize(const Vect2 &newSize);

   inline void Select(bool bSelect) ;
   inline bool IsSelected() const ;

   inline UINT GetID();

   void SetName(CTSTR lpNewName);
   void SetRender(bool render);
   Vect4 GetCrop();
   Vect2 GetCropTL();
   Vect2 GetCropTR();
   Vect2 GetCropBR();
   Vect2 GetCropBL();

   void Update();

   void MoveUp();
   void MoveDown();
   void MoveToTop();
   void MoveToBottom();
};

//====================================================================================

class BASE_EXPORT Scene {
   friend class SceneItem;
   friend class OBS;

   bool bSceneStarted = false;
   bool bMissingSources = false;
   bool bIsSetSourceVisible = false;
   List<SceneItem*> sceneItems;
   DWORD bkColor;

   UINT hotkeyID = 0;
   bool mIsRestart = false;


public:
   Scene();
   virtual ~Scene();
   void SetIsRestart(bool isRestart);

   virtual SceneItem* InsertImageSource(UINT pos, XElement *sourceElement, SOURCE_TYPE sourceType);
   virtual SceneItem* AddImageSource(XElement *sourceElement);

   virtual void RemoveImageSource(SceneItem *item);
   void RemoveImageSourceSaveElement(SceneItem *item);
   void RemoveImageSource(CTSTR lpName);

   virtual void Tick(float fSeconds);
   virtual void Preprocess(const Vect2 renderFrameSize,const Vect2 renderFrameOffset);
   virtual void Render(SceneRenderType);

   virtual void UpdateSettings() {}

   virtual void RenderSelections(Shader *solidPixelShader);

   virtual void BeginScene();
   virtual void EndScene();
   bool SetSourceVisible(wchar_t *sourceName,bool isVisible);
   bool FitItemsToScreen(SceneItem *item);

   //--------------------------------
   inline bool HasMissingSources() const ;
   inline void GetItemsOnPoint(const Vect2 &pos, List<SceneItem*> &items) const;
   inline void DeselectAll();

   inline UINT NumSceneItems() const ;
   inline SceneItem* GetSceneItem(UINT id) const;

   inline SceneItem* GetSceneItem(CTSTR lpName) const ;
   inline SceneItem *GetSceneItemBySourceType(int type) ;
   inline void SceneHideSource(int type,bool bHide);
   inline void GetSelectedItems(List<SceneItem*> &items) const;
   inline void GetAllItems(List<SceneItem*> &items) const;
};



