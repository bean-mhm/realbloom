#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <complex>
#include <stdint.h>

#include "glfft/glfft.hpp"
#include "glfft/glfft_gl_interface.hpp"

#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryConvFftGpu.h"

#include "Utils/GlShaderStorageBuffer.h"
#include "Utils/Array2D.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

namespace RealBloom
{

    class ConvFftGpu
    {
    private:
        BinaryConvFftGpuInput* m_binInput;

        uint32_t m_paddedWidth = 0;
        uint32_t m_paddedHeight = 0;
        uint32_t m_inputLeftPadding = 0;
        uint32_t m_inputTopPadding = 0;
        uint32_t m_kernelLeftPadding = 0;
        uint32_t m_kernelTopPadding = 0;

        Array2D<float> m_inputPadded[3];
        Array2D<float> m_kernelPadded[3];

        std::shared_ptr<GLFFT::GLContext> m_context = nullptr;
        std::shared_ptr<GLFFT::ProgramCache> m_programCache = nullptr;

        std::shared_ptr<GLFFT::Buffer> m_inputPaddedSSBO[3];
        std::shared_ptr<GLFFT::Buffer> m_inputFtSSBO[3];
        std::shared_ptr<GLFFT::Buffer> m_kernelPaddedSSBO[3];
        std::shared_ptr<GLFFT::Buffer> m_kernelFtSSBO[3];
        std::shared_ptr<GLFFT::Buffer> m_iFftSSBO[3];
        Array2D<float> m_iFFT[3];

        std::vector<float> m_outputBuffer;

    public:
        ConvFftGpu(BinaryConvFftGpuInput* binInput);
        ~ConvFftGpu();

        void pad();
        void inputFFT(uint32_t ch);
        void kernelFFT(uint32_t ch);
        void inverseConvolve(uint32_t ch);
        void output();

        const std::vector<float>& getBuffer() const;
    };

}
