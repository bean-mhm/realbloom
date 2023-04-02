#pragma once

#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

class GlVertexBuffer : public GlWrapper
{
private:
    GLuint m_vbo = 0;

public:
    GlVertexBuffer();
    ~GlVertexBuffer();

    GLuint getVBO() const;

    void upload(const void* data, GLsizeiptr size, GLenum usage = GL_STATIC_DRAW);
    void bind();

};
