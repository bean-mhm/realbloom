#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>
#include <GL/wglew.h>

#include <windows.h>
#include <GL/GL.h>
#include <stdint.h>
#include <functional>
#include <string>

#include "Misc.h"
#include "RandomNumber.h"

bool oglOneTimeContext(
    uint32_t width,
    uint32_t height,
    void* data,
    std::function<void(void*)> job,
    std::function<void(std::string)> errHandler,
    std::string& outError);