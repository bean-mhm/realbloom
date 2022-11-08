#pragma once

#include <vector>
#include <string>
#include <format>
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
    constexpr float CONV_MULTIPLIER = 0.001f;
    constexpr int CONV_MAX_CHUNKS = 2048;
    constexpr int CONV_MAX_SLEEP = 5000;

    enum class ConvolutionDeviceType
    {
        CPU,
        GPU
    };

    struct ConvolutionDevice
    {
        ConvolutionDeviceType deviceType = ConvolutionDeviceType::CPU;
        uint32_t numThreads = 1;
        uint32_t numChunks = 1;
        uint32_t chunkSleep = 0;
    };

    struct ConvolutionParams
    {
        ConvolutionDevice device;
        float kernelRotation = 0.0f;
        float kernelScaleW = 1.0f;
        float kernelScaleH = 1.0f;
        float kernelCropW = 1.0f;
        float kernelCropH = 1.0f;
        float kernelCenterX = 0.5f;
        float kernelCenterY = 0.5f;
        bool  kernelPreviewCenter = true;
        float kernelContrast = 0.0f;
        float kernelIntensity = 1.0f;
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

        ConvolutionDevice device;
        uint32_t numChunksDone = 0;
    };

    class ConvolutionThread;

    class Convolution
    {
    private:
        ConvolutionState m_state;
        ConvolutionParams m_params;
        CmImage* m_imageInput;
        CmImage* m_imageKernel;
        CmImage* m_imageKernelPreview;
        CmImage* m_imageConvPreview;
        CmImage* m_imageConvLayer;
        CmImage* m_imageConvMix;

        std::thread* m_thread;
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
        void mixConv(bool additive, float inputMix, float convMix, float mix, float convIntensity);

        void convolve();
        void cancelConv();

        bool isWorking();

        // outStatType: 0 = normal, 1 = info, 2 = warning, 3 = error
        void getConvStats(std::string& outTime, std::string& outStatus, uint32_t& outStatType);
        std::string getResourceInfo();
    };

}