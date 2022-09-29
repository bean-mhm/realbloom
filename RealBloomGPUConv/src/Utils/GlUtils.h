#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <GL/GLU.h>

#include <iostream>
#include <vector>
#include <string>

#include "Misc.h"

bool createShader(const char* shaderName, GLenum shaderType, const char* shaderSource, GLuint& outShaderID);
bool checkShader(const char* shaderName, GLuint shaderID);

// returns true if we're good to go, false if there are errors
bool checkGlErrors(std::string& outErrors);