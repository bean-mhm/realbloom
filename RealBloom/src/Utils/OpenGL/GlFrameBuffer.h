#pragma once

#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"
#include "../Misc.h"

// OpenGL Framebuffer Wrapper
class GlFramebuffer : public GlWrapper
{
public:
    GlFramebuffer(uint32_t width, uint32_t height);
    virtual ~GlFramebuffer();

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    GLuint getFramebuffer() const;
    GLuint getColorBuffer() const;

    void bind();
    void unbind();
    void viewport();

private:
    GLuint m_framebuffer = 0;
    GLuint m_texColorBuffer = 0;

    uint32_t m_width;
    uint32_t m_height;

};

