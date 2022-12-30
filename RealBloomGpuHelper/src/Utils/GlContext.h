#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>
#include <GL/wglew.h>

#include <Windows.h>
#include <GL/GL.h>

#include <string>
#include <functional>
#include <stdint.h>

#include "Utils/Misc.h"
#include "Utils/RandomNumber.h"

enum class GlContextVersion
{
    GL_3_2,
    GL_4_3
};

bool oglOneTimeContext(GlContextVersion glVersion, std::function<void()> job, std::string& outError);
