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

    constexpr float CONV_MULTIPLIER = 1.0 / 1024.0;
    constexpr int CONV_MAX_CHUNKS = 2048;
    constexpr int CONV_MAX_SLEEP = 5000;

    enum class ConvolutionMethod
    {
        FFT_CPU = 0,
        FFT_GPU = 1,
        NAIVE_CPU = 2,
        NAIVE_GPU = 3
    };

    struct ConvolutionMethodInfo
    {
        ConvolutionMethod method = ConvolutionMethod::FFT_CPU;
        uint32_t numThreads = 1;
        uint32_t numChunks = 1;
        uint32_t chunkSleep = 0;
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
        CmImage* m_imgConvMix = nullptr;

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
        void setImgConvMix(CmImage* image);

        void previewThreshold(size_t* outNumPixels = nullptr);
        void previewInput(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void previewKernel(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void mix(bool additive, float inputMix, float convMix, float mix, float convExposure);
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
