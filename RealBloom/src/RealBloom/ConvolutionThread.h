#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <cstdint>

#include "Convolution.h"
#include "../Utils/NumberHelpers.h"

namespace RealBloom
{

    enum class ConvolutionThreadState
    {
        None,
        Initializing,
        Working,
        Done,
    };

    struct ConvolutionThreadStats
    {
        ConvolutionThreadState state = ConvolutionThreadState::None;
        uint32_t numPixels = 1;
        uint32_t numDone = 0;
    };

    class ConvolutionThread
    {
    private:
        uint32_t m_numThreads;
        uint32_t m_threadIndex;
        std::shared_ptr<std::jthread> m_thread;

        ConvolutionThreadStats m_state;
        std::atomic_bool m_mustStop = false;

        ConvolutionParams m_params;

        float* m_inputBuffer;
        uint32_t m_inputWidth;
        uint32_t m_inputHeight;

        float* m_kernelBuffer;
        uint32_t m_kernelWidth;
        uint32_t m_kernelHeight;

        std::vector<float> m_outputBuffer;

    public:
        ConvolutionThread(
            uint32_t numThreads, uint32_t threadIndex, const ConvolutionParams& params,
            float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
            float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight);

        void start();
        void stop();

        std::vector<float>& getBuffer();
        std::shared_ptr<std::jthread> getThread();
        void setThread(std::shared_ptr<std::jthread> thread);

        ConvolutionThreadStats* getStats();
    };

}
