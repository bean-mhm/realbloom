#pragma once

#include <stdint.h>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

class GlTexture : public GlWrapper
{
private:
    uint32_t m_width;
    uint32_t m_height;

    GLuint m_texture = 0;
    GLint m_internalFormat = 0;

public:
    GlTexture(
        uint32_t width,
        uint32_t height,
        GLenum wrap = GL_CLAMP,
        GLenum minFilter = GL_LINEAR,
        GLenum magFilter = GL_LINEAR,
        GLint internalFormat = GL_RGBA32F);
    ~GlTexture();

    GLuint getTexture() const;

    // RGBA32F
    void upload(float* data);
    void bind(GLenum texUnit);
    void bind();
};

