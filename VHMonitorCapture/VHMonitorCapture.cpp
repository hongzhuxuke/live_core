#include "vhmonitorcapture.h"
#include <list>
#include <vector>
#include <gdiplus.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
using namespace std;
vector <VHD_WindowInfo> g_WindowInfo;
vector <VHD_WindowInfo> g_CheckWindowInfo;
unsigned int g_index = 0;
unsigned int g_desktopIndex = 0;
unsigned int g_imageProcess = 0;

int g_DstStride_y = 0;
int g_DstStride_u = 0;
int g_DstStride_v = 0;

uint8_t* g_DstData_y = nullptr;
uint8_t* g_DstData_u = nullptr;
uint8_t* g_DstData_v = nullptr;

uint8_t* g_Out_DstData_y = nullptr;

int g_desktopWidth = 0;
int g_desktopHeight = 0;
FILE* mPlayerAudioFile = nullptr; 

typedef struct _tagVMonitorInfo_t
{
   HMONITOR hMonitor; //显示器句柄
   TCHAR szDevice[32]; //显示器名
   RECT rcVirtual; //虚拟显示屏坐标
   RECT rcMonitor; //物理显示坐标
   RECT rcWork; //工作显示坐标
   BOOL bPrimary; //主显示器？

   _tagVMonitorInfo_t()
   {
      memset(this, 0, sizeof(*this));
   }
}VMONITORINFO, *LPVMONITORINFO;


BOOL CALLBACK VHD_Windows_Enum_Callback(HWND hwnd, LPARAM) {
   if (!IsWindowVisible(hwnd)) {
      return TRUE;
   }

   WCHAR name[1024];
   GetWindowText(hwnd, name, 1024);

   if (wcslen(name) == 0) {
      return TRUE;
   }

   VHD_WindowInfo windowInfo;
   memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
   windowInfo.type = VHD_Window;
   wcsncpy_s(windowInfo.name, name, 1024);
   windowInfo.hwnd = hwnd;
   GetWindowRect(hwnd, &windowInfo.rect);


   g_WindowInfo.push_back(windowInfo);
   return TRUE;
}

BOOL CALLBACK MonitorInfoEnumProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM) {
   VHD_WindowInfo windowInfo;
   memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
   windowInfo.type = VHD_Desktop;
   windowInfo.screen_id = g_desktopIndex;
   wsprintf(windowInfo.name, L"DISPLAY:%d", g_desktopIndex++);
   windowInfo.hwnd = (HWND)hMonitor;
   windowInfo.rect = *rect;

   //GetMonitorInfo 获取显示器信息
   MONITORINFOEX infoEx;
   memset(&infoEx, 0, sizeof(infoEx));
   infoEx.cbSize = sizeof(infoEx);
   if (GetMonitorInfo(hMonitor, &infoEx)){
      memcpy(windowInfo.name, infoEx.szDevice, sizeof(infoEx.szDevice));
   }

   g_WindowInfo.push_back(windowInfo);
   return TRUE;
}

BOOL CALLBACK CheckMonitorInfoEnumProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM) {
   VHD_WindowInfo windowInfo;
   memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
   windowInfo.type = VHD_Desktop;
   wsprintf(windowInfo.name, L"DISPLAY:%d", g_desktopIndex++);
   windowInfo.hwnd = (HWND)hMonitor;
   windowInfo.rect = *rect;
   g_CheckWindowInfo.push_back(windowInfo);
   return TRUE;
}

typedef HANDLE(WINAPI *OPPROC) (DWORD, BOOL, DWORD);


size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out,
                     size_t outsize, int flags) {
   int i_insize = (int)insize;
   int ret;

   if (i_insize == 0)
      i_insize = (int)wcslen(in);

   ret = WideCharToMultiByte(CP_UTF8, 0, in, i_insize, out, (int)outsize,
                             NULL, NULL);

   return (ret > 0) ? (size_t)ret : 0;
}

size_t os_wcs_to_utf8(const wchar_t *str, size_t len, char *dst,
                      size_t dst_size) {
   size_t in_len;
   size_t out_len;

   if (!str)
      return 0;

   in_len = (len != 0) ? len : wcslen(str);
   out_len = dst ? (dst_size - 1) : wchar_to_utf8(str, in_len, NULL, 0, 0);

   if (dst) {
      if (!dst_size)
         return 0;

      if (out_len)
         out_len = wchar_to_utf8(str, in_len,
         dst, out_len + 1, 0);

      dst[out_len] = 0;
   }

   return out_len;
}

static bool GetWindowTitle(HWND window, string &title) {
   size_t len = (size_t)GetWindowTextLengthW(window);
   wstring wtitle;

   wtitle.resize(len);
   if (!GetWindowTextW(window, &wtitle[0], (int)len + 1))
      return false;

   len = os_wcs_to_utf8(wtitle.c_str(), 0, nullptr, 0);
   title.resize(len);
   os_wcs_to_utf8(wtitle.c_str(), 0, &title[0], len + 1);
   return true;
}

static bool WindowValid(HWND window) {
   LONG_PTR styles, ex_styles;
   RECT rect;
   DWORD id;

   if (!IsWindowVisible(window))
      return false;
   GetWindowThreadProcessId(window, &id);
   if (id == GetCurrentProcessId())
      return false;

   GetClientRect(window, &rect);
   styles = GetWindowLongPtr(window, GWL_STYLE);
   ex_styles = GetWindowLongPtr(window, GWL_EXSTYLE);

   if (ex_styles & WS_EX_TOOLWINDOW)
      return false;
   if (styles & WS_CHILD)
      return false;

   return true;
}

BOOL
Utf8ToWchar(const char *utf8Str,
WCHAR **wStr) {
   size_t strLen;
   size_t WcharLen;
   if (!utf8Str) {
      *wStr = NULL;
      return FALSE;
   }
   strLen = strlen(utf8Str);
   if (strLen == 0) {
      *wStr = NULL;
      return FALSE;
   }

   /* Get the buffer size. */
   WcharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
   if (WcharLen == 0) {
      //   fprintf(stderr, "Utf8ToWchar: Error getting buffer size: %d\n", 
      //          GetLastError());
      *wStr = NULL;
      return FALSE;
   }

   *wStr = (WCHAR *)malloc(sizeof(WCHAR)* (WcharLen + 1));

   if (*wStr == NULL) {
      ;
      //throws exception here
   }
   //   assert(*wStr);

   WcharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1,
                                  *wStr, (int)WcharLen);
   if (WcharLen == 0) {
      //  fprintf(stderr, "Utf8ToWchar MultiByteToWideChar error: %d\n", 
      //          GetLastError());
      free(*wStr);
      *wStr = NULL;
      return FALSE;
   }
   (*wStr)[WcharLen] = L'\0';
   return TRUE;
}

void VHD_Window_Enum()
{
   vector<string> windows;
   HWND window = GetWindow(GetDesktopWindow(), GW_CHILD);

   while (window) {
      string title;
      if (WindowValid(window) && GetWindowTitle(window, title) && GetParent(window) == NULL) {
         RECT clientRect;
         GetWindowRect(window, &clientRect);
         BOOL bFoundModule = false;
         VHD_WindowInfo windowInfo;
         memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
         windowInfo.type = VHD_Window;
         WCHAR *name = new WCHAR[1024];
         Utf8ToWchar(title.c_str(), &name);
         wcsncpy_s(windowInfo.name, name, 1024);
         delete []name;
         windowInfo.hwnd = window;
         GetWindowRect(window, &windowInfo.rect);

         if (clientRect.right - clientRect.left > 0 &&
             clientRect.bottom - clientRect.top > 0) {
            g_WindowInfo.push_back(windowInfo);
         }
      }
      windows.emplace_back(title);
      window = GetNextWindow(window, GW_HWNDNEXT);
   }
   return;

//
//#define MAX_PATH 1024
//   HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
//   //get all windows,which parent is desktop
//   
//   do {
//
//      wchar_t wzWindowName[1024] = { 0 };
//      GetWindowText(hwndCurrent, wzWindowName, 1024);
//
//      if(wcslen(wzWindowName)==0) {
//         continue;
//      }
//
//      RECT clientRect;
//      GetWindowRect(hwndCurrent, &clientRect);
//      
//      DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
//      DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);
//
//      if (0 != wcslen(wzWindowName) && NULL != wcsstr(wzWindowName, L"battlefield")) {
//         exStyles &= ~WS_EX_TOOLWINDOW;
//      }
//      
//      DWORD processID;
//      GetWindowThreadProcessId(hwndCurrent, &processID);
//      if (processID == GetCurrentProcessId())
//         continue;
//
//      if ((exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 &&
//          clientRect.bottom != 0 && clientRect.right != 0 /*&& hwndParent == NULL*/) {
//         if(!IsIconic(hwndCurrent)&&IsWindowVisible(hwndCurrent) && GetParent(hwndCurrent) == NULL) {
//            BOOL bFoundModule = false;         
//            VHD_WindowInfo windowInfo;
//            memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
//            windowInfo.type = VHD_Window;
//            wcsncpy_s(windowInfo.name, wzWindowName, 1024);
//            windowInfo.hwnd = hwndCurrent;
//            GetWindowRect(hwndCurrent, &windowInfo.rect);
//            
//            if(clientRect.right - clientRect.left > 0 && 
//                  clientRect.bottom - clientRect.top > 0) {
//               g_WindowInfo.push_back(windowInfo);
//            }
//
//            continue;
//         } 
//      }
//
//      if(clientRect.right-clientRect.left==0||clientRect.bottom-clientRect.top==0) {
//         continue;
//      }
//
//      if(!IsWindowVisible(hwndCurrent)&&!IsIconic(hwndCurrent)) {
//         continue;
//      }
//      
//      exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
//      //DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);
//      
//      if(WS_EX_WINDOWEDGE&exStyles||WS_EX_APPWINDOW&exStyles||WS_EX_STATICEDGE&exStyles) {
//         
//         VHD_WindowInfo windowInfo;
//         memset(&windowInfo, 0, sizeof(VHD_WindowInfo));
//         windowInfo.type = VHD_Window;
//         wcsncpy_s(windowInfo.name, wzWindowName, 1024);
//         windowInfo.hwnd = hwndCurrent;
//         GetWindowRect(hwndCurrent, &windowInfo.rect);
//         g_WindowInfo.push_back(windowInfo);
//
//      }
//
//   } while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));
}

void VHD_ActiveWindow(VHD_WindowInfo &info) {
   ShowWindow(info.hwnd, SW_RESTORE);
   BringWindowToTop(info.hwnd);
}


void VHD_Window_Enum_init(VHD_Window_Type type) {
   g_WindowInfo.clear();
   g_index = 0;
   g_desktopIndex = 0;
   if (type&VHD_Desktop) {
      EnumDisplayMonitors(NULL, NULL, MonitorInfoEnumProc, NULL);
   }

   if (type&VHD_Window) {
      //EnumWindows(VHD_Windows_Enum_Callback, NULL);
      VHD_Window_Enum();
   }
}

std::vector<VHD_WindowInfo> VHD_DesktopWindowInfo() {
   return g_WindowInfo;
}

bool VHD_Window_Enum_update(VHD_WindowInfo *pInfo) {
   if (g_index >= g_WindowInfo.size()) {
      return false;
   }

   if (pInfo == NULL) {
      return false;
   }

   *pInfo = g_WindowInfo[g_index];
   g_index++;

   return true;
}
void VHD_Window_Enum_final() {
   g_WindowInfo.clear();
   g_index = 0;
   g_desktopIndex = 0;
}

VHD_WindowInfo VHD_CreateAreaWindow(int left, int top, int right, int bottom) {
   VHD_WindowInfo areaInfo;
   areaInfo.rect.left = left;
   areaInfo.rect.top = top;
   areaInfo.rect.right = right;
   areaInfo.rect.bottom = bottom;
   areaInfo.type=VHD_AREA;

   return areaInfo;
}
void VHD_FillRect(HDC desHdc,int color,int left,int top,int right,int bottom){
   HBRUSH hbrush = CreateSolidBrush(color);
   SelectObject(desHdc, hbrush);
   Rectangle(desHdc,left,top, right, bottom);
   DeleteObject(hbrush);

}
bool VHD_Render_Old(VHD_WindowInfo &info, const HDC &desHdc, int width, int height, bool bRenderCursor){
   return true;
}

BOOL PaintMouse(HDC hdc)
{
    HDC bufdc = NULL;
    CURSORINFO cursorInfo = { 0 };
    ICONINFO iconInfo = { 0 };
    HBITMAP bmpOldMask = NULL;

    bufdc = CreateCompatibleDC(hdc);
    RtlZeroMemory(&iconInfo, sizeof(cursorInfo));
    cursorInfo.cbSize = sizeof(cursorInfo);

    GetCursorInfo(&cursorInfo);

    GetIconInfo(cursorInfo.hCursor, &iconInfo);

    bmpOldMask = (HBITMAP)SelectObject(bufdc, iconInfo.hbmMask);
    BitBlt(hdc, cursorInfo.ptScreenPos.x, cursorInfo.ptScreenPos.y, 20, 20, bufdc, 0, 0, SRCAND);
    SelectObject(bufdc, iconInfo.hbmColor);
    BitBlt(hdc, cursorInfo.ptScreenPos.x, cursorInfo.ptScreenPos.y, 20, 20, bufdc, 0, 0, SRCPAINT);

    SelectObject(bufdc, bmpOldMask);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DeleteDC(bufdc);
    return TRUE;
}

bool VHD_PreView(VHD_WindowInfo &info, const HDC &desHdc, int width, int height, bool bRenderCursor) {
   HDC captureDC = NULL;
   RECT srcRect = { 0, 0, 0, 0 };

   captureDC = GetDC(NULL);
   srcRect = info.rect;

   if (captureDC == NULL) {
      return true;
   }

   //background-color
   VHD_FillRect(desHdc, RGB(38, 48, 58), 0, 0, width, height);

   HDC hMemDC = nullptr;
   HANDLE hBmp = nullptr, hOldBmp = nullptr;
   hMemDC = CreateCompatibleDC(captureDC);
   if (hMemDC) {
      hBmp = CreateCompatibleBitmap(captureDC, width, height);
      if (hBmp) {
         hOldBmp = SelectObject(hMemDC, hBmp);
         BitBlt(hMemDC, 0, 0, width, height, captureDC, srcRect.left, srcRect.top, CAPTUREBLT | SRCCOPY);
         hBmp = SelectObject(hMemDC, hOldBmp);
         DeleteObject(hBmp);
      }
      DeleteDC(hMemDC);
   }

   ReleaseDC(NULL, captureDC);
   RECT cursorRect = info.rect;

   if (!bRenderCursor) {
      return true;
   }

   CURSORINFO ci;
   memset(&ci, 0, sizeof(ci));
   ci.cbSize = sizeof(ci);
   GetCursorInfo(&ci);
   if (ci.flags & CURSOR_SHOWING) {
      HICON hIcon = CopyIcon(ci.hCursor);
      if (hIcon) {
         ICONINFO ii;
         if (GetIconInfo(hIcon, &ii)) {
            POINT capturePos = { cursorRect.left, cursorRect.top };

            int x = ci.ptScreenPos.x - int(ii.xHotspot) - capturePos.x;
            int y = ci.ptScreenPos.y - int(ii.yHotspot) - capturePos.y;

            DrawIcon(desHdc, x, y, hIcon);
            DeleteObject(ii.hbmColor);
            DeleteObject(ii.hbmMask);
         }
         DestroyIcon(hIcon);
      }
   }
   return true;
}

#define COLORREF2RGB(Color) (Color & 0xff00) | ((Color >> 16) & 0xff) | ((Color << 16) & 0xff0000)

bool VHD_Render(VHD_WindowInfo &info, const HDC &desHdc, int width, int height, bool bRenderCursor,bool bOpenEnhance) {
   HDC captureDC = NULL;
   RECT srcRect = { 0, 0, 0, 0 };

   //std::list<RECT> window_cover_rect;
   
   if (info.type == VHD_Window) {
      if(IsIconic(info.hwnd)) {
         return true;
      }

      if (!IsWindow(info.hwnd)) {
         return false;
      }

      GetWindowRect(info.hwnd, &info.rect);
      wchar_t wzWindowName[1024] = { 0 };
      HWND lHwnd = info.hwnd;

      if(info.isCompatible) {
         captureDC = GetDC(NULL);         
         srcRect = info.rect;         
      }
      else {
         //captureDC = GetDC(lHwnd);
         captureDC = GetWindowDC(lHwnd);

         GetWindowRect(info.hwnd, &info.rect);
         srcRect.left=0;
         srcRect.top=0;
         srcRect.right = info.rect.right - info.rect.left;
         srcRect.bottom = info.rect.bottom - info.rect.top;
      }
   } 
   else {
      captureDC = GetDC(NULL);
      srcRect = info.rect;
   }

   if (captureDC == NULL) {
      return true;
   }

   //background-color
   VHD_FillRect(desHdc,RGB(38,48,58), 0,0, width, height);
   if (info.isCompatible) {
      int aScreenWidth, aScreenHeight;
      aScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      aScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
      
      int left = srcRect.left;
      int top = srcRect.top;
      int right = srcRect.right;
      int bottom = srcRect.bottom;

      if (left<0) {
         left=0;
      }
      
      if (top<0) {
         top=0;
      }
      if (right>aScreenWidth) {
         right = aScreenWidth;
      }
      if (bottom>aScreenHeight) {
         bottom = aScreenHeight;
      }

      info.rect.left = left;
      info.rect.top = top;
      info.rect.right = right;
      info.rect.bottom = bottom;

      width = right - left;
      height = bottom - top;
      BitBlt(desHdc, 0, 0, width, height, captureDC, left, top, SRCCOPY | CAPTUREBLT);
   }
   else {
       HDC hMemDC = nullptr;
       HBITMAP hBmp = nullptr, hOldBmp = nullptr;
       HBITMAP DirectBitmap = nullptr;
       hMemDC = CreateCompatibleDC(captureDC);
       if (hMemDC) {
           hBmp = CreateCompatibleBitmap(captureDC, width, height);
           if (hBmp) {
              hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
              BitBlt(hMemDC, 0, 0, width, height, captureDC, srcRect.left, srcRect.top, CAPTUREBLT | SRCCOPY);
           }
           BitBlt(desHdc, 0, 0, width, height, hMemDC, 0, 0, CAPTUREBLT | SRCCOPY);
           hBmp = (HBITMAP)SelectObject(hMemDC, hOldBmp);
           DeleteObject(DirectBitmap);
           DeleteObject(hBmp);
           DeleteDC(hMemDC);
       }
   }

   ReleaseDC(NULL,captureDC);
   RECT cursorRect = info.rect;

   if (!bRenderCursor) {
      return true;
   }

   CURSORINFO ci;
   memset(&ci, 0, sizeof(ci));
   ci.cbSize = sizeof(ci);
   GetCursorInfo(&ci);
   if (ci.flags & CURSOR_SHOWING) {
      HICON hIcon = CopyIcon(ci.hCursor);
      if (hIcon) {
         ICONINFO ii;
         if (GetIconInfo(hIcon, &ii)) {
            POINT capturePos = { cursorRect.left, cursorRect.top };

            int x = ci.ptScreenPos.x - int(ii.xHotspot) - capturePos.x;
            int y = ci.ptScreenPos.y - int(ii.yHotspot) - capturePos.y;

            DrawIcon(desHdc, x, y, hIcon);
            DeleteObject(ii.hbmColor);
            DeleteObject(ii.hbmMask);
         }
         DestroyIcon(hIcon);
      }
   }
   return true;
}
