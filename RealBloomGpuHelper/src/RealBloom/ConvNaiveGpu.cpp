#include "ConvNaiveGpu.h"

#pragma region Shaders
static const char* vertexSource = R"glsl(
    #version 150 core

    in vec2 pos;
    in vec3 color;

    out vec3 vColor;

    void main()
    {
        gl_Position = vec4(pos, 0.0, 1.0);
        vColor = color;
    }
    )glsl";

static const char* geometrySource = R"glsl(
    #version 150 core

    layout(points) in;
    layout(triangle_strip, max_vertices = 6) out;

    // Output from vertex shader for each vertex
    in vec3 vColor[];

    // Output to fragment shader
    out vec3 gColor;
    out vec2 gTexUV;

    uniform vec2 kernelTopLeft;
    uniform vec2 kernelSize;

    void main()
    {
        gColor = vColor[0];

        // Triangle 1

        // Top Left
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x, kernelTopLeft.y, 0.0, 1.0);
        gTexUV = vec2(0, 0);
        EmitVertex();

        // Top Right
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x + kernelSize.x, kernelTopLeft.y, 0.0, 1.0);
        gTexUV = vec2(1, 0);
        EmitVertex();

        // Bottom Left
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x, kernelTopLeft.y - kernelSize.y, 0.0, 1.0);
        gTexUV = vec2(0, 1);
        EmitVertex();

        EndPrimitive();

        // Triangle 2

        // Bottom Left
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x, kernelTopLeft.y - kernelSize.y, 0.0, 1.0);
        gTexUV = vec2(0, 1);
        EmitVertex();

        // Top Right
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x + kernelSize.x, kernelTopLeft.y, 0.0, 1.0);
        gTexUV = vec2(1, 0);
        EmitVertex();

        // Bottom Right
        gl_Position = gl_in[0].gl_Position + vec4(-kernelTopLeft.x + kernelSize.x, kernelTopLeft.y - kernelSize.y, 0.0, 1.0);
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

    uniform sampler2D texKernel;

    void main()
    {
        outColor = texture(texKernel, gTexUV) * vec4(gColor, 1.0);
    }
    )glsl";
#pragma endregion

namespace RealBloom
{

    void ConvNaiveGpu::makeKernelTexture(float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
    {
        glGenTextures(1, &m_texKernel);
        checkGlStatus(__FUNCTION__, "glGenTextures");

        glActiveTexture(GL_TEXTURE0);
        checkGlStatus(__FUNCTION__, "glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, m_texKernel);
        checkGlStatus(__FUNCTION__, "glBindTexture");

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        checkGlStatus(__FUNCTION__, "glTexParameteri");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kernelWidth, kernelHeight, 0, GL_RGBA, GL_FLOAT, kernelBuffer);
        checkGlStatus(__FUNCTION__, "glTexImage2D");
    }

    void ConvNaiveGpu::makeProgram()
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

    void ConvNaiveGpu::definePoints(GLfloat* points, uint32_t size)
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
             X      Y     R     G     B
            -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 1.0f, 1.0f, 1.0f
        }*/
    }

    void ConvNaiveGpu::useProgram()
    {
        glUseProgram(m_shaderProgram);
        checkGlStatus(__FUNCTION__, "glUseProgram");
    }

    void ConvNaiveGpu::bindKernelTexture()
    {
        glActiveTexture(GL_TEXTURE0);
        checkGlStatus(__FUNCTION__, "glActiveTexture");

        glBindTexture(GL_TEXTURE_2D, m_texKernel);
        checkGlStatus(__FUNCTION__, "glBindTexture");
    }

    void ConvNaiveGpu::setUniforms(uint32_t kernelWidth, uint32_t kernelHeight, float* kernelTopLeft, float* kernelSize)
    {
        GLint kernelTopLeftUniform = glGetUniformLocation(m_shaderProgram, "kernelTopLeft");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(kernelTopLeft)");

        glUniform2f(kernelTopLeftUniform, kernelTopLeft[0], kernelTopLeft[1]);
        checkGlStatus(__FUNCTION__, "glUniform2f(kernelTopLeftUniform)");

        GLint kernelSizeUniform = glGetUniformLocation(m_shaderProgram, "kernelSize");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(kernelSize)");

        glUniform2f(kernelSizeUniform, kernelSize[0], kernelSize[1]);
        checkGlStatus(__FUNCTION__, "glUniform2f(kernelSizeUniform)");

        GLint texKernelUniform = glGetUniformLocation(m_shaderProgram, "texKernel");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(texKernel)");

        glUniform1i(texKernelUniform, 0);
        checkGlStatus(__FUNCTION__, "glUniform1i(texKernelUniform)");

    }

    void ConvNaiveGpu::specifyLayout()
    {
        // Specify vertex data layout

        GLint posAttrib = glGetAttribLocation(m_shaderProgram, "pos");
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
        checkGlStatus(__FUNCTION__, "pos");

        GLint colAttrib = glGetAttribLocation(m_shaderProgram, "color");
        glEnableVertexAttribArray(colAttrib);
        glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
        checkGlStatus(__FUNCTION__, "color");
    }

    void ConvNaiveGpu::drawScene(uint32_t numPoints)
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

    ConvNaiveGpu::ConvNaiveGpu(BinaryConvNaiveGpuInput* binInput)
        : m_binInput(binInput)
    {}

    void ConvNaiveGpu::prepare()
    {
        m_status.reset();

        m_width = m_binInput->inputWidth;
        m_height = m_binInput->inputHeight;

        try
        {
            // Upload the kernel texture to the GPU
            makeKernelTexture(
                m_binInput->kernelBuffer.data(),
                m_binInput->kernelWidth,
                m_binInput->kernelHeight);

            // Shader program
            makeProgram();
        }
        catch (const std::exception& e)
        {
            m_status.setError(makeError(__FUNCTION__, "", e.what()));
        }
    }

    void ConvNaiveGpu::process(uint32_t chunkIndex)
    {
        if (!m_status.isOK())
            return;

        uint32_t inputPixels = m_width * m_height;

        uint32_t numChunks = m_binInput->numChunks;

        uint32_t kernelWidth = m_binInput->kernelWidth;
        uint32_t kernelHeight = m_binInput->kernelHeight;

        float threshold = m_binInput->cp_convThreshold;
        float transKnee = transformKnee(m_binInput->cp_convKnee);

        float kernelOriginX = (int)floorf(m_binInput->cp_kernelOriginX * (float)kernelWidth);
        float kernelOriginY = (int)floorf(m_binInput->cp_kernelOriginY * (float)kernelHeight);

        float offsetX = floorf((float)kernelWidth / 2.0f) - kernelOriginX;
        float offsetY = floorf((float)kernelHeight / 2.0f) - kernelOriginY;

        // Collect points (vertex data)
        clearVector(m_vertexData);
        float color[3]{ 0, 0, 0 };
        for (uint32_t i = 0; i < inputPixels; i++)
        {
            if (i % numChunks == chunkIndex)
            {
                uint32_t redIndex = i * 4;

                color[0] = m_binInput->inputBuffer[redIndex + 0];
                color[1] = m_binInput->inputBuffer[redIndex + 1];
                color[2] = m_binInput->inputBuffer[redIndex + 2];

                float v = rgbToGrayscale(color, CONV_THRESHOLD_GRAYSCALE_TYPE);
                if (v > threshold)
                {
                    uint32_t ix = i % m_width;
                    uint32_t iy = (i - ix) / m_width;

                    float px = (float)ix + offsetX;
                    float py = (float)iy + offsetY;

                    px = px / m_width;
                    py = 1.0f - (py / m_height);

                    // [0, 1] -> [-1, 1]
                    px = px * 2.0f - 1.0f;
                    py = py * 2.0f - 1.0f;

                    px *= GL_COORD_SCALE;
                    py *= GL_COORD_SCALE;

                    // Smooth Transition
                    float mul = softThreshold(v, threshold, transKnee);

                    m_vertexData.push_back(px);
                    m_vertexData.push_back(py);
                    m_vertexData.push_back(color[0] * mul);
                    m_vertexData.push_back(color[1] * mul);
                    m_vertexData.push_back(color[2] * mul);
                }
            }
        }

        // Draw
        std::shared_ptr<GlFrameBuffer> frameBuffer = nullptr;
        try
        {
            // Capabilities
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            checkGlStatus("", "Capabilities");

            // Frame buffer
            frameBuffer = std::make_shared<GlFrameBuffer>(m_width, m_height);
            frameBuffer->bind();
            frameBuffer->viewport();

            // Upload vertex data
            definePoints(m_vertexData.data(), m_vertexData.size());

            // Use the program
            useProgram();

            // Bind the kernel texture
            bindKernelTexture();

            // Set uniforms
            float kernelTopLeft[2], kernelSize[2];
            kernelTopLeft[0] = GL_COORD_SCALE * 2 * (floorf((float)m_binInput->kernelWidth / 2.0f) / (float)m_binInput->inputWidth);
            kernelTopLeft[1] = GL_COORD_SCALE * 2 * (floorf((float)m_binInput->kernelHeight / 2.0f) / (float)m_binInput->inputHeight);
            kernelSize[0] = GL_COORD_SCALE * 2 * (float)m_binInput->kernelWidth / (float)m_binInput->inputWidth;
            kernelSize[1] = GL_COORD_SCALE * 2 * (float)m_binInput->kernelHeight / (float)m_binInput->inputHeight;
            setUniforms(kernelWidth, kernelHeight, kernelTopLeft, kernelSize);

            // Specify vertex data layout
            specifyLayout();

            // Draw
            drawScene(m_vertexData.size() / getNumAttribs());

            // Copy pixel data from the frame buffer
            uint32_t fbSize = m_width * m_height * 4;
            std::vector<float> fbData;
            fbData.resize(fbSize);
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->getFrameBuffer());
                checkGlStatus("", "glBindFramebuffer");

                glReadBuffer(GL_COLOR_ATTACHMENT0);
                checkGlStatus("", "glReadBuffer");

                glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, fbData.data());
                checkGlStatus("", "glReadPixels");
            }

            // Copy to output buffer
            m_outputBuffer.resize(fbSize);
            float convMultiplier = m_binInput->convMultiplier;
            for (uint32_t y = 0; y < m_height; y++)
            {
                for (uint32_t x = 0; x < m_width; x++)
                {
                    uint32_t redIndexFb = ((m_height - 1 - y) * m_width + x) * 4;
                    uint32_t redIndexOutput = (y * m_width + x) * 4;

                    m_outputBuffer[redIndexOutput + 0] = fbData[redIndexFb + 0] * convMultiplier;
                    m_outputBuffer[redIndexOutput + 1] = fbData[redIndexFb + 1] * convMultiplier;
                    m_outputBuffer[redIndexOutput + 2] = fbData[redIndexFb + 2] * convMultiplier;
                    m_outputBuffer[redIndexOutput + 3] = 1.0f;
                }
            }
        }
        catch (const std::exception& e)
        {
            m_status.setError(makeError(__FUNCTION__, "", e.what()));
        }

        // Clean up
        try
        {
            try
            {
                frameBuffer = nullptr;
            }
            catch (const std::exception& e)
            {
                printError("", "Cleanup (frameBuffer)", e.what());
            }

            glDeleteBuffers(1, &m_vbo);
            glDeleteVertexArrays(1, &m_vao);

            checkGlStatus("", "Cleanup");
        }
        catch (const std::exception& e)
        {
            printError(__FUNCTION__, "", e.what());
        }
    }

    void ConvNaiveGpu::cleanUp()
    {
        try
        {
            glDeleteTextures(1, &m_texKernel);
            glDeleteProgram(m_shaderProgram);
            glDeleteShader(m_vertexShader);
            glDeleteShader(m_geometryShader);
            glDeleteShader(m_fragmentShader);
            checkGlStatus("", "Cleanup");
        }
        catch (const std::exception& e)
        {
            printError(__FUNCTION__, "", e.what());
        }
    }

    uint32_t ConvNaiveGpu::getNumVertices() const
    {
        return m_vertexData.size() / getNumAttribs();
    }

    const std::vector<float>& ConvNaiveGpu::getBuffer() const
    {
        return m_outputBuffer;
    }

    const BaseStatus& ConvNaiveGpu::getStatus() const
    {
        return m_status;
    }

    uint32_t ConvNaiveGpu::getNumAttribs()
    {
        return 5;
    }

}
