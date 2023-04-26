#pragma once

#include <array>
#include <vector>
#include <cstdint>

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "OpenGL/GlTexture.h"
#include "OpenGL/GlFrameBuffer.h"
#include "OpenGL/GlFullPlaneVertices.h"
#include "OpenGL/GlUtils.h"

#include "Bilinear.h"
#include "NumberHelpers.h"
#include "Misc.h"

struct ImageTransformParams
{
    struct CropResizeParams
    {
        std::array<float, 2> crop{ 1.0f, 1.0f };
        std::array<float, 2> resize{ 1.0f, 1.0f };
        std::array<float, 2> origin{ 0.5f, 0.5f };
        bool previewOrigin = false;

        void reset();
    };

    struct TransformParams
    {
        std::array<float, 2> scale{ 1.0f, 1.0f };
        float rotate = 0.0f;
        std::array<float, 2> translate{ 0.0f, 0.0f };
        std::array<float, 2> origin{ 0.5f, 0.5f };
        bool previewOrigin = false;

        void reset();
    };

    struct ColorParams
    {
        std::array<float, 3> filter{ 1.0f, 1.0f, 1.0f };
        float exposure = 0.0f;
        float contrast = 0.0f;
        GrayscaleType grayscaleType = GrayscaleType::None;

        void reset();
    };

    CropResizeParams cropResize;
    TransformParams transform;
    ColorParams color;
    bool transparency = false;

    void reset();
};

class ImageTransform
{
private:
    static GLuint s_vertShader;
    static GLuint s_fragShader;
    static GLuint s_program;

    static bool s_gpuInitialized;
    static void ensureInitGPU();

    static void applyNoCropCPU(
        const ImageTransformParams& params,
        const std::vector<float>* lastBuffer,
        uint32_t lastBufferWidth,
        uint32_t lastBufferHeight,
        std::vector<float>& outputBuffer,
        uint32_t resizedWidth,
        uint32_t resizedHeight,
        float resizeX,
        float resizeY,
        bool previewMode);

    static void applyNoCropGPU(
        const ImageTransformParams& params,
        const std::vector<float>* lastBuffer,
        uint32_t lastBufferWidth,
        uint32_t lastBufferHeight,
        std::vector<float>& outputBuffer,
        uint32_t resizedWidth,
        uint32_t resizedHeight,
        float resizeX,
        float resizeY,
        bool previewMode);

    static float getPreviewMarkValue(float x, float y, float originX, float originY, float squareRadius);

public:
    static bool S_USE_GPU;

    ImageTransform() = delete;
    ImageTransform(const ImageTransform&) = delete;
    ImageTransform& operator= (const ImageTransform&) = delete;

    static void cleanUp();

    static void getOutputDimensions(
        const ImageTransformParams& params,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t& outCroppedWidth,
        uint32_t& outCroppedHeight,
        float& outCropX,
        float& outCropY,
        uint32_t& outResizedWidth,
        uint32_t& outResizedHeight,
        float& outResizeX,
        float& outResizeY
    );

    static void getOutputDimensions(
        const ImageTransformParams& params,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t& outputWidth,
        uint32_t& outputHeight
    );

    static void apply(
        const ImageTransformParams& params,
        const std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        std::vector<float>& outputBuffer,
        uint32_t& outputWidth,
        uint32_t& outputHeight,
        bool previewMode);

};
