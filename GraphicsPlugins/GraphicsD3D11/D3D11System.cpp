/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/

#include <DXGI.h>
#include <dxgi1_2.h>
#include "D3D11System.h"

ConfigFile   *GlobalConfig=NULL;
//#define SafeRelease(punk)  \
//if ((punk) != NULL)  \
//{ (punk)->Release(); (punk) = NULL; }
class OBS;
extern ConfigFile   *GlobalConfig;
D3D11System::D3D11System(HWND renderHwnd,int renderWidth,int renderHeight,bool &ok)
   :GraphicsSystem()
{
    OBSApiLog("D3D11System::D3D11System w:%d,h:%d",renderWidth,renderHeight);
    ok=false;
    mRenderWidth=renderWidth;
    mRenderHeight=renderHeight;

    HRESULT err;

#ifdef USE_DXGI1_2
        REFIID iidVal = OSGetVersion() >= 8 ? __uuidof(IDXGIFactory2) : __uuidof(IDXGIFactory1);
#else
        REFIID iidVal = __uuidof(IDXGIFactory1);
#endif
        
   if(FAILED(err = CreateDXGIFactory1(iidVal, (void**)factory.Assign()))){
      return ;
   }


    DXGI_SWAP_CHAIN_DESC swapDesc;
    zero(&swapDesc, sizeof(swapDesc));
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.Width = renderWidth;
    swapDesc.BufferDesc.Height = renderHeight;


    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = renderHwnd;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.Windowed = TRUE;

    bDisableCompatibilityMode = 1;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if(GlobalConfig->GetInt(TEXT("General"), TEXT("UseDebugD3D")))
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;

    //驱动类型
    D3D_DRIVER_TYPE driverTypes[] =
    {
       D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP,
       D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_SOFTWARE
    };

    unsigned int totalDriverTypes = ARRAYSIZE(driverTypes);

    //级别
    D3D_FEATURE_LEVEL featureLevels[] =
    {
       D3D_FEATURE_LEVEL_11_0,
       D3D_FEATURE_LEVEL_10_1,
       D3D_FEATURE_LEVEL_10_0
    };


    unsigned int driver = 0;
    unsigned int totalFeatureLevels = ARRAYSIZE(featureLevels);

    for (driver = 0; driver < totalDriverTypes; ++driver) {
       //创建设备和交换链
       err = D3D11CreateDeviceAndSwapChain(0,
                                              driverTypes[driver],
                                              0,
                                              createFlags,
                                              featureLevels,
                                              totalFeatureLevels,
                                              D3D11_SDK_VERSION,
                                              &swapDesc,
                                              &swap,
                                              &d3d,
                                              &featureLevel,
                                              &d3dContext);

       if (SUCCEEDED(err)) 
       {
          OBSApiLog("D3D11CreateDeviceAndSwapChain Successed! driverTypes[%d] featureLevel[%d]",
             driverTypes[driver],
             featureLevel
          );
          driverType = driverTypes[driver];
          break;
       }
       else
       {
          OBSApiLog("D3D11CreateDeviceAndSwapChain Failed! driverTypes[%d] featureLevel[%d]",
             driverTypes[driver],
             featureLevel
          );
       }
    }






    //------------------------------------------------------------------

    D3D11_DEPTH_STENCIL_DESC depthDesc;
    zero(&depthDesc, sizeof(depthDesc));
    depthDesc.DepthEnable = FALSE;

    err = d3d->CreateDepthStencilState(&depthDesc, &depthState);
    if(FAILED(err)){
        OBSApiLog("D3D11System CreateDepthStencilState Failed![%d]",err);
        return ;
    }
    d3dContext->OMSetDepthStencilState(depthState, 0);

    //------------------------------------------------------------------

    D3D11_RASTERIZER_DESC rasterizerDesc;
    zero(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;

    err = d3d->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    if(FAILED(err)){
        OBSApiLog("D3D11System CreateRasterizerState Failed![%d]",err);
        return ;
    }
    d3dContext->RSSetState(rasterizerState);

    //------------------------------------------------------------------

    rasterizerDesc.ScissorEnable = TRUE;

    err = d3d->CreateRasterizerState(&rasterizerDesc, &scissorState);
    if(FAILED(err)){
        OBSApiLog("D3D11System CreateRasterizerState Failed![%d]",err);
        return ;
    }
    //------------------------------------------------------------------

    ID3D11Texture2D *backBuffer = NULL;
    err = swap->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);
    if(FAILED(err)){
      OBSApiLog("D3D11System swap->GetBuffer Failed![%d]",err);
      return ;
    }
    err = d3d->CreateRenderTargetView(backBuffer, NULL, &swapRenderView);
    if(FAILED(err)){
      OBSApiLog("D3D11System CreateRenderTargetView Failed![%d]",err);
      return ;
    }
    
    backBuffer->Release();

    //------------------------------------------------------------------

    D3D11_BLEND_DESC disabledBlendDesc;
    zero(&disabledBlendDesc, sizeof(disabledBlendDesc));
    for(int i=0; i<8; i++)
    {
       disabledBlendDesc.RenderTarget[i].BlendEnable = TRUE;
       disabledBlendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
       disabledBlendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
       disabledBlendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
       disabledBlendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
       disabledBlendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
       disabledBlendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
       disabledBlendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
    }


    err = d3d->CreateBlendState(&disabledBlendDesc, &disabledBlend);
    if(FAILED(err)){
         OBSApiLog("D3D11System CreateBlendState Failed![%d]",err);   
         return ;
    }
    this->BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, 1.0f);
    bBlendingEnabled = true;
    ok=true;
}

D3D11System::~D3D11System()
{
   if (spriteVertexBuffer) {
      delete spriteVertexBuffer;
      spriteVertexBuffer = nullptr;
    }
   if (boxVertexBuffer) {
      delete boxVertexBuffer;
      boxVertexBuffer = nullptr;
   }

    for(UINT i=0; i<blends.Num(); i++)
        SafeRelease(blends[i].blendState);

    SafeRelease(scissorState);
    SafeRelease(rasterizerState);
    SafeRelease(depthState);
    SafeRelease(disabledBlend);
    SafeRelease(swapRenderView);
    SafeRelease(swap);
    SafeRelease(d3d);
    SafeRelease(d3dContext);
}

void D3D11System::UnloadAllData()
{
    LoadVertexShader(NULL);
    LoadPixelShader(NULL);
    LoadVertexBuffer(NULL);
    for(UINT i=0; i<8; i++)
    {
        LoadSamplerState(NULL, i);
        LoadTexture(NULL, i);
    }

    UINT zeroVal = 0;
    LPVOID nullBuff[8];
    float bla[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    zero(nullBuff, sizeof(nullBuff));

    d3dContext->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)nullBuff);
    d3dContext->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)nullBuff);
    d3dContext->OMSetDepthStencilState(NULL, 0);
    d3dContext->PSSetSamplers(0, 1, (ID3D11SamplerState**)nullBuff);
    d3dContext->OMSetBlendState(NULL, bla, 0xFFFFFFFF);
    d3dContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)nullBuff, NULL);
    d3dContext->IASetVertexBuffers(0, 8, (ID3D11Buffer**)nullBuff, &zeroVal, &zeroVal);
    d3dContext->PSSetShaderResources(0, 8, (ID3D11ShaderResourceView**)nullBuff);
    d3dContext->IASetInputLayout(NULL);
    d3dContext->PSSetShader(NULL,NULL,0);
    d3dContext->VSSetShader(NULL, NULL, 0);
    d3dContext->RSSetState(NULL);
    d3dContext->RSSetScissorRects(0, NULL);
}
LPVOID D3D11System::GetContent() {
   return (LPVOID)d3dContext;
}
LPVOID D3D11System::GetDevice()
{
    return (LPVOID)d3d;
}
void D3D11System::Present()
{
   if(swap)
   {
      swap->Present(0, 0);
   }
}
void D3D11System::Flush()
{
   if(d3d)
   {
      d3dContext->Flush();
   }
}
long D3D11System::GetDeviceRemovedReason()
{
   if(d3d)
   {
      return (long)d3d->GetDeviceRemovedReason();
   }
   
   return 0;
}
void D3D11System::CopyTexture(Texture *texDest, Texture *texSrc)
{
   if(texDest!=NULL&&texSrc!=NULL)
   {
      D3D11Texture *d3d11TexDest = static_cast<D3D11Texture*>(texDest);
      D3D11Texture *d3d11TexSrc = static_cast<D3D11Texture*>(texSrc);
      if (d3dContext != NULL&&d3d11TexDest != NULL&&d3d11TexSrc != NULL)
      {
         d3dContext->CopyResource(d3d11TexDest->texture, d3d11TexSrc->texture);
      }
   }
}

void D3D11System::Init()
{
    VBData *data = new VBData;
    data->UVList.SetSize(1);

    data->VertList.SetSize(4);
    data->UVList[0].SetSize(4);

    spriteVertexBuffer = CreateVertexBuffer(data, FALSE);

    //------------------------------------------------------------------

    data = new VBData;
    data->VertList.SetSize(5);
    boxVertexBuffer = CreateVertexBuffer(data, FALSE);

    //------------------------------------------------------------------

    GraphicsSystem::Init();
}


////////////////////////////
//Texture Functions
Texture* D3D11System::CreateTextureFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle)
{
    return D3D11Texture::CreateFromSharedHandle(width, height, handle);
}

Texture* D3D11System::CreateSharedTexture(unsigned int width, unsigned int height)
{
    return D3D11Texture::CreateShared(width, height);
}
Texture* D3D11System::CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat)
{
   return D3D11Texture::CreateReadTexture(width,height,colorFormat);
}

Texture* D3D11System::CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bBuildMipMaps, BOOL bStatic)
{
    return D3D11Texture::CreateTexture(width, height, colorFormat, lpData, bBuildMipMaps, bStatic);
}

Texture* D3D11System::CreateTextureFromFile(CTSTR lpFile, BOOL bBuildMipMaps)
{
    return D3D11Texture::CreateFromFile(lpFile, bBuildMipMaps);
}

Texture* D3D11System::CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps)
{
    return D3D11Texture::CreateRenderTarget(width, height, colorFormat, bGenMipMaps);
}

Texture* D3D11System::CreateGDITexture(unsigned int width, unsigned int height)
{
    return D3D11Texture::CreateGDITexture(width, height);
}

bool D3D11System::GetTextureFileInfo(CTSTR lpFile, TextureInfo &info)
{
    D3DX11_IMAGE_INFO ii;
    if(SUCCEEDED(D3DX11GetImageInfoFromFile(lpFile, NULL, &ii, NULL)))
    {
        info.width = ii.Width;
        info.height = ii.Height;
        switch(ii.Format)
        {
            case DXGI_FORMAT_A8_UNORM:              info.type = GS_ALPHA;       break;
            case DXGI_FORMAT_R8_UNORM:              info.type = GS_GRAYSCALE;   break;
            case DXGI_FORMAT_B8G8R8X8_UNORM:        info.type = GS_BGR;         break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:        info.type = GS_BGRA;        break;
            case DXGI_FORMAT_R8G8B8A8_UNORM:        info.type = GS_RGBA;        break;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:    info.type = GS_RGBA16F;     break;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:    info.type = GS_RGBA32F;     break;
            case DXGI_FORMAT_BC1_UNORM:             info.type = GS_DXT1;        break;
            case DXGI_FORMAT_BC2_UNORM:             info.type = GS_DXT3;        break;
            case DXGI_FORMAT_BC3_UNORM:             info.type = GS_DXT5;        break;
            default:
                info.type = GS_UNKNOWNFORMAT;
        }

        return true;
    }

    return false;
}

SamplerState* D3D11System::CreateSamplerState(SamplerInfo &info)
{
    return D3D11SamplerState::CreateSamplerState(info);
}


UINT D3D11System::GetNumOutputs()
{
    UINT count = 0;

    IDXGIDevice *device;
    if(SUCCEEDED(d3d->QueryInterface(__uuidof(IDXGIDevice), (void**)&device)))
    {
        IDXGIAdapter *adapter;
        if(SUCCEEDED(device->GetAdapter(&adapter)))
        {
            IDXGIOutput *outputInterface;

            while(SUCCEEDED(adapter->EnumOutputs(count, &outputInterface)))
            {
                count++;
                outputInterface->Release();
            }

            adapter->Release();
        }

        device->Release();
    }

    return count;
}

OutputDuplicator *D3D11System::CreateOutputDuplicator(UINT outputID)
{
    D3D11OutputDuplicator *duplicator = new D3D11OutputDuplicator;
    if(duplicator->Init(outputID))
        return duplicator;

    delete duplicator;
    return NULL;
}


////////////////////////////
//Shader Functions
Shader* D3D11System::CreateVertexShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName)
{
    return D3D11VertexShader::CreateVertexShaderFromBlob(blob, lpShader, lpFileName);
}

Shader* D3D11System::CreatePixelShaderFromBlob(ShaderBlob const &blob, CTSTR lpShader, CTSTR lpFileName)
{
    return D3D11PixelShader::CreatePixelShaderFromBlob(blob, lpShader, lpFileName);
}

Shader* D3D11System::CreateVertexShader(CTSTR lpShader, CTSTR lpFileName)
{
    return D3D11VertexShader::CreateVertexShader(lpShader, lpFileName);
}

Shader* D3D11System::CreatePixelShader(CTSTR lpShader, CTSTR lpFileName)
{
    return D3D11PixelShader::CreatePixelShader(lpShader, lpFileName);
}

void D3D11System::CreateVertexShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName)
{
    D3D11VertexShader::CreateVertexShaderBlob(blob, lpShader, lpFileName);
}

void D3D11System::CreatePixelShaderBlob(ShaderBlob &blob, CTSTR lpShader, CTSTR lpFileName)
{
    D3D11PixelShader::CreatePixelShaderBlob(blob, lpShader, lpFileName);
}


////////////////////////////
//Vertex Buffer Functions
VertexBuffer* D3D11System::CreateVertexBuffer(VBData *vbData, BOOL bStatic)
{
    return D3D11VertexBuffer::CreateVertexBuffer(vbData, bStatic);
}


////////////////////////////
//Main Rendering Functions
void D3D11System::LoadVertexBuffer(VertexBuffer* vb)
{
    if(vb != curVertexBuffer)
    {
        D3D11VertexBuffer *d3dVB = static_cast<D3D11VertexBuffer*>(vb);
        if(curVertexShader)
        {
            List<ID3D11Buffer*> buffers;
            List<UINT> strides;
            List<UINT> offsets;

            if(d3dVB)
                d3dVB->MakeBufferList(curVertexShader, buffers, strides);
            else
            {
                UINT nBuffersToClear = curVertexShader->NumBuffersExpected();
                buffers.SetSize(nBuffersToClear);
                strides.SetSize(nBuffersToClear);
            }
            offsets.SetSize(buffers.Num());
            d3dContext->IASetVertexBuffers(0, buffers.Num(), buffers.Array(), strides.Array(), offsets.Array());
        }
        curVertexBuffer = d3dVB;
    }
}

void D3D11System::LoadTexture(Texture *texture, UINT idTexture)
{
    if(curTextures[idTexture] != texture)
    {
        D3D11Texture *d3dTex = static_cast<D3D11Texture*>(texture);
        if(d3dTex)
           d3dContext->PSSetShaderResources(idTexture, 1, &d3dTex->resource);
        else
        {
            LPVOID lpNull = NULL;
            d3dContext->PSSetShaderResources(idTexture, 1, (ID3D11ShaderResourceView**)&lpNull);
        }

        curTextures[idTexture] = d3dTex;
    }
}

void D3D11System::LoadSamplerState(SamplerState *sampler, UINT idSampler)
{
    if(curSamplers[idSampler] != sampler)
    {
        D3D11SamplerState *d3dSampler = static_cast<D3D11SamplerState*>(sampler);
        if(d3dSampler)
           d3dContext->PSSetSamplers(idSampler, 1, &d3dSampler->state);
        else
        {
            LPVOID lpNull = NULL;
            d3dContext->PSSetSamplers(idSampler, 1, (ID3D11SamplerState**)&lpNull);
        }
        curSamplers[idSampler] = d3dSampler;
    }
}

void D3D11System::LoadVertexShader(Shader *vShader)
{
    if(curVertexShader != vShader)
    {
        if(vShader)
        {
            D3D11VertexBuffer *lastVertexBuffer = curVertexBuffer;
            if(curVertexBuffer)
                LoadVertexBuffer(NULL);

            D3D11VertexShader *shader = static_cast<D3D11VertexShader*>(vShader);

            d3dContext->VSSetShader(shader->vertexShader,NULL,0);
            d3dContext->IASetInputLayout(shader->inputLayout);
            d3dContext->VSSetConstantBuffers(0, 1, &shader->constantBuffer);

            if(lastVertexBuffer)
                LoadVertexBuffer(lastVertexBuffer);
        }
        else
        {
            LPVOID lpNULL = NULL;

            d3dContext->VSSetShader(NULL,NULL,0);
            d3dContext->VSSetConstantBuffers(0, 1, (ID3D11Buffer**)&lpNULL);
        }

        curVertexShader = static_cast<D3D11VertexShader*>(vShader);
    }
}

void D3D11System::LoadPixelShader(Shader *pShader)
{
    if(curPixelShader != pShader)
    {
        if(pShader)
        {
            D3D11PixelShader *shader = static_cast<D3D11PixelShader*>(pShader);

            d3dContext->PSSetShader(shader->pixelShader,NULL,0);
            d3dContext->PSSetConstantBuffers(0, 1, &shader->constantBuffer);

            for(UINT i=0; i<shader->Samplers.Num(); i++)
                LoadSamplerState(shader->Samplers[i].sampler, i);
        }
        else
        {
            LPVOID lpNULL = NULL;

            d3dContext->PSSetShader(NULL,NULL,0);
            d3dContext->PSSetConstantBuffers(0, 1, (ID3D11Buffer**)&lpNULL);

            for(UINT i=0; i<8; i++)
                curSamplers[i] = NULL;

            ID3D11SamplerState *states[8];
            zero(states, sizeof(states));
            d3dContext->PSSetSamplers(0, 8, states);
        }

        curPixelShader = static_cast<D3D11PixelShader*>(pShader);
    }
}

Shader* D3D11System::GetCurrentPixelShader()
{
    return curPixelShader;
}

Shader* D3D11System::GetCurrentVertexShader()
{
    return curVertexShader;
}

void D3D11System::SetRenderTarget(Texture *texture)
{
    if(curRenderTarget != texture)
    {
        if(texture)
        {
            ID3D11RenderTargetView *view = static_cast<D3D11Texture*>(texture)->renderTarget;
            if(!view)
            {
                AppWarning(TEXT("tried to set a texture that wasn't a render target as a render target"));
                return;
            }

            d3dContext->OMSetRenderTargets(1, &view, NULL);
        }
        else
           d3dContext->OMSetRenderTargets(1, &swapRenderView, NULL);

        curRenderTarget = static_cast<D3D11Texture*>(texture);
    }
}

const D3D11_PRIMITIVE_TOPOLOGY topologies[] = {D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};

void D3D11System::Draw(GSDrawMode drawMode, DWORD startVert, DWORD nVerts)
{
    if(!curVertexBuffer)
    {
        AppWarning(TEXT("Tried to call draw without setting a vertex buffer"));
        return;
    }
    if(!curVertexShader)
    {
        AppWarning(TEXT("Tried to call draw without setting a vertex shader"));
        return;
    }
    if(!curPixelShader)
    {
        AppWarning(TEXT("Tried to call draw without setting a pixel shader"));
        return;
    }
    curVertexShader->SetMatrix(curVertexShader->GetViewProj(), curViewProjMatrix);
    curVertexShader->UpdateParams();
    curPixelShader->UpdateParams();
    D3D11_PRIMITIVE_TOPOLOGY newTopology = topologies[(int)drawMode];
    if(newTopology != curTopology)
    {
       d3dContext->IASetPrimitiveTopology(newTopology);
        curTopology = newTopology;
    }
    if(nVerts == 0)
        nVerts = static_cast<D3D11VertexBuffer*>(curVertexBuffer)->numVerts;

    d3dContext->Draw(nVerts, startVert);
}


////////////////////////////
//Drawing mode functions

const D3D11_BLEND blendConvert[] = {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR};

void  D3D11System::EnableBlending(BOOL bEnable)
{
    if(bBlendingEnabled != bEnable)
    {
        if(bBlendingEnabled = bEnable)
           d3dContext->OMSetBlendState(curBlendState, curBlendFactor, 0xFFFFFFFF);
        else
           d3dContext->OMSetBlendState(disabledBlend, curBlendFactor, 0xFFFFFFFF);
    }
}

void D3D11System::BlendFunction(GSBlendType srcFactor, GSBlendType destFactor, float fFactor)
{
    bool bUseFactor = (srcFactor >= GS_BLEND_FACTOR || destFactor >= GS_BLEND_FACTOR);

    if(bUseFactor)
        curBlendFactor[0] = curBlendFactor[1] = curBlendFactor[2] = curBlendFactor[3] = fFactor;

    for(UINT i=0; i<blends.Num(); i++)
    {
        SavedBlendState &blendInfo = blends[i];
        if(blendInfo.srcFactor == srcFactor && blendInfo.destFactor == destFactor)
        {
            if(bUseFactor || curBlendState != blendInfo.blendState)
            {
               d3dContext->OMSetBlendState(blendInfo.blendState, curBlendFactor, 0xFFFFFFFF);
                curBlendState = blendInfo.blendState;
            }
            return;
        }
    }

    //blend wasn't found, create a new one and save it for later
    D3D11_BLEND_DESC blendDesc;
    zero(&blendDesc, sizeof(blendDesc));
    for(int i=0; i<8; i++)
    {
        blendDesc.RenderTarget[i].BlendEnable           = TRUE;
        blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[i].SrcBlend = blendConvert[srcFactor];
        blendDesc.RenderTarget[i].DestBlend = blendConvert[destFactor];
    }


    SavedBlendState *savedBlend = blends.CreateNew();
    savedBlend->destFactor      = destFactor;
    savedBlend->srcFactor       = srcFactor;

    if(FAILED(d3d->CreateBlendState(&blendDesc, &savedBlend->blendState)))
        CrashError(TEXT("Could not set blend state"));

    if(bBlendingEnabled)
       d3dContext->OMSetBlendState(savedBlend->blendState, curBlendFactor, 0xFFFFFFFF);

    curBlendState = savedBlend->blendState;
}

void D3D11System::ClearColorBuffer(DWORD color)
{
    Color4 floatColor;
    floatColor.MakeFromRGBA(color);

    D3D11Texture *d3dTex = static_cast<D3D11Texture*>(curRenderTarget);
    if(d3dTex)
       d3dContext->ClearRenderTargetView(d3dTex->renderTarget, floatColor.ptr);
    else
       d3dContext->ClearRenderTargetView(swapRenderView, floatColor.ptr);
}


////////////////////////////
//Other Functions
void D3D11System::Ortho(float left, float right, float top, float bottom, float znear, float zfar)
{
    Matrix4x4Ortho(curProjMatrix, left, right, top, bottom, znear, zfar);
    ResetViewMatrix();
}

void D3D11System::Frustum(float left, float right, float top, float bottom, float znear, float zfar)
{
    Matrix4x4Frustum(curProjMatrix, left, right, top, bottom, znear, zfar);
    ResetViewMatrix();
}


void D3D11System::SetViewport(float x, float y, float width, float height)
{
    D3D11_VIEWPORT vp;
    zero(&vp, sizeof(vp));
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = INT(x);
    vp.TopLeftY = INT(y);
    vp.Width    = UINT(width);
    vp.Height   = UINT(height);
    d3dContext->RSSetViewports(1, &vp);
}

void D3D11System::SetScissorRect(XRect *pRect)
{
    if(pRect)
    {
       d3dContext->RSSetState(scissorState);
        D3D11_RECT rc = {pRect->x, pRect->y, pRect->x+pRect->cx, pRect->y+pRect->cy};
        d3dContext->RSSetScissorRects(1, &rc);
    }
    else
    {
       d3dContext->RSSetState(rasterizerState);
       d3dContext->RSSetScissorRects(0, NULL);
    }
}

//(jim) hey, I changed this to x, y, x2, y2
void D3D11System::SetCropping(float left, float top, float right, float bottom)
{
    curCropping[0] = left;
    curCropping[1] = top;
    curCropping[2] = right;
    curCropping[3] = bottom;
}

void D3D11System::DrawSpriteEx(Texture *texture, DWORD color, float x, float y, float x2, float y2, float u, float v, float u2, float v2)
{
    DrawSpriteExRotate(texture, color, x, y, x2, y2, 0.0f, u, v, u2, v2, 0.0f);
}

void D3D11System::DrawSpriteExRotate(Texture *texture, DWORD color, float x, float y, float x2, float y2, float degrees, float u, float v, float u2, float v2, float texDegrees)
{
    if(!curPixelShader)
        return; 

    if(!texture)
    {
        AppWarning(TEXT("Trying to draw a sprite with a NULL texture"));
        return;
    }

    HANDLE hColor = curPixelShader->GetParameterByName(TEXT("outputColor"));

    if(hColor)
        curPixelShader->SetColor(hColor, color);

    //------------------------------
    // crop positional values

    Vect2 totalSize = Vect2(x2-x, y2-y);
    Vect2 invMult   = Vect2(totalSize.x < 0.0f ? -1.0f : 1.0f, totalSize.y < 0.0f ? -1.0f : 1.0f);
    totalSize.Abs();

    if(y2-y < 0) {
        float tempFloat = curCropping[1];
        curCropping[1] = curCropping[3];
        curCropping[3] = tempFloat;
    }

    if(x2-x < 0) {
        float tempFloat = curCropping[0];
        curCropping[0] = curCropping[2];
        curCropping[2] = tempFloat;
    }

    bool bFlipX = (x2 - x) < 0.0f;
    bool bFlipY = (y2 - y) < 0.0f;

    x  += curCropping[0] * invMult.x;
    y  += curCropping[1] * invMult.y;
    x2 -= curCropping[2] * invMult.x;
    y2 -= curCropping[3] * invMult.y;

    bool cropXUnder = bFlipX ? ((x - x2) < 0.0f) : ((x2 - x) < 0.0f);
    bool cropYUnder = bFlipY ? ((y - y2) < 0.0f) : ((y2 - y) < 0.0f);

    // cropped out completely (eg mouse cursor texture)
    if (cropXUnder || cropYUnder)
        return;

    //------------------------------
    // crop texture coordinate values

    float cropMult[4];
    cropMult[0] = curCropping[0]/totalSize.x;
    cropMult[1] = curCropping[1]/totalSize.y;
    cropMult[2] = curCropping[2]/totalSize.x;
    cropMult[3] = curCropping[3]/totalSize.y;

    Vect2 totalUVSize = Vect2(u2-u, v2-v);
    u  += cropMult[0] * totalUVSize.x;
    v  += cropMult[1] * totalUVSize.y;
    u2 -= cropMult[2] * totalUVSize.x;
    v2 -= cropMult[3] * totalUVSize.y;

    //------------------------------
    // draw

    VBData *data = spriteVertexBuffer->GetData();
    data->VertList[0].Set(x,  y,  0.0f);
    data->VertList[1].Set(x,  y2, 0.0f);
    data->VertList[2].Set(x2, y,  0.0f);
    data->VertList[3].Set(x2, y2, 0.0f);

    if (!CloseFloat(degrees, 0.0f)) {
        List<Vect> &coords = data->VertList;

        Vect2 center(x+totalSize.x/2, y+totalSize.y/2);

        Matrix rotMatrix;
        rotMatrix.SetIdentity();
        rotMatrix.Rotate(AxisAngle(0.0f, 0.0f, 1.0f, RAD(degrees)));

        for (int i = 0; i < 4; i++) {
            Vect val = coords[i]-Vect(center);
            val.TransformVector(rotMatrix);
            coords[i] = val;
            coords[i] += Vect(center);
        }
    }

    List<UVCoord> &coords = data->UVList[0];
    coords[0].Set(u,  v);
    coords[1].Set(u,  v2);
    coords[2].Set(u2, v);
    coords[3].Set(u2, v2);

    if (!CloseFloat(texDegrees, 0.0f)) {
        Matrix rotMatrix;
        rotMatrix.SetIdentity();
        rotMatrix.Rotate(AxisAngle(0.0f, 0.0f, 1.0f, -RAD(texDegrees)));

        Vect2 minVal = Vect2(0.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            Vect val = Vect(coords[i]);
            val.TransformVector(rotMatrix);
            coords[i] = val;
            minVal.ClampMax(coords[i]);
        }

        for (int i = 0; i < 4; i++)
            coords[i] -= minVal;
    }
    spriteVertexBuffer->FlushBuffers();
    LoadVertexBuffer(spriteVertexBuffer);
    LoadTexture(texture);
    Draw(GS_TRIANGLESTRIP);
}

void D3D11System::DrawBox(const Vect2 &upperLeft, const Vect2 &size)
{
    VBData *data = boxVertexBuffer->GetData();

    Vect2 bottomRight = upperLeft+size;

    data->VertList[0] = upperLeft;
    data->VertList[1].Set(bottomRight.x, upperLeft.y);
    data->VertList[2].Set(bottomRight.x, bottomRight.y);
    data->VertList[3].Set(upperLeft.x, bottomRight.y);
    data->VertList[4] = upperLeft;

    boxVertexBuffer->FlushBuffers();

    LoadVertexBuffer(boxVertexBuffer);

    Draw(GS_LINESTRIP);
}

void D3D11System::ResetViewMatrix()
{
    Matrix4x4Convert(curViewMatrix, MatrixStack[curMatrix].GetTranspose());
    Matrix4x4Multiply(curViewProjMatrix, curViewMatrix, curProjMatrix);
    Matrix4x4Transpose(curViewProjMatrix, curViewProjMatrix);
}

void D3D11System::ResizeView()
{
    LPVOID nullVal = NULL;
    d3dContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&nullVal, NULL);

    SafeRelease(swapRenderView);

    swap->ResizeBuffers(2, 0, 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0);

    ID3D11Texture2D *backBuffer = NULL;
    HRESULT err = swap->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);
    if(FAILED(err))
        CrashError(TEXT("Unable to get back buffer from swap chain"));

    err = d3d->CreateRenderTargetView(backBuffer, NULL, &swapRenderView);
    if(FAILED(err))
        CrashError(TEXT("Unable to get render view from back buffer"));

    backBuffer->Release();
}
bool GraphicsInit(HWND mRenderWidgetHwnd,int renderWidth,int renderHeight,ConfigFile *configFile)  {
   GlobalConfig=configFile;
   bool ok=false;
   D3D11System *GS = new D3D11System(mRenderWidgetHwnd, renderWidth, renderHeight,ok);
   if(ok)
   {
      OBSGraphics_Register(GS);
   }
   else
   {
      delete GS;
   }
   return ok;
}
void GraphicsDestory()
{
   OBSGraphics_UnRegister();
}
D3D11RenderView::D3D11RenderView(){

}
D3D11RenderView::~D3D11RenderView(){

}
void D3D11RenderView::Init(HWND hwnd,int cx,int cy,D3D11System *gs){
   this->mHwnd = hwnd;
   this->mWidth = cx;
   this->mHeight = cy;
	DXGI_SWAP_CHAIN_DESC swapDesc;
	memset(&swapDesc, 0, sizeof(swapDesc));
	swapDesc.BufferCount       = 2;// 2
	swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapDesc.BufferDesc.Width  = cx;
	swapDesc.BufferDesc.Height = cy;
	swapDesc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow      = (HWND)hwnd;
	swapDesc.SampleDesc.Count  = 1;
	swapDesc.Windowed          = true;

   ComPtr<ID3D11Texture2D> backBuffer = NULL;
   OBSApiLog("D3D11RenderView::Init CreateSwapChain");
   HRESULT hr = gs->factory->CreateSwapChain(gs->d3d, &swapDesc, swap.Assign());
   if(hr!=S_OK) {
      OBSApiLog("D3D11RenderView::Init CreateSwapChain Failed! %X",hr);
      return ;
   }
   OBSApiLog("D3D11RenderView::Init GetBuffer %X", hr);
   hr = swap->GetBuffer(0,IID_ID3D11Texture2D,(void**)backBuffer.Assign());   
   if(hr!=S_OK) {
      OBSApiLog("D3D11RenderView::Init GetBuffer Failed! %X",hr);
      return ;
   }

   hr=gs->d3d->CreateRenderTargetView(backBuffer, NULL, swapRenderView.Assign());
   if(hr!=S_OK) {
      OBSApiLog("D3D11RenderView::Init GetBuffer CreateRenderTargetView! %X",hr);
      return ;
   }
   
   OBSApiLog("D3D11RenderView::Init End");
}
void *D3D11RenderView::GetRenderView(){
   return swapRenderView.Get();
}
void *D3D11RenderView::GetRenderSwap(){
   return swap.Get();

}

OBSRenderView *D3D11System::CreateRenderTargetView(HWND hwnd,int cx,int cy){
   D3D11RenderView *view = new D3D11RenderView();
   view->Init(hwnd,cx,cy,this);
   return view;
}
void D3D11System::DestoryRenderTargetView(OBSRenderView *view){
   delete view;
}
void D3D11System::SetRenderTargetView(OBSRenderView *view){
   currentView = dynamic_cast<D3D11RenderView *>(view);
   if(currentView) {
      ID3D11RenderTargetView *v = (ID3D11RenderTargetView *)currentView->GetRenderView();
      IDXGISwapChain *s = (IDXGISwapChain *)currentView->GetRenderSwap();
      
      if(swap_bak==NULL&&swapRenderView_bak==NULL) {
         swap_bak = this->swap;
         swapRenderView_bak = this->swapRenderView;
      }

      this->swap = s;
      swapRenderView = v;
      
      d3dContext->OMSetRenderTargets(1, &v, NULL);
   }
   else {
      if(swap_bak!=NULL&&swapRenderView_bak!=NULL) {
         this->swap = swap_bak;
         this->swapRenderView = swapRenderView_bak;
         swap_bak = NULL;
         swapRenderView_bak = NULL;
      }

     d3dContext->OMSetRenderTargets(1, &swapRenderView, NULL);
   }

}


