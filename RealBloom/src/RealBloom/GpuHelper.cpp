#include "GpuHelper.h"

std::string strFromGpuHelperOperationType(GpuHelperOperationType opType)
{
    if (opType == GpuHelperOperationType::Diffraction)
        return "Diffraction";

    if (opType == GpuHelperOperationType::Dispersion)
        return "Dispersion";

    if (opType == GpuHelperOperationType::ConvNaive)
        return "ConvNaive";

    if (opType == GpuHelperOperationType::ConvFFT)
        return "ConvFFT";

    return "";
}
