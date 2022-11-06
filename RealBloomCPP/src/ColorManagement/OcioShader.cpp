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
{
    // Make fragment shader source code
    std::ostringstream fragSource;
    fragSource
        << "#version 150 core" << std::endl
        << std::endl
        << shaderDesc->getShaderText() << std::endl
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
        << "    outColor = " << "vec4(1.0, 0.0, 0.0, 1.0);"/*shaderDesc->getFunctionName() << "(col);"*/ << std::endl
        << "}" << std::endl;

    // Print out the shader source code
    if (OCIO_SHADER_LOG)
        std::cout << stringFormat(
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
    {
        setError(__FUNCTION__, stringFormat("Vertex shader compilation error: ", shaderLog.c_str()));
        return;
    }

    // Create and compile the fragment shader
    if (!createShader(GL_FRAGMENT_SHADER, fragSource.str().c_str(), m_fragShader, shaderLog))
    {
        setError(__FUNCTION__, stringFormat("Fragment shader compilation error: ", shaderLog.c_str()));
        return;
    }

    // Create program
    m_program = glCreateProgram();
    if (!checkGlStatus(__FUNCTION__, "glCreateProgram")) return;

    glAttachShader(m_program, m_vertShader);
    if (!checkGlStatus(__FUNCTION__, "glAttachShader(m_program, m_vertShader)")) return;

    glAttachShader(m_program, m_fragShader);
    if (!checkGlStatus(__FUNCTION__, "glAttachShader(m_program, m_fragShader)")) return;

    glBindFragDataLocation(m_program, 0, "outColor");
    if (!checkGlStatus(__FUNCTION__, "glBindFragDataLocation")) return;

    glLinkProgram(m_program);
    if (!checkGlStatus(__FUNCTION__, "glLinkProgram")) return;

    glUseProgram(m_program);
    if (!checkGlStatus(__FUNCTION__, "glUseProgram")) return;

    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(m_program, "pos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    if (!checkGlStatus(__FUNCTION__, "Vertex buffer layout specification (pos)")) return;

    GLint texAttrib = glGetAttribLocation(m_program, "texUV");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    if (!checkGlStatus(__FUNCTION__, "Vertex buffer layout specification (texUV)")) return;

    prepareLuts();
}

OcioShader::~OcioShader()
{
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
    if (hasFailed())
        return;

    glUseProgram(m_program);
    if (!checkGlStatus(__FUNCTION__, "glUseProgram")) return;
}

void OcioShader::prepareLuts()
{
    if (hasFailed())
        return;

    //
}

void OcioShader::setInputTexture(GLint tex)
{
    if (hasFailed())
        return;

    GLint uniform = glGetUniformLocation(m_program, "img");
    if (!checkGlStatus(__FUNCTION__, "glGetUniformLocation")) return;

    glUniform1i(uniform, tex);
    if (!checkGlStatus(__FUNCTION__, "glUniform1i")) return;
}

void OcioShader::setExposureMul(float exposureMul)
{
    if (hasFailed())
        return;

    GLint uniform = glGetUniformLocation(m_program, "exposureMul");
    if (!checkGlStatus(__FUNCTION__, "glGetUniformLocation")) return;

    glUniform1f(uniform, exposureMul);
    if (!checkGlStatus(__FUNCTION__, "glUniform1f")) return;
}
