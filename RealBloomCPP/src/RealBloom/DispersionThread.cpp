#include "DispersionThread.h"

namespace RealBloom
{

    DispersionThread::DispersionThread(
        uint32_t numThreads, uint32_t threadIndex, const DispersionParams& params,
        float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
        float* cmfSamples, float cmfLuminanceMul)
        : m_numThreads(numThreads), m_threadIndex(threadIndex), m_params(params),
        m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_cmfSamples(cmfSamples), m_cmfLuminanceMul(cmfLuminanceMul)
    {}

    void DispersionThread::start()
    {
        if (m_numThreads < 1)
            return;

        m_state.state = DispersionThreadState::Initializing;
        m_mustStop = false;

        // Parameters

        uint32_t dispSteps = m_params.steps;

        m_state.numSteps = 0;
        m_state.numDone = 0;
        for (uint32_t i = 1; i <= dispSteps; i++)
            if (i % m_numThreads == m_threadIndex)
                m_state.numSteps++;

        float dispAmount = fminf(fmaxf(m_params.amount, 0.0f), 1.0f);
        std::array<float, 3> dispColor = m_params.color;

        float centerX = (float)m_inputWidth / 2.0f;
        float centerY = (float)m_inputHeight / 2.0f;

        // Scaled buffer for individual steps
        uint32_t scaledBufferSize = m_inputWidth * m_inputHeight * 3;
        std::vector<float> scaledBuffer;
        scaledBuffer.resize(scaledBufferSize);

        // Start

        m_state.state = DispersionThreadState::Working;

        for (uint32_t i = 1; i <= dispSteps; i++)
        {
            if (m_mustStop)
            {
                m_state.state = DispersionThreadState::Done;
                break;
            }

            if (i % m_numThreads != m_threadIndex)
                continue;

            float scale = 1.0f - (dispAmount * (1.0f - (i / (float)dispSteps)));
            scale = fmaxf(EPSILON, scale);

            float scaledArea = scale * scale;
            float scaledMul = 1.0f / scaledArea;

            uint32_t smpIndex = (i - 1) * 3;
            float wlR = m_cmfSamples[smpIndex + 0] * m_cmfLuminanceMul * scaledMul * dispColor[0];
            float wlG = m_cmfSamples[smpIndex + 1] * m_cmfLuminanceMul * scaledMul * dispColor[1];
            float wlB = m_cmfSamples[smpIndex + 2] * m_cmfLuminanceMul * scaledMul * dispColor[2];

            // Clear scaledBuffer
            for (uint32_t i = 0; i < scaledBufferSize; i++)
                scaledBuffer[i] = 0;

            // Scale
            if (areEqual(scale, 1))
            {
                uint32_t redIndexDP, redIndexScaled;
                for (uint32_t y = 0; y < m_inputHeight; y++)
                {
                    for (uint32_t x = 0; x < m_inputWidth; x++)
                    {
                        redIndexDP = (y * m_inputWidth + x) * 4;
                        redIndexScaled = (y * m_inputWidth + x) * 3;

                        scaledBuffer[redIndexScaled + 0] = m_inputBuffer[redIndexDP + 0];
                        scaledBuffer[redIndexScaled + 1] = m_inputBuffer[redIndexDP + 1];
                        scaledBuffer[redIndexScaled + 2] = m_inputBuffer[redIndexDP + 2];
                    }
                }
            }
            else
            {
                float scaledWidth = (float)m_inputWidth * scale;
                float scaledHeight = (float)m_inputHeight * scale;
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
                        redIndexScaled = (y * m_inputWidth + x) * 3;
                        transX = (((float)x + 0.5f - centerX) / scale) + centerX;
                        transY = (((float)y + 0.5f - centerY) / scale) + centerY;
                        Bilinear::calculate(transX, transY, bil);

                        if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.topLeftPos[1] * m_inputWidth + bil.topLeftPos[0]) * 4;
                            blendAddRGB(scaledBuffer.data(), redIndexScaled, m_inputBuffer, redIndexBuffer, bil.topLeftWeight);
                        }

                        if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.topRightPos[1] * m_inputWidth + bil.topRightPos[0]) * 4;
                            blendAddRGB(scaledBuffer.data(), redIndexScaled, m_inputBuffer, redIndexBuffer, bil.topRightWeight);
                        }

                        if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.bottomLeftPos[1] * m_inputWidth + bil.bottomLeftPos[0]) * 4;
                            blendAddRGB(scaledBuffer.data(), redIndexScaled, m_inputBuffer, redIndexBuffer, bil.bottomLeftWeight);
                        }

                        if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.bottomRightPos[1] * m_inputWidth + bil.bottomRightPos[0]) * 4;
                            blendAddRGB(scaledBuffer.data(), redIndexScaled, m_inputBuffer, redIndexBuffer, bil.bottomRightWeight);
                        }
                    }
                }
            }

            // Colorize and add to dispBuffer
            {
                uint32_t redIndexOutput, redIndexScaled;
                for (uint32_t y = 0; y < m_inputHeight; y++)
                {
                    for (uint32_t x = 0; x < m_inputWidth; x++)
                    {
                        redIndexScaled = (y * m_inputWidth + x) * 3;
                        redIndexOutput = (y * m_inputWidth + x) * 4;

                        m_outputBuffer[redIndexOutput + 0] += scaledBuffer[redIndexScaled + 0] * wlR;
                        m_outputBuffer[redIndexOutput + 1] += scaledBuffer[redIndexScaled + 1] * wlG;
                        m_outputBuffer[redIndexOutput + 2] += scaledBuffer[redIndexScaled + 2] * wlB;
                    }
                }
            }

            m_state.numDone++;
        }

        m_state.state = DispersionThreadState::Done;
    }

    void DispersionThread::stop()
    {
        m_mustStop = true;
    }

    std::vector<float>& DispersionThread::getBuffer()
    {
        return m_outputBuffer;
    }

    std::shared_ptr<std::thread> DispersionThread::getThread()
    {
        return m_thread;
    }

    void DispersionThread::setThread(std::shared_ptr<std::thread> thread)
    {
        m_thread = thread;
    }

    DispersionThreadStats* DispersionThread::getStats()
    {
        return &m_state;
    }

}
