#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "GL/glew.h"
#include "GL/glut.h"
#include "GL/wglew.h"
#include "log.h"

#pragma warning(disable : 4996)

namespace vhall {
  class VHOpenGLUtil {
  public:
    static void loadTexture(GLubyte* data, int width, int height, GLuint& usedTexId) {
      if (usedTexId == -1) {
        glGenTextures(1, &usedTexId);
        glBindTexture(GL_TEXTURE_2D, usedTexId);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, /*GL_RGB*/GL_BGR_EXT, GL_UNSIGNED_BYTE, data);
      }
      else {
        glBindTexture(GL_TEXTURE_2D, usedTexId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, /*GL_RGB*/GL_BGR_EXT, GL_UNSIGNED_BYTE, data);
      }
      return;
    }
    static void InitFBO(GLuint & textureId, GLuint & renderbufferId, GLuint & framebufferId, int width, int height) {
      /* Create texture obj */
      glGenTextures(1, &textureId);
      glBindTexture(GL_TEXTURE_2D, textureId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
    
      /* create a renderbuffer obj to store depth info */
      glGenRenderbuffers(1, &renderbufferId);
      glBindRenderbuffer(GL_RENDERBUFFER, renderbufferId);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      /* create a framebuffer object */
      glGenFramebuffers(1, &framebufferId);
      glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);

      /* attach the texture to FBO color attachment point */
      glFramebufferTexture2D(
        GL_FRAMEBUFFER,        // 1. fbo target: GL_FRAMEBUFFER 
        GL_COLOR_ATTACHMENT0,  // 2. attachment point
        GL_TEXTURE_2D,         // 3. tex target: GL_TEXTURE_2D
        textureId,             // 4. tex ID
        0);                    // 5. mipmap level: 0(base)

      /* attach the renderbuffer to depth attachment point */
      glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,      // 1. fbo target: GL_FRAMEBUFFER
        GL_DEPTH_ATTACHMENT, // 2. attachment point
        GL_RENDERBUFFER,     // 3. rbo target: GL_RENDERBUFFER
        renderbufferId);     // 4. rbo ID

      /* check FBO status */
      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("glCheckFramebufferStatus error");
      }
      /* switch back to window-system-provided framebuffer */
      //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
  };
};