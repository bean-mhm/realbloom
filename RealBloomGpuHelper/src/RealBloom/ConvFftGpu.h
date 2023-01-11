#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include <string>
#include <vector>
#include <memory>
#include <complex>
#include <stdint.h>

#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryConvFftGpu.h"

#include "Utils/OpenGL/GlUtils.h"
#include "Utils/Array2D.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

namespace RealBloom
{

    class ConvFftGpu
    {
    private:
        BinaryConvFftGpuInput* m_binInput;

        uint32_t m_paddedWidth = 0;
        uint32_t m_paddedHeight = 0;
        uint32_t m_inputLeftPadding = 0;
        uint32_t m_inputTopPadding = 0;
        uint32_t m_kernelLeftPadding = 0;
        uint32_t m_kernelTopPadding = 0;

        Array2D<std::complex<float>> m_inputPadded[3];
        Array2D<std::complex<float>> m_kernelPadded[3];
        Array2D<std::complex<float>> m_inputFT[3];
        Array2D<std::complex<float>> m_kernelFT[3];
        Array2D<std::complex<float>> m_mulFT[3];
        Array2D<std::complex<float>> m_iFFT[3];

        std::vector<float> m_outputBuffer;

    public:
        ConvFftGpu(BinaryConvFftGpuInput* binInput);
        ~ConvFftGpu();

        void pad();
        void inputFFT(uint32_t ch);
        void kernelFFT(uint32_t ch);
        void multiply(uint32_t ch);
        void inverse(uint32_t ch);
        void output();

        const std::vector<float>& getBuffer() const;

    };

}
