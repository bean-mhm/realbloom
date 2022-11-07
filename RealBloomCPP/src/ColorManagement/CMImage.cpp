#include "CMImage.h"

using namespace std;

std::shared_ptr<GlFrameBuffer> CMImage::s_frameBuffer = nullptr;

CMImage::CMImage(const std::string& id, const std::string& name, uint32_t width, uint32_t height, std::array<float, 4> fillColor)
    : m_id(id), m_name(name), m_width(width), m_height(height)
{
    resize(width, height, true);
    fill(fillColor, true);
}

CMImage::~CMImage()
{
    lock_guard<mutex> lock(m_mutex);

    if (m_imageData)
    {
        delete[] m_imageData;
    }
}

void CMImage::cleanUp()
{
    s_frameBuffer = nullptr;
}

std::string CMImage::getID()
{
    return m_id;
}

std::string CMImage::getName()
{
    return m_name;
}

void CMImage::resize(uint32_t newWidth, uint32_t newHeight, bool shouldLock)
{
    if (shouldLock) lock();

    if ((m_imageData != nullptr) && (m_width == newWidth) && (m_height == newHeight))
    {
        if (shouldLock) unlock();
        return;
    }

    if (newWidth < 2 || newHeight < 2)
    {
        if (shouldLock) unlock();
        return;
    }

    if (m_imageData)
    {
        delete[] m_imageData;
        m_imageData = nullptr;
    }

    m_width = newWidth;
    m_height = newHeight;

    m_imageDataSize = m_width * m_height * 4;
    m_imageData = new float[m_imageDataSize];

    if (shouldLock) unlock();
}

void CMImage::fill(color_t color, bool shouldLock)
{
    if (shouldLock) lock();
    if (!m_imageData)
    {
        if (shouldLock) unlock();
        return;
    }

    for (size_t i = 0; i < m_imageDataSize; i += 4)
    {
        m_imageData[i] = color[0];
        m_imageData[i + 1] = color[1];
        m_imageData[i + 2] = color[2];
        m_imageData[i + 3] = color[3];
    }

    if (shouldLock) unlock();
}

void CMImage::fill(std::vector<float> buffer, bool shouldLock)
{
    if (shouldLock) lock();
    if (!m_imageData)
    {
        if (shouldLock) unlock();
        return;
    }

    std::copy(buffer.data(), buffer.data() + std::min(m_imageDataSize, (uint32_t)buffer.size()), m_imageData);
    if (shouldLock) unlock();
}

void CMImage::fill(float* buffer, bool shouldLock)
{
    if (shouldLock) lock();
    if (!m_imageData)
    {
        if (shouldLock) unlock();
        return;
    }

    std::copy(buffer, buffer + m_imageDataSize, m_imageData);
    if (shouldLock) unlock();
}

void CMImage::renderUV()
{
    lock_guard<mutex> lock(m_mutex);

    if (!m_imageData)
        return;

    int redIndex = 0;
    float u, v;
    for (size_t y = 0; y < m_height; y++)
    {
        for (size_t x = 0; x < m_width; x++)
        {
            redIndex = (y * m_width + x) * 4;
            u = ((float)x + 0.5f) / (float)m_width;
            v = ((float)y + 0.5f) / (float)m_height;
            v = 1 - v;

            m_imageData[redIndex] = u;
            m_imageData[redIndex + 1] = v;
            m_imageData[redIndex + 2] = 0;
            m_imageData[redIndex + 3] = 1;
        }
    }
}

void CMImage::lock()
{
    m_mutex.lock();
}

void CMImage::unlock()
{
    m_mutex.unlock();
}

uint32_t CMImage::getWidth() const
{
    return m_width;
}

uint32_t CMImage::getHeight() const
{
    return m_height;
}

uint32_t CMImage::getImageDataSize() const
{
    return m_imageDataSize;
}

float* CMImage::getImageData()
{
    return m_imageData;
}

void CMImage::moveToGPU()
{
    m_moveToGpu = true;
}

void CMImage::moveToGPU_Internal()
{
    lock_guard<mutex> lock(m_mutex);

    if (!m_imageData)
        return;

    bool sizeChanged = false;
    if ((m_oldWidth != m_width) || (m_oldHeight != m_height))
    {
        m_oldWidth = m_width;
        m_oldHeight = m_height;
        sizeChanged = true;
    }

    // Exposure
    float exposure = CMS::getExposure();
    float exposureMul = powf(2, exposure);

    // Temporary buffer to apply transforms on (CPU)
    float* transData = nullptr;

    // Color Transform (CPU)
    if (!CMS_USE_GPU && CMS::hasProcessors())
    {
        transData = new float[m_imageDataSize];
        std::copy(m_imageData, m_imageData + m_imageDataSize, transData);

        if (exposure != 0)
            for (uint32_t i = 0; i < m_imageDataSize; i++)
                if (i % 4 != 3) transData[i] *= exposureMul;
        try
        {
            OCIO::PackedImageDesc img(
                transData,
                m_width,
                m_height,
                OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
                OCIO::BitDepth::BIT_DEPTH_F32,
                4,                 // 4 bytes to go to the next color channel
                4 * 4,             // 4 color channels * 4 bytes per channel (till the next pixel)
                m_width * 4 * 4);  // width * 4 channels * 4 bytes (till the next row)

            CMS::getCpuProcessor()->apply(img);
        } catch (std::exception& exception)
        {
            printErr(__FUNCTION__, "Color Transform (CPU)", exception.what());
        }
    }

    static bool lastTextureFailed = true;

    // Recreate the texture if the size has changed or if it's in error state
    if (sizeChanged || lastTextureFailed)
    {
        lastTextureFailed = false;
        try
        {
            m_texture = std::make_shared<GlTexture>(m_width, m_height, GL_CLAMP, GL_LINEAR, GL_LINEAR, GL_RGBA32F);
        } catch (const std::exception&)
        {
            lastTextureFailed = true;
            printErr(__FUNCTION__, "Failed to create texture.");
        }
    }

    if (!lastTextureFailed)
    {
        if (!CMS_USE_GPU && (transData != nullptr))
            m_texture->upload(transData);
        else
            m_texture->upload(m_imageData);
    }

    // Clean up
    DELARR(transData);

    static bool fbFailed = true;

    // Color Transform (GPU)
    if (CMS_USE_GPU && CMS::hasProcessors() && !lastTextureFailed)
    {
        try
        {
            // Recreate our frame buffer if needed
            {
                bool mustRecreate = false;
                if (s_frameBuffer.get() == nullptr)
                {
                    mustRecreate = true;
                } else
                {
                    if ((s_frameBuffer->getWidth() != m_width) || (s_frameBuffer->getHeight() != m_height))
                        mustRecreate = true;
                    else if (fbFailed)
                        mustRecreate = true;
                }

                if (mustRecreate)
                {
                    fbFailed = false;
                    try
                    {
                        s_frameBuffer = std::make_shared<GlFrameBuffer>(m_width, m_height);
                    } catch (const std::exception& e)
                    {
                        fbFailed = true;
                        throw e;
                    }
                }
            }

            // Bind the frame buffer so we can render the transformed image into it
            s_frameBuffer->bind();

            // Set the viewport
            s_frameBuffer->viewport();

            // Use the shader program
            std::shared_ptr<OcioShader> shader = CMS::getShader();
            shader->useProgram();

            // Set the input uniforms
            m_texture->bind(GL_TEXTURE0);
            shader->setInputTexture(0);
            shader->setExposureMul(exposureMul);

            // Define vertex data
            GLuint vao;
            GLuint vbo;
            {
                GLfloat vertices[] = {
                    //  Pos           UV
                        -1.0f,  1.0f, 0.0f, 1.0f, // Top-left
                         1.0f,  1.0f, 1.0f, 1.0f, // Top-right
                         1.0f, -1.0f, 1.0f, 0.0f, // Bottom-right
                         1.0f, -1.0f, 1.0f, 0.0f, // Bottom-right
                        -1.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
                        -1.0f,  1.0f, 0.0f, 1.0f  // Top-left
                };

                // Create Vertex Array Object
                glGenVertexArrays(1, &vao);
                glBindVertexArray(vao);
                checkGlStatus(__FUNCTION__, "VAO");

                // Create a Vertex Buffer Object and copy the vertex data to it
                glGenBuffers(1, &vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                checkGlStatus(__FUNCTION__, "VBO");
            }

            // Clear the buffer
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            checkGlStatus(__FUNCTION__, "Clear");

            // Draw
            {
                glBindVertexArray(vao);
                checkGlStatus(__FUNCTION__, "glBindVertexArray(vao)");

                shader->enableAttribs();

                glDrawArrays(GL_TRIANGLES, 0, 6);
                checkGlStatus(__FUNCTION__, "glDrawArrays");

                shader->disableAttribs();

                glBindVertexArray(0);
                checkGlStatus(__FUNCTION__, "glBindVertexArray(0)");
            }

            // Unbind the frame buffer so we can continue rendering to the screen
            s_frameBuffer->unbind();

            // Clean up
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            checkGlStatus(__FUNCTION__, "Cleanup");
        } catch (const std::exception&)
        {
            printErr(__FUNCTION__, "GPU Color Transform failed.");
        }
    }

#if 0
    // (Junk code, will be removed)
    if (CMS_USE_GPU)
    {
        OCIO::GpuShaderDescRcPtr shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
        shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
        shaderDesc->setFunctionName("OCIODisplay");
        shaderDesc->setResourcePrefix("ocio_");

        OCIO::ConstGPUProcessorRcPtr gpuProc = CMS::getGpuProcessor();
        gpuProc->extractGpuShaderInfo(shaderDesc);

        // Create oglBuilder using the shaderDesc.
        OCIO::OpenGLBuilderRcPtr m_oglBuilder = OCIO::OpenGLBuilder::Create(shaderDesc);
        m_oglBuilder->setVerbose(true);

        // Allocate & upload all the LUTs in a dedicated GPU texture.
        // Note: The start index for the texture indices is 1 as one texture
        //       was already created for the input image.
        m_oglBuilder->allocateAllTextures(1);

        std::ostringstream mainShader;
        mainShader
            << std::endl
            << "uniform sampler2D img;" << std::endl
            << std::endl
            << "void main()" << std::endl
            << "{" << std::endl
            << "    vec4 col = texture2D(img, gl_TexCoord[0].st);" << std::endl
            << "    gl_FragColor = " << shaderDesc->getFunctionName() << "(col);" << std::endl
            << "}" << std::endl;

        // Build the fragment shader program.
        m_oglBuilder->buildProgram(mainShader.str().c_str(), false);

        // Enable the fragment shader program, and all needed resources.
        m_oglBuilder->useProgram();

        // The image texture.
        glUniform1i(glGetUniformLocation(m_oglBuilder->getProgramHandle(), "img"), m_glTexture);

        // The LUT textures.
        m_oglBuilder->useAllTextures();

        // Enable uniforms for dynamic properties.
        m_oglBuilder->useAllUniforms();

        // Bind the frame buffer so we can render the transformed image into it.
        m_frameBuffer->bind();

        // Draw
        glViewport(0, 0, m_width, m_height);

        glEnable(GL_TEXTURE_2D);
        glClearColor(0.1, 0.5, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glColor3f(1, 1, 1);

        glPushMatrix();
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-.95, -.95);

        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-.95, .95);

        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(.95, .95);

        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(.95, -.95);
        glEnd();
        glPopMatrix();

        // Detach the frame buffer so we can continue rendering to the screen
        m_frameBuffer->unbind();
    }
#endif
}

uint32_t CMImage::getGlTexture()
{
    if (m_moveToGpu)
    {
        m_moveToGpu = false;
        moveToGPU_Internal();
    }

    if (CMS_USE_GPU && s_frameBuffer.get())
        return s_frameBuffer->getColorBuffer();
    return m_texture->getTexture();
}