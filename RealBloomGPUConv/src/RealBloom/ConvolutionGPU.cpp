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
    void setFailure(ConvolutionGPUData* data, std::string err)
    {
        if (data)
        {
            data->success = false;
            data->error = err;
            data->done = true;
        }
    }

    // returns true if we're good to go
    bool checkErrors(ConvolutionGPUData* data, const char* operation, std::string& errorList)
    {
        if (!checkGlErrors(errorList))
        {
            setFailure(data, stringFormat("OpenGL Error in \"%s\": %s", operation, errorList.c_str()));
            return false;
        }
        return true;
    }

    void init(void* pdata);

    void ConvolutionGPU::initGL()
    {
        std::string err1;

        if (!oglOneTimeContext(
            m_frameWidth,
            m_frameHeight,
            this,
            init,
            [&](std::string err2)
            {
                setFailure(m_data, stringFormat("OpenGL Initialization Error: \"%s\"", err2.c_str()));
            },
            err1))
        {
            setFailure(m_data, stringFormat("OpenGL Initialization Error: \"%s\"", err1.c_str()));
        }
    }

    void ConvolutionGPU::makeFrameBuffer()
    {
        std::string errorList = "";

        glGenFramebuffers(1, &m_frameBuffer);
        if (!checkErrors(m_data, "makeFrameBuffer: glGenFramebuffers", errorList)) return;

        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        if (!checkErrors(m_data, "makeFrameBuffer: glBindFramebuffer", errorList)) return;

        glViewport(0, 0, m_frameWidth, m_frameHeight);
        if (!checkErrors(m_data, "makeFrameBuffer: glViewport", errorList)) return;

        // Make a color buffer for our frame buffer (render target)
        glGenTextures(1, &m_texColorBuffer);
        glBindTexture(GL_TEXTURE_2D, m_texColorBuffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_frameWidth, m_frameHeight, 0, GL_RGBA, GL_FLOAT, NULL);

        if (!checkErrors(m_data, "makeFrameBuffer: Color Buffer Generation for the Framebuffer", errorList)) return;

        // Assign the color buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texColorBuffer, 0);
        if (!checkErrors(m_data, "makeFrameBuffer: glFramebufferTexture2D", errorList)) return;

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (!checkErrors(m_data, "makeFrameBuffer: glCheckFramebufferStatus", errorList)) return;

        if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
        {
            setFailure(m_data, stringFormat("Framebuffer was not ready. Status: %s", toHex(fbStatus)));
        }
    }

    void ConvolutionGPU::definePoints(GLfloat* points, uint32_t numPoints, uint32_t numAttribs)
    {
        std::string errorList = "";

        // Create Vertex Array Object
        glGenVertexArrays(1, &m_vao);
        if (!checkErrors(m_data, "definePoints: glGenVertexArrays", errorList)) return;

        glBindVertexArray(m_vao);
        if (!checkErrors(m_data, "definePoints: glBindVertexArray", errorList)) return;

        // Create a Vertex Buffer Object and copy the vertex data to it
        glGenBuffers(1, &m_vbo);
        if (!checkErrors(m_data, "definePoints: Vertex Buffer Object Generation", errorList)) return;

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        if (!checkErrors(m_data, "definePoints: glBindBuffer", errorList)) return;

        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * numPoints * numAttribs, points, GL_STATIC_DRAW);
        if (!checkErrors(m_data, "definePoints: glBufferData", errorList)) return;

        /*Example:
        {
             X      Y     R     G     B
            -0.5f,  0.5f, 1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 1.0f, 1.0f, 1.0f,
        }*/
    }

    void ConvolutionGPU::makeProgram()
    {
        std::string errorList = "";

        // Create and compile shaders
        if (!createShader("Vertex Shader", GL_VERTEX_SHADER, vertexSource, m_vertexShader))
        {
            setFailure(m_data, "Vertex shader could not be compiled.");
            return;
        }

        if (!createShader("Geometry Shader", GL_GEOMETRY_SHADER, geometrySource, m_geometryShader))
        {
            setFailure(m_data, "Geometry shader could not be compiled.");
            return;
        }

        if (!createShader("Fragment Shader", GL_FRAGMENT_SHADER, fragmentSource, m_fragmentShader))
        {
            setFailure(m_data, "Fragment shader could not be compiled.");
            return;
        }

        // Link the vertex and fragment shader into a shader program
        m_shaderProgram = glCreateProgram();
        if (!checkErrors(m_data, "makeProgram: glCreateProgram", errorList)) return;

        glAttachShader(m_shaderProgram, m_vertexShader);
        if (!checkErrors(m_data, "makeProgram: glAttachShader (Vertex Shader)", errorList)) return;

        glAttachShader(m_shaderProgram, m_geometryShader);
        if (!checkErrors(m_data, "makeProgram: glAttachShader (Geometry Shader)", errorList)) return;

        glAttachShader(m_shaderProgram, m_fragmentShader);
        if (!checkErrors(m_data, "makeProgram: glAttachShader (Fragment Shader)", errorList)) return;

        glBindFragDataLocation(m_shaderProgram, 0, "outColor");
        if (!checkErrors(m_data, "makeProgram: glBindFragDataLocation", errorList)) return;

        glLinkProgram(m_shaderProgram);
        if (!checkErrors(m_data, "makeProgram: glLinkProgram", errorList)) return;

        glUseProgram(m_shaderProgram);
        if (!checkErrors(m_data, "makeProgram: glUseProgram", errorList)) return;

        // Specify the layout of the vertex data
        GLint posAttrib = glGetAttribLocation(m_shaderProgram, "pos");
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
        if (!checkErrors(m_data, "makeProgram: Vertex buffer layout specification (pos)", errorList)) return;

        GLint colAttrib = glGetAttribLocation(m_shaderProgram, "color");
        glEnableVertexAttribArray(colAttrib);
        glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
        if (!checkErrors(m_data, "makeProgram: Vertex buffer layout specification (color)", errorList)) return;
    }

    void ConvolutionGPU::makeKernelTexture(float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight, float* kernelTopLeft, float* kernelSize)
    {
        std::string errorList = "";

        // Set uniforms
        GLint kernelTopLeftUniform = glGetUniformLocation(m_shaderProgram, "kernelTopLeft");
        if (!checkErrors(m_data, "makeKernelTexture: glGetUniformLocation (kernelTopLeft)", errorList)) return;
        glUniform2f(kernelTopLeftUniform, kernelTopLeft[0], kernelTopLeft[1]);
        if (!checkErrors(m_data, "makeKernelTexture: glUniform2f (kernelTopLeftUniform)", errorList)) return;

        GLint kernelSizeUniform = glGetUniformLocation(m_shaderProgram, "kernelSize");
        if (!checkErrors(m_data, "makeKernelTexture: glGetUniformLocation (kernelSize)", errorList)) return;
        glUniform2f(kernelSizeUniform, kernelSize[0], kernelSize[1]);
        if (!checkErrors(m_data, "makeKernelTexture: glUniform2f (kernelSizeUniform)", errorList)) return;

        // Generate a texture
        glGenTextures(1, &m_texKernel);
        if (!checkErrors(m_data, "makeKernelTexture: glGenTextures", errorList)) return;

        glActiveTexture(GL_TEXTURE0);
        if (!checkErrors(m_data, "makeKernelTexture: glActiveTexture", errorList)) return;

        glBindTexture(GL_TEXTURE_2D, m_texKernel);
        if (!checkErrors(m_data, "makeKernelTexture: glBindTexture", errorList)) return;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!checkErrors(m_data, "makeKernelTexture: glTexParameteri", errorList)) return;

        GLint texKernelUniform = glGetUniformLocation(m_shaderProgram, "texKernel");
        if (!checkErrors(m_data, "makeKernelTexture: glGetUniformLocation (texKernel)", errorList)) return;

        glUniform1i(texKernelUniform, 0);
        if (!checkErrors(m_data, "makeKernelTexture: glUniform1i (texKernelUniform)", errorList)) return;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kernelWidth, kernelHeight, 0, GL_RGBA, GL_FLOAT, kernelBuffer);
        if (!checkErrors(m_data, "makeKernelTexture: glTexImage2D", errorList)) return;
    }

    void ConvolutionGPU::drawScene(uint32_t numPoints)
    {
        std::string errorList = "";

        // Clear the screen to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (!checkErrors(m_data, "drawScene: glClearColor", errorList)) return;

        glClear(GL_COLOR_BUFFER_BIT);
        if (!checkErrors(m_data, "drawScene: glClear", errorList)) return;

        glBlendFunc(GL_ONE, GL_ONE);
        if (!checkErrors(m_data, "drawScene: glBlendFunc", errorList)) return;

        glDrawArrays(GL_POINTS, 0, numPoints);
        if (!checkErrors(m_data, "drawScene: glDrawArrays", errorList)) return;
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
        m_frameWidth = binInput->inputWidth;
        m_frameHeight = binInput->inputHeight;

        uint32_t inputWidth = binInput->inputWidth;
        uint32_t inputHeight = binInput->inputHeight;
        uint32_t inputPixels = inputWidth * inputHeight;
        uint32_t kernelWidth = binInput->kernelWidth;
        uint32_t kernelHeight = binInput->kernelHeight;
        float threshold = binInput->cp_convThreshold;

        float kernelCenterX = (int)floorf(binInput->cp_kernelCenterX * (float)kernelWidth);
        float kernelCenterY = (int)floorf(binInput->cp_kernelCenterY * (float)kernelHeight);

        float offsetX = floorf((float)kernelWidth / 2.0f) - kernelCenterX;
        float offsetY = floorf((float)kernelHeight / 2.0f) - kernelCenterY;

        float v;
        float px, py;
        float color[3]{ 0, 0, 0 };
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

                v = color[0];
                if (color[1] > v)
                    v = color[1];
                if (color[2] > v)
                    v = color[2];

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

                    m_data->points.push_back(px);
                    m_data->points.push_back(py);
                    m_data->points.push_back(color[0]);
                    m_data->points.push_back(color[1]);
                    m_data->points.push_back(color[2]);

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
        uint32_t inputWidth, inputHeight, kernelWidth, kernelHeight, numPoints;
        inputWidth = binInput->inputWidth;
        inputHeight = binInput->inputHeight;
        kernelWidth = binInput->kernelWidth;
        kernelHeight = binInput->kernelHeight;
        numPoints = data->numPoints;

        // For error checking
        std::string errorList = "";

        // Grab the name of the renderer
        const char* rendererS = (const char*)glGetString(GL_RENDERER);
        checkErrors(data, "init: glGetString(GL_RENDERER)", errorList);

        if (!(data->done) && rendererS)
        {
            data->gpuName = rendererS;
        }

        // Capabilities
        if (!(data->done))
        {            
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            checkErrors(data, "init: Capability Management", errorList);
        }

        if (!(data->done))
        {
            instance->makeFrameBuffer();
        }

        if (!(data->done))
        {
            instance->definePoints(data->points.data(), numPoints, 5);
        }

        if (!(data->done))
        {
            instance->makeProgram();
        }

        if (!(data->done))
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

        if (!(data->done))
        {
            instance->drawScene(numPoints);
        }

        if (!(data->done))
        {
            uint32_t fbSize = inputWidth * inputHeight * 4;
            GLfloat* fbData = new float[fbSize];

            glBindFramebuffer(GL_READ_FRAMEBUFFER, instance->m_frameBuffer);
            checkErrors(data, "init: glBindFramebuffer", errorList);

            if (!(data->done))
            {
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                checkErrors(data, "init: glReadBuffer", errorList);
            }

            if (!(data->done))
            {
                glReadPixels(0, 0, inputWidth, inputHeight, GL_RGBA, GL_FLOAT, fbData);
                checkErrors(data, "init: glReadPixels", errorList);
            }

            if (!(data->done))
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

            delete[] fbData;
        }

        // Clean up
        glDeleteTextures(1, &(instance->m_texColorBuffer));
        glDeleteFramebuffers(1, &(instance->m_frameBuffer));

        glDeleteTextures(1, &(instance->m_texKernel));

        glDeleteProgram(instance->m_shaderProgram);
        glDeleteShader(instance->m_vertexShader);
        glDeleteShader(instance->m_geometryShader);
        glDeleteShader(instance->m_fragmentShader);

        glDeleteBuffers(1, &(instance->m_vbo));
        glDeleteVertexArrays(1, &(instance->m_vao));

        if (!(data->done))
        {
            data->success = true;
            data->error = "Success";
            data->done = true;
        }
    }

}