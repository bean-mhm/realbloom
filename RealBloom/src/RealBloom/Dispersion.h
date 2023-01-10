#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <math.h>
#include <stdint.h>

#include "GpuHelper.h"
#include "Binary/BinaryData.h"
#include "Binary/BinaryDispGpu.h"

#include "../ColorManagement/CmImage.h"
#include "../ColorManagement/CMF.h"

#include "../Utils/Bilinear.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{
    constexpr int DISP_MAX_STEPS = 2048;

    enum class DispersionMethod
    {
        CPU = 0,
        GPU = 1
    };

    struct DispersionMethodInfo
    {
        DispersionMethod method = DispersionMethod::CPU;
        uint32_t numThreads = 1;
    };

    struct DispersionParams
    {
        DispersionMethodInfo methodInfo;
        float exposure = 0.0f;
        float contrast = 0.0f;
        std::array<float, 3> color{ 1, 1, 1 };
        float amount = 0.0f;
        uint32_t steps = 32;
    };

    struct DispersionState
    {
        bool working = false;
        bool mustCancel = false;

        bool failed = false;
        std::string error = "";

        std::chrono::time_point<std::chrono::system_clock> timeStart;
        std::chrono::time_point<std::chrono::system_clock> timeEnd;
        bool hasTimestamps = false;

        std::string getError() const;
        void setError(const std::string& err);
    };

    class DispersionThread;

    class Dispersion
    {
    private:
        DispersionState m_state;
        DispersionParams m_params;
        DispersionParams m_capturedParams;

        CmImage* m_imgInput = nullptr;
        CmImage* m_imgDisp = nullptr;

        CmImage m_imgInputSrc;

        std::thread* m_thread = nullptr;
        std::vector<std::shared_ptr<DispersionThread>> m_threads;

    public:
        Dispersion();
        DispersionParams* getParams();

        void setImgInput(CmImage* image);
        void setImgDisp(CmImage* image);

        CmImage* getImgInputSrc();

        void previewCmf(CmfTable* table);
        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void compute();
        void cancel();

        bool isWorking() const;
        bool hasFailed() const;
        std::string getError() const;

        uint32_t getNumStepsDone() const;
        std::string getStatus() const;

    private:
        void dispCPU(
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize,
            std::vector<float>& cmfSamples);

        void dispGPU(
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize,
            std::vector<float>& cmfSamples);

    };

}
