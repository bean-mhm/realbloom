#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <math.h>
#include <complex>
#include <memory>

#include "simple_fft/fft_settings.h"
#include "simple_fft/fft.hpp"

#include "Convolution.h"
#include "../Utils/Array2D.h"

namespace RealBloom
{

    class ConvolutionFFT
    {
    private:
        float* m_inputBuffer;
        uint32_t m_inputWidth;
        uint32_t m_inputHeight;

        float* m_kernelBuffer;
        uint32_t m_kernelWidth;
        uint32_t m_kernelHeight;

        uint32_t m_paddedSize = 0;

        Array2D<float> m_inputPadded0[3];
        Array2D<float> m_kernelPadded0[3];

        Array2D<std::complex<float>> m_inputFT0[3];
        Array2D<std::complex<float>> m_kernelFT0[3];

        Array2D<std::complex<float>> m_mulFT0[3];
        Array2D<std::complex<float>> m_iFFT0[3];

        std::vector<float> m_outputBufferV;

    public:
        ConvolutionFFT(
            ConvolutionParams& convParams,
            float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
            float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight);
        ~ConvolutionFFT();

        void pad();
        void inputFFT(uint32_t ch);
        void kernelFFT(uint32_t ch);
        void multiply(uint32_t ch);
        void inverse(uint32_t ch);
        void output();

        uint32_t getPaddedSize() const;
        const std::vector<float>& getBuffer() const;
    };

}