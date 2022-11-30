#include "Convolution.h"
#include "ConvolutionThread.h"
#include "ConvolutionFFT.h"

namespace RealBloom
{

    constexpr uint32_t CONV_WAIT_TIMESTEP = 50;
    constexpr uint32_t CONV_PROG_TIMESTEP = 1000;

    constexpr uint32_t CONVGPU_FILE_TIMEOUT = 5000;
    constexpr bool CONVGPU_DELETE_TEMP = true;

    constexpr const char* S_CANCELED_BY_USER = "Canceled by user.";

    void drawRect(CmImage* image, int rx, int ry, int rw, int rh);
    void fillRect(CmImage* image, int rx, int ry, int rw, int rh);
    std::string strFromColorChannelID(uint32_t ch);

    void Convolution::setErrorState(const std::string& error)
    {
        m_state.error = error;
        m_state.failed = true;
    }

    Convolution::Convolution() : m_imgKernelSrc("", "")
    {}

    ConvolutionParams* Convolution::getParams()
    {
        return &m_params;
    }

    void Convolution::setImgInput(CmImage* image)
    {
        m_imgInput = image;
    }

    void Convolution::setImgKernel(CmImage* image)
    {
        m_imgKernel = image;
    }

    void Convolution::setImgConvPreview(CmImage* image)
    {
        m_imgConvPreview = image;
    }

    void Convolution::setImgConvLayer(CmImage* image)
    {
        m_imgConvLayer = image;
    }

    void Convolution::setImgConvMix(CmImage* image)
    {
        m_imgConvMix = image;
    }

    CmImage* Convolution::getImgKernelSrc()
    {
        return &m_imgKernelSrc;
    }

    void Convolution::previewThreshold(size_t* outNumPixels)
    {
        float threshold = m_params.convThreshold;
        size_t numPixels = 0;
        {
            // Input image
            std::lock_guard<CmImage> lock1(*m_imgInput);
            float* inputBuffer = m_imgInput->getImageData();
            uint32_t inputWidth = m_imgInput->getWidth();
            uint32_t inputHeight = m_imgInput->getHeight();

            // Convolution Preview Image
            std::lock_guard<CmImage> lock2(*m_imgConvPreview);
            m_imgConvPreview->resize(inputWidth, inputHeight, false);
            float* prevBuffer = m_imgConvPreview->getImageData();

            float v;
            uint32_t redIndex;
            float mul;
            for (uint32_t y = 0; y < inputHeight; y++)
            {
                for (uint32_t x = 0; x < inputWidth; x++)
                {
                    redIndex = (y * inputWidth + x) * 4;
                    prevBuffer[redIndex + 0] = 0;
                    prevBuffer[redIndex + 1] = 0;
                    prevBuffer[redIndex + 2] = 0;
                    prevBuffer[redIndex + 3] = 1;

                    v = rgbToGrayscale(inputBuffer[redIndex + 0], inputBuffer[redIndex + 1], inputBuffer[redIndex + 2]);
                    if (v > threshold)
                    {
                        numPixels++;

                        mul = softThreshold(v, threshold, m_params.convKnee);
                        blendAddRGB(prevBuffer, redIndex, inputBuffer, redIndex, mul);
                    }
                }
            }
        }
        m_imgConvPreview->moveToGPU();

        if (outNumPixels)
            *outNumPixels = numPixels;
    }

    void Convolution::kernel(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        std::vector<float> kernelBuffer;
        uint32_t kernelBufferSize = 0;
        uint32_t kernelWidth = 0, kernelHeight = 0;
        {
            std::lock_guard<CmImage> lock(m_imgKernelSrc);
            float* kernelSrcBuffer = m_imgKernelSrc.getImageData();
            kernelBufferSize = m_imgKernelSrc.getImageDataSize();
            kernelWidth = m_imgKernelSrc.getWidth();
            kernelHeight = m_imgKernelSrc.getHeight();

            kernelBuffer.resize(kernelBufferSize);
            std::copy(kernelSrcBuffer, kernelSrcBuffer + kernelBufferSize, kernelBuffer.data());
        }

        float centerX, centerY;
        centerX = (float)kernelWidth / 2.0f;
        centerY = (float)kernelHeight / 2.0f;

        float scaleW = fmaxf(m_params.kernelScaleX, 0.01f);
        float scaleH = fmaxf(m_params.kernelScaleY, 0.01f);
        float cropW = fminf(fmaxf(m_params.kernelCropX, 0.1f), 1.0f);
        float cropH = fminf(fmaxf(m_params.kernelCropY, 0.1f), 1.0f);
        float rotation = m_params.kernelRotation;

        float kernelExpMul = applyExposure(m_params.kernelExposure);
        float kernelContrast = m_params.kernelContrast;

        uint32_t scaledWidth, scaledHeight;
        scaledWidth = (uint32_t)floorf(scaleW * (float)kernelWidth);
        scaledHeight = (uint32_t)floorf(scaleH * (float)kernelHeight);

        uint32_t croppedWidth, croppedHeight;
        croppedWidth = (uint32_t)floorf(cropW * (float)scaledWidth);
        croppedHeight = (uint32_t)floorf(cropH * (float)scaledHeight);

        float scaledCenterX, scaledCenterY;
        scaledCenterX = (scaleW * (float)kernelWidth) / 2.0f;
        scaledCenterY = (scaleH * (float)kernelHeight) / 2.0f;

        if (!previewMode && (outBuffer == nullptr))
        {
            *outWidth = croppedWidth;
            *outHeight = croppedHeight;
            return;
        }

        // Normalize, apply contrast and exposure
        {
            // Get the brightest value
            float v = 0;
            float maxV = EPSILON;
            for (uint32_t i = 0; i < kernelBufferSize; i++)
            {
                v = fmaxf(0.0f, kernelBuffer[i]);
                if ((v > maxV) && (i % 4 != 3)) // if not alpha channel
                    maxV = v;
            }

            uint32_t redIndex;
            for (uint32_t y = 0; y < kernelHeight; y++)
            {
                for (uint32_t x = 0; x < kernelWidth; x++)
                {
                    redIndex = (y * kernelWidth + x) * 4;

                    // Normalize
                    kernelBuffer[redIndex + 0] /= maxV;
                    kernelBuffer[redIndex + 1] /= maxV;
                    kernelBuffer[redIndex + 2] /= maxV;

                    // Calculate contrast for the grayscale value
                    float grayscale = rgbToGrayscale(kernelBuffer[redIndex + 0], kernelBuffer[redIndex + 1], kernelBuffer[redIndex + 2]);
                    float mul = kernelExpMul * (contrastCurve(grayscale, kernelContrast) / fmaxf(grayscale, EPSILON));

                    // Apply contrast
                    kernelBuffer[redIndex + 0] *= mul;
                    kernelBuffer[redIndex + 1] *= mul;
                    kernelBuffer[redIndex + 2] *= mul;

                    // De-normalize if needed
                    if (!m_params.kernelNormalize)
                    {
                        kernelBuffer[redIndex + 0] *= maxV;
                        kernelBuffer[redIndex + 1] *= maxV;
                        kernelBuffer[redIndex + 2] *= maxV;
                    }
                }
            }
        }

        // Scale and Rotation
        std::vector<float> scaledBuffer;
        uint32_t scaledBufferSize = 0;
        if ((scaleW == 1) && (scaleH == 1) && (rotation == 0))
        {
            scaledBufferSize = kernelBufferSize;
            scaledBuffer.resize(scaledBufferSize);
            std::copy(kernelBuffer.data(), kernelBuffer.data() + kernelBufferSize, scaledBuffer.data());
        }
        else
        {
            scaledBufferSize = scaledWidth * scaledHeight * 4;
            scaledBuffer.resize(scaledBufferSize);

            uint32_t redIndexKernel, redIndexScaled;
            float transX, transY;
            float transX_rot = 0, transY_rot = 0;
            float targetColor[3];
            Bilinear::Result bil;
            for (uint32_t y = 0; y < scaledHeight; y++)
            {
                for (uint32_t x = 0; x < scaledWidth; x++)
                {
                    transX = ((float)x + 0.5f) / scaleW;
                    transY = ((float)y + 0.5f) / scaleH;
                    rotatePoint(transX, transY, centerX, centerY, -rotation, transX_rot, transY_rot);
                    Bilinear::calculate(transX_rot, transY_rot, bil);

                    // CURRENT COLOR = (KERNEL[TOPLEFT] * TOPLEFTMIX) + (KERNEL[TOPRIGHT] * TOPRIGHTMIX) + ...

                    targetColor[0] = 0;
                    targetColor[1] = 0;
                    targetColor[2] = 0;

                    if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], kernelWidth, kernelHeight))
                    {
                        redIndexKernel = (bil.topLeftPos[1] * kernelWidth + bil.topLeftPos[0]) * 4;
                        blendAddRGB(targetColor, 0, kernelBuffer.data(), redIndexKernel, bil.topLeftWeight);
                    }
                    if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], kernelWidth, kernelHeight))
                    {
                        redIndexKernel = (bil.topRightPos[1] * kernelWidth + bil.topRightPos[0]) * 4;
                        blendAddRGB(targetColor, 0, kernelBuffer.data(), redIndexKernel, bil.topRightWeight);
                    }
                    if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], kernelWidth, kernelHeight))
                    {
                        redIndexKernel = (bil.bottomLeftPos[1] * kernelWidth + bil.bottomLeftPos[0]) * 4;
                        blendAddRGB(targetColor, 0, kernelBuffer.data(), redIndexKernel, bil.bottomLeftWeight);
                    }
                    if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], kernelWidth, kernelHeight))
                    {
                        redIndexKernel = (bil.bottomRightPos[1] * kernelWidth + bil.bottomRightPos[0]) * 4;
                        blendAddRGB(targetColor, 0, kernelBuffer.data(), redIndexKernel, bil.bottomRightWeight);
                    }

                    redIndexScaled = (y * scaledWidth + x) * 4;
                    scaledBuffer[redIndexScaled + 0] = targetColor[0];
                    scaledBuffer[redIndexScaled + 1] = targetColor[1];
                    scaledBuffer[redIndexScaled + 2] = targetColor[2];
                    scaledBuffer[redIndexScaled + 3] = 1.0f;
                }
            }
        }

        // No longer need the original buffer
        clearVector(kernelBuffer);

        // Crop
        std::vector<float> croppedBuffer;
        uint32_t croppedBufferSize = 0;
        if ((cropW == 1) && (cropH == 1))
        {
            croppedBufferSize = scaledBufferSize;
            croppedBuffer.resize(croppedBufferSize);
            std::copy(scaledBuffer.data(), scaledBuffer.data() + scaledBufferSize, croppedBuffer.data());
        }
        else
        {
            croppedBufferSize = croppedWidth * croppedHeight * 4;
            croppedBuffer.resize(croppedBufferSize);

            float cropMaxOffsetX = scaledWidth - croppedWidth;
            float cropMaxOffsetY = scaledHeight - croppedHeight;

            uint32_t cropStartX = (uint32_t)floorf(m_params.kernelCenterX * cropMaxOffsetX);
            uint32_t cropStartY = (uint32_t)floorf(m_params.kernelCenterY * cropMaxOffsetY);

            uint32_t redIndexCropped, redIndexScaled;
            for (uint32_t y = 0; y < croppedHeight; y++)
            {
                for (uint32_t x = 0; x < croppedWidth; x++)
                {
                    redIndexCropped = (y * croppedWidth + x) * 4;
                    redIndexScaled = 4 * (((y + cropStartY) * scaledWidth) + x + cropStartX);

                    croppedBuffer[redIndexCropped + 0] = scaledBuffer[redIndexScaled + 0];
                    croppedBuffer[redIndexCropped + 1] = scaledBuffer[redIndexScaled + 1];
                    croppedBuffer[redIndexCropped + 2] = scaledBuffer[redIndexScaled + 2];
                    croppedBuffer[redIndexCropped + 3] = scaledBuffer[redIndexScaled + 3];
                }
            }
        }

        clearVector(scaledBuffer);

        // Copy to outBuffer if it's requested by another function
        if (!previewMode && (outBuffer != nullptr))
        {
            *outWidth = croppedWidth;
            *outHeight = croppedHeight;
            outBuffer->resize(croppedBufferSize);
            *outBuffer = croppedBuffer;
        }

        // Copy to kernel preview image
        {
            std::lock_guard<CmImage> lock(*m_imgKernel);
            m_imgKernel->resize(croppedWidth, croppedHeight, false);
            float* prevBuffer = m_imgKernel->getImageData();
            std::copy(croppedBuffer.data(), croppedBuffer.data() + croppedBufferSize, prevBuffer);
        }

        // Preview center point
        if (m_params.kernelPreviewCenter)
        {
            int newCenterX = (int)floorf(m_params.kernelCenterX * (float)croppedWidth - 0.5f);
            int newCenterY = (int)floorf(m_params.kernelCenterY * (float)croppedHeight - 0.5f);
            int prevSquareSize = (int)roundf(0.15f * fminf((float)croppedWidth, (float)croppedHeight));

            drawRect(
                m_imgKernel,
                newCenterX - (prevSquareSize / 2),
                newCenterY - (prevSquareSize / 2),
                prevSquareSize, prevSquareSize);

            fillRect(m_imgKernel, newCenterX - 2, newCenterY - 2, 4, 4);
        }

        m_imgKernel->moveToGPU();
    }

    void Convolution::mixConv(bool additive, float inputMix, float convMix, float mix, float convExposure)
    {
        inputMix = fmaxf(inputMix, 0.0f);
        convMix = fmaxf(convMix, 0.0f);
        if (!additive)
        {
            mix = fminf(fmaxf(mix, 0.0f), 1.0f);
            convMix = mix;
            inputMix = 1.0f - mix;
        }

        {
            // Input buffer
            std::lock_guard<CmImage> inputImageLock(*m_imgInput);
            float* inputBuffer = m_imgInput->getImageData();
            uint32_t inputWidth = m_imgInput->getWidth();
            uint32_t inputHeight = m_imgInput->getHeight();

            // Conv Buffer
            std::lock_guard<CmImage> convLayerImageLock(*m_imgConvLayer);
            float* convBuffer = m_imgConvLayer->getImageData();
            uint32_t convWidth = m_imgConvLayer->getWidth();
            uint32_t convHeight = m_imgConvLayer->getHeight();

            if ((inputWidth == convWidth) && (inputHeight == convHeight))
            {
                // Conv Mix Buffer
                std::lock_guard<CmImage> convMixImageLock(*m_imgConvMix);
                m_imgConvMix->resize(inputWidth, inputHeight, false);
                float* convMixBuffer = m_imgConvMix->getImageData();

                float multiplier = convMix * applyExposure(convExposure);
                uint32_t redIndex;
                for (uint32_t y = 0; y < inputHeight; y++)
                {
                    for (uint32_t x = 0; x < inputWidth; x++)
                    {
                        redIndex = (y * inputWidth + x) * 4;

                        convMixBuffer[redIndex + 0] =
                            (inputBuffer[redIndex + 0] * inputMix) + (convBuffer[redIndex + 0] * multiplier);
                        convMixBuffer[redIndex + 1] =
                            (inputBuffer[redIndex + 1] * inputMix) + (convBuffer[redIndex + 1] * multiplier);
                        convMixBuffer[redIndex + 2] =
                            (inputBuffer[redIndex + 2] * inputMix) + (convBuffer[redIndex + 2] * multiplier);

                        convMixBuffer[redIndex + 3] = 1;
                    }
                }
            }
        }
        m_imgConvMix->moveToGPU();
    }

    void Convolution::convolve()
    {
        // If there's already a convolution process going on
        cancelConv();

        // Clamp the number of threads
        uint32_t maxThreads = std::thread::hardware_concurrency();
        uint32_t numThreads = m_params.methodInfo.numThreads;
        if (numThreads > maxThreads) numThreads = maxThreads;
        if (numThreads < 1) numThreads = 1;

        // Same for chunks
        uint32_t numChunks = m_params.methodInfo.numChunks;
        if (numChunks < 1) numChunks = 1;
        if (numChunks > CONV_MAX_CHUNKS) numChunks = CONV_MAX_CHUNKS;

        uint32_t chunkSleep = m_params.methodInfo.chunkSleep;
        if (chunkSleep > CONV_MAX_SLEEP) chunkSleep = CONV_MAX_SLEEP;

        // Reset state
        m_state.working = true;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.error = "";
        m_state.timeStart = std::chrono::system_clock::now();
        m_state.hasTimestamps = false;
        m_state.methodInfo = m_params.methodInfo;
        m_state.numChunksDone = 0;
        m_state.fftStage = "";

        // Start the main thread
        m_thread = new std::thread([this, numThreads, maxThreads, numChunks, chunkSleep]()
            {
                // Kernel Buffer
                std::vector<float> kernelBuffer;
                uint32_t kernelWidth = 0, kernelHeight = 0;
                kernel(false, &kernelBuffer, &kernelWidth, &kernelHeight);

                // Input Buffer
                std::vector<float> inputBuffer;
                uint32_t inputWidth = 0, inputHeight = 0;
                uint32_t inputBufferSize = 0;
                {
                    // Input buffer from input image
                    std::lock_guard<CmImage> lock(*m_imgInput);
                    inputWidth = m_imgInput->getWidth();
                    inputHeight = m_imgInput->getHeight();
                    inputBufferSize = m_imgInput->getImageDataSize();
                    float* inputImageData = m_imgInput->getImageData();

                    // Copy the buffer so the input image doesn't stay locked throughout the process
                    inputBuffer.resize(inputBufferSize);
                    std::copy(inputImageData, inputImageData + inputBufferSize, inputBuffer.data());
                }

                ConvolutionGPUBinaryInput cgBinInput;
                if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
                {
                    try
                    {
                        // FFT
                        ConvolutionFFT fftConv(
                            m_params, inputBuffer.data(), inputWidth, inputHeight,
                            kernelBuffer.data(), kernelWidth, kernelHeight
                        );

                        uint32_t numStages = 14;
                        uint32_t currStage = 0;

                        // Prepare padded buffers
                        currStage++;
                        m_state.fftStage = strFormat("%u/%u Preparing", currStage, numStages);
                        fftConv.pad();

                        if (m_state.mustCancel) throw std::exception(S_CANCELED_BY_USER);

                        // Repeat for 3 color channels
                        for (uint32_t i = 0; i < 3; i++)
                        {
                            // Input FFT
                            currStage++;
                            m_state.fftStage = strFormat("%u/%u %s: Input FFT", currStage, numStages, strFromColorChannelID(i).c_str());
                            fftConv.inputFFT(i);

                            if (m_state.mustCancel) throw std::exception(S_CANCELED_BY_USER);

                            // Kernel FFT
                            currStage++;
                            m_state.fftStage = strFormat("%u/%u %s: Kernel FFT", currStage, numStages, strFromColorChannelID(i).c_str());
                            fftConv.kernelFFT(i);

                            if (m_state.mustCancel) throw std::exception(S_CANCELED_BY_USER);

                            // Multiply the Fourier transforms
                            currStage++;
                            m_state.fftStage = strFormat("%u/%u %s: Multiplying", currStage, numStages, strFromColorChannelID(i).c_str());
                            fftConv.multiply(i);

                            if (m_state.mustCancel) throw std::exception(S_CANCELED_BY_USER);

                            // Inverse FFT
                            currStage++;
                            m_state.fftStage = strFormat("%u/%u %s: Inverse FFT", currStage, numStages, strFromColorChannelID(i).c_str());
                            fftConv.inverse(i);

                            if (m_state.mustCancel)throw std::exception(S_CANCELED_BY_USER);
                        }

                        // Get the final output
                        currStage++;
                        m_state.fftStage = strFormat("%u/%u Finalizing", currStage, numStages);
                        fftConv.output();

                        // Pour the result into the conv. layer
                        {
                            std::lock_guard<CmImage> lock(*m_imgConvLayer);
                            m_imgConvLayer->resize(inputWidth, inputHeight, false);
                            float* convBuffer = m_imgConvLayer->getImageData();
                            std::copy(
                                fftConv.getBuffer().data(),
                                fftConv.getBuffer().data() + fftConv.getBuffer().size(),
                                convBuffer
                            );
                        }
                    } catch (const std::exception& e)
                    {
                        setErrorState(e.what());
                    }
                }
                else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
                {
                    // Create and prepare threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        ConvolutionThread* ct = new ConvolutionThread(
                            numThreads, i, m_params,
                            inputBuffer.data(), inputWidth, inputHeight,
                            kernelBuffer.data(), kernelWidth, kernelHeight
                        );

                        std::vector<float>& threadBuffer = ct->getBuffer();
                        threadBuffer.resize(inputBufferSize);
                        for (uint32_t j = 0; j < inputBufferSize; j++)
                            if (j % 4 == 3) threadBuffer[j] = 1.0f;
                            else threadBuffer[j] = 0.0f;

                        m_threads.push_back(ct);
                    }

                    // Start the threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        ConvolutionThread* ct = m_threads[i];
                        std::shared_ptr<std::thread> t = std::make_shared<std::thread>([ct]()
                            {
                                ct->start();
                            });
                        ct->setThread(t);
                    }

                    // Wait for the threads
                    {
                        std::chrono::time_point<std::chrono::system_clock> lastTime = std::chrono::system_clock::now();
                        while (true)
                        {
                            uint32_t numThreadsDone = 0;
                            ConvolutionThread* ct;
                            ConvolutionThreadStats* stats;
                            for (uint32_t i = 0; i < numThreads; i++)
                            {
                                if (m_threads[i])
                                {
                                    ct = m_threads[i];
                                    stats = ct->getStats();
                                    if (stats->state == ConvolutionThreadState::Done)
                                        numThreadsDone++;
                                }
                            }

                            // Keep in mind that the threads will be "Done" when mustCancel is true
                            if (numThreadsDone >= numThreads)
                                break;

                            // Take a snapshot of the current progress
                            if ((!m_state.mustCancel) && (getElapsedMs(lastTime) > CONV_PROG_TIMESTEP) && (numThreadsDone < numThreads))
                            {
                                std::vector<float> progBuffer;
                                progBuffer.resize(inputBufferSize);
                                for (uint32_t i = 0; i < inputBufferSize; i++)
                                    if (i % 4 == 3) progBuffer[i] = 1.0f;
                                    else progBuffer[i] = 0.0f;

                                for (uint32_t i = 0; i < numThreads; i++)
                                {
                                    if (m_threads[i])
                                    {
                                        std::vector<float>& currentBuffer = m_threads[i]->getBuffer();
                                        uint32_t redIndex;
                                        for (uint32_t y = 0; y < inputHeight; y++)
                                        {
                                            for (uint32_t x = 0; x < inputWidth; x++)
                                            {
                                                redIndex = (y * inputWidth + x) * 4;
                                                progBuffer[redIndex + 0] += currentBuffer[redIndex + 0] * CONV_MULTIPLIER;
                                                progBuffer[redIndex + 1] += currentBuffer[redIndex + 1] * CONV_MULTIPLIER;
                                                progBuffer[redIndex + 2] += currentBuffer[redIndex + 2] * CONV_MULTIPLIER;
                                            }
                                        }
                                    }
                                }

                                // Copy progBuffer into the convolution layer
                                {
                                    std::lock_guard<CmImage> lock(*m_imgConvLayer);
                                    m_imgConvLayer->resize(inputWidth, inputHeight, false);
                                    float* convBuffer = m_imgConvLayer->getImageData();

                                    std::copy(progBuffer.data(), progBuffer.data() + inputBufferSize, convBuffer);
                                }
                                m_imgConvLayer->moveToGPU();

                                lastTime = std::chrono::system_clock::now();
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(CONV_WAIT_TIMESTEP));
                        }
                    }

                    // Join the threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        if (m_threads[i])
                            threadJoin(m_threads[i]->getThread().get());
                    }

                    clearVector(inputBuffer);
                    clearVector(kernelBuffer);
                }
                else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
                {
                    // Prepare input data for GPU convolution
                    cgBinInput.numChunks = numChunks;
                    cgBinInput.chunkSleep = chunkSleep;
                    cgBinInput.cp_kernelCenterX = m_params.kernelCenterX;
                    cgBinInput.cp_kernelCenterY = m_params.kernelCenterY;
                    cgBinInput.cp_convThreshold = m_params.convThreshold;
                    cgBinInput.cp_convKnee = m_params.convKnee;
                    cgBinInput.convMultiplier = CONV_MULTIPLIER;
                    cgBinInput.inputWidth = inputWidth;
                    cgBinInput.inputHeight = inputHeight;
                    cgBinInput.kernelWidth = kernelWidth;
                    cgBinInput.kernelHeight = kernelHeight;
                    cgBinInput.inputBuffer = inputBuffer.data();
                    cgBinInput.kernelBuffer = kernelBuffer.data();
                }

                if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
                {
                    // Prepare the convolution layer
                    std::lock_guard<CmImage> lock(*m_imgConvLayer);
                    m_imgConvLayer->resize(inputWidth, inputHeight, false);
                    m_imgConvLayer->fill(color_t{ 0, 0, 0, 1 }, false);
                    float* convBuffer = m_imgConvLayer->getImageData();

                    // Add the buffers from each thread
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        if (m_state.mustCancel)
                            break;

                        if (m_threads[i])
                        {
                            std::vector<float>& threadBuffer = m_threads[i]->getBuffer();
                            if (!m_state.mustCancel)
                            {
                                uint32_t redIndex;
                                for (uint32_t y = 0; y < inputHeight; y++)
                                {
                                    if (m_state.mustCancel)
                                        break;

                                    for (uint32_t x = 0; x < inputWidth; x++)
                                    {
                                        redIndex = (y * inputWidth + x) * 4;
                                        convBuffer[redIndex + 0] += threadBuffer[redIndex + 0] * CONV_MULTIPLIER;
                                        convBuffer[redIndex + 1] += threadBuffer[redIndex + 1] * CONV_MULTIPLIER;
                                        convBuffer[redIndex + 2] += threadBuffer[redIndex + 2] * CONV_MULTIPLIER;
                                    }
                                }
                            }
                        }
                    }
                }
                else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
                {
                    // Create input file for GPU convolution
                    std::string tempDir;
                    getTempDirectory(tempDir);
                    uint32_t randomNumber = (uint32_t)RandomNumber::nextInt();

                    // Filenames
                    std::string cgInpFilename = strFormat(
                        "%sgpuconv%u",
                        tempDir.c_str(),
                        randomNumber);
                    std::string cgOutFilename = cgInpFilename + "out";
                    std::string cgStatFilename = cgInpFilename + "stat";

                    // Global mutex for IO operations on the stat file
                    std::string cgStatMutexName = "GlobalRbStatMutex" + std::to_string(randomNumber);
                    {
                        cgBinInput.statMutexNameSize = cgStatMutexName.size();
                        cgBinInput.statMutexNameBuffer = new char[cgBinInput.statMutexNameSize];
                        cgStatMutexName.copy(cgBinInput.statMutexNameBuffer, cgBinInput.statMutexNameSize);
                    }
                    HANDLE cgStatMutex = createMutex(cgStatMutexName);
                    if (cgStatMutex == NULL)
                        setErrorState(strFormat("Mutex \"%s\" could not be created.", cgStatMutexName.c_str()));

                    // Create the input file
                    std::ofstream cgInpFile;
                    if (!m_state.failed)
                    {
                        cgInpFile.open(cgInpFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
                        if (!cgInpFile.is_open())
                        {
                            setErrorState(strFormat("File \"%s\" could not be created/opened.", cgInpFilename.c_str()));
                        }
                    }

                    // Write input data
                    if (!m_state.failed)
                    {
                        cgInpFile.write(AS_BYTES(cgBinInput.statMutexNameSize), sizeof(cgBinInput.statMutexNameSize));
                        cgInpFile.write(AS_BYTES(cgBinInput.numChunks), sizeof(cgBinInput.numChunks));
                        cgInpFile.write(AS_BYTES(cgBinInput.chunkSleep), sizeof(cgBinInput.chunkSleep));

                        cgInpFile.write(AS_BYTES(cgBinInput.cp_kernelCenterX), sizeof(cgBinInput.cp_kernelCenterX));
                        cgInpFile.write(AS_BYTES(cgBinInput.cp_kernelCenterY), sizeof(cgBinInput.cp_kernelCenterY));
                        cgInpFile.write(AS_BYTES(cgBinInput.cp_convThreshold), sizeof(cgBinInput.cp_convThreshold));
                        cgInpFile.write(AS_BYTES(cgBinInput.cp_convKnee), sizeof(cgBinInput.cp_convKnee));
                        cgInpFile.write(AS_BYTES(cgBinInput.convMultiplier), sizeof(cgBinInput.convMultiplier));

                        cgInpFile.write(AS_BYTES(cgBinInput.inputWidth), sizeof(cgBinInput.inputWidth));
                        cgInpFile.write(AS_BYTES(cgBinInput.inputHeight), sizeof(cgBinInput.inputHeight));
                        cgInpFile.write(AS_BYTES(cgBinInput.kernelWidth), sizeof(cgBinInput.kernelWidth));
                        cgInpFile.write(AS_BYTES(cgBinInput.kernelHeight), sizeof(cgBinInput.kernelHeight));

                        cgInpFile.write(cgBinInput.statMutexNameBuffer, cgBinInput.statMutexNameSize);

                        uint32_t inputSize = inputWidth * inputHeight * 4;
                        cgInpFile.write(PTR_AS_BYTES(inputBuffer.data()), inputSize * sizeof(float));
                        clearVector(inputBuffer);

                        uint32_t kernelSize = kernelWidth * kernelHeight * 4;
                        cgInpFile.write(PTR_AS_BYTES(kernelBuffer.data()), kernelSize * sizeof(float));
                        clearVector(kernelBuffer);

                        cgInpFile.flush();
                        cgInpFile.close();
                        if (cgInpFile.fail())
                        {
                            setErrorState("There was a problem when writing to the input file.");
                        }
                    }
                    DELARR(cgBinInput.statMutexNameBuffer);

                    // Execute GPU Helper
                    STARTUPINFOA cgStartupInfo;
                    PROCESS_INFORMATION cgProcessInfo;

                    ZeroMemory(&cgStartupInfo, sizeof(cgStartupInfo));
                    cgStartupInfo.cb = sizeof(cgStartupInfo);
                    ZeroMemory(&cgProcessInfo, sizeof(cgProcessInfo));

                    bool cgHasHandles = false;
                    std::string cgCommandLine = strFormat("RealBloomGPUConv.exe \"%s\"", cgInpFilename.c_str());
                    if (!m_state.failed)
                    {
                        if (CreateProcessA(
                            NULL,                            // No module name (use command line)
                            (char*)(cgCommandLine.c_str()),  // Command line
                            NULL,                            // Process handle not inheritable
                            NULL,                            // Thread handle not inheritable
                            FALSE,                           // Set handle inheritance to FALSE
                            CREATE_NO_WINDOW,                // Creation flags
                            NULL,                            // Use parent's environment block
                            NULL,                            // Use parent's starting directory 
                            &cgStartupInfo,                  // Pointer to STARTUPINFO structure
                            &cgProcessInfo))                 // Pointer to PROCESS_INFORMATION structure
                        {
                            cgHasHandles = true;

                            // Close when the parent dies
                            HANDLE hJobObject = Async::getShared("hJobObject");
                            if (hJobObject)
                                AssignProcessToJobObject(hJobObject, cgProcessInfo.hProcess);
                        }
                        else
                        {
                            setErrorState(strFormat("CreateProcess failed (%d).", GetLastError()));
                        }
                    }

                    // Wait for the output file to be created
                    if (!m_state.failed)
                    {
                        auto t1 = std::chrono::system_clock::now();
                        while (!std::filesystem::exists(cgOutFilename))
                        {
                            if (getElapsedMs(t1) > CONVGPU_FILE_TIMEOUT)
                            {
                                killProcess(cgProcessInfo);
                                setErrorState("RealBloomGPUConv did not create an output file.");
                                break;
                            }
                            else if (m_state.mustCancel)
                            {
                                killProcess(cgProcessInfo);
                                setErrorState(S_CANCELED_BY_USER);
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(CONV_WAIT_TIMESTEP));
                        }
                    }

                    // Wait for our little GPU helper buddy to finish its job
                    if (!m_state.failed)
                    {
                        auto t1 = std::chrono::system_clock::now();
                        while (processIsRunning(cgProcessInfo))
                        {
                            if (m_state.mustCancel)
                            {
                                killProcess(cgProcessInfo);
                                setErrorState(S_CANCELED_BY_USER);
                                break;
                            }

                            // Read stat file and update conv layer
                            if (!(m_state.mustCancel) && std::filesystem::exists(cgStatFilename) && (getElapsedMs(t1) > CONV_PROG_TIMESTEP))
                            {
                                ConvolutionGPUBinaryStat cgBinStat;
                                waitForMutex(cgStatMutex);

                                // Open and read the stat file, check if numChunksDone has increased,
                                // and if it has, then read the rest of the file and the buffer, and
                                // show it using the conv layer image.
                                std::ifstream cgStatFile;
                                cgStatFile.open(cgStatFilename, std::ifstream::in | std::ifstream::binary);
                                bool statParseFailed = false;
                                if (cgStatFile.is_open())
                                {
                                    cgStatFile.seekg(std::ifstream::beg);

                                    cgStatFile.read(AS_BYTES(cgBinStat.numChunksDone), sizeof(cgBinStat.numChunksDone));
                                    statParseFailed |= cgStatFile.fail();

                                    if (!statParseFailed && (cgBinStat.numChunksDone > m_state.numChunksDone))
                                        m_state.numChunksDone = cgBinStat.numChunksDone;
                                    else
                                        statParseFailed = true;

                                    if (!statParseFailed)
                                    {
                                        cgStatFile.read(AS_BYTES(cgBinStat.bufferSize), sizeof(cgBinStat.bufferSize));
                                        statParseFailed |= cgStatFile.fail();

                                        if (!statParseFailed && (cgBinStat.bufferSize > 0))
                                        {
                                            cgBinStat.buffer = new float[cgBinStat.bufferSize];
                                            cgStatFile.read(PTR_AS_BYTES(cgBinStat.buffer), cgBinStat.bufferSize * sizeof(float));
                                            statParseFailed |= cgStatFile.fail();
                                        }
                                        else
                                            statParseFailed = true;
                                    }

                                    if (!statParseFailed && (cgBinStat.bufferSize > 0) && (cgBinStat.buffer != nullptr))
                                    {
                                        // Copy the stat buffer into the convolution layer
                                        {
                                            std::lock_guard<CmImage> lock(*m_imgConvLayer);
                                            m_imgConvLayer->resize(inputWidth, inputHeight, false);
                                            float* convBuffer = m_imgConvLayer->getImageData();
                                            std::copy(cgBinStat.buffer, cgBinStat.buffer + cgBinStat.bufferSize, convBuffer);
                                        }
                                        m_imgConvLayer->moveToGPU();
                                    }

                                    DELARR(cgBinStat.buffer);
                                    cgStatFile.close();
                                }

                                releaseMutex(cgStatMutex);
                                t1 = std::chrono::system_clock::now();
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(CONV_WAIT_TIMESTEP));
                        }
                    }

                    // No longer need the mutex
                    closeMutex(cgStatMutex);

                    // Open the output file
                    std::ifstream cgOutFile;
                    if (!m_state.failed)
                    {
                        cgOutFile.open(cgOutFilename, std::ifstream::in | std::ifstream::binary);
                        if (!cgOutFile.is_open() || cgOutFile.fail())
                            setErrorState(strFormat("Output file \"%s\" could not be opened.", cgOutFilename.c_str()));
                        else
                            cgOutFile.seekg(std::ifstream::beg);
                    }

                    // Parse the output file
                    ConvolutionGPUBinaryOutput cgBinOutput;
                    if (!m_state.failed)
                    {
                        bool parseFailed = false;
                        std::string parseStage = "Initialization";
                        if (!parseFailed)
                        {
                            cgOutFile.read(AS_BYTES(cgBinOutput.status), sizeof(cgBinOutput.status));
                            parseFailed |= cgOutFile.fail();
                            parseStage = "status";
                        }
                        if (!parseFailed)
                        {
                            cgOutFile.read(AS_BYTES(cgBinOutput.errorSize), sizeof(cgBinOutput.errorSize));
                            parseFailed |= cgOutFile.fail();
                            parseStage = "errorSize";
                        }
                        if (!parseFailed)
                        {
                            cgOutFile.read(AS_BYTES(cgBinOutput.bufferSize), sizeof(cgBinOutput.bufferSize));
                            parseFailed |= cgOutFile.fail();
                            parseStage = "bufferSize";
                        }
                        if (!parseFailed)
                        {
                            if (cgBinOutput.errorSize > 0)
                            {
                                cgBinOutput.errorBuffer = new char[cgBinOutput.errorSize + 1];
                                cgBinOutput.errorBuffer[cgBinOutput.errorSize] = '\0';

                                cgOutFile.read(cgBinOutput.errorBuffer, cgBinOutput.errorSize);
                                parseFailed |= cgOutFile.fail();
                                parseStage = "errorBuffer";
                            }
                        }
                        if (!parseFailed)
                        {
                            if (cgBinOutput.bufferSize > 0)
                            {
                                cgBinOutput.buffer = new float[cgBinOutput.bufferSize];

                                cgOutFile.read(PTR_AS_BYTES(cgBinOutput.buffer), cgBinOutput.bufferSize * sizeof(float));
                                parseFailed |= cgOutFile.fail();
                                parseStage = "buffer";
                            }
                        }
                        cgOutFile.close();

                        if (parseFailed)
                            if (parseStage == "status")
                                setErrorState(
                                    "Could not retrieve data from the output file. The GPU Helper has probably "
                                    "crashed. Try using more chunks, as the GPU might not be able to handle the "
                                    "current amount of data at once.");
                            else
                                setErrorState(strFormat(
                                    "Could not retrieve data from the output file, failed at \"%s\".", parseStage.c_str()));
                    }

                    // If successful, copy the output buffer to our conv. layer,
                    // otherwise set error
                    if (!m_state.failed)
                    {
                        if (cgBinOutput.status == 1)
                        {
                            // Prepare the convolution layer
                            std::lock_guard<CmImage> lock(*m_imgConvLayer);
                            m_imgConvLayer->resize(inputWidth, inputHeight, false);
                            m_imgConvLayer->fill(color_t{ 0, 0, 0, 1 }, false);
                            float* convBuffer = m_imgConvLayer->getImageData();
                            uint32_t convBufferSize = m_imgConvLayer->getImageDataSize();

                            if (convBufferSize == cgBinOutput.bufferSize)
                            {
                                for (uint32_t i = 0; i < cgBinOutput.bufferSize; i++)
                                {
                                    convBuffer[i] = cgBinOutput.buffer[i];
                                }
                            }
                            else
                            {
                                setErrorState(strFormat(
                                    "Output buffer size (%u) does not match the input size (%u).", cgBinOutput.bufferSize, convBufferSize));
                            }
                        }
                        else
                        {
                            std::string errorS = "Unknown";
                            if (cgBinOutput.errorSize > 0)
                                errorS = cgBinOutput.errorBuffer;

                            setErrorState(errorS);
                        }
                    }

                    // Clean up
                    DELARR(cgBinOutput.errorBuffer);
                    DELARR(cgBinOutput.buffer);
                    if (cgHasHandles)
                    {
                        CloseHandle(cgProcessInfo.hProcess);
                        CloseHandle(cgProcessInfo.hThread);
                        cgHasHandles = false;
                    }

                    // Delete the temporary files
                    if (CONVGPU_DELETE_TEMP)
                    {
                        deleteFile(cgInpFilename);
                        deleteFile(cgStatFilename);
                        deleteFile(cgOutFilename);
                    }
                }

                // Clean up
                if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
                {
                    for (uint32_t i = 0; i < numThreads; i++)
                        DELPTR(m_threads[i]);
                    m_threads.clear();
                }
                clearVector(inputBuffer);
                clearVector(kernelBuffer);

                // Update the conv. layer image
                if (!m_state.failed && !m_state.mustCancel)
                {
                    {
                        std::lock_guard<CmImage> lock(*m_imgConvLayer);
                        float* buffer = m_imgConvLayer->getImageData();
                        uint32_t num = m_imgConvLayer->getImageDataSize();

                        float minV[3] = { buffer[0], buffer[1], buffer[2] };
                        float maxV[3] = { buffer[0], buffer[1], buffer[2] };
                        for (size_t i = 0; i < num; i++)
                        {
                            if (i % 4 != 3)
                            {
                                if (buffer[i] > maxV[i % 4])
                                    maxV[i % 4] = buffer[i];
                                if (buffer[i] < minV[i % 4])
                                    minV[i % 4] = buffer[i];
                            }
                        }

                        // Print details about the output
                        if (0)
                        {
                            std::cout << strFormat(
                                "device type:  %u\n"
                                "R range:      %.3f - %.3f\n"
                                "G range:      %.3f - %.3f\n"
                                "B range:      %.3f - %.3f\n\n",
                                (uint32_t)m_params.methodInfo.method,
                                minV[0], maxV[0],
                                minV[1], maxV[1],
                                minV[2], maxV[2]
                            );
                        }
                    }

                    m_imgConvLayer->moveToGPU();
                    Async::schedule([this]()
                        {
                            bool* pConvMixParamsChanged = (bool*)Async::getShared("convMixParamsChanged");
                            if (pConvMixParamsChanged != nullptr) *pConvMixParamsChanged = true;
                        });
                }

                m_state.working = false;
                m_state.timeEnd = std::chrono::system_clock::now();
                m_state.hasTimestamps = true;
            });
    }

    void Convolution::cancelConv()
    {
        if (m_state.working)
        {
            m_state.mustCancel = true;
            if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
            {
                // Tell the sub-threads to stop
                ConvolutionThread* ct;
                for (uint32_t i = 0; i < m_threads.size(); i++)
                {
                    ct = m_threads[i];
                    if (ct)
                    {
                        ct->stop();
                    }
                }
            }

            // Wait for the main convolution thread
            threadJoin(m_thread);
            DELPTR(m_thread);
        }

        m_state.working = false;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.hasTimestamps = false;
        m_state.numChunksDone = 0;
        m_state.fftStage = "";
    }

    bool Convolution::isWorking()
    {
        return m_state.working;
    }

    void Convolution::getConvStats(std::string& outTime, std::string& outStatus, uint32_t& outStatType)
    {
        outTime = "";
        outStatus = "";
        outStatType = 0;

        if (m_state.failed && !m_state.mustCancel)
        {
            outStatus = m_state.error;
            outStatType = 3;
        }
        else if (m_state.working && !m_state.mustCancel)
        {
            float elapsedSec = (float)getElapsedMs(m_state.timeStart) / 1000.0f;
            if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
            {
                uint32_t numThreads = m_threads.size();
                uint32_t numPixels = 0;
                uint32_t numDone = 0;

                for (uint32_t i = 0; i < numThreads; i++)
                {
                    if (m_threads[i])
                    {
                        ConvolutionThreadStats* stats = m_threads[i]->getStats();
                        if (stats->state == ConvolutionThreadState::Working || stats->state == ConvolutionThreadState::Done)
                        {
                            numPixels += stats->numPixels;
                            numDone += stats->numDone;
                        }
                    }
                }

                float progress = (numPixels > 0) ? (float)numDone / (float)numPixels : 1.0f;
                float remainingSec = (elapsedSec * (float)(numPixels - numDone)) / fmaxf((float)(numDone), EPSILON);
                outTime = strFormat(
                    "%.1f%%%% (%u/%u)\n%s / %s",
                    progress * 100.0f,
                    numDone,
                    numPixels,
                    strFromElapsed(elapsedSec).c_str(),
                    strFromElapsed(remainingSec).c_str());
            }
            else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
            {
                outTime = strFormat(
                    "%u/%u chunks\n%s",
                    m_state.numChunksDone,
                    m_state.methodInfo.numChunks,
                    strFromElapsed(elapsedSec));
            }
            else if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
            {
                outTime = strFormat(
                    "%s\n%s",
                    m_state.fftStage.c_str(),
                    strFromElapsed(elapsedSec));
            }
        }
        else if (m_state.hasTimestamps && !m_state.mustCancel)
        {
            std::chrono::milliseconds elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_state.timeEnd - m_state.timeStart);
            float elapsedSec = (float)elapsedMs.count() / 1000.0f;
            outTime = strFormat("Done (%s)", strFromDuration(elapsedSec).c_str());
        }
    }

    std::string Convolution::getResourceInfo()
    {
        // Kernel Size
        uint32_t kernelWidth = 0, kernelHeight = 0;
        kernel(false, nullptr, &kernelWidth, &kernelHeight);

        // Input Size
        uint32_t inputWidth = m_imgInput->getWidth();
        uint32_t inputHeight = m_imgInput->getHeight();

        // Estimate resource usage
        uint64_t numPixels = 0;
        uint64_t numPixelsPerBlock = 0;
        uint64_t ramUsage = 0;
        uint64_t vramUsage = 0;

        // Number of pixels that pass the threshold
        if ((m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU) || (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU))
            previewThreshold(&numPixels);

        // Number of pixels per thread/chunk
        if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
            numPixelsPerBlock = numPixels / m_params.methodInfo.numThreads;
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
            numPixelsPerBlock = numPixels / m_params.methodInfo.numChunks;

        // Input and kernel size in bytes
        uint64_t inputSizeBytes = (uint64_t)inputWidth * (uint64_t)inputHeight * 4 * sizeof(float);
        uint64_t kernelSizeBytes = (uint64_t)kernelWidth * (uint64_t)kernelHeight * 4 * sizeof(float);

        // Calculate memory usage for different methods
        if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
        {
            uint32_t paddedWidth, paddedHeight;
            ConvolutionFFT::calcPadding(
                inputWidth, inputHeight,
                kernelWidth, kernelHeight,
                m_params.kernelCenterX, m_params.kernelCenterY,
                paddedWidth, paddedHeight);

            ramUsage = inputSizeBytes + kernelSizeBytes + ((uint64_t)paddedWidth * (uint64_t)paddedHeight * 10 * sizeof(float));
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
        {
            ramUsage = inputSizeBytes + kernelSizeBytes + (inputSizeBytes * m_params.methodInfo.numThreads);
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
        {
            // input buffer + kernel buffer + final output buffer + output buffer for chunk
            // + (numPoints * 5) + fbData (same size as input buffer)
            ramUsage = (inputSizeBytes * 4) + kernelSizeBytes + (numPixelsPerBlock * 5 * sizeof(float));

            // (numPoints * 5) + kernel buffer + frame buffer (same size as input buffer)
            vramUsage = (numPixelsPerBlock * 5 * sizeof(float)) + kernelSizeBytes + inputSizeBytes;
        }

        // Format output
        if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
        {
            return strFormat(
                "Est. Memory: %s",
                strFromSize(ramUsage).c_str());
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
        {
            return strFormat(
                "Total Pixels: %s\nPixels/Thread: %s\nEst. Memory: %s",
                strFromBigNumber(numPixels).c_str(),
                strFromBigNumber(numPixelsPerBlock).c_str(),
                strFromSize(ramUsage).c_str());
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
        {
            return strFormat(
                "Total Pixels: %s\nPixels/Chunk: %s\nEst. Memory: %s\nEst. VRAM: %s",
                strFromBigNumber(numPixels).c_str(),
                strFromBigNumber(numPixelsPerBlock).c_str(),
                strFromSize(ramUsage).c_str(),
                strFromSize(vramUsage).c_str());
        }

        return "";
    }

    void drawRect(CmImage* image, int rx, int ry, int rw, int rh)
    {
        std::lock_guard<CmImage> lock(*image);
        float* buffer = image->getImageData();
        uint32_t imageWidth = image->getWidth();
        uint32_t imageHeight = image->getHeight();

        int rx2 = rx + rw;
        int ry2 = ry + rh;

        uint32_t redIndex;
        for (int y = ry; y <= ry2; y++)
        {
            if (checkBounds(rx, y, imageWidth, imageHeight))
            {
                redIndex = (y * imageWidth + rx) * 4;
                buffer[redIndex + 0] = 0;
                buffer[redIndex + 1] = 0;
                buffer[redIndex + 2] = 1;
                buffer[redIndex + 3] = 1;
            }

            if (checkBounds(rx2, y, imageWidth, imageHeight))
            {
                redIndex = (y * imageWidth + rx2) * 4;
                buffer[redIndex + 0] = 0;
                buffer[redIndex + 1] = 0;
                buffer[redIndex + 2] = 1;
                buffer[redIndex + 3] = 1;
            }
        }
        for (int x = rx; x <= rx2; x++)
        {
            if (checkBounds(x, ry, imageWidth, imageHeight))
            {
                redIndex = (ry * imageWidth + x) * 4;
                buffer[redIndex + 0] = 0;
                buffer[redIndex + 1] = 0;
                buffer[redIndex + 2] = 1;
                buffer[redIndex + 3] = 1;
            }

            if (checkBounds(x, ry2, imageWidth, imageHeight))
            {
                redIndex = (ry2 * imageWidth + x) * 4;
                buffer[redIndex + 0] = 0;
                buffer[redIndex + 1] = 0;
                buffer[redIndex + 2] = 1;
                buffer[redIndex + 3] = 1;
            }
        }
    }

    void fillRect(CmImage* image, int rx, int ry, int rw, int rh)
    {
        std::lock_guard<CmImage> lock(*image);
        float* buffer = image->getImageData();
        uint32_t imageWidth = image->getWidth();
        uint32_t imageHeight = image->getHeight();

        int rx2 = rx + rw;
        int ry2 = ry + rh;

        uint32_t redIndex;
        for (int y = ry; y <= ry2; y++)
        {
            for (int x = rx; x <= rx2; x++)
            {
                if (checkBounds(x, y, imageWidth, imageHeight))
                {
                    redIndex = (y * imageWidth + x) * 4;
                    buffer[redIndex + 0] = 0;
                    buffer[redIndex + 1] = 0;
                    buffer[redIndex + 2] = 1;
                    buffer[redIndex + 3] = 1;
                }
            }
        }
    }

    std::string strFromColorChannelID(uint32_t ch)
    {
        if (ch == 0)
            return "R";
        if (ch == 1)
            return "G";
        return "B";
    }

}
