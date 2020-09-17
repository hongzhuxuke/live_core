#include "Main.h"
#include "Graphics.h"
#include <gdiplus.h>

#include "Msg_OBSControl.h"
#include "Msg_MainUI.h"
#include "IDShowPlugin.h"
#include "IDeckLinkDevice.h"
#include "IMediaCore.h"

#define DEFAULT_SCENE_NAME L"s01"
#define CLASS_SCENE_Scene  L"Scene"
#define SOURCE_TYPE_PERSISTENT   L"persistent"
#define SOURCE_PERSISTENT_ORDER  L"persistentorder"
#define ORDER_TOP             0
#define ORDER_BOTTON             -1

#define OP_TYPE_DIALOG_ADD        1
#define OP_TYPE_DIALOG_MODIFY     2

#define SOURCE_NAME_DEFAULT L"SOURCE_DEFAULT"
#define SOURCEMARGIN 30
void destory();
int init();

int CGraphics::mSourceIndex = 0;
#define DEFAULT_SOURCE_NAME L"Source"

#define NEW_DEVICES_SOURCE  L"NewDeviceSource"


char* CGraphics::WStr2CStr(const wchar_t* WStr)
{
  /* 长度设置 */
	size_t len = wcslen(WStr) + 1;
	size_t converted = 0;
  /* 准备转换的对象 */
	char *CStr;
	CStr = (char*)malloc(len * sizeof(char));
  /* 转换 */
	wcstombs_s(&converted, CStr, len, WStr, _TRUNCATE);
  /* 返回 */
	return CStr;
}

CGraphics::CGraphics(HWND msgWnd, HWND renderFrame, SIZE baseSize, int scaleType, const wchar_t *logPath) {

   mRenderWnd = renderFrame;
   mMsgWnd = msgWnd;
   mBaseSize = baseSize;
   mObs = new OBS(msgWnd, renderFrame, scaleType, logPath);

   ReloadSceneConfig();
   mMediaSourceIndex = 0;

   //load default setting
   LoadDefaultSetting();
   AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), baseSize.cx);
   AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), baseSize.cy);

   this->funcDeleteSourceHook = NULL;
   this->mDeleteSourceHookParam = NULL;
   mMediaSourceItem = NULL;
   mMediaSourcePos.x = 0;
   mMediaSourcePos.y = 0;

   mMediaSourceSize.x = 0;
   mMediaSourceSize.y = 0;
   mMediaSourcePosSizeInit = false;
   mFps = 25;
   mIsAutoMode = false;
   mLastSize = Vect2(0, 0);
}
CGraphics::~CGraphics() {
   if (mObs != NULL) {
      delete mObs;
      mObs = NULL;
   }
}
void CGraphics::SetDataReceiver(IDataReceiver *r){
   if(mObs) {
      mObs->SetDataReceiver(r);
   }
}

bool CGraphics::AddPersistentSource(const wchar_t* path, const RECT& rect, const int & index) {
   bool ret = false;
   RECT renderRect = { 0, 0, 0, 0 };
   XElement*     sources = GetSourcesElement();
   String strName;
   SceneItem *item = NULL;
   strName << DEFAULT_SOURCE_NAME << L"_PersistentSource_" << mSourceIndex++;
   //get new source name 
   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), L"BitmapImageSource");

   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data)
      data = newSourceElement->CreateElement(TEXT("data"));

   float x = rect.left;
   float y = rect.top;
   float w = rect.right - rect.left;
   float h = rect.bottom - rect.top;

   x /= mBaseSize.cx;

   y /= mBaseSize.cy;

   w /= mBaseSize.cx;

   h /= mBaseSize.cy;


   String sPath = path;
   data->SetString(L"path", sPath);
   data->SetFloat(L"x", x);
   data->SetFloat(L"y", y);
   data->SetFloat(L"cx", w);
   data->SetFloat(L"cy", h);

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      OBSApiLog("CGraphics::AddPersistentSource SRC_SOURCE_PERSISTENT");
      if (mObs->scene) {
         item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_SOURCE_PERSISTENT);
         float posX = float(rect.left);
         float posY = float(rect.top);
         item->pos.x = posX;
         item->pos.y = posY;
         newSourceElement->SetInt(SOURCE_TYPE_PERSISTENT, 1);
         if (ORDER_TOP == index)
            item->MoveToTop();
         else if (ORDER_BOTTON == index) {
            item->MoveToBottom();
         }
         newSourceElement->SetInt(SOURCE_PERSISTENT_ORDER, index);
      }
      mObs->LeaveSceneMutex();
   }
   return true;
}

int CGraphics::GetRenderState(DeviceInfo devinfo) {
   int ret = 0;
   if (mObs == nullptr) {
      return false;
   }
   mObs->EnterSceneMutex();
   SceneItem *item = GetDeviceItem(devinfo);
   if (item) {
      ret = item->source->HasRendData();
   }
   mObs->LeaveSceneMutex();
   return ret;
}

bool CGraphics::AddNewSource(SOURCE_TYPE captureType, void* sourceCtx, void **pSourceItem) {
   bool ret = false;
   if (mObs == nullptr) {
      return false;
   }
   mObs->EnterSceneMutex();
   switch (captureType) {
   case  SRC_DSHOW_DEVICE:{
      STRU_OBSCONTROL_ADDCAMERA* pAddCamero = (STRU_OBSCONTROL_ADDCAMERA*)sourceCtx;
      /* 添加 */
      if (device_operator_add == pAddCamero->m_dwType) {
         OBSApiLog("CGraphics::AddNewSource SRC_DSHOW_DEVICE device_operator_add");
         ret = processDevice(pAddCamero->m_deviceInfo, pAddCamero->m_PosType, 0,pAddCamero->m_renderHwnd, pAddCamero->m_PriviewRenderHwnd, pAddCamero->m_HideSourceItem);
      }
      /* 删除 */
      else if (device_operator_del == pAddCamero->m_dwType) {
         OBSApiLog("CGraphics::AddNewSource SRC_DSHOW_DEVICE device_operator_del");
         ret = deleteDevice(sourceCtx);
      } 
      else if (device_operator_modify == pAddCamero->m_dwType) {
         OBSApiLog("CGraphics::AddNewSource SRC_DSHOW_DEVICE device_operator_modify");
         ret = ModifyDeviceSource(pAddCamero->m_deviceInfo, pAddCamero->m_deviceInfo, pAddCamero->m_PosType);
      }
      else if (device_operator_beauty == pAddCamero->m_dwType) {
         ret = ModifyBeautyDeviceSource(pAddCamero->m_deviceInfo, pAddCamero->m_PriviewRenderHwnd, pAddCamero->m_HideSourceItem, pAddCamero->m_beautyLevel);
      }
      break;
   }
   case SRC_MONITOR_AREA:{
      OBSApiLog("CGraphics::AddNewSource insert SRC_MONITOR_AREA");
      SceneItem * item = NULL;
      while (true) {
         item = GetSceneItem(SRC_MONITOR_AREA);
         if (!item) {
            break;
         }
         if (mObs->scene) {
            mObs->scene->RemoveImageSource(item);
            mObs->scene->DeselectAll();
         }
         item = NULL;
      }

      if (!item) {
         ret = processScreenArea(sourceCtx);
      } else {
         ret = false;
      }
      OBSApiLog("CGraphics::AddNewSource insert SRC_MONITOR_AREA ok");
   }
   break;
   case SRC_MONITOR:{
      OBSApiLog("CGraphics::AddNewSource SRC_MONITOR");
      SceneItem * item = GetSceneItem(SRC_MONITOR);
      if (!item) {
            ret = processMonitor(sourceCtx);
      } else {
         /*if (this->funcDeleteSourceHook) {
            if (this->funcDeleteSourceHook(item, this->mDeleteSourceHookParam, SRC_MONITOR)) {
               mObs->scene->RemoveImageSource(item);
               mObs->scene->DeselectAll();
               ret = true;
            }
         }*/
         //ret = processMonitor(sourceCtx);
         XElement *sourceElement = item->GetElement();
         if (sourceElement) {
            XElement *data = sourceElement->GetElement(L"data");
            if (data) {
               VHD_WindowInfo* desktop_Info = (VHD_WindowInfo*)sourceCtx;
               data->SetHexListPtr(L"winInfo", (unsigned char *)sourceCtx, sizeof(VHD_WindowInfo));
               if (item->source) {
                  item->source->UpdateSettings();
               }
            }
         }
         ret = false;
      }
      break;
   }
   case SRC_WINDOWS:{
      OBSApiLog("CGraphics::AddNewSource SRC_WINDOWS");
      SceneItem * item = GetSceneItem(SRC_WINDOWS);
      if (!item) {
         ret = processWindow(sourceCtx);
      } else {
         ret = modifyWindow(item->GetElement(), item, sourceCtx);
      }
   }
   break;
   case SRC_PIC:{
      OBSApiLog("CGraphics::AddNewSource SRC_PIC");
      STRU_OBSCONTROL_IMAGE* pImageControl = (STRU_OBSCONTROL_IMAGE*)sourceCtx;
      if (pImageControl->m_dwType == 1) {
         ret = processPicture(sourceCtx);
      }
      else {
         List<SceneItem*> items;
         if (mObs->scene) {
            mObs->scene->GetSelectedItems(items);
            if (items.Num() > 0) {
               XElement* source = items[0]->GetElement();
               ret = modifyPicture(source, sourceCtx);
            }
         }
      }
   }
   break;
   case SRC_TEXT:{
      STRU_OBSCONTROL_TEXT* pTextControl = (STRU_OBSCONTROL_TEXT*)sourceCtx;
      /* 添加 */
      if (pTextControl->m_iControlType == 1) {
         OBSApiLog("CGraphics::AddNewSource SRC_TEXT add");
         ret = processText(sourceCtx);
      }
      /* 修改 */
      else if (pTextControl->m_iControlType == 2) {
         List<SceneItem*> items;
         if (mObs->scene) {
            mObs->scene->GetSelectedItems(items);
            XElement* source = items[0]->GetElement();
            ret = modifyText(source, sourceCtx);
         }
      }
      /* 删除 */
      else {
         OBSApiLog("CGraphics::AddNewSource SRC_TEXT del");
         if (mObs) {
            if (mObs->scene) {
               List <SceneItem*> items;
               mObs->scene->GetSelectedItems(items);
               if (items.Num()) {
                  XElement* source = items[0]->GetElement();
                  XElement *data = source->GetElement(TEXT("data"));
                  if (data) {
                     unsigned char *baseData = (unsigned char *)data->GetHex(TEXT("baseData"), 0);
                     if (baseData) {
                        delete []baseData;
                        baseData = NULL;
                        data->SetHex(TEXT("baseData"), 0);
                     }
                  }
                  mObs->scene->RemoveImageSource(items[0]);
               }
            }
         }
      }
      break;
   }
   case SRC_MEDIA_OUT:
      OBSApiLog("CGraphics::AddNewSource add SRC_MEDIA_OUT");
      void *sourceItem = processMediaOut(sourceCtx);
      ret = (sourceItem != NULL);
      if (pSourceItem) {
         (*pSourceItem) = sourceItem;
      }
   }
   /* 场景化参数调整（编码底层感知） */
   AdjusteSceneParam();
   UpdatePersistentSource();
   mObs->LeaveSceneMutex();
   return ret;
}
void CGraphics::ClearSourceByType(SOURCE_TYPE sourceType) {
   if(!mObs) {
      return ;
   }
   mObs->EnterSceneMutex();
   if(!mObs->scene) {
      mObs->LeaveSceneMutex();
      return;
   }
   int index=0;
   bool bDone=false;
   while(!bDone) {
      bDone = true;
      for(int i = index;i<mObs->scene->NumSceneItems();i++) {
         SceneItem *item=mObs->scene->GetSceneItem(i);
         if(item) {
               SOURCE_TYPE type = (SOURCE_TYPE)item->GetSourceType();
               if(sourceType == type) {
                  mObs->scene->RemoveImageSource(item);
                  i = index;
                  bDone = false;                  
                  if (sourceType == SRC_MEDIA_OUT) {
                     mMediaSourceItem = nullptr;
                  }
                  break;
               }
         }
      }
   }
   mObs->LeaveSceneMutex();
   AdjusteSceneParam();
}

void CGraphics::ClearSource(void *sourceItem) {
    if (!mObs) {
        return;
    }
    mObs->EnterSceneMutex();
    if (mObs->scene) {
        if (sourceItem == NULL) {
            List<SceneItem*> items;
            mObs->scene->GetSelectedItems(items);
            while (items.Num()) {
                if (IsInPersistent(items[0]))
                    break;
                mObs->scene->RemoveImageSource(items[0]);
                items.Remove(0);
                OBSApiLog(" CGraphics::ClearSource remove item index 0");
            }
        } else {
            SceneItem *item = (SceneItem *)sourceItem;
            if (item) {
                if (mMediaSourceItem == item) {
                    mMediaSourceItem = nullptr;
                    OBSApiLog("CGraphics::ClearSource remove MediaSourceItem");
                }
                OBSApiLog("CGraphics::ClearSource RemoveImageSource");
                mObs->scene->RemoveImageSource(item);
            }
        }
        AdjusteSceneParam();
    }
    mObs->LeaveSceneMutex();
}
void CGraphics::ClearAllSource() {
    if (!mObs) {
        return;
    }
    mObs->EnterSceneMutex();
    if (mObs->scene) {
        List<SceneItem*> items;
        mObs->scene->GetAllItems(items);
        while (items.Num()) {
            if (IsInPersistent(items[0])) {
                items.Remove(0);
                continue;
            }
            SOURCE_TYPE sourceType = (SOURCE_TYPE)items[0]->GetSourceType();
            if (sourceType == SRC_TEXT || sourceType == SRC_PIC || sourceType == SRC_DSHOW_DEVICE) {
                items.Remove(0);
                continue;
            }

            if (this->funcDeleteSourceHook) {
                if (this->mMediaSourceItem == items[0]) {
                    this->mMediaSourceItem = NULL;
                }
                XElement* source = items[0]->GetElement();
                XElement *data = source->GetElement(TEXT("data"));
                if (!data) {
                    _ASSERT(FALSE);
                    mObs->LeaveSceneMutex();
                    return;
                }

                unsigned char *baseData = (unsigned char *)data->GetHex(TEXT("baseData"), 0);
                if (baseData) {
                    delete []baseData;
                    baseData = NULL;
                    data->SetHex(TEXT("baseData"), 0);
                }

                if (this->funcDeleteSourceHook(items[0], this->mDeleteSourceHookParam, sourceType)) {
                    mObs->scene->RemoveImageSource(items[0]);
                    items.Remove(0);
                    this->funcDeleteSourceHook(NULL, this->mDeleteSourceHookParam, sourceType);
                }
            }
            else {
                mObs->scene->RemoveImageSource(items[0]);
                items.Remove(0);
            }
        }
        UpdatePersistentSource();
    }
    mObs->LeaveSceneMutex();
}

int CGraphics::ProcessSource(const int & opType) {
    int nRet = 0;
    if (!mObs) {
        return nRet;
    }
    mObs->EnterSceneMutex();
    if (mObs->scene) {
        List<SceneItem*> items;
        mObs->scene->GetSelectedItems(items);
        while (items.Num()) {
            if (IsInPersistent(items[0]))
                break;
            if (OP_TYPE_FULL_SCREEN == opType) {
                FitItemsToScreen(items[0]);
                break;
            } 
            else if (OP_TYPE_MOVE_TOP == opType) {
                items[0]->MoveToTop();
                break;
            } 
            else if (OP_TYPE_MOVE_BOTTOM == opType) {
                items[0]->MoveToBottom();
                break;
            } 
            else if (OP_TYPE_MODIFY == opType) {
                XElement* source = items[0]->GetElement();
                XElement *data = source->GetElement(TEXT("data"));
                SOURCE_TYPE sourceType = (SOURCE_TYPE)items[0]->GetSourceType();
                DWORD dwType = -1;
                switch (sourceType) {
                case SRC_DSHOW_DEVICE:
                    dwType = WM_USER_MODIFYCAMERA;
                    break;
                case SRC_MONITOR_AREA:
                    dwType = WM_USER_MODIFY_REGION;
                    break;
                case SRC_MONITOR:
                    dwType = WM_USER_MODIFY_MONITOR;
                    break;
                case SRC_MEDIA_OUT:
                    dwType = WM_USER_MODIFY_MEDIA;
                    break;
                default:
                    break;
                }
                ::PostMessage(mMsgWnd, dwType, 0, NULL);
                modifySource(source, items);
                break;
            }
            else if (OP_TYPE_DELETE == opType) {
                if (this->funcDeleteSourceHook) {
                    if (this->mMediaSourceItem == items[0]) {
                        this->mMediaSourceItem = NULL;
                    }
                    SOURCE_TYPE sourceType;
                    XElement* source = items[0]->GetElement();
                    XElement *data = source->GetElement(TEXT("data"));
                    if (!data) {
                        _ASSERT(FALSE);
                        mObs->LeaveSceneMutex();
                        return nRet;
                    }
                    unsigned char *baseData = (unsigned char *)data->GetHex(TEXT("baseData"), 0);
                    if (baseData) {
                        delete []baseData;
                        baseData = NULL;
                        data->SetHex(TEXT("baseData"), 0);
                    }
                    sourceType = (SOURCE_TYPE)items[0]->GetSourceType();
                    DWORD dwType = -1;
                    switch (sourceType) {
                    case SRC_DSHOW_DEVICE:
                        OBSApiLog("CGraphics::ProcessSource delete source SRC_DSHOW_DEVICE");
                        dwType = WM_USER_DELETE_CAMERA;
                        break;
                    case SRC_MONITOR_AREA:
                        OBSApiLog("CGraphics::ProcessSource delete source WM_USER_DELETE_REGION");
                        dwType = WM_USER_DELETE_REGION;
                        nRet = 1;
                        break;
                    case SRC_MONITOR:
                        OBSApiLog("CGraphics::ProcessSource delete source WM_USER_DELETE_MONITOR");
                        dwType = WM_USER_DELETE_MONITOR;
                        break;
                    case SRC_WINDOWS:
                        OBSApiLog("CGraphics::ProcessSource delete source WM_USER_DELETE_WINDOWS");
                        dwType = WM_USER_DELETE_WINDOWS;
                        nRet = 1;
                        break;
                    case SRC_TEXT: 
                        OBSApiLog("CGraphics::ProcessSource delete source WM_USER_DELETE_TEXT");
                        dwType = WM_USER_DELETE_TEXT;
                        break;
                    case SRC_PIC:
                        OBSApiLog("CGraphics::ProcessSource delete source WM_USER_DELETE_PIC");
                        dwType = WM_USER_DELETE_PIC;
                        break;
                    case SRC_MEDIA_OUT:
                        OBSApiLog("CGraphics::ProcessSource delete source SRC_MEDIA_OUT");
                        dwType = WM_USER_DELETE_MEDIA;
                        break;
                    default:
                        break;
                    }
                    ::PostMessage(mMsgWnd, dwType, 0, NULL);
                    if (this->funcDeleteSourceHook(items[0], this->mDeleteSourceHookParam, sourceType)) {
                        OBSApiLog("CGraphics::ProcessSource RemoveImageSource sourceType:%d", sourceType);
                        mObs->scene->RemoveImageSource(items[0]);
                        items.Remove(0);
                        this->funcDeleteSourceHook(NULL, this->mDeleteSourceHookParam, sourceType);
                    }
                } 
                else {
                    OBSApiLog("CGraphics::ProcessSource RemoveImageSource funcDeleteSourceHook is nullptr");
                    mObs->scene->RemoveImageSource(items[0]);
                    items.Remove(0);
                }
            }
        }
        AdjusteSceneParam();
        UpdatePersistentSource();
    }
    mObs->LeaveSceneMutex();
    return nRet;
}
bool CGraphics::isSelectAreaSource() {
   bool ret = false;
   if (!mObs) {
       return ret;
   }
   mObs->EnterSceneMutex();
   if (mObs->scene) {
      List<SceneItem*> items;
      mObs->scene->GetSelectedItems(items);
      if (items.Num() > 0) {
         XElement* source = items[0]->GetElement();
         XElement* data = source->GetElement(TEXT("data"));
         if (scmpi(source->GetString(L"class"), CLASS_NAME_WINDOW) == 0 && data->GetInt(TEXT("regionCapture"), 1) == 1)
            ret = true;
      }
   }
   mObs->LeaveSceneMutex();
   return ret;
}
bool CGraphics::IsHasMonitorSource() {
   XElement*     sources = GetSourcesElement();
   //remove old 
   StringList sourceNameList;
   for (UINT i = 0; i < sources->NumElements(); i++) {
      XElement* source = sources->GetElementByID(i);
      if (scmpi(source->GetString(L"class"), CLASS_NAME_MONITOR) == 0) {/* 获取是否在屏幕共享 */
         sourceNameList.Add(source->GetName());
      }
   }
   if (sourceNameList.Num()>0)
      return true;
   else
      return false;
}

bool CGraphics::IsHasWindowsSource() {
   XElement*     sources = GetSourcesElement();
   //remove old 
   StringList sourceNameList;
   for (UINT i = 0; i < sources->NumElements(); i++) {
      XElement* source = sources->GetElementByID(i);
      if (scmpi(source->GetString(L"class"), CLASS_NAME_WINDOW) == 0
          || scmpi(source->GetString(L"class"), CLASS_NAME_MONITOR) == 0) {
         sourceNameList.Add(source->GetName());
      }
   }
   if (sourceNameList.Num()>0)
      return true;
   else
      return false;
}
int CGraphics::GetCurrentItemType() {
   int type = -1;
   if (mObs == nullptr) {
      return type;
   }
   mObs->EnterSceneMutex();
   if (mObs->scene != nullptr) {
      List<SceneItem*> items;
      mObs->scene->GetSelectedItems(items);
      if (items.Num() > 0) {
         type = items[0]->GetSourceType();
      }
   }
   mObs->LeaveSceneMutex();
   return type;
}

bool CGraphics::IsHasNoPersistentSource() {
   bool ret = false;
   mObs->EnterSceneMutex();
   List<SceneItem*> items;
   if (mObs->scene) {
      mObs->scene->GetSelectedItems(items);
      if (items.Num() > 0) {
         ret = true;
      }
   }
   mObs->LeaveSceneMutex();
   return ret;
}

bool CGraphics::IsModifiable() {
    mObs->EnterSceneMutex();
    if (mObs->scene) {
        List<SceneItem*> items;
        mObs->scene->GetSelectedItems(items);
        XElement* source = items[0]->GetElement();
        if (scmpi(source->GetString(L"class"), CLASS_NAME_TEXT) == 0 || scmpi(source->GetString(L"class"), CLASS_NAME_BITMAP) == 0
            || scmpi(source->GetString(L"class"), CLASS_NAME_WINDOW) == 0 || scmpi(source->GetString(L"class"), CLASS_NAME_DEVICE) == 0) {
            mObs->LeaveSceneMutex();
            return true;
        } else {
            mObs->LeaveSceneMutex();
            return false;
        }
    }
    mObs->LeaveSceneMutex();
    return false;
}

bool CGraphics::Start() {
   if (mObs) {
      mObs->ToggleCapturing();
   }
   return true;
}

void CGraphics::Shutdown() {
   if (mObs) {
      mObs->Stop(false);
   }
}

bool CGraphics::Preview(const SIZE &baseSize, const SIZE &outputSize, bool isAutoMode) {
   mObs->EnterSceneMutex();
   mIsAutoMode = isAutoMode;
   mObs->SetScene(L"s01");
   AppConfig->SetInt(TEXT("Video"), TEXT("OutputCX"), outputSize.cx);
   AppConfig->SetInt(TEXT("Video"), TEXT("OutputCY"), outputSize.cy);
   AppConfig->SetInt(TEXT("Video"), TEXT("BaseWidth"), baseSize.cx);
   AppConfig->SetInt(TEXT("Video"), TEXT("BaseHeight"), baseSize.cy);

   AppConfig->SetInt(TEXT("LastSize"), TEXT("x"), mLastSize.x);
   AppConfig->SetInt(TEXT("LastSize"), TEXT("y"), mLastSize.y);
   OBSApiLog("Preview baseSize.x %d baseSize.y %d   outputSize.x %d  outputSize.y %d", baseSize.cx, baseSize.cy, outputSize.cx, outputSize.cy);

   mBaseSize = baseSize;
   mOutputSize = outputSize;
   mObs->SetAutoMode(isAutoMode);
   mObs->ToggleCapturing();
   mObs->SetSyncWithEncoder(false);
   if (mObs->bRunning) {
      for (UINT s01ItemIndex = 0; s01ItemIndex < mObs->scene->NumSceneItems(); s01ItemIndex++) {
         SceneItem* s01Item = mObs->scene->GetSceneItem(s01ItemIndex);
         XElement *sourceElement = s01Item->GetElement();
         if (sourceElement) {

            CTSTR sourceClass = sourceElement->GetString(TEXT("class"), CLASS_NAME_UNKNOW);
            if (sourceClass) {
               if (wcscmp(sourceClass, CLASS_NAME_MEDIAOUTPUT) == 0) {
                  mMediaSourceItem = s01Item;
               }
            }
         }

         int persistent = s01Item->GetElement()->GetInt(SOURCE_TYPE_PERSISTENT, 0);
         if (persistent == 1) {
            int order = s01Item->GetElement()->GetInt(SOURCE_PERSISTENT_ORDER, 2);
            if (ORDER_TOP == order) {
               s01Item->MoveToTop();
            } else if (ORDER_BOTTON == order) {
               s01Item->MoveToBottom();
            }
         }
      }
   }
   mObs->LeaveSceneMutex();
   return true;
}
void CGraphics::Resize(const RECT& client, bool bRedrawRenderFrame) {
   RECT crc;
   if (bRedrawRenderFrame) {
      GetClientRect(mRenderWnd, &crc);
   } else {
      crc = client;
   }
   if (mObs) {
      mObs->clientWidth = crc.right;
      mObs->clientHeight = crc.bottom;
      mObs->bSizeChanging = true;
      mObs->ResizeRenderFrame(bRedrawRenderFrame);
   }
}
void CGraphics::StopPreview(bool isRestart) {
   if (mObs) {
      mLastSize = mObs->GetBaseSize();
      mObs->Stop(isRestart);
      mMediaSourceItem = NULL;
   }
}
bool CGraphics::Destory() {
   return false;
}
SIZE CGraphics::GetResolution() {
   UINT width;
   UINT height;
   mObs->GetOutputSize(width, height);
   SIZE sz = { width, height };
   return  sz;
}
SIZE CGraphics::GetBaseSize() {
   UINT width;
   UINT height;
   mObs->GetBaseSize(width, height);
   SIZE sz = { width, height };
   return  sz;
}

int CGraphics::GetFps() {
   return mFps;
}
void CGraphics::SetFps(int fps) {
   mFps = fps;
}

ItemModifyType GetItemModifyType(const Vect2 &mousePos, const Vect2 &itemPos, const Vect2 &itemSize, const Vect4 &crop, const Vect2 &scaleVal);
void CGraphics::OnMouseEvent(const UINT& mouseEvent, const POINTS& mousePos) {
   mObs->EnterSceneMutex();
   OnMouseEventRealization(mouseEvent, mousePos);
   mObs->LeaveSceneMutex();
}
void CGraphics::OnMouseEventRealizationDown(const UINT& mouseEvent, const POINTS& imousePos) {
   POINTS pos = imousePos;
   Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
   Vect2 framePos = mObs->MapWindowToFramePos(mousePos);
   SetFocus(mRenderWnd);
   mObs->scene->DeselectAll();
   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);
   if (!items.Num()) {
      mObs->scene->GetItemsOnPoint(framePos, items);

      if (items.Num()) {
         SceneItem *topItem = items.Last();
         if (IsInPersistent(topItem) == false) {
            mObs->bItemWasSelected = topItem->bSelected;
            topItem->Select(true);
            if (mObs->bEditMode)
            if (mObs->modifyType == ItemModifyType_None)
               mObs->modifyType = ItemModifyType_Move;
         }
      }
   } else {
      mObs->scene->GetItemsOnPoint(framePos, items);
      if (items.Num()) {
         SceneItem *topItem = items.Last();
         mObs->bItemWasSelected = topItem->bSelected;
      }
   }
   if (mouseEvent == WM_LBUTTONDOWN) {
      mObs->bMouseDown = true;
      mObs->lastMousePos = mObs->startMousePos = mousePos;
      mObs->scene->GetSelectedItems(items);
      for (UINT i = 0; i < items.Num(); i++) {
         items[i]->startPos = items[i]->pos;
         items[i]->startSize = items[i]->size;
      }
   } else
      mObs->bRMouseDown = true;
}

void CGraphics::UpdataSourceElement() {
   mObs->EnterSceneMutex();
   List<SceneItem*> items;
   SetFocus(mRenderWnd);
   mObs->scene->DeselectAll();
   mObs->scene->GetAllItems(items);
   if (items.Num()) {
      SceneItem *topItem = items.Last();
      if (IsInPersistent(topItem) == false) {
         mObs->bItemWasSelected = topItem->bSelected;
         topItem->Select(true);
      }
   }

   for (UINT i = 0; i < items.Num(); i++) {
      items[i]->startPos = items[i]->pos;
      items[i]->startSize = items[i]->size;
   }
   mObs->LeaveSceneMutex();
}

void CGraphics::OnMouseEventRealizationUp(const UINT& mouseEvent, const POINTS& imousePos) {
   if (mObs->scene) {
      POINTS pos = imousePos;
      if (mObs->bMouseDown || mObs->bRMouseDown) {
         Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
         bool bControlDown = HIBYTE(GetKeyState(VK_CONTROL)) != 0;
         List<SceneItem*> items;
         if (!mObs->bMouseMoved) {
            Vect2 framePos = mObs->MapWindowToFramePos(mousePos);
            mObs->scene->GetItemsOnPoint(framePos, items);
            if (bControlDown && mObs->bItemWasSelected) {
               for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
                  SceneItem *item = mObs->scene->GetSceneItem(i);
                  if (item) {
                     item->Select(false);
                  }
               }
               SceneItem *lastItem = items.Last();
               lastItem->Select(false);
            } else {
               if (items.Num()) {
                  SceneItem *topItem = items.Last();
                  if (!bControlDown) {
                     //mObs->scene->DeselectAll();
                  }
                  for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
                     SceneItem *item = mObs->scene->GetSceneItem(i);
                     if (item) {
                        item->Select(false);
                     }
                  }
                  topItem->Select(true);
                  SetFocus(mRenderWnd);
                  if (mObs->modifyType == ItemModifyType_None)
                     mObs->modifyType = ItemModifyType_Move;
               } 
               else if (!bControlDown) {
                 //clicked on empty space without control
                  //mObs->scene->DeselectAll();
               }
            }
         } else if (mouseEvent == WM_LBUTTONUP) {
            mObs->scene->GetSelectedItems(items);
            ReleaseCapture();
            mObs->bMouseMoved = false;
            for (UINT i = 0; i < items.Num(); i++) {
               SceneItem *item = items[i];
               /* 暂时注释解决非全屏摄像头第一次移动飘到左上角问题 */
               //                   (item->pos += 0.5f).Floor();
               //                   (item->size += 0.5f).Floor();
               XElement *itemElement = item->GetElement();
               if (itemElement) {
                  XElement *data = itemElement->GetElement(TEXT("data"));
                  if (data) {
                     float x = item->pos.x;
                     float y = item->pos.y;
                     float w = item->size.x;
                     float h = item->size.y;

                     x /= mBaseSize.cx;
                     y /= mBaseSize.cy;
                     w /= mBaseSize.cx;
                     h /= mBaseSize.cy;

                     data->SetFloat(TEXT("x"), x);
                     data->SetFloat(TEXT("y"), y);
                     data->SetFloat(TEXT("cx"), w);
                     data->SetFloat(TEXT("cy"), h);
                     data->SetInt(L"isFullscreen", 0);
                  }
               }
            }
            mObs->modifyType = ItemModifyType_None;
         }
         mObs->bMouseDown = false;
         mObs->bRMouseDown = false;
      }
   }
}

void CGraphics::ModifyTypeItemMove(List<SceneItem*> &items, bool bControlDown, Vect2 &totalAdjust, Vect2 &snapSize, Vect2 &baseRenderSize) {
   bool bFoundSnap = false;
   for (UINT i = 0; i < items.Num(); i++) {
      SceneItem *item = items[i];
      if (item->GetSourceType() == SRC_MONITOR) {
         return;
      }
      item->pos = item->startPos + totalAdjust;
      if (!bControlDown) {
         Vect2 pos = item->pos;
         Vect2 bottomRight = pos + item->size;
         pos += item->GetCropTL();
         bottomRight += item->GetCropBR();

         bool bVerticalSnap = true;
         if (CloseFloat(pos.x, 0.0f, snapSize.x))
            item->pos.x = -item->GetCrop().x;
         else if (CloseFloat(bottomRight.x, baseRenderSize.x, snapSize.x))
            item->pos.x = baseRenderSize.x - item->size.x + item->GetCrop().w;
         else
            bVerticalSnap = false;

         bool bHorizontalSnap = true;
         if (CloseFloat(pos.y, 0.0f, snapSize.y))
            item->pos.y = -item->GetCrop().y;
         else if (CloseFloat(bottomRight.y, baseRenderSize.y, snapSize.y))
            item->pos.y = baseRenderSize.y - item->size.y + item->GetCrop().z;
         else
            bHorizontalSnap = true;

         if (bVerticalSnap || bHorizontalSnap) {
            bFoundSnap = true;
            totalAdjust = item->pos - item->startPos;
         }
      }

   }

   if (bFoundSnap) {
      for (UINT i = 0; i < items.Num(); i++) {
         SceneItem *item = items[i];
         item->pos = item->startPos + totalAdjust;
         Vect2 itemSize = item->GetSize();
         if (item->pos.x>mBaseSize.cx - SOURCEMARGIN) {
            item->pos.x = mBaseSize.cx - SOURCEMARGIN;
         }

         if (item->pos.y > mBaseSize.cy - SOURCEMARGIN) {
            item->pos.y = mBaseSize.cy - SOURCEMARGIN;
         }

         if (item->pos.x + itemSize.x < SOURCEMARGIN) {
            item->pos.x = SOURCEMARGIN - itemSize.x;
         }

         if (item->pos.y + itemSize.y < SOURCEMARGIN) {
            item->pos.y = SOURCEMARGIN - itemSize.y;
         }
      }
   }
}


void CGraphics::AnalysisModifyType(ItemModifyType modifyType, Vect2 &mousePos) {
   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);

   SceneItem *&scaleItem = mObs->scaleItem;
   Vect2 scaleVal = mObs->GetWindowToFrameScale();
   Vect2 baseRenderSize = mObs->GetBaseSize();
   Vect2 framePos = mObs->MapWindowToFramePos(mousePos);
   bool bControlDown = HIBYTE(GetKeyState(VK_LCONTROL)) != 0 || HIBYTE(GetKeyState(VK_RCONTROL)) != 0;
   bool bKeepAspect = HIBYTE(GetKeyState(VK_LSHIFT)) == 0 && HIBYTE(GetKeyState(VK_RSHIFT)) == 0;
   bool bCropSymmetric = bKeepAspect;
   Vect2 totalAdjust = (mousePos - mObs->startMousePos)*scaleVal;
   Vect2 frameStartMousePos = mObs->MapWindowToFramePos(mObs->startMousePos);
   Vect2 minSize = scaleVal*21.0f;
   Vect2 snapSize = scaleVal*10.0f;
   Vect2 baseScale;
   float baseScaleAspect;
   Vect2 cropFactor;
   Vect2 pos;

   if (scaleItem) {
      if (scaleItem->GetSource()) {
         baseScale = scaleItem->GetSource()->GetSize();
      } else {
         bKeepAspect = false;
         baseScale = scaleItem->size;
      }

      baseScaleAspect = baseScale.x / baseScale.y;
      cropFactor = baseScale / scaleItem->GetSize();
   }

   int edgeAll = edgeLeft | edgeRight | edgeBottom | edgeTop;
   switch (modifyType) {
   case ItemModifyType_Move:
      ModifyTypeItemMove(items, bControlDown, totalAdjust, snapSize, baseRenderSize);
      break;
   case ItemModifyType_CropTop:
      if (!scaleItem)
         break;
      scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
      if (!bCropSymmetric)
         scaleItem->crop.z = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeTop | (!bCropSymmetric ? edgeBottom : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropBottom:
      if (!scaleItem)
         break;
      scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
      if (!bCropSymmetric)
         scaleItem->crop.y = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeBottom | (!bCropSymmetric ? edgeTop : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropLeft:
      if (!scaleItem)
         break;
      scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
      if (!bCropSymmetric)
         scaleItem->crop.w = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | (!bCropSymmetric ? edgeRight : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropRight:
      if (!scaleItem)
         break;
      scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
      if (!bCropSymmetric)
         scaleItem->crop.x = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | (!bCropSymmetric ? edgeLeft : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropBottomLeft:
      if (!scaleItem)
         break;
      if (bCropSymmetric) {
         scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
         scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
      } else {
         float amount = MIN(((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y), ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x));
         scaleItem->crop.w = amount * cropFactor.x;
         scaleItem->crop.x = amount * cropFactor.x;
         scaleItem->crop.y = amount * cropFactor.y;
         scaleItem->crop.z = amount * cropFactor.y;
      }
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | edgeBottom | (!bCropSymmetric ? edgeAll : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropBottomRight:
      if (!scaleItem)
         break;
      if (bCropSymmetric) {
         scaleItem->crop.z = ((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y) * cropFactor.y;
         scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
      } else {
         float amount = MIN(((scaleItem->pos.y + scaleItem->size.y - frameStartMousePos.y) - totalAdjust.y), ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x));
         scaleItem->crop.w = amount * cropFactor.x;
         scaleItem->crop.x = amount * cropFactor.x;
         scaleItem->crop.y = amount * cropFactor.y;
         scaleItem->crop.z = amount * cropFactor.y;
      }
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | edgeBottom | (!bCropSymmetric ? edgeAll : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropTopLeft:
      if (!scaleItem)
         break;
      if (bCropSymmetric) {
         scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
         scaleItem->crop.x = ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x) * cropFactor.x;
      } else {
         float amount = MIN(((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y), ((frameStartMousePos.x - scaleItem->pos.x) + totalAdjust.x));
         scaleItem->crop.w = amount * cropFactor.x;
         scaleItem->crop.x = amount * cropFactor.x;
         scaleItem->crop.y = amount * cropFactor.y;
         scaleItem->crop.z = amount * cropFactor.y;
      }
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeLeft | edgeTop | (!bCropSymmetric ? edgeAll : 0), !bCropSymmetric);
      break;

   case ItemModifyType_CropTopRight:
      if (!scaleItem)
         break;
      if (bCropSymmetric) {
         scaleItem->crop.y = ((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y) * cropFactor.y;
         scaleItem->crop.w = ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x) * cropFactor.x;
      } else {
         float amount = MIN(((frameStartMousePos.y - scaleItem->pos.y) + totalAdjust.y), ((scaleItem->pos.x + scaleItem->size.x - frameStartMousePos.x) - totalAdjust.x));
         scaleItem->crop.w = amount * cropFactor.x;
         scaleItem->crop.x = amount * cropFactor.x;
         scaleItem->crop.y = amount * cropFactor.y;
         scaleItem->crop.z = amount * cropFactor.y;
      }
      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeRight | edgeTop | (!bCropSymmetric ? edgeAll : 0), !bCropSymmetric);
      break;
   case ItemModifyType_ScaleBottom:
      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->GetCropTL();

      scaleItem->size.y = scaleItem->startSize.y + totalAdjust.y;
      if (scaleItem->size.y < minSize.y)
         scaleItem->size.y = minSize.y;

      if (!bControlDown) {
         float bottom = scaleItem->pos.y + scaleItem->size.y - scaleItem->GetCrop().z;

         if (CloseFloat(bottom, baseRenderSize.y, snapSize.y)) {
            bottom = baseRenderSize.y;
            scaleItem->size.y = bottom - scaleItem->pos.y + scaleItem->GetCrop().z;
         }
      }

      if (bKeepAspect)
         scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
      else
         scaleItem->size.x = scaleItem->startSize.x;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropTL();

      break;
   case ItemModifyType_ScaleTop:

      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

      scaleItem->size.y = scaleItem->startSize.y - totalAdjust.y;
      if (scaleItem->size.y < minSize.y)
         scaleItem->size.y = minSize.y;

      if (!bControlDown) {
         float top = scaleItem->startPos.y + (scaleItem->startSize.y - scaleItem->size.y) + scaleItem->GetCrop().x;
         if (CloseFloat(top, 0.0f, snapSize.y))
            scaleItem->size.y = scaleItem->startPos.y + scaleItem->startSize.y + scaleItem->GetCrop().x;
      }

      if (bKeepAspect)
         scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
      else
         scaleItem->size.x = scaleItem->startSize.x;

      totalAdjust.y = scaleItem->startSize.y - scaleItem->size.y;
      scaleItem->pos.y = scaleItem->startPos.y + totalAdjust.y;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;

      break;
   case ItemModifyType_ScaleRight:
      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->GetCropTL();

      scaleItem->size.x = scaleItem->startSize.x + totalAdjust.x;
      if (scaleItem->size.x < minSize.x)
         scaleItem->size.x = minSize.x;

      if (!bControlDown) {
         float right = scaleItem->pos.x + scaleItem->size.x - scaleItem->GetCrop().y;

         if (CloseFloat(right, baseRenderSize.x, snapSize.x)) {
            right = baseRenderSize.x;
            scaleItem->size.x = right - scaleItem->pos.x + scaleItem->GetCrop().y;
         }
      }

      if (bKeepAspect)
         scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      else
         scaleItem->size.y = scaleItem->startSize.y;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropTL();

      break;
   case ItemModifyType_ScaleLeft:

      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

      scaleItem->size.x = scaleItem->startSize.x - totalAdjust.x;
      if (scaleItem->size.x < minSize.x)
         scaleItem->size.x = minSize.x;

      if (!bControlDown) {
         float left = scaleItem->startPos.x + (scaleItem->startSize.x - scaleItem->size.x) + scaleItem->GetCrop().w;

         if (CloseFloat(left, 0.0f, snapSize.x))
            scaleItem->size.x = scaleItem->startPos.x + scaleItem->startSize.x + scaleItem->GetCrop().w;
      }

      if (bKeepAspect)
         scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      else
         scaleItem->size.y = scaleItem->startSize.y;

      totalAdjust.x = scaleItem->startSize.x - scaleItem->size.x;
      scaleItem->pos.x = scaleItem->startPos.x + totalAdjust.x;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;

      break;
   case ItemModifyType_ScaleBottomRight:

      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->GetCropTL();

      scaleItem->size = scaleItem->startSize + totalAdjust;
      scaleItem->size.ClampMin(minSize);

      if (!bControlDown) {
         Vect2 cropPart = Vect2(scaleItem->GetCrop().y, scaleItem->GetCrop().z);
         Vect2 lowerRight = scaleItem->pos + scaleItem->size - cropPart;


         if (CloseFloat(lowerRight.x, baseRenderSize.x, snapSize.x)) {
            lowerRight.x = baseRenderSize.x;
            scaleItem->size.x = lowerRight.x - scaleItem->pos.x + cropPart.x;
         }
         if (CloseFloat(lowerRight.y, baseRenderSize.y, snapSize.y)) {
            lowerRight.y = baseRenderSize.y;
            scaleItem->size.y = lowerRight.y - scaleItem->pos.y + cropPart.y;
         }
      }

      if (bKeepAspect) {
         float scaleAspect = scaleItem->size.x / scaleItem->size.y;
         if (scaleAspect < baseScaleAspect)
            scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
         else if (scaleAspect > baseScaleAspect)
            scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      }

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropTL();

      break;

   case ItemModifyType_ScaleTopLeft:

      if (!scaleItem)
         break;
      pos = scaleItem->pos + scaleItem->size + scaleItem->GetCropBR();

      scaleItem->size = scaleItem->startSize - totalAdjust;
      scaleItem->size.ClampMin(minSize);

      if (!bControlDown) {
         Vect2 cropPart = Vect2(scaleItem->crop.w, scaleItem->crop.x);
         Vect2 topLeft = scaleItem->startPos + (scaleItem->startSize - scaleItem->size) + cropPart;

         if (CloseFloat(topLeft.x, 0.0f, snapSize.x))
            scaleItem->size.x = scaleItem->startPos.x + scaleItem->startSize.x + cropPart.x;
         if (CloseFloat(topLeft.y, 0.0f, snapSize.y))
            scaleItem->size.y = scaleItem->startPos.y + scaleItem->startSize.y + cropPart.y;
      }

      if (bKeepAspect) {
         float scaleAspect = scaleItem->size.x / scaleItem->size.y;
         if (scaleAspect < baseScaleAspect)
            scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
         else if (scaleAspect > baseScaleAspect)
            scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      }

      totalAdjust = scaleItem->startSize - scaleItem->size;
      scaleItem->pos = scaleItem->startPos + totalAdjust;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropBR() - scaleItem->size;

      break;
   case ItemModifyType_ScaleBottomLeft:

      if (!scaleItem)
         break;
      pos = scaleItem->pos + Vect2(scaleItem->size.x, 0) + scaleItem->GetCropTR();

      scaleItem->size.x = scaleItem->startSize.x - totalAdjust.x;
      scaleItem->size.y = scaleItem->startSize.y + totalAdjust.y;
      scaleItem->size.ClampMin(minSize);

      if (!bControlDown) {
         Vect2 cropPart = Vect2(scaleItem->GetCrop().w, scaleItem->GetCrop().z);
         float left = scaleItem->startPos.x + (scaleItem->startSize.x - scaleItem->size.x) + cropPart.x;
         float bottom = scaleItem->pos.y + scaleItem->size.y - cropPart.y;

         if (CloseFloat(left, 0.0f, snapSize.x))
            scaleItem->size.x = scaleItem->startPos.x + scaleItem->startSize.x + cropPart.x;

         if (CloseFloat(bottom, baseRenderSize.y, snapSize.y)) {
            bottom = baseRenderSize.y;
            scaleItem->size.y = bottom - scaleItem->pos.y + cropPart.y;
         }
      }

      if (bKeepAspect) {
         float scaleAspect = scaleItem->size.x / scaleItem->size.y;
         if (scaleAspect < baseScaleAspect)
            scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
         else if (scaleAspect > baseScaleAspect)
            scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      }

      totalAdjust.x = scaleItem->startSize.x - scaleItem->size.x;
      scaleItem->pos.x = scaleItem->startPos.x + totalAdjust.x;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropTR() - Vect2(scaleItem->size.x, 0);

      break;

   case ItemModifyType_ScaleTopRight:

      if (!scaleItem)
         break;

      pos = scaleItem->pos + Vect2(0, scaleItem->size.y) + scaleItem->GetCropBL();

      scaleItem->size.x = scaleItem->startSize.x + totalAdjust.x;
      scaleItem->size.y = scaleItem->startSize.y - totalAdjust.y;
      scaleItem->size.ClampMin(minSize);

      if (!bControlDown) {
         Vect2 cropPart = Vect2(scaleItem->GetCrop().y, scaleItem->GetCrop().x);
         float right = scaleItem->pos.x + scaleItem->size.x - cropPart.x;
         float top = scaleItem->startPos.y + (scaleItem->startSize.y - scaleItem->size.y) + cropPart.y;

         if (CloseFloat(right, baseRenderSize.x, snapSize.x)) {
            right = baseRenderSize.x;
            scaleItem->size.x = right - scaleItem->pos.x + cropPart.x;
         }

         if (CloseFloat(top, 0.0f, snapSize.y))
            scaleItem->size.y = scaleItem->startPos.y + scaleItem->startSize.y + cropPart.y;
      }

      if (bKeepAspect) {
         float scaleAspect = scaleItem->size.x / scaleItem->size.y;
         if (scaleAspect < baseScaleAspect)
            scaleItem->size.x = scaleItem->size.y*baseScaleAspect;
         else if (scaleAspect > baseScaleAspect)
            scaleItem->size.y = scaleItem->size.x / baseScaleAspect;
      }

      totalAdjust.y = scaleItem->startSize.y - scaleItem->size.y;
      scaleItem->pos.y = scaleItem->startPos.y + totalAdjust.y;

      mObs->EnsureCropValid(scaleItem, minSize, snapSize, bControlDown, edgeAll, false);

      scaleItem->pos = pos - scaleItem->GetCropBL() - Vect2(0, scaleItem->size.y);

      break;
   }

}

void CGraphics::ModifyTypeConvert() {
   bool isCropping = HIBYTE(GetKeyState(VK_MENU)) != 0;
   if (isCropping) {
      switch (mObs->modifyType) {
      case ItemModifyType_ScaleTop:
         mObs->modifyType = ItemModifyType_CropTop;
         break;
      case ItemModifyType_ScaleLeft:
         mObs->modifyType = ItemModifyType_CropLeft;
         break;
      case ItemModifyType_ScaleRight:
         mObs->modifyType = ItemModifyType_CropRight;
         break;
      case ItemModifyType_ScaleBottom:
         mObs->modifyType = ItemModifyType_CropBottom;
         break;
      case ItemModifyType_ScaleTopLeft:
         mObs->modifyType = ItemModifyType_CropTopLeft;
         break;
      case ItemModifyType_ScaleTopRight:
         mObs->modifyType = ItemModifyType_CropTopRight;
         break;
      case ItemModifyType_ScaleBottomRight:
         mObs->modifyType = ItemModifyType_CropBottomRight;
         break;
      case ItemModifyType_ScaleBottomLeft:
         mObs->modifyType = ItemModifyType_CropBottomLeft;
         break;
      }
   }

}
void CGraphics::ItemScale(SceneItem *&scaleItem) {
   if (scaleItem) {
      Vect2 itemSize = scaleItem->GetSize();
      if (scaleItem->pos.x > mBaseSize.cx - SOURCEMARGIN) {
         scaleItem->pos.x = mBaseSize.cx - SOURCEMARGIN;
      }

      if (scaleItem->pos.y > mBaseSize.cy - SOURCEMARGIN) {
         scaleItem->pos.y = mBaseSize.cy - SOURCEMARGIN;
      }

      if (scaleItem->pos.x + itemSize.x < SOURCEMARGIN) {
         scaleItem->pos.x = SOURCEMARGIN - itemSize.x;
      }

      if (scaleItem->pos.y + itemSize.y < SOURCEMARGIN) {
         scaleItem->pos.y = SOURCEMARGIN - itemSize.y;
      }
   }

}

void CGraphics::OnMouseEventRealizationMoveWithoutMouseDown(const UINT& mouseEvent, const POINTS& imousePos) {
   POINTS pos = imousePos;
   Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
   if (mObs) {
      SceneItem *&scaleItem = mObs->scaleItem;

      if (!mObs->bMouseMoved && mousePos.Dist(mObs->startMousePos) > 2.0f) {
         SetCapture(mRenderWnd);
         mObs->bMouseMoved = true;
      }

      if (!mObs->bMouseMoved) {
         return;
      }

      ModifyTypeConvert();

      //auto proportion
      if (mIsAutoMode&&scaleItem&&ItemModifyType_Move != mObs->modifyType) {
         XElement *sourceElement = scaleItem->GetElement();
         String classType = sourceElement->GetString(L"class");
         if (scmpi(classType, CLASS_NAME_MONITOR) == 0) {
            scaleItem->size = scaleItem->source->GetSize();
            return;
         }
      }

      if (scaleItem&&scaleItem->GetSourceType() == SRC_MONITOR) {
         return;
      }

      AnalysisModifyType(mObs->modifyType, mousePos);
      ItemScale(scaleItem);
      RecordDataSourceInfo();

      mObs->lastMousePos = mousePos;
   }
}

void CGraphics::RecordDataSourceInfo() {
   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);
   for (UINT i = 0; i < items.Num(); i++) {
      SceneItem *item = items[i];
      if (item == this->mMediaSourceItem) {

         this->mMediaSourcePos.x = item->pos.x;
         this->mMediaSourcePos.y = item->pos.y;
         this->mMediaSourcePos.x /= mBaseSize.cx;
         this->mMediaSourcePos.y /= mBaseSize.cy;

         this->mMediaSourceSize.x = item->size.x;
         this->mMediaSourceSize.y = item->size.y;
         this->mMediaSourceSize.x /= mBaseSize.cx;
         this->mMediaSourceSize.y /= mBaseSize.cy;

         this->mMediaSourcePosSizeInit = true;

         XElement *sourceElement = item->GetElement();
         if (sourceElement) {
            XElement *dataElement = sourceElement->GetElement(TEXT("data"));
            dataElement->SetInt(L"isFullscreen", 0);
         }
      } else {
         XElement *sourceElement = item->GetElement();
         String classType = sourceElement->GetString(L"class");
         if (scmpi(classType, CLASS_NAME_DEVICE) == 0
             ||
             scmpi(classType, CLASS_NAME_DECKLINK) == 0
             ) {
            XElement *dataElement = sourceElement->GetElement(TEXT("data"));
            if (dataElement == NULL) {
               continue;
            }

            bool isFullScreen = false;
            int x = item->GetPos().x;
            int y = item->GetPos().y;
            int cx = item->GetSize().x;
            int cy = item->GetSize().y;

            dataElement->SetInt(L"isFullscreen", isFullScreen ? 1 : 0);
            dataElement->SetInt(L"posType", (int)enum_PosType_custom);
            dataElement->SetInt(L"x", x);
            dataElement->SetInt(L"y", y);
            dataElement->SetInt(L"cx", cx);
            dataElement->SetInt(L"cy", cy);
         }
      }
   }

}


void CGraphics::OnMouseEventRealizationMoveWithMouseDown(const UINT& mouseEvent, const POINTS& imousePos) {
   POINTS pos = imousePos;
   Vect2 mousePos = Vect2(float(pos.x), float(pos.y));
   Vect2 scaleVal = mObs->GetWindowToFrameScale();
   SceneItem *&scaleItem = mObs->scaleItem; //just reduces a bit of typing
   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);
   mObs->scaleItem = NULL;
   mObs->modifyType = ItemModifyType_None;
   for (int i = int(items.Num() - 1); i >= 0; i--) {
      SceneItem *item = items[i];
      if (IsInPersistent(item))
         continue;
      // Get item in window coordinates
      Vect2 adjPos = mObs->MapFrameToWindowPos(item->GetPos());
      Vect2 adjSize = mObs->MapFrameToWindowSize(item->GetSize());
      Vect2 adjSizeBase = mObs->MapFrameToWindowSize(item->GetSource() ? item->GetSource()->GetSize() : item->GetSize());

      ItemModifyType curType = GetItemModifyType(mousePos, adjPos, adjSize, item->GetCrop(), scaleVal);
      if (curType > ItemModifyType_Move) {
         mObs->modifyType = curType;
         scaleItem = item;
         break;
      } else if (curType == ItemModifyType_Move)
         mObs->modifyType = ItemModifyType_Move;
   }
}

void CGraphics::OnMouseEventRealizationMove(const UINT& mouseEvent, const POINTS& imousePos) {
   if (!mObs->bMouseDown) {
      OnMouseEventRealizationMoveWithMouseDown(mouseEvent, imousePos);
   } 
   else {
      OnMouseEventRealizationMoveWithoutMouseDown(mouseEvent, imousePos);
   }
   SyncCursor();
}
void CGraphics::SyncCursor() {
   switch (mObs->modifyType) {
   case ItemModifyType_ScaleBottomLeft:
   case ItemModifyType_ScaleTopRight:
      SetCursor(LoadCursor(NULL, IDC_SIZENESW));
      return;

   case ItemModifyType_ScaleBottomRight:
   case ItemModifyType_ScaleTopLeft:
      SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
      return;

   case ItemModifyType_CropLeft:
   case ItemModifyType_CropRight:
   case ItemModifyType_ScaleLeft:
   case ItemModifyType_ScaleRight:
      SetCursor(LoadCursor(NULL, IDC_SIZEWE));
      return;

   case ItemModifyType_CropTop:
   case ItemModifyType_CropBottom:
   case ItemModifyType_ScaleTop:
   case ItemModifyType_ScaleBottom:
      SetCursor(LoadCursor(NULL, IDC_SIZENS));
      return;

   default:
      SetCursor(LoadCursor(NULL, IDC_ARROW));
      return;
   }

}

void CGraphics::OnMouseEventRealization(const UINT& mouseEvent, const POINTS& imousePos) {
    if (!mObs) {
        return;
    }
    mObs->EnterSceneMutex();
    if (!mObs->scene) {
        mObs->LeaveSceneMutex();
        return;
    }

    mObs->bEditMode = true;
    if (mouseEvent == WM_LBUTTONDOWN || mouseEvent == WM_RBUTTONDOWN) {
        OnMouseEventRealizationDown(mouseEvent, imousePos);
    } else if (mouseEvent == WM_MOUSEMOVE) {
        OnMouseEventRealizationMove(mouseEvent, imousePos);
    } else if (mouseEvent == WM_LBUTTONUP || mouseEvent == WM_RBUTTONUP) {
        OnMouseEventRealizationUp(mouseEvent, imousePos);
    }
    mObs->LeaveSceneMutex();
}
unsigned char** CGraphics::LockCurrentFramePic(QWORD &t,unsigned long long currentTime) {
   if (mObs) {
      return mObs->LockCurrentFramePic(t, currentTime);
   }
   return NULL;
}
void CGraphics::UnlockCurrentFramePic() {
   return mObs->UnlockCurrentFramePic();
}
void CGraphics::SetSourceDeleteHook(IGraphicDeleteSourceHook funcDeleteSourceHook, void *param) {
   this->funcDeleteSourceHook = funcDeleteSourceHook;
   this->mDeleteSourceHookParam = param;
}
void CGraphics::SourceRefit(void *item) {
   mObs->EnterSceneMutex();
   if (!item) {
      mObs->LeaveSceneMutex();
      return;
   }
   /* 当渲染线程失败进行重置调用ThreadOBSReset时，会先清空各个场景源，但是并没有重置mMediaSourceItem，
   所以如果插播视频继续的话，对象已经被释放就会出现崩溃问题。所以Refit时重新获取下插播渲染源，进行重置。 */
   SceneItem *tempItem = GetSceneItem(SRC_MEDIA_OUT);
   if (tempItem != NULL && tempItem != mMediaSourceItem) {
      mMediaSourceItem = tempItem;
      item = tempItem;
   }

   if (mMediaSourceItem != item) {
      mObs->LeaveSceneMutex();
      return;
   }

   SceneItem *sceneItem = (SceneItem *)item;
   XElement *sourceElement = sceneItem->GetElement();
   if (sourceElement) {
      XElement *data = sourceElement->GetElement(L"data");
      if (data) {
         bool isFullscreen = data->GetInt(L"isFullscreen", 0) != 0;
         if (isFullscreen) {
            FitItemsToScreen(sceneItem);
            mObs->LeaveSceneMutex();
            return;
         }
      }
   }

   if (!mMediaSourcePosSizeInit) {
      FitItemsToScreen(sceneItem);
   } else {
      Vect2 sourceSize = sceneItem->GetSource()->GetSize();
      if (sourceSize.y == 0) {
         mObs->LeaveSceneMutex();
         return;
      }

      if (mMediaSourceSize.y == 0) {
         mObs->LeaveSceneMutex();
         return;
      }
      float mBaseSizeK = mBaseSize.cx;
      mBaseSizeK /= mBaseSize.cy;

      float sourceK = sourceSize.x / sourceSize.y;
      float mediaSourceK = mMediaSourceSize.x / mMediaSourceSize.y;
      if (sourceK > mediaSourceK) {
         //mediaw
         mMediaSourceSize.y = mMediaSourceSize.x / sourceK*mBaseSizeK;
      } else {
         //mediah
         mMediaSourceSize.x = mMediaSourceSize.y*sourceK / mBaseSizeK;
      }

      int x = mMediaSourcePos.x*mBaseSize.cx;
      int y = mMediaSourcePos.y*mBaseSize.cy;
      int w = mMediaSourceSize.x*mBaseSize.cx;
      int h = mMediaSourceSize.y*mBaseSize.cy;
      if (x + w < 0) {
          x = 0;
      }
      sceneItem->pos.x = x;
      sceneItem->pos.y = y;
      sceneItem->size.x = w;
      sceneItem->size.y = h;
   }
   mObs->LeaveSceneMutex();
}

void CGraphics::SetSyncWithEncoder(const bool & isWillsync) {
   return mObs->SetSyncWithEncoder(isWillsync);
}

void CGraphics::LoadDefaultSetting() {

}
/*
1.open this default config
2.find scene element
*/
void CGraphics::ReloadSceneConfig() {
   if (!mObs) {
      return;
   }
   String strScenesConfig;
   strScenesConfig = FormattedString(L"%s\\scenes.xconfig", lpAppDataPath);
   if (!mObs->scenesConfig.Open(strScenesConfig))
      CrashError(TEXT("Could not open '%s'"), strScenesConfig.Array());

   XElement *scenes = mObs->scenesConfig.GetElement(TEXT("scenes"));
   if (!scenes)
      scenes = mObs->scenesConfig.CreateElement(TEXT("scenes"));

   XElement *sceneElement = scenes->GetElement(DEFAULT_SCENE_NAME);
   if (sceneElement) {
      scenes->RemoveElement(DEFAULT_SCENE_NAME);
   }

   UINT numScenes = scenes->NumElements();
   if (!numScenes) {
      XElement *scene = scenes->CreateElement(DEFAULT_SCENE_NAME);
      scene->SetString(TEXT("class"), CLASS_SCENE_Scene);
      numScenes++;
   }
}

/*
get the sources element from scenes config
if not exist, create it
*/
XElement* CGraphics::GetSceneElement() {
   return mObs->GetSceneElement();
}

XElement* CGraphics::GetSourcesElement() {
   XElement *scenes = mObs->scenesConfig.GetElement(TEXT("scenes"));
   XElement *sceneElement = scenes->GetElement(DEFAULT_SCENE_NAME);
   if (!sceneElement) {
      sceneElement = scenes->CreateElement(DEFAULT_SCENE_NAME);
      sceneElement->SetString(TEXT("class"), CLASS_SCENE_Scene);
   }
   XElement *sources = sceneElement->GetElement(TEXT("sources"));
   if (!sources)
      sources = sceneElement->CreateElement(TEXT("sources"));
   return  sources;
}
/* 添加摄像头 */
bool CGraphics::processDevice(DeviceInfo deviceInfo, DataSourcePosType m_PosType, int index,HWND renderHwnd, HWND privew_renderHwnd, int isHideSouceItem) {
   OBSApiLog("CGraphics::processDevice\n");
   if (!mObs) {
      OBSApiLog("CGraphics::processDevice OBS is NULL\n");
      return false;
   }
   mObs->EnterSceneMutex();
   if (mObs->scene == nullptr) {
      mObs->LeaveSceneMutex();
      OBSApiLog("CGraphics::processDevice OBS  scene is NULL\n");
      return false;
   }

   String sourcesName;
   sourcesName << NEW_DEVICES_SOURCE << deviceInfo.m_sDeviceName << deviceInfo.m_sDeviceID;

   XElement *sources = GetSourcesElement();
   XElement *deviceSource = sources->InsertElement(0, sourcesName);
   String strName;
   strName << DEFAULT_SOURCE_NAME << L"_DeviceCapture_"<<deviceInfo.m_sDeviceName<<deviceInfo.m_sDeviceID<<deviceInfo.m_sDeviceDisPlayName;
   if (deviceInfo.m_sDeviceType == TYPE_DECKLINK) {
      deviceSource->SetString(L"class", CLASS_NAME_DECKLINK);
   } else {
      deviceSource->SetString(L"class", CLASS_NAME_DEVICE);
   }
   
   deviceSource->SetName(strName);

   XElement *data = deviceSource->InsertElement(0, L"data");

   data->SetString(L"sourceName", strName);
   data->SetInt(L"customResolution", 1);
   data->SetString(L"deviceName", deviceInfo.m_sDeviceName);
   data->SetString(L"deviceID", deviceInfo.m_sDeviceID);
   data->SetString(L"displayName", deviceInfo.m_sDeviceDisPlayName);

   data->SetInt(L"posType", (int)m_PosType);
   data->SetInt(L"renderHwnd", (int)renderHwnd);
   data->SetInt(L"privew_renderHwnd",(int)privew_renderHwnd);
   data->SetInt(L"isHideSouceItem", isHideSouceItem);
   UINT width = 1280;
   UINT height = 720;
   int frameInternal = 333333;
   DeinterlacingType type = DEINTERLACING_NONE;
   VideoFormat format = VideoFormat::Any;
   OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute");

   if (!GetDeviceDefaultAttribute(deviceInfo,
      width,
      height,
      frameInternal,
      type,
      format)) {

      OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute Failed!");
      if (deviceInfo.m_sDeviceType == TYPE_DSHOW_VIDEO) {

         OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute Failed TYPE_DSHOW_VIDEO!");
         FrameInfo currentFrameInfo;

         OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute Failed GetDShowVideoFrameInfoList!");
         if (GetDShowVideoFrameInfoList(deviceInfo,
            NULL,
            &currentFrameInfo,
            type
            )) {

            OBSApiLog("CGraphics::processDevice GetDShowVideoFrameInfoList! Successed");
            width = currentFrameInfo.maxCX;
            height = currentFrameInfo.maxCY;
            frameInternal = currentFrameInfo.minFrameInterval;
            format = currentFrameInfo.format;
         } else {
            OBSApiLog("CGraphics::processDevice GetDShowVideoFrameInfoList! Failed!");
         }

      } else if (deviceInfo.m_sDeviceType == TYPE_DECKLINK) {
         OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute Failed TYPE_DECKLINK!");
         GetDeckLinkDeviceInfo(deviceInfo, width, height, frameInternal);
      }
   } else {
      OBSApiLog("CGraphics::processDevice GetDeviceDefaultAttribute Success!");
   }

   OBSApiLog("CGraphics::processDevice InsertImageSource!");
   SceneItem *item = mObs->scene->InsertImageSource(index, deviceSource, SRC_DSHOW_DEVICE);
   OBSApiLog("CGraphics::processDevice InsertImageSource End!");

   if (item) {
      item->GetElement()->MoveToTop();
   }
   OBSApiLog("CGraphics::processDevice FitItemToScreenByPosType!");

   FitItemToScreenByPosType(item, m_PosType, width, height);
   char* devname = WStr2CStr(deviceInfo.m_sDeviceDisPlayName);
   if (devname) {
      OBSApiLog("Add camera:%s width:%d height:%d format:%d", devname, width, height, format);
      free(devname);
   }
   if (isHideSouceItem > 0 && item) {
      Vect2 new_size;
      new_size.x = 1;
      new_size.y = 1;
      item->SetSize(new_size);
      item->source->SetPreviewState(true);
   }
   SceneItem *monitorItem=GetSceneItem(SRC_MONITOR);
   if(monitorItem) {
      monitorItem->MoveToTop();
   }
   mObs->LeaveSceneMutex();
   OBSApiLog("CGraphics::processDevice return !");
   return true;
}
/* 删除摄像头 */
bool CGraphics::deleteDevice(void* sourceCtx) {
   STRU_OBSCONTROL_ADDCAMERA* pAddCamera = (STRU_OBSCONTROL_ADDCAMERA*)sourceCtx;
   if (NULL == pAddCamera) {
      return false;
   }

   if (!mObs) {
      return false;
   }
   if (mObs->scene == nullptr) {
      return false;
   }

   XElement *sources = GetSourcesElement();
   for (UINT i = 0; i < sources->NumElements(); i++) {
      XElement *sourceElement = sources->GetElementByID(i);
      if (sourceElement) {
         String classType = sourceElement->GetString(L"class", L"");
         if (classType != CLASS_NAME_DECKLINK&&classType != CLASS_NAME_DEVICE) {
            continue;
         }

         XElement *data = sourceElement->GetElement(TEXT("data"));
         if (data == NULL) {
            continue;
         }

         DeviceInfo deviceInfo;
         String deviceName = data->GetString(L"deviceName", L"");
         if (deviceName.Length()) {
            wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
         }

         String deviceId = data->GetString(L"deviceID", L"");
         if (deviceId.Length()) {
            wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
         }

         String displayName = data->GetString(L"displayName", L"");
         if (displayName.Length()) {
            wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
         }

         char* devname = WStr2CStr(deviceInfo.m_sDeviceDisPlayName);
         if (devname) {
            free(devname);
         }
         if (classType == CLASS_NAME_DECKLINK) {
            deviceInfo.m_sDeviceType = TYPE_DECKLINK;
         } else if (classType == CLASS_NAME_DEVICE) {
            deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
         }

         String name=data->GetString(L"sourceName", L"");
         SceneItem *item = mObs->scene->GetSceneItem(name);

         if (pAddCamera->m_deviceInfo == deviceInfo) {
            mObs->EnterSceneMutex();
            if (this->funcDeleteSourceHook && this->mDeleteSourceHookParam) {
               if (this->funcDeleteSourceHook(item, this->mDeleteSourceHookParam, SRC_DSHOW_DEVICE))
               {
                  mObs->scene->RemoveImageSource(sourceElement->GetName());
               }
            }
            else {
               mObs->scene->RemoveImageSource(sourceElement->GetName());
            }
            mObs->LeaveSceneMutex();
         }
      }
   }
   return true;
}

bool CGraphics::ModifyBeautyDeviceSource(DeviceInfo& srcDevice, HWND m_PriviewRenderHwnd ,int hidePreview, int beautyLevel) {
   if (!mObs) {
      return NULL;
   }
   mObs->EnterSceneMutex();
   SceneItem *item = GetDeviceItem(srcDevice);
   if (!item) {
      mObs->LeaveSceneMutex();
      return false;
   }

   if (mObs->scene) {
      for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
         SceneItem *item_used = mObs->scene->GetSceneItem(i);
         if (item_used) {
            XElement*sourceElement = item_used->GetElement();
            if (sourceElement) {
               String classType = sourceElement->GetString(L"class", L"");
               if (classType != CLASS_NAME_DECKLINK && classType != CLASS_NAME_DEVICE) {
                  continue;
               }
               //预览时，关闭之前设备占用的窗口
               item_used->source->SetBeautyLevel(0);
            }
         }
      }
   }

   item->source->SetBeautyLevel(beautyLevel);
   item->source->SetBeautyPreviewHandle(m_PriviewRenderHwnd, hidePreview);
   mObs->LeaveSceneMutex();

   return true;
}

bool CGraphics::ModifyDeviceSource(
   DeviceInfo&srcDevice,
   DeviceInfo&desDevice,
   DataSourcePosType posType) {
   if (!mObs) {
      return NULL;
   }

   SceneItem *item = GetDeviceItem(srcDevice);
   if (!item) {
      return false;
   }
   mObs->EnterSceneMutex();

   if (!(srcDevice == desDevice)) {
      int index = 0;
      DataSourcePosType secPosType = emum_PosType_rightDown;
      float x = -1.0f, y = -1.0f, cx = 0.0f, cy = 0.0f;


      for (UINT i = 0; i < mObs->scene->NumSceneItems(); i++) {
         SceneItem *itemSource = mObs->scene->GetSceneItem(i);
         if (item == itemSource) {
            index = i;
            break;
         }
      }

      XElement *sourceElement = item->GetElement();
      if (!sourceElement) {
         mObs->LeaveSceneMutex();
         return false;
      }

      XElement *data = sourceElement->GetElement(L"data");
      if (!data) {
         mObs->LeaveSceneMutex();
         return false;
      }

      secPosType = (DataSourcePosType)data->GetInt(L"posType", (int)emum_PosType_rightDown);
      x = data->GetFloat(L"x", -1.0f);
      y = data->GetFloat(L"y", -1.0f);
      cx = data->GetFloat(L"cx", 0.0f);
      cy = data->GetFloat(L"cy", 0.0f);
      HWND renderHwnd = (HWND)data->GetInt(L"renderHwnd", NULL);
      mObs->scene->RemoveImageSource(sourceElement->GetName());

      if (!processDevice(desDevice, enum_PosType_auto, index,renderHwnd)) {
         mObs->LeaveSceneMutex();
         return false;
      }

      item = GetDeviceItem(desDevice);
      sourceElement = item->GetElement();
      if (!sourceElement) {
         mObs->LeaveSceneMutex();
         return false;
      }

      data = sourceElement->GetElement(L"data");
      if (!data) {
         mObs->LeaveSceneMutex();
         return false;
      }

      if (secPosType == enum_PosType_custom) {
         data->SetFloat(L"x", x);
         data->SetFloat(L"y", y);
         data->SetFloat(L"cx", cx);
         data->SetFloat(L"cy", cy);
         if (FitItemsToScreenCustom(item)) {
            data->SetInt(L"posType", (int)enum_PosType_custom);
         }
      }
   }


   Vect2 size = item->source->GetSize();
   FitItemToScreenByPosType(item, posType, size.x, size.y);
   mObs->LeaveSceneMutex();

   return true;
}
SceneItem *CGraphics::GetDeviceItem(DeviceInfo &deviceInfoPar) {
   DeviceInfo deviceInfo;
   if (!mObs) {
      return NULL;
   }

   mObs->EnterSceneMutex();
   if (mObs->scene) {
      for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
         SceneItem *item = mObs->scene->GetSceneItem(i);
         if (item) {
            XElement*sourceElement = item->GetElement();
            if (sourceElement) {
               String classType = sourceElement->GetString(L"class", L"");
               if (classType != CLASS_NAME_DECKLINK && classType != CLASS_NAME_DEVICE) {
                  continue;
               }

               XElement *data = sourceElement->GetElement(TEXT("data"));
               if (data == NULL) {
                  continue;
               }

               DeviceInfo deviceInfo;
               String deviceName = data->GetString(L"deviceName", L"");
               if (deviceName.Length()) {
                  wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
               }

               String deviceId = data->GetString(L"deviceID", L"");
               if (deviceId.Length()) {
                  wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
               }

               String displayName = data->GetString(L"displayName", L"");
               if (displayName.Length()) {
                  wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
               }

               if (classType == CLASS_NAME_DECKLINK) {
                  deviceInfo.m_sDeviceType = TYPE_DECKLINK;
               }
               else if (classType == CLASS_NAME_DEVICE) {
                  deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
               }

               if (deviceInfoPar == deviceInfo) {

                  mObs->LeaveSceneMutex();
                  return item;
               }
            }
         }
      }
   }
   mObs->LeaveSceneMutex();
   return NULL;
}

ImageSource *CGraphics::GetDeviceSource(DeviceInfo &deviceInfoPar) {
   DeviceInfo deviceInfo;
   ImageSource *source = NULL;
   if (mObs == NULL || mObs->scene == NULL) {
      return NULL;
   }

   mObs->EnterSceneMutex();
   for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
      SceneItem *item = mObs->scene->GetSceneItem(i);
      if (item) {
         XElement*sourceElement = item->GetElement();
         if (sourceElement) {
            String classType = sourceElement->GetString(L"class", L"");
            if (classType != CLASS_NAME_DECKLINK&&classType != CLASS_NAME_DEVICE) {
               continue;
            }

            XElement *data = sourceElement->GetElement(TEXT("data"));
            if (data == NULL) {
               continue;
            }

            DeviceInfo deviceInfo;
            String deviceName = data->GetString(L"deviceName", L"");
            if (deviceName.Length()) {
               wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
            }

            String deviceId = data->GetString(L"deviceID", L"");
            if (deviceId.Length()) {
               wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
            }

            String displayName = data->GetString(L"displayName", L"");
            if (displayName.Length()) {
               wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
            }

            if (classType == CLASS_NAME_DECKLINK) {
               deviceInfo.m_sDeviceType = TYPE_DECKLINK;
            } else if (classType == CLASS_NAME_DEVICE) {
               deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
            }

            if (deviceInfoPar == deviceInfo) {
               source = item->GetSource();
               break;
            }
         }
      }
   }

   mObs->LeaveSceneMutex();
   return source;
}

bool CGraphics::ReloadDevice(DeviceInfo &deviceInfo) {
   if (!mObs) {
      return false;
   }

   ImageSource *source = GetDeviceSource(deviceInfo);
   if (!source) {
      return false;
   }

   mObs->EnterSceneMutex();

   source->Reload();

   mObs->LeaveSceneMutex();

   return true;
}

bool CGraphics::IsHasSource(SOURCE_TYPE type) {
   return NULL != GetSceneItem(type);
}
void CGraphics::ModifyAreaShared(int left, int top, int right, int bottom) {
   if (!mObs) {
      return;
   }

   mObs->EnterSceneMutex();
   SceneItem *item = GetSceneItem(SRC_MONITOR_AREA);
   if (item) {
      XElement *sourceElement = item->GetElement();
      if (sourceElement) {
         XElement *data = sourceElement->GetElement(L"data");
         if (data) {
            VHD_WindowInfo windowInfo;
            data->GetHexListPtr(L"winInfo", (unsigned char *)&windowInfo, sizeof(VHD_WindowInfo));
            bool reFullScreen=false;
            int ow=windowInfo.rect.right-windowInfo.rect.left;
            int oh=windowInfo.rect.bottom-windowInfo.rect.top;
            if(right-left!=ow) {
               reFullScreen=true;
            }
            if(bottom-top!=oh) {
               reFullScreen=true;
            }

            windowInfo.rect.left = left;
            windowInfo.rect.top = top;
            windowInfo.rect.right = right;
            windowInfo.rect.bottom = bottom;
            data->SetHexListPtr(L"winInfo", (unsigned char *)&windowInfo, sizeof(VHD_WindowInfo));

            ImageSource *imgSource = item->GetSource();
            imgSource->UpdateSettings();

            bool isFullScreen = data->GetInt(L"isFullscreen", 0) != 0;
            if (isFullScreen&&reFullScreen) {
               FitItemsToScreen(item);
            }
         }
      }
   }
   mObs->LeaveSceneMutex();
}

bool CGraphics::processScreenArea(void* sourceCtx) {
   bool ret = false;

   RECT captureRect = *(RECT*)(sourceCtx);
   XElement*     sources = GetSourcesElement();
   String strName;
   strName << DEFAULT_SOURCE_NAME << L"_WindowCapture_" << mSourceIndex++;

   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), CLASS_NAME_WINDOW);
   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data)
      data = newSourceElement->CreateElement(TEXT("data"));

   RemoveDownLayerSource();
   data->SetInt(L"sourceType", SRC_MONITOR_AREA);

   VHD_WindowInfo windowInfo = VHD_CreateAreaWindow(captureRect.left, captureRect.top, captureRect.right, captureRect.bottom);
   data->SetHexListPtr(L"winInfo", (unsigned char *)&windowInfo, sizeof(VHD_WindowInfo));

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      SceneItem *item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_MONITOR_AREA);
      FitItemsToScreen(item);
      mObs->LeaveSceneMutex();
   }
   OBSApiLog("CGraphics::processScreenArea ok");

   return ret;
}

/* 桌面捕获数据源 */
bool CGraphics::processMonitor(void* sourceCtx) {

   XElement*     sources = GetSourcesElement();
   String strName;
   RemoveDownLayerSource();
   strName << DEFAULT_SOURCE_NAME << L"_monitor_" << mSourceIndex++;
   //get new source name 
   XElement *newSourceElement = sources->InsertElement(0, strName);
   sources->SetInt(TEXT("render"), 1);

   ClassInfo *imageSourceClass = mObs->GetImageSourceClass(CLASS_NAME_MONITOR);

   newSourceElement->SetString(TEXT("class"), imageSourceClass->strClass);
   //fill source element data

   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data)
      data = newSourceElement->CreateElement(TEXT("data"));

   data->SetInt(L"sourceType", SRC_MONITOR);
   data->SetHexListPtr(L"winInfo", (unsigned char *)sourceCtx, sizeof(VHD_WindowInfo));

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      mObs->scene->DeselectAll();
      SceneItem *item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_MONITOR);
      FitItemsToScreen(item);
      
      mObs->scene->DeselectAll();
      mObs->LeaveSceneMutex();
   }
   return true;
}
/* 获得该类型的数据?? */
SceneItem *CGraphics::GetSceneItem(SOURCE_TYPE type) {
	if (NULL != mObs && NULL != mObs->scene)
	{
		return mObs->scene->GetSceneItemBySourceType(type);
	}
	else
	{
		type = SRC_SOURCE_UNKNOW;
	}
   return NULL;
}
bool CGraphics::processWindow(void* sourceCtx) {
   STRU_OBSCONTROL_WINDOWSRC* pWindowSrc = (STRU_OBSCONTROL_WINDOWSRC*)sourceCtx;
   if (NULL == pWindowSrc) {
      return false;
   }

   Vect2 baseSize = mObs->GetBaseSize();
   bool ret = false;
   XElement*     sources = GetSourcesElement();
   String strName;
   strName << DEFAULT_SOURCE_NAME << L"_WindowCapture_" << mSourceIndex++;
   //get new source name 
   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), CLASS_NAME_WINDOW);
   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data)
      data = newSourceElement->CreateElement(TEXT("data"));

   data->SetInt(L"sourceType", SRC_WINDOWS);
   data->SetHexListPtr(L"winInfo", (unsigned char *)&pWindowSrc->m_windowInfo, sizeof(VHD_WindowInfo));

   RemoveDownLayerSource();

   float cx = pWindowSrc->m_windowInfo.rect.right - pWindowSrc->m_windowInfo.rect.left;
   cx /= baseSize.x;
   float cy = pWindowSrc->m_windowInfo.rect.bottom - pWindowSrc->m_windowInfo.rect.top;
   cy /= baseSize.y;

   data->SetFloat(L"x", 0);
   data->SetFloat(L"y", 0);
   data->SetFloat(L"cx", cx);
   data->SetFloat(L"cy", cy);

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      SceneItem *item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_WINDOWS);
      item->SetSourceType(SRC_WINDOWS);
      FitItemsToScreen(item);
      mObs->LeaveSceneMutex();
   }
   return ret;
}
void CGraphics::FitItemsToScreenRightDown(SceneItem *item, int width, int height) {
   float x, y, cx, cy;
   Vect2 baseSize = mObs->GetBaseSize();
   float k = width;
   bool isReset = false;
   if (height != 0) {
      float baseSizeK = baseSize.x;
      baseSizeK /= baseSize.y;

      k /= height;
#define VIDEODEFAULTWIDTH 0.2678571
      cx = VIDEODEFAULTWIDTH;
      cy = cx / k*baseSizeK;
      x = 1 - cx;
      y = 1 - cy;
   } else {
      x = 0.0f;
      y = 0.0f;
      cx = 1.0f;
      cy = 1.0f;
      isReset = true;
   }
   Vect2 pos;
   pos.x = x*baseSize.x;
   pos.y = y*baseSize.y;
   Vect2 size;
   size.x = cx*baseSize.x;
   size.y = cy*baseSize.y;
   item->SetPos(pos);
   item->SetSize(size);

}
bool CGraphics::FitItemsToScreenCustom(SceneItem *item) {
   if (!mObs) {
      return false;
   }

   Vect2 baseSize = mObs->GetBaseSize();

   XElement *source = item->GetElement();
   if (!source) {
      return false;
   }

   XElement *data = source->GetElement(L"data");
   if (!data) {
      return false;
   }


   float x = data->GetFloat(TEXT("x"), -1.0f);
   float y = data->GetFloat(TEXT("y"), -1.0f);
   float cx = data->GetFloat(TEXT("cx"), 0.0f);
   float cy = data->GetFloat(TEXT("cy"), 0.0f);

   if (cx == 0.0f || cy == 0.0f || x + cx<0.0f || x + cx>1.0f || y + cy<0.0f || y + cy>1.0f) {
      return false;
   }

   Vect2 pos;
   pos.x = x*baseSize.x;
   pos.y = y*baseSize.y;
   Vect2 size;
   size.x = cx*baseSize.x;
   size.y = cy*baseSize.y;
   item->SetPos(pos);
   item->SetSize(size);
   return true;
}

void CGraphics::FitItemToScreenByPosType(SceneItem *item,
                                         DataSourcePosType type, int width, int height) {
   if (!item) {
      return;
   }

   if (type == enum_PosType_fullScreen) {
      FitItemsToScreen(item);
   } else if (type == emum_PosType_rightDown) {
      FitItemsToScreenRightDown(item, width, height);
   } else if (enum_PosType_custom) {
      FitItemsToScreenCustom(item);
   } else if (enum_PosType_auto) {
      type = enum_PosType_custom;
      if (!FitItemsToScreenCustom(item)) {
         type = emum_PosType_rightDown;
         FitItemsToScreenRightDown(item, width, height);
      }
   }

   XElement *source = item->GetElement();
   if (!source) {
      return;
   }

   XElement *data = source->GetElement(L"data");
   if (!data) {
      return;
   }

   data->SetInt(L"posType", (int)type);
}

void CGraphics::FitItemsToScreen(SceneItem *item) {

   Vect2 baseSize = mObs->GetBaseSize();
   double baseAspect = double(baseSize.x) / double(baseSize.y);
   if (item->source) {
      Vect2 itemSize = item->source->GetSize();
      itemSize.x -= (item->crop.x + item->crop.w);
      itemSize.y -= (item->crop.y + item->crop.z);
      OBSApiLog("FitItemsToScreen %f %f %f %f", item->crop.x, item->crop.w, item->crop.y, item->crop.z);
      Vect2 size = baseSize;
      double sourceAspect = double(itemSize.x) / double(itemSize.y);
      OBSApiLog("FitItemsToScreen baseAspect %f sourceAspect %f", baseAspect, sourceAspect);
      if (!CloseDouble(baseAspect, sourceAspect)) {
         if (item->GetSourceType() == SRC_MEDIA_OUT) {
            if (baseAspect > sourceAspect)
                if (itemSize.y > baseSize.y && itemSize.x < baseSize.x) {
                    //视频竖屏录制的视频
                    size.x = float(double(baseSize.y) * sourceAspect);
                }
                else if (itemSize.y < baseSize.y && itemSize.x < baseSize.x) {
                    //视频小于画布大小
                    size.y = baseSize.y;
                    size.x = float(double(baseSize.y) * sourceAspect);
                }
                else {
                    size.y = float(double(size.x) / sourceAspect);
                }
            else
               size.x = float(double(size.y) * sourceAspect);
         } else {
            if (baseAspect < sourceAspect)
               size.y = float(double(size.x) / sourceAspect);
            else
               size.x = float(double(size.y) * sourceAspect);
         }


         size.x = (float)round(size.x);
         size.y = (float)round(size.y);
      }

      Vect2 scale = itemSize / size;

      OBSApiLog("size.x %f size.y %f  scale.x %f scale.y %f", size.x, size.y, scale.x, scale.y);


      size.x += (item->crop.x + item->crop.w) / scale.x;
      size.y += (item->crop.y + item->crop.z) / scale.y;
      item->size = size;

      Vect2 pos;
      pos.x = (baseSize.x*0.5f) - ((item->size.x + item->GetCrop().x - item->GetCrop().w)*0.5f);
      pos.y = (baseSize.y*0.5f) - ((item->size.y + item->GetCrop().y - item->GetCrop().z)*0.5f);
      pos.x = (float)round(pos.x);
      pos.y = (float)round(pos.y);
      item->pos = pos;

      OBSApiLog("FitItemsToScreen item type %d  item->GetCrop().x %f item->GetCrop().w %f   item->GetCrop().y %f  item->GetCrop().z %f", item->GetSourceType(), item->GetCrop().x, item->GetCrop().w, item->GetCrop().y, item->GetCrop().z);
      OBSApiLog("FitItemsToScreen item type %d  pos.x %f pos.y %f   size.x %f  size.y %f  itemSize.x %f itemSize.y %f", item->GetSourceType(), pos.x, pos.y, size.x , size.y, itemSize.x, itemSize.y);
      XElement *itemElement = item->GetElement();
      if (itemElement) {
         XElement *data = itemElement->GetElement(L"data");
         if (!data) {
            data = itemElement->CreateElement(TEXT("data"));
         }

         data->SetInt(L"isFullscreen", 1);
         data->SetInt(L"posType", (int)enum_PosType_fullScreen);
      }
   }
}
void CGraphics::RemoveDownLayerSource() {
   return;
   XElement*     sources = GetSourcesElement();
   //remove old 
   StringList sourceNameList;
   for (UINT i = 0; i < sources->NumElements(); i++) {
      XElement* source = sources->GetElementByID(i);
      if (scmpi(source->GetString(L"class"), CLASS_NAME_DEVICE) != 0
          && scmpi(source->GetString(L"class"), CLASS_NAME_DECKLINK) != 0
          && scmpi(source->GetString(L"class"), CLASS_NAME_TEXT) != 0
          && scmpi(source->GetString(L"class"), CLASS_NAME_BITMAP) != 0
          && scmpi(source->GetString(L"class"), CLASS_NAME_MEDIAOUTPUT) != 0
          )

      {
         sourceNameList.Add(source->GetName());
      }
   }

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      for (UINT i = 0; i < sourceNameList.Num(); i++)
         mObs->scene->RemoveImageSource(sourceNameList[i]);
      mObs->LeaveSceneMutex();
   }
}
bool CGraphics::IsInPersistent(SceneItem* item) {
   int persistent = item->GetElement()->GetInt(TEXT("persistent"), 0);
   /*for (UINT i = 0; i < mPersistentSceneItems.Num(); i++) {
   if (scmpi(item->GetName(), mPersistentSceneItems[i].itemName) == 0) {
   return true;
   }
   }*/
   return persistent == 1;
}
void CGraphics::UpdatePersistentSource() {
   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      for (UINT s01ItemIndex = 0; s01ItemIndex < mObs->scene->NumSceneItems(); s01ItemIndex++) {
         SceneItem* s01Item = mObs->scene->GetSceneItem(s01ItemIndex);
         int persistent = s01Item->GetElement()->GetInt(SOURCE_TYPE_PERSISTENT, 0);
         if (persistent == 1) {
            int order = s01Item->GetElement()->GetInt(SOURCE_PERSISTENT_ORDER, 0);
            if (ORDER_TOP == order) {
               s01Item->MoveToTop();
            } else {
               s01Item->MoveToBottom();
            }
         }
      }
      mObs->LeaveSceneMutex();
   }
}
void CGraphics::OnLButtonDown(const POINTS&  mousePos) {
}
void CGraphics::OnMouseMove(const POINTS&  mousePos) {
}
void OnLButtonUp(const POINTS&  mousePos) {

}

bool CGraphics::processPicture(void* sourceCtx) {
   STRU_OBSCONTROL_IMAGE* pImageControl = (STRU_OBSCONTROL_IMAGE*)sourceCtx;
   if (NULL == pImageControl) {
      return false;
   }

   Vect2 baseSize = mObs->GetBaseSize();

   int x = pImageControl->x*baseSize.x;
   int y = pImageControl->y*baseSize.y;
   int w = pImageControl->w*baseSize.x;
   int h = pImageControl->h*baseSize.y;


   RECT renderRect = { 0, 0, 0, 0 };
   XElement*     sources = GetSourcesElement();
   String strName;
   SceneItem *item = NULL;
   strName << DEFAULT_SOURCE_NAME << L"_BitmapImage_" << mSourceIndex++;

   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), L"BitmapImageSource");

   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data)
      data = newSourceElement->CreateElement(TEXT("data"));

   if (wcslen(pImageControl->m_strPath) == 0) {
      return false;
   }

   data->SetString(L"path", pImageControl->m_strPath);
   data->SetString(L"sourceName", strName);
   data->SetFloat(L"x", pImageControl->x);
   data->SetFloat(L"y", pImageControl->y);
   data->SetFloat(L"cx", pImageControl->w);
   data->SetFloat(L"cy", pImageControl->h);


   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      if (mObs->scene) {
         item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_PIC);
         item->MoveToTop();
         item->pos.x = x;
         item->pos.y = y;
         item->size.x = w;
         item->size.y = h;
      }
      mObs->LeaveSceneMutex();
   }

   if(pImageControl->isFullScreen && item) {
      FitItemsToScreen(item);
   }

   return true;
}

bool CGraphics::processText(void* sourceCtx) {
   STRU_OBSCONTROL_TEXT* pTextControl = (STRU_OBSCONTROL_TEXT*)sourceCtx;
   if (NULL == pTextControl) {
      return false;
   }
   bool ret = false;
   XElement*     sources = GetSourcesElement();
   String strName;
   strName << DEFAULT_SOURCE_NAME << L"_TestSource_" << mSourceIndex++;
   //get new source name 
   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), L"TextSource");

   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data) {
      data = newSourceElement->CreateElement(TEXT("data"));
   }

   if (!data) {
      return false;
   }

   data->SetString(TEXT("sourceName"), strName);
   data->SetInt(TEXT("baseSizeCX"), pTextControl->m_w);
   data->SetInt(TEXT("baseSizeCY"), pTextControl->m_h);
   data->SetString(TEXT("baseData"), String(pTextControl->m_textPic));

   data->SetString(TEXT("font"), pTextControl->m_strFont);
   data->SetInt(TEXT("color"), pTextControl->m_iColor);
   data->SetInt(TEXT("fontSize"), 72);
   data->SetInt(TEXT("textOpacity"), 100);
   data->SetInt(TEXT("scrollSpeed"), 0);
   //int fontweight = mTextCharFmt.fontWeight();
   data->SetInt(TEXT("bold"), pTextControl->m_iBold);
   data->SetInt(TEXT("italic"), pTextControl->m_iItalic);
   data->SetInt(TEXT("vertical"), 0);
   data->SetInt(TEXT("wrap"), 0);
   data->SetInt(TEXT("scrollMode"), 0);
   data->SetInt(TEXT("underline"), pTextControl->m_iUnderLine);
   data->SetInt(TEXT("pointFiltering"), 0);

   data->SetInt(TEXT("backgroundColor"), 0);
   data->SetInt(TEXT("backgroundOpacity"), 0);

   data->SetInt(TEXT("useOutline"), 0);
   data->SetInt(TEXT("outlineColor"), 0);
   data->SetFloat(TEXT("outlineSize"), 2);
   data->SetInt(TEXT("outlineOpacity"), 0);

   data->SetInt(TEXT("useTextExtents"), 0);
   data->SetInt(TEXT("extentWidth"), 32);
   data->SetInt(TEXT("extentHeight"), 32);
   data->SetInt(TEXT("align"), 0);
   data->SetString(TEXT("file"), L"");
   data->SetString(TEXT("text"), pTextControl->m_strText);
   data->SetInt(TEXT("mode"), 0);


   float ix, iy, iw, ih;
   ix = pTextControl->m_ix;
   iy = pTextControl->m_iy;
   iw = pTextControl->m_iw;
   ih = pTextControl->m_ih;

   data->SetFloat(L"x", ix);
   data->SetFloat(L"y", iy);
   data->SetFloat(L"cx", iw);
   data->SetFloat(L"cy", ih);

   Vect2 baseSize = mObs->GetBaseSize();

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      SceneItem *item = mObs->scene->InsertImageSource(0, newSourceElement, SRC_TEXT);
      item->MoveToTop();

      item->pos.x = ix*baseSize.x;
      item->pos.y = iy*baseSize.y;
      item->size.x = iw*baseSize.x;
      item->size.y = ih*baseSize.y;

      mObs->LeaveSceneMutex();
   }
   return ret;
}

bool CGraphics::modifySource(XElement *selectedElement, List<SceneItem *>& selectedSceneItems) {
   XElement* source = selectedElement->GetElement(TEXT("data"));

   bool ret = false;
   RECT renderRect = { 0, 0, 0, 0 };
   /* 文本 */
   if (scmpi(selectedElement->GetString(L"class"), CLASS_NAME_TEXT) == 0) {
      STRU_OBSCONTROL_TEXT* pTextFormat = new STRU_OBSCONTROL_TEXT;
      pTextFormat->m_iControlType = 2;       /* 2代表修改 */
      pTextFormat->m_iBold = source->GetInt(TEXT("bold"), 1);
      pTextFormat->m_iItalic = source->GetInt(TEXT("italic"), 1);
      pTextFormat->m_iColor = source->GetInt(TEXT("color"), 0xFFFFFFFF);
      pTextFormat->m_iUnderLine = source->GetInt(TEXT("underline"), 1);
      wcscpy(pTextFormat->m_strFont, source->GetString(TEXT("font")));

      /* 避免右键修改空文字时崩溃 */
      String text = source->GetString(TEXT("text"));
      if (text.IsEmpty()) {
         wcscpy(pTextFormat->m_strText, L"");
      } else {
         wcscpy(pTextFormat->m_strText, text);
      }
      wcscpy(pTextFormat->m_strSourceName, source->GetString(TEXT("sourceName"), SOURCE_NAME_DEFAULT));

      SceneItem *item = selectedSceneItems[0];
      Vect2 baseSize = mObs->GetBaseSize();
      if (item) {
         pTextFormat->m_x = item->pos.x;
         pTextFormat->m_y = item->pos.y;
         pTextFormat->m_w = item->size.x;
         pTextFormat->m_h = item->size.y;

         pTextFormat->m_ix = item->pos.x;
         pTextFormat->m_iy = item->pos.y;
         pTextFormat->m_iw = item->size.x;
         pTextFormat->m_ih = item->size.y;

         pTextFormat->m_ix /= baseSize.x;
         pTextFormat->m_iy /= baseSize.y;
         pTextFormat->m_iw /= baseSize.x;
         pTextFormat->m_ih /= baseSize.y;
      }

      ::PostMessage(mMsgWnd, WM_USER_MODIFYTEXT, 0, LPARAM(pTextFormat));
      return ret;
   }
   /* 图片 */
   else if (scmpi(selectedElement->GetString(L"class"), CLASS_NAME_BITMAP) == 0) {

      Vect2 baseSize = mObs->GetBaseSize();

      STRU_OBSCONTROL_IMAGE* pImageInfo = new STRU_OBSCONTROL_IMAGE;

      pImageInfo->m_dwType = 2;

      CTSTR filename = source->GetString(L"path", NULL);
      pImageInfo->x = source->GetFloat(L"x", 0);
      pImageInfo->y = source->GetFloat(L"y", 0);
      pImageInfo->w = source->GetFloat(L"cx", 0);
      pImageInfo->h = source->GetFloat(L"cy", 0);

      SceneItem *item = selectedSceneItems[0];

      pImageInfo->x = item->pos.x;
      pImageInfo->y = item->pos.y;
      pImageInfo->w = item->size.x;
      pImageInfo->h = item->size.y;

      pImageInfo->x /= baseSize.x;
      pImageInfo->y /= baseSize.y;
      pImageInfo->w /= baseSize.x;
      pImageInfo->h /= baseSize.y;

      if (!filename) {
         return false;
      }

      wcscpy(pImageInfo->m_strPath, filename);
      wcscpy(pImageInfo->m_strSourceName, source->GetString(TEXT("sourceName"), SOURCE_NAME_DEFAULT));

      ::PostMessage(mMsgWnd, WM_USER_MODIFYIMAGE, 0, LPARAM(pImageInfo));
   }
   /* 窗口捕获 */
   else if (scmpi(selectedElement->GetString(L"class"), CLASS_NAME_WINDOW) == 0) {
      ::PostMessage(mMsgWnd, WM_USER_MODIFYWINDOW, 0, 0);
   }
   /* DSHOW 视频设备 */
   else if (scmpi(selectedElement->GetString(L"class"), CLASS_NAME_DEVICE) == 0) {

      XElement *data = selectedElement->GetElement(L"data");
      if (!data) {
         return false;
      }

      String deviceName = data->GetString(L"deviceName", L"");
      String devicePath = data->GetString(L"deviceID", L"");
      String deviceDisplayName = data->GetString(L"displayName", L"");

      DeviceInfo deviceInfo;
      if (deviceName.Length()) {
         wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
      }

      if (devicePath.Length()) {
         wcscpy(deviceInfo.m_sDeviceID, devicePath.Array());
      }

      if (deviceDisplayName.Length()) {
         wcscpy(deviceInfo.m_sDeviceDisPlayName, deviceDisplayName.Array());
      }

      deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
      //::PostMessage(mMsgWnd, WM_USER_MODIFYCAMERA, 0, LPARAM(deviceInfo));

      PostMsg(MSG_MAINUI_MODIFY_CAMERA, &deviceInfo, sizeof(DeviceInfo));
      return ret;
   }
   /* DECKLINK 设备 */
   else if (scmpi(selectedElement->GetString(L"class"), CLASS_NAME_DECKLINK) == 0) {
      XElement *data = selectedElement->GetElement(L"data");
      if (!data) {
         return false;
      }

      String deviceName = data->GetString(L"deviceName", L"");
      String devicePath = data->GetString(L"deviceID", L"");
      String deviceDisplayName = data->GetString(L"displayName", L"");

      DeviceInfo deviceInfo;
      if (deviceName.Length()) {
         wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
      }

      if (devicePath.Length()) {
         wcscpy(deviceInfo.m_sDeviceID, devicePath.Array());
      }

      if (deviceDisplayName.Length()) {
         wcscpy(deviceInfo.m_sDeviceDisPlayName, deviceDisplayName.Array());
      }

      deviceInfo.m_sDeviceType = TYPE_DECKLINK;

      //::PostMessage(mMsgWnd, WM_USER_MODIFYCAMERA, 0, LPARAM(deviceInfo));

      PostMsg(MSG_MAINUI_MODIFY_CAMERA, &deviceInfo, sizeof(DeviceInfo));
      return ret;
   } else {
      return ret;
   }

   if (ret == false) {

   } else {
      ImageSource *imgSource = NULL;
      Vect2 multiple;

      if (mObs->bRunning && selectedSceneItems.Num()) {
         float cx = selectedElement->GetFloat(TEXT("cx"), 32.0f);
         float cy = selectedElement->GetFloat(TEXT("cy"), 32.0f);
         mObs->EnterSceneMutex();
         if (source) {
            selectedElement->SetFloat(TEXT("cx"), cx);
            selectedElement->SetFloat(TEXT("cy"), cy);
            selectedSceneItems[0]->GetSource()->UpdateSettings();
            Vect2 itemSize = selectedSceneItems[0]->GetSize();
            Vect2 sourceSize = selectedSceneItems[0]->GetSource()->GetSize();
            if (sourceSize.x*itemSize.y != sourceSize.y*itemSize.x) {
               itemSize.x = sourceSize.x*itemSize.y / sourceSize.y;
               selectedSceneItems[0]->SetSize(itemSize);
            }


         }
         mObs->LeaveSceneMutex();
      }
   }
   return ret;
}
bool CGraphics::modifyText(XElement *selectedElement, void* sourceCtx) {
   STRU_OBSCONTROL_TEXT* pTextControl = (STRU_OBSCONTROL_TEXT*)sourceCtx;
   if (NULL == pTextControl || pTextControl->m_iControlType != 2) {
      return false;
   }

   float ix, iy, iw, ih;
   ix = pTextControl->m_ix;
   iy = pTextControl->m_iy;
   iw = pTextControl->m_iw;
   ih = pTextControl->m_ih;


   XElement* data = selectedElement->GetElement(TEXT("data"));
   //unsigned char *baseData = (unsigned char *)data->GetHex(TEXT("baseData"), 0);
   //if (baseData) {
   //   delete []baseData;
   //   baseData = NULL;
   //}

   data->SetInt(TEXT("baseSizeCX"), pTextControl->m_w);
   data->SetInt(TEXT("baseSizeCY"), pTextControl->m_h);
   data->SetString(TEXT("baseData"),pTextControl->m_textPic);

   data->SetString(TEXT("font"), pTextControl->m_strFont);
   data->SetInt(TEXT("color"), pTextControl->m_iColor);
   data->SetInt(TEXT("fontSize"), 72);
   data->SetInt(TEXT("textOpacity"), 100);
   data->SetInt(TEXT("scrollSpeed"), 0);
   //int fontweight = mTextCharFmt.fontWeight();
   data->SetInt(TEXT("bold"), pTextControl->m_iBold);
   data->SetInt(TEXT("italic"), pTextControl->m_iItalic);
   data->SetInt(TEXT("vertical"), 0);
   data->SetInt(TEXT("wrap"), 0);
   data->SetInt(TEXT("scrollMode"), 0);
   data->SetInt(TEXT("underline"), pTextControl->m_iUnderLine);
   data->SetInt(TEXT("pointFiltering"), 0);

   data->SetInt(TEXT("backgroundColor"), 0);
   data->SetInt(TEXT("backgroundOpacity"), 0);

   data->SetInt(TEXT("useOutline"), 0);
   data->SetInt(TEXT("outlineColor"), 0);
   data->SetFloat(TEXT("outlineSize"), 2);
   data->SetInt(TEXT("outlineOpacity"), 0);

   data->SetInt(TEXT("useTextExtents"), 0);
   data->SetInt(TEXT("extentWidth"), 32);
   data->SetInt(TEXT("extentHeight"), 32);
   data->SetInt(TEXT("align"), 0);
   data->SetString(TEXT("file"), L"");
   data->SetString(TEXT("text"), pTextControl->m_strText);
   data->SetInt(TEXT("mode"), 0);


   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);
   LOGFONT lf;
   zero(&lf, sizeof(lf));
   String strFont = pTextControl->m_strFont;
   UINT fontSize = data->GetInt(TEXT("fontSize"), 10);
   String strOutputText = pTextControl->m_strText;
   BOOL bUseOutline = data->GetInt(TEXT("useOutline"), 0);
   float outlineSize = data->GetFloat(TEXT("outlineSize"), 2);

   Vect2 baseSize = mObs->GetBaseSize();

   if (mObs->bRunning && items.Num()) {
      mObs->EnterSceneMutex();

      SceneItem *item = items[0];
      if (item) {

         item->pos.x = ix*baseSize.x;
         item->pos.y = iy*baseSize.y;
         item->size.x = iw*baseSize.x;
         item->size.y = ih*baseSize.y;
         selectedElement->SetInt(L"x", item->pos.x);
         selectedElement->SetInt(L"y", item->pos.y);
         selectedElement->SetInt(L"cx", item->GetSize().x);
         selectedElement->SetInt(L"cy", item->GetSize().y);
         item->GetSource()->UpdateSettings();
         mObs->LeaveSceneMutex();
      }
   }
   return true;
}

bool CGraphics::modifyPicture(XElement *selectedElement, void* sourceCtx) {
   STRU_OBSCONTROL_IMAGE* pImageControl = (STRU_OBSCONTROL_IMAGE*)sourceCtx;
   if (NULL == selectedElement || NULL == pImageControl || pImageControl->m_dwType != 2) {
      return false;
   }
   float ix, iy, iw, ih;
   ix = pImageControl->x;
   iy = pImageControl->y;
   iw = pImageControl->w;
   ih = pImageControl->h;

   XElement* data = selectedElement->GetElement(TEXT("data"));
   if (data) {
      data->SetString(L"path", pImageControl->m_strPath);
   }



   List<SceneItem*> items;
   mObs->scene->GetSelectedItems(items);

   data->SetString(L"path", pImageControl->m_strPath);
   data->SetFloat(L"x", ix);
   data->SetFloat(L"y", iy);
   data->SetFloat(L"cx", iw);
   data->SetFloat(L"cy", ih);
   SceneItem *item = items[0];
   if (!item) {
      return false;
   }

   Vect2 baseSize = mObs->GetBaseSize();

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      item->pos.x = ix*baseSize.x;
      item->pos.y = iy*baseSize.y;
      item->size.x = iw*baseSize.x;
      item->size.y = ih*baseSize.y;
      item->GetSource()->UpdateSettings();
      mObs->LeaveSceneMutex();
   }

   return true;
}

bool CGraphics::modifyWindow(XElement *selectedElement, SceneItem * item, void* sourceCtx) {
   STRU_OBSCONTROL_WINDOWSRC* pWindowSrc = (STRU_OBSCONTROL_WINDOWSRC*)sourceCtx;
   if (!item || NULL == pWindowSrc) {
      return false;
   }

   XElement* data = selectedElement->GetElement(TEXT("data"));
   data->SetHexListPtr(L"winInfo", (unsigned char *)&pWindowSrc->m_windowInfo, sizeof(VHD_WindowInfo));

   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      if (data) {
         item->GetSource()->UpdateSettings();
      }
      mObs->LeaveSceneMutex();
   }
   return true;
}

void *CGraphics::processMediaOut(void* sourceCtx) {
   bool ret = false;
   RECT renderRect = { 0, 0, 0, 0 };
   XElement*     sources = GetSourcesElement();
   String strName;
   strName << DEFAULT_SOURCE_NAME << L"_MediaOut_" << mSourceIndex++;

   XElement *newSourceElement = sources->InsertElement(0, strName);
   newSourceElement->SetInt(TEXT("render"), 1);
   newSourceElement->SetString(TEXT("class"), CLASS_NAME_MEDIAOUTPUT);
   XElement *data = newSourceElement->GetElement(TEXT("data"));
   if (!data) {
      data = newSourceElement->CreateElement(TEXT("data"));
   }

   SceneItem *item = NULL;
   if (mObs->bRunning) {
      mObs->EnterSceneMutex();
      if (mMediaSourceIndex < 0) {
         mMediaSourceIndex = 0;
      }
      item = mObs->scene->InsertImageSource(mMediaSourceIndex, newSourceElement, SRC_MEDIA_OUT);
      if (item) {
         this->mMediaSourceItem = item;
      }

      mObs->LeaveSceneMutex();
   }

   return item;
}
bool CGraphics::SetSourceVisible(wchar_t *sourceName, bool isVisible, bool isWait) {
   if (mObs) {
      return mObs->SetSourceVisible(sourceName, isVisible, isWait);
   }
   return false;
}
bool CGraphics::WaitSetSourceVisible() {
   if (mObs) {
      return mObs->WaitSetSourceVisible();
   }
   return false;
}
unsigned char * CGraphics::OBSMemoryCreate(int size) {
   return new unsigned char[size];
}
void *CGraphics::GetMediaSource() {
   return mMediaSourceItem;
}

void CGraphics::OBSMemoryFree(void *p) {
   if (p) {
      delete p;
   }
}
int CGraphics::GetGraphicsDeviceInfoCount() {
   if (!mObs) {
      return -1;
   }

   XElement*     sources = GetSourcesElement();
   if (!sources) {
      return -1;
   }

   int deviceCount = 0;
   DWORD elementCount = sources->NumElements();
   for (int i = 0; i < elementCount; i++) {
      XElement* source = sources->GetElementByID(i);
      if (!source) {
         break;
      }

      String classType = source->GetString(L"class", L"");
      if (classType == CLASS_NAME_DECKLINK || classType == CLASS_NAME_DEVICE) {
         deviceCount++;
      }
   }

   return deviceCount;
}
bool CGraphics::GetGraphicsDeviceInfo
(DeviceInfo &deviceInfo,
DataSourcePosType &posType,
int count) {
   if (!mObs) {
      return false;
   }

   XElement *sources = GetSourcesElement();
   if (!sources) {
      return -1;
   }

   int deviceCount = 0;
   DWORD elementCount = sources->NumElements();
   for (int i = 0; i < elementCount; i++) {
      XElement* source = sources->GetElementByID(i);
      if (!source) {
         break;
      }

      String classType = source->GetString(L"class", L"");
      if (classType == CLASS_NAME_DECKLINK || classType == CLASS_NAME_DEVICE) {
         if (deviceCount == count) {
            XElement *data = source->GetElement(L"data");
            if (!data) {
               return false;
            }

            String deviceName = data->GetString(L"deviceName", L"");
            if (deviceName.Length()) {
               wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
            }

            String deviceId = data->GetString(L"deviceID", L"");
            if (deviceId.Length()) {
               wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
            }

            String displayName = data->GetString(L"displayName", L"");
            if (displayName.Length()) {
               wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
            }

            if (classType == CLASS_NAME_DECKLINK) {
               deviceInfo.m_sDeviceType = TYPE_DECKLINK;
            } else if (classType == CLASS_NAME_DEVICE) {
               deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
            }

            posType = (DataSourcePosType)data->GetInt(L"posType", enum_PosType_custom);

            return true;
         }
         deviceCount++;
      }
   }

   return false;
}
void CGraphics::DeviceRecheck(DeviceList &deviceList) {
   if (!mObs) {
      return;
   }

   if (!IsHasSource(SRC_DSHOW_DEVICE)) {
      return;
   }


   XElement *sources = GetSourcesElement();
   if (!sources) {
      return;
   }

   mObs->EnterSceneMutex();
   if (mObs->scene) {
      for (int i = 0; i < mObs->scene->NumSceneItems(); i++) {
         SceneItem *item = mObs->scene->GetSceneItem(i);
         if (item) {
            XElement *sourceElement = item->GetElement();
            if (sourceElement) {
               String classType = sourceElement->GetString(L"class", L"");
               if (classType == CLASS_NAME_DECKLINK || classType == CLASS_NAME_DEVICE) {
                  XElement *data = sourceElement->GetElement(L"data");
                  ImageSource *source = item->GetSource();
                  if (data&&source) {
                     DeviceInfo deviceInfo;
                     String deviceName = data->GetString(L"deviceName", L"");
                     if (deviceName.Length()) {
                        wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
                     }

                     String deviceId = data->GetString(L"deviceID", L"");
                     if (deviceId.Length()) {
                        wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
                     }

                     String displayName = data->GetString(L"displayName", L"");
                     if (displayName.Length()) {
                        wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
                     }

                     if (classType == CLASS_NAME_DECKLINK) {
                        deviceInfo.m_sDeviceType = TYPE_DECKLINK;
                     }
                     else if (classType == CLASS_NAME_DEVICE) {
                        deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
                     }

                     auto itor = deviceList.
                        find(deviceInfo);
                     if (itor == deviceList.end()) {
                        source->SetDeviceExist(false);
                     }
                     else {
                        source->SetDeviceExist(true);
                     }
                  }
               }
            }
         }
      }
   }

   mObs->LeaveSceneMutex();
}

bool CGraphics::GetGraphicsDeviceInfoExist(DeviceInfo &deviceInfoPar, bool &isFullScreen) {
   if (!mObs) {
      return false;
   }

   XElement *sources = GetSourcesElement();
   if (!sources) {
      return -1;
   }

   DWORD elementCount = sources->NumElements();
   for (int i = 0; i < elementCount; i++) {
      XElement* source = sources->GetElementByID(i);
      if (!source) {
         break;
      }

      String classType = source->GetString(L"class", L"");
      if (classType == CLASS_NAME_DECKLINK || classType == CLASS_NAME_DEVICE) {
         DeviceInfo deviceInfo;
         XElement *data = source->GetElement(L"data");
         if (!data) {
            return false;
         }

         String deviceName = data->GetString(L"deviceName", L"");
         if (deviceName.Length()) {
            wcscpy(deviceInfo.m_sDeviceName, deviceName.Array());
         }

         String deviceId = data->GetString(L"deviceID", L"");
         if (deviceId.Length()) {
            wcscpy(deviceInfo.m_sDeviceID, deviceId.Array());
         }

         String displayName = data->GetString(L"displayName", L"");
         if (displayName.Length()) {
            wcscpy(deviceInfo.m_sDeviceDisPlayName, displayName.Array());
         }

         if (classType == CLASS_NAME_DECKLINK) {
            deviceInfo.m_sDeviceType = TYPE_DECKLINK;
         } else if (classType == CLASS_NAME_DEVICE) {
            deviceInfo.m_sDeviceType = TYPE_DSHOW_VIDEO;
         }

         if (deviceInfoPar == deviceInfo) {

            isFullScreen = data->GetInt(L"isFullScreen", 0) != 0;
            return true;
         }
      }
   }

   return false;

}

void CGraphics::SetCreateTextTextureMemory(FuncCreateTextureByText c, FuncFreeMemory f) {
   OBSAPI_SetCreateTextTextureMemory(c, f);
}
void CGraphics::SetCreateTextureFromFileHook(FuncCheckFileTexture fc) {
   OBSAPI_SetCreateTextureFromFileHook(fc);
}
void CGraphics::SetMediaCore(IMediaCore *mediaCore) {
   mMediaCore=mediaCore;
}

void CGraphics::SetSceneInfo(VideoSceneType sceneType) {
   if(!mMediaCore) {
      return ;
   }
   mMediaCore->SetSceneInfo(sceneType)  ;
}

void CGraphics::GetSceneInfo(VideoSceneType& sceneType) {
   if(!mMediaCore) {
      return ;
   }
   mMediaCore->GetSceneInfo(sceneType) ;
}

void CGraphics::DesktopEnhanceSwitchOn(bool open) {

}

void CGraphics::AdjusteSceneParam() {
   /* 存在设备或者插播则认为是自然场景 */
    if (IsHasSource(SRC_MONITOR)) {
        SetSceneInfo(SceneType_Artificial);
    } 
    else {                                      //360P
        SetSceneInfo(SceneType_Natural);
    }
}
HWND CGraphics::GetDeviceRenderHwnd(void *item)
{
   if(!item) {
      return NULL;
   }

   SceneItem *sceneItem = (SceneItem *)item;
   XElement *sourceElement = sceneItem->mpElement;
   if(!sourceElement) {
      return NULL;
   }
   
   XElement *data = sourceElement->GetElement(L"data");
   if(!data) {
      return NULL;
   }

   return (HWND)data->GetInt(L"renderHwnd",NULL);
}

void CGraphics::DoHideLogo(bool bHide) {
   if(mObs) {
      mObs->DoHideLogo(bHide);
   }
}
void CGraphics::ReinitMedia(){
   mMediaSourcePosSizeInit = false;
}

__declspec(dllexport) IGraphics* CreateGraphics(HWND msgWnd, HWND renderFrame, SIZE baseSize, int scaleType, const wchar_t *logPath) {
   init();
   return     new CGraphics(msgWnd, renderFrame, baseSize, scaleType, logPath);
}

__declspec(dllexport) void DestoryGraphics(IGraphics** graphics) {
   if (*graphics) {
      delete *graphics;
      *graphics = NULL;
   }

   destory();
}
