#include "Dispersion.h"

namespace RealBloom
{

    constexpr uint32_t DISP_PROG_TIMESTEP = 33;
    constexpr uint32_t DISP_PROG_INTERVAL = 4;

    Dispersion::Dispersion()
    {
        //
    }

    DispersionParams* Dispersion::getParams()
    {
        return &m_params;
    }

    void Dispersion::setDiffPatternImage(Image32Bit* image)
    {
        m_imageDP = image;
    }

    void Dispersion::setDispersionImage(Image32Bit* image)
    {
        m_imageDisp = image;
    }

    void Dispersion::compute()
    {
        cancel();

        m_state.numSteps = m_params.steps;
        m_state.numStepsDone = 0;
        m_state.working = true;
        m_state.mustCancel = false;

        // Start the thread
        m_thread = new std::thread([this]()
            {
                // Diff pattern Buffer
                std::vector<float> dpBuffer;
                uint32_t dpWidth = 0, dpHeight = 0;
                uint32_t dpBufferSize = 0;
                {
                    // Buffer from diff pattern image
                    std::lock_guard<Image32Bit> dpImageLock(*m_imageDP);
                    m_imageDP->getDimensions(dpWidth, dpHeight);
                    float* dpImageData = m_imageDP->getImageData();

                    // Copy the buffer so the image doesn't stay locked throughout the process
                    dpBufferSize = m_imageDP->getImageDataSize();
                    dpBuffer.resize(dpBufferSize);
                    std::copy(dpImageData, dpImageData + dpBufferSize, dpBuffer.data());
                }

                uint32_t dispSteps = m_params.steps;
                if (dispSteps > DISP_MAX_STEPS) dispSteps = DISP_MAX_STEPS;
                if (dispSteps < 1) dispSteps = 1;

                float dispAmount = fminf(fmaxf(m_params.amount, 0.0f), 1.0f);
                float dispColor[3];
                std::copy(m_params.color, m_params.color + 3, dispColor);

                double wavelength = 0;
                double wlR = 0, wlG = 0, wlB = 0;
                double wavelengthPos = 0; // how far along the wavelength range
                float scale = 0;

                // Scaled buffer for individual steps
                uint32_t scaledBufferSize = dpWidth * dpHeight * 3;
                std::vector<float> scaledBuffer;
                scaledBuffer.resize(scaledBufferSize);

                float centerX, centerY;
                centerX = (float)dpWidth / 2.0f;
                centerY = (float)dpHeight / 2.0f;

                // To match the brightness later
                float inputBrightest = EPSILON, outputBrightest = EPSILON;
                {
                    float inputCurrentValue = 0;
                    uint32_t redIndex;
                    for (uint32_t y = 0; y < dpHeight; y++)
                    {
                        for (uint32_t x = 0; x < dpWidth; x++)
                        {
                            redIndex = (y * dpWidth + x) * 4;
                            inputCurrentValue = rgbToGrayscale(dpBuffer[redIndex + 0], dpBuffer[redIndex + 1], dpBuffer[redIndex + 2]);
                            if (inputCurrentValue > inputBrightest)
                                inputBrightest = inputCurrentValue;
                        }
                    }
                }

                // Create and clear the dispersion buffer
                std::vector<float> dispBuffer;
                dispBuffer.resize(dpBufferSize);
                for (uint32_t i = 0; i < dpBufferSize; i++)
                    if (i % 4 == 3)
                        dispBuffer[i] = 1.0f;
                    else
                        dispBuffer[i] = 0.0f;

                // Apply Dispersion
                auto lastProgTime = std::chrono::system_clock::now();
                for (uint32_t i = 1; i <= dispSteps; i++)
                {
                    // I don't have scientific proof, but taking it to the power of e
                    // tends to make the overall color of the final result neutral,
                    // while other values produce unbalanced colors.
                    wavelengthPos = pow((double)i / ((double)dispSteps + 1.0), 2.71828182845904523536);
                    wavelength = Wavelength::LEN_MIN + (wavelengthPos * Wavelength::LEN_RANGE);
                    Wavelength::getRGB(wavelength, wlR, wlG, wlB);
                    scale = 1.0f - (dispAmount * (1.0f - (i / (float)dispSteps)));
                    scale = fmaxf(EPSILON, scale);

                    // Clear scaledBuffer
                    for (uint32_t i = 0; i < scaledBufferSize; i++)
                        scaledBuffer[i] = 0;

                    // Scale
                    {
                        float scaledWidth = (float)dpWidth * scale;
                        float scaledHeight = (float)dpHeight * scale;
                        float scaledRectX1, scaledRectY1, scaledRectX2, scaledRectY2;
                        uint32_t iScaledRectX1, iScaledRectY1, iScaledRectX2, iScaledRectY2;

                        scaledRectX1 = centerX - (scaledWidth / 2.0f);
                        scaledRectY1 = centerY - (scaledHeight / 2.0f);
                        scaledRectX2 = centerX + (scaledWidth / 2.0f);
                        scaledRectY2 = centerY + (scaledHeight / 2.0f);

                        iScaledRectX1 = (uint32_t)floorf(scaledRectX1 - 0.5f);
                        iScaledRectY1 = (uint32_t)floorf(scaledRectY1 - 0.5f);
                        iScaledRectX2 = (uint32_t)ceilf(scaledRectX2 - 0.5f);
                        iScaledRectY2 = (uint32_t)ceilf(scaledRectY2 - 0.5f);

                        float transX = 0, transY = 0;
                        int redIndexBuffer, redIndexScaled = 0;
                        Bilinear::Result bil;
                        for (uint32_t y = iScaledRectY1; y <= iScaledRectY2; y++)
                        {
                            for (uint32_t x = iScaledRectX1; x <= iScaledRectX2; x++)
                            {
                                redIndexScaled = (y * dpWidth + x) * 3;
                                transX = (((float)x + 0.5f - centerX) / scale) + centerX;
                                transY = (((float)y + 0.5f - centerY) / scale) + centerY;
                                Bilinear::calculate(transX, transY, bil);

                                if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], dpWidth, dpHeight))
                                {
                                    redIndexBuffer = (bil.topLeftPos[1] * dpWidth + bil.topLeftPos[0]) * 4;
                                    blendAddRGB(scaledBuffer.data(), redIndexScaled, dpBuffer.data(), redIndexBuffer, bil.topLeftWeight);
                                }

                                if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], dpWidth, dpHeight))
                                {
                                    redIndexBuffer = (bil.topRightPos[1] * dpWidth + bil.topRightPos[0]) * 4;
                                    blendAddRGB(scaledBuffer.data(), redIndexScaled, dpBuffer.data(), redIndexBuffer, bil.topRightWeight);
                                }

                                if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], dpWidth, dpHeight))
                                {
                                    redIndexBuffer = (bil.bottomLeftPos[1] * dpWidth + bil.bottomLeftPos[0]) * 4;
                                    blendAddRGB(scaledBuffer.data(), redIndexScaled, dpBuffer.data(), redIndexBuffer, bil.bottomLeftWeight);
                                }

                                if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], dpWidth, dpHeight))
                                {
                                    redIndexBuffer = (bil.bottomRightPos[1] * dpWidth + bil.bottomRightPos[0]) * 4;
                                    blendAddRGB(scaledBuffer.data(), redIndexScaled, dpBuffer.data(), redIndexBuffer, bil.bottomRightWeight);
                                }
                            }
                        }
                    }

                    // Colorize and add to dispBuffer
                    {
                        float f_wlR = (float)wlR, f_wlG = (float)wlG, f_wlB = (float)wlB;
                        uint32_t redIndexBuffer, redIndexScaled;
                        for (uint32_t y = 0; y < dpHeight; y++)
                        {
                            for (uint32_t x = 0; x < dpWidth; x++)
                            {
                                redIndexScaled = (y * dpWidth + x) * 3;
                                redIndexBuffer = (y * dpWidth + x) * 4;

                                dispBuffer[redIndexBuffer + 0] += scaledBuffer[redIndexScaled + 0] * f_wlR * dispColor[0];
                                dispBuffer[redIndexBuffer + 1] += scaledBuffer[redIndexScaled + 1] * f_wlG * dispColor[1];
                                dispBuffer[redIndexBuffer + 2] += scaledBuffer[redIndexScaled + 2] * f_wlB * dispColor[2];
                            }
                        }
                    }

                    // Update the progress
                    m_state.numStepsDone = i + 1;
                    if (!m_state.mustCancel
                        && (i % DISP_PROG_INTERVAL == 0)                      // update every x steps
                        && (getElapsedMs(lastProgTime) > DISP_PROG_TIMESTEP)  // update every x ms
                        && (m_state.numStepsDone < m_state.numSteps))         // skip the last step
                    {
                        m_imageDisp->resize(dpWidth, dpHeight);
                        {
                            std::lock_guard<Image32Bit> dispImageLock(*m_imageDisp);
                            float* imageBuffer = m_imageDisp->getImageData();
                            std::copy(dispBuffer.data(), dispBuffer.data() + dpBufferSize, imageBuffer);
                        }
                        Async::schedule([this]() { m_imageDisp->moveToGPU(); });

                        lastProgTime = std::chrono::system_clock::now();
                    }

                    if (m_state.mustCancel)
                        break;
                }

                // Calculate the brightest value in dispBuffer
                if (!m_state.mustCancel)
                {
                    float outputCurrentValue = 0;
                    uint32_t redIndex = 0;
                    for (uint32_t y = 0; y < dpHeight; y++)
                    {
                        for (uint32_t x = 0; x < dpWidth; x++)
                        {
                            redIndex = (y * dpWidth + x) * 4;

                            outputCurrentValue = rgbToGrayscale(dispBuffer[redIndex + 0], dispBuffer[redIndex + 1], dispBuffer[redIndex + 2]);
                            if (outputCurrentValue > outputBrightest)
                                outputBrightest = outputCurrentValue;
                        }
                    }
                }

                // Match the brightness
                if (!m_state.mustCancel)
                {
                    float brightnessRatio = inputBrightest / outputBrightest;
                    uint32_t redIndex = 0;
                    for (uint32_t y = 0; y < dpHeight; y++)
                    {
                        for (uint32_t x = 0; x < dpWidth; x++)
                        {
                            redIndex = (y * dpWidth + x) * 4;

                            dispBuffer[redIndex + 0] *= brightnessRatio;
                            dispBuffer[redIndex + 1] *= brightnessRatio;
                            dispBuffer[redIndex + 2] *= brightnessRatio;
                        }
                    }
                }

                // Update the dispersion image
                if (!m_state.mustCancel)
                {
                    m_imageDisp->resize(dpWidth, dpHeight);
                    {
                        std::lock_guard<Image32Bit> dispImageLock(*m_imageDisp);
                        float* imageBuffer = m_imageDisp->getImageData();
                        std::copy(dispBuffer.data(), dispBuffer.data() + dpBufferSize, imageBuffer);
                    }
                    Async::schedule([this]()
                        {
                            m_imageDisp->moveToGPU();
                            int* pSelImageIndex = (int*)Async::getShared("selImageIndex");
                            if (pSelImageIndex != nullptr) *pSelImageIndex = 2;
                        });
                }

                m_state.working = false;
            });
    }

    void Dispersion::cancel()
    {
        if (m_state.working)
        {
            m_state.mustCancel = true;

            // Wait for the thread
            threadJoin(m_thread);
            DELPTR(m_thread);
        }

        m_state.working = false;
        m_state.mustCancel = false;
    }

    bool Dispersion::isWorking()
    {
        return m_state.working;
    }

    std::string Dispersion::getStats()
    {
        if (m_state.working && !m_state.mustCancel)
            return stringFormat("%u/%u steps", m_state.numStepsDone, m_state.numSteps);

        return "";
    }

}