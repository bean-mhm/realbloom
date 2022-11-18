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

    void Dispersion::setDiffPatternImage(CmImage* image)
    {
        m_imageDP = image;
    }

    void Dispersion::setDispersionImage(CmImage* image)
    {
        m_imageDisp = image;
    }

    void Dispersion::previewCmf()
    {
        std::shared_ptr<CmfTable> table = CMF::getActiveTable();
        if (table.get() == nullptr)
            return;

        uint32_t pWidth = std::max(2u, (uint32_t)table->getCount() * 2);
        uint32_t pHeight = std::max(2u, (uint32_t)pWidth / 10);

        std::vector<float> buffer;
        buffer.resize(pWidth * pHeight * 4);

        // Get wavelength samples
        std::vector<float> samples;
        table->sampleRGB(pWidth, samples);

        // Copy to buffer
        uint32_t redIndex, smpIndex;
        for (uint32_t y = 0; y < pHeight; y++)
        {
            for (uint32_t x = 0; x < pWidth; x++)
            {
                redIndex = (y * pWidth + x) * 4;
                smpIndex = x * 3;

                buffer[redIndex + 0] = samples[smpIndex + 0];
                buffer[redIndex + 1] = samples[smpIndex + 1];
                buffer[redIndex + 2] = samples[smpIndex + 2];
                buffer[redIndex + 3] = 1.0f;
            }
        }

        // Copy to image
        {
            std::lock_guard<CmImage> lock(*m_imageDisp);
            m_imageDisp->resize(pWidth, pHeight, false);
            float* imageBuffer = m_imageDisp->getImageData();
            std::copy(buffer.data(), buffer.data() + buffer.size(), imageBuffer);
        }
        m_imageDisp->moveToGPU();
    }

    void Dispersion::compute()
    {
        cancel();

        m_state.numSteps = m_params.steps;
        m_state.numStepsDone = 0;
        m_state.working = true;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.error = "";

        // Start the thread
        m_thread = new std::thread([this]()
            {
                try
                {
                    // CMF table
                    std::shared_ptr<CmfTable> table = CMF::getActiveTable();
                    if (table.get() == nullptr)
                        throw std::exception("An active CMF table is needed.");

                    // Diff pattern Buffer
                    std::vector<float> dpBuffer;
                    uint32_t dpWidth = 0, dpHeight = 0;
                    uint32_t dpBufferSize = 0;
                    {
                        // Buffer from diff pattern image
                        std::lock_guard<CmImage> dpImageLock(*m_imageDP);
                        float* dpImageData = m_imageDP->getImageData();
                        dpWidth = m_imageDP->getWidth();
                        dpHeight = m_imageDP->getHeight();

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

                    float wlR = 0, wlG = 0, wlB = 0;
                    float scale = 0;

                    // Scaled buffer for individual steps
                    uint32_t scaledBufferSize = dpWidth * dpHeight * 3;
                    std::vector<float> scaledBuffer;
                    scaledBuffer.resize(scaledBufferSize);

                    float centerX, centerY;
                    centerX = (float)dpWidth / 2.0f;
                    centerY = (float)dpHeight / 2.0f;

                    // Create and clear the dispersion buffer
                    std::vector<float> dispBuffer;
                    dispBuffer.resize(dpBufferSize);
                    for (uint32_t i = 0; i < dpBufferSize; i++)
                        if (i % 4 == 3)
                            dispBuffer[i] = 1.0f;
                        else
                            dispBuffer[i] = 0.0f;

                    // Sample wavelengths
                    std::vector<float> cmfSamples;
                    table->sampleRGB(dispSteps, cmfSamples);
                    if (cmfSamples.size() < (dispSteps * 3))
                        throw std::exception("Invalid number of samples provided by CmfTable.");

                    // Calculate the perceived luminance for the sum of the samples
                    float luminanceSum = 0.0f, luminanceMul = 0.0f;
                    {
                        uint32_t smpIndex;
                        for (uint32_t i = 0; i < dispSteps; i++)
                        {
                            smpIndex = i * 3;
                            luminanceSum += rgbToGrayscale(
                                cmfSamples[smpIndex + 0],
                                cmfSamples[smpIndex + 1],
                                cmfSamples[smpIndex + 2]
                            );
                        }
                        luminanceMul = 1.0f / luminanceSum;
                    }

                    // Apply Dispersion
                    auto lastProgTime = std::chrono::system_clock::now();
                    uint32_t smpIndex;
                    for (uint32_t i = 1; i <= dispSteps; i++)
                    {
                        scale = 1.0f - (dispAmount * (1.0f - (i / (float)dispSteps)));
                        scale = fmaxf(EPSILON, scale);

                        float scaledArea = scale * scale;
                        float scaledMul = 1.0f / scaledArea;

                        smpIndex = (i - 1) * 3;
                        wlR = cmfSamples[smpIndex + 0] * luminanceMul * scaledMul * dispColor[0];
                        wlG = cmfSamples[smpIndex + 1] * luminanceMul * scaledMul * dispColor[1];
                        wlB = cmfSamples[smpIndex + 2] * luminanceMul * scaledMul * dispColor[2];

                        // Clear scaledBuffer
                        for (uint32_t i = 0; i < scaledBufferSize; i++)
                            scaledBuffer[i] = 0;

                        // Scale
                        if (areEqual(scale, 1))
                        {
                            uint32_t redIndexDP, redIndexScaled;
                            for (uint32_t y = 0; y < dpHeight; y++)
                            {
                                for (uint32_t x = 0; x < dpWidth; x++)
                                {
                                    redIndexDP = (y * dpWidth + x) * 4;
                                    redIndexScaled = (y * dpWidth + x) * 3;

                                    scaledBuffer[redIndexScaled + 0] = dpBuffer[redIndexDP + 0];
                                    scaledBuffer[redIndexScaled + 1] = dpBuffer[redIndexDP + 1];
                                    scaledBuffer[redIndexScaled + 2] = dpBuffer[redIndexDP + 2];
                                }
                            }
                        } else
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
                            uint32_t redIndexBuffer, redIndexScaled;
                            for (uint32_t y = 0; y < dpHeight; y++)
                            {
                                for (uint32_t x = 0; x < dpWidth; x++)
                                {
                                    redIndexScaled = (y * dpWidth + x) * 3;
                                    redIndexBuffer = (y * dpWidth + x) * 4;

                                    dispBuffer[redIndexBuffer + 0] += scaledBuffer[redIndexScaled + 0] * wlR;
                                    dispBuffer[redIndexBuffer + 1] += scaledBuffer[redIndexScaled + 1] * wlG;
                                    dispBuffer[redIndexBuffer + 2] += scaledBuffer[redIndexScaled + 2] * wlB;
                                }
                            }
                        }

                        if (m_state.mustCancel)
                            throw std::exception();

                        // Update the progress
                        m_state.numStepsDone = i;
                        if ((i % DISP_PROG_INTERVAL == 0) &&                      // update every x steps
                            (getElapsedMs(lastProgTime) > DISP_PROG_TIMESTEP) &&  // update every x ms
                            (m_state.numStepsDone < m_state.numSteps))         // skip the last step
                        {
                            {
                                std::lock_guard<CmImage> dispImageLock(*m_imageDisp);
                                m_imageDisp->resize(dpWidth, dpHeight, false);
                                float* imageBuffer = m_imageDisp->getImageData();
                                std::copy(dispBuffer.data(), dispBuffer.data() + dpBufferSize, imageBuffer);
                            }
                            m_imageDisp->moveToGPU();

                            lastProgTime = std::chrono::system_clock::now();
                        }
                    }

                    if (m_state.mustCancel)
                        throw std::exception();

                    // Update the dispersion image
                    {
                        {
                            std::lock_guard<CmImage> dispImageLock(*m_imageDisp);
                            m_imageDisp->resize(dpWidth, dpHeight, false);
                            float* imageBuffer = m_imageDisp->getImageData();
                            std::copy(dispBuffer.data(), dispBuffer.data() + dpBufferSize, imageBuffer);
                        }
                        m_imageDisp->moveToGPU();
                        Async::schedule([this]()
                            {
                                int* pSelImageIndex = (int*)Async::getShared("selImageIndex");
                                if (pSelImageIndex != nullptr) *pSelImageIndex = 2;
                            });
                    }
                } catch (const std::exception& e)
                {
                    if (!m_state.mustCancel)
                    {
                        m_state.failed = true;
                        m_state.error = e.what();
                    }
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
        m_state.failed = false;
        m_state.error = "";
    }

    bool Dispersion::isWorking() const
    {
        return m_state.working;
    }

    bool Dispersion::hasFailed() const
    {
        return m_state.failed;
    }

    std::string Dispersion::getStats() const
    {
        if (m_state.working && !m_state.mustCancel)
            return strFormat("%u/%u steps", m_state.numStepsDone, m_state.numSteps);

        if (m_state.failed)
            return m_state.error;

        return "";
    }

}