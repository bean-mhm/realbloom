#include "GlTexture.h"

GlTexture::GlTexture(uint32_t width, uint32_t height, GLenum wrap, GLenum minFilter, GLenum magFilter, GLint internalFormat)
    : m_width(width), m_height(height), m_internalFormat(internalFormat)
{
    glGenTextures(1, &m_texture);
    checkGlStatus(__FUNCTION__, "glGenTextures");

    glBindTexture(GL_TEXTURE_2D, m_texture);
    checkGlStatus(__FUNCTION__, "glBindTexture");

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap); // Same
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    checkGlStatus(__FUNCTION__, "glTexParameteri");

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    checkGlStatus(__FUNCTION__, "glPixelStorei");
#endif
}

GlTexture::~GlTexture()
{
    glDeleteTextures(1, &m_texture);
    checkGlStatus(__FUNCTION__, "glDeleteTextures");
}

GLuint GlTexture::getTexture() const
{
    return m_texture;
}

void GlTexture::upload(float* data)
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    checkGlStatus(__FUNCTION__, "glBindTexture");

    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, GL_RGBA, GL_FLOAT, data);
    checkGlStatus(__FUNCTION__, "glTexImage2D");
}

void GlTexture::bind(GLenum texUnit)
{
    glActiveTexture(texUnit);
    checkGlStatus(__FUNCTION__, "glActiveTexture");

    glBindTexture(GL_TEXTURE_2D, m_texture);
    checkGlStatus(__FUNCTION__, "glBindTexture");
}

void GlTexture::bind()
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    checkGlStatus(__FUNCTION__, "glBindTexture");
}
