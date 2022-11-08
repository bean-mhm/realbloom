#include "CMImage.h"

using namespace std;

std::shared_ptr<GlFrameBuffer> CmImage::s_frameBuffer = nullptr;

CmImage::CmImage(const std::string& id, const std::string& name, uint32_t width, uint32_t height, std::array<float, 4> fillColor)
    : m_id(id), m_name(name), m_width(width), m_height(height)
{
    resize(width, height, true);
    fill(fillColor, true);
}

CmImage::~CmImage()
{
    lock_guard<mutex> lock(m_mutex);

    if (m_imageData)
    {
        delete[] m_imageData;
    }
}

void CmImage::cleanUp()
{
    s_frameBuffer = nullptr;
}

std::string CmImage::getID()
{
    return m_id;
}

std::string CmImage::getName()
{
    return m_name;
}

void CmImage::resize(uint32_t newWidth, uint32_t newHeight, bool shouldLock)
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

void CmImage::fill(color_t color, bool shouldLock)
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

void CmImage::fill(std::vector<float> buffer, bool shouldLock)
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

void CmImage::fill(float* buffer, bool shouldLock)
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

void CmImage::renderUV()
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

void CmImage::lock()
{
    m_mutex.lock();
}

void CmImage::unlock()
{
    m_mutex.unlock();
}

uint32_t CmImage::getWidth() const
{
    return m_width;
}

uint32_t CmImage::getHeight() const
{
    return m_height;
}

uint32_t CmImage::getImageDataSize() const
{
    return m_imageDataSize;
}

float* CmImage::getImageData()
{
    return m_imageData;
}

void CmImage::moveToGPU()
{
    m_moveToGpu = true;
}

void CmImage::moveToGPU_Internal()
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
    if (!CMS_USE_GPU && CMS::ok())
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
        try
        {
            m_texture = std::make_shared<GlTexture>(m_width, m_height, GL_CLAMP, GL_LINEAR, GL_LINEAR, GL_RGBA32F);
            lastTextureFailed = false;
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
    if (CMS_USE_GPU && CMS::ok() && !lastTextureFailed)
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
                    try
                    {
                        s_frameBuffer = std::make_shared<GlFrameBuffer>(m_width, m_height);
                        fbFailed = false;
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

            // Use the LUTs and uniforms associated with the shader
            shader->useLuts();
            shader->useUniforms();

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
}

uint32_t CmImage::getGlTexture()
{
    if (m_moveToGpu)
    {
        m_moveToGpu = false;
        moveToGPU_Internal();
    }

    if (CMS_USE_GPU && s_frameBuffer.get())
        return s_frameBuffer->getColorBuffer();
    if (m_texture.get())
        return m_texture->getTexture();
    return 0;
}
