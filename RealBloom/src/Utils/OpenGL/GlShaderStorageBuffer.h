#pragma once

#include <vector>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "GlUtils.h"
#include "../Misc.h"

class GlShaderStorageBuffer : public GlWrapper
{
private:
    GLuint m_ssbo = 0;

public:
    GlShaderStorageBuffer();
    virtual ~GlShaderStorageBuffer();

    GLuint getSSBO() const;

    void bind(uint32_t index);
    void unbind();

    void read(float* target, uint32_t size);
    void write(float* source, uint32_t size);
    void zeroFill(uint32_t size);

};
