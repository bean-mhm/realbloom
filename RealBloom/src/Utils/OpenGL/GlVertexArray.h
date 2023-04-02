#pragma once

#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

class GlVertexArray : public GlWrapper
{
private:
    GLuint m_vao = 0;

public:
    GlVertexArray();
    ~GlVertexArray();

    GLuint getVAO() const;

    void bind();
    void unbind();

};
