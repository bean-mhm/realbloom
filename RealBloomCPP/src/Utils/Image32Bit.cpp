#include "Image32Bit.h"

using namespace std;

Image32Bit::Image32Bit(uint32_t id, std::string name, uint32_t width, uint32_t height, ImVec4 fillColor)
    : m_id(id), m_name(name), m_width(width), m_height(height)
{
    resize(width, height);
    fill(fillColor);
}

Image32Bit::Image32Bit(uint32_t id, std::string name, uint32_t squareSize, ImVec4 fillColor)
    : m_id(id), m_name(name), m_width(squareSize), m_height(squareSize)
{
    resize(squareSize, squareSize);
    fill(fillColor);
}

Image32Bit::~Image32Bit()
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

uint32_t Image32Bit::getID()
{
    return m_id;
}

std::string Image32Bit::getName()
{
    return m_name;
}

void Image32Bit::resize(uint32_t newWidth, uint32_t newHeight)
{
    lock_guard<mutex> lock(m_mutex);

    if ((m_imageData != nullptr) && (m_width == newWidth) && (m_height == newHeight))
        return;

    if (newWidth < 2 || newHeight < 2)
        return;

    if (m_imageData)
    {
        delete[] m_imageData;
        m_imageData = nullptr;
    }

    m_width = newWidth;
    m_height = newHeight;

    m_imageDataSize = m_width * m_height * 4;
    m_imageData = new float[m_imageDataSize];
}

void Image32Bit::fill(ImVec4 color)
{
    lock_guard<mutex> lock(m_mutex);

    if (!m_imageData)
        return;

    for (size_t i = 0; i < m_imageDataSize; i += 4)
    {
        m_imageData[i] = color.x;
        m_imageData[i + 1] = color.y;
        m_imageData[i + 2] = color.z;
        m_imageData[i + 3] = color.w;
    }
}

void Image32Bit::renderUV()
{
    lock_guard<mutex> lock(m_mutex);

    if (!m_imageData)
        return;

    int index = 0;
    float u, v;
    for (size_t y = 0; y < m_height; y++)
    {
        for (size_t x = 0; x < m_width; x++)
        {
            index = (y * m_width + x) * 4;
            u = (float)x / (float)(m_width - 1);
            v = (float)y / (float)(m_height - 1);
            v = 1 - v;

            m_imageData[index] = u;
            m_imageData[index + 1] = v;
            m_imageData[index + 2] = 0;
            m_imageData[index + 3] = 1;
        }
    }
}

void Image32Bit::lock()
{
    m_mutex.lock();
}

void Image32Bit::unlock()
{
    m_mutex.unlock();
}

uint32_t Image32Bit::getWidth() const
{
    return m_width;
}

uint32_t Image32Bit::getHeight() const
{
    return m_height;
}

void Image32Bit::getDimensions(uint32_t& outWidth, uint32_t& outHeight) const
{
    outWidth = m_width;
    outHeight = m_height;
}

uint32_t Image32Bit::getImageDataSize() const
{
    return m_imageDataSize;
}

float* Image32Bit::getImageData() const
{
    return m_imageData;
}

bool Image32Bit::moveToGPU()
{
    if (!m_imageData)
        return false;

    bool sizeChanged = false;
    if (m_oldWidth != m_width || m_oldHeight != m_height)
    {
        m_oldWidth = m_width;
        m_oldHeight = m_height;
        sizeChanged = true;
    }

    lock_guard<mutex> lock(m_mutex);

    // Create a OpenGL texture identifier
    GLuint image_texture = m_glTexture;
    GLuint oldTexture = m_glTexture;
    if (sizeChanged)
        glGenTextures(1, &image_texture);

    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Same
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    uint8_t* imageByteData = new unsigned char[m_imageDataSize];
    float v = 0;
    for (size_t i = 0; i < m_imageDataSize; i++)
    {
        v = m_imageData[i];
        v = linearToSrgb(v);

        imageByteData[i] = (uint8_t)(v * 255);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageByteData);
    delete[] imageByteData;

    m_glTexture = image_texture;
    if (sizeChanged && oldTexture)
        glDeleteTextures(1, &oldTexture);

    return true;
}

GLuint Image32Bit::getGlTexture() const
{
    return m_glTexture;
}