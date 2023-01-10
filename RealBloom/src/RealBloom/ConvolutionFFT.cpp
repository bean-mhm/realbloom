#include "ConvolutionFFT.h"

namespace RealBloom
{

    ConvolutionFFT::ConvolutionFFT(ConvolutionParams& convParams, float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight, float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
        : m_params(convParams),
        m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_kernelBuffer(kernelBuffer), m_kernelWidth(kernelWidth), m_kernelHeight(kernelHeight)
    {}

    ConvolutionFFT::~ConvolutionFFT()
    {}

    void ConvolutionFFT::pad()
    {
        // Padded size

        calcFftConvPadding(
            false, false,
            m_inputWidth, m_inputHeight,
            m_kernelWidth, m_kernelHeight,
            m_params.kernelCenterX, m_params.kernelCenterY,
            m_paddedWidth, m_paddedHeight
        );

        // Padding amount

        m_inputLeftPadding = floorf((float)(m_paddedWidth - m_inputWidth) / 2.0f);
        m_inputTopPadding = floorf((float)(m_paddedHeight - m_inputHeight) / 2.0f);

        m_kernelLeftPadding = floorf(((float)m_paddedWidth / 2.0f) - ((float)m_kernelWidth * m_params.kernelCenterX));
        m_kernelTopPadding = floorf(((float)m_paddedHeight / 2.0f) - ((float)m_kernelHeight * m_params.kernelCenterY));

        if (m_kernelWidth % 2 == 1) m_kernelLeftPadding += 1;
        if (m_kernelHeight % 2 == 1) m_kernelTopPadding += 1;

        // Print the dimensions
        if (0)
        {
            printInfo(__FUNCTION__, "", strFormat(
                "FFT Convolution\n"
                "Input:          %u x %u\n"
                "Kernel:         %u x %u\n"
                "Padded:         %u x %u\n",
                m_inputWidth, m_inputHeight,
                m_kernelWidth, m_kernelHeight,
                m_paddedWidth, m_paddedHeight
            ));
        }

        // Input padding + threshold
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_inputPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_inputPadded[i].getVector()) v = 0;
            }

            float threshold = m_params.threshold;
            float transKnee = transformKnee(m_params.knee);

            float inpColor[3];
            for (int y = 0; y < m_inputHeight; y++)
            {
                for (int x = 0; x < m_inputWidth; x++)
                {
                    uint32_t redIndex = (y * m_inputWidth + x) * 4;
                    inpColor[0] = m_inputBuffer[redIndex + 0];
                    inpColor[1] = m_inputBuffer[redIndex + 1];
                    inpColor[2] = m_inputBuffer[redIndex + 2];

                    float v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                    if (v > threshold)
                    {
                        // Smooth Transition
                        float mul = softThreshold(v, threshold, transKnee);

                        int transX = x + m_inputLeftPadding;
                        int transY = y + m_inputTopPadding;

                        m_inputPadded[0](transY, transX) = inpColor[0] * mul;
                        m_inputPadded[1](transY, transX) = inpColor[1] * mul;
                        m_inputPadded[2](transY, transX) = inpColor[2] * mul;
                    }
                }
            }
        }

        // Kernel padding
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_kernelPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_kernelPadded[i].getVector()) v = 0;
            }

            int transX, transY;
            uint32_t redIndex;
            for (int y = 0; y < m_kernelHeight; y++)
            {
                for (int x = 0; x < m_kernelWidth; x++)
                {
                    redIndex = (y * m_kernelWidth + x) * 4;
                    transX = x + m_kernelLeftPadding;
                    transY = y + m_kernelTopPadding;
                    m_kernelPadded[0](transY, transX) = m_kernelBuffer[redIndex + 0];
                    m_kernelPadded[1](transY, transX) = m_kernelBuffer[redIndex + 1];
                    m_kernelPadded[2](transY, transX) = m_kernelBuffer[redIndex + 2];
                }
            }
        }
    }

    void ConvolutionFFT::inputFFT(uint32_t ch)
    {
        m_inputFT[ch].resize(m_paddedHeight, m_paddedWidth);

        pocketfft::shape_t shape{ m_paddedWidth, m_paddedHeight };
        pocketfft::stride_t strideIn{ sizeof(float), (ptrdiff_t)(m_paddedWidth * sizeof(float)) };
        pocketfft::stride_t strideOut{ sizeof(std::complex<float>), (ptrdiff_t)(m_paddedWidth * sizeof(std::complex<float>)) };
        float fftScale = 1.0f / sqrtf((float)m_paddedWidth * (float)m_paddedHeight);
        pocketfft::r2c(
            shape,
            strideIn,
            strideOut,
            { 0, 1 },
            pocketfft::FORWARD,
            m_inputPadded[ch].getVector().data(),
            m_inputFT[ch].getVector().data(),
            fftScale,
            0);

        m_inputPadded[ch].reset();
    }

    void ConvolutionFFT::kernelFFT(uint32_t ch)
    {
        m_kernelFT[ch].resize(m_paddedHeight, m_paddedWidth);

        pocketfft::shape_t shape{ m_paddedWidth, m_paddedHeight };
        pocketfft::stride_t strideIn{ sizeof(float), (ptrdiff_t)(m_paddedWidth * sizeof(float)) };
        pocketfft::stride_t strideOut{ sizeof(std::complex<float>), (ptrdiff_t)(m_paddedWidth * sizeof(std::complex<float>)) };
        float fftScale = 1.0f / sqrtf((float)m_paddedWidth * (float)m_paddedHeight);
        pocketfft::r2c(
            shape,
            strideIn,
            strideOut,
            { 0, 1 },
            pocketfft::FORWARD,
            m_kernelPadded[ch].getVector().data(),
            m_kernelFT[ch].getVector().data(),
            fftScale,
            0);

        m_kernelPadded[ch].reset();
    }

    void ConvolutionFFT::multiply(uint32_t ch)
    {
        m_mulFT[ch].resize(m_paddedHeight, m_paddedWidth);

        for (size_t y = 0; y < m_paddedHeight; y++)
            for (size_t x = 0; x < m_paddedWidth; x++)
                m_mulFT[ch](y, x) = m_inputFT[ch](y, x) * m_kernelFT[ch](y, x);

        m_inputFT[ch].reset();
        m_kernelFT[ch].reset();
    }

    void ConvolutionFFT::inverse(uint32_t ch)
    {
        m_iFFT[ch].resize(m_paddedHeight, m_paddedWidth);

        pocketfft::shape_t shape{ m_paddedWidth, m_paddedHeight };
        pocketfft::stride_t strideIn{ sizeof(std::complex<float>), (ptrdiff_t)(m_paddedWidth * sizeof(std::complex<float>)) };
        pocketfft::stride_t strideOut{ sizeof(float), (ptrdiff_t)(m_paddedWidth * sizeof(float)) };
        pocketfft::c2r(
            shape,
            strideIn,
            strideOut,
            { 0, 1 },
            pocketfft::BACKWARD,
            m_mulFT[ch].getVector().data(),
            m_iFFT[ch].getVector().data(),
            1.0f,
            0);

        m_mulFT[ch].reset();
    }

    void ConvolutionFFT::output()
    {
        // Prepare output buffer
        uint32_t outputSize = m_inputWidth * m_inputHeight * 4;
        m_outputBuffer.resize(outputSize);

        // Crop the iFFT output and apply convolution multiplier
        int transX1 = 0, transY1 = 0;
        int transX2 = 0, transY2 = 0;
        uint32_t redIndex;
        for (uint32_t y = 0; y < m_inputHeight; y++)
        {
            for (uint32_t x = 0; x < m_inputWidth; x++)
            {
                transX1 = x + m_inputLeftPadding;
                transY1 = y + m_inputTopPadding;

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
                redIndex = (y * m_inputWidth + x) * 4;
                m_outputBuffer[redIndex + 0] = fmaxf(m_iFFT[0](transY2, transX2) * CONV_MULTIPLIER, 0.0f);
                m_outputBuffer[redIndex + 1] = fmaxf(m_iFFT[1](transY2, transX2) * CONV_MULTIPLIER, 0.0f);
                m_outputBuffer[redIndex + 2] = fmaxf(m_iFFT[2](transY2, transX2) * CONV_MULTIPLIER, 0.0f);
                m_outputBuffer[redIndex + 3] = 1.0f;
            }
        }

        for (uint32_t i = 0; i < 3; i++)
            m_iFFT[i].reset();
    }

    const std::vector<float>& ConvolutionFFT::getBuffer() const
    {
        return m_outputBuffer;
    }

}
