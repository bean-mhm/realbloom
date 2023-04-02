#pragma once

#include <memory>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>

#include "GlVertexArray.h"
#include "GlVertexBuffer.h"
#include "GlUtils.h"

class GlFullPlaneVertices
{
private:
    static std::shared_ptr<GlVertexArray> m_vao;
    static std::shared_ptr<GlVertexBuffer> m_vbo;

    static void ensureInit();

public:
    GlFullPlaneVertices() = delete;
    GlFullPlaneVertices(const GlFullPlaneVertices&) = delete;
    GlFullPlaneVertices& operator= (const GlFullPlaneVertices&) = delete;

    static void cleanUp();

    static void enable(
        GLuint program,
        GLsizei stride = 4 * sizeof(GLfloat),
        const void* posAttribOffset = 0,
        const void* texAttribOffset = (void*)(2 * sizeof(GLfloat)),
        const char* posAttribName = "pos",
        const char* texAttribName = "texUV");

    static void disable(
        GLuint program,
        const char* posAttribName = "pos",
        const char* texAttribName = "texUV");

};
