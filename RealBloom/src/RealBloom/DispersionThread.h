#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <cstdint>

#include "Dispersion.h"
#include "../Utils/NumberHelpers.h"

namespace RealBloom
{

    enum class DispersionThreadState
    {
        None,
        Initializing,
        Working,
        Done,
    };

    struct DispersionThreadStats
    {
        DispersionThreadState state = DispersionThreadState::None;
        uint32_t numSteps = 1;
        uint32_t numDone = 0;
    };

    // Dispersion Thread, used for method: CPU
    class DispersionThread
    {
    public:
        DispersionThread(
            uint32_t numThreads, uint32_t threadIndex, const DispersionParams& params,
            float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight,
            float* cmfSamples);

        void start();
        void stop();

        std::vector<float>& getOutputBuffer();
        std::shared_ptr<std::jthread> getThread();
        void setThread(std::shared_ptr<std::jthread> thread);

        DispersionThreadStats* getStats();

    private:
        uint32_t m_numThreads;
        uint32_t m_threadIndex;
        std::shared_ptr<std::jthread> m_thread;

        DispersionThreadStats m_state;
        std::atomic_bool m_mustStop;

        DispersionParams m_params;

        float* m_inputBuffer;
        uint32_t m_inputWidth;
        uint32_t m_inputHeight;

        float* m_cmfSamples;

        std::vector<float> m_outputBuffer;

    };

}
