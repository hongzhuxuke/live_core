// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "Logging.h"
//Logger *g_pLogger = NULL;
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
      //g_pLogger = new Logger(L"MediaReader.log", CURRENT);
      break;
	case DLL_THREAD_ATTACH:
   
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
      //if(g_pLogger)
      {
        // delete g_pLogger;
        // g_pLogger=NULL;
      }
		break;
	}
	return TRUE;
}


