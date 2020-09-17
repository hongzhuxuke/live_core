#pragma once

#include <vector>

#ifdef  VH_DESKTOP_CAPTURE_EXPORT
#define VH_DESKTOP_CAPTURE_EXPORT     __declspec(dllimport)
#else
#define VH_DESKTOP_CAPTURE_EXPORT     __declspec(dllexport)
#endif

namespace vlive_desktopcatpure{

enum class CaputreResult {
   // The frame was captured successfully.
   CAPUTRE_SUCCESS,
   // There was a temporary error. The caller should continue calling
   // CaptureFrame(), in the expectation that it will eventually recover.
   CAPUTRE_ERROR_TEMPORARY,
   // Capture has failed and will keep failing if the caller tries calling
   // CaptureFrame() again.
   CAPUTRE_ERROR_PERMANENT,
   CAPUTRE_MAX_VALUE = CAPUTRE_ERROR_PERMANENT
};

// Interface that must be implemented by the DesktopCapturer consumers.
class CaputreCallback {
public:
   // Called after a frame has been captured. |frame| is not nullptr if and
   // only if |result| is SUCCESS.
   virtual void OnCaptureResult(CaputreResult result, const unsigned char*, int width, int height) = 0;
};

class VHDesktopCaptureInterface {
public:
   virtual ~VHDesktopCaptureInterface(){};
   virtual int Init() = 0;
   virtual void Start(CaputreCallback *) = 0;
   virtual void GetScreenSource(std::vector<int> screens) = 0;
   virtual void SetScreenSoucce(const int screen_id) = 0;
   virtual void CaputreFrame() = 0;
};


VH_DESKTOP_CAPTURE_EXPORT VHDesktopCaptureInterface* GetVHDesktopCaptureInstance();
VH_DESKTOP_CAPTURE_EXPORT void DestoryHDesktopCaptureInstance(VHDesktopCaptureInterface* obj);
}