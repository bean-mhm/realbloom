#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <math.h>
#include <stdint.h>

#include "../Utils/NumberHelpers.h"
#include "../Utils/Bilinear.h"

#include "../ColorManagement/CmImage.h"
#include "../ColorManagement/CMF.h"
#include "../Utils/Misc.h"
#include "../Async.h"

namespace RealBloom
{
    constexpr int DISP_MAX_STEPS = 2048;

    struct DispersionParams
    {
        float exposure = 0.0f;
        float contrast = 0.0f;
        uint32_t steps = 32;
        float amount = 0.0f;
        std::array<float, 3> color{ 1, 1, 1 };
        uint32_t numThreads = 1;
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

        uint32_t numSteps = 0;
    };

    class DispersionThread;

    class Dispersion
    {
    private:
        DispersionState m_state;
        DispersionParams m_params;

        CmImage* m_imgInput = nullptr;
        CmImage* m_imgDisp = nullptr;

        CmImage m_imgInputSrc;

        std::thread* m_thread = nullptr;
        std::vector<DispersionThread*> m_threads;

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

        uint32_t getNumDone() const;
        std::string getStats() const;
    };

}