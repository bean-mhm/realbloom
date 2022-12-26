#pragma once

#include <vector>
#include <string>
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

#include "../ColorManagement/CmImage.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Bilinear.h"
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
        NAIVE_CPU = 1,
        NAIVE_GPU = 2
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
        float kernelExposure = 0.0f;
        float kernelContrast = 0.0f;
        std::array<float, 3> kernelColor{ 1, 1, 1 };
        float kernelRotation = 0.0f;
        float kernelScaleX = 1.0f;
        float kernelScaleY = 1.0f;
        float kernelCropX = 1.0f;
        float kernelCropY = 1.0f;
        bool  kernelPreviewCenter = true;
        float kernelCenterX = 0.5f;
        float kernelCenterY = 0.5f;
        float convThreshold = 0.0f;
        float convKnee = 0.0f;
        bool  kernelAutoExposure = true;
    };

    struct ConvolutionState
    {
        bool working = false;
        bool mustCancel = false;

        bool failed = false;
        std::string error = "";

        std::chrono::time_point<std::chrono::system_clock> timeStart;
        std::chrono::time_point<std::chrono::system_clock> timeEnd;
        bool hasTimestamps = false;

        ConvolutionMethodInfo methodInfo;
        uint32_t numChunksDone = 0;
        std::string fftStage = "";

        void setError(std::string err);
    };

    class ConvolutionThread;

    class Convolution
    {
    private:
        ConvolutionState m_state;
        ConvolutionParams m_params;

        CmImage* m_imgInput = nullptr;
        CmImage* m_imgKernel = nullptr;
        CmImage* m_imgConvPreview = nullptr;
        CmImage* m_imgConvMix = nullptr;

        CmImage m_imgKernelSrc;
        CmImage m_imgOutput;

        std::thread* m_thread = nullptr;
        std::vector<ConvolutionThread*> m_threads;

    public:
        Convolution();
        ConvolutionParams* getParams();

        void setImgInput(CmImage* image);
        void setImgKernel(CmImage* image);
        void setImgConvPreview(CmImage* image);
        void setImgConvMix(CmImage* image);

        CmImage* getImgKernelSrc();

        void previewThreshold(size_t* outNumPixels = nullptr);
        void kernel(bool previewMode = true, std::vector<float>* outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void mixConv(bool additive, float inputMix, float convMix, float mix, float convExposure);
        void convolve();
        void cancelConv();

        bool isWorking() const;
        bool hasFailed() const;
        std::string getError() const;

        // outStatType: 0 = normal, 1 = info, 2 = warning, 3 = error
        void getConvStats(std::string& outTime, std::string& outStatus, uint32_t& outStatType);
        std::string getResourceInfo();

    private:
        void convFftCPU(
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
