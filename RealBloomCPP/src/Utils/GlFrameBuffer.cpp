#include "GlFrameBuffer.h"

GlFrameBuffer::GlFrameBuffer(uint32_t width, uint32_t height)
    : m_width(width), m_height(height)
{
    glGenFramebuffers(1, &m_frameBuffer);
    if (!checkGlStatus(__FUNCTION__, "glGenFramebuffers")) return;

    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    if (!checkGlStatus(__FUNCTION__, "glBindFramebuffer")) return;

    // Make a color buffer for our frame buffer (render target)
    glGenTextures(1, &m_texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, m_texColorBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
    if (!checkGlStatus(__FUNCTION__, "Color Buffer")) return;

    // Assign the color buffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texColorBuffer, 0);
    if (!checkGlStatus(__FUNCTION__, "glFramebufferTexture2D")) return;

    GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (!checkGlStatus(__FUNCTION__, "glCheckFramebufferStatus")) return;

    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        setError(__FUNCTION__, stringFormat("Framebuffer was not ready. Status: %s", toHex(fbStatus)));
        return;
    }
}

GlFrameBuffer::~GlFrameBuffer()
{
    unbind();
    glDeleteTextures(1, &(m_texColorBuffer));
    glDeleteFramebuffers(1, &(m_frameBuffer));
}

bool GlFrameBuffer::bind()
{
    if (hasFailed())
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    return checkGlStatus(__FUNCTION__, "glBindFramebuffer");
}

bool GlFrameBuffer::unbind()
{
    if (hasFailed())
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    checkGlStatus(__FUNCTION__, "glBindFramebuffer");

    glDrawBuffer(GL_BACK);
    checkGlStatus(__FUNCTION__, "glDrawBuffer");

    return !hasFailed();
}

GLuint GlFrameBuffer::getColorBuffer() const
{
    return m_texColorBuffer;
}
