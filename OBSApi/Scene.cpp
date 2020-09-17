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


#include "OBSApi.h"


SceneItem::~SceneItem() {
   delete source;
   source = nullptr;
}

void SceneItem::SetName(CTSTR lpNewName) {
   mpElement->SetName(lpNewName);
}

int SceneItem::GetSourceType() {
   return mSourceType;
}
void SceneItem::SetSourceType(int sourceType) {
   mSourceType = sourceType;
}

CTSTR SceneItem::GetName() const {
   return mpElement->GetName();
}
ImageSource* SceneItem::GetSource() const {
   return source;
}
XElement* SceneItem::GetElement() const {
   return mpElement;
}

Vect2 SceneItem::GetPos() const {
   return pos;
}
Vect2 SceneItem::GetSize() const {
   return size;
}
Vect2 SceneItem::GetScale() const {
   return source ? (source->GetSize() / size) : Vect2(1.0f, 1.0f);
}

bool SceneItem::IsCropped() const {
   return !(CloseFloat(crop.w, 0.0f) && CloseFloat(crop.x, 0.0f) && CloseFloat(crop.y, 0.0f) && CloseFloat(crop.z, 0.0f));
}

void SceneItem::SetPos(const Vect2 &newPos) {
   pos = newPos;
}

void SceneItem::SetSize(const Vect2 &newSize) {
   size = newSize;
}

void SceneItem::Select(bool bSelect) {
   bSelected = bSelect;
}
bool SceneItem::IsSelected() const {
   return bSelected;
}

UINT SceneItem::GetID() {
   SceneItem *thisItem = this;
   return parent->sceneItems.FindValueIndex(thisItem);
}

void SceneItem::SetRender(bool render) {
   if (mpElement) {
      mpElement->SetInt(TEXT("render"), (int)((render) ? 1 : 0));
      bRender = render;
      CTSTR lpClass = mpElement->GetString(TEXT("class"));

      if (bRender) {
         if (!lpClass) {
            AppWarning(TEXT("No class for source '%s' in scene '%s'"), mpElement->GetName(), OBSAPI_GetSceneElement()->GetName());
         }
         else {
            XElement *data = mpElement->GetElement(TEXT("data"));
            source = OBSAPI_CreateImageSource(lpClass, data);
            if (!source) {
               AppWarning(TEXT("Could not create image source '%s' in scene '%s'"), mpElement->GetName(), OBSAPI_GetSceneElement()->GetName());
            }
            else {
               OBSAPI_EnterSceneMutex();
               if (parent && parent->bSceneStarted) {
                  source->BeginScene();

                  if (scmp(lpClass, L"GlobalSource") == 0)
                     source->GlobalSourceEnterScene();
               }
               OBSAPI_LeaveSceneMutex();
            }
         }
      }
      else {
         if (source) {
            OBSAPI_EnterSceneMutex();

            ImageSource *src = source;
            source = NULL;

            if (scmp(lpClass, L"GlobalSource") == 0)
               src->GlobalSourceLeaveScene();

            if (parent && parent->bSceneStarted)
               src->EndScene();
            delete src;

            OBSAPI_LeaveSceneMutex();
         }
      }
   }

}

Vect4 SceneItem::GetCrop() {
   Vect4 scaledCrop = crop;
   Vect2 scale = GetScale();
   scaledCrop.x /= scale.x; scaledCrop.y /= scale.y;
   scaledCrop.z /= scale.y; scaledCrop.w /= scale.x;
   return scaledCrop;
}

// The following functions return the difference in x/y coordinates, not the absolute distances.
Vect2 SceneItem::GetCropTL() {
   return Vect2(GetCrop().x, GetCrop().y);
}

Vect2 SceneItem::GetCropTR() {
   return Vect2(-GetCrop().w, GetCrop().y);
}

Vect2 SceneItem::GetCropBR() {
   return Vect2(-GetCrop().w, -GetCrop().z);
}

Vect2 SceneItem::GetCropBL() {
   return Vect2(GetCrop().x, -GetCrop().z);
}


void SceneItem::Update() {
   pos = Vect2(mpElement->GetFloat(TEXT("x")), mpElement->GetFloat(TEXT("y")));
   size = Vect2(mpElement->GetFloat(TEXT("cx"), 100.0f), mpElement->GetFloat(TEXT("cy"), 100.0f));

}

void SceneItem::MoveUp() {
   SceneItem *thisItem = this;
   UINT id = parent->sceneItems.FindValueIndex(thisItem);
   assert(id != INVALID);

   if (id > 0) {
      OBSAPI_EnterSceneMutex();

      parent->sceneItems.SwapValues(id, id - 1);
      GetElement()->MoveUp();

      OBSAPI_LeaveSceneMutex();
   }
}

void SceneItem::MoveDown() {
   SceneItem *thisItem = this;
   UINT id = parent->sceneItems.FindValueIndex(thisItem);
   assert(id != INVALID);

   if (id < (parent->sceneItems.Num() - 1)) {
      OBSAPI_EnterSceneMutex();

      parent->sceneItems.SwapValues(id, id + 1);
      GetElement()->MoveDown();

      OBSAPI_LeaveSceneMutex();
   }
}

void SceneItem::MoveToTop() {
   SceneItem *thisItem = this;
   UINT id = parent->sceneItems.FindValueIndex(thisItem);
   assert(id != INVALID);

   if (id > 0) {
      OBSAPI_EnterSceneMutex();

      parent->sceneItems.Remove(id);
      parent->sceneItems.Insert(0, this);

      GetElement()->MoveToTop();

      OBSAPI_LeaveSceneMutex();
   }
}

void SceneItem::MoveToBottom() {
   SceneItem *thisItem = this;
   UINT id = parent->sceneItems.FindValueIndex(thisItem);
   assert(id != INVALID);

   if (id < (parent->sceneItems.Num() - 1)) {
      OBSAPI_EnterSceneMutex();

      parent->sceneItems.Remove(id);
      parent->sceneItems << this;

      GetElement()->MoveToBottom();

      OBSAPI_LeaveSceneMutex();
   }
}

//====================================================================================

Scene::~Scene() {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem*item = sceneItems[i];
      if (!mIsRestart) {
         XElement *data = item->GetElement()->GetElement(TEXT("data"));
         unsigned char *baseData = (unsigned char *)data->GetHex(TEXT("baseData"), 0);
         if (baseData) {
            delete []baseData;
            baseData = NULL;
            data->SetHex(TEXT("baseData"), 0);
         }
      }
      delete sceneItems[i];
   }
}
Scene::Scene() {
   mIsRestart = false;
}
void Scene::SetIsRestart(bool isRestart) {
   mIsRestart = isRestart;
}

SceneItem* Scene::AddImageSource(XElement *sourceElement) {
   SOURCE_TYPE sourceType = SRC_SOURCE_UNKNOW;
   if (sourceElement)
   {
      String classType = sourceElement->GetString(L"class", L"");
      if (classType == CLASS_NAME_DEVICE)
      {
         sourceType = SRC_DSHOW_DEVICE;
      }
      else if (classType == CLASS_NAME_DECKLINK)
      {
         sourceType = SRC_DSHOW_DEVICE;
      }
      else if (classType == CLASS_NAME_TEXT)
      {
         sourceType = SRC_TEXT;
      }
      else if (classType == CLASS_NAME_BITMAP)
      {
         String name = sourceElement->GetName();
         String persistentSource(L"Source_PersistentSource");
         if (name.Length() > persistentSource.Length()) {
            name = name.Left(persistentSource.Length());
         }
         if (name.Compare(persistentSource)) {
            sourceType = SRC_SOURCE_PERSISTENT;
         }
         else {
            sourceType = SRC_PIC;
         }
      }
      else if (classType == CLASS_NAME_MONITOR)
      {
         sourceType = SRC_MONITOR;
      }
      else if (classType == CLASS_NAME_WINDOW)
      {
         sourceType = SRC_WINDOWS;
      }
      else if (classType == CLASS_NAME_MEDIAOUTPUT)
      {
         sourceType = SRC_MEDIA_OUT;
      }
   }

   return InsertImageSource(sceneItems.Num(), sourceElement, sourceType);
}

SceneItem* Scene::InsertImageSource(UINT pos, XElement *sourceElement, SOURCE_TYPE sourceType) {
   if (GetSceneItem(sourceElement->GetName()) != NULL) {
      //AppWarning(TEXT("Scene source '%s' already in scene.  actually, no one should get this error.  if you do send it to jim immidiately."), sourceElement->GetName());
      return NULL;
   }

   if (pos > sceneItems.Num()) {
      AppWarning(TEXT("Scene::InsertImageSource: pos >= sceneItems.Num()"));
      pos = sceneItems.Num();
   }

   SceneItem *item = NULL;
   Vect2 baseSize = OBSAPI_GetBaseSize();
   bool render = sourceElement->GetInt(TEXT("render"), 1) > 0;

   XElement *data = sourceElement->GetElement(TEXT("data"));
   if (data) {
      float x = data->GetFloat(TEXT("x"), 0.0f);
      float y = data->GetFloat(TEXT("y"), 0.0f);
      float cx = data->GetFloat(TEXT("cx"), 1.0f);
      float cy = data->GetFloat(TEXT("cy"), 1.0f);
      x *= baseSize.x;
      y *= baseSize.y;
      cx *= baseSize.x;
      cy *= baseSize.y;

      item = new SceneItem();
      item->mpElement = sourceElement;
      item->parent = this;
      item->pos = Vect2(x, y);
      item->size = Vect2(cx, cy);

      item->crop.w = sourceElement->GetFloat(TEXT("crop.right"));
      item->crop.x = sourceElement->GetFloat(TEXT("crop.left"));
      item->crop.y = sourceElement->GetFloat(TEXT("crop.top"));
      item->crop.z = sourceElement->GetFloat(TEXT("crop.bottom"));

      item->SetRender(render);

      item->SetSourceType(sourceType);

      bool bHide = (sourceElement->GetInt(TEXT("sourceHide"), 0) == 1);

      OBSAPI_EnterSceneMutex();
      sceneItems.Insert(pos, item);
      if (item->source) {
         item->source->Hide(bHide);
      }
      OBSAPI_LeaveSceneMutex();
      OBSApiLog("Scene::InsertImageSource sourceType:%d", sourceType);

   }

   return item;
}

void Scene::RemoveImageSource(SceneItem *item) {
   if (item && item->source && item->GetElement() && item->GetElement()->GetParent()) {
      if (bSceneStarted) {
         item->source->EndScene();
      }
      item->GetElement()->GetParent()->RemoveElement(item->GetElement());
      sceneItems.RemoveItem(item);
      delete item;
      item = nullptr;
   }
   return;
}

void Scene::RemoveImageSourceSaveElement(SceneItem *item) {
   if (item && item->source) {
      if (bSceneStarted) {
         item->source->EndScene();
      }
      sceneItems.RemoveItem(item);
      delete item;
      item = nullptr;
   }
}

void Scene::RemoveImageSource(CTSTR lpName) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      if (scmpi(sceneItems[i]->GetName(), lpName) == 0) {
         int type = sceneItems[i]->GetSourceType();
         OBSApiLog("Scene::RemoveImageSource type:%d", type);
         RemoveImageSource(sceneItems[i]);
         return;
      }
   }
}

void Scene::Preprocess(const Vect2 renderFrameSize, const Vect2 renderFrameOffset) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (!item) {
         _ASSERT(FALSE);
         continue;
      }

      if (item->source && item->bRender) {
         item->source->SetRendFrameSize(renderFrameSize);
         item->source->SetRenderFrameOffset(renderFrameOffset);
         bool ret = item->source->Preprocess();
         if (!ret) {
            item->GetElement()->GetParent()->RemoveElement(item->GetElement());
            sceneItems.RemoveItem(item);
            delete item;
            continue;
         }
      }
   }
}

void Scene::Tick(float fSeconds) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (item->source)
         item->source->Tick(fSeconds);
   }
}
bool Scene::SetSourceVisible(wchar_t *sourceName, bool isVisible) {
   for (int i = sceneItems.Num() - 1; i >= 0; i--) {
      SceneItem *item = sceneItems[i];
      CTSTR itemName = item->GetName();
      if (wcscmp(itemName, sourceName) == 0) {

         Log(TEXT("Scene::SetSourceVisible"));
         item->SetRender(isVisible);
         bIsSetSourceVisible = true;
         return true;
      }
   }
   return false;
}
bool Scene::FitItemsToScreen(SceneItem *item)
{
   if (item) {
      XElement *itemElement = item->GetElement();
      if (!itemElement) {
         return false;
      }

      XElement *data = itemElement->GetElement(L"data");
      if (!data) {
         return false;
      }

      bool isFullscreen = data->GetInt(L"isFullscreen", 0) != 0;
      if (!isFullscreen) {
         return false;
      }

      Vect2 baseSize = OBSAPI_GetBaseSize();
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
      }
   }

   return true;
}

void Scene::Render(SceneRenderType renderType) {
   ClearColorBuffer();
   Vect2 baseSize = OBSAPI_GetBaseSize();

   for (int i = sceneItems.Num() - 1; i >= 0; i--) {
      SceneItem *item = sceneItems[i];
      if (!item) {
         _ASSERT(FALSE);
         continue;
      }

      if (item->source && item->bRender) {
         SetCropping(item->GetCrop().x, item->GetCrop().y, item->GetCrop().w, item->GetCrop().z);
         item->source->Render(item->pos, item->size, renderType);
         Vect2 sourceSize = item->source->GetSize();
         Vect2 itemSize = item->GetSize();


         //float diff = sourceSize.x*itemSize.y - sourceSize.y*itemSize.x;
         if (sourceSize.y > 0 && itemSize.y > 0) {
            int diff = abs(sourceSize.x * 100 / sourceSize.y - itemSize.x * 100 / itemSize.y);

            if (diff > 1) {
               if (item->GetSourceType() == SRC_WINDOWS || item->GetSourceType() == SRC_DSHOW_DEVICE && sourceSize.x != 0 || item->GetSourceType() == SRC_MONITOR_AREA) {
                  if (!FitItemsToScreen(item)) {
                     itemSize.y = itemSize.x / sourceSize.x*sourceSize.y;
                     item->SetSize(itemSize);
                  }
               }
            }
         }

         SetCropping(0.0f, 0.0f, 0.0f, 0.0f);
      }
   }
   if (bIsSetSourceVisible) {
      bIsSetSourceVisible = false;
      Log(TEXT("Scene Render"));
   }
}

void Scene::RenderSelections(Shader *solidPixelShader) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];

      if (item->bSelected) {
         Vect2 baseScale = item->GetSource() ? item->GetSource()->GetSize() : item->GetSize();
         Vect2 cropFactor = baseScale / item->GetSize();
         Vect4 crop = item->GetCrop();
         Vect2 pos = OBSAPI_MapFrameToWindowPos(item->GetPos());
         Vect2 scale = OBSAPI_GetFrameToWindowScale();
         crop.x *= scale.x; crop.y *= scale.y;
         crop.z *= scale.y; crop.w *= scale.x;
         pos.x += crop.x;
         pos.y += crop.y;
         Vect2 size = OBSAPI_MapFrameToWindowSize(item->GetSize());
         size.x -= (crop.x + crop.w);
         size.y -= (crop.y + crop.z);
         Vect2 selectBoxSize = Vect2(10.0f, 10.0f);

         DrawBox(pos, selectBoxSize);
         DrawBox((pos + size) - selectBoxSize, selectBoxSize);
         DrawBox(pos + Vect2(size.x - selectBoxSize.x, 0.0f), selectBoxSize);
         DrawBox(pos + Vect2(0.0f, size.y - selectBoxSize.y), selectBoxSize);

         // Top
         if (CloseFloat(crop.y, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
         else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
         DrawBox(pos, Vect2(size.x, 0.0f));

         // Left
         if (CloseFloat(crop.x, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
         else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
         DrawBox(pos, Vect2(0.0f, size.y));

         // Right
         if (CloseFloat(crop.w, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
         else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
         DrawBox(pos + Vect2(size.x, 0.0f), Vect2(0.0f, size.y));

         // Bottom
         if (CloseFloat(crop.z, 0.0f)) solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFF0000);
         else solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0x00FF00);
         DrawBox(pos + Vect2(0.0f, size.y), Vect2(size.x, 0.0f));

         //#define DRAW_UNCROPPED_SELECTION_BOX
#ifdef DRAW_UNCROPPED_SELECTION_BOX
         Vect2 realPos = OBSMapFrameToWindowPos(item->GetPos());
         Vect2 realSize = item->GetSize() * scale;
         solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFFFF);
         solidPixelShader->SetColor(solidPixelShader->GetParameter(0), 0xFFFFFF);
         DrawBox(realPos, Vect2(0.0f, realSize.y));
         DrawBox(realPos, Vect2(realSize.x, 0.0f));
         DrawBox(realPos + Vect2(realSize.x, 0.0f), Vect2(0.0f, realSize.y));
         DrawBox(realPos + Vect2(0.0f, realSize.y), Vect2(realSize.x, 0.0f));
#endif
      }
   }
}

void Scene::BeginScene() {
   if (bSceneStarted)
      return;

   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (item->source)
         item->source->BeginScene();
   }

   bSceneStarted = true;
}

void Scene::EndScene() {
   if (!bSceneStarted)
      return;

   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (item->source)
         item->source->EndScene();
   }

   bSceneStarted = false;
}


bool Scene::HasMissingSources() const {
   return bMissingSources;
}

void Scene::GetItemsOnPoint(const Vect2 &pos, List<SceneItem*> &items) const {
   items.Clear();
   for (int i = sceneItems.Num() - 1; i >= 0; i--) {
      SceneItem *item = sceneItems[i];
      int persistent = item->GetElement()->GetInt(TEXT("persistent"), 0);
      if (persistent == 1) {
         continue;
      }
      Vect2 scale = (item->GetSource() ? item->GetSource()->GetSize() : item->GetSize()) / item->GetSize();

      Vect2 upperLeft = item->GetPos();
      upperLeft += item->GetCropTL();
      Vect2 lowerRight = upperLeft + item->GetSize();
      lowerRight += item->GetCropBR();
      if (item->bRender && pos.x >= upperLeft.x && pos.y >= upperLeft.y && pos.x <= lowerRight.x && pos.y <= lowerRight.y)
         items << item;
   }
}

void Scene::DeselectAll() {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      if (sceneItems[i]) {
         sceneItems[i]->bSelected = false;
      }
   }
}
//--------------------------------

UINT Scene::NumSceneItems() const {
   return sceneItems.Num();
}

SceneItem* Scene::GetSceneItem(UINT id) const {
   return sceneItems[id];
}

SceneItem* Scene::GetSceneItem(CTSTR lpName) const {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      if (sceneItems[i] != NULL) {
         if (scmpi(sceneItems[i]->GetName(), lpName) == 0)
            return sceneItems[i];
      }
   }
   return NULL;
}

SceneItem *Scene::GetSceneItemBySourceType(int type) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (item) {
         if (item->GetSourceType() == type) {
            return item;
         }
      }
   }
   return NULL;
}
void Scene::SceneHideSource(int type, bool bHide) {
   for (UINT i = 0; i < sceneItems.Num(); i++) {
      SceneItem *item = sceneItems[i];
      if (item) {
         if (item->GetSourceType() == type) {
            item->source->Hide(bHide);
         }
         item->mpElement->AddInt(L"sourceHide", bHide ? 1 : 0);
      }
   }
}

void Scene::GetSelectedItems(List<SceneItem*> &items) const {
   items.Clear();
   if (sceneItems.Num() > 0) {
      for (UINT i = 0; i < sceneItems.Num(); i++) {
         if (sceneItems[i] != NULL && sceneItems[i]->GetElement()) {
            int persistent = sceneItems[i]->GetElement()->GetInt(TEXT("persistent"), 0);
            if (sceneItems[i]->bSelected&&persistent == 0)
               items << sceneItems[i];
         }
      }
   }
}

void Scene::GetAllItems(List<SceneItem*> &items) const {
   items.Clear();
   if (sceneItems.Num() > 0) {
      for (UINT i = 0; i < sceneItems.Num(); i++) {
         if (sceneItems[i] != NULL && sceneItems[i]->GetElement()) {
            int persistent = sceneItems[i]->GetElement()->GetInt(TEXT("persistent"), 0);
            if (persistent == 0)
               items << sceneItems[i];
         }
      }
   }
}