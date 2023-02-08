#include "GlShaderStorageBuffer.h"

GlShaderStorageBuffer::GlShaderStorageBuffer()
{
    glGenBuffers(1, &m_ssbo);
    checkGlStatus(__FUNCTION__, "glGenBuffers");

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");
}

GlShaderStorageBuffer::~GlShaderStorageBuffer()
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glDeleteBuffers(1, &m_ssbo);
    checkGlStatus(__FUNCTION__, "Cleanup");
}

GLuint GlShaderStorageBuffer::getSSBO() const
{
    return m_ssbo;
}

void GlShaderStorageBuffer::bind(uint32_t index)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, m_ssbo);
    checkGlStatus(__FUNCTION__, "glBindBufferBase");
}

void GlShaderStorageBuffer::unbind()
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    checkGlStatus(__FUNCTION__, "glBindBuffer");
}

void GlShaderStorageBuffer::read(float* target, uint32_t size)
{
    GLint bufferSize = 0;
    glGetNamedBufferParameteriv(m_ssbo, GL_BUFFER_SIZE, &bufferSize);
    checkGlStatus(__FUNCTION__, "glGetNamedBufferParameteriv(GL_BUFFER_SIZE)");
    printInfo(__FUNCTION__, "bufferSize", std::to_string(bufferSize));

    glGetNamedBufferSubData(
        m_ssbo,                // buffer
        0,                     // offset
        size * sizeof(float),  // size
        target);               // data
    checkGlStatus(__FUNCTION__, "glGetNamedBufferSubData");
}

void GlShaderStorageBuffer::write(float* source, uint32_t size)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");

    glBufferData(GL_SHADER_STORAGE_BUFFER, size * sizeof(float), source, GL_STREAM_COPY);
    checkGlStatus(__FUNCTION__, "glBufferData");
}

void GlShaderStorageBuffer::zeroFill(uint32_t size)
{
    std::vector<float> zeroBuffer;
    zeroBuffer.resize(size);
    for (auto& v : zeroBuffer)
        v = 0;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    checkGlStatus(__FUNCTION__, "glBindBuffer");

    glBufferData(GL_SHADER_STORAGE_BUFFER, size * sizeof(float), zeroBuffer.data(), GL_STREAM_COPY);
    checkGlStatus(__FUNCTION__, "glBufferData");
}
