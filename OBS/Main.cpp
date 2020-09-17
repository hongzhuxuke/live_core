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

#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>

#include <intrin.h>
#include <inttypes.h>

#include <gdiplus.h>


//----------------------------

ConfigFile  *GlobalConfig = NULL;
ConfigFile  *AppConfig = NULL;
bool        bIsPortable = false;
bool        bStreamOnStart = false;
TCHAR       lpAppPath[MAX_PATH];
TCHAR       lpAppDataPath[MAX_PATH];


HANDLE hOBSMutex = NULL;

void LoadGlobalIni() {
   GlobalConfig = new ConfigFile;
   String strGlobalIni;
   strGlobalIni << lpAppDataPath << TEXT("\\global.ini");
}

int HasSSE2Support() {
   int cpuInfo[4];

   __cpuid(cpuInfo, 1);

   return (cpuInfo[3] & (1 << 26)) != 0;
}

typedef BOOL(WINAPI *getUserModeExceptionProc)(LPDWORD);
typedef BOOL(WINAPI *setUserModeExceptionProc)(DWORD);

void InitializeExceptionHandler() {
   HMODULE k32;

   //standard app-wide unhandled exception filter
   SetUnhandledExceptionFilter(OBSExceptionHandler);

   //fix for exceptions being swallowed inside callbacks (see KB976038)
   k32 = GetModuleHandle(TEXT("KERNEL32"));
   if (k32) {
      DWORD dwFlags;
      getUserModeExceptionProc procGetProcessUserModeExceptionPolicy;
      setUserModeExceptionProc procSetProcessUserModeExceptionPolicy;

      procGetProcessUserModeExceptionPolicy = (getUserModeExceptionProc)GetProcAddress(k32, "GetProcessUserModeExceptionPolicy");
      procSetProcessUserModeExceptionPolicy = (setUserModeExceptionProc)GetProcAddress(k32, "SetProcessUserModeExceptionPolicy");

      if (procGetProcessUserModeExceptionPolicy && procSetProcessUserModeExceptionPolicy) {
         if (procGetProcessUserModeExceptionPolicy(&dwFlags))
            procSetProcessUserModeExceptionPolicy(dwFlags & ~1);
      }
   }
}

void SetWorkingFolder(void) {
   String modulePath;

   if (GetFileAttributes(TEXT("locale\\en.txt")) != INVALID_FILE_ATTRIBUTES)
      return;

   modulePath.SetLength(MAX_PATH);

   if (GetModuleFileName(NULL, modulePath, modulePath.Length() - 1)) {
      TCHAR *p;

      p = srchr(modulePath, '\\');
      if (p)
         *p = 0;

      SetCurrentDirectory(modulePath);
   }
}

BOOL bDisableComposition;
OSFileChangeData *pGCHLogMF = NULL;

String *strCaptureHookLog=NULL;



ULONG_PTR gdipToken;
const Gdiplus::GdiplusStartupInput gdipInput;

void SetupIni(CTSTR profile) 
{
   //first, find out which profile we're using

   String strProfile = profile ? profile : GlobalConfig->GetString(TEXT("General"), TEXT("Profile"));
   DWORD lastVersion = GlobalConfig->GetInt(TEXT("General"), TEXT("LastAppVersion"));
   String strIni;

   if (profile)
      GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), profile);

   bool bFoundProfile = false;

   if (strProfile.IsValid()) {
      strIni << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");
      bFoundProfile = OSFileExists(strIni) != 0;
   }

   if (!bFoundProfile) {
      OSFindData ofd;

      strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\*.ini");
      HANDLE hFind = OSFindFirstFile(strIni, ofd);
      if (hFind) {
         do {
            if (ofd.bDirectory) continue;

            strProfile = GetPathWithoutExtension(ofd.fileName);
            GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);
            bFoundProfile = true;

            break;

         } while (OSFindNextFile(hFind, ofd));

         OSFindClose(hFind);
      }
   }

   //--------------------------------------------
   // open, or if no profile found, create one

   if (bFoundProfile) {
      strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

      if (AppConfig->Open(strIni))
         return;
   }

   strProfile = TEXT("Untitled");
   GlobalConfig->SetString(TEXT("General"), TEXT("Profile"), strProfile);

   strIni.Clear() << lpAppDataPath << TEXT("\\profiles\\") << strProfile << TEXT(".ini");

   if (!AppConfig->Create(strIni))
      CrashError(TEXT("Could not create '%s'"), strIni.Array());

   AppConfig->SetString(TEXT("Audio"), TEXT("Device"), TEXT("Default"));
   AppConfig->SetFloat(TEXT("Audio"), TEXT("MicVolume"), 1.0f);
   AppConfig->SetFloat(TEXT("Audio"), TEXT("DesktopVolume"), 1.0f);

   AppConfig->SetInt(TEXT("Video"), TEXT("Monitor"), 0);
   AppConfig->SetInt(TEXT("Video"), TEXT("FPS"), 30);
   AppConfig->SetFloat(TEXT("Video"), TEXT("Downscale"), 1.0f);
   AppConfig->SetInt(TEXT("Video"), TEXT("DisableAero"), 0);

   AppConfig->SetInt(TEXT("Video Encoding"), TEXT("BufferSize"), 1000);
   AppConfig->SetInt(TEXT("Video Encoding"), TEXT("MaxBitrate"), 1000);
   AppConfig->SetString(TEXT("Video Encoding"), TEXT("Preset"), TEXT("veryfast"));
   AppConfig->SetInt(TEXT("Video Encoding"), TEXT("Quality"), 8);

   AppConfig->SetInt(TEXT("Audio Encoding"), TEXT("Format"), 1);
   AppConfig->SetString(TEXT("Audio Encoding"), TEXT("Bitrate"), TEXT("128"));
   AppConfig->SetInt(TEXT("Audio Encoding"), TEXT("isStereo"), 1);

   AppConfig->SetInt(TEXT("Publish"), TEXT("Service"), 0);
   AppConfig->SetInt(TEXT("Publish"), TEXT("Mode"), 0);
}

int init() {

   // use app data file
   bIsPortable = false;
   if (!HasSSE2Support()) {
      OBSMessageBox(NULL, TEXT("OBS requires an SSE2-compatible CPU."), TEXT("Unsupported CPU"), MB_ICONERROR);
      return 1;
   }

#if defined _M_X64 && _MSC_VER == 1800
   //workaround AVX2 bug in VS2013, http://connect.microsoft.com/VisualStudio/feedback/details/811093
   _set_FMA3_enable(0);
#endif

   //  LPWSTR *args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
   LPWSTR profile = NULL;
   LPWSTR sceneCollection = NULL;

   bool bDisableMutex = false;

   //------------------------------------------------------------
   HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
   SetProcessDEPPolicy(PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
#ifdef _M_X64
#else
   //InitializeExceptionHandler();
#endif // x64

   Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, NULL);

   if (InitXT(NULL, TEXT("FastAlloc"),TEXT("OBS"))) {
      //InitSockets();
      //CoInitializeEx(NULL, COINIT_MULTITHREADED);
      CoInitialize(0);
      EnableProfiling(TRUE);

      //always make sure we're running inside our app folder so that locale files and plugins work
      SetWorkingFolder();

      //get current working dir
      {
         String strDirectory;
         UINT dirSize = GetCurrentDirectory(0, 0);
         strDirectory.SetLength(dirSize);
         GetCurrentDirectory(dirSize, strDirectory.Array());

         scpy(lpAppPath, strDirectory);
      }

      //if -portable isn't specified in command line check if there's a file named "obs_portable_mode" in current working dir, if so, obs goes into portable mode
      if (!bIsPortable) {
         String strPMFileName = lpAppPath;
         strPMFileName += TEXT("\\obs_portable_mode");
         if (OSFileExists(strPMFileName))
            bIsPortable = true;
      }

      TSTR lpAllocator = NULL;

      {
         if (bIsPortable)
            scpy(lpAppDataPath, lpAppPath);
         else {
            SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, lpAppDataPath);
            scat_n(lpAppDataPath, TEXT("\\VhallHelper"), 12);
         }

         if (!OSFileExists(lpAppDataPath) && !OSCreateDirectory(lpAppDataPath))
            CrashError(TEXT("Couldn't create directory '%s'"), lpAppDataPath);

         String strAppDataPath = lpAppDataPath;
         String strProfilesPath = strAppDataPath + TEXT("\\profiles");
         if (!OSFileExists(strProfilesPath) && !OSCreateDirectory(strProfilesPath))
            CrashError(TEXT("Couldn't create directory '%s'"), strProfilesPath.Array());

         String strSceneCollectionPath = strAppDataPath + TEXT("\\sceneCollection");
         if (!OSFileExists(strSceneCollectionPath) && !OSCreateDirectory(strSceneCollectionPath))
            CrashError(TEXT("Couldn't create directory '%s'"), strSceneCollectionPath.Array());

         String strLogsPath = strAppDataPath + TEXT("\\logs");
         if (!OSFileExists(strLogsPath) && !OSCreateDirectory(strLogsPath))
            CrashError(TEXT("Couldn't create directory '%s'"), strLogsPath.Array());

         String strCrashPath = strAppDataPath + TEXT("\\crashDumps");
         if (!OSFileExists(strCrashPath) && !OSCreateDirectory(strCrashPath))
            CrashError(TEXT("Couldn't create directory '%s'"), strCrashPath.Array());

         String strPluginDataPath = strAppDataPath + TEXT("\\pluginData");
         if (!OSFileExists(strPluginDataPath) && !OSCreateDirectory(strPluginDataPath))
            CrashError(TEXT("Couldn't create directory '%s'"), strPluginDataPath.Array());

         String strUpdatePath = strAppDataPath + TEXT("\\updates");
         if (!OSFileExists(strUpdatePath) && !OSCreateDirectory(strUpdatePath))
            CrashError(TEXT("Couldn't create directory '%s'"), strUpdatePath.Array());

         String servicesPath = strAppDataPath + L"\\services";
         if (!OSFileExists(servicesPath) && !OSCreateDirectory(servicesPath))
            CrashError(TEXT("Couldn't create directory '%s'"), servicesPath.Array());

         LoadGlobalIni();

         String strAllocator = GlobalConfig->GetString(TEXT("General"), TEXT("Allocator"));
         if (strAllocator.IsValid()) {
            UINT size = strAllocator.DataLength();
            lpAllocator = (TSTR)malloc(size);
            mcpy(lpAllocator, strAllocator.Array(), size);
         }
      }

      if (lpAllocator) {
         delete GlobalConfig;

         ResetXTAllocator(lpAllocator);
         free(lpAllocator);

         LoadGlobalIni();
      }

      //EnableMemoryTracking(true, 8961);

      //--------------------------------------------   
      GlobalConfig->SetString(TEXT("General"), TEXT("LastAppDirectory"), lpAppPath);

      //--------------------------------------------

      AppConfig = new ConfigFile;
      SetupIni(profile);

      //--------------------------------------------

      String strLogFileWildcard;
      strLogFileWildcard << lpAppDataPath << TEXT("\\logs\\*.log");

      OSFindData ofd;
      HANDLE hFindLogs = OSFindFirstFile(strLogFileWildcard, ofd);
      if (hFindLogs) {
         int numLogs = 0;
         String strFirstLog;

         do {
            if (ofd.bDirectory) continue;
            if (!numLogs++)
               strFirstLog << lpAppDataPath << TEXT("\\logs\\") << ofd.fileName;
         } while (OSFindNextFile(hFindLogs, ofd));

         OSFindClose(hFindLogs);

         if (numLogs >= GlobalConfig->GetInt(TEXT("General"), TEXT("MaxLogs"), 20))
            OSDeleteFile(strFirstLog);
      }

      SYSTEMTIME st;
      GetLocalTime(&st);

      String strLog;
      strLog << lpAppDataPath << FormattedString(TEXT("\\logs\\%u-%02u-%02u-%02u%02u-%02u"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond) << TEXT(".log");

      InitXTLog(strLog);

      //--------------------------------------------

      bDisableComposition = AppConfig->GetInt(TEXT("Video"), TEXT("DisableAero"), 0);

      if (bDisableComposition)
         DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);

      //--------------------------------------------

      strCaptureHookLog=new String();
      *strCaptureHookLog << lpAppDataPath << L"\\pluginData\\captureHookLog.txt";


      pGCHLogMF = OSMonitorFileStart(*strCaptureHookLog, true);


   }
   return 0;
}
void destory() {

   GlobalConfig->SetInt(TEXT("General"), TEXT("LastAppVersion"), OBS_VERSION);

   delete AppConfig;
   delete GlobalConfig;

   if (bDisableComposition)
      DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);

   //TerminateSockets();

   bool skipGCHLog = false;

   if (pGCHLogMF) {
      if (!OSFileHasChanged(pGCHLogMF))
         skipGCHLog = true;

      OSMonitorFileDestroy(pGCHLogMF);
   }

   //FIXME: File monitoring needs fixing.  Half the time game capture logs are not
   //getting attached even when users clearly used it.
   if (true) //!skipGCHLog)
   {
      XFile captureHookLog;

      if (captureHookLog.Open(*strCaptureHookLog, XFILE_READ | XFILE_SHARED, XFILE_OPENEXISTING)) {
         String strContents;
         captureHookLog.ReadFileToString(strContents);
         LogRaw(L"\r\n\r\nLast game capture log:");
         LogRaw(strContents.Array(), strContents.Length());
      }
   }


   Gdiplus::GdiplusShutdown(gdipToken);
   strCaptureHookLog->Clear();
   delete strCaptureHookLog;
   strCaptureHookLog=NULL;
   TerminateXT();

   //------------------------------------------------------------

   CloseHandle(hOBSMutex);

   // LocalFree(args);
}
