#include "DiffractionPattern.h"

constexpr double CONTRAST_CONSTANT = 0.0002187;

namespace RealBloom
{

    DiffractionPattern::DiffractionPattern()
        : m_hasRawData(false), m_maxMag{ EPSILON, EPSILON, EPSILON }, m_success(false), m_error("")
    {
        //
    }

    RealBloom::DiffractionPatternParams* DiffractionPattern::getParams()
    {
        return &m_params;
    }

    void DiffractionPattern::compute(float* buffer)
    {
        m_hasRawData = false;
        m_success = false;
        m_error = "Unknown";

        uint32_t width = m_params.width;
        uint32_t height = m_params.height;
        bool grayscale = m_params.grayscale;

        // Validate the dimensions
        if (width < 4 || height < 4)
        {
            m_error = "The input image is too small.";
            return;
        }
        if (width != height)
        {
            m_error = "The input image must be a square.";
            return;
        }
        if (!std::has_single_bit(width) || !std::has_single_bit(height))
        {
            m_error = "The input dimensions must be a power of 2.";
            return;
        }

        // Setup the output buffer
        m_rawR.resize(width * height);
        if (!grayscale)
        {
            m_rawG.resize(width * height);
            m_rawB.resize(width * height);
        }

        // Allocate arrays for FFTW
        fftw_complex* inR = nullptr;
        fftw_complex* inG = nullptr;
        fftw_complex* inB = nullptr;
        fftw_complex* outR = nullptr;
        fftw_complex* outG = nullptr;
        fftw_complex* outB = nullptr;
        inR = new fftw_complex[width * height];
        outR = new fftw_complex[width * height];
        if (!grayscale)
        {
            inG = new fftw_complex[width * height];
            outG = new fftw_complex[width * height];
            inB = new fftw_complex[width * height];
            outB = new fftw_complex[width * height];
        }

        //Fill in the arrays with the pixel colors
        {
            uint32_t redIndex = 0;
            if (grayscale)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    for (uint32_t x = 0; x < width; x++)
                    {
                        redIndex = (y * width + x) * 4;

                        inR[y * width + x][0] = rgbToGrayscale(buffer[redIndex + 0], buffer[redIndex + 1], buffer[redIndex + 2]);
                        inR[y * width + x][1] = 0.0;
                    }
                }
            } else
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    for (uint32_t x = 0; x < width; x++)
                    {
                        redIndex = (y * width + x) * 4;

                        inR[y * width + x][0] = buffer[redIndex];
                        inR[y * width + x][1] = 0.0;

                        inG[y * width + x][0] = buffer[redIndex + 1];
                        inG[y * width + x][1] = 0.0;

                        inB[y * width + x][0] = buffer[redIndex + 2];
                        inB[y * width + x][1] = 0.0;
                    }
                }
            }
        }

        // Plan and execute Forward FFT
        fftw_plan planR = nullptr, planG = nullptr, planB = nullptr;

        planR = fftw_plan_dft_2d(width, height, inR, outR, FFTW_FORWARD, FFTW_MEASURE);
        fftw_execute(planR);

        if (!grayscale)
        {
            planG = fftw_plan_dft_2d(width, height, inG, outG, FFTW_FORWARD, FFTW_MEASURE);
            fftw_execute(planG);
            planB = fftw_plan_dft_2d(width, height, inB, outB, FFTW_FORWARD, FFTW_MEASURE);
            fftw_execute(planB);
        }

        // Set up variables for saving the raw output
        uint32_t indexOrig, indexTrans = 0;
        int transX = 0, transY = 0;

        m_maxMag[0] = EPSILON;
        m_maxMag[1] = EPSILON;
        m_maxMag[2] = EPSILON;
        double currentMag = 0;

        // Save the raw output into m_rawFFT
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                // Fix the coordinates
                {
                    if (x < (width / 2))
                        transX = x + (width / 2);
                    else
                        transX = x - (width / 2);

                    if (y < (height / 2))
                        transY = y + (height / 2);
                    else
                        transY = y - (height / 2);
                }

                // Calculate the indices
                indexOrig = y * width + x;
                indexTrans = transY * width + transX;

                // Save the results while finding the maximum magnitudes
                {
                    currentMag = getMagnitude(outR[indexOrig][0], outR[indexOrig][1]);
                    if (currentMag > m_maxMag[0])
                        m_maxMag[0] = currentMag;
                    m_rawR[indexTrans] = currentMag;

                    if (!grayscale)
                    {
                        currentMag = getMagnitude(outG[indexOrig][0], outG[indexOrig][1]);
                        if (currentMag > m_maxMag[1])
                            m_maxMag[1] = currentMag;
                        m_rawG[indexTrans] = currentMag;

                        currentMag = getMagnitude(outB[indexOrig][0], outB[indexOrig][1]);
                        if (currentMag > m_maxMag[2])
                            m_maxMag[2] = currentMag;
                        m_rawB[indexTrans] = currentMag;
                    }
                }
            }
        }

        // Free memory
        delete[] inR;
        delete[] outR;
        if (!grayscale)
        {
            delete[] inG;
            delete[] inB;
            delete[] outG;
            delete[] outB;
        }
        if (planR) fftw_destroy_plan(planR);
        if (planG) fftw_destroy_plan(planG);
        if (planB) fftw_destroy_plan(planB);

        m_hasRawData = true;
        m_success = true;
        m_error = "Success";
    }

    bool DiffractionPattern::getRgbaOutput(std::vector<float>& outBuffer)
    {
        if (!m_hasRawData)
            return false;

        uint32_t width = m_params.width;
        uint32_t height = m_params.height;
        double contrast = m_params.contrast;
        double multiplier = applyExposure(m_params.exposure);
        bool grayscale = m_params.grayscale;

        double logOfMaxMag;
        if (grayscale)
        {
            double logsOfMaxMag[3];
            logsOfMaxMag[0] = log(CONTRAST_CONSTANT * m_maxMag[0] + 1.0);
            logsOfMaxMag[1] = log(CONTRAST_CONSTANT * m_maxMag[1] + 1.0);
            logsOfMaxMag[2] = log(CONTRAST_CONSTANT * m_maxMag[2] + 1.0);
            logOfMaxMag = fmax(fmax(logsOfMaxMag[0], logsOfMaxMag[1]), logsOfMaxMag[2]);
        } else
        {
            logOfMaxMag = log(CONTRAST_CONSTANT * m_maxMag[0] + 1.0);
        }

        outBuffer.resize(width * height * 4);
        uint32_t pixelIndex = 0;
        uint32_t redIndex = 0;
        double v = 0.0, v2 = 0.0;
        double vrgb[3], rgbMultiplier;
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                pixelIndex = y * width + x;
                redIndex = pixelIndex * 4;
                if (grayscale)
                {
                    v = (log(CONTRAST_CONSTANT * m_rawR[pixelIndex] + 1.0) / logOfMaxMag);
                    v2 = contrastCurve(v, contrast) * multiplier;

                    outBuffer[redIndex + 0] = v2;
                    outBuffer[redIndex + 1] = v2;
                    outBuffer[redIndex + 2] = v2;
                } else
                {
                    vrgb[0] = (log(CONTRAST_CONSTANT * m_rawR[pixelIndex] + 1.0) / logOfMaxMag);
                    vrgb[1] = (log(CONTRAST_CONSTANT * m_rawG[pixelIndex] + 1.0) / logOfMaxMag);
                    vrgb[2] = (log(CONTRAST_CONSTANT * m_rawB[pixelIndex] + 1.0) / logOfMaxMag);

                    v = rgbToGrayscale(vrgb[0], vrgb[1], vrgb[2]);
                    rgbMultiplier = (contrastCurve(v, contrast) / fmaxf(v, EPSILON)) * multiplier;

                    v2 = vrgb[0] * rgbMultiplier;
                    outBuffer[redIndex + 0] = v2;

                    v2 = vrgb[1] * rgbMultiplier;
                    outBuffer[redIndex + 1] = v2;

                    v2 = vrgb[2] * rgbMultiplier;
                    outBuffer[redIndex + 2] = v2;
                }
                outBuffer[redIndex + 3] = 1.0f;
            }
        }

        return true;
    }

    bool DiffractionPattern::hasRawData() const
    {
        return m_hasRawData;
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
