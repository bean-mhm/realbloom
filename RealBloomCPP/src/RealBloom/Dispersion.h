#pragma once

#include <vector>
#include <math.h>

#include "../Utils/NumberHelpers.h"
#include "../Utils/Bilinear.h"
#include "../Utils/Wavelength.h"

#include "../ColorManagement/CMImage.h"
#include "../Utils/Misc.h"
#include "../Async.h"

namespace RealBloom
{
    constexpr int DISP_MAX_STEPS = 1024;

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
    };

    class Dispersion
    {
    private:
        DispersionState m_state;
        DispersionParams m_params;
        CMImage* m_imageDP = nullptr;
        CMImage* m_imageDisp = nullptr;
        std::thread* m_thread = nullptr;

    public:
        Dispersion();
        DispersionParams* getParams();
        void setDiffPatternImage(CMImage* image);
        void setDispersionImage(CMImage* image);

        void compute();
        void cancel();

        bool isWorking();

        std::string getStats();
    };

}