#pragma once

#include <array>
#include <vector>
#include <cstdint>

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
        float exposure = 0.0f;
        float contrast = 0.0f;
        std::array<float, 3> filter{ 1.0f, 1.0f, 1.0f };
        bool mono = false;
        RgbToMonoMethod monoMethod = RgbToMonoMethod::Luminance;

        void reset();
    };

    CropResizeParams cropResize;
    TransformParams transform;
    ColorParams color;

    void reset();
};

class ImageTransform
{
public:
    ImageTransform() = delete;
    ImageTransform(const ImageTransform&) = delete;
    ImageTransform& operator= (const ImageTransform&) = delete;

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
        uint32_t& outputHeight);

private:
    static bool USE_GPU;

    static void applyCPU(
        const ImageTransformParams& params,
        const std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        std::vector<float>& outputBuffer,
        uint32_t& outputWidth,
        uint32_t& outputHeight);

    static void applyGPU(
        const ImageTransformParams& params,
        const std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        std::vector<float>& outputBuffer,
        uint32_t& outputWidth,
        uint32_t& outputHeight);

    static float getPreviewMarkValue(float originX, float originY, float x, float y, uint32_t bufferWidth, uint32_t bufferHeight);

};