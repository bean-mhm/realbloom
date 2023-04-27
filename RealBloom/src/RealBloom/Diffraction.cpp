#include "Diffraction.h"

#include <omp.h>

namespace RealBloom
{

    static constexpr double CONTRAST_CONSTANT = 0.0002187;

    Diffraction::Diffraction()
    {}

    DiffractionParams* Diffraction::getParams()
    {
        return &m_params;
    }

    CmImage* Diffraction::getImgInputSrc()
    {
        return &m_imgInputSrc;
    }

    void Diffraction::setImgInput(CmImage* image)
    {
        m_imgInput = image;
    }

    void Diffraction::setImgDiff(CmImage* image)
    {
        m_imgDiff = image;
    }

    void Diffraction::previewInput(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        processInputImage(previewMode, m_params.inputTransformParams, m_imgInputSrc, *m_imgInput, outBuffer, outWidth, outHeight);
    }

    void Diffraction::compute()
    {
        m_status.reset();

        try
        {
            // Input buffer
            std::vector<float> inputBuffer;
            uint32_t inputWidth = 0, inputHeight = 0;
            previewInput(false, &inputBuffer, &inputWidth, &inputHeight);
            uint32_t inputBufferSize = inputWidth * inputHeight * 4;

            // Validate the dimensions
            if ((inputWidth < 4) || (inputHeight < 4))
                throw std::exception("Input dimensions are too small.");

            // Output dimensions
            uint32_t fftWidth = (inputWidth % 2 == 0) ? (inputWidth + 1) : (inputWidth);
            uint32_t fftHeight = (inputHeight % 2 == 0) ? (inputHeight + 1) : (inputHeight);

            const bool grayscale = (m_params.inputTransformParams.color.grayscaleType != GrayscaleType::None);
            const int numChannels = grayscale ? 1 : 3;

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
            if (grayscale)
            {
#pragma omp parallel for
                for (int y = 0; y < inputHeight; y++)
                {
                    for (int x = 0; x < inputWidth; x++)
                    {
                        uint32_t redIndex = (y * inputWidth + x) * 4;
                        fftInput[0](y, x) = inputBuffer[redIndex];
                    }
                }
            }
            else
            {
#pragma omp parallel for
                for (int y = 0; y < inputHeight; y++)
                {
                    for (int x = 0; x < inputWidth; x++)
                    {
                        uint32_t redIndex = (y * inputWidth + x) * 4;
                        fftInput[0](y, x) = inputBuffer[redIndex + 0];
                        fftInput[1](y, x) = inputBuffer[redIndex + 1];
                        fftInput[2](y, x) = inputBuffer[redIndex + 2];
                    }
                }
            }

            // FFT
            {
                pocketfft::shape_t shape{ fftWidth, fftHeight };
                pocketfft::stride_t strideIn{ sizeof(double), (ptrdiff_t)(fftWidth * sizeof(double)) };
                pocketfft::stride_t strideOut{ sizeof(std::complex<double>), (ptrdiff_t)(fftWidth * sizeof(std::complex<double>)) };

                for (uint32_t i = 0; i < 3; i++)
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

            Array2D<double> fftMag[3];

            double maxMags[3]{ EPSILON, EPSILON, EPSILON };
            std::mutex maxMagsMutex;

            for (uint32_t i = 0; i < 3; i++)
            {
                if ((!grayscale) || (grayscale && (i == 0)))
                {
                    fftMag[i].resize(fftHeight, fftWidth);
                }
            }

            const int centerX = (int)fftWidth / 2;
            const int centerY = (int)fftHeight / 2;

#pragma omp parallel for
            for (int y = 0; y < fftHeight; y++)
            {
                for (int x = 0; x < fftWidth; x++)
                {
                    // Transform the sample coordinates

                    int transX = x;
                    int transY = y;

                    if (transY > centerY)
                    {
                        transX = fftWidth - 1 - transX;
                        transY = fftHeight - 1 - transY;
                    }

                    if (transY <= centerY)
                    {
                        transY = centerY - transY;
                    }

                    if (transX > centerX)
                    {
                        transX = centerX * 3 - transX + 1;
                    }

                    if (transX <= centerX)
                    {
                        transX = centerX - transX;
                    }

                    transX = std::clamp(transX, 0, (int)fftWidth - 1);
                    transY = std::clamp(transY, 0, (int)fftHeight - 1);

                    // Save the results while finding the maximum magnitudes
                    for (uint32_t i = 0; i < numChannels; i++)
                    {
                        double currentMag = getMagnitude(fftOutput[i](transY, transX));
                        fftMag[i](y, x) = currentMag;

                        std::scoped_lock lock(maxMagsMutex);
                        if (currentMag > maxMags[i])
                            maxMags[i] = currentMag;
                    }
                }
            }

            double logOfMaxMag;
            if (grayscale)
            {
                double logsOfMaxMag[3];
                for (uint32_t i = 0; i < 3; i++)
                {
                    logsOfMaxMag[i] = log(CONTRAST_CONSTANT * maxMags[i] + 1.0);
                }

                logOfMaxMag = fmax(fmax(logsOfMaxMag[0], logsOfMaxMag[1]), logsOfMaxMag[2]);
            }
            else
            {
                logOfMaxMag = log(CONTRAST_CONSTANT * maxMags[0] + 1.0);
            }

            // Update the output image
            {
                std::scoped_lock lock(*m_imgDiff);
                m_imgDiff->resize(fftWidth, fftHeight, false);
                float* imageBuffer = m_imgDiff->getImageData();

#pragma omp parallel for
                for (int y = 0; y < fftHeight; y++)
                {
                    for (int x = 0; x < fftWidth; x++)
                    {
                        uint32_t redIndex = (y * fftWidth + x) * 4;
                        if (grayscale)
                        {
                            float v = log(CONTRAST_CONSTANT * fftMag[0](y, x) + 1.0) / logOfMaxMag;
                            imageBuffer[redIndex + 0] = v;
                            imageBuffer[redIndex + 1] = v;
                            imageBuffer[redIndex + 2] = v;
                        }
                        else
                        {
                            imageBuffer[redIndex + 0] = log(CONTRAST_CONSTANT * fftMag[0](y, x) + 1.0) / logOfMaxMag;
                            imageBuffer[redIndex + 1] = log(CONTRAST_CONSTANT * fftMag[1](y, x) + 1.0) / logOfMaxMag;
                            imageBuffer[redIndex + 2] = log(CONTRAST_CONSTANT * fftMag[2](y, x) + 1.0) / logOfMaxMag;
                        }
                        imageBuffer[redIndex + 3] = 1.0f;
                    }
                }
            }
            m_imgDiff->moveToGPU();
        }
        catch (const std::exception& e)
        {
            m_status.setError(e.what());
        }
    }

    const BaseStatus& Diffraction::getStatus() const
    {
        return m_status;
    }

}
