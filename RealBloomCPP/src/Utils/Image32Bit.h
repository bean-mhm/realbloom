#pragma once

#include <string>
#include <stdint.h>
#include <mutex>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include "../imgui/imgui.h"

#include "NumberHelpers.h"

class Image32Bit
{
private:
    uint32_t m_id;
    std::string m_name;

    uint32_t m_width, m_height;
    float* m_imageData = nullptr; // RGBA. Every 4 elements represent a pixel
    uint32_t m_imageDataSize = 0; // Number of elements in imageData

    std::mutex m_mutex;

    uint32_t m_oldWidth = 0, m_oldHeight = 0;
    GLuint m_glTexture = 0;
public:
    Image32Bit(uint32_t id, std::string name, uint32_t width, uint32_t height, ImVec4 fillColor = { 0, 0, 0, 1 });
    Image32Bit(uint32_t id, std::string name, uint32_t squareSize, ImVec4 fillColor = { 0, 0, 0, 1 });
    ~Image32Bit();

    uint32_t getID();
    std::string getName();

    void lock();
    void unlock();

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    void getDimensions(uint32_t& outWidth, uint32_t& outHeight) const;
    uint32_t getImageDataSize() const;
    float* getImageData() const;

    bool moveToGPU();
    GLuint getGlTexture() const;

    void resize(uint32_t newWidth, uint32_t newHeight);
    void fill(ImVec4 color);
    void renderUV();
};