#include "ConvFftGpu.h"

#define DJ_FFT_IMPLEMENTATION // define this in exactly *one* .cpp file
#include "dj_fft/dj_fft.h"

namespace RealBloom
{
    ConvFftGpu::ConvFftGpu(BinaryConvFftGpuInput* binInput)
        : m_binInput(binInput)
    {}

    ConvFftGpu::~ConvFftGpu()
    {}

    void ConvFftGpu::pad()
    {
        // Padded size
        calcFftConvPadding(
            true, true,
            m_binInput->inputWidth, m_binInput->inputHeight,
            m_binInput->kernelWidth, m_binInput->kernelHeight,
            m_binInput->cp_kernelCenterX, m_binInput->cp_kernelCenterY,
            m_paddedWidth, m_paddedHeight
        );

        // Padding amount

        m_inputLeftPadding = floorf((float)(m_paddedWidth - m_binInput->inputWidth) / 2.0f);
        m_inputTopPadding = floorf((float)(m_paddedHeight - m_binInput->inputHeight) / 2.0f);

        m_kernelLeftPadding = floorf(((float)m_paddedWidth / 2.0f) - ((float)m_binInput->kernelWidth * m_binInput->cp_kernelCenterX));
        m_kernelTopPadding = floorf(((float)m_paddedHeight / 2.0f) - ((float)m_binInput->kernelHeight * m_binInput->cp_kernelCenterY));

        if (m_binInput->kernelWidth % 2 == 1) m_kernelLeftPadding += 1;
        if (m_binInput->kernelHeight % 2 == 1) m_kernelTopPadding += 1;

        // Print the dimensions
        if (1)
        {
            printInfo(__FUNCTION__, "", strFormat(
                "FFT Convolution\n"
                "Input:          %u x %u\n"
                "Kernel:         %u x %u\n"
                "Padded:         %u x %u\n",
                m_binInput->inputWidth, m_binInput->inputHeight,
                m_binInput->kernelWidth, m_binInput->kernelHeight,
                m_paddedWidth, m_paddedHeight
            ));
        }

        // Scale the values to achieve a normalized output after convolution
        float fftScale = 1.0f / sqrtf((float)m_paddedWidth * (float)m_paddedHeight);

        // Input padding + threshold
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_inputPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_inputPadded[i].getVector()) v = { 0, 0 };
            }

            float threshold = m_binInput->cp_convThreshold;
            float inpColor[3];
            for (int y = 0; y < m_binInput->inputHeight; y++)
            {
                for (int x = 0; x < m_binInput->inputWidth; x++)
                {
                    uint32_t redIndex = (y * m_binInput->inputWidth + x) * 4;
                    inpColor[0] = m_binInput->inputBuffer[redIndex + 0];
                    inpColor[1] = m_binInput->inputBuffer[redIndex + 1];
                    inpColor[2] = m_binInput->inputBuffer[redIndex + 2];

                    float v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                    if (v > threshold)
                    {
                        float mul = fftScale * softThreshold(v, threshold, m_binInput->cp_convKnee);

                        int transX = x + m_inputLeftPadding;
                        int transY = y + m_inputTopPadding;

                        m_inputPadded[0](transY, transX) = { inpColor[0] * mul, 0 };
                        m_inputPadded[1](transY, transX) = { inpColor[1] * mul, 0 };
                        m_inputPadded[2](transY, transX) = { inpColor[2] * mul, 0 };
                    }
                }
            }
        }

        // Kernel padding
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_kernelPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_kernelPadded[i].getVector()) v = { 0, 0 };
            }

            for (int y = 0; y < m_binInput->kernelHeight; y++)
            {
                for (int x = 0; x < m_binInput->kernelWidth; x++)
                {
                    uint32_t redIndex = (y * m_binInput->kernelWidth + x) * 4;

                    int transX = x + m_kernelLeftPadding;
                    int transY = y + m_kernelTopPadding;

                    m_kernelPadded[0](transY, transX) = { fftScale * m_binInput->kernelBuffer[redIndex + 0], 0 };
                    m_kernelPadded[1](transY, transX) = { fftScale * m_binInput->kernelBuffer[redIndex + 1], 0 };
                    m_kernelPadded[2](transY, transX) = { fftScale * m_binInput->kernelBuffer[redIndex + 2], 0 };
                }
            }
        }
    }

    void ConvFftGpu::inputFFT(uint32_t ch)
    {
        m_inputFT[ch].resize(m_paddedHeight, m_paddedWidth);
        m_inputFT[ch].getVector() = dj::fft2d_gpu_glready(m_inputPadded[ch].getVector(), dj::fft_dir::DIR_FWD);
        checkGlStatus(__FUNCTION__, "");

        m_inputPadded[ch].reset();
    }

    void ConvFftGpu::kernelFFT(uint32_t ch)
    {
        m_kernelFT[ch].resize(m_paddedHeight, m_paddedWidth);
        m_kernelFT[ch].getVector() = dj::fft2d_gpu_glready(m_kernelPadded[ch].getVector(), dj::fft_dir::DIR_FWD);
        checkGlStatus(__FUNCTION__, "");

        m_kernelPadded[ch].reset();
    }

    void ConvFftGpu::multiply(uint32_t ch)
    {
        m_mulFT[ch].resize(m_paddedHeight, m_paddedWidth);

        for (size_t y = 0; y < m_paddedHeight; y++)
            for (size_t x = 0; x < m_paddedWidth; x++)
                m_mulFT[ch](y, x) = m_inputFT[ch](y, x) * m_kernelFT[ch](y, x);

        m_inputFT[ch].reset();
        m_kernelFT[ch].reset();
    }

    void ConvFftGpu::inverse(uint32_t ch)
    {
        m_iFFT[ch].resize(m_paddedHeight, m_paddedWidth);
        m_iFFT[ch].getVector() = dj::fft2d_gpu_glready(m_mulFT[ch].getVector(), dj::fft_dir::DIR_BWD);
        checkGlStatus(__FUNCTION__, "");

        m_mulFT[ch].reset();
    }

    void ConvFftGpu::output()
    {
        // Prepare output buffer
        uint32_t outputSize = m_binInput->inputWidth * m_binInput->inputHeight * 4;
        m_outputBuffer.resize(outputSize);

        // Crop the iFFT output and apply convolution multiplier
        int transX1 = 0, transY1 = 0;
        int transX2 = 0, transY2 = 0;
        uint32_t redIndex;
        for (uint32_t y = 0; y < m_binInput->inputHeight; y++)
        {
            for (uint32_t x = 0; x < m_binInput->inputWidth; x++)
            {
                transX1 = x + m_inputLeftPadding;
                transY1 = y + m_inputTopPadding;

                transX2 = transX1;
                transY2 = transY1;

                // Fix the coordinates
                {
                    if (transX1 < (m_paddedWidth / 2))
                        transX2 = transX1 + (m_paddedWidth / 2);
                    else
                        transX2 = transX1 - (m_paddedWidth / 2);

                    if (transY1 < (m_paddedHeight / 2))
                        transY2 = transY1 + (m_paddedHeight / 2);
                    else
                        transY2 = transY1 - (m_paddedHeight / 2);
                }

                // Get the output
                redIndex = (y * m_binInput->inputWidth + x) * 4;
                m_outputBuffer[redIndex + 0] = fmaxf(getMagnitude(m_iFFT[0](transY2, transX2)) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 1] = fmaxf(getMagnitude(m_iFFT[1](transY2, transX2)) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 2] = fmaxf(getMagnitude(m_iFFT[2](transY2, transX2)) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 3] = 1.0f;
            }
        }

        for (uint32_t i = 0; i < 3; i++)
            m_iFFT[i].reset();
    }

    const std::vector<float>& ConvFftGpu::getBuffer() const
    {
        return m_outputBuffer;
    }

}
