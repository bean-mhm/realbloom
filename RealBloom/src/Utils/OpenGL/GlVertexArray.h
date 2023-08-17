#pragma once

#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

// OpenGL Vertex Array Wrapper
class GlVertexArray : public GlWrapper
{
public:
    GlVertexArray();
    ~GlVertexArray();

    GLuint getVAO() const;

    void bind();
    void unbind();

private:
    GLuint m_vao = 0;

};
