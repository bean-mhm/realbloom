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
#include <cstdint>

#include "../Misc.h"

bool oglOneTimeContext(int versionMajor, int versionMinor, std::function<void()> job, std::string& outError);
