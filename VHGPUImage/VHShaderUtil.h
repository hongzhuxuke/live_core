#pragma once
#include <iostream>
#include <string>
namespace vhall{
  class VHShaderUtil {
  public:
    VHShaderUtil();
    ~VHShaderUtil();
    static int loadShader(int shaderType, std::string source);
    static int createProgram(std::string vertexSource, std::string fragmentSource);
    static void checkGlError(std::string op);
  };

}