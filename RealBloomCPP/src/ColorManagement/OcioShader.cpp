#include "OcioShader.h"

#pragma region Vertex Shader Source
constexpr const char* VERTEX_SOURCE = R"glsl(
    #version 150 core

    in vec2 pos;
    in vec2 texUV;

    out vec2 vTexUV;

    void main()
    {
        gl_Position = vec4(pos, 0.0, 1.0);
        vTexUV = texUV;
    }
)glsl";
#pragma endregion

OcioShader::OcioShader(OCIO::GpuShaderDescRcPtr shaderDesc)
    : m_shaderDesc(shaderDesc)
{
    // Make fragment shader source code
    std::ostringstream fragSource;
    fragSource
        << "#version 130" << std::endl
        << std::endl
        << m_shaderDesc->getShaderText() << std::endl
        << std::endl
        << "in vec2 vTexUV;" << std::endl
        << std::endl
        << "out vec4 outColor;" << std::endl
        << std::endl
        << "uniform sampler2D img;" << std::endl
        << "uniform float exposureMul;" << std::endl
        << std::endl
        << "void main()" << std::endl
        << "{" << std::endl
        << "    vec4 col = texture(img, vTexUV) * vec4(exposureMul, exposureMul, exposureMul, 1.0);" << std::endl
        << "    outColor = " << m_shaderDesc->getFunctionName() << "(col);" << std::endl
        << "}" << std::endl;

    // Print out the shader source code
    if (OCIO_SHADER_LOG)
        std::cout << strFormat(
            "\n"
            "[%s]\n"
            "-------------------------------------------------------------------\n"
            "\n"
            "Vertex Shader:\n"
            "\n"
            "%s\n"
            "\n"
            "Fragment Shader:\n"
            "\n"
            "%s\n"
            "\n"
            "-------------------------------------------------------------------\n"
            "\n",

            __FUNCTION__,
            VERTEX_SOURCE,
            fragSource.str().c_str());

    std::string shaderLog;

    // Create and compile the vertex shader
    if (!createShader(GL_VERTEX_SHADER, VERTEX_SOURCE, m_vertShader, shaderLog))
        throw std::exception(
            printErr(__FUNCTION__, strFormat("Vertex shader compilation error: %s", shaderLog.c_str())).c_str()
        );

    // Create and compile the fragment shader
    if (!createShader(GL_FRAGMENT_SHADER, fragSource.str().c_str(), m_fragShader, shaderLog))
        throw std::exception(
            printErr(__FUNCTION__, strFormat("Fragment shader compilation error: %s", shaderLog.c_str())).c_str()
        );

    // Create program
    m_program = glCreateProgram();
    checkGlStatus(__FUNCTION__, "glCreateProgram");

    glAttachShader(m_program, m_vertShader);
    checkGlStatus(__FUNCTION__, "glAttachShader(m_program, m_vertShader)");

    glAttachShader(m_program, m_fragShader);
    checkGlStatus(__FUNCTION__, "glAttachShader(m_program, m_fragShader)");

    glBindFragDataLocation(m_program, 0, "outColor");
    checkGlStatus(__FUNCTION__, "glBindFragDataLocation");

    glLinkProgram(m_program);
    checkGlStatus(__FUNCTION__, "glLinkProgram");

    glUseProgram(m_program);
    checkGlStatus(__FUNCTION__, "glUseProgram");

    prepareLuts();
    prepareUniforms();
}

OcioShader::~OcioShader()
{
    // Delete all the textures
    const size_t max = m_textureIds.size();
    for (size_t idx = 0; idx < max; ++idx)
    {
        const TextureId& data = m_textureIds[idx];
        glDeleteTextures(1, &data.m_uid);
        checkGlStatus(__FUNCTION__, strFormat("glDeleteTextures (idx: %u)", idx));
    }
    m_textureIds.clear();

    // Delete all the uniforms
    m_uniforms.clear();

    // Clean up
    glDeleteProgram(m_program);
    glDeleteShader(m_fragShader);
    glDeleteShader(m_vertShader);
    checkGlStatus(__FUNCTION__, "Cleanup");
}

GLuint OcioShader::getProgram() const
{
    return m_program;
}

void OcioShader::useProgram()
{
    glUseProgram(m_program);
    checkGlStatus(__FUNCTION__, "glUseProgram");
}

void OcioShader::prepareLuts()
{
    // This is the first available index for the textures.
    uint32_t currIndex = m_lutStartIndex;

    // Process the 3D LUT first.

    const uint32_t maxTexture3D = m_shaderDesc->getNum3DTextures();
    for (uint32_t idx = 0; idx < maxTexture3D; ++idx)
    {
        // 1. Get the information of the 3D LUT.

        const char* textureName = nullptr;
        const char* samplerName = nullptr;
        uint32_t edgelen = 0;
        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
        m_shaderDesc->get3DTexture(idx, textureName, samplerName, edgelen, interpolation);

        if (!textureName || !*textureName
            || !samplerName || !*samplerName
            || edgelen == 0)
        {
            throw OCIO::Exception(
                printErr(__FUNCTION__, strFormat("The texture data is corrupted (3D LUTs, idx: %u)", idx)).c_str()
            );
        }

        const float* values = nullptr;
        m_shaderDesc->get3DTextureValues(idx, values);
        if (!values)
        {
            throw OCIO::Exception(
                printErr(__FUNCTION__, strFormat("The texture values are missing (3D LUTs, idx: %u)", idx)).c_str()
            );
        }

        // 2. Allocate the 3D LUT.

        uint32_t texId = 0;
        allocateTexture3D(currIndex, texId, interpolation, edgelen, values);

        // 3. Keep the texture id & name for the later enabling.

        m_textureIds.push_back(TextureId(texId, textureName, samplerName, GL_TEXTURE_3D));

        currIndex++;
    }

    // Process the 1D LUTs.

    const uint32_t maxTexture2D = m_shaderDesc->getNumTextures();
    for (uint32_t idx = 0; idx < maxTexture2D; ++idx)
    {
        // 1. Get the information of the 1D LUT.

        const char* textureName = nullptr;
        const char* samplerName = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
        m_shaderDesc->getTexture(idx, textureName, samplerName, width, height, channel, interpolation);

        if (!textureName || !*textureName
            || !samplerName || !*samplerName
            || width == 0)
        {
            throw OCIO::Exception(
                printErr(__FUNCTION__, strFormat("The texture data is corrupted (1D LUTs, idx: %u)", idx)).c_str()
            );
        }

        const float* values = 0x0;
        m_shaderDesc->getTextureValues(idx, values);
        if (!values)
        {
            throw OCIO::Exception(
                printErr(__FUNCTION__, strFormat("The texture values are missing (1D LUTs, idx: %u)", idx)).c_str()
            );
        }

        // 2. Allocate the 1D LUT (a 2D texture is needed to hold large LUTs).

        uint32_t texId = 0;
        allocateTexture2D(currIndex, texId, width, height, channel, interpolation, values);

        // 3. Keep the texture id & name for the later enabling.

        uint32_t type = (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D;
        m_textureIds.push_back(TextureId(texId, textureName, samplerName, type));
        currIndex++;
    }
}

void OcioShader::useLuts()
{
    // Use the LUT textures
    const size_t max = m_textureIds.size();
    for (size_t idx = 0; idx < max; ++idx)
    {
        const TextureId& data = m_textureIds[idx];

        GLenum texUnit = (GLenum)(GL_TEXTURE0 + m_lutStartIndex + idx);
        glActiveTexture(texUnit);
        checkGlStatus(__FUNCTION__, strFormat("glActiveTexture(%u) (idx: %u)", texUnit, idx));

        glBindTexture(data.m_type, data.m_uid);
        checkGlStatus(__FUNCTION__, strFormat("glBindTexture(%u, %u) (idx: %u)", data.m_type, data.m_uid, idx));

        GLint loc = glGetUniformLocation(m_program, data.m_samplerName.c_str());
        checkGlStatus(__FUNCTION__, strFormat("glGetUniformLocation(%s) (idx: %u)", data.m_samplerName.c_str(), idx));

        glUniform1i(loc, GLint(m_lutStartIndex + idx));
        checkGlStatus(__FUNCTION__, strFormat("glUniform1i(%d, %d) (idx: %u)", loc, GLint(m_lutStartIndex + idx), idx));
    }
}

void OcioShader::prepareUniforms()
{
    const uint32_t maxUniforms = m_shaderDesc->getNumUniforms();
    for (uint32_t idx = 0; idx < maxUniforms; ++idx)
    {
        OCIO::GpuShaderDesc::UniformData data;
        const char* name = m_shaderDesc->getUniform(idx, data);
        if (data.m_type == OCIO::UNIFORM_UNKNOWN)
        {
            throw OCIO::Exception(
                printErr(__FUNCTION__, strFormat("Unknown uniform type (idx: %u)", idx)).c_str()
            );
        }
        // Transfer uniform.
        m_uniforms.emplace_back(name, data);
        // Connect uniform with program.
        m_uniforms.back().setUp(m_program);
    }
}

void OcioShader::useUniforms()
{
    for (auto uniform : m_uniforms)
    {
        uniform.use();
    }
}

void OcioShader::setInputTexture(GLint tex)
{
    GLint uniform = glGetUniformLocation(m_program, "img");
    checkGlStatus(__FUNCTION__, "glGetUniformLocation");

    glUniform1i(uniform, tex);
    checkGlStatus(__FUNCTION__, "glUniform1i");
}

void OcioShader::setExposureMul(float exposureMul)
{
    GLint uniform = glGetUniformLocation(m_program, "exposureMul");
    checkGlStatus(__FUNCTION__, "glGetUniformLocation");

    glUniform1f(uniform, exposureMul);
    checkGlStatus(__FUNCTION__, "glUniform1f");
}

void OcioShader::enableAttribs()
{
    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(m_program, "pos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    checkGlStatus(__FUNCTION__, "posAttrib");

    GLint texAttrib = glGetAttribLocation(m_program, "texUV");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    checkGlStatus(__FUNCTION__, "texAttrib");
}

void OcioShader::disableAttribs()
{
    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(m_program, "pos");
    glDisableVertexAttribArray(posAttrib);
    checkGlStatus(__FUNCTION__, "posAttrib");

    GLint texAttrib = glGetAttribLocation(m_program, "texUV");
    glDisableVertexAttribArray(texAttrib);
    checkGlStatus(__FUNCTION__, "texAttrib");
}

void OcioShader::setTextureParameters(GLenum textureType, OCIO::Interpolation interpolation)
{
    if (interpolation == OCIO::INTERP_NEAREST)
    {
        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else
    {
        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(textureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void OcioShader::allocateTexture3D(uint32_t index, uint32_t& texId,
    OCIO::Interpolation interpolation,
    uint32_t edgelen, const float* values)
{
    if (values == 0x0)
        throw OCIO::Exception(
            printErr(__FUNCTION__, strFormat("Missing texture data (idx: %u)", index)).c_str()
        );

    glGenTextures(1, &texId);
    checkGlStatus(__FUNCTION__, strFormat("glGenTextures (idx: %u)", index));

    glActiveTexture(GL_TEXTURE0 + index);
    checkGlStatus(__FUNCTION__, strFormat("glActiveTexture (idx: %u)", index));

    glBindTexture(GL_TEXTURE_3D, texId);
    checkGlStatus(__FUNCTION__, strFormat("glBindTexture (idx: %u)", index));

    setTextureParameters(GL_TEXTURE_3D, interpolation);
    checkGlStatus(__FUNCTION__, strFormat("setTextureParameters (idx: %u)", index));

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
    checkGlStatus(__FUNCTION__, strFormat("glTexImage3D (idx: %u)", index));
}

void OcioShader::allocateTexture2D(uint32_t index, uint32_t& texId,
    uint32_t width, uint32_t height,
    OCIO::GpuShaderDesc::TextureType channel,
    OCIO::Interpolation interpolation, const float* values)
{
    if (values == nullptr)
        throw OCIO::Exception(
            printErr(__FUNCTION__, strFormat("Missing texture data (idx: %u)", index)).c_str()
        );

    GLint internalformat = GL_RGB32F;
    GLenum format = GL_RGB;

    if (channel == OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL)
    {
        internalformat = GL_R32F;
        format = GL_RED;
    }

    glGenTextures(1, &texId);
    checkGlStatus(__FUNCTION__, strFormat("glGenTextures (idx: %u)", index));

    glActiveTexture(GL_TEXTURE0 + index);
    checkGlStatus(__FUNCTION__, strFormat("glActiveTexture (idx: %u)", index));

    if (height > 1)
    {
        glBindTexture(GL_TEXTURE_2D, texId);
        checkGlStatus(__FUNCTION__, strFormat("glBindTexture (idx: %u)", index));

        setTextureParameters(GL_TEXTURE_2D, interpolation);
        checkGlStatus(__FUNCTION__, strFormat("setTextureParameters (idx: %u)", index));

        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, values);
        checkGlStatus(__FUNCTION__, strFormat("glTexImage2D (idx: %u)", index));
    } else
    {
        glBindTexture(GL_TEXTURE_1D, texId);
        checkGlStatus(__FUNCTION__, strFormat("glBindTexture (idx: %u)", index));

        setTextureParameters(GL_TEXTURE_1D, interpolation);

        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
        checkGlStatus(__FUNCTION__, strFormat("glTexImage1D (idx: %u)", index));
    }
}

OcioShader::Uniform::Uniform(const std::string& name, const OCIO::GpuShaderDesc::UniformData& data)
    : m_name(name), m_data(data), m_handle(0)
{}

void OcioShader::Uniform::setUp(uint32_t program)
{
    m_handle = glGetUniformLocation(program, m_name.c_str());
    checkGlStatus(__FUNCTION__, strFormat("glGetUniformLocation(%s)", m_name.c_str()));
}

void OcioShader::Uniform::use()
{
    // Update value.
    if (m_data.m_getDouble)
    {
        glUniform1f(m_handle, (GLfloat)m_data.m_getDouble());
    } else if (m_data.m_getBool)
    {
        glUniform1f(m_handle, (GLfloat)(m_data.m_getBool() ? 1.0f : 0.0f));
    } else if (m_data.m_getFloat3)
    {
        glUniform3f(m_handle, (GLfloat)m_data.m_getFloat3()[0],
            (GLfloat)m_data.m_getFloat3()[1],
            (GLfloat)m_data.m_getFloat3()[2]);
    } else if (m_data.m_vectorFloat.m_getSize && m_data.m_vectorFloat.m_getVector)
    {
        glUniform1fv(m_handle, (GLsizei)m_data.m_vectorFloat.m_getSize(),
            (GLfloat*)m_data.m_vectorFloat.m_getVector());
    } else if (m_data.m_vectorInt.m_getSize && m_data.m_vectorInt.m_getVector)
    {
        glUniform1iv(m_handle, (GLsizei)m_data.m_vectorInt.m_getSize(),
            (GLint*)m_data.m_vectorInt.m_getVector());
    } else
        throw OCIO::Exception(
            printErr(__FUNCTION__, strFormat("Uniform %s is not linked to any value.", m_name.c_str())).c_str()
        );
    checkGlStatus(__FUNCTION__, strFormat("\"%s\"", m_name.c_str()));
}
