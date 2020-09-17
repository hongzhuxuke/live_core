#include "VHGLContext.h"
#include "../log.h"

const static bool FULL_SCREEN = false;
const static bool VSYNC_ENABLED = true;
const static float SCREEN_DEPTH = 1000.0f;
const static float SCREEN_NEAR = 0.1f;

namespace vhall {

  VHGLContext::VHGLContext() {
    mOpenGL = nullptr;
  }

  VHGLContext::VHGLContext(const VHGLContext& other) {
  }

  VHGLContext::~VHGLContext() {
    Shutdown();
  }

  bool VHGLContext::Initialize() {
    bool result = 0;
    // Initialize the width and height of the screen to zero.
    int screenWidth = 100;
    int screenHeight = 100;

    // Create the OpenGL object.
    mOpenGL.reset(new VHOpenGLBase());
    if (!mOpenGL) {
      return false;
    }

    // Create the window the application will be using and also initialize OpenGL.
    result = InitializeWindows(mOpenGL, screenWidth, screenHeight);
    if (!result) {
      LOGE("Could not initialize the window.");
      return false;
    }

    return true;
  }

  void VHGLContext::Shutdown() {
    // Release the OpenGL object.
    if (mOpenGL) {
      mOpenGL->Shutdown(mHwnd);
      mOpenGL.reset();
    }

    // Shutdown the window.
    ShutdownWindows();
    return;
  }

  void VHGLContext::Run() {
    MSG msg;
    bool done, result;

    // Initialize the message structure.
    ZeroMemory(&msg, sizeof(MSG));

    // Loop until there is a quit message from the window or the user.
    done = false;
    while (!done) {
      // Handle the windows messages.
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // If windows signals to end the application then exit out.
      if (msg.message == WM_QUIT) {
        done = true;
      }
      else {
        // Otherwise do the frame processing.
        result = Frame();
        if (!result) {
          done = true;
        }
      }
    }

    return;
  }

  bool VHGLContext::Frame() {
    // Do the frame processing for the graphics object.
    mOpenGL->EndScene();
    return true;
  }

  LRESULT CALLBACK VHGLContext::MessageHandler(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProc(hwnd, umsg, wparam, lparam);
  }

  bool VHGLContext::InitializeWindows(std::shared_ptr<VHOpenGLBase> OpenGL, int& screenWidth, int& screenHeight) {
    WNDCLASSEX wc;
    DEVMODE dmScreenSettings;
    int posX, posY;
    bool result;

    // Get an external pointer to this object.	
    ApplicationHandle = this;

    // Get the instance of this application.
    mHinstance = GetModuleHandle(NULL);

    // Give the application a name.
    mApplicationName = L"Engine";

    // Setup the windows class with default settings.
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = mHinstance;
    wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = mApplicationName;
    wc.cbSize = sizeof(WNDCLASSEX);

    // Register the window class.
    RegisterClassEx(&wc);

    // Create a temporary window for the OpenGL extension setup.
    mHwnd = CreateWindowEx(WS_EX_APPWINDOW, mApplicationName, mApplicationName, WS_POPUP, 0, 0, 1080, 1440, NULL, NULL, mHinstance, NULL);
    if (mHwnd == NULL) {
      return false;
    }

    // Don't show the window.
    ShowWindow(mHwnd, SW_HIDE);

    // Initialize a temporary OpenGL window and load the OpenGL extensions.
    result = OpenGL->InitializeExtensions(mHwnd);
    if (!result) {
      LOGE("Could not initialize the OpenGL extensions.");
      return false;
    }

    // Release the temporary window now that the extensions have been initialized.
    DestroyWindow(mHwnd);
    mHwnd = NULL;

    // Determine the resolution of the clients desktop screen.
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    screenWidth = 100;
    screenHeight = 100;
    // Setup the screen settings depending on whether it is running in full screen or in windowed mode.
    if (FULL_SCREEN) {
      // If full screen set the screen to maximum size of the users desktop and 32bit.
      memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
      dmScreenSettings.dmSize = sizeof(dmScreenSettings);
      dmScreenSettings.dmPelsWidth = (unsigned long)screenWidth;
      dmScreenSettings.dmPelsHeight = (unsigned long)screenHeight;
      dmScreenSettings.dmBitsPerPel = 32;
      dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

      // Change the display settings to full screen.
      ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

      // Set the position of the window to the top left corner.
      posX = posY = 0;
    }
    else {
      // If windowed then set it to 800x600 resolution.
      screenWidth = 100;
      screenHeight = 100;

      // Place the window in the middle of the screen.
      posX = (GetSystemMetrics(SM_CXSCREEN) - screenWidth) / 2;
      posY = (GetSystemMetrics(SM_CYSCREEN) - screenHeight) / 2;
    }

    // Create the window with the screen settings and get the handle to it.
    mHwnd = CreateWindowEx(WS_EX_APPWINDOW, mApplicationName, mApplicationName, WS_POPUP,
      posX, posY, screenWidth, screenHeight, NULL, NULL, mHinstance, NULL);
    if (mHwnd == NULL) {
      return false;
    }

    // Initialize OpenGL now that the window has been created.
    result = mOpenGL->InitializeOpenGL(mHwnd, screenWidth, screenHeight, SCREEN_DEPTH, SCREEN_NEAR, VSYNC_ENABLED);
    if (!result) {
      LOGE("Could not initialize OpenGL, check if video card supports OpenGL 4");
      return false;
    }

    // Bring the window up on the screen and set it as main focus.
    ShowWindow(mHwnd, SW_HIDE);
    SetForegroundWindow(mHwnd);
    SetFocus(mHwnd);

    // Hide the mouse cursor.
    ShowCursor(false);

    return true;
  }

  void VHGLContext::ShutdownWindows() {
    // Show the mouse cursor.
    ShowCursor(true);

    // Fix the display settings if leaving full screen mode.
    if (FULL_SCREEN) {
      ChangeDisplaySettings(NULL, 0);
    }

    // Remove the window.
    DestroyWindow(mHwnd);
    mHwnd = NULL;

    // Remove the application instance.
    UnregisterClass(mApplicationName, mHinstance);
    mHinstance = NULL;

    // Release the pointer to this class.
    ApplicationHandle = NULL;

    return;
  }

  LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam) {
    switch (umessage) {
      // Check if the window is being closed.
    case WM_CLOSE:
    {
      PostQuitMessage(0);
      return 0;
    }

    // All other messages pass to the message handler in the system class.
    default:
    {
      return ApplicationHandle->MessageHandler(hwnd, umessage, wparam, lparam);
    }
    }
  }

} // namespace vhall