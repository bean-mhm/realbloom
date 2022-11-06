#pragma once

#include <stdint.h>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

class GlFrameBuffer : public GlWrapper
{
private:
    GLuint m_frameBuffer = 0;
    GLuint m_texColorBuffer = 0;

    uint32_t m_width;
    uint32_t m_height;

public:
    GlFrameBuffer(uint32_t width, uint32_t height);
    virtual ~GlFrameBuffer();

    bool bind();
    bool unbind();
    GLuint getColorBuffer() const;
};

