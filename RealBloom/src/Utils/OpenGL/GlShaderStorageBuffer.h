#pragma once

#include <vector>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"
#include "../Misc.h"

// OpenGL Shader Storage Buffer Wrapper
class GlShaderStorageBuffer : public GlWrapper
{
public:
    GlShaderStorageBuffer();
    virtual ~GlShaderStorageBuffer();

    GLuint getSSBO() const;

    void bind(uint32_t index);
    void unbind();

    void read(float* target, uint32_t size);
    void write(float* source, uint32_t size);
    void zeroFill(uint32_t size);

private:
    GLuint m_ssbo = 0;

};
