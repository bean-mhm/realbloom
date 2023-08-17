#include "GlVertexBuffer.h"

GlVertexBuffer::GlVertexBuffer()
{
    glGenBuffers(1, &m_vbo);
    checkGlStatus(__FUNCTION__, "glGenBuffers");

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");

}

GlVertexBuffer::~GlVertexBuffer()
{
    glDeleteBuffers(1, &m_vbo);
    clearGlStatus();
}

GLuint GlVertexBuffer::getVBO() const
{
    return m_vbo;
}

void GlVertexBuffer::upload(const void* data, GLsizeiptr size, GLenum usage)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");

    glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    checkGlStatus(__FUNCTION__, "glBufferData");
}

void GlVertexBuffer::bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");
}
