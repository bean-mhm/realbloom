#include "CMImage.h"

using namespace std;

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

    if (m_glTexture)
    {
        glDeleteTextures(1, &m_glTexture);
    }
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

    // Temporary buffer to apply transforms on
    float* transData = new float[m_imageDataSize];
    std::copy(m_imageData, m_imageData + m_imageDataSize, transData);

    // Exposure
    float exposure = CMS::getExposure();
    if (exposure != 0)
    {
        float exposureMul = powf(2, exposure);
        for (uint32_t i = 0; i < m_imageDataSize; i++)
            if (i % 4 != 3) transData[i] *= exposureMul;
    }

    // Color Transform
    try
    {
        OCIO::ConstConfigRcPtr config = CMS::getConfig();
        OCIO::PackedImageDesc img(
            transData,
            m_width,
            m_height,
            OCIO::ChannelOrdering::CHANNEL_ORDERING_RGBA,
            OCIO::BitDepth::BIT_DEPTH_F32,
            4,                 // 4 bytes to go to the next color channel
            4 * 4,             // 4 color channels * 4 bytes per channel
            m_width * 4 * 4);  // width * 4 channels * 4 bytes

        // Display View Transform
        {
            OCIO::DisplayViewTransformRcPtr transform = OCIO::DisplayViewTransform::Create();
            transform->setSrc(OCIO::ROLE_SCENE_LINEAR);
            transform->setDisplay(CMS::getActiveDisplay().c_str());
            transform->setView(CMS::getActiveView().c_str());

            OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
            OCIO::ConstCPUProcessorRcPtr cpuProc = proc->getDefaultCPUProcessor();
            cpuProc->apply(img);
        }

        // Look Transform
        std::string lookName = CMS::getActiveLook();
        if ((!lookName.empty()) && (lookName != "None"))
        {
            OCIO::ConstLookRcPtr look = config->getLook(lookName.c_str());
            OCIO::ConstTransformRcPtr transform = look->getTransform();

            OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
            OCIO::ConstCPUProcessorRcPtr cpuProc = proc->getDefaultCPUProcessor();
            cpuProc->apply(img);
        }
    } catch (OCIO::Exception& exception)
    {
        std::cerr << "OpenColorIO Error [CMImage::moveToGPU_Internal()]: " << exception.what() << std::endl;
    }

    // OpenGL Texture
    GLuint imageTexture = m_glTexture;
    GLuint oldTexture = m_glTexture;
    {
        // Create an OpenGL texture identifier
        if (sizeChanged)
            glGenTextures(1, &imageTexture);
        glBindTexture(GL_TEXTURE_2D, imageTexture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Same
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

        // Upload to GPU
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_FLOAT, transData);
    }

    // Clean up
    delete[] transData;

    m_glTexture = imageTexture;
    if (sizeChanged)
        glDeleteTextures(1, &oldTexture);
}

uint32_t CMImage::getGlTexture()
{
    if (m_moveToGpu)
    {
        m_moveToGpu = false;
        moveToGPU_Internal();
    }
    return m_glTexture;
}