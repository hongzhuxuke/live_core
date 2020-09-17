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
//#define SafeRelease(punk)  \
//if ((punk) != NULL)  \
//{ (punk)->Release(); (punk) = NULL; }

inline bool IsPow2(UINT num)
{
    return num >= 2 && (num & (num-1)) == 0;
}
void CopyPackedRGB(BYTE *lpDest, BYTE *lpSource, UINT nPixels) {
   DWORD curComponent = 0;

   UINT totalBytes = (nPixels * 3);
   UINT alignedBytes = totalBytes & 0xFFFFFFFC;
   UINT nDWords = alignedBytes >> 2;

   DWORD *lpDWDest = (DWORD*)lpDest;
   DWORD *lpDWSrc = (DWORD*)lpSource;

   while (nDWords) {
      switch (curComponent) {
      case 0: *(lpDWDest++) = *lpDWSrc & 0xFFFFFF; *lpDWDest = *(lpDWSrc++) >> 24; break; //RBGR
      case 1: *(lpDWDest++) |= ((*lpDWSrc) << 8) & 0xFFFF00; *lpDWDest = *(lpDWSrc++) >> 16; break; //GRBG
      case 2: *(lpDWDest++) |= ((*lpDWSrc) << 16) & 0xFFFF00; *(lpDWDest++) = *(lpDWSrc++) >> 8;  break; //BGRB
      }

      if (curComponent == 2)
         curComponent = 0;
      else
         curComponent++;

      nDWords--;
   }

   totalBytes -= alignedBytes;
   lpSource = (LPBYTE)lpDWSrc;
   lpDest = (LPBYTE)lpDWDest;

   if (curComponent != 0)
      lpDest += curComponent;

   while (totalBytes--) {
      *(lpDest++) = *(lpSource++);

      if (curComponent == 2) {
         *(lpDest++) = 0;
         curComponent = 0;
      } else
         curComponent++;
   }
         }
const DXGI_FORMAT convertFormat[] = {
      DXGI_FORMAT_UNKNOWN, 
      DXGI_FORMAT_A8_UNORM,
      DXGI_FORMAT_R8_UNORM,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_B8G8R8X8_UNORM, 
      DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_FORMAT_R16G16B16A16_FLOAT,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      DXGI_FORMAT_B5G5R5A1_UNORM, 
      DXGI_FORMAT_B5G6R5_UNORM,
      DXGI_FORMAT_R10G10B10A2_UNORM,
      DXGI_FORMAT_BC1_UNORM, 
      DXGI_FORMAT_BC3_UNORM, 
      DXGI_FORMAT_BC5_UNORM,
      DXGI_FORMAT_420_OPAQUE
      };
const UINT formatPitch[] = {0, 1, 1, 4, 4, 4, 4, 8, 16, 0, 0, 0};

const D3D11_TEXTURE_ADDRESS_MODE convertAddressMode[] = {D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR, D3D11_TEXTURE_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE};
const D3D11_FILTER convertFilter[] = {D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_ANISOTROPIC, D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT};


SamplerState* D3D11SamplerState::CreateSamplerState(SamplerInfo &info)
{
    D3D11_SAMPLER_DESC sampDesc;
    zero(&sampDesc, sizeof(sampDesc));
    sampDesc.AddressU       = convertAddressMode[(UINT)info.addressU];
    sampDesc.AddressV       = convertAddressMode[(UINT)info.addressV];
    sampDesc.AddressW       = convertAddressMode[(UINT)info.addressW];
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.Filter         = convertFilter[(UINT)info.filter];
    sampDesc.MaxAnisotropy  = 1;//info.maxAnisotropy;
    sampDesc.MaxLOD         = FLT_MAX;
    mcpy(sampDesc.BorderColor, info.borderColor.ptr, sizeof(Color4));

    ID3D11SamplerState *state;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    HRESULT err = d3d->CreateSamplerState(&sampDesc, &state);
    if(FAILED(err))
    {
        OBSApiLog("D3D11SamplerState::CreateSamplerState: unable to create sampler state, result = %08lX", err);
        return NULL;
    }

    //-------------------------------------------

    D3D11SamplerState *samplerState = new D3D11SamplerState;
    samplerState->state = state;
    mcpy(&samplerState->info, &info, sizeof(SamplerInfo));

    return samplerState;
}

D3D11SamplerState::~D3D11SamplerState()
{
    SafeRelease(state);
}
inline GSColorFormat GetGSFormatFromDXGIFormat(DXGI_FORMAT dxgiFormat) {
   UINT numVals = sizeof(convertFormat) / sizeof(DXGI_FORMAT);

   for (UINT i = 0; i<numVals; i++) {
      if (convertFormat[i] == dxgiFormat)
         return (GSColorFormat)i;
   }

   return GS_UNKNOWNFORMAT;
}

Texture* D3D11Texture::CreateFromSharedHandle(unsigned int width, unsigned int height, HANDLE handle)
{
    HRESULT err;

    if(!handle)
    {
        OBSApiLog("D3D11Texture::CreateFromSharedHandle: NULL handle value.");
        return NULL;
    }

    ID3D11Resource *tempResource;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    if(FAILED(err = d3d->OpenSharedResource(handle, __uuidof(ID3D11Resource), (void**)&tempResource)))
    {
        OBSApiLog("D3D11Texture::CreateFromSharedHandle: Failed to open shared handle, result = 0x%08lX", err);
        return NULL;
    }

    ID3D11Texture2D *texVal;
    if(FAILED(err = tempResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texVal)))
    {
        SafeRelease(tempResource);
        OBSApiLog("D3D11Texture::CreateFromSharedHandle: could not query interface, result = 0x%08lX", err);
        return NULL;
    }

    tempResource->Release();

    //------------------------------------------

    D3D11_TEXTURE2D_DESC td;
    texVal->GetDesc(&td);

    //------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = td.Format;
    resourceDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *resource = NULL;

    if(FAILED(err = d3d->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        OBSApiLog("D3D11Texture::CreateFromSharedHandle: CreateShaderResourceView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11Texture *newTex = new D3D11Texture;
    newTex->format = GetGSFormatFromDXGIFormat(td.Format);;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->bDynamic = false;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

Texture* D3D11Texture::CreateTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat, void *lpData, BOOL bGenMipMaps, BOOL bStatic)
{
   //gLogger->logInfo("D3D11Texture::CreateTexture");
   HRESULT err;

   if (colorFormat >= GS_DXT1) {
      OBSApiLog("D3D11Texture::CreateTexture: tried to create a blank DXT texture.  Use CreateFromFile instead.");
      return NULL;
   }

   DXGI_FORMAT format = convertFormat[(UINT)colorFormat];

   if (bGenMipMaps && (!IsPow2(width) || !IsPow2(height))) {
      OBSApiLog("D3D11Texture::CreateTexture: Cannot generate mipmaps for a non-power-of-two sized texture.  Disabling mipmap generation.");
      bGenMipMaps = FALSE;
   }
   ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());

   D3D11_TEXTURE2D_DESC td;
   zero(&td, sizeof(td));
   td.Width = width;
   td.Height = height;
   td.MipLevels = bGenMipMaps ? 0 : 1;
   td.ArraySize = 1;
   td.Format = format;
   td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   td.SampleDesc.Count = 1;
   td.Usage = bStatic ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
   td.CPUAccessFlags = bStatic ? 0 : D3D11_CPU_ACCESS_WRITE;

   D3D11_SUBRESOURCE_DATA srd;
   D3D11_SUBRESOURCE_DATA *lpSRD;
   if (lpData) {
      srd.pSysMem = lpData;
      srd.SysMemPitch = width*formatPitch[(UINT)colorFormat];
      srd.SysMemSlicePitch = 0;
      lpSRD = &srd;
   } else
      lpSRD = NULL;

   ID3D11Texture2D *texVal;
   if (FAILED(err =d3d->CreateTexture2D(&td, lpSRD, &texVal))) {
      OBSApiLog("D3D11Texture::CreateTexture: CreateTexture2D failed, result = 0x%08lX", err);
      return NULL;
   }

   //------------------------------------------

   D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
   zero(&resourceDesc, sizeof(resourceDesc));
   resourceDesc.Format = format;
   resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
   resourceDesc.Texture2D.MipLevels = bGenMipMaps ? -1 : 1;

   ID3D11ShaderResourceView *resource;
   if (FAILED(err = d3d->CreateShaderResourceView(texVal, &resourceDesc, &resource))) {
      SafeRelease(texVal);
      OBSApiLog("D3D11Texture::CreateTexture: CreateShaderResourceView failed, result = 0x%08lX", err);
      return NULL;
   }

   //------------------------------------------

   D3D11Texture *newTex = new D3D11Texture;
   newTex->format = colorFormat;
   newTex->resource = resource;
   newTex->texture = texVal;
   newTex->bDynamic = !bStatic;
   newTex->width = width;
   newTex->height = height;

   return newTex;
}

Texture* D3D11Texture::CreateFromFile(CTSTR lpFile, BOOL bBuildMipMaps)
{
    HRESULT err;

    D3DX11_IMAGE_INFO ii;
    if(FAILED(D3DX11GetImageInfoFromFile(lpFile, NULL, &ii, NULL)))
    {
        OBSApiLog("D3D11Texture::CreateFromFile: Could not get information about texture file '%s'", lpFile);
        return NULL;
    }

    //------------------------------------------

    if(bBuildMipMaps && (!IsPow2(ii.Width) || !IsPow2(ii.Height)))
        bBuildMipMaps = FALSE;

    D3DX11_IMAGE_LOAD_INFO ili;
    ili.Width           = D3DX11_DEFAULT;
    ili.Height          = D3DX11_DEFAULT;
    ili.Depth           = D3DX11_DEFAULT;
    ili.FirstMipLevel   = D3DX11_DEFAULT;
    ili.MipLevels       = bBuildMipMaps ? 0 : 1;
    ili.Usage           = (D3D11_USAGE)D3DX11_DEFAULT;
    ili.BindFlags       = D3DX11_DEFAULT;
    ili.CpuAccessFlags  = D3DX11_DEFAULT;
    ili.MiscFlags       = D3DX11_DEFAULT;
    ili.Format          = (DXGI_FORMAT)D3DX11_DEFAULT;
    ili.Filter          = D3DX11_DEFAULT;
    ili.MipFilter       = D3DX11_DEFAULT;
    ili.pSrcInfo        = NULL;

    ID3D11Resource *texResource;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    if(FAILED(err = D3DX11CreateTextureFromFile(d3d, lpFile, &ili, NULL, &texResource, NULL)))
    {
        OBSApiLog("D3D11Texture::CreateFromFile: failed to load '%s'", lpFile);
        return NULL;
    }

    //------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = ii.Format;
    resourceDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = bBuildMipMaps ? -1 : 1;

    ID3D11ShaderResourceView *resource;
    if(FAILED(err = d3d->CreateShaderResourceView(texResource, &resourceDesc, &resource)))
    {
        SafeRelease(texResource);
        OBSApiLog("D3D11Texture::CreateFromFile: CreateShaderResourceView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    ID3D11Texture2D *tex2D;
    err = texResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex2D);
    if(FAILED(err))
    {
        SafeRelease(texResource);
        SafeRelease(resource);
        OBSApiLog("D3D11Texture::CreateFromFile: could not query texture interface");
        return NULL;
    }

    texResource->Release();

    //------------------------------------------

    D3D11Texture *newTex = new D3D11Texture;
    newTex->resource = resource;
    newTex->texture = tex2D;
    newTex->width = ii.Width;
    newTex->height = ii.Height;

    switch(ii.Format)
    {
        case DXGI_FORMAT_R8_UNORM:              newTex->format = GS_ALPHA;       break;
        case DXGI_FORMAT_A8_UNORM:              newTex->format = GS_GRAYSCALE;   break;
        case DXGI_FORMAT_B8G8R8X8_UNORM:        newTex->format = GS_BGR;         break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:        newTex->format = GS_BGRA;        break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:        newTex->format = GS_RGBA;        break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:    newTex->format = GS_RGBA16F;     break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    newTex->format = GS_RGBA32F;     break;
        case DXGI_FORMAT_BC1_UNORM:             newTex->format = GS_DXT1;        break;
        case DXGI_FORMAT_BC2_UNORM:             newTex->format = GS_DXT3;        break;
        case DXGI_FORMAT_BC3_UNORM:             newTex->format = GS_DXT5;        break;
        default:
            newTex->format = GS_UNKNOWNFORMAT;
    }

    return newTex;
}

Texture* D3D11Texture::CreateRenderTarget(unsigned int width, unsigned int height, GSColorFormat colorFormat, BOOL bGenMipMaps)
{
    HRESULT err;

    if(colorFormat >= GS_DXT1)
    {
        OBSApiLog("D3D11Texture::CreateRenderTarget: tried to a blank DXT render target");
        return NULL;
    }

    DXGI_FORMAT format = convertFormat[(UINT)colorFormat];

    D3D11_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = bGenMipMaps ? 0 : 1;
    td.ArraySize        = 1;
    td.Format           = format;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    ID3D11Texture2D *texVal;
    if(FAILED(err = d3d->CreateTexture2D(&td, NULL, &texVal)))
    {
        OBSApiLog("D3D11Texture::CreateRenderTarget: CreateTexture2D failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = format;
    resourceDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = bGenMipMaps ? -1 : 1;

    ID3D11ShaderResourceView *resource;
    if(FAILED(err = d3d->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        OBSApiLog("D3D11Texture::CreateRenderTarget: CreateShaderResourceView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    ID3D11RenderTargetView *view;
    err = d3d->CreateRenderTargetView(texVal, NULL, &view);
    if(FAILED(err))
    {
        SafeRelease(texVal);
        SafeRelease(resource);
        OBSApiLog("D3D11Texture::CreateRenderTarget: CreateRenderTargetView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11Texture *newTex = new D3D11Texture;
    newTex->format = colorFormat;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->renderTarget = view;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}

Texture* D3D11Texture::CreateGDITexture(unsigned int width, unsigned int height)
{
    HRESULT err;

    D3D11_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.MiscFlags        = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

    D3D11_SUBRESOURCE_DATA srd;
    zero(&srd, sizeof(srd));

    ID3D11Texture2D *texVal;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    if(FAILED(err = d3d->CreateTexture2D(&td, NULL, &texVal)))
    {
        OBSApiLog("D3D11Texture::CreateGDITexture: CreateTexture2D failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = DXGI_FORMAT_B8G8R8A8_UNORM;
    resourceDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *resource;
    if(FAILED(err = d3d->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        OBSApiLog("D3D11Texture::CreateGDITexture: CreateShaderResourceView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11Texture *newTex = new D3D11Texture;
    newTex->format = GS_BGRA;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->width = width;
    newTex->height = height;
    newTex->bGDICompatible = true;

    return newTex;
}
Texture* D3D11Texture::CreateReadTexture(unsigned int width, unsigned int height, GSColorFormat colorFormat)
{
   HRESULT err;

   if (colorFormat >= GS_DXT1) {
      OBSApiLog("D3D11Texture::CreateReadTexture: tried to create a blank DXT texture.  Use CreateFromFile instead.");
      return NULL;
   }

   DXGI_FORMAT format = convertFormat[(UINT)colorFormat];

   ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());

   D3D11_TEXTURE2D_DESC td;
   zero(&td, sizeof(td));
   td.Width = width;
   td.Height = height;
   td.MipLevels = 1;
   td.ArraySize = 1;
   td.Format = format;
   td.SampleDesc.Count = 1;
   td.Usage = D3D11_USAGE_STAGING;
   td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

   ID3D11Texture2D *texVal;
   if (FAILED(err =d3d->CreateTexture2D(&td, NULL, &texVal))) {
      OBSApiLog("D3D11Texture::CreateTexture: CreateTexture2D failed, result = 0x%08lX", err);
      return NULL;
   }

   D3D11Texture *newTex = new D3D11Texture;
   newTex->format = colorFormat;
   newTex->resource = NULL;
   newTex->texture = texVal;
   newTex->bDynamic = false;
   newTex->width = width;
   newTex->height = height;
   newTex->mIsReadOnly=true;

   return newTex;
}


Texture* D3D11Texture::CreateShared(unsigned int width, unsigned int height)
{
    HRESULT err;

    D3D11_TEXTURE2D_DESC td;
    zero(&td, sizeof(td));
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.CPUAccessFlags   = 0;
    td.MiscFlags		= D3D11_RESOURCE_MISC_SHARED;

    ID3D11Texture2D *texVal;
    ID3D11Device *d3d=static_cast<ID3D11Device *>(GetD3D());
    if(FAILED(err = d3d->CreateTexture2D(&td, NULL, &texVal)))
    {
        OBSApiLog("D3D11Texture::CreateShared: CreateTexture2D failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
    zero(&resourceDesc, sizeof(resourceDesc));
    resourceDesc.Format              = DXGI_FORMAT_B8G8R8A8_UNORM;
    resourceDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *resource;
    if(FAILED(err = d3d->CreateShaderResourceView(texVal, &resourceDesc, &resource)))
    {
        SafeRelease(texVal);
        OBSApiLog("D3D11Texture::CreateShared: CreateShaderResourceView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    ID3D11RenderTargetView *view;
    err = d3d->CreateRenderTargetView(texVal, NULL, &view);
    if(FAILED(err))
    {
        SafeRelease(texVal);
        SafeRelease(resource);
        OBSApiLog("D3D11Texture::CreateShared: CreateRenderTargetView failed, result = 0x%08lX", err);
        return NULL;
    }

    //------------------------------------------

    D3D11Texture *newTex = new D3D11Texture;
    newTex->format = GS_BGRA;
    newTex->resource = resource;
    newTex->texture = texVal;
    newTex->renderTarget = view;
    newTex->width = width;
    newTex->height = height;

    return newTex;
}
D3D11Texture::D3D11Texture():Texture()
{
   mIsReadOnly=false;
}

D3D11Texture::~D3D11Texture()
{
    SafeRelease(renderTarget);
    SafeRelease(resource);
    SafeRelease(texture);
}

DWORD D3D11Texture::Width() const
{
    return width;
}

DWORD D3D11Texture::Height() const
{
    return height;
}

BOOL D3D11Texture::HasAlpha() const
{
    return format == 1 || (format >= GS_RGBA && format <= GS_RGBA32F) || format == GS_DXT3 || format == GS_DXT5;
}

GSColorFormat D3D11Texture::GetFormat() const
{
    return (GSColorFormat)format;
}

bool D3D11Texture::GetDC(HDC &hDC)
{
    if(!bGDICompatible)
    {
        OBSApiLog("D3D11Texture::GetDC: function was called on a non-GDI-compatible texture");
        return false;
    }

    HRESULT err;
    if(FAILED(err = texture->QueryInterface(__uuidof(IDXGISurface1), (void**)&surface)))
    {
        OBSApiLog("D3D11Texture::GetDC: could not query surface interface, result = %08lX", err);
        return false;
    }

    if(FAILED(err = surface->GetDC(TRUE, &hDC)))
    {
        OBSApiLog("D3D11Texture::GetDC: could not get DC, result = %08lX", err);
        SafeRelease(surface);
        return false;
    }

    return true;
}

void D3D11Texture::ReleaseDC()
{
    if(!surface)
    {
        OBSApiLog("D3D11Texture::ReleaseDC: no DC to release");
        return;
    }

    surface->ReleaseDC(NULL);
    SafeRelease(surface);
}

//====================================================================================

void CopyPackedRGB(BYTE *lpDest, BYTE *lpSource, UINT nPixels);

void D3D11Texture::SetImage(void *lpData, GSImageFormat imageFormat, UINT pitch)
{
    if(!bDynamic)
    {
        OBSApiLog("3D11Texture::SetImage: cannot call on a non-dynamic texture");
        return;
    }

    bool bMatchingFormat = false;
    UINT pixelBytes = 0;    //What is this needed for?

    switch(format)
    {
        case GS_ALPHA:      bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_A8); pixelBytes = 1; break;
        case GS_GRAYSCALE:  bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_L8); pixelBytes = 1; break;
        case GS_RGB:        bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGB || imageFormat == GS_IMAGEFORMAT_RGBX); pixelBytes = 4; break;
        case GS_RGBA:       bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA); pixelBytes = 4; break;
        case GS_BGR:        bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_BGR || imageFormat == GS_IMAGEFORMAT_BGRX); pixelBytes = 4; break;
        case GS_BGRA:       bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_BGRA); pixelBytes = 4; break;
        case GS_RGBA16F:    bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA16F); pixelBytes = 8; break;
        case GS_RGBA32F:    bMatchingFormat = (imageFormat == GS_IMAGEFORMAT_RGBA32F); pixelBytes = 16; break;
    }

    if(!bMatchingFormat)
    {
        OBSApiLog("D3D11Texture::SetImage: invalid or mismatching image format specified");
        return;
    }

    BYTE *mapData;
    UINT mapPitch;
    if (!D3D11ResourceMap(texture, mapData, mapPitch, D3D11_MAP_WRITE_DISCARD))
    {
        OBSApiLog("D3D11Texture::SetImage: map failed");
        return;
    }

    //-------------------------------------------------------------------------
    if((format == GS_RGB || format == GS_BGR) && (imageFormat == GS_IMAGEFORMAT_BGR || imageFormat == GS_IMAGEFORMAT_RGB))
    {
       if (pitch == (width * 3) && mapPitch == (width * 4))
           CopyPackedRGB((BYTE*)mapData, (BYTE*)lpData, width*height);
        else
        {
            for(UINT y=0; y<height; y++)
            {
                LPBYTE curInput  = ((LPBYTE)lpData)    + (pitch*y);
                LPBYTE curOutput = ((LPBYTE)mapData) + (mapPitch*y);

                CopyPackedRGB(curOutput, curInput, width);
            }
        }
    }
    //-------------------------------------------------------------------------
    else
    {
       if (pitch == mapPitch)
          mcpy(mapData, lpData, pitch*height);
        else
        {
           UINT bestPitch = MIN(pitch, mapPitch);

            for(UINT y=0; y<height; y++)
            {
                LPBYTE curInput  = ((LPBYTE)lpData)    + (pitch*y);
                LPBYTE curOutput = ((LPBYTE)mapData) + (mapPitch*y);

                mcpy(curOutput, curInput, bestPitch);
            }
        }
    }

    D3D11ResourceUnMap(texture);
}

bool D3D11Texture::Map(BYTE *&lpData, UINT &pitch)
{
    D3D11_MAP mapType;
    if(mIsReadOnly)
    {
       mapType = D3D11_MAP_READ;
    }
    else
    {
       mapType = D3D11_MAP_WRITE_DISCARD;
    }

    if (!D3D11ResourceMap(texture, lpData, pitch, mapType))
    {
       //CrashError(TEXT("Texture->Map failed"));
       OBSApiLog("D3D11Texture::Map D3D11ResourceMap FAILED");
       return false;
    }

    return true;
}

void D3D11Texture::Unmap()
{
    D3D11ResourceUnMap(texture);
}

HANDLE D3D11Texture::GetSharedHandle()
{
    HRESULT err;
    HANDLE handle = NULL;
    IDXGIResource *pDXGIResource;
    if FAILED(err = texture->QueryInterface(__uuidof(IDXGIResource), (void **)&pDXGIResource))
    {
        OBSApiLog("D3D11Texture::GetSharedHandle: QueryInterface failed, result = %08lX", err);
        return handle;
    }
    
    if FAILED(err = pDXGIResource->GetSharedHandle(&handle))
    {
        OBSApiLog("D3D11Texture::GetSharedHandle: GetSharedHandle failed, result = %08lX", err);
    }
    
    return handle;
}
