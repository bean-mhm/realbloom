#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <atomic>

#include "GpuHelper.h"
#include "Binary/BinaryData.h"
#include "Binary/BinaryConvNaiveGpu.h"
#include "Binary/BinaryConvFftGpu.h"

#include "ModuleHelpers.h"

#include "../ColorManagement/CmImage.h"

#include "../Utils/ImageTransform.h"
#include "../Utils/Bilinear.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Status.h"
#include "../Utils/Misc.h"

#include "../Async.h"

namespace RealBloom
{

    constexpr float CONV_MULTIPLIER = 1.0f;
    constexpr uint32_t CONV_NAIVE_GPU_MAX_CHUNKS = 2048;
    constexpr uint32_t CONV_NAIVE_GPU_MAX_SLEEP = 5000;

    enum class ConvolutionMethod
    {
        FFT_CPU,
        FFT_GPU,
        NAIVE_CPU,
        NAIVE_GPU
    };
    constexpr uint32_t ConvolutionMethod_EnumSize = 4;

    struct ConvolutionMethodInfo
    {
        ConvolutionMethod method = ConvolutionMethod::FFT_CPU;
        bool FFT_CPU_deconvolve = false;
        uint32_t NAIVE_CPU_numThreads = getDefNumThreads();
        uint32_t NAIVE_GPU_numChunks = 10;
        uint32_t NAIVE_GPU_chunkSleep = 0;
    };

    struct ConvolutionParams
    {
        ConvolutionMethodInfo methodInfo;
        ImageTransformParams inputTransformParams;
        ImageTransformParams kernelTransformParams;
        bool useKernelTransformOrigin = true;
        float threshold = 0.0f;
        float knee = 0.0f;
        bool  autoExposure = true;
        bool blendAdditive = false;
        float blendInput = 1.0f;
        float blendConv = 0.2f;
        float blendMix = 0.2f;
        float blendExposure = 0.0f;
    };

    struct ConvolutionStatus : public TimedWorkingStatus
    {
    private:
        typedef TimedWorkingStatus super;

    private:
        uint32_t m_numChunksDone = 0;
        std::string m_fftStage = "";

    public:
        ConvolutionStatus() {};
        ~ConvolutionStatus() {};

        uint32_t getNumChunksDone() const;
        void setNumChunksDone(uint32_t numChunksDone);

        const std::string& getFftStage() const;
        void setFftStage(const std::string& fftStage);

        virtual void reset() override;

    };

    class ConvolutionThread;

    class Convolution
    {
    private:
        ConvolutionStatus m_status;
        ConvolutionParams m_params;
        ConvolutionParams m_capturedParams;

        CmImage m_imgInputSrc;
        CmImage* m_imgInput = nullptr;

        CmImage m_imgKernelSrc;
        CmImage* m_imgKernel = nullptr;

        CmImage* m_imgConvPreview = nullptr;
        CmImage* m_imgConvResult = nullptr;

        CmImage m_imgInputCaptured;
        CmImage m_imgOutput;

        std::shared_ptr<std::jthread> m_thread = nullptr;
        std::vector<std::shared_ptr<ConvolutionThread>> m_cpuThreads;

    public:
        Convolution();
        ConvolutionParams* getParams();

        CmImage* getImgInputSrc();
        void setImgInput(CmImage* image);

        CmImage* getImgKernelSrc();
        void setImgKernel(CmImage* image);

        void setImgConvPreview(CmImage* image);
        void setImgConvResult(CmImage* image);

        void previewThreshold(size_t* outNumPixels = nullptr);
        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void previewKernel(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void blend();
        void convolve();
        void cancel();

        const ConvolutionStatus& getStatus() const;

        // outMessageType: 0 = normal, 1 = info, 2 = warning, 3 = error
        void getStatusText(std::string& outStatus, std::string& outMessage, uint32_t& outMessageType) const;

        std::string getResourceInfo();

        static std::array<float, 2> getKernelOrigin(const ConvolutionParams& params);

    private:
        void convFftCPU(
            std::vector<float>& kernelBuffer,
            uint32_t kernelWidth,
            uint32_t kernelHeight,
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize);

        void convFftGPU(
            std::vector<float>& kernelBuffer,
            uint32_t kernelWidth,
            uint32_t kernelHeight,
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize);

        void convNaiveCPU(
            std::vector<float>& kernelBuffer,
            uint32_t kernelWidth,
            uint32_t kernelHeight,
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize);

        void convNaiveGPU(
            std::vector<float>& kernelBuffer,
            uint32_t kernelWidth,
            uint32_t kernelHeight,
            std::vector<float>& inputBuffer,
            uint32_t inputWidth,
            uint32_t inputHeight,
            uint32_t inputBufferSize);

    };

}
