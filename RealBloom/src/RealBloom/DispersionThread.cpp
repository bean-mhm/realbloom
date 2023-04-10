#include "DispersionThread.h"

namespace RealBloom
{

    DispersionThread::DispersionThread(
        uint32_t numThreads, uint32_t threadIndex, const DispersionParams& params,
        float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
        float* cmfSamples)
        : m_numThreads(numThreads), m_threadIndex(threadIndex), m_params(params),
        m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_cmfSamples(cmfSamples)
    {}

    void DispersionThread::start()
    {
        if (m_numThreads < 1)
            return;

        m_state.state = DispersionThreadState::Initializing;
        m_mustStop = false;

        // Parameters

        uint32_t steps = m_params.steps;

        m_state.numSteps = 0;
        m_state.numDone = 0;
        for (uint32_t i = 1; i <= steps; i++)
            if (i % m_numThreads == m_threadIndex)
                m_state.numSteps++;

        float amount = fminf(fmaxf(m_params.amount, 0.0f), 1.0f);

        float centerX = (float)m_inputWidth / 2.0f;
        float centerY = (float)m_inputHeight / 2.0f;

        // Scaled buffer for individual steps
        uint32_t scaledBufferSize = m_inputWidth * m_inputHeight * 3;
        std::vector<float> scaledBuffer;
        scaledBuffer.resize(scaledBufferSize);

        // Start

        m_state.state = DispersionThreadState::Working;

        for (uint32_t i = 0; i < steps; i++)
        {
            if (m_mustStop)
            {
                m_state.state = DispersionThreadState::Done;
                break;
            }

            if (i % m_numThreads != m_threadIndex)
                continue;

            float scale = lerp(
                1.0f - (amount / 2.0f),
                1.0f + (amount / 2.0f),
                (i + 1.0f) / (float)steps);

            float scaledArea = scale * scale;
            float scaledMul = 1.0f / scaledArea;

            uint32_t smpIndex = i * 3;
            float wlR = m_cmfSamples[smpIndex + 0] * scaledMul;
            float wlG = m_cmfSamples[smpIndex + 1] * scaledMul;
            float wlB = m_cmfSamples[smpIndex + 2] * scaledMul;

            // Clear scaledBuffer
            for (auto& v : scaledBuffer)
                v = 0.0f;

            // Scale
            if (areEqual(scale, 1))
            {
                for (int y = 0; y < m_inputHeight; y++)
                {
                    for (int x = 0; x < m_inputWidth; x++)
                    {
                        uint32_t redIndexDP = (y * m_inputWidth + x) * 4;
                        uint32_t redIndexScaled = (y * m_inputWidth + x) * 3;

                        scaledBuffer[redIndexScaled + 0] = m_inputBuffer[redIndexDP + 0];
                        scaledBuffer[redIndexScaled + 1] = m_inputBuffer[redIndexDP + 1];
                        scaledBuffer[redIndexScaled + 2] = m_inputBuffer[redIndexDP + 2];
                    }
                }
            }
            else
            {
                for (int y = 0; y < m_inputHeight; y++)
                {
                    float targetColor[3];
                    Bilinear bil;

                    for (int x = 0; x < m_inputWidth; x++)
                    {
                        float transX = (((x + 0.5f) - centerX) / scale) + centerX;
                        float transY = (((y + 0.5f) - centerY) / scale) + centerY;
                        bil.calc(transX, transY);

                        targetColor[0] = 0;
                        targetColor[1] = 0;
                        targetColor[2] = 0;

                        int redIndexBuffer;

                        if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.topLeftPos[1] * m_inputWidth + bil.topLeftPos[0]) * 4;
                            blendAddRGB(targetColor, 0, m_inputBuffer, redIndexBuffer, bil.topLeftWeight);
                        }
                        if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.topRightPos[1] * m_inputWidth + bil.topRightPos[0]) * 4;
                            blendAddRGB(targetColor, 0, m_inputBuffer, redIndexBuffer, bil.topRightWeight);
                        }
                        if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.bottomLeftPos[1] * m_inputWidth + bil.bottomLeftPos[0]) * 4;
                            blendAddRGB(targetColor, 0, m_inputBuffer, redIndexBuffer, bil.bottomLeftWeight);
                        }
                        if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], m_inputWidth, m_inputHeight))
                        {
                            redIndexBuffer = (bil.bottomRightPos[1] * m_inputWidth + bil.bottomRightPos[0]) * 4;
                            blendAddRGB(targetColor, 0, m_inputBuffer, redIndexBuffer, bil.bottomRightWeight);
                        }

                        int redIndexScaled = (y * m_inputWidth + x) * 3;
                        std::copy(targetColor, targetColor + 3, &(scaledBuffer[redIndexScaled]));
                    }
                }
            }

            // Colorize and add to dispBuffer
            for (int y = 0; y < m_inputHeight; y++)
            {
                for (int x = 0; x < m_inputWidth; x++)
                {
                    uint32_t redIndexScaled = (y * m_inputWidth + x) * 3;
                    uint32_t redIndexOutput = (y * m_inputWidth + x) * 4;

                    m_outputBuffer[redIndexOutput + 0] += scaledBuffer[redIndexScaled + 0] * wlR;
                    m_outputBuffer[redIndexOutput + 1] += scaledBuffer[redIndexScaled + 1] * wlG;
                    m_outputBuffer[redIndexOutput + 2] += scaledBuffer[redIndexScaled + 2] * wlB;
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

    std::vector<float>& DispersionThread::getOutputBuffer()
    {
        return m_outputBuffer;
    }

    std::shared_ptr<std::jthread> DispersionThread::getThread()
    {
        return m_thread;
    }

    void DispersionThread::setThread(std::shared_ptr<std::jthread> thread)
    {
        m_thread = thread;
    }

    DispersionThreadStats* DispersionThread::getStats()
    {
        return &m_state;
    }

}
