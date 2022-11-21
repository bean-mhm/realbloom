#include "ConvolutionFFT.h"

namespace RealBloom
{

    ConvolutionFFT::ConvolutionFFT(ConvolutionParams& convParams, float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight, float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
        : m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_kernelBuffer(kernelBuffer), m_kernelWidth(kernelWidth), m_kernelHeight(kernelHeight)
    {}

    ConvolutionFFT::~ConvolutionFFT()
    {}

    uint32_t upperPowerOf2(uint32_t v)
    {
        return (uint32_t)floor(pow(2, ceil(log(v) / log(2))));
    }

    void ConvolutionFFT::pad()
    {
        uint32_t totalWidth = m_inputWidth + m_kernelWidth;
        uint32_t totalHeight = m_inputHeight + m_kernelHeight;
        m_paddedSize = upperPowerOf2(std::max(totalWidth, totalHeight));

        std::cout << strFormat(
            "Input:   %u x %u\n"
            "Kernel:  %u x %u\n"
            "Total:   %u x %u\n"
            "Final:   %u x %u\n\n",
            m_inputWidth, m_inputHeight,
            m_kernelWidth, m_kernelHeight,
            totalWidth, totalHeight,
            m_paddedSize, m_paddedSize
        );

        // Input padding
        {
            uint32_t leftPadding = (m_paddedSize - m_inputWidth) / 2;
            uint32_t topPadding = (m_paddedSize - m_inputHeight) / 2;

            for (uint32_t i = 0; i < 3; i++)
            {
                m_inputPadded0[i].resize(m_paddedSize, m_paddedSize);

                int transX, transY;
                uint32_t redIndex;
                for (int y = 0; y < m_inputHeight; y++)
                {
                    for (int x = 0; x < m_inputWidth; x++)
                    {
                        transX = x + leftPadding;
                        transY = y + topPadding;

                        redIndex = (y * m_inputWidth + x) * 4;
                        m_inputPadded0[i](transY, transX) = m_inputBuffer[redIndex + i];
                    }
                }
            }
        }

        // Kernel padding
        {
            uint32_t leftPadding = (m_paddedSize - m_kernelWidth) / 2;
            uint32_t topPadding = (m_paddedSize - m_kernelHeight) / 2;

            for (uint32_t i = 0; i < 3; i++)
            {
                m_kernelPadded0[i].resize(m_paddedSize, m_paddedSize);

                int transX, transY;
                uint32_t redIndex;
                for (int y = 0; y < m_kernelHeight; y++)
                {
                    for (int x = 0; x < m_kernelWidth; x++)
                    {
                        transX = x + leftPadding;
                        transY = y + topPadding;

                        redIndex = (y * m_kernelWidth + x) * 4;
                        m_kernelPadded0[i](transY, transX) = m_kernelBuffer[redIndex + i];
                    }
                }
            }
        }
    }

    void ConvolutionFFT::inputFFT(uint32_t ch)
    {
        m_inputFT0[ch].resize(m_paddedSize, m_paddedSize);

        const char* error = nullptr;
        if (!simple_fft::FFT(m_inputPadded0[ch], m_inputFT0[ch], m_paddedSize, m_paddedSize, error))
            if (error)
                throw std::exception(error);
            else
                throw std::exception("");

        m_inputPadded0[ch].clear();
    }

    void ConvolutionFFT::kernelFFT(uint32_t ch)
    {
        m_kernelFT0[ch].resize(m_paddedSize, m_paddedSize);

        const char* error = nullptr;
        if (!simple_fft::FFT(m_kernelPadded0[ch], m_kernelFT0[ch], m_paddedSize, m_paddedSize, error))
            if (error)
                throw std::exception(error);
            else
                throw std::exception("");

        m_kernelPadded0[ch].clear();
    }

    void ConvolutionFFT::multiply(uint32_t ch)
    {
        m_mulFT0[ch].resize(m_paddedSize, m_paddedSize);

        for (size_t y = 0; y < m_paddedSize; y++)
            for (size_t x = 0; x < m_paddedSize; x++)
                m_mulFT0[ch](y, x) = m_inputFT0[ch](y, x) * m_kernelFT0[ch](y, x);

        m_inputFT0[ch].clear();
        m_kernelFT0[ch].clear();
    }

    void ConvolutionFFT::inverse(uint32_t ch)
    {
        m_iFFT0[ch].resize(m_paddedSize, m_paddedSize);

        const char* error = nullptr;
        if (!simple_fft::IFFT(m_mulFT0[ch], m_iFFT0[ch], m_paddedSize, m_paddedSize, error))
            if (error)
                throw std::exception(error);
            else
                throw std::exception("");

        m_mulFT0[ch].clear();
    }

    void ConvolutionFFT::output()
    {
        uint32_t outputSize = m_paddedSize * m_paddedSize * 4;
        m_outputBufferV.resize(outputSize);

        for (size_t i = 0; i < outputSize; i++)
            if ((i % 4) == 3) m_outputBufferV[i] = 1.0f;
            else m_outputBufferV[i] = 0.0f;

        int transX = 0, transY = 0;
        uint32_t redIndex;
        for (uint32_t y = 0; y < m_paddedSize; y++)
        {
            for (uint32_t x = 0; x < m_paddedSize; x++)
            {
                // Fix the coordinates
                if (x < (m_paddedSize / 2))
                    transX = ((m_paddedSize / 2) - 1) - x;
                else
                    transX = (m_paddedSize - 1) - (x - (m_paddedSize / 2));
                transX = m_paddedSize - 1 - transX;

                if (y < (m_paddedSize / 2))
                    transY = ((m_paddedSize / 2) - 1) - y;
                else
                    transY = (m_paddedSize - 1) - (y - (m_paddedSize / 2));
                transY = m_paddedSize - 1 - transY;

                redIndex = (y * m_paddedSize + x) * 4;

                // Get the real part
                m_outputBufferV[redIndex + 0] = m_iFFT0[0](transY, transX).real();
                m_outputBufferV[redIndex + 1] = m_iFFT0[1](transY, transX).real();
                m_outputBufferV[redIndex + 2] = m_iFFT0[2](transY, transX).real();
            }
        }
    }

    uint32_t ConvolutionFFT::getPaddedSize() const
    {
        return m_paddedSize;
    }

    const std::vector<float>& ConvolutionFFT::getBuffer() const
    {
        return m_outputBufferV;
    }

}
