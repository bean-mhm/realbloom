#include "ConvolutionGPU.h"

#pragma region Shaders
const char* vertexSource = R"glsl(
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

const char* fragmentSource = R"glsl(
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

const char* geometrySource = R"glsl(
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
#pragma endregion

constexpr float GPU_COORD_SCALE = 2.0f;

namespace RealBloom
{

    void init(void* pdata);

    void ConvolutionGPU::initGL()
    {
        std::string err2;

        if (!oglOneTimeContext(
            m_width,
            m_height,
            init,
            this,
            [&](std::string err)
            {
                m_data->setError(stringFormat("OpenGL Initialization Error: \"%s\"", err.c_str()));
            },
            err2))
        {
            m_data->setError(stringFormat("OpenGL Initialization Error: \"%s\"", err2.c_str()));
        }
    }

    void ConvolutionGPU::definePoints(GLfloat* points, uint32_t numPoints, uint32_t numAttribs)
    {
        std::string errorList = "";

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

        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * numPoints * numAttribs, points, GL_STATIC_DRAW);
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

    void ConvolutionGPU::makeProgram()
    {
        // Create and compile shaders
        std::string shaderLog;

        if (!createShader(GL_VERTEX_SHADER, vertexSource, m_vertexShader, shaderLog))
            throw std::exception(
                formatErr(__FUNCTION__, stringFormat("Vertex shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        if (!createShader(GL_GEOMETRY_SHADER, geometrySource, m_geometryShader, shaderLog))
            throw std::exception(
                formatErr(__FUNCTION__, stringFormat("Geometry shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        if (!createShader(GL_FRAGMENT_SHADER, fragmentSource, m_fragmentShader, shaderLog))
            throw std::exception(
                formatErr(__FUNCTION__, stringFormat("Fragment shader compilation error: %s", shaderLog.c_str())).c_str()
            );

        // Link the vertex and fragment shader into a shader program
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

        // Specify the layout of the vertex data
        GLint posAttrib = glGetAttribLocation(m_shaderProgram, "pos");
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
        checkGlStatus(__FUNCTION__, "Vertex buffer layout specification (pos)");

        GLint colAttrib = glGetAttribLocation(m_shaderProgram, "color");
        glEnableVertexAttribArray(colAttrib);
        glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
        checkGlStatus(__FUNCTION__, "Vertex buffer layout specification (color)");
    }

    void ConvolutionGPU::makeKernelTexture(float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight, float* kernelTopLeft, float* kernelSize)
    {
        // Set uniforms
        GLint kernelTopLeftUniform = glGetUniformLocation(m_shaderProgram, "kernelTopLeft");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(kernelTopLeft)");

        glUniform2f(kernelTopLeftUniform, kernelTopLeft[0], kernelTopLeft[1]);
        checkGlStatus(__FUNCTION__, "glUniform2f(kernelTopLeftUniform)");

        GLint kernelSizeUniform = glGetUniformLocation(m_shaderProgram, "kernelSize");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(kernelSize)");

        glUniform2f(kernelSizeUniform, kernelSize[0], kernelSize[1]);
        checkGlStatus(__FUNCTION__, "glUniform2f(kernelSizeUniform)");

        // Generate a texture
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

        GLint texKernelUniform = glGetUniformLocation(m_shaderProgram, "texKernel");
        checkGlStatus(__FUNCTION__, "glGetUniformLocation(texKernel)");

        glUniform1i(texKernelUniform, 0);
        checkGlStatus(__FUNCTION__, "glUniform1i(texKernelUniform)");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kernelWidth, kernelHeight, 0, GL_RGBA, GL_FLOAT, kernelBuffer);
        checkGlStatus(__FUNCTION__, "glTexImage2D");
    }

    void ConvolutionGPU::drawScene(uint32_t numPoints)
    {
        // Clear the screen to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        checkGlStatus(__FUNCTION__, "glClearColor");

        glClear(GL_COLOR_BUFFER_BIT);
        checkGlStatus(__FUNCTION__, "glClear");

        glBlendFunc(GL_ONE, GL_ONE);
        checkGlStatus(__FUNCTION__, "glBlendFunc");

        glDrawArrays(GL_POINTS, 0, numPoints);
        checkGlStatus(__FUNCTION__, "glDrawArrays");
    }

    ConvolutionGPU::ConvolutionGPU(ConvolutionGPUData* data)
        : m_data(data)
    {}

    void ConvolutionGPU::start(uint32_t numChunks, uint32_t chunkIndex)
    {
        m_data->done = false;
        m_data->error = "";
        m_data->success = false;
        m_data->numPoints = 0;

        ConvolutionGPUBinaryInput* binInput = m_data->binaryInput;
        m_width = binInput->inputWidth;
        m_height = binInput->inputHeight;

        uint32_t inputWidth = binInput->inputWidth;
        uint32_t inputHeight = binInput->inputHeight;
        uint32_t inputPixels = inputWidth * inputHeight;
        uint32_t kernelWidth = binInput->kernelWidth;
        uint32_t kernelHeight = binInput->kernelHeight;
        float threshold = binInput->cp_convThreshold;
        float knee = binInput->cp_convKnee;

        float kernelCenterX = (int)floorf(binInput->cp_kernelCenterX * (float)kernelWidth);
        float kernelCenterY = (int)floorf(binInput->cp_kernelCenterY * (float)kernelHeight);

        float offsetX = floorf((float)kernelWidth / 2.0f) - kernelCenterX;
        float offsetY = floorf((float)kernelHeight / 2.0f) - kernelCenterY;

        float v;
        float px, py;
        float color[3]{ 0, 0, 0 };
        float mul;
        uint32_t ix, iy;
        uint32_t redIndex;
        for (uint32_t i = 0; i < inputPixels; i++)
        {
            if (i % numChunks == chunkIndex)
            {
                redIndex = i * 4;

                color[0] = binInput->inputBuffer[redIndex + 0];
                color[1] = binInput->inputBuffer[redIndex + 1];
                color[2] = binInput->inputBuffer[redIndex + 2];

                v = rgbToGrayscale(color[0], color[1], color[2]);
                if (v > threshold)
                {
                    ix = i % inputWidth;
                    iy = (i - ix) / inputWidth;

                    px = (float)ix + offsetX;
                    py = (float)iy + offsetY;

                    // Technically the correct way is (px + 0.5f) / inputWidth but we don't
                    // wanna use bilinear, for performance reasons.
                    px = px / inputWidth;
                    py = 1.0f - (py / inputHeight);

                    // 0.0 -> 1.0  to  -1.0 -> +1.0
                    px = px * 2.0f - 1.0f;
                    py = py * 2.0f - 1.0f;

                    px *= GPU_COORD_SCALE;
                    py *= GPU_COORD_SCALE;

                    // Smooth Transition
                    mul = softThreshold(v, threshold, knee);

                    m_data->points.push_back(px);
                    m_data->points.push_back(py);
                    m_data->points.push_back(color[0] * mul);
                    m_data->points.push_back(color[1] * mul);
                    m_data->points.push_back(color[2] * mul);

                    m_data->numPoints++;
                }
            }
        }

        initGL();

        while (!(m_data->done))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void init(void* pdata)
    {
        if (!pdata)
            return;

        ConvolutionGPU* instance = (ConvolutionGPU*)pdata;
        ConvolutionGPUData* data = instance->m_data;
        ConvolutionGPUBinaryInput* binInput = data->binaryInput;

        // Copy some variables from data for more readability
        uint32_t inputWidth = binInput->inputWidth;
        uint32_t inputHeight = binInput->inputHeight;
        uint32_t kernelWidth = binInput->kernelWidth;
        uint32_t kernelHeight = binInput->kernelHeight;
        uint32_t numPoints = data->numPoints;

        std::shared_ptr<GlFrameBuffer> frameBuffer = nullptr;
        GLfloat* fbData = nullptr;
        try
        {
            // Grab the name of the renderer
            const char* rendererS = (const char*)glGetString(GL_RENDERER);
            checkGlStatus(__FUNCTION__, "glGetString(GL_RENDERER)");
            data->gpuName = rendererS;

            // Capabilities
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            checkGlStatus(__FUNCTION__, "Capabilities");

            // Make a frame buffer that we'll render to
            frameBuffer = std::make_shared<GlFrameBuffer>(instance->m_width, instance->m_height);

            // Define the input data
            instance->definePoints(data->points.data(), numPoints, 5);

            // Prepare the shader program
            instance->makeProgram();

            // Upload the kernel texture to the GPU along with some uniforms
            {
                float kernelTopLeft[2], kernelSize[2];
                kernelTopLeft[0] = GPU_COORD_SCALE * 2 * (floorf((float)kernelWidth / 2.0f) / (float)inputWidth);
                kernelTopLeft[1] = GPU_COORD_SCALE * 2 * (floorf((float)kernelHeight / 2.0f) / (float)inputHeight);
                kernelSize[0] = GPU_COORD_SCALE * 2 * (float)kernelWidth / (float)inputWidth;
                kernelSize[1] = GPU_COORD_SCALE * 2 * (float)kernelHeight / (float)inputHeight;

                instance->makeKernelTexture(
                    binInput->kernelBuffer,
                    binInput->kernelWidth,
                    binInput->kernelHeight,
                    kernelTopLeft,
                    kernelSize);
            }

            // Draw
            instance->drawScene(numPoints);

            // Copy pixel data from the frame buffer
            uint32_t fbSize = inputWidth * inputHeight * 4;
            fbData = new float[fbSize];
            try
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->getFrameBuffer());
                checkGlStatus(__FUNCTION__, "glBindFramebuffer");

                glReadBuffer(GL_COLOR_ATTACHMENT0);
                checkGlStatus(__FUNCTION__, "glReadBuffer");

                glReadPixels(0, 0, inputWidth, inputHeight, GL_RGBA, GL_FLOAT, fbData);
                checkGlStatus(__FUNCTION__, "glReadPixels");
            } catch (const std::exception& e)
            {
                DELARR(fbData);
                throw e;
            }

            // Copy to output buffer
            {
                uint32_t obSize = inputWidth * inputHeight * 4;
                data->outputBuffer.resize(obSize);

                float convMultiplier = data->binaryInput->convMultiplier;
                uint32_t redIndexFb, redIndexOutput;
                for (uint32_t y = 0; y < inputHeight; y++)
                {
                    for (uint32_t x = 0; x < inputWidth; x++)
                    {
                        redIndexFb = ((inputHeight - 1 - y) * inputWidth + x) * 4;
                        redIndexOutput = (y * inputWidth + x) * 4;

                        data->outputBuffer[redIndexOutput + 0] = fbData[redIndexFb + 0] * convMultiplier;
                        data->outputBuffer[redIndexOutput + 1] = fbData[redIndexFb + 1] * convMultiplier;
                        data->outputBuffer[redIndexOutput + 2] = fbData[redIndexFb + 2] * convMultiplier;
                        data->outputBuffer[redIndexOutput + 3] = 1;
                    }
                }
            }

            data->success = true;
            data->error = "Success";
            data->done = true;
        } catch (const std::exception& e)
        {
            data->setError(formatErr(__FUNCTION__, e.what()));
        }

        // Clean up
        try
        {
            DELARR(fbData);

            try { frameBuffer = nullptr; } catch (...) {}

            glDeleteTextures(1, &(instance->m_texKernel));

            glDeleteProgram(instance->m_shaderProgram);
            glDeleteShader(instance->m_vertexShader);
            glDeleteShader(instance->m_geometryShader);
            glDeleteShader(instance->m_fragmentShader);

            glDeleteBuffers(1, &(instance->m_vbo));
            glDeleteVertexArrays(1, &(instance->m_vao));

            checkGlStatus(__FUNCTION__, "Cleanup");
        } catch (const std::exception& e)
        {
            printErr(__FUNCTION__, e.what());
        }
    }

    void ConvolutionGPUData::setError(std::string message)
    {
        success = false;
        error = message;
        done = true;
    }

}