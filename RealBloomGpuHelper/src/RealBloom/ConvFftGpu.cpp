#include "ConvFftGpu.h"

namespace RealBloom
{
    ConvFftGpu::ConvFftGpu(BinaryConvFftGpuInput* binInput)
        : m_binInput(binInput)
    {
        m_context = std::make_shared<GLFFT::GLContext>();
        checkGlStatus(__FUNCTION__, "m_context");

        m_programCache = std::make_shared<GLFFT::ProgramCache>();
    }

    ConvFftGpu::~ConvFftGpu()
    {}

    void ConvFftGpu::pad()
    {
        // Padded size
        calcFftConvPadding(
            true,
            m_binInput->inputWidth, m_binInput->inputHeight,
            m_binInput->kernelWidth, m_binInput->kernelHeight,
            m_binInput->cp_kernelCenterX, m_binInput->cp_kernelCenterY,
            m_paddedWidth, m_paddedHeight
        );

        // Padding amount

        m_inputLeftPadding = floorf((float)(m_paddedWidth - m_binInput->inputWidth) / 2.0f);
        m_inputTopPadding = floorf((float)(m_paddedHeight - m_binInput->inputHeight) / 2.0f);

        m_kernelLeftPadding = floorf(((float)m_paddedWidth / 2.0f) - ((float)m_binInput->kernelWidth * m_binInput->cp_kernelCenterX));
        m_kernelTopPadding = floorf(((float)m_paddedHeight / 2.0f) - ((float)m_binInput->kernelHeight * m_binInput->cp_kernelCenterY));

        if (m_binInput->kernelWidth % 2 == 1) m_kernelLeftPadding += 1;
        if (m_binInput->kernelHeight % 2 == 1) m_kernelTopPadding += 1;

        // Print the dimensions
        if (0)
        {
            std::cout << strFormat(
                "FFT Convolution\n"
                "Input:          %u x %u\n"
                "Kernel:         %u x %u\n"
                "Padded:         %u x %u\n\n",
                m_binInput->inputWidth, m_binInput->inputHeight,
                m_binInput->kernelWidth, m_binInput->kernelHeight,
                m_paddedWidth, m_paddedHeight
            );
        }

        // Input padding + threshold
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_inputPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_inputPadded[i].getVector()) v = 0;
            }

            float threshold = m_binInput->cp_convThreshold;
            float v, mul;
            float inpColor[3];
            int transX, transY;
            uint32_t redIndex;
            for (int y = 0; y < m_binInput->inputHeight; y++)
            {
                for (int x = 0; x < m_binInput->inputWidth; x++)
                {
                    redIndex = (y * m_binInput->inputWidth + x) * 4;
                    inpColor[0] = m_binInput->inputBuffer[redIndex + 0];
                    inpColor[1] = m_binInput->inputBuffer[redIndex + 1];
                    inpColor[2] = m_binInput->inputBuffer[redIndex + 2];

                    v = rgbToGrayscale(inpColor[0], inpColor[1], inpColor[2]);
                    if (v > threshold)
                    {
                        // Smooth Transition
                        mul = softThreshold(v, threshold, m_binInput->cp_convKnee);

                        transX = x + m_inputLeftPadding;
                        transY = y + m_inputTopPadding;

                        m_inputPadded[0](transY, transX) = inpColor[0] * mul;
                        m_inputPadded[1](transY, transX) = inpColor[1] * mul;
                        m_inputPadded[2](transY, transX) = inpColor[2] * mul;
                    }
                }
            }
        }

        // Kernel padding
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                m_kernelPadded[i].resize(m_paddedHeight, m_paddedWidth);
                for (auto& v : m_kernelPadded[i].getVector()) v = 0;
            }

            int transX, transY;
            uint32_t redIndex;
            for (int y = 0; y < m_binInput->kernelHeight; y++)
            {
                for (int x = 0; x < m_binInput->kernelWidth; x++)
                {
                    redIndex = (y * m_binInput->kernelWidth + x) * 4;
                    transX = x + m_kernelLeftPadding;
                    transY = y + m_kernelTopPadding;
                    m_kernelPadded[0](transY, transX) = m_binInput->kernelBuffer[redIndex + 0];
                    m_kernelPadded[1](transY, transX) = m_binInput->kernelBuffer[redIndex + 1];
                    m_kernelPadded[2](transY, transX) = m_binInput->kernelBuffer[redIndex + 2];
                }
            }
        }
    }

    void ConvFftGpu::inputFFT(uint32_t ch)
    {
        GLFFT::FFTOptions options; // Default initializes to something conservative in terms of performance.
        options.type.fp16 = false; // Use mediump float (if GLES) in shaders.
        options.type.input_fp16 = false; // Use FP32 input.
        options.type.output_fp16 = false; // Use FP16 output.
        options.type.normalize = true; // Normalized FFT.

        GLFFT::FFT fft(
            m_context.get(),
            m_paddedWidth,
            m_paddedHeight,
            GLFFT::RealToComplex,
            GLFFT::Forward,
            GLFFT::SSBO,
            GLFFT::SSBO,
            m_programCache,
            options);

        size_t inputSizeBytes = m_paddedWidth * m_paddedHeight * sizeof(float);
        size_t outputSizeBytes = m_paddedWidth * m_paddedHeight * sizeof(float) * 2;

        m_inputPaddedSSBO[ch] = m_context->create_buffer(m_inputPadded[ch].getVector().data(), inputSizeBytes, GLFFT::AccessStreamCopy);
        checkGlStatus(__FUNCTION__, "Input buffer");

        m_inputFtSSBO[ch] = m_context->create_buffer(nullptr, outputSizeBytes, GLFFT::AccessStreamRead);
        checkGlStatus(__FUNCTION__, "Output buffer");

        // Do the FFT
        GLFFT::CommandBuffer* cmd = m_context->request_command_buffer();
        fft.process(cmd, m_inputFtSSBO[ch].get(), m_inputPaddedSSBO[ch].get(), m_inputPaddedSSBO[ch].get());
        cmd->barrier();
        m_context->submit_command_buffer(cmd);
        m_context->wait_idle();
        checkGlStatus(__FUNCTION__, "Process");
    }

    void ConvFftGpu::kernelFFT(uint32_t ch)
    {
        GLFFT::FFTOptions options; // Default initializes to something conservative in terms of performance.
        options.type.fp16 = false; // Use mediump float (if GLES) in shaders.
        options.type.input_fp16 = false; // Use FP32 input.
        options.type.output_fp16 = false; // Use FP16 output.
        options.type.normalize = false; // Normalized FFT.

        GLFFT::FFT fft(
            m_context.get(),
            m_paddedWidth,
            m_paddedHeight,
            GLFFT::RealToComplex,
            GLFFT::Forward,
            GLFFT::SSBO,
            GLFFT::SSBO,
            m_programCache,
            options);

        size_t inputSizeBytes = m_paddedWidth * m_paddedHeight * sizeof(float);
        size_t outputSizeBytes = m_paddedWidth * m_paddedHeight * sizeof(float) * 2;

        m_kernelPaddedSSBO[ch] = m_context->create_buffer(m_kernelPadded[ch].getVector().data(), inputSizeBytes, GLFFT::AccessStreamCopy);
        checkGlStatus(__FUNCTION__, "Input buffer");

        m_kernelFtSSBO[ch] = m_context->create_buffer(nullptr, outputSizeBytes, GLFFT::AccessStreamRead);
        checkGlStatus(__FUNCTION__, "Output buffer");

        // Do the FFT
        GLFFT::CommandBuffer* cmd = m_context->request_command_buffer();
        fft.process(cmd, m_kernelFtSSBO[ch].get(), m_kernelPaddedSSBO[ch].get(), m_kernelPaddedSSBO[ch].get());
        cmd->barrier();
        m_context->submit_command_buffer(cmd);
        m_context->wait_idle();
        checkGlStatus(__FUNCTION__, "Process");
    }

    void ConvFftGpu::inverseConvolve(uint32_t ch)
    {
        GLFFT::FFTOptions options; // Default initializes to something conservative in terms of performance.
        options.type.fp16 = false; // Use mediump float (if GLES) in shaders.
        options.type.input_fp16 = false; // Use FP32 input.
        options.type.output_fp16 = false; // Use FP16 output.
        options.type.normalize = false; // Normalized FFT.

        GLFFT::FFT fft(
            m_context.get(),
            m_paddedWidth,
            m_paddedHeight,
            GLFFT::ComplexToReal,
            GLFFT::InverseConvolve,
            GLFFT::SSBO,
            GLFFT::SSBO,
            m_programCache,
            options);

        size_t outputSizeBytes = m_paddedWidth * m_paddedHeight * sizeof(float);
        m_iFftSSBO[ch] = m_context->create_buffer(nullptr, outputSizeBytes, GLFFT::AccessStreamRead);
        checkGlStatus(__FUNCTION__, "Output buffer");

        // Do the FFT
        GLFFT::CommandBuffer* cmd = m_context->request_command_buffer();
        fft.process(cmd, m_iFftSSBO[ch].get(), m_inputFtSSBO[ch].get(), m_kernelFtSSBO[ch].get());
        cmd->barrier();
        m_context->submit_command_buffer(cmd);
        m_context->wait_idle();
        checkGlStatus(__FUNCTION__, "Process");

        // Read the result back onto the CPU
        {
            // Prepare
            m_iFFT[ch].resize(m_paddedHeight, m_paddedWidth);

            // Map
            const void* data = m_context->map(m_iFftSSBO[ch].get(), 0, outputSizeBytes);
            checkGlStatus(__FUNCTION__, "Readback (map)");

            if (data == nullptr)
                throw std::exception(printErr(__FUNCTION__, "Readback", "data was null.").c_str());

            // Copy
            memcpy(m_iFFT[ch].getVector().data(), data, outputSizeBytes);

            // Unmap
            m_context->unmap(m_iFftSSBO[ch].get());
            checkGlStatus(__FUNCTION__, "Readback (unmap)");
        }

        // Clean up
        m_inputPaddedSSBO[ch] = nullptr;
        m_inputFtSSBO[ch] = nullptr;
        m_kernelPaddedSSBO[ch] = nullptr;
        m_kernelFtSSBO[ch] = nullptr;
        m_iFftSSBO[ch] = nullptr;
        checkGlStatus(__FUNCTION__, "Cleanup");
    }

    void ConvFftGpu::output()
    {
        // Prepare output buffer
        uint32_t outputSize = m_binInput->inputWidth * m_binInput->inputHeight * 4;
        m_outputBuffer.resize(outputSize);

        // Crop the iFFT output and apply convolution multiplier
        int transX1 = 0, transY1 = 0;
        int transX2 = 0, transY2 = 0;
        uint32_t redIndex;
        for (uint32_t y = 0; y < m_binInput->inputHeight; y++)
        {
            for (uint32_t x = 0; x < m_binInput->inputWidth; x++)
            {
                transX1 = x + m_inputLeftPadding;
                transY1 = y + m_inputTopPadding;

                transX2 = transX1;
                transY2 = transY1;

                // Fix the coordinates
                {
                    if (transX1 < (m_paddedWidth / 2))
                        transX2 = transX1 + (m_paddedWidth / 2);
                    else
                        transX2 = transX1 - (m_paddedWidth / 2);
                
                    if (transY1 < (m_paddedHeight / 2))
                        transY2 = transY1 + (m_paddedHeight / 2);
                    else
                        transY2 = transY1 - (m_paddedHeight / 2);
                }

                // Get the output
                redIndex = (y * m_binInput->inputWidth + x) * 4;
                m_outputBuffer[redIndex + 0] = fmaxf(m_iFFT[0](transY2, transX2) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 1] = fmaxf(m_iFFT[1](transY2, transX2) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 2] = fmaxf(m_iFFT[2](transY2, transX2) * m_binInput->convMultiplier, 0.0f);
                m_outputBuffer[redIndex + 3] = 1.0f;
            }
        }

        for (uint32_t i = 0; i < 3; i++)
            m_iFFT[i].reset();
    }

    const std::vector<float>& ConvFftGpu::getBuffer() const
    {
        return m_outputBuffer;
    }

}
