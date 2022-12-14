#pragma once

#include <string>
#include <stdint.h>
#include <mutex>
#include <array>
#include <vector>
#include <memory>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "CMS.h"
#include "OcioShader.h"
#include "../Utils/GlTexture.h"
#include "../Utils/GlFrameBuffer.h"
#include "../Utils/GlUtils.h"
#include "../Utils/Misc.h"

// Color Managed Image
// Internal format is always RGBA32F
class CmImage
{
private:
    std::string m_id;
    std::string m_name;
    std::string m_sourceName = "";

    uint32_t m_width, m_height;
    bool m_useExposure = true;
    bool m_useGlobalFB = true;

    float* m_imageData = nullptr;
    uint32_t m_imageDataSize = 0;

    std::mutex m_mutex;

    uint32_t m_oldWidth = 0, m_oldHeight = 0;
    std::shared_ptr<GlTexture> m_texture = nullptr;

    static std::shared_ptr<GlFrameBuffer> s_frameBuffer;
    std::shared_ptr<GlFrameBuffer> m_localFrameBuffer = nullptr;

    bool m_moveToGpu = true;
    void moveToGPU_Internal();

public:
    CmImage(
        const std::string& id,
        const std::string& name,
        uint32_t width = 32,
        uint32_t height = 32,
        std::array<float, 4> fillColor = { 0, 0, 0, 1 },
        bool useExposure = true,
        bool useGlobalFB = true
    );
    ~CmImage();
    static void cleanUp();

    std::string getID();
    std::string getName();

    std::string getSourceName();
    void setSourceName(const std::string& sourceName);

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

    void fill(std::array<float, 4> color, bool shouldLock);
    void fill(std::vector<float> buffer, bool shouldLock);
    void fill(float* buffer, bool shouldLock);
    void renderUV();
};
