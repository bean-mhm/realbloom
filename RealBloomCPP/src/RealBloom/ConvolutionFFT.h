#pragma once

#include <string>
#include <vector>
#include <stdint.h>

#include "Convolution.h"

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

        std::vector<float> m_inputPadded;
        std::vector<float> m_kernelPadded;
        std::vector<float> m_inputFT;
        std::vector<float> m_kernelFT;
        std::vector<float> m_convFT;
        std::vector<float> m_outputBuffer;

    public:
        ConvolutionFFT(
            ConvolutionParams& convParams,
            float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
            float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight);
        ~ConvolutionFFT();

        void pad();
        void inputFFT();
        void kernelFFT();
        void multiply();
        void iFFT();

        std::vector<float>& getBuffer();
    };

}