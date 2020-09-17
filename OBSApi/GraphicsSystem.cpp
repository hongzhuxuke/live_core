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

#pragma warning(disable: 4530)

#define MANUAL_BUFFER_SIZE 64

GraphicsSystem *GS = nullptr;
static FuncCreateTextureByText G_FuncCreateTexture=NULL;
static FuncFreeMemory G_FuncFreeMemory=NULL;
static FuncCheckFileTexture G_FuncCheckFileTexture= NULL;

GraphicsSystem::GraphicsSystem()
: curMatrix(0) {
   MatrixStack << Matrix().SetIdentity();
}

GraphicsSystem::~GraphicsSystem() {
}

void GraphicsSystem::Init() {
}

Shader* GraphicsSystem::CreateVertexShaderFromFile(CTSTR lpFileName) {
   XFile ShaderFile;

   String fullPathFilename;

   if ((lpFileName[0] != '.' && lpFileName[0] != '/' && lpFileName[0] != '\\') && !(lpFileName[0] && lpFileName[1] == ':'))
      fullPathFilename << OBSAPI_GetAppPath() << L"\\" << lpFileName;
   else
      fullPathFilename << lpFileName;

   if (!ShaderFile.Open(fullPathFilename, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
      CrashError(TEXT("CreateVertexShaderFromFile: Couldn't open %s: %d"), lpFileName, GetLastError());

   String strShader;
   ShaderFile.ReadFileToString(strShader);
   if (strShader.IsEmpty())
      CrashError(TEXT("CreateVertexShaderFromFile: Couldn't read %s: %d"), lpFileName, GetLastError());

   return CreateVertexShader(strShader, lpFileName);
}

Shader* GraphicsSystem::CreatePixelShaderFromFile(CTSTR lpFileName) {
   XFile ShaderFile;

   String fullPathFilename;

   if ((lpFileName[0] != '.' && lpFileName[0] != '/' && lpFileName[0] != '\\') && !(lpFileName[0] && lpFileName[1] == ':'))
      fullPathFilename << OBSAPI_GetAppPath() << L"\\" << lpFileName;
   else
      fullPathFilename << lpFileName;

   if (!ShaderFile.Open(fullPathFilename, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
      CrashError(TEXT("CreatePixelShaderFromFile: Couldn't open %s: %d"), lpFileName, GetLastError());

   String strShader;
   ShaderFile.ReadFileToString(strShader);
   if (strShader.IsEmpty())
      CrashError(TEXT("CreatePixelShaderFromFile: Couldn't read %s: %d"), lpFileName, GetLastError());

   return CreatePixelShader(strShader, lpFileName);
}

template class BASE_EXPORT FutureShader<CreateVertexShaderFromBlob>;
template class BASE_EXPORT FutureShader<CreatePixelShaderFromBlob>;

FuturePixelShader GraphicsSystem::CreatePixelShaderFromFileAsync(CTSTR fileName) {
   using namespace std;
   using Context = FutureShaderContainer::FutureShaderContext;

   String fullPathFilename;

   if ((fileName[0] != '.' && fileName[0] != '/' && fileName[0] != '\\') && !(fileName[0] && fileName[1] == ':'))
      fullPathFilename << OBSAPI_GetAppPath() << L"\\" << fileName;
   else
      fullPathFilename << fileName;

   wstring const fn = fullPathFilename.Array();
   auto &cs = futureShaders.contexts;

   ScopedLock m(futureShaders.lock);

   bool initialized = cs.find(fn) != end(cs);

   Context &c = cs[fn];

   if (!initialized) {
      c.readyEvent.reset(CreateEvent(nullptr, true, false, nullptr));
      c.fileName = fn;
      c.thread.reset(OSCreateThread(static_cast<XTHREAD>([](void *arg) -> DWORD {
         Context &c = *(Context*)arg;
         XFile ShaderFile;

         if (!ShaderFile.Open(c.fileName.c_str(), XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING))
            return 1;

         String strShader;
         ShaderFile.ReadFileToString(strShader);

         c.fileData = strShader.Array();

         GS->CreatePixelShaderBlob(c.shaderData, strShader.Array(), c.fileName.c_str());

         SetEvent(c.readyEvent.get());
         return 0;
      }), &c));
   }

   if (c.thread && WaitForSingleObject(c.readyEvent.get(), 0) == WAIT_OBJECT_0)
      c.thread.reset();

   return FuturePixelShader(c.readyEvent.get(), c.shaderData, c.fileData, c.fileName);
}


void GraphicsSystem::DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2) {
   assert(texture);

   DrawSpriteEx(texture, color, x, y, x2, y2, 0.0f, 0.0f, 1.0f, 1.0f);
}


/////////////////////////////////
//manual rendering functions

void GraphicsSystem::StartVertexBuffer() {
   bNormalSet = FALSE;
   bColorSet = FALSE;
   TexCoordSetList.Clear();

   vbd = new VBData;
   dwCurPointVert = 0;
   dwCurTexVert = 0;
   dwCurColorVert = 0;
   dwCurNormVert = 0;
}

VertexBuffer *GraphicsSystem::SaveVertexBuffer() {
   if (vbd->VertList.Num()) {
      VertexBuffer *buffer;

      buffer = CreateVertexBuffer(vbd);

      vbd = NULL;

      return buffer;
   } else {
      delete vbd;
      vbd = NULL;

      return NULL;
   }
}

void GraphicsSystem::Vertex(float x, float y, float z) {
   Vect v(x, y, z);
   Vertex(v);
}

void GraphicsSystem::Vertex(const Vect &v) {
   if (!bNormalSet && vbd->NormalList.Num())
      Normal(vbd->NormalList[vbd->NormalList.Num() - 1]);
   bNormalSet = 0;

   /////////////////
   if (!bColorSet && vbd->ColorList.Num())
      Color(vbd->ColorList[vbd->ColorList.Num() - 1]);
   bColorSet = 0;

   /////////////////
   for (DWORD i = 0; i < TexCoordSetList.Num(); i++) {
      if (!TexCoordSetList[i] && vbd->UVList[i].Num()) {
         List<UVCoord> &UVList = vbd->UVList[i];
         TexCoord(UVCoord(UVList[UVList.Num() - 1]), i);
      }
      TexCoordSetList.Clear(i);
   }

   vbd->VertList << v;

   ++dwCurPointVert;
}

void GraphicsSystem::Normal(float x, float y, float z) {
   Vect v(x, y, z);
   Normal(v);
}

void GraphicsSystem::Normal(const Vect &v) {
   vbd->NormalList << v;

   ++dwCurNormVert;

   bNormalSet = TRUE;
}

void GraphicsSystem::Color(DWORD dwRGBA) {
   vbd->ColorList << dwRGBA;

   ++dwCurColorVert;

   bColorSet = TRUE;
}

void GraphicsSystem::Color(const Color4 &v) {
   Color(Vect4_to_RGBA(v));
}

void GraphicsSystem::TexCoord(float u, float v, int idTexture) {
   UVCoord uv(u, v);
   TexCoord(uv, idTexture);
}

void GraphicsSystem::TexCoord(const UVCoord &uv, int idTexture) {
   if (vbd->UVList.Num() < (DWORD)(idTexture + 1)) {
      vbd->UVList.SetSize(idTexture + 1);
      TexCoordSetList.SetSize(idTexture + 1);
   }

   vbd->UVList[idTexture] << uv;

   ++dwCurTexVert;

   TexCoordSetList.Set(idTexture);
}
inline void  GraphicsSystem::MatrixPush() {
   MatrixStack << Matrix(MatrixStack[curMatrix]);
   ++curMatrix;
}

inline void  GraphicsSystem::MatrixPop() {
   MatrixStack.Remove(curMatrix);
   --curMatrix;

   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixSet(const Matrix &m) {
   MatrixStack[curMatrix] = m;
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixMultiply(const Matrix &m) {
   MatrixStack[curMatrix] *= m;
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(float x, float y, float z, float a) {
   MatrixStack[curMatrix] *= Quat(AxisAngle(x, y, z, a));
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(const AxisAngle &aa) {
   MatrixStack[curMatrix] *= Quat(aa);
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixRotate(const Quat &q) {
   MatrixStack[curMatrix] *= q;
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranslate(float x, float y) {
   MatrixStack[curMatrix] *= Vect(x, y, 0.0f);
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranslate(const Vect2 &pos) {
   MatrixStack[curMatrix] *= Vect(pos);
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixScale(const Vect2 &scale) {
   MatrixStack[curMatrix].Scale(scale.x, scale.y, 1.0f);
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixScale(float x, float y) {
   MatrixStack[curMatrix].Scale(x, y, 1.0f);
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixTranspose() {
   MatrixStack[curMatrix].Transpose();
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixIdentity() {
   MatrixStack[curMatrix].SetIdentity();
   ResetViewMatrix();
}

inline void  GraphicsSystem::MatrixGet(Vect &v, Quat &q) {
   q.CreateFromMatrix(MatrixStack[curMatrix]);
   v = MatrixStack[curMatrix].T;
}

inline void  GraphicsSystem::MatrixGet(Matrix &m) {
   m = MatrixStack[curMatrix];
}

void  MatrixPush() { 
	if (NULL != GS)
		GS->MatrixPush();
}
void  MatrixPop() {
	if (NULL != GS)
		GS->MatrixPop();
}
void  MatrixSet(const Matrix &m) {
	if (NULL != GS)
		GS->MatrixSet(m);
}
void  MatrixGet(Matrix &m) {
	if (NULL != GS)
		GS->MatrixGet(m);
}
void  MatrixMultiply(const Matrix &m) { 
	if (NULL != GS)
		GS->MatrixMultiply(m);
}
void  MatrixRotate(float x, float y, float z, float a) {
	if (NULL != GS)
		GS->MatrixRotate(x, y, z, a);
}  //axis angle
void  MatrixRotate(const AxisAngle &aa) {
	if (NULL != GS)
		GS->MatrixRotate(aa);
}
void  MatrixRotate(const Quat &q) {
	if (NULL != GS)
		GS->MatrixRotate(q);
}
void  MatrixTranslate(float x, float y) { 
	if (NULL != GS)
		GS->MatrixTranslate(x, y);
}
void  MatrixTranslate(const Vect2 &pos) { 
	if (NULL != GS)
		GS->MatrixTranslate(pos);
}
void  MatrixScale(const Vect2 &scale) { 
	if (NULL != GS)
		GS->MatrixScale(scale);
}
void  MatrixScale(float x, float y) { 
	if (NULL != GS)
		GS->MatrixScale(x, y);
}
void  MatrixTranspose() {
	if (NULL != GS)
		GS->MatrixTranspose();
}
void  MatrixIdentity() { 
	if (NULL != GS)
		GS->MatrixIdentity();
}
Texture* CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bGenMipMaps, BOOL bStatic) {
   if (NULL!=GS)
		return GS->CreateTexture(width, height, colorFormat, lpData, bGenMipMaps, bStatic);
   else
	   return NULL;
}

Texture* CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps){
	if (NULL != GS) {
		wchar_t des[1024] = { 0 };
		if (G_FuncCheckFileTexture) {
			if (G_FuncCheckFileTexture((void *)lpFile, (void *)des)) {
				return GS->CreateTextureFromFile(des, bBuildMipMaps);
			}
		}
		return GS->CreateTextureFromFile(lpFile, bBuildMipMaps);
	}
   else
	   return NULL;
}

Texture* CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps){
	if (NULL != GS)
		return GS->CreateRenderTarget(width, height, colorFormat, bGenMipMaps);
   else
	   return NULL;
}
Texture* CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat){
	if (NULL != GS)
		return GS->CreateReadTexture(width, height, colorFormat);
   else
	   return NULL;
}
Texture* CreateGDITexture(unsigned int width, unsigned int height){
	if (NULL != GS)
		return GS->CreateGDITexture(width, height);
   else
	   return NULL;
}
Texture* CreateTextureFromText(wchar_t * text)
{
   if(!G_FuncCreateTexture)
   {
      return NULL;
   }
   if(!G_FuncFreeMemory)
   {
      return NULL;
   }

   int w=0;
   int h=0;
   unsigned char *buf=NULL;
   if(!G_FuncCreateTexture(text,buf,w,h))
   {
      return NULL;
   }

   
   //Texture *tex = CreateTexture(w,h,GS_RGBA,buf,FALSE,FALSE);
   Texture *tex=CreateTexture(w,h,GS_RGBA,NULL,FALSE,FALSE);
   if(tex)
   {
      BYTE * lpData=NULL;
      UINT pitch=0;
      if(tex->Map(lpData,pitch))
      {

         for(int i=0;i<h;i++)
         {
            memcpy(lpData+i*pitch, buf+(int)(i*w*4),(int)w*4);
            //memset(lpData+i*pitch,0xF0,pitch);
         }

         tex->Unmap();
      }
      
   }

   G_FuncFreeMemory(buf);
   return tex;
}

Texture* CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle){
	if (NULL != GS)
		return GS->CreateTextureFromSharedHandle(width, height, handle);
   else
	   return NULL;
}
Texture* CreateSharedTexture(unsigned int width, unsigned int height){
	if (NULL != GS)
		return GS->CreateSharedTexture(width, height);
   else
	   return NULL;
}
SamplerState* CreateSamplerState(SamplerInfo &info){
	if (NULL != GS)
		return GS->CreateSamplerState(info);
   else
	   return NULL;
}

Shader* CreateVertexShader(CTSTR lpShader, CTSTR lpFileName) {
	if (NULL != GS)
		return GS->CreateVertexShader(lpShader, lpFileName);
   else
	   return NULL;
}
Shader* CreatePixelShader(CTSTR lpShader, CTSTR lpFileName)  {
	if (NULL != GS)
		return GS->CreatePixelShader(lpShader, lpFileName);
   else
	   return NULL;
}
Shader* CreateVertexShaderFromFile(CTSTR lpFileName) {
	if (NULL != GS)
		return GS->CreateVertexShaderFromFile(lpFileName);
   else
	   return NULL;
}
Shader* CreatePixelShaderFromFile(CTSTR lpFileName){
	if (NULL != GS)
		return GS->CreatePixelShaderFromFile(lpFileName);
   else
	   return NULL;
}
FuturePixelShader CreatePixelShaderFromFileAsync(CTSTR fileName){
	if (NULL != GS)
		return GS->CreatePixelShaderFromFileAsync(fileName);
	else
	{
		FuturePixelShader temp;
		return temp;
   }
	   
}
VertexBuffer* CreateVertexBuffer(VBData *vbData, BOOL bStatic){
	if (NULL != GS)
		return GS->CreateVertexBuffer(vbData, bStatic);
   else
	   return NULL;
}
void  LoadVertexBuffer(VertexBuffer* vb){
	if (NULL!=GS)
		GS->LoadVertexBuffer(vb);
}
void  LoadTexture(Texture *texture, UINT idTexture){
	if (NULL!=GS)
		GS->LoadTexture(texture, idTexture);
}
void  LoadSamplerState(SamplerState *sampler, UINT idSampler) {
	if (NULL!=GS)
		GS->LoadSamplerState(sampler, idSampler);
}
void  LoadVertexShader(Shader *vShader){
	if (NULL!=GS)
		GS->LoadVertexShader(vShader);
}
void  LoadPixelShader(Shader *pShader){
	if (NULL!=GS)
		GS->LoadPixelShader(pShader);
}
Shader* GetCurrentPixelShader(){
	if (NULL!=GS)
		return GS->GetCurrentPixelShader();
}
Shader* GetCurrentVertexShader(){
	if (NULL!=GS)
		return GS->GetCurrentVertexShader();
}
void  SetRenderTarget(Texture *texture) {  
	if (NULL!=GS)
		GS->SetRenderTarget(texture);
}
void  Draw(GSDrawMode drawMode, DWORD StartVert, DWORD nVerts){
	if (NULL!=GS)
		GS->Draw(drawMode, StartVert, nVerts);
}
void  EnableBlending(BOOL bEnabled){
	if (NULL!=GS)
		GS->EnableBlending(bEnabled);
}
void  BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor){
	if (NULL!=GS)
		GS->BlendFunction(srcFactor, destFactor, fFactor);
}
void  ClearColorBuffer(DWORD color){
	if (NULL!=GS)
		GS->ClearColorBuffer(color);
}
void SetCropping(float top,float left,float bottom,float right){
	if (NULL!=GS)
		GS->SetCropping(top,left,bottom,right);
}
void  StartVertexBuffer(){
	if (NULL!=GS)
		GS->StartVertexBuffer();
}
VertexBuffer* SaveVertexBuffer(){
	if (NULL!=GS)
		return GS->SaveVertexBuffer();
}
void  Vertex(float x, float y, float z){
	if (NULL!=GS)
		GS->Vertex(x, y, z);
}
void  Vertex(const Vect &v){
	if (NULL!=GS)
		GS->Vertex(v);
}
void  Vertex(const Vect2 &v2){
	if (NULL!=GS)
		GS->Vertex(v2);
}
void  Normal(float x, float y, float z){
	if (NULL!=GS)
		GS->Normal(x, y, z);
}
void  Normal(const Vect &v){
	if (NULL!=GS)
		GS->Normal(v);
}
void  Color(DWORD dwRGBA){
	if (NULL != GS)
		GS->Color(dwRGBA);
}
void  Color(const Color4 &v){
	if (NULL != GS)
		GS->Color(v);
}
void  Color(float R, float G, float B, float A){
   if (NULL != GS)
   {
	   Color4 rgba(R, G, B, A);
	   GS->Color(rgba);
   }
	   
}
void  TexCoord(float u, float v, int idTexture){
	if (NULL != GS)
		GS->TexCoord(u, v, idTexture);
}
void  TexCoord(const UVCoord &uv, int idTexture){
	if (NULL != GS)
		GS->TexCoord(uv, idTexture);
}
void  Ortho(float left, float right, float top, float bottom, float znear, float zfar){
	if (NULL != GS)
		GS->Ortho(left, right, top, bottom, znear, zfar);
}
void  Frustum(float left, float right, float top, float bottom, float znear, float zfar){
	if (NULL != GS)
		GS->Frustum(left, right, top, bottom, znear, zfar);
}
void  SetViewport(float x, float y, float width, float height) {
	if (NULL != GS)
		GS->SetViewport(x, y, width, height);
}
void  SetScissorRect(XRect *pRect){
	if (NULL != GS)
		GS->SetScissorRect(pRect);
}
void DrawSprite(Texture *texture, DWORD color, float x, float y){
   if (!texture) 
      return;
   if (NULL != GS)
	   GS->DrawSprite(texture, color, x, y, (float)texture->Width(), (float)texture->Height());
}
void DrawSprite(Texture *texture, DWORD color, float x, float y, float x2, float y2){
	if (NULL != GS)
		GS->DrawSprite(texture, color, x, y, x2, y2);
}
void DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2){
	if (NULL != GS)
		GS->DrawSpriteEx(texture, color, x, y, x2, y2, u, v, u2, v2);
}
void DrawBox(const Vect2 &upperLeft, const Vect2 &size){
	if (NULL != GS)
		GS->DrawBox(upperLeft, size);
}
void DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees){
	if (NULL != GS)
		GS->DrawSpriteExRotate(texture, color, x, y, x2, y2, degrees, u, v, u2, v2, texDegrees);
}
Shader* CreateVertexShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName){
	if (NULL != GS)
		return GS->CreateVertexShaderFromBlob(blob, lpShader, lpFileName);
	else
		return NULL;
}
Shader* CreatePixelShaderFromBlob(ShaderBlob const& blob, CTSTR lpShader, CTSTR lpFileName){
	if (NULL != GS)
		return GS->CreatePixelShaderFromBlob(blob, lpShader, lpFileName);
	else
		return NULL;
}
LPVOID GetD3D(){
	if (NULL != GS)
		return GS->GetDevice();
   else
	   return NULL;
}
LPVOID GetD3DContent(){
	if (NULL != GS)
		return GS->GetContent();
   else
	   return NULL;
}
void OBSGraphics_Register(GraphicsSystem *graphicsSystem){
		GS = graphicsSystem;
}
void OBSGraphics_UnRegister(){
   if(GS){
      delete GS;
      GS=NULL;
   }
}
GraphicsSystem *OBSGraphics_GetGraphicsSystem(){
	if (NULL != GS)
		return GS; 
	else
		return NULL;
}
long OBSGraphics_GetDeviceRemovedReason(){
	if (NULL != GS)
		return GS->GetDeviceRemovedReason();
	else
		return NULL;
}
void OBSGraphics_Init(){
	if (NULL != GS)
		GS->Init();
}
void OBSGraphics_ResizeView(){
	if (NULL != GS)
		GS->ResizeView();
}
void OBSGraphics_Present(){
	if (NULL != GS)
		GS->Present();
}
void OBSGraphics_CopyTexture(Texture * texDest,Texture * texSrc){
	if (NULL != GS)
		GS->CopyTexture(texDest, texSrc);
}
void OBSGraphics_UnloadAllData(){
   if(GS){
      GS->UnloadAllData();
   }
}
void OBSGraphics_Flush(){
   if(GS){
      GS->Flush();
   }
}
static HINSTANCE G_dllHandle=NULL;
LPCWSTR libFileName[]={
   L"GraphicsPlugins\\GraphicsD3D10.dll",
   L"GraphicsPlugins\\GraphicsD3D11.dll",
   L"GraphicsPlugins\\GraphicsD3D9.dll",
   L"GraphicsPlugins\\GraphicsOpenGL.dll"
};
UINT libFileNameCount=sizeof(libFileName)/sizeof(LPCWSTR);
static HANDLE G_GraphicMutex=NULL;


bool OBSGraphicsLoadLibrary(LPCWSTR filename, HWND mRenderWidgetHwnd, int renderWidth, int renderHeight, ConfigFile *configFile)
{
   //InitXT(NULL, L"FastAlloc", L"OBSGraphics");
   HINSTANCE dllInterface = LoadLibrary(filename);
   if(dllInterface==NULL)
   {
      return false;
   }

   void *GraphicsInit = GetProcAddress(dllInterface, "GraphicsInit");
   if(GraphicsInit==NULL)
   {
		Log(TEXT("GraphicsInit IS null AND FreeLibrary"));
      FreeLibrary(dllInterface);
      dllInterface=NULL;
      return false;
   }
	Log(TEXT("GraphicsInit)(mRenderWidgetHwnd, renderWidth, renderHeight, configFile)"));
   bool ret=((bool(*)(HWND, int, int, ConfigFile *))GraphicsInit)(mRenderWidgetHwnd, renderWidth, renderHeight, configFile);
	Log(TEXT("GraphicsInit)(mRenderWidgetHwnd, renderWidth, renderHeight, configFile) END"));
   if(ret)
   {
		Log(TEXT("OBSGraphicsLoadLibrary G_dllHandle=dllInterface"));
      G_dllHandle=dllInterface;
   }
   else
   {
		Log(TEXT(" OBSGraphicsLoadLibrary FreeLibrary"));
      FreeLibrary(dllInterface);
      dllInterface=NULL;
   }

   return ret;
}
bool OBSGraphics_Load(HWND mRenderWidgetHwnd,int renderWidth,int renderHeight,ConfigFile *configFile)
{
   Log(TEXT("OBSGraphics_Load"));
   bool ret=false;
   if(G_dllHandle!=NULL)
   {
      OBSGraphics_Unload();
   }

   for(UINT i=0;i<libFileNameCount;i++)
   {
		Log(TEXT("OBSGraphics_Load %d"), i);
      if (OBSGraphicsLoadLibrary(libFileName[i], mRenderWidgetHwnd, renderWidth, renderHeight, configFile))
      {
         ret=true;
         break;
      }
   }
   
   G_GraphicMutex=OSCreateMutex();

   return ret;
}
void OBSGraphics_Unload()
{
   Log(TEXT("OBSGraphics_Unload"));
   if(G_dllHandle)
   {
      void *GraphicsDestory = GetProcAddress(G_dllHandle, "GraphicsDestory");
      if(GraphicsDestory)
      {
         ((void(*)())GraphicsDestory)();
      }
      FreeLibrary(G_dllHandle);
      G_dllHandle=NULL;
   }
   if(G_GraphicMutex)
   {
      OSCloseMutex(G_GraphicMutex);
      G_GraphicMutex=NULL;
   }
   G_FuncCreateTexture=NULL;
   G_FuncFreeMemory=NULL;
}
void OBSGraphics_Lock()
{
   OSEnterMutex(G_GraphicMutex);
}
void OBSGraphics_UnLock()
{
   OSLeaveMutex(G_GraphicMutex);
}
void OBSAPI_SetCreateTextTextureMemory(FuncCreateTextureByText c,FuncFreeMemory f)
{
   G_FuncCreateTexture=c;
   G_FuncFreeMemory=f;
}
void OBSAPI_SetCreateTextureFromFileHook(FuncCheckFileTexture f) {
   G_FuncCheckFileTexture = f;
}
OBSRenderView *OBSGraphics_CreateRenderTargetView(HWND hwnd,int cx,int cy){
	if (NULL != GS)
		return GS->CreateRenderTargetView(hwnd, cx, cy);
	else
		return NULL;
}
void OBSGraphics_DestoryRenderTargetView(OBSRenderView *view){
	if (NULL != GS)
		GS->DestoryRenderTargetView(view);
}
void OBSGraphics_SetRenderTargetView(OBSRenderView *view){
	if (NULL != GS)
		GS->SetRenderTargetView(view);
}


