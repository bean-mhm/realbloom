#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

#include "../ColorManagement/CMImage.h"
#include "../Utils/NumberHelpers.h"
#include "../Utils/Bilinear.h"
#include "../Utils/RandomNumber.h"
#include "../Utils/Misc.h"
#include "../Async.h"

#include "ConvolutionGPUBinary.h"
#include <filesystem>
#include <iostream>
#include <fstream>

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
        bool kernelNormalize = true;
        float kernelExposure = 0.0f;
        float kernelContrast = 0.0f;
        float kernelRotation = 0.0f;
        float kernelScaleX = 1.0f;
        float kernelScaleY = 1.0f;
        float kernelCropX = 1.0f;
        float kernelCropY = 1.0f;
        bool  kernelPreviewCenter = true;
        float kernelCenterX = 0.5f;
        float kernelCenterY = 0.5f;
        float convThreshold = 0.5f;
        float convKnee = 0.2f;
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
    };

    class ConvolutionThread;

    class Convolution
    {
    private:
        ConvolutionState m_state;
        ConvolutionParams m_params;

        CmImage* m_imageInput = nullptr;
        CmImage* m_imageKernel = nullptr;
        CmImage* m_imageKernelPreview = nullptr;
        CmImage* m_imageConvPreview = nullptr;
        CmImage* m_imageConvLayer = nullptr;
        CmImage* m_imageConvMix = nullptr;

        std::thread* m_thread = nullptr;
        std::vector<ConvolutionThread*> m_threads;

        void setErrorState(const std::string& error);

    public:
        Convolution();
        ConvolutionParams* getParams();

        void setInputImage(CmImage* image);
        void setKernelImage(CmImage* image);
        void setKernelPreviewImage(CmImage* image);
        void setConvPreviewImage(CmImage* image);
        void setConvLayerImage(CmImage* image);
        void setConvMixImage(CmImage* image);

        void previewThreshold(size_t* outNumPixels = nullptr);
        void kernel(bool previewMode = true, float** outBuffer = nullptr, uint32_t* outWidth = nullptr, uint32_t* outHeight = nullptr);
        void mixConv(bool additive, float inputMix, float convMix, float mix, float convExposure);

        void convolve();
        void cancelConv();

        bool isWorking();

        // outStatType: 0 = normal, 1 = info, 2 = warning, 3 = error
        void getConvStats(std::string& outTime, std::string& outStatus, uint32_t& outStatType);
        std::string getResourceInfo();
    };

}
