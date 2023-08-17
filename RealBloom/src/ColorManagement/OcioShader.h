#pragma once

#include <vector>
#include <string>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "../Utils/OpenGL/GlUtils.h"
#include "../Utils/Misc.h"

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OpenColorIO_v2_1;

constexpr bool OCIO_SHADER_LOG = false;

// OpenColorIO Shader
class OcioShader : public GlWrapper
{
public:
    OcioShader(OCIO::GpuShaderDescRcPtr shaderDesc);
    ~OcioShader();

    GLuint getProgram() const;
    void useProgram();

    void setInputTexture(GLint tex);
    void setExposureMul(float exposureMul);

    void useLuts();
    void useUniforms();

private:
    struct TextureId
    {
        uint32_t    m_uid = -1;
        std::string m_textureName;
        std::string m_samplerName;
        uint32_t    m_type = -1;

        TextureId(uint32_t uid,
            const std::string& textureName,
            const std::string& samplerName,
            uint32_t type)
            : m_uid(uid)
            , m_textureName(textureName)
            , m_samplerName(samplerName)
            , m_type(type)
        {}
    };

    // Uniform are used for dynamic parameters.
    class Uniform
    {
    public:
        Uniform(const std::string& name, const OCIO::GpuShaderDesc::UniformData& data);

        void setUp(uint32_t program);

        void use();

    private:
        Uniform() = delete;
        std::string m_name;
        OCIO::GpuShaderDesc::UniformData m_data;

        uint32_t m_handle;
    };

private:
    GLuint m_vertShader = 0;
    GLuint m_fragShader = 0;
    GLuint m_program = 0;

    OCIO::GpuShaderDescRcPtr m_shaderDesc;
    std::vector<TextureId> m_textureIds;
    std::vector<Uniform> m_uniforms;

    uint32_t m_lutStartIndex = 1;

    void setTextureParameters(GLenum textureType, OCIO::Interpolation interpolation);
    void allocateTexture3D(uint32_t index, uint32_t& texId,
        OCIO::Interpolation interpolation,
        uint32_t edgelen, const float* values);
    void allocateTexture2D(uint32_t index, uint32_t& texId,
        uint32_t width, uint32_t height,
        OCIO::GpuShaderDesc::TextureType channel,
        OCIO::Interpolation interpolation, const float* values);

    void prepareLuts();
    void prepareUniforms();

};
