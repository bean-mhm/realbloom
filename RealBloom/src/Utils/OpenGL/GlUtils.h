#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>

#include "../Misc.h"

constexpr float GL_COORD_SCALE = 2.0f;

constexpr const char* GL_BASE_VERTEX_SOURCE = R"glsl(
    #version 150 core

    in vec2 pos;
    in vec2 texUV;

    out vec2 vTexUV;

    void main()
    {
        gl_Position = vec4(pos, 0.0, 1.0);
        vTexUV = texUV;
    }
)glsl";

// Base class for OpenGL wrapper classes
class GlWrapper
{
public:
    GlWrapper();
    virtual ~GlWrapper();
};

bool createShader(GLenum shaderType, const char* shaderSource, GLuint& outShaderID, std::string& outLog);
bool checkShader(GLuint shaderID, std::string& outLog);

void checkGlStatus(const std::string& source, const std::string& stage);
void clearGlStatus();

void findAndSetUniform1i(GLuint program, const char* name, GLint v0);
void findAndSetUniform1f(GLuint program, const char* name, GLfloat v0);
void findAndSetUniform2f(GLuint program, const char* name, GLfloat v0, GLfloat v1);
void findAndSetUniform3f(GLuint program, const char* name, GLfloat v0, GLfloat v1, GLfloat v2);
void findAndSetUniform4f(GLuint program, const char* name, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
