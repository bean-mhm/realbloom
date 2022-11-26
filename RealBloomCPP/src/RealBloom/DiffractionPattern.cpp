#include "DiffractionPattern.h"

namespace RealBloom
{

    constexpr double CONTRAST_CONSTANT = 0.0002187;

    DiffractionPattern::DiffractionPattern()
    {}

    RealBloom::DiffractionPatternParams* DiffractionPattern::getParams()
    {
        return &m_params;
    }

    void DiffractionPattern::compute(float* buffer)
    {
        for (size_t i = 0; i < 3; i++)
            m_fftMag[i].reset();

        m_hasFftData = false;
        m_success = false;
        m_error = "";

        try
        {
            uint32_t inputWidth = m_params.inputWidth;
            uint32_t inputHeight = m_params.inputHeight;
            bool grayscale = m_params.grayscale;

            uint32_t fftWidth = (inputWidth % 2 == 0) ? (inputWidth) : (inputWidth + 1);
            uint32_t fftHeight = (inputHeight % 2 == 0) ? (inputHeight) : (inputHeight + 1);

            // Validate the dimensions
            if ((inputWidth < 4) || (inputHeight < 4))
                throw std::exception("Input dimensions are too small.");

            // FFT buffers (RGB)
            Array2D<double> fftInput[3];
            Array2D<std::complex<double>> fftOutput[3];

            // Resize the buffers
            for (size_t i = 0; i < 3; i++)
            {
                if ((!grayscale) || (grayscale && (i == 0)))
                {
                    fftInput[i].resize(fftHeight, fftWidth);
                    fftInput[i].fill(0);
                    fftOutput[i].resize(fftHeight, fftWidth);
                }
            }

            // Fill in the input buffer
            {
                uint32_t redIndex = 0;
                if (grayscale)
                {
                    for (uint32_t y = 0; y < inputHeight; y++)
                    {
                        for (uint32_t x = 0; x < inputWidth; x++)
                        {
                            redIndex = (y * inputWidth + x) * 4;
                            fftInput[0](y, x) = rgbToGrayscale(buffer[redIndex + 0], buffer[redIndex + 1], buffer[redIndex + 2]);
                        }
                    }
                }
                else
                {
                    for (uint32_t y = 0; y < inputHeight; y++)
                    {
                        for (uint32_t x = 0; x < inputWidth; x++)
                        {
                            redIndex = (y * inputWidth + x) * 4;
                            fftInput[0](y, x) = buffer[redIndex + 0];
                            fftInput[1](y, x) = buffer[redIndex + 1];
                            fftInput[2](y, x) = buffer[redIndex + 2];
                        }
                    }
                }
            }

            // FFT
            {
                pocketfft::shape_t shape{ fftWidth, fftHeight };
                pocketfft::stride_t strideIn{ sizeof(double), (ptrdiff_t)(fftWidth * sizeof(double)) };
                pocketfft::stride_t strideOut{ sizeof(std::complex<double>), (ptrdiff_t)(fftWidth * sizeof(std::complex<double>)) };

                for (size_t i = 0; i < 3; i++)
                {
                    if ((!grayscale) || (grayscale && (i == 0)))
                    {
                        pocketfft::r2c(
                            shape,
                            strideIn,
                            strideOut,
                            { 0, 1 },
                            pocketfft::FORWARD,
                            fftInput[i].getVector().data(),
                            fftOutput[i].getVector().data(),
                            1.0,
                            0);
                        fftInput[i].reset();
                    }
                }
            }

            // Store magnitude data

            m_maxMag[0] = EPSILON;
            m_maxMag[1] = EPSILON;
            m_maxMag[2] = EPSILON;
            double currentMag = 0;

            for (size_t i = 0; i < 3; i++)
                if ((!grayscale) || (grayscale && (i == 0)))
                {
                    m_fftMag[i].resize(fftHeight, fftWidth);
                }

            int shiftX = (int)fftWidth / 2;
            int shiftY = (int)fftHeight / 2;

            int transX, transY;
            for (uint32_t y = 0; y < fftHeight; y++)
            {
                for (uint32_t x = 0; x < fftWidth; x++)
                {
                    // Shift and mirror
                    transX = shiftIndex((y < shiftY) ? (fftWidth - x) : x, shiftX, fftWidth);
                    transY = shiftIndex((y <= shiftY) ? (fftHeight - y) : y, shiftY, fftHeight);

                    // Save the results while finding the maximum magnitudes
                    {
                        currentMag = getMagnitude(fftOutput[0](transY, transX));
                        m_fftMag[0](y, x) = currentMag;
                        if (currentMag > m_maxMag[0])
                            m_maxMag[0] = currentMag;

                        if (!grayscale)
                        {
                            currentMag = getMagnitude(fftOutput[1](transY, transX));
                            m_fftMag[1](y, x) = currentMag;
                            if (currentMag > m_maxMag[1])
                                m_maxMag[1] = currentMag;

                            currentMag = getMagnitude(fftOutput[2](transY, transX));
                            m_fftMag[2](y, x) = currentMag;
                            if (currentMag > m_maxMag[2])
                                m_maxMag[2] = currentMag;
                        }
                    }
                }
            }

            m_hasFftData = true;
            m_success = true;
        } catch (const std::exception& e)
        {
            m_hasFftData = false;
            m_success = false;
            m_error = e.what();
        }
    }

    uint32_t DiffractionPattern::getOutputWidth() const
    {
        return m_fftMag[0].getNumCols();
    }

    uint32_t DiffractionPattern::getOutputHeight() const
    {
        return m_fftMag[0].getNumRows();
    }

    bool DiffractionPattern::getRgbaOutput(std::vector<float>& outBuffer) const
    {
        if (!m_hasFftData)
            return false;

        uint32_t width = getOutputWidth();
        uint32_t height = getOutputHeight();

        double contrast = m_params.contrast;
        double exposureMul = applyExposure(m_params.exposure);
        bool grayscale = m_params.grayscale;

        double logOfMaxMag;
        if (grayscale)
        {
            double logsOfMaxMag[3];
            logsOfMaxMag[0] = log(CONTRAST_CONSTANT * m_maxMag[0] + 1.0);
            logsOfMaxMag[1] = log(CONTRAST_CONSTANT * m_maxMag[1] + 1.0);
            logsOfMaxMag[2] = log(CONTRAST_CONSTANT * m_maxMag[2] + 1.0);
            logOfMaxMag = fmax(fmax(logsOfMaxMag[0], logsOfMaxMag[1]), logsOfMaxMag[2]);
        }
        else
        {
            logOfMaxMag = log(CONTRAST_CONSTANT * m_maxMag[0] + 1.0);
        }

        outBuffer.resize(width * height * 4);
        uint32_t redIndex = 0;
        double v = 0.0, v2 = 0.0;
        double vrgb[3], contrastMul;
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                redIndex = (y * width + x) * 4;
                if (grayscale)
                {
                    v = (log(CONTRAST_CONSTANT * m_fftMag[0](y, x) + 1.0) / logOfMaxMag);
                    v2 = contrastCurve(v, contrast) * exposureMul;

                    outBuffer[redIndex + 0] = v2;
                    outBuffer[redIndex + 1] = v2;
                    outBuffer[redIndex + 2] = v2;
                }
                else
                {
                    vrgb[0] = (log(CONTRAST_CONSTANT * m_fftMag[0](y, x) + 1.0) / logOfMaxMag);
                    vrgb[1] = (log(CONTRAST_CONSTANT * m_fftMag[1](y, x) + 1.0) / logOfMaxMag);
                    vrgb[2] = (log(CONTRAST_CONSTANT * m_fftMag[2](y, x) + 1.0) / logOfMaxMag);

                    v = rgbToGrayscale(vrgb[0], vrgb[1], vrgb[2]);
                    contrastMul = (contrastCurve(v, contrast) / fmaxf(v, EPSILON)) * exposureMul;

                    v2 = vrgb[0] * contrastMul;
                    outBuffer[redIndex + 0] = v2;

                    v2 = vrgb[1] * contrastMul;
                    outBuffer[redIndex + 1] = v2;

                    v2 = vrgb[2] * contrastMul;
                    outBuffer[redIndex + 2] = v2;
                }
                outBuffer[redIndex + 3] = 1.0f;
            }
        }

        return true;
    }

    bool DiffractionPattern::hasFftData() const
    {
        return m_hasFftData;
    }

    bool DiffractionPattern::success()
    {
        return m_success;
    }

    std::string DiffractionPattern::getError()
    {
        return m_error;
    }

}
