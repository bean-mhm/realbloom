#pragma once

#include <vector>
#include <math.h>

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
        float amount = 0.0f;
        uint32_t steps = 32;
        float color[3]{ 0.0f, 0.0f, 0.0f };
    };

    struct DispersionState
    {
        uint32_t numSteps = 0;
        uint32_t numStepsDone = 0;
        bool working = false;
        bool mustCancel = false;
        bool failed = false;
        std::string error = "";
    };

    class Dispersion
    {
    private:
        DispersionState m_state;
        DispersionParams m_params;

        CmImage* m_imageDP = nullptr;
        CmImage* m_imageDisp = nullptr;

        std::thread* m_thread = nullptr;

    public:
        Dispersion();
        DispersionParams* getParams();

        void setDiffPatternImage(CmImage* image);
        void setDispersionImage(CmImage* image);

        void previewCmf();

        void compute();
        void cancel();

        bool isWorking() const;
        bool hasFailed() const;
        std::string getStats() const;
    };

}