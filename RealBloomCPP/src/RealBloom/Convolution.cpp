#include "Convolution.h"
#include "ConvolutionThread.h"

#include <Windows.h>

constexpr uint32_t CONV_WAIT_TIMESTEP = 50;
constexpr uint32_t CONV_PROG_TIMESTEP = 1000;

constexpr uint32_t CONVGPU_FILE_TIMEOUT = 5000;
constexpr bool CONVGPU_DELETE_TEMP = true;

constexpr const char* TEXT_CANCELED_BY_USER = "Canceled by user.";

namespace RealBloom
{

    void drawRect(CmImage* image, int rx, int ry, int rw, int rh);
    void fillRect(CmImage* image, int rx, int ry, int rw, int rh);

    void Convolution::setErrorState(const std::string& error)
    {
        m_state.error = error;
        m_state.failed = true;
    }

    Convolution::Convolution()
        : m_imageInput(nullptr), m_imageKernel(nullptr), m_imageKernelPreview(nullptr),
        m_imageConvPreview(nullptr), m_imageConvLayer(nullptr), m_imageConvMix(nullptr),
        m_thread(nullptr)
    {
        //
    }

    ConvolutionParams* Convolution::getParams()
    {
        return &m_params;
    }

    void Convolution::setInputImage(CmImage* image)
    {
        m_imageInput = image;
    }

    void Convolution::setKernelImage(CmImage* image)
    {
        m_imageKernel = image;
    }

    void Convolution::setKernelPreviewImage(CmImage* image)
    {
        m_imageKernelPreview = image;
    }

    void Convolution::setConvPreviewImage(CmImage* image)
    {
        m_imageConvPreview = image;
    }

    void Convolution::setConvLayerImage(CmImage* image)
    {
        m_imageConvLayer = image;
    }

    void Convolution::setConvMixImage(CmImage* image)
    {
        m_imageConvMix = image;
    }

    void Convolution::previewThreshold(size_t* outNumPixels)
    {
        float threshold = m_params.convThreshold;
        size_t numPixels = 0;
        {
            // Input image
            std::lock_guard<CmImage> lock1(*m_imageInput);
            float* inputBuffer = m_imageInput->getImageData();
            uint32_t inputWidth = m_imageInput->getWidth();
            uint32_t inputHeight = m_imageInput->getHeight();

            // Convolution Preview Image
            std::lock_guard<CmImage> lock2(*m_imageConvPreview);
            m_imageConvPreview->resize(inputWidth, inputHeight, false);
            float* prevBuffer = m_imageConvPreview->getImageData();

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
        m_imageConvPreview->moveToGPU();

        if (outNumPixels)
            *outNumPixels = numPixels;
    }

    void Convolution::kernel(bool previewMode, float** outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        {
            std::vector<float> kernelBuffer;
            uint32_t kernelBufferSize = 0;
            uint32_t kernelWidth = 0, kernelHeight = 0;
            {
                std::lock_guard<CmImage> lock1(*m_imageKernel);
                float* srcKernelBuffer = m_imageKernel->getImageData();
                kernelBufferSize = m_imageKernel->getImageDataSize();
                kernelWidth = m_imageKernel->getWidth();
                kernelHeight = m_imageKernel->getHeight();

                kernelBuffer.resize(kernelBufferSize);
                std::copy(srcKernelBuffer, srcKernelBuffer + kernelBufferSize, kernelBuffer.data());
            }

            float centerX, centerY;
            centerX = (float)kernelWidth / 2.0f;
            centerY = (float)kernelHeight / 2.0f;

            float scaleW = fmaxf(m_params.kernelScaleW, 0.01f);
            float scaleH = fmaxf(m_params.kernelScaleH, 0.01f);
            float cropW = fminf(fmaxf(m_params.kernelCropW, 0.1f), 1.0f);
            float cropH = fminf(fmaxf(m_params.kernelCropH, 0.1f), 1.0f);
            float rotation = m_params.kernelRotation;

            float kernelContrast = m_params.kernelContrast;
            float kernelMultiplier = intensityCurve(fmaxf(0.0f, m_params.kernelIntensity));

            uint32_t scaledWidth, scaledHeight;
            scaledWidth = (uint32_t)floorf(scaleW * (float)kernelWidth);
            scaledHeight = (uint32_t)floorf(scaleH * (float)kernelHeight);

            uint32_t croppedWidth, croppedHeight;
            croppedWidth = (uint32_t)floorf(cropW * (float)scaledWidth);
            croppedHeight = (uint32_t)floorf(cropH * (float)scaledHeight);

            float scaledCenterX, scaledCenterY;
            scaledCenterX = (scaleW * (float)kernelWidth) / 2.0f;
            scaledCenterY = (scaleH * (float)kernelHeight) / 2.0f;

            // Normalize, apply contrast and multiplier
            {
                float v = 0;
                float maxV = EPSILON;
                for (uint32_t i = 0; i < kernelBufferSize; i++)
                {
                    v = fmaxf(0.0f, kernelBuffer[i]);
                    if ((v > maxV) && (i % 4 != 3)) // if not alpha channel
                        maxV = v;
                }

                float grayscale, rgbMultiplier;
                uint32_t redIndex;
                for (uint32_t y = 0; y < kernelHeight; y++)
                {
                    for (uint32_t x = 0; x < kernelWidth; x++)
                    {
                        redIndex = (y * kernelWidth + x) * 4;

                        kernelBuffer[redIndex + 0] /= maxV;
                        kernelBuffer[redIndex + 1] /= maxV;
                        kernelBuffer[redIndex + 2] /= maxV;

                        grayscale = rgbToGrayscale(kernelBuffer[redIndex + 0], kernelBuffer[redIndex + 1], kernelBuffer[redIndex + 2]);
                        rgbMultiplier = (contrastCurve(grayscale, kernelContrast) / fmaxf(grayscale, EPSILON)) * kernelMultiplier;

                        kernelBuffer[redIndex + 0] *= rgbMultiplier;
                        kernelBuffer[redIndex + 1] *= rgbMultiplier;
                        kernelBuffer[redIndex + 2] *= rgbMultiplier;
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
            } else
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
            kernelBuffer.clear();

            // Crop
            std::vector<float> croppedBuffer;
            uint32_t croppedBufferSize = 0;
            if ((cropW == 1) && (cropH == 1))
            {
                croppedBufferSize = scaledBufferSize;
                croppedBuffer.resize(croppedBufferSize);
                std::copy(scaledBuffer.data(), scaledBuffer.data() + scaledBufferSize, croppedBuffer.data());
            } else
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
            scaledBuffer.clear();

            // Copy to outBuffer if it's requested by another function
            if (!previewMode && (outBuffer != nullptr))
            {
                *outWidth = croppedWidth;
                *outHeight = croppedHeight;
                *outBuffer = new float[croppedBufferSize];
                std::copy(croppedBuffer.data(), croppedBuffer.data() + croppedBufferSize, *outBuffer);
            }

            // Copy to kernel preview image
            {
                std::lock_guard<CmImage> lock2(*m_imageKernelPreview);
                m_imageKernelPreview->resize(croppedWidth, croppedHeight, false);
                float* prevBuffer = m_imageKernelPreview->getImageData();
                std::copy(croppedBuffer.data(), croppedBuffer.data() + croppedBufferSize, prevBuffer);
            }

            // Preview center point
            if (m_params.kernelPreviewCenter)
            {
                int newCenterX = (int)floorf(m_params.kernelCenterX * (float)croppedWidth - 0.5f);
                int newCenterY = (int)floorf(m_params.kernelCenterY * (float)croppedHeight - 0.5f);
                int prevSquareSize = (int)roundf(0.15f * fminf((float)croppedWidth, (float)croppedHeight));

                drawRect(
                    m_imageKernelPreview,
                    newCenterX - (prevSquareSize / 2),
                    newCenterY - (prevSquareSize / 2),
                    prevSquareSize, prevSquareSize);

                fillRect(m_imageKernelPreview, newCenterX - 2, newCenterY - 2, 4, 4);
            }
        }

        m_imageKernelPreview->moveToGPU();
    }

    void Convolution::mixConv(bool additive, float inputMix, float convMix, float mix, float convIntensity)
    {
        inputMix = fmaxf(inputMix, 0.0f);
        convMix = fmaxf(convMix, 0.0f);
        convIntensity = fmaxf(convIntensity, 0.0f);
        if (!additive)
        {
            mix = fminf(fmaxf(mix, 0.0f), 1.0f);
            convMix = mix;
            inputMix = 1.0f - mix;
        }

        {
            // Input buffer
            std::lock_guard<CmImage> inputImageLock(*m_imageInput);
            float* inputBuffer = m_imageInput->getImageData();
            uint32_t inputWidth = m_imageInput->getWidth();
            uint32_t inputHeight = m_imageInput->getHeight();

            // Conv Buffer
            std::lock_guard<CmImage> convLayerImageLock(*m_imageConvLayer);
            float* convBuffer = m_imageConvLayer->getImageData();
            uint32_t convWidth = m_imageConvLayer->getWidth();
            uint32_t convHeight = m_imageConvLayer->getHeight();

            if ((inputWidth == convWidth) && (inputHeight == convHeight))
            {
                // Conv Mix Buffer
                std::lock_guard<CmImage> convMixImageLock(*m_imageConvMix);
                m_imageConvMix->resize(inputWidth, inputHeight, false);
                float* convMixBuffer = m_imageConvMix->getImageData();

                float multiplier;
                if (additive)
                    multiplier = intensityCurve(convMix);
                else
                    multiplier = convMix * intensityCurve(convIntensity);

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

        m_imageConvMix->moveToGPU();
    }

    void Convolution::convolve()
    {
        // If there's already a convolution process going on
        cancelConv();

        // Clamp the number of threads so it's not negative or some crazy number
        uint32_t maxThreads = std::thread::hardware_concurrency();
        uint32_t numThreads = m_params.device.numThreads;
        if (numThreads > maxThreads) numThreads = maxThreads;
        if (numThreads < 1) numThreads = 1;

        // Same for chunks
        uint32_t numChunks = m_params.device.numChunks;
        if (numChunks < 1) numChunks = 1;
        if (numChunks > CONV_MAX_CHUNKS) numChunks = CONV_MAX_CHUNKS;

        uint32_t chunkSleep = m_params.device.chunkSleep;

        // Reset state
        m_state.working = true;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.error = "";
        m_state.timeStart = std::chrono::system_clock::now();
        m_state.hasTimestamps = false;
        m_state.device = m_params.device;
        m_state.numChunksDone = 0;

        // Start the main thread
        m_thread = new std::thread([this, numThreads, maxThreads, numChunks, chunkSleep]()
            {
                // Kernel Buffer
                float* kernelBuffer = nullptr;
                uint32_t kernelWidth = 0, kernelHeight = 0;
                kernel(false, &kernelBuffer, &kernelWidth, &kernelHeight);

                // Input Buffer
                float* inputBuffer = nullptr;
                uint32_t inputWidth = 0, inputHeight = 0;
                uint32_t inputBufferSize = 0;
                {
                    // Input buffer from input image
                    std::lock_guard<CmImage> inputImageLock(*m_imageInput);
                    inputWidth = m_imageInput->getWidth();
                    inputHeight = m_imageInput->getHeight();
                    float* inputImageData = m_imageInput->getImageData();

                    // Copy the buffer so the input image doesn't stay locked throughout the process
                    inputBufferSize = m_imageInput->getImageDataSize();
                    inputBuffer = new float[inputBufferSize];
                    std::copy(inputImageData, inputImageData + inputBufferSize, inputBuffer);
                }

                ConvolutionGPUBinaryInput cgBinInput;
                if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
                {
                    // Create and prepare threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        ConvolutionThread* ct = new ConvolutionThread(
                            numThreads, i, m_params,
                            inputBuffer, inputWidth, inputHeight,
                            kernelBuffer, kernelWidth, kernelHeight);

                        std::vector<float>* threadBuffer = ct->getBuffer();
                        threadBuffer->resize(inputBufferSize);
                        for (uint32_t j = 0; j < inputBufferSize; j++)
                            if (j % 4 == 3) (*threadBuffer)[j] = 1.0f;
                            else (*threadBuffer)[j] = 0.0f;

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
                        uint32_t numThreadsDone;
                        std::chrono::time_point<std::chrono::system_clock> lastTime = std::chrono::system_clock::now();
                        while (true)
                        {
                            numThreadsDone = 0;
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

                            // Take a snapshot of the current progress and put it into conv result
                            if ((!m_state.mustCancel) && (getElapsedMs(lastTime) > CONV_PROG_TIMESTEP) && (numThreadsDone < numThreads))
                            {
                                float* progBuffer = new float[inputBufferSize];
                                for (uint32_t i = 0; i < inputBufferSize; i++)
                                    if (i % 4 == 3) progBuffer[i] = 1.0f;
                                    else progBuffer[i] = 0.0f;

                                for (uint32_t i = 0; i < numThreads; i++)
                                {
                                    if (m_threads[i])
                                    {
                                        std::vector<float> currentBuffer = *(m_threads[i]->getBuffer());
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
                                    std::lock_guard<CmImage> convLayerImageLock(*m_imageConvLayer);
                                    m_imageConvLayer->resize(inputWidth, inputHeight, false);
                                    float* convBuffer = m_imageConvLayer->getImageData();

                                    std::copy(progBuffer, progBuffer + inputBufferSize, convBuffer);
                                    delete[] progBuffer;
                                }
                                m_imageConvLayer->moveToGPU();

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
                    DELARR(inputBuffer);
                    DELARR(kernelBuffer);
                } else
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
                    cgBinInput.inputBuffer = inputBuffer;
                    cgBinInput.kernelBuffer = kernelBuffer;
                }

                if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
                {
                    // Prepare the convolution layer

                    std::lock_guard<CmImage> convLayerImageLock(*m_imageConvLayer);
                    m_imageConvLayer->resize(inputWidth, inputHeight, false);
                    m_imageConvLayer->fill(color_t{ 0, 0, 0, 1 }, false);
                    float* convBuffer = m_imageConvLayer->getImageData();

                    // Add the buffers from each thread
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        if (m_state.mustCancel)
                            break;

                        if (m_threads[i])
                        {
                            std::vector<float> threadBuffer = *(m_threads[i]->getBuffer());
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
                                        blendAddRGB(convBuffer, redIndex, threadBuffer.data(), redIndex, CONV_MULTIPLIER);
                                    }
                                }
                            }
                        }
                    }
                } else
                {
                    // Create input file for GPU convolution
                    std::string tempDir;
                    getTempDirectory(tempDir);
                    uint32_t randomNumber = (uint32_t)RandomNumber::nextInt();

                    // Filenames
                    std::string cgInpFilename = stringFormat(
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
                        setErrorState(stringFormat("Mutex \"%s\" could not be created.", cgStatMutexName.c_str()));

                    // Create the input file
                    std::ofstream cgInpFile;
                    if (!m_state.failed)
                    {
                        cgInpFile.open(cgInpFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
                        if (!cgInpFile.is_open())
                        {
                            setErrorState(stringFormat("File \"%s\" could not be created/opened.", cgInpFilename.c_str()));
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
                        cgInpFile.write(PTR_AS_BYTES(inputBuffer), inputSize * sizeof(float));
                        DELARR(inputBuffer);

                        uint32_t kernelSize = kernelWidth * kernelHeight * 4;
                        cgInpFile.write(PTR_AS_BYTES(kernelBuffer), kernelSize * sizeof(float));
                        DELARR(kernelBuffer);

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
                    std::string cgCommandLine = stringFormat("RealBloomGPUConv.exe \"%s\"", cgInpFilename.c_str());
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
                        } else
                        {
                            setErrorState(stringFormat("CreateProcess failed (%d).", GetLastError()));
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
                            } else if (m_state.mustCancel)
                            {
                                killProcess(cgProcessInfo);
                                setErrorState(TEXT_CANCELED_BY_USER);
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
                                setErrorState(TEXT_CANCELED_BY_USER);
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
                                        } else
                                            statParseFailed = true;
                                    }

                                    if (!statParseFailed && (cgBinStat.bufferSize > 0) && (cgBinStat.buffer != nullptr))
                                    {
                                        // Copy the stat buffer into the convolution layer
                                        {
                                            std::lock_guard<CmImage> convLayerImageLock(*m_imageConvLayer);
                                            m_imageConvLayer->resize(inputWidth, inputHeight, false);
                                            float* convBuffer = m_imageConvLayer->getImageData();
                                            std::copy(cgBinStat.buffer, cgBinStat.buffer + cgBinStat.bufferSize, convBuffer);
                                        }
                                        m_imageConvLayer->moveToGPU();
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
                            setErrorState(stringFormat("Output file \"%s\" could not be opened.", cgOutFilename.c_str()));
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
                                setErrorState(stringFormat(
                                    "Could not retrieve data from the output file, failed at \"%s\".", parseStage.c_str()));
                    }

                    // If successful, copy the output buffer to our conv. layer,
                    // otherwise set error
                    if (!m_state.failed)
                    {
                        if (cgBinOutput.status == 1)
                        {
                            // Prepare the convolution layer
                            std::lock_guard<CmImage> convLayerImageLock(*m_imageConvLayer);
                            m_imageConvLayer->resize(inputWidth, inputHeight, false);
                            m_imageConvLayer->fill(color_t{ 0, 0, 0, 1 }, false);
                            float* convBuffer = m_imageConvLayer->getImageData();
                            uint32_t convBufferSize = m_imageConvLayer->getImageDataSize();

                            if (convBufferSize == cgBinOutput.bufferSize)
                            {
                                for (uint32_t i = 0; i < cgBinOutput.bufferSize; i++)
                                {
                                    convBuffer[i] = cgBinOutput.buffer[i];
                                }
                            } else
                            {
                                setErrorState(stringFormat(
                                    "Output buffer size (%u) does not match the input size (%u).", cgBinOutput.bufferSize, convBufferSize));
                            }
                        } else
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
                if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
                {
                    for (uint32_t i = 0; i < numThreads; i++)
                        DELPTR(m_threads[i]);
                    m_threads.clear();
                }
                DELARR(inputBuffer);
                DELARR(kernelBuffer);

                // Update the conv. layer image
                if (!m_state.failed && !m_state.mustCancel)
                {
                    m_imageConvLayer->moveToGPU();
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
            if (m_state.device.deviceType == ConvolutionDeviceType::CPU)
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

        uint32_t numThreads = m_threads.size();
        if (m_state.failed && !m_state.mustCancel)
        {
            outStatus = m_state.error;
            outStatType = 3;
        } else if (m_state.working && !m_state.mustCancel)
        {
            float elapsedSec = (float)getElapsedMs(m_state.timeStart) / 1000.0f;
            if (m_state.device.deviceType == ConvolutionDeviceType::CPU)
            {
                uint32_t numPixels = 0;
                uint32_t numDone = 0;

                ConvolutionThreadStats* stats = nullptr;
                for (uint32_t i = 0; i < numThreads; i++)
                {
                    if (m_threads[i])
                    {
                        stats = m_threads[i]->getStats();
                        if (stats->state == ConvolutionThreadState::Working || stats->state == ConvolutionThreadState::Done)
                        {
                            numPixels += stats->numPixels;
                            numDone += stats->numDone;
                        }
                    }
                }

                float progress = (numPixels > 0) ? (float)numDone / (float)numPixels : 1.0f;
                float remainingSec = (elapsedSec * (float)(numPixels - numDone)) / fmaxf((float)(numDone), EPSILON);
                outTime = stringFormat(
                    "%.1f%%%% (%u/%u)\n%s / %s",
                    progress * 100.0f,
                    numDone,
                    numPixels,
                    stringFromDuration2(elapsedSec).c_str(),
                    stringFromDuration2(remainingSec).c_str());
            } else
            {
                outTime = stringFormat(
                    "%u/%u chunks\n%s",
                    m_state.numChunksDone,
                    m_state.device.numChunks,
                    stringFromDuration2(elapsedSec));
            }
        } else if (m_state.hasTimestamps && !m_state.mustCancel)
        {
            std::chrono::milliseconds elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_state.timeEnd - m_state.timeStart);
            float elapsedSec = (float)elapsedMs.count() / 1000.0f;
            outTime = stringFormat("Done (%s)", stringFromDuration(elapsedSec).c_str());
        }
    }

    std::string Convolution::getResourceInfo()
    {
        // Kernel Buffer
        float* kernelBuffer = nullptr;
        uint32_t kernelWidth = 0, kernelHeight = 0;
        kernel(false, &kernelBuffer, &kernelWidth, &kernelHeight);
        delete[] kernelBuffer;

        // Input Buffer
        uint32_t inputWidth = m_imageInput->getWidth();
        uint32_t inputHeight = m_imageInput->getHeight();

        // Estimate resource usage
        uint64_t numPixels = 0;
        uint64_t numPixelsPerBlock = 0;
        uint64_t ramUsage = 0;
        uint64_t vramUsage = 0;

        previewThreshold(&numPixels);
        if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
            numPixelsPerBlock = numPixels / m_params.device.numThreads;
        else
            numPixelsPerBlock = numPixels / m_params.device.numChunks;

        uint64_t inputSizeBytes = (uint64_t)inputWidth * (uint64_t)inputHeight * 4 * sizeof(float);
        uint64_t kernelSizeBytes = (uint64_t)kernelWidth * (uint64_t)kernelHeight * 4 * sizeof(float);
        if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
        {
            ramUsage = inputSizeBytes + kernelSizeBytes + (inputSizeBytes * m_params.device.numThreads);
        } else
        {
            // input buffer + kernel buffer + final output buffer + output buffer for chunk
            // + (numPoints * 5) + fbData (same size as input buffer)
            ramUsage = (inputSizeBytes * 4) + kernelSizeBytes + (numPixelsPerBlock * 5 * sizeof(float));

            // (numPoints * 5) + kernel buffer + frame buffer (same size as input buffer)
            vramUsage = (numPixelsPerBlock * 5 * sizeof(float)) + kernelSizeBytes + inputSizeBytes;
        }

        if (m_params.device.deviceType == ConvolutionDeviceType::CPU)
        {
            return stringFormat(
                "Total Pixels: %s\nPixels/Thread: %s\nEst. Memory: %s",
                stringFromBigNumber(numPixels).c_str(),
                stringFromBigNumber(numPixelsPerBlock).c_str(),
                stringFromSize(ramUsage).c_str());
        } else
        {
            return stringFormat(
                "Total Pixels: %s\nPixels/Chunk: %s\nEst. Memory: %s\nEst. VRAM: %s",
                stringFromBigNumber(numPixels).c_str(),
                stringFromBigNumber(numPixelsPerBlock).c_str(),
                stringFromSize(ramUsage).c_str(),
                stringFromSize(vramUsage).c_str());
        }
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

}