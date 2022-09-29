#pragma once

#include <vector>
#include <math.h>

#include "../Utils/NumberHelpers.h"
#include "../Utils/Bilinear.h"
#include "../Utils/Wavelength.h"

#include "../Utils/Misc.h"
#include "../Utils/Image32Bit.h"
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
        Image32Bit* m_imageDP = nullptr;
        Image32Bit* m_imageDisp = nullptr;
        std::thread* m_thread = nullptr;

    public:
        Dispersion();
        DispersionParams* getParams();
        void setDiffPatternImage(Image32Bit* image);
        void setDispersionImage(Image32Bit* image);

        void compute();
        void cancel();

        bool isWorking();

        std::string getStats();
    };

}