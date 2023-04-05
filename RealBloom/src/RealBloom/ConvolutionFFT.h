#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <complex>
#include <memory>
#include <cstdint>
#include <cmath>

#include "pocketfft/pocketfft_hdronly.h"

#include "Convolution.h"
#include "../Utils/Array2D.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Misc.h"

namespace RealBloom
{

    class ConvolutionFFT
    {
    private:
        ConvolutionParams m_params;

        float* m_inputBuffer;
        uint32_t m_inputWidth;
        uint32_t m_inputHeight;

        float* m_kernelBuffer;
        uint32_t m_kernelWidth;
        uint32_t m_kernelHeight;

        uint32_t m_paddedWidth = 0;
        uint32_t m_paddedHeight = 0;
        uint32_t m_inputLeftPadding = 0;
        uint32_t m_inputTopPadding = 0;
        uint32_t m_kernelLeftPadding = 0;
        uint32_t m_kernelTopPadding = 0;

        Array2D<float> m_inputPadded[3];
        Array2D<float> m_kernelPadded[3];

        Array2D<std::complex<float>> m_inputFT[3];
        Array2D<std::complex<float>> m_kernelFT[3];
        Array2D<std::complex<float>> m_mulFT[3];
        Array2D<float> m_iFFT[3];

        std::vector<float> m_outputBuffer;

    public:
        ConvolutionFFT(
            ConvolutionParams& convParams,
            float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
            float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight);
        ~ConvolutionFFT();

        void pad();
        void inputFFT(uint32_t ch);
        void kernelFFT(uint32_t ch);
        void multiplyOrDivide(uint32_t ch);
        void inverse(uint32_t ch);
        void output();

        const std::vector<float>& getBuffer() const;

    };

}
