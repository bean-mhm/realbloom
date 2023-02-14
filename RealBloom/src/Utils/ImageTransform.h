#pragma once

#include <array>
#include <vector>

struct ImageTransform
{
    enum class GrayscaleMethod
    {
        None = 0,
        Weighted = 1,
        Average = 2,
        Maximum = 3
    };

    struct ResizeCropParams
    {
        std::array<float, 2> resize{ 1.0f, 1.0f };
        std::array<float, 2> crop{ 1.0f, 1.0f };
        std::array<float, 2> origin{ 0.5f, 0.5f };
        bool previewOrigin = false;
    };

    struct TransformParams
    {
        std::array<float, 2> translate{ 0.0f, 0.0f };
        std::array<float, 2> scale{ 1.0f, 1.0f };
        float rotate = 0.0f;
        std::array<float, 2> origin{ 0.5f, 0.5f };
        bool previewOrigin = false;
    };

    struct ColorParams
    {
        float exposure = 0.0f;
        float contrast = 0.0f;
        std::array<float, 3> color{ 1.0f, 1.0f, 1.0f };
        GrayscaleMethod grayscaleMethod = GrayscaleMethod::None;
    };

    ResizeCropParams resizeCropParams;
    TransformParams transformParams;
    ColorParams colorParams;

    void apply(const std::vector<float>& inputBuffer, std::vector<float>& outputBuffer);

};
