#ifndef __GRAHICS_INCLUDE__
#define __GRAHICS_INCLUDE__  
#include <stdint.h>
#include "live_open_define.h"
#include "IGraphics.h"
class SceneItem;

class __declspec(dllexport) CGraphics : public IGraphics {
public:
   CGraphics(HWND msgWnd, HWND renderFrame, SIZE baseSize,int scaleType, const wchar_t *logPath = NULL);
   virtual ~CGraphics();   
   virtual void SetDataReceiver(IDataReceiver *);
   virtual bool AddPersistentSource(const wchar_t* path, const RECT& rect, const int & index);
   virtual bool AddNewSource(SOURCE_TYPE captureType, void* sourceCtx, void **pSourceItem = NULL);
   virtual void ClearSource(void *sourceItem = NULL);   
   virtual void ClearSourceByType(SOURCE_TYPE sourceType);
   virtual int ProcessSource(const int & opType);
   void ClearAllSource();
   virtual bool IsHasMonitorSource();
   virtual bool IsHasWindowsSource();
   virtual bool IsHasNoPersistentSource();   
   virtual int  GetCurrentItemType();
   virtual bool IsModifiable();
   virtual bool Preview(const SIZE &, const SIZE &, bool);
   virtual bool Start();
   virtual void Resize(const RECT& client, bool bRedrawRenderFrame);
   virtual void StopPreview(bool isRestart);
   virtual void Shutdown();
   virtual bool Destory();
   virtual  SIZE GetResolution();
   virtual  SIZE GetBaseSize();
   virtual int GetFps();
   virtual void SetSceneInfo(VideoSceneType sceneType);
   virtual void GetSceneInfo(VideoSceneType& sceneType);
   virtual void DesktopEnhanceSwitchOn(bool open);
   virtual int GetRenderState(DeviceInfo);
   virtual void SetFps(int );
   virtual void OnMouseEvent(const UINT& mouseEvent, const POINTS& mousePos);
   void OnMouseEventRealization(const UINT& mouseEvent, const POINTS& mousePos);
   virtual void SetSyncWithEncoder(const bool & isWillsync);
   virtual unsigned char** LockCurrentFramePic(QWORD &t,unsigned long long currentTime = 0);
   virtual void UnlockCurrentFramePic();
   virtual void SetSourceDeleteHook(IGraphicDeleteSourceHook funcDeleteSourceHook, void *param);
   virtual void SourceRefit(void *item);
   virtual bool SetSourceVisible(wchar_t *sourceName,bool isVisible,bool isWait);
   virtual bool WaitSetSourceVisible();
   virtual void UpdataSourceElement();
   virtual void SetPostMsgHook(IPOSTCRMessageFunc funcPostMsg)
   {
      this->funcPostCRMsg=funcPostMsg;      
      mObs->SetPostMsgHook(this->funcPostCRMsg);
   }
   virtual void *GetMediaSource();
   unsigned char * OBSMemoryCreate(int size);
   void OBSMemoryFree(void *);   
   virtual int GetGraphicsDeviceInfoCount();
   virtual bool GetGraphicsDeviceInfo(DeviceInfo &,DataSourcePosType &posType,int count);
   virtual bool GetGraphicsDeviceInfoExist(DeviceInfo &deviceInfo,bool &);
   virtual bool ModifyDeviceSource(DeviceInfo&srcDevice,DeviceInfo&desDevice,DataSourcePosType posType);
   virtual bool ModifyBeautyDeviceSource(DeviceInfo&srcDevice, HWND m_PriviewRenderHwnd, int hide_preview, int beautyLevel);
   virtual void DeviceRecheck(DeviceList &);
   virtual void SetCreateTextTextureMemory(FuncCreateTextureByText,FuncFreeMemory);
   virtual void SetCreateTextureFromFileHook(FuncCheckFileTexture);
   virtual void SetMediaCore(IMediaCore *);
   virtual void DoHideLogo(bool bHide);
   virtual void ReinitMedia();

private:
   char* WStr2CStr(const wchar_t* WStr);
   void LoadDefaultSetting();
   void ReloadSceneConfig();
   XElement* GetSourcesElement();
   XElement* GetSceneElement();
   bool processDevice(DeviceInfo deviceInfo,DataSourcePosType m_PosType,int,HWND renderHwnd, HWND privew_renderHwnd = nullptr,int isHideSouceItem = 0);
   bool deleteDevice(void* sourceCtx);
   bool processScreenArea(void* sourceCtx);
   bool processMonitor(void* sourceCtx);
   bool processWindow(void* sourceCtx);
   bool processPicture(void* sourceCtx);
   bool processText(void* sourceCtx);
   void *processMediaOut(void* sourceCtx);

   //适应到全屏
   void FitItemsToScreen(SceneItem *item);
   //适应到右下角
   void FitItemsToScreenRightDown(SceneItem *item,int,int);
   //自适应到自定义位置
   bool FitItemsToScreenCustom(SceneItem *item);
   
   
   void FitItemToScreenByPosType(SceneItem *item,DataSourcePosType,int,int);
   void RemoveDownLayerSource();
   void OnLButtonDown(const POINTS& mousePos);
   void OnMouseMove(const POINTS& mousePos);
   void OnLButtonUp(const POINTS& mousePos);
   bool IsInPersistent(SceneItem*);
   void UpdatePersistentSource();

   bool isSelectAreaSource();
   bool modifySource(XElement *selectedElement, List<SceneItem *>& selectedSceneItems);
   bool modifyText(XElement *selectedElement, void* sourceCtx);
   bool modifyPicture(XElement *selectedElement, void* sourceCtx);
   SceneItem *GetSceneItem(SOURCE_TYPE type);
   bool modifyWindow(XElement *selectedElement, SceneItem * item, void* sourceCtx);
   long PostMsg(DWORD adwMessageID, void * apData, DWORD adwLen)
   {
      if(!funcPostCRMsg)
      {
         return -1;
      }
      return funcPostCRMsg(adwMessageID,apData,adwLen);
   }
   
   virtual bool ReloadDevice(DeviceInfo &deviceInfo);
   
   virtual bool IsHasSource(SOURCE_TYPE );
   ImageSource *GetDeviceSource(DeviceInfo &);
   SceneItem *GetDeviceItem(DeviceInfo &);

   void ModifyAreaShared(int left,int top,int right,int bottom);

   virtual HWND GetDeviceRenderHwnd(void *);

   //场景化参数调整（编码底层感知）
   void AdjusteSceneParam();
private:   
   void OnMouseEventRealizationDown(const UINT& mouseEvent, const POINTS& imousePos);
   void OnMouseEventRealizationUp(const UINT& mouseEvent, const POINTS& imousePos);
   void OnMouseEventRealizationMove(const UINT& mouseEvent, const POINTS& imousePos);     
   void OnMouseEventRealizationMoveWithoutMouseDown(const UINT& mouseEvent, const POINTS& imousePos); 
   void OnMouseEventRealizationMoveWithMouseDown(const UINT& mouseEvent, const POINTS& imousePos) ;

   void AnalysisModifyType(ItemModifyType modifyType,Vect2 &mousePos);
   void SyncCursor();
   void RecordDataSourceInfo();
   void ModifyTypeConvert();
   void ItemScale(SceneItem *&);   
   void ModifyTypeItemMove(List<SceneItem*> &items,bool bControlDown,Vect2 &totalAdjust,Vect2 &snapSize,Vect2 &baseRenderSize);
private:
   Vect2 mLastSize;
   OBS *mObs = NULL;
   HWND mMsgWnd;
   HWND mRenderWnd;
   SIZE mBaseSize;
   SIZE mOutputSize;
   static int mSourceIndex;

   Vect2 mMediaSourcePos;
   Vect2 mMediaSourceSize;
   bool mMediaSourcePosSizeInit;

   int mFps;
   
   SceneItem *mMediaSourceItem;
   
   int mMediaSourceIndex;
   IGraphicDeleteSourceHook funcDeleteSourceHook = NULL;
   IPOSTCRMessageFunc       funcPostCRMsg = NULL;
   void *mDeleteSourceHookParam = nullptr;
   bool mIsAutoMode;
   //SceneType mSceneType;
   //int mMaxBitrate;
   IMediaCore *mMediaCore = NULL;
};
#endif

