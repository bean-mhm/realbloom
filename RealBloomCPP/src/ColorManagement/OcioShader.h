#pragma once

#include <stdint.h>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "../Utils/GlUtils.h"

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

constexpr bool OCIO_SHADER_LOG = true;

class OcioShader : public GlWrapper
{
private:
    GLuint m_vertShader = 0;
    GLuint m_fragShader = 0;

    GLuint m_program = 0;
    
public:
    OcioShader(OCIO::GpuShaderDescRcPtr shaderDesc);
    ~OcioShader();

    GLuint getProgram() const;
    void useProgram();

    void prepareLuts();
    void setInputTexture(GLint tex);
    void setExposureMul(float exposureMul);
};

