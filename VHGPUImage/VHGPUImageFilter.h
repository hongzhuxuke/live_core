#pragma once
#include <iostream>
#include <string>
#include <list>
#include <mutex>
#include "GL/glew.h"
#include "GL/glut.h"
#include "VhRunnable.h"

namespace vhall {
  class VHGPUImageFilter {
  public:
    VHGPUImageFilter();
    VHGPUImageFilter(std::string vertexShader, std::string fragmentShader);
    virtual ~VHGPUImageFilter();
    void init();
    void ifNeedInit();
    void destroy();
    virtual void onOutputSizeChanged(int width, int height);
    virtual void onDraw(int textureId, const GLfloat* cubeBuffer, const GLfloat* textureBuffer);

    bool isInitialized() {
      return bInitialized;
    }

    int getOutputWidth() {
      return outputWidth;
    }

    int getOutputHeight() {
      return outputHeight;
    }

    int getProgram() {
      return glProgId;
    }

    int getAttribPosition() {
      return glAttribPosition;
    }

    int getAttribTextureCoordinate() {
      return glAttribTextureCoordinate;
    }

    int getUniformTexture() {
      return glUniformTexture;
    }

  public:
    const static float cubes[8];
    const static float textureCoords[8];
  protected:
    static std::string NO_FILTER_VERTEX_SHADER;
    static std::string NO_FILTER_FRAGMENT_SHADER;

    virtual void onInit();
    virtual void onInitialized();
    virtual void onDestroy();
    virtual void onDrawArraysPre();
    void runPendingOnDrawTasks();

    void setInteger(int location, int intValue);
    void setFloat(int location, float floatValue);
    void setFloatVec2(int location, float* arrayValue);
    void setFloatVec3(int location, float* arrayValue);
    void setFloatVec4(int location, float* arrayValue);
    void setFloatArray(int location, float* arrayValue, int len);
    void setPoint(int location, float x, float y);
    void setUniformMatrix3f(int location, float* matrix);
    void setUniformMatrix4f(int location, float* matrix);
    void appendRunOnDraw(vhall::IRunnable* r);

    void checkGLError(const std::string& op);
  private:
    std::mutex  runOnDrawLock;
    std::list<vhall::IRunnable*> runOnDraw;

    GLuint vao;
    GLuint vbo;
    std::string vertexShader;
    std::string fragmentShader;
    int glProgId;
    int glAttribPosition;
    int glUniformTexture;
    int glAttribTextureCoordinate;
    int outputWidth;
    int outputHeight;
    bool bInitialized;
  };
}