#include "GlTexture.h"

GlTexture::GlTexture(uint32_t width, uint32_t height, GLenum wrap, GLenum minFilter, GLenum magFilter, GLint internalFormat)
    : m_width(width), m_height(height), m_internalFormat(internalFormat)
{
    glGenTextures(1, &m_texture);
    if (!checkGlStatus(__FUNCTION__, "glGenTextures")) return;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (!checkGlStatus(__FUNCTION__, "glBindTexture")) return;

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap); // Same
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    if (!checkGlStatus(__FUNCTION__, "glTexParameteri")) return;

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (!checkGlStatus(__FUNCTION__, "glPixelStorei")) return;
#endif
}

GlTexture::~GlTexture()
{
    glDeleteTextures(1, &m_texture);
    if (!checkGlStatus(__FUNCTION__, "glDeleteTextures")) return;
}

GLuint GlTexture::getTexture() const
{
    return m_texture;
}

void GlTexture::upload(float* data)
{
    if (hasFailed())
        return;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (!checkGlStatus(__FUNCTION__, "glBindTexture")) return;

    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, GL_RGBA, GL_FLOAT, data);
    if (!checkGlStatus(__FUNCTION__, "glTexImage2D")) return;
}

void GlTexture::bind(GLenum texUnit)
{
    if (hasFailed())
        return;

    glActiveTexture(texUnit);
    if (!checkGlStatus(__FUNCTION__, "glActiveTexture")) return;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (!checkGlStatus(__FUNCTION__, "glBindTexture")) return;
}

void GlTexture::bind()
{
    if (hasFailed())
        return;

    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (!checkGlStatus(__FUNCTION__, "glBindTexture")) return;
}
