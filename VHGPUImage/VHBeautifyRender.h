#pragma once
#include <iostream>
#include <atomic>
#include <mutex>
#include <list>
#include <vector>
#include "VHBeautifyFilter.h"
#include "VHGPUImageBrightnessFilter.h"
#include "talk/base/messagehandler.h"

namespace talk_base {
   class Thread;
}

namespace vhall {
  typedef std::function<void(std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t ts)> FrameCB;

  class VHGLContext;

  class VHBeautifyRender :public talk_base::MessageHandler {
  public:
    VHBeautifyRender(int level, const FrameCB cb);
    virtual ~VHBeautifyRender();

    void createInstance(int width, int height);

    void init(int width, int height);
    void ifNeedInit(int width, int height);
    void destroy();
    void onOutputSizeChanged(int width, int height);
    
    /* process RGB video data */
    void loadFrame(std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t ts);
    void SetBeautifyLevel(int level);

    bool isInitialized() {
      return bInitialized;
    }
    enum {
      TaskGLInit,
      TaskGLLoadFrame,
      TaskGLReadFrame,
      TaskGLDestroy,
      TaskGLSetLevel,
    };
  protected:
    virtual void OnMessage(talk_base::Message* msg);
  private:
    void OnLoadFrame(std::shared_ptr<unsigned char[]> data, int length, int width, int height, int64_t ts);
    void OnReadFrameBuffer(std::shared_ptr<unsigned char[]> data, int widht, int height, int64_t ts);
    void OnSetBeautifyLevel(int level);
    void OnDestroy();

    bool bInitialized = false;
    int level = 4;
    
    std::shared_ptr<VHGLContext> glContext = nullptr;
    
    std::shared_ptr<vhall::VHBeautifyFilter> filter = nullptr;
    GLuint loadTextureId = -1;
    GLuint fboTextureId = -1;
    GLuint renderbufferId = -1;
    GLuint framebufferId = -1;
    GLuint pboIds[2] = {0};

    std::vector<FrameCB> frame_cb_;
    std::shared_ptr<talk_base::Thread> mThread;
    std::mutex countMtx;
    int countToProcess = 0;
  };

}