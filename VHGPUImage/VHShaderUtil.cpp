#include "VHShaderUtil.h"
#include "GL/glew.h"
#include "GL/glut.h"
#include "log.h"

namespace vhall
{
  VHShaderUtil::VHShaderUtil() {
  }

  VHShaderUtil::~VHShaderUtil() {
  }

  int VHShaderUtil::loadShader(int shaderType, std::string source) {
    GLuint shader;
    GLint compiled;
    shader = glCreateShader(shaderType);
    if (shader == 0) {
      return 0;
    }

    const GLchar* srccode = source.c_str();
    glShaderSource(shader, 1, &srccode, NULL);
    checkGlError("glShaderSource");

    glCompileShader(shader);
    checkGlError("glCompileShader");

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (compiled == 0) {
      GLint infoLen = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
      if (infoLen > 0)
      {
        char *infoLog = new char[infoLen + 1];
        glGetShaderInfoLog(shader, infoLen + 1, NULL, infoLog);
        LOGE("Could not compile shader, type: %d, info: %s", shaderType, infoLog);
        delete[]infoLog;
      }
      glDeleteShader(shader);
      return 0;
    }

    return shader;
  }

  int VHShaderUtil::createProgram(std::string vertexSource, std::string fragmentSource) {
    int vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
      return 0;
    }

    int pixelShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (pixelShader == 0) {
      glDeleteShader(vertexShader);
      return 0;
    }

    int program = glCreateProgram();
    if (program != 0) {
      glAttachShader(program, vertexShader);
      checkGlError("glAttachShader");
      glAttachShader(program, pixelShader);
      checkGlError("glAttachShader");
      glLinkProgram(program);
      int linkStatus;
      glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
      if (linkStatus == 0) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
          char *infoLog = new char[infoLen + 1];
          glGetProgramInfoLog(program, infoLen + 1, NULL, infoLog);
          LOGE("Could not link program, error: %s", infoLog);
          delete[]infoLog;
        }
        glDeleteProgram(program);
        return 0;
      }
    }

    return program;
  }
  void VHShaderUtil::checkGlError(std::string op) {
    int error;
    for(error; (error = glGetError()) != GL_NO_ERROR;) {
      std::string msg;
      switch (error) {
      case GL_INVALID_ENUM:
        msg = "invalid enum";
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        msg = "invalid framebuffer operation"; 
        break;
      case GL_INVALID_OPERATION:
        msg = "invalid operation";
        break;
      case GL_INVALID_VALUE:
        msg = "invalid value";
        break;
      case GL_OUT_OF_MEMORY:
        msg = "out of memory";
        break;
      default: msg = "unknown error";
        break;
      }
      LOGE("After tag %s, glGetError %s(0x%x) ", op.c_str(), msg.c_str(), error);
      break;
    }
  }
}


