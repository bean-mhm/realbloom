#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <cstdint>
#include <cmath>

#include "GpuHelper.h"
#include "Binary/BinaryData.h"
#include "Binary/BinaryDispGpu.h"

#include "ModuleHelpers.h"

#include "../ColorManagement/CmImage.h"
#include "../ColorManagement/CMF.h"

#include "../Utils/ImageTransform.h"
#include "../Utils/Bilinear.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{
    constexpr uint32_t DISP_MAX_STEPS = 2048;

    enum class DispersionMethod
    {
        CPU,
        GPU
    };
    constexpr uint32_t DispersionMethod_EnumSize = 2;

    struct DispersionMethodInfo
    {
        DispersionMethod method = DispersionMethod::CPU;
        uint32_t numThreads = getDefNumThreads();
    };

    struct DispersionParams
    {
        DispersionMethodInfo methodInfo;
        ImageTransformParams inputTransformParams;
        float amount = 0.4f;
        uint32_t steps = 32;
    };

    class DispersionThread;

    // Dispersion module
    class Dispersion
    {
    public:
        Dispersion();
        DispersionParams* getParams();

        CmImage* getImgInputSrc();
        void setImgInput(CmImage* image);

        void setImgDisp(CmImage* image);

        void previewCmf(CmfTable* table);
        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void compute();
        void cancel();

        const TimedWorkingStatus& getStatus() const;
        std::string getStatusText() const;
        uint32_t getNumStepsDoneCpu() const;

    private:
        TimedWorkingStatus m_status;
        DispersionParams m_params;
        DispersionParams m_capturedParams;

        CmImage m_imgInputSrc;
        CmImage* m_imgInput = nullptr;

        CmImage* m_imgDisp = nullptr;


        std::shared_ptr<std::jthread> m_thread = nullptr;
        std::vector<std::shared_ptr<DispersionThread>> m_threads;

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
