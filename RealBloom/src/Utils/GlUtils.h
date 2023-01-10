#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>

#include <iostream>
#include <vector>
#include <string>

#include "Misc.h"

constexpr float GL_COORD_SCALE = 2.0f;

class GlWrapper
{
public:
    GlWrapper();
    virtual ~GlWrapper();
};

bool createShader(GLenum shaderType, const char* shaderSource, GLuint& outShaderID, std::string& outLog);
bool checkShader(GLuint shaderID, std::string& outLog);

void checkGlStatus(const std::string& source, const std::string& stage);