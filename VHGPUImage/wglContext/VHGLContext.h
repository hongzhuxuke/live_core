#pragma once
#include <windows.h>
#include <iostream>
#include "VHOpenGLBase.h"
#define WIN32_LEAN_AND_MEAN

namespace vhall {
  class VHGLContext {
  public:
    VHGLContext();
    VHGLContext(const VHGLContext&);
    ~VHGLContext();

    bool Initialize();
    void Shutdown();
    void Run();
    LRESULT CALLBACK MessageHandler(HWND, UINT, WPARAM, LPARAM);
  private:
    bool Frame();
    bool InitializeWindows(std::shared_ptr<VHOpenGLBase>, int&, int&);
    void ShutdownWindows();

  private:
    LPCWSTR mApplicationName;
    HINSTANCE mHinstance;
    HWND mHwnd;

    std::shared_ptr<VHOpenGLBase> mOpenGL;
  };

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  static VHGLContext* ApplicationHandle = 0;
}