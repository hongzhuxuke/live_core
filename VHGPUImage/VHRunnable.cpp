#include <iostream>
#include "VHRunnable.h"
#include "VHShaderUtil.h"
namespace  vhall {
  VHRunnable1i::VHRunnable1i(int location, int intValue) {
    this->location = location;
    this->intValue = intValue;
  }

  void VHRunnable1i::run() {
    glUniform1i(location, intValue);
    VHShaderUtil::checkGlError("Runnable1i");
  }

  VHRunnable1f::VHRunnable1f(int location, float floatValue) {
    this->location = location;
    this->floatValue = floatValue;
  }

  void VHRunnable1f::run() {
    glUniform1f(location, floatValue);
    VHShaderUtil::checkGlError("Runnable1f");
  }


  VHRunnable2fv::VHRunnable2fv(int location, float* valueptr) {
    this->loc = location;
    this->ptr = valueptr;
  }

  void VHRunnable2fv::run() {
    glUniform2fv(loc, 1, ptr);
    VHShaderUtil::checkGlError("Runnable2fv");
  }


  VHRunnable3fv::VHRunnable3fv(int location, float* valueptr) {
    this->loc = location;
    this->ptr = valueptr;
  }

  void VHRunnable3fv::run() {
    glUniform3fv(loc, 1, ptr);
    VHShaderUtil::checkGlError("Runnable3fv");
  }

  VHRunnable4fv::VHRunnable4fv(int location, float* valueptr) {
    this->loc = location;
    this->ptr = valueptr;
  }

  void VHRunnable4fv::run() {
    glUniform4fv(loc, 1, ptr);
    VHShaderUtil::checkGlError("Runnable4fv");
  }


  VHRunnableXfv::VHRunnableXfv(int location, float* valueptr, int len) {
    this->loc = location;
    this->ptr = valueptr;
    this->len = len;
  }

  void VHRunnableXfv::run() {
    glUniform1fv(loc, len, ptr);
    VHShaderUtil::checkGlError("RunnableXfv");
  }

  VHRunnablePt::VHRunnablePt(int location, float x, float y) {
    this->loc = location;
    this->x = x;
    this->y = y;
  }

  void VHRunnablePt::run() {
    float vec2[2];
    vec2[0] = x;
    vec2[1] = y;
    glUniform2fv(loc, 1, vec2);
    VHShaderUtil::checkGlError("RunnablePt");
  }

  VHRunnableMatrix3f::VHRunnableMatrix3f(int location, float* matrix) {
    this->loc = location;
    this->matrix = matrix;
  }

  void VHRunnableMatrix3f::run() {
    glUniformMatrix3fv(loc, 1, false, matrix);
    VHShaderUtil::checkGlError("RunnableMatrix3f");
  }

  VHRunnableMatrix4f::VHRunnableMatrix4f(int location, float* matrix) {
    this->loc = location;
    this->matrix = matrix;
  }

  void VHRunnableMatrix4f::run() {
    glUniformMatrix4fv(loc, 1, false, matrix);
    VHShaderUtil::checkGlError("glUniformMatrix4fv");
  }
}