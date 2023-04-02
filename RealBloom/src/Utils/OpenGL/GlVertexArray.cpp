#include "GlVertexArray.h"

GlVertexArray::GlVertexArray()
{
    glGenVertexArrays(1, &m_vao);
    checkGlStatus(__FUNCTION__, "glGenVertexArrays");

    glBindVertexArray(m_vao);
    checkGlStatus(__FUNCTION__, "glBindVertexArray");
}

GlVertexArray::~GlVertexArray()
{
    glDeleteVertexArrays(1, &m_vao);
    clearGlStatus();
}

GLuint GlVertexArray::getVAO() const
{
    return m_vao;
}

void GlVertexArray::bind()
{
    glBindVertexArray(m_vao);
    checkGlStatus(__FUNCTION__, "glBindVertexArray");
}

void GlVertexArray::unbind()
{
    glBindVertexArray(0);
    checkGlStatus(__FUNCTION__, "glBindVertexArray");
}
