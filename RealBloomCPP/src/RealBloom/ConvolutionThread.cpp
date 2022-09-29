#include "ConvolutionThread.h"
#include "Convolution.h"

namespace RealBloom
{

    ConvolutionThread::ConvolutionThread(
        uint32_t numThreads, uint32_t threadIndex, ConvolutionParams& convParams,
        float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
        float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
        : m_numThreads(numThreads), m_threadIndex(threadIndex), m_state(), m_mustStop(false),
        m_params(convParams),
        m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_kernelBuffer(kernelBuffer), m_kernelWidth(kernelWidth), m_kernelHeight(kernelHeight)
    {
        m_state.numPixels = 1;
        m_state.numDone = 0;
        m_state.state = ConvolutionThreadState::None;
    }

    void ConvolutionThread::start()
    {
        if (m_numThreads < 1)
            return;

        m_state.state = ConvolutionThreadState::Initializing;
        m_mustStop = false;

        // Parameters
        float threshold = m_params.convThreshold;
        int kernelCenterX = (int)floorf(m_params.kernelCenterX * (float)m_kernelWidth);
        int kernelCenterY = (int)floorf(m_params.kernelCenterY * (float)m_kernelHeight);
        uint32_t inputPixels = m_inputWidth * m_inputHeight;
        // Count the number of pixels that pass the threshold
        {
            uint32_t num = 0;

            float v;
            float inpColor[3];
            int redIndexInput;
            for (uint32_t i = 0; i < inputPixels; i++)
            {
                if (i % m_numThreads == m_threadIndex)
                {
                    redIndexInput = i * 4;
                    inpColor[0] = m_inputBuffer[redIndexInput + 0];
                    inpColor[1] = m_inputBuffer[redIndexInput + 1];
                    inpColor[2] = m_inputBuffer[redIndexInput + 2];

                    v = inpColor[0];
                    if (inpColor[1] > v)
                        v = inpColor[1];
                    if (inpColor[2] > v)
                        v = inpColor[2];

                    if (v > threshold)
                    {
                        num++;
                    }
                }
            }

            m_state.numPixels = num;
        }

        if (m_state.numPixels < 1)
        {
            m_state.state = ConvolutionThreadState::Done;
            return;
        }

        m_state.state = ConvolutionThreadState::Working;
        if (m_numThreads <= 1)
        {
            // Convolve
            float v;
            float inpColor[3];
            float convColor[3];
            int redIndexInput, redIndexKernel, redIndexConv;
            int cx, cy;
            for (int iy = 0; iy < (int)m_inputHeight; iy++)
            {
                if (m_mustStop)
                {
                    m_state.state = ConvolutionThreadState::Done;
                    break;
                }

                for (int ix = 0; ix < (int)m_inputWidth; ix++)
                {
                    redIndexInput = (iy * m_inputWidth + ix) * 4;
                    inpColor[0] = m_inputBuffer[redIndexInput + 0];
                    inpColor[1] = m_inputBuffer[redIndexInput + 1];
                    inpColor[2] = m_inputBuffer[redIndexInput + 2];

                    v = inpColor[0];
                    if (inpColor[1] > v)
                        v = inpColor[1];
                    if (inpColor[2] > v)
                        v = inpColor[2];

                    if (v > threshold)
                    {
                        for (int ky = 0; ky < (int)m_kernelHeight; ky++)
                        {
                            for (int kx = 0; kx < (int)m_kernelWidth; kx++)
                            {
                                cx = (kx - kernelCenterX) + ix;
                                cy = (ky - kernelCenterY) + iy;
                                if (checkBounds(cx, cy, m_inputWidth, m_inputHeight))
                                {
                                    redIndexKernel = (ky * m_kernelWidth + kx) * 4;

                                    convColor[0] = m_kernelBuffer[redIndexKernel + 0] * inpColor[0];
                                    convColor[1] = m_kernelBuffer[redIndexKernel + 1] * inpColor[1];
                                    convColor[2] = m_kernelBuffer[redIndexKernel + 2] * inpColor[2];

                                    redIndexConv = (cy * m_inputWidth + cx) * 4;
                                    blendAddRGB(m_outputBuffer.data(), redIndexConv, convColor, 0, 1);
                                }
                            }
                        }
                        m_state.numDone += 1;
                    }
                }
            }
        } else
        {
            float v;
            float inpColor[3];
            float convColor[3];
            int redIndexInput, redIndexKernel, redIndexConv;
            int ix, iy, cx, cy;
            for (uint32_t i = 0; i < inputPixels; i++)
            {
                if (m_mustStop)
                {
                    m_state.state = ConvolutionThreadState::Done;
                    break;
                }

                if (i % m_numThreads == m_threadIndex)
                {
                    redIndexInput = i * 4;
                    inpColor[0] = m_inputBuffer[redIndexInput + 0];
                    inpColor[1] = m_inputBuffer[redIndexInput + 1];
                    inpColor[2] = m_inputBuffer[redIndexInput + 2];

                    v = inpColor[0];
                    if (inpColor[1] > v)
                        v = inpColor[1];
                    if (inpColor[2] > v)
                        v = inpColor[2];

                    if (v > threshold)
                    {
                        ix = i % m_inputWidth;
                        iy = (i - ix) / m_inputWidth;
                        for (int ky = 0; ky < (int)m_kernelHeight; ky++)
                        {
                            for (int kx = 0; kx < (int)m_kernelWidth; kx++)
                            {
                                cx = (kx - kernelCenterX) + ix;
                                cy = (ky - kernelCenterY) + iy;
                                if (checkBounds(cx, cy, m_inputWidth, m_inputHeight))
                                {
                                    redIndexKernel = (ky * m_kernelWidth + kx) * 4;

                                    convColor[0] = m_kernelBuffer[redIndexKernel + 0] * inpColor[0];
                                    convColor[1] = m_kernelBuffer[redIndexKernel + 1] * inpColor[1];
                                    convColor[2] = m_kernelBuffer[redIndexKernel + 2] * inpColor[2];

                                    redIndexConv = (cy * m_inputWidth + cx) * 4;
                                    blendAddRGB(m_outputBuffer.data(), redIndexConv, convColor, 0, 1);
                                }
                            }
                        }
                        m_state.numDone += 1;
                    }
                }
            }
        }
        m_state.state = ConvolutionThreadState::Done;
    }

    void ConvolutionThread::stop()
    {
        m_mustStop = true;
    }

    std::vector<float>* ConvolutionThread::getBuffer()
    {
        return &m_outputBuffer;
    }

    std::shared_ptr<std::thread> ConvolutionThread::getThread()
    {
        return m_thread;
    }

    void ConvolutionThread::setThread(std::shared_ptr<std::thread> thread)
    {
        m_thread = thread;
    }

    ConvolutionThreadStats* ConvolutionThread::getStats()
    {
        return &m_state;
    }

}