#include "ConvolutionThread.h"
#include "Convolution.h"

namespace RealBloom
{

    ConvolutionThread::ConvolutionThread(
        uint32_t numThreads, uint32_t threadIndex, const ConvolutionParams& params,
        float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
        float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
        : m_numThreads(numThreads), m_threadIndex(threadIndex),
        m_params(params),
        m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
        m_kernelBuffer(kernelBuffer), m_kernelWidth(kernelWidth), m_kernelHeight(kernelHeight)
    {}

    void ConvolutionThread::start()
    {
        if (m_numThreads < 1)
            return;

        m_state.state = ConvolutionThreadState::Initializing;
        m_mustStop = false;

        // Parameters

        float threshold = m_params.threshold;
        float transKnee = transformKnee(m_params.knee);

        int kernelCenterX = (int)floorf(m_params.kernelCenterX * (float)m_kernelWidth);
        int kernelCenterY = (int)floorf(m_params.kernelCenterY * (float)m_kernelHeight);

        uint32_t inputPixels = m_inputWidth * m_inputHeight;

        // Count the number of pixels that pass the threshold
        {
            uint32_t num = 0;

            float v;
            float inpColor[3];
            uint32_t redIndexInput;
            for (uint32_t i = 0; i < inputPixels; i++)
            {
                if (i % m_numThreads == m_threadIndex)
                {
                    redIndexInput = i * 4;
                    inpColor[0] = m_inputBuffer[redIndexInput + 0];
                    inpColor[1] = m_inputBuffer[redIndexInput + 1];
                    inpColor[2] = m_inputBuffer[redIndexInput + 2];

                    v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                    if (v > threshold)
                        num++;
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
            float mul;
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

                    v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                    if (v > threshold)
                    {
                        // Smooth Transition
                        mul = softThreshold(v, threshold, transKnee);
                        inpColor[0] *= mul;
                        inpColor[1] *= mul;
                        inpColor[2] *= mul;

                        // (Disabled, seems to have no effect)
                        // Check if we will stay inside the image boundaries
                        // Disable bounds checking when guaranteed to stay in bounds

                        /*bool willStayInside;
                        {
                            int rectX1 = ix - kernelCenterX;
                            int rectY1 = iy - kernelCenterY;
                            int rectX2 = (int)m_kernelWidth - 1 - kernelCenterX + ix;
                            int rectY2 = (int)m_kernelHeight - 1 - kernelCenterY + iy;

                            willStayInside = ((rectX1 >= 0) &&
                                (rectY1 >= 0) &&
                                (rectX2 < m_inputWidth) &&
                                (rectY2 < m_inputHeight));
                        }*/

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
            float mul;
            for (uint32_t i = 0; i < inputPixels; i++)
            {
                if (m_mustStop)
                {
                    m_state.state = ConvolutionThreadState::Done;
                    break;
                }

                if (i % m_numThreads != m_threadIndex)
                    continue;

                redIndexInput = i * 4;
                inpColor[0] = m_inputBuffer[redIndexInput + 0];
                inpColor[1] = m_inputBuffer[redIndexInput + 1];
                inpColor[2] = m_inputBuffer[redIndexInput + 2];

                v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                if (v > threshold)
                {
                    // Smooth Transition
                    mul = softThreshold(v, threshold, transKnee);
                    inpColor[0] *= mul;
                    inpColor[1] *= mul;
                    inpColor[2] *= mul;

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
                    m_state.numDone++;
                }
            }
        }
        m_state.state = ConvolutionThreadState::Done;
    }

    void ConvolutionThread::stop()
    {
        m_mustStop = true;
    }

    std::vector<float>& ConvolutionThread::getBuffer()
    {
        return m_outputBuffer;
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