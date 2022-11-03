#pragma once

#include <string>
#include <stdint.h>
#include <mutex>
#include <array>
#include <vector>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "CMS.h"

typedef std::array<float, 4> color_t;

// Color Managed Image
// Internal format is always RGBA32F
class CMImage
{
private:
    std::string m_id;
    std::string m_name;

    uint32_t m_width, m_height;
    float* m_imageData = nullptr;
    uint32_t m_imageDataSize = 0;

    std::mutex m_mutex;

    uint32_t m_oldWidth = 0, m_oldHeight = 0;
    GLuint m_glTexture = 0;

    bool m_moveToGpu = false;
    void moveToGPU_Internal();
public:
    CMImage(
        const std::string& id,
        const std::string& name,
        uint32_t width = 128,
        uint32_t height = 128,
        std::array<float, 4> fillColor = { 0, 0, 0, 1 });
    ~CMImage();

    std::string getID();
    std::string getName();

    void lock();
    void unlock();

    uint32_t getWidth() const;
    uint32_t getHeight() const;

    // Number of elements in imageData
    uint32_t getImageDataSize() const;

    // RGBA. Every 4 elements represent a pixel.
    float* getImageData();

    void moveToGPU();
    GLuint getGlTexture();

    // shouldLock must be false if the image is already locked.
    void resize(uint32_t newWidth, uint32_t newHeight, bool shouldLock);

    void fill(color_t color, bool shouldLock);
    void fill(std::vector<float> buffer, bool shouldLock);
    void fill(float* buffer, bool shouldLock);
    void renderUV();
};