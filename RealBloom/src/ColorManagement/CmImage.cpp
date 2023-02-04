#include "CmImage.h"

std::shared_ptr<GlFrameBuffer> CmImage::s_frameBuffer = nullptr;

CmImage::CmImage(const std::string& id, const std::string& name, uint32_t width, uint32_t height, std::array<float, 4> fillColor, bool useExposure, bool useGlobalFB)
    : m_id(id), m_name(name), m_width(width), m_height(height), m_useExposure(useExposure), m_useGlobalFB(useGlobalFB)
{
    lock();
    resize(std::max(width, 1u), std::max(height, 1u), false);
    fill(fillColor, false);
    unlock();
}

CmImage::~CmImage()
{
    std::scoped_lock lock(m_mutex);
}

const std::string& CmImage::getID() const
{
    return m_id;
}

const std::string& CmImage::getName() const
{
    return m_name;
}

const std::string& CmImage::getSourceName() const
{
    return m_sourceName;
}

void CmImage::setSourceName(const std::string& sourceName)
{
    m_sourceName = sourceName;
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
    return m_imageData.size();
}

float* CmImage::getImageData()
{
    return m_imageData.data();
}

uint32_t CmImage::getGlTexture()
{
    if (m_moveToGpu)
    {
        m_moveToGpu = false;
        moveToGPU_Internal();
    }

    if (CMS::usingGPU())
    {
        if (m_useGlobalFB && (s_frameBuffer.get() != nullptr))
        {
            return s_frameBuffer->getColorBuffer();
        }
        else if (!m_useGlobalFB && (m_localFrameBuffer.get() != nullptr))
        {
            return m_localFrameBuffer->getColorBuffer();
        }
    }
    else if (m_texture.get())
    {
        return m_texture->getTexture();
    }

    return 0;
}

void CmImage::lock()
{
    m_mutex.lock();
}

void CmImage::unlock()
{
    m_mutex.unlock();
}

void CmImage::moveToGPU()
{
    m_moveToGpu = true;
}

void CmImage::resize(uint32_t newWidth, uint32_t newHeight, bool shouldLock)
{
    if (shouldLock) lock();

    if ((m_imageData.size() > 0) && (m_width == newWidth) && (m_height == newHeight))
    {
        if (shouldLock) unlock();
        return;
    }

    if (newWidth < 1 || newHeight < 1)
    {
        if (shouldLock) unlock();
        return;
    }

    clearVector(m_imageData);

    m_width = newWidth;
    m_height = newHeight;

    m_imageData.resize(m_width * m_height * 4);

    if (shouldLock) unlock();
}

void CmImage::fill(std::array<float, 4> color, bool shouldLock)
{
    if (shouldLock) lock();

    for (size_t i = 0; i < m_imageData.size(); i += 4)
    {
        m_imageData[i + 0] = color[0];
        m_imageData[i + 1] = color[1];
        m_imageData[i + 2] = color[2];
        m_imageData[i + 3] = color[3];
    }

    if (shouldLock) unlock();
}

void CmImage::fill(std::vector<float> buffer, bool shouldLock)
{
    if (shouldLock) lock();

    std::copy(buffer.data(), buffer.data() + std::min(m_imageData.size(), buffer.size()), m_imageData.data());

    if (shouldLock) unlock();
}

void CmImage::fill(float* buffer, bool shouldLock)
{
    if (shouldLock) lock();

    std::copy(buffer, buffer + m_imageData.size(), m_imageData.data());

    if (shouldLock) unlock();
}

void CmImage::renderUV()
{
    std::scoped_lock lock(m_mutex);

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

void CmImage::moveToGPU_Internal()
{
    std::scoped_lock lock(m_mutex);

    // Apply View Transform
    static bool lastResult = false;
    try
    {
        applyViewTransform(
            m_imageData.data(),
            m_width,
            m_height,
            m_useExposure ? CMS::getExposure() : 0.0f,
            m_texture,
            m_useGlobalFB ? s_frameBuffer : m_localFrameBuffer,
            !lastResult,
            true,
            false);
        lastResult = true;
    }
    catch (const std::exception& e)
    {
        printError(__FUNCTION__, "", e.what());
        lastResult = false;
    }
}

void CmImage::applyViewTransform(
    float* buffer,
    uint32_t width,
    uint32_t height,
    float exposure,
    std::shared_ptr<GlTexture>& texture,
    std::shared_ptr<GlFrameBuffer>& frameBuffer,
    bool recreate,
    bool uploadToGPU,
    bool readback,
    std::vector<float>* outBuffer)
{
    try
    {
        CMS::ensureOK();
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }

    uint32_t size = width * height * 4;
    float exposureMul = powf(2, exposure);
    bool gpuMode = CMS::usingGPU() && uploadToGPU;

    // Temporary buffer to apply transforms on (CPU)
    std::vector<float> transBuffer;

    // Color Transform (CPU)
    if (!gpuMode)
    {
        transBuffer.resize(size);
        std::copy(buffer, buffer + size, transBuffer.data());

        if (exposure != 0.0f)
            for (uint32_t i = 0; i < size; i++)
                if (i % 4 != 3) transBuffer[i] *= exposureMul;

        try
        {
            OCIO::PackedImageDesc img(
                transBuffer.data(),
                width,
                height,
                OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
                OCIO::BitDepth::BIT_DEPTH_F32,
                4,                 // 4 bytes to go to the next color channel
                4 * 4,             // 4 color channels * 4 bytes per channel (till the next pixel)
                width * 4 * 4);  // width * 4 channels * 4 bytes (till the next row)

            CMS::getCpuProcessor()->apply(img);
        }
        catch (std::exception& e)
        {
            throw std::exception(makeError(__FUNCTION__, "Color Transform (CPU)", e.what()).c_str());
        }
    }

    // Recreate the texture if needed
    if (uploadToGPU)
    {
        bool mustRecreate = false;

        if (texture.get() == nullptr)
            mustRecreate = true;
        else if ((texture->getWidth() != width) || (texture->getHeight() != height))
            mustRecreate = true;

        mustRecreate |= recreate;

        if (mustRecreate)
        {
            try
            {
                texture = std::make_shared<GlTexture>(width, height, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR, GL_RGBA32F);
            }
            catch (const std::exception& e)
            {
                throw std::exception(makeError(__FUNCTION__, "Create texture", e.what()).c_str());
            }
        }
    }

    // Upload the texture to the GPU
    if (uploadToGPU)
    {
        try
        {
            if (!gpuMode && (transBuffer.size() > 0))
                texture->upload(transBuffer.data());
            else
                texture->upload(buffer);
        }
        catch (const std::exception& e)
        {
            throw std::exception(makeError(__FUNCTION__, "Upload texture", e.what()).c_str());
        }
    }

    // Color Transform (GPU)
    if (gpuMode)
    {
        try
        {
            // Recreate the frame buffer if needed
            {
                bool mustRecreate = false;

                if (frameBuffer.get() == nullptr)
                    mustRecreate = true;
                else if ((frameBuffer->getWidth() != width) || (frameBuffer->getHeight() != height))
                    mustRecreate = true;

                mustRecreate |= recreate;

                if (mustRecreate)
                {
                    frameBuffer = std::make_shared<GlFrameBuffer>(width, height);
                }
            }

            // Bind the frame buffer so we can render the transformed image into it
            frameBuffer->bind();

            // Set the viewport
            frameBuffer->viewport();

            // Use the shader program
            std::shared_ptr<OcioShader> shader = CMS::getShader();
            shader->useProgram();

            // Set the input uniforms
            texture->bind(GL_TEXTURE0);
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
                checkGlStatus("", "VAO");

                // Create a Vertex Buffer Object and copy the vertex data to it
                glGenBuffers(1, &vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
                checkGlStatus("", "VBO");
            }

            // Clear the buffer
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            checkGlStatus("", "Clear");

            // Draw
            {
                glBindVertexArray(vao);
                checkGlStatus("", "glBindVertexArray(vao)");

                shader->enableAttribs();

                glDrawArrays(GL_TRIANGLES, 0, 6);
                checkGlStatus("", "glDrawArrays");

                shader->disableAttribs();

                glBindVertexArray(0);
                checkGlStatus("", "glBindVertexArray(0)");
            }

            // Unbind the frame buffer so we can continue rendering to the screen
            frameBuffer->unbind();

            // Clean up
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            checkGlStatus("", "Cleanup");
        }
        catch (const std::exception& e)
        {
            throw std::exception(makeError(__FUNCTION__, "Color Transform (GPU)", e.what()).c_str());
        }
    }

    // Read back the result

    if (!readback || (outBuffer == nullptr)) return;

    if (!gpuMode)
    {
        *outBuffer = transBuffer;
        return;
    }

    try
    {
        outBuffer->resize(size);

        glBindTexture(GL_TEXTURE_2D, frameBuffer->getColorBuffer());
        checkGlStatus("", "glBindTexture");

        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, outBuffer->data());
        checkGlStatus("", "glGetTexImage");
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "Read back (GPU)", e.what()).c_str());
    }
}

void CmImage::cleanUp()
{
    s_frameBuffer = nullptr;
}
