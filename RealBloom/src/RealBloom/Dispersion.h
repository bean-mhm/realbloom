#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>
#include <cmath>

#include "GpuHelper.h"
#include "Binary/BinaryData.h"
#include "Binary/BinaryDispGpu.h"

#include "../ColorManagement/CmImage.h"
#include "../ColorManagement/CMF.h"

#include "../Utils/Bilinear.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
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

    class DispersionThread;

    class Dispersion
    {
    private:
        TimedWorkingStatus m_status;
        DispersionParams m_params;
        DispersionParams m_capturedParams;

        CmImage* m_imgInput = nullptr;
        CmImage* m_imgDisp = nullptr;

        CmImage m_imgInputSrc;

        std::shared_ptr<std::jthread> m_thread = nullptr;
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

        uint32_t getNumStepsDoneCpu() const;

        const TimedWorkingStatus& getStatus() const;
        std::string getStatusText() const;

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
