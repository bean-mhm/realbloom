#include "DispGpu.h"

#pragma region Shaders
static const char* vertexSource = R"glsl(
    #version 150 core

    in float scale;
    in vec3 color;

    out float vScale;
    out vec3 vColor;

    void main()
    {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        vScale = scale;
        vColor = color;
    }
)glsl";

static const char* geometrySource = R"glsl(
    #version 150 core

    layout(points) in;
    layout(triangle_strip, max_vertices = 6) out;

    // Output from vertex shader for each vertex
    in float vScale[];
    in vec3 vColor[];

    // Output to fragment shader
    out vec3 gColor;
    out vec2 gTexUV;

    void main()
    {
        float scale = vScale[0];
        gColor = vColor[0];

        // Triangle 1

        // Top Left
        gl_Position = vec4(-scale, scale, 0.0, 1.0);
        gTexUV = vec2(0, 0);
        EmitVertex();

        // Top Right
        gl_Position = vec4(scale, scale, 0.0, 1.0);
        gTexUV = vec2(1, 0);
        EmitVertex();

        // Bottom Left
        gl_Position = vec4(-scale, -scale, 0.0, 1.0);
        gTexUV = vec2(0, 1);
        EmitVertex();

        EndPrimitive();

        // Triangle 2

        // Bottom Left
        gl_Position = vec4(-scale, -scale, 0.0, 1.0);
        gTexUV = vec2(0, 1);
        EmitVertex();

        // Top Right
        gl_Position = vec4(scale, scale, 0.0, 1.0);
        gTexUV = vec2(1, 0);
        EmitVertex();

        // Bottom Right
        gl_Position = vec4(scale, -scale, 0.0, 1.0);
        gTexUV = vec2(1, 1);
        EmitVertex();

        EndPrimitive();
    }
)glsl";

static const char* fragmentSource = R"glsl(
    #version 150 core

    in vec3 gColor;
    in vec2 gTexUV;

    out vec4 outColor;

    uniform sampler2D texInput;

    void main()
    {
        outColor = texture(texInput, gTexUV) * vec4(gColor, 1.0);
    }
)glsl";
#pragma endregion

namespace RealBloom
{

    DispGpu::DispGpu(BinaryDispGpuInput* binInput)
        : m_binInput(binInput)
    {}

    void DispGpu::makeInputTexture(float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight)
    {
        glGenTextures(1, &m_texInput);
        checkGlStatus(__FUNCTION__, "glGenTextures");

        glActiveTexture(GL_TEXTURE0);
        checkGlStatus(__FUNCTION__, "glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, m_texInput);
        checkGlStatus(__FUNCTION__, "glBindTexture");

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        checkGlStatus(__FUNCTION__, "glTexParameteri");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, inputWidth, inputHeight, 0, GL_RGBA, GL_FLOAT, inputBuffer);
        checkGlStatus(__FUNCTION__, "glTexImage2D");
    }

    void DispGpu::makeProgram()
    {
        // Create and compile shaders
        std::string shaderLog;

        if (!createShader(GL_VERTEX_SHADER, vertexSource, m_vertexShader, shaderLog))
            throw std::exception(
                makeError(__FUNCTION__, "", strFormat("Vertex shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        if (!createShader(GL_GEOMETRY_SHADER, geometrySource, m_geometryShader, shaderLog))
            throw std::exception(
                makeError(__FUNCTION__, "", strFormat("Geometry shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        if (!createShader(GL_FRAGMENT_SHADER, fragmentSource, m_fragmentShader, shaderLog))
            throw std::exception(
                makeError(__FUNCTION__, "", strFormat("Fragment shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        // Link the vertex and fragment shaders
        m_shaderProgram = glCreateProgram();
        checkGlStatus(__FUNCTION__, "glCreateProgram");

        glAttachShader(m_shaderProgram, m_vertexShader);
        checkGlStatus(__FUNCTION__, "glAttachShader (Vertex Shader)");

        glAttachShader(m_shaderProgram, m_geometryShader);
        checkGlStatus(__FUNCTION__, "glAttachShader (Geometry Shader)");

        glAttachShader(m_shaderProgram, m_fragmentShader);
        checkGlStatus(__FUNCTION__, "glAttachShader (Fragment Shader)");

        glBindFragDataLocation(m_shaderProgram, 0, "outColor");
        checkGlStatus(__FUNCTION__, "glBindFragDataLocation");

        glLinkProgram(m_shaderProgram);
        checkGlStatus(__FUNCTION__, "glLinkProgram");

        glUseProgram(m_shaderProgram);
        checkGlStatus(__FUNCTION__, "glUseProgram");
    }

    void DispGpu::definePoints(GLfloat* points, uint32_t size)
    {
        // Create Vertex Array Object
        glGenVertexArrays(1, &m_vao);
        checkGlStatus(__FUNCTION__, "glGenVertexArrays");

        glBindVertexArray(m_vao);
        checkGlStatus(__FUNCTION__, "glBindVertexArray");

        // Create a Vertex Buffer Object and copy the vertex data to it
        glGenBuffers(1, &m_vbo);
        checkGlStatus(__FUNCTION__, "glGenBuffers");

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        checkGlStatus(__FUNCTION__, "glBindBuffer");

        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * size, points, GL_STATIC_DRAW);
        checkGlStatus(__FUNCTION__, "glBufferData");

        /*Example:
        {
             Scale   R     G     B
             0.5f,  1.0f, 0.0f, 0.0f
             0.5f,  1.0f, 0.0f, 0.0f,
             0.5f,  1.0f, 0.0f, 0.0f,
             0.5f,  1.0f, 0.0f, 0.0f,
        }*/
    }

    void DispGpu::useProgram()
    {
        glUseProgram(m_shaderProgram);
        checkGlStatus(__FUNCTION__, "glUseProgram");
    }

    void DispGpu::bindInputTexture()
    {
        glActiveTexture(GL_TEXTURE0);
        checkGlStatus(__FUNCTION__, "glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, m_texInput);
        checkGlStatus(__FUNCTION__, "glBindTexture");
    }

    void DispGpu::setUniforms()
    {
        GLint texInputUniform = glGetUniformLocation(m_shaderProgram, "texInput");
        glUniform1i(texInputUniform, 0);
        checkGlStatus(__FUNCTION__, "glUniform1i(texInputUniform)");
    }

    void DispGpu::specifyLayout()
    {
        // Specify vertex data layout

        GLint scaleAttrib = glGetAttribLocation(m_shaderProgram, "scale");
        glEnableVertexAttribArray(scaleAttrib);
        glVertexAttribPointer(scaleAttrib, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
        checkGlStatus(__FUNCTION__, "scale");

        GLint colAttrib = glGetAttribLocation(m_shaderProgram, "color");
        glEnableVertexAttribArray(colAttrib);
        glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(1 * sizeof(GLfloat)));
        checkGlStatus(__FUNCTION__, "color");
    }

    void DispGpu::drawScene(uint32_t numPoints)
    {
        // Clear the screen to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        checkGlStatus(__FUNCTION__, "glClearColor");

        glClear(GL_COLOR_BUFFER_BIT);
        checkGlStatus(__FUNCTION__, "glClear");

        // Additive blending
        glBlendFunc(GL_ONE, GL_ONE);
        checkGlStatus(__FUNCTION__, "glBlendFunc");

        // Draw
        glDrawArrays(GL_POINTS, 0, numPoints);
        checkGlStatus(__FUNCTION__, "glDrawArrays");
    }

    void DispGpu::process()
    {
        m_status.reset();

        m_width = m_binInput->inputWidth;
        m_height = m_binInput->inputHeight;

        uint32_t steps = m_binInput->dp_steps;
        float amount = m_binInput->dp_amount;

        // Vertex data
        std::vector<float> vertexData;
        vertexData.reserve(steps * getNumAttribs());

        for (uint32_t i = 0; i < steps; i++)
        {
            float scale, areaMul;
            calcDispScale(i, steps, amount, scale, areaMul);

            uint32_t smpIndex = i * 3;
            float wlR = m_binInput->cmfSamples[smpIndex + 0] * areaMul;
            float wlG = m_binInput->cmfSamples[smpIndex + 1] * areaMul;
            float wlB = m_binInput->cmfSamples[smpIndex + 2] * areaMul;

            vertexData.push_back(scale);
            vertexData.push_back(wlR);
            vertexData.push_back(wlG);
            vertexData.push_back(wlB);
        }

        // Draw
        std::shared_ptr<GlFramebuffer> framebuffer = nullptr;
        try
        {
            // Capabilities
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            checkGlStatus("", "Capabilities");

            // Framebuffer
            framebuffer = std::make_shared<GlFramebuffer>(m_width, m_height);
            framebuffer->bind();
            framebuffer->viewport();

            // Upload the input texture to the GPU
            makeInputTexture(
                m_binInput->inputBuffer.data(),
                m_binInput->inputWidth,
                m_binInput->inputHeight);

            // Shader program
            makeProgram();

            // Upload vertex data
            definePoints(vertexData.data(), vertexData.size());

            // Use the program
            useProgram();

            // Bind the input texture
            bindInputTexture();

            // Set uniforms
            setUniforms();

            // Specify vertex data layout
            specifyLayout();

            // Draw
            drawScene(vertexData.size() / getNumAttribs());
            
            // Copy pixel data from the framebuffer
            uint32_t fbSize = m_width * m_height * 4;
            std::vector<float> fbData;
            fbData.resize(fbSize);
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->getFramebuffer());
                checkGlStatus("", "glBindFramebuffer");

                glReadBuffer(GL_COLOR_ATTACHMENT0);
                checkGlStatus("", "glReadBuffer");

                glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, fbData.data());
                checkGlStatus("", "glReadPixels");
            }

            // Copy to output buffer
            m_outputBuffer.resize(fbSize);
            for (uint32_t y = 0; y < m_height; y++)
            {
                for (uint32_t x = 0; x < m_width; x++)
                {
                    uint32_t redIndexFb = ((m_height - 1 - y) * m_width + x) * 4;
                    uint32_t redIndexOutput = (y * m_width + x) * 4;

                    m_outputBuffer[redIndexOutput + 0] = fbData[redIndexFb + 0];
                    m_outputBuffer[redIndexOutput + 1] = fbData[redIndexFb + 1];
                    m_outputBuffer[redIndexOutput + 2] = fbData[redIndexFb + 2];
                    m_outputBuffer[redIndexOutput + 3] = 1.0f;
                }
            }
        }
        catch (const std::exception& e)
        {
            m_status.setError(makeError(__FUNCTION__, "", e.what()));
        }

        // Clean up

        framebuffer = nullptr;

        glDeleteBuffers(1, &m_vbo);
        glDeleteVertexArrays(1, &m_vao);

        glDeleteTextures(1, &m_texInput);
        glDeleteProgram(m_shaderProgram);
        glDeleteShader(m_vertexShader);
        glDeleteShader(m_geometryShader);
        glDeleteShader(m_fragmentShader);

        clearGlStatus();
    }

    const std::vector<float>& DispGpu::getBuffer() const
    {
        return m_outputBuffer;
    }

    const BaseStatus& DispGpu::getStatus() const
    {
        return m_status;
    }

    uint32_t DispGpu::getNumAttribs()
    {
        return 4;
    }

}
