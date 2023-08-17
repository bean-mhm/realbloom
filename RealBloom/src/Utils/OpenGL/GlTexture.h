#pragma once

#include <array>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"

// OpenGL Texture Wrapper
class GlTexture : public GlWrapper
{
public:
    GlTexture(
        uint32_t width,
        uint32_t height,
        GLenum wrap = GL_CLAMP_TO_BORDER,
        GLenum minFilter = GL_LINEAR,
        GLenum magFilter = GL_LINEAR,
        GLint internalFormat = GL_RGBA32F);
    ~GlTexture();

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    GLuint getTexture() const;

    // RGBA32F
    void upload(const float* data);
    void bind(GLenum texUnit);
    void bind();
    void setBorderColor(std::array<float, 4> color);

private:
    uint32_t m_width;
    uint32_t m_height;

    GLuint m_texture = 0;
    GLint m_internalFormat = 0;

};
