#include "GlFrameBuffer.h"

GlFrameBuffer::GlFrameBuffer(uint32_t width, uint32_t height)
    : m_width(width), m_height(height)
{
    glGenFramebuffers(1, &m_frameBuffer);
    checkGlStatus(__FUNCTION__, "glGenFramebuffers");

    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    checkGlStatus(__FUNCTION__, "glBindFramebuffer");

    // Make a color buffer for our frame buffer (render target)
    glGenTextures(1, &m_texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, m_texColorBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
    checkGlStatus(__FUNCTION__, "Color Buffer");

    // Assign the color buffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texColorBuffer, 0);
    checkGlStatus(__FUNCTION__, "glFramebufferTexture2D");

    GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGlStatus(__FUNCTION__, "glCheckFramebufferStatus");

    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
        throw std::exception(
            makeError(__FUNCTION__, "", strFormat("Frame buffer was not ready. Status: %s", toHexStr(fbStatus).c_str())).c_str()
        );
}

GlFrameBuffer::~GlFrameBuffer()
{
    try
    {
        unbind();
    }
    catch (const std::exception& e)
    {
        printError(__FUNCTION__, "unbind", e.what());
    }

    glDeleteTextures(1, &(m_texColorBuffer));
    glDeleteFramebuffers(1, &(m_frameBuffer));
    clearGlStatus();
}

uint32_t GlFrameBuffer::getWidth() const
{
    return m_width;
}

uint32_t GlFrameBuffer::getHeight() const
{
    return m_height;
}

GLuint GlFrameBuffer::getFrameBuffer() const
{
    return m_frameBuffer;
}

GLuint GlFrameBuffer::getColorBuffer() const
{
    return m_texColorBuffer;
}

void GlFrameBuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
    checkGlStatus(__FUNCTION__, "glBindFramebuffer");
}

void GlFrameBuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    checkGlStatus(__FUNCTION__, "glBindFramebuffer");

    glDrawBuffer(GL_BACK);
    checkGlStatus(__FUNCTION__, "glDrawBuffer");
}

void GlFrameBuffer::viewport()
{
    glViewport(0, 0, m_width, m_height);
    checkGlStatus(__FUNCTION__, "glViewport");
}
