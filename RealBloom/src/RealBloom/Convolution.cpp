#include "Convolution.h"
#include "ConvolutionThread.h"
#include "ConvolutionFFT.h"

#include <omp.h>

namespace RealBloom
{

    static constexpr uint32_t CONV_PROG_TIMESTEP = 1000;

    void drawRect(CmImage* image, int rx, int ry, int rw, int rh);
    void fillRect(CmImage* image, int rx, int ry, int rw, int rh);

    uint32_t ConvolutionStatus::getNumChunksDone() const
    {
        return m_numChunksDone;
    }

    void ConvolutionStatus::setNumChunksDone(uint32_t numChunksDone)
    {
        m_numChunksDone = numChunksDone;
    }

    const std::string& ConvolutionStatus::getFftStage() const
    {
        return m_fftStage;
    }

    void ConvolutionStatus::setFftStage(const std::string& fftStage)
    {
        m_fftStage = fftStage;
    }

    void ConvolutionStatus::reset()
    {
        super::reset();
        m_numChunksDone = 0;
        m_fftStage = "";
    }

    Convolution::Convolution()
    {}

    ConvolutionParams* Convolution::getParams()
    {
        return &m_params;
    }

    CmImage* Convolution::getImgInputSrc()
    {
        return &m_imgInputSrc;
    }

    void Convolution::setImgInput(CmImage* image)
    {
        m_imgInput = image;
    }

    CmImage* Convolution::getImgKernelSrc()
    {
        return &m_imgKernelSrc;
    }

    void Convolution::setImgKernel(CmImage* image)
    {
        m_imgKernel = image;
    }

    void Convolution::setImgConvPreview(CmImage* image)
    {
        m_imgConvPreview = image;
    }

    void Convolution::setImgConvResult(CmImage* image)
    {
        m_imgConvResult = image;
    }

    void Convolution::previewThreshold(size_t* outNumPixels)
    {
        float threshold = m_params.threshold;
        float transKnee = transformKnee(m_params.knee);
        std::atomic_uint64_t numPixels = 0;

        {
            // Input image
            std::scoped_lock lock1(*m_imgInput);
            float* inputBuffer = m_imgInput->getImageData();
            uint32_t inputWidth = m_imgInput->getWidth();
            uint32_t inputHeight = m_imgInput->getHeight();

            // Convolution Preview Image
            std::scoped_lock lock2(*m_imgConvPreview);
            m_imgConvPreview->resize(inputWidth, inputHeight, false);
            float* prevBuffer = m_imgConvPreview->getImageData();

#pragma omp parallel for
            for (int y = 0; y < inputHeight; y++)
            {
                for (int x = 0; x < inputWidth; x++)
                {
                    uint32_t redIndex = (y * inputWidth + x) * 4;
                    prevBuffer[redIndex + 0] = 0;
                    prevBuffer[redIndex + 1] = 0;
                    prevBuffer[redIndex + 2] = 0;
                    prevBuffer[redIndex + 3] = 1;

                    float v = rgbaToGrayscale(&inputBuffer[redIndex], CONV_THRESHOLD_GRAYSCALE_TYPE);
                    if (v > threshold)
                    {
                        numPixels++;
                        float mul = softThreshold(v, threshold, transKnee);
                        blendAddRGB(prevBuffer, redIndex, inputBuffer, redIndex, mul);
                    }
                }
            }
        }
        m_imgConvPreview->moveToGPU();

        if (outNumPixels)
            *outNumPixels = numPixels;
    }

    void Convolution::previewInput(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        processInputImage(previewMode, m_params.inputTransformParams, m_imgInputSrc, *m_imgInput, outBuffer, outWidth, outHeight);
    }

    void Convolution::previewKernel(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        // Return the output dimensions if requested
        if (!previewMode && !outBuffer)
        {
            uint32_t transWidth, transHeight;
            ImageTransform::getOutputDimensions(m_params.kernelTransformParams, m_imgKernelSrc.getWidth(), m_imgKernelSrc.getHeight(), transWidth, transHeight);
            *outWidth = transWidth;
            *outHeight = transHeight;
            return;
        }

        processInputImage(previewMode, m_params.kernelTransformParams, m_imgKernelSrc, *m_imgKernel, outBuffer, outWidth, outHeight);

        bool outerRequest = (!previewMode && outBuffer && outWidth && outHeight);

        // Auto-adjust the exposure
        if (m_params.autoExposure && outerRequest)
        {
            // Get the sum of the grayscale values
            float sumV = 0.0f;
            for (uint32_t y = 0; y < *outHeight; y++)
            {
                for (uint32_t x = 0; x < *outWidth; x++)
                {
                    uint32_t redIndex = (y * *outWidth + x) * 4;

                    float grayscale = rgbaToGrayscale(&(*outBuffer)[redIndex], GrayscaleType::Average);
                    sumV += grayscale;
                }
            }

            // Divide by the sum, and cancel out the convolution multiplier
            if (sumV != 0.0f)
            {
                float mul = 1.0 / ((double)sumV * (double)CONV_MULTIPLIER);
                for (uint32_t i = 0; i < (*outBuffer).size(); i++)
                {
                    if (i % 4 != 3)
                        (*outBuffer)[i] *= mul;
                }
            }
        }
    }

    void Convolution::blend()
    {
        float blendInput = fmaxf(m_params.blendInput, 0.0f);
        float blendConv = fmaxf(m_params.blendConv, 0.0f);
        float blendMix = 0.0f;
        if (!m_params.blendAdditive)
        {
            blendMix = fminf(fmaxf(m_params.blendMix, 0.0f), 1.0f);
            blendConv = blendMix;
            blendInput = 1.0f - blendMix;
        }

        float blendExposure = m_params.blendExposure;

        {
            // Input buffer
            std::scoped_lock lock1(m_imgInputCaptured);
            float* inputBuffer = m_imgInputCaptured.getImageData();
            uint32_t inputWidth = m_imgInputCaptured.getWidth();
            uint32_t inputHeight = m_imgInputCaptured.getHeight();

            // Conv. Buffer
            std::scoped_lock lock2(m_imgOutput);
            float* convBuffer = m_imgOutput.getImageData();
            uint32_t convWidth = m_imgOutput.getWidth();
            uint32_t convHeight = m_imgOutput.getHeight();

            if ((inputWidth == convWidth) && (inputHeight == convHeight))
            {
                // Conv. Result Buffer
                std::scoped_lock lock3(*m_imgConvResult);
                m_imgConvResult->resize(inputWidth, inputHeight, false);
                float* convResultBuffer = m_imgConvResult->getImageData();

                float mul = blendConv * getExposureMul(blendExposure);
                for (uint32_t y = 0; y < inputHeight; y++)
                {
                    for (uint32_t x = 0; x < inputWidth; x++)
                    {
                        uint32_t redIndex = (y * inputWidth + x) * 4;

                        convResultBuffer[redIndex + 0] =
                            (inputBuffer[redIndex + 0] * blendInput) + (convBuffer[redIndex + 0] * mul);
                        convResultBuffer[redIndex + 1] =
                            (inputBuffer[redIndex + 1] * blendInput) + (convBuffer[redIndex + 1] * mul);
                        convResultBuffer[redIndex + 2] =
                            (inputBuffer[redIndex + 2] * blendInput) + (convBuffer[redIndex + 2] * mul);

                        convResultBuffer[redIndex + 3] = 1;
                    }
                }
            }
        }
        m_imgConvResult->moveToGPU();
    }

    void Convolution::convolve()
    {
        // If there's already a convolution process going on
        cancel();

        // Capture the parameters to avoid changes during the process
        m_capturedParams = m_params;

        // Reset the status
        m_status.reset();
        m_status.setWorking();
        m_status.setFftStage("Initializing");

        // Start the main thread
        m_thread = std::make_shared<std::jthread>([this]()
            {
                // Input buffer
                std::vector<float> inputBuffer;
                uint32_t inputWidth = 0, inputHeight = 0;
                previewInput(false, &inputBuffer, &inputWidth, &inputHeight);
                uint32_t inputBufferSize = inputWidth * inputHeight * 4;

                // Kernel buffer
                std::vector<float> kernelBuffer;
                uint32_t kernelWidth = 0, kernelHeight = 0;
                previewKernel(false, &kernelBuffer, &kernelWidth, &kernelHeight);
                uint32_t kernelBufferSize = kernelWidth * kernelHeight * 4;

                // Call the appropriate function
                switch (m_capturedParams.methodInfo.method)
                {
                case RealBloom::ConvolutionMethod::FFT_CPU:
                    convFftCPU(
                        kernelBuffer, kernelWidth, kernelHeight,
                        inputBuffer, inputWidth, inputHeight, inputBufferSize);
                    break;
                case RealBloom::ConvolutionMethod::FFT_GPU:
                    convFftGPU(
                        kernelBuffer, kernelWidth, kernelHeight,
                        inputBuffer, inputWidth, inputHeight, inputBufferSize);
                    break;
                case RealBloom::ConvolutionMethod::NAIVE_CPU:
                    convNaiveCPU(
                        kernelBuffer, kernelWidth, kernelHeight,
                        inputBuffer, inputWidth, inputHeight, inputBufferSize);
                    break;
                case RealBloom::ConvolutionMethod::NAIVE_GPU:
                    convNaiveGPU(
                        kernelBuffer, kernelWidth, kernelHeight,
                        inputBuffer, inputWidth, inputHeight, inputBufferSize);
                    break;
                default:
                    break;
                }

                // Update the captured input image, used for blending
                {
                    std::scoped_lock lock(m_imgInputCaptured);
                    m_imgInputCaptured.resize(inputWidth, inputHeight, false);
                    float* buffer = m_imgInputCaptured.getImageData();
                    std::copy(inputBuffer.data(), inputBuffer.data() + inputBufferSize, buffer);
                }

                // Update convBlendParamsChanged
                if (m_status.isOK() && !m_status.mustCancel())
                    Async::emitSignal("convBlendParamsChanged", nullptr);

                m_status.setDone();
            }
        );
    }

    void Convolution::cancel()
    {
        if (!m_status.isWorking())
        {
            m_status.reset();
            return;
        }

        m_status.setMustCancel();

        if (m_capturedParams.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
        {
            // Tell the sub-threads to stop
            for (auto& ct : m_cpuThreads)
                ct->stop();
        }

        // Wait for the main thread
        threadJoin(m_thread.get());
        m_thread = nullptr;

        m_status.reset();
    }

    const ConvolutionStatus& Convolution::getStatus() const
    {
        return m_status;
    }

    void Convolution::getStatusText(std::string& outStatus, std::string& outMessage, uint32_t& outMessageType) const
    {
        outStatus = "";
        outMessage = "";
        outMessageType = 0;

        if (m_status.mustCancel())
            return;

        if (!m_status.isOK())
        {
            outMessage = m_status.getError();
            outMessageType = 3;
        }
        else if (m_status.isWorking())
        {
            float elapsedSec = m_status.getElapsedSec();
            if (m_capturedParams.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
            {
                uint32_t numThreads = m_cpuThreads.size();
                uint32_t numPixels = 0;
                uint32_t numDone = 0;

                for (uint32_t i = 0; i < numThreads; i++)
                {
                    ConvolutionThreadStats* stats = m_cpuThreads[i]->getStats();
                    if (stats->state == ConvolutionThreadState::Working || stats->state == ConvolutionThreadState::Done)
                    {
                        numPixels += stats->numPixels;
                        numDone += stats->numDone;
                    }
                }

                float progress = (numPixels > 0) ? (float)numDone / (float)numPixels : 1.0f;
                float remainingSec = (elapsedSec * (float)(numPixels - numDone)) / fmaxf((float)(numDone), EPSILON);
                outStatus = strFormat(
                    "%.1f%%%% (%u/%u)\n%s / %s",
                    progress * 100.0f,
                    numDone,
                    numPixels,
                    strFromElapsed(elapsedSec).c_str(),
                    strFromElapsed(remainingSec).c_str());
            }
            else if (m_capturedParams.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
            {
                outStatus = strFormat(
                    "%u/%u chunks\n%s",
                    m_status.getNumChunksDone(),
                    m_capturedParams.methodInfo.NAIVE_GPU_numChunks,
                    strFromElapsed(elapsedSec).c_str());
            }
            else if (m_capturedParams.methodInfo.method == ConvolutionMethod::FFT_CPU)
            {
                outStatus = strFormat(
                    "%s\n%s",
                    m_status.getFftStage().c_str(),
                    strFromElapsed(elapsedSec).c_str());
            }
            else if (m_capturedParams.methodInfo.method == ConvolutionMethod::FFT_GPU)
            {
                outStatus = strFromElapsed(elapsedSec).c_str();
            }
        }
        else if (m_status.hasTimestamps())
        {
            float elapsedSec = m_status.getElapsedSec();
            outStatus = strFormat("Done (%s)", strFromDuration(elapsedSec).c_str());
        }
    }

    std::string Convolution::getResourceInfo()
    {
        // Kernel Size
        uint32_t kernelWidth = 0, kernelHeight = 0;
        previewKernel(false, nullptr, &kernelWidth, &kernelHeight);

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
            numPixelsPerBlock = numPixels / m_params.methodInfo.NAIVE_CPU_numThreads;
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
            numPixelsPerBlock = numPixels / m_params.methodInfo.NAIVE_GPU_numChunks;

        // Input and kernel size in bytes
        uint64_t inputSizeBytes = (uint64_t)inputWidth * (uint64_t)inputHeight * 4 * sizeof(float);
        uint64_t kernelSizeBytes = (uint64_t)kernelWidth * (uint64_t)kernelHeight * 4 * sizeof(float);

        // Calculate memory usage for different methods
        if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
        {
            std::array<float, 2> kernelOrigin = getKernelOrigin(m_params);
            uint32_t paddedWidth, paddedHeight;
            calcFftConvPadding(
                false, false,
                inputWidth, inputHeight,
                kernelWidth, kernelHeight,
                kernelOrigin[0], kernelOrigin[1],
                paddedWidth, paddedHeight);

            ramUsage = inputSizeBytes + kernelSizeBytes + ((uint64_t)paddedWidth * (uint64_t)paddedHeight * 10 * sizeof(float));
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
        {
            ramUsage = inputSizeBytes + kernelSizeBytes + (inputSizeBytes * m_params.methodInfo.NAIVE_CPU_numThreads);
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
        {
            // input buffer + kernel buffer + final output buffer + output buffer for chunk
            // + (numPoints * 5) + fbData (same size as input buffer)
            ramUsage = (inputSizeBytes * 4) + kernelSizeBytes + (numPixelsPerBlock * 5 * sizeof(float));

            // (numPoints * 5) + kernel buffer + framebuffer (same size as input buffer)
            vramUsage = (numPixelsPerBlock * 5 * sizeof(float)) + kernelSizeBytes + inputSizeBytes;
        }

        // Format output
        if (m_params.methodInfo.method == ConvolutionMethod::FFT_CPU)
        {
            return strFormat(
                "Est. Memory: %s",
                strFromDataSize(ramUsage).c_str());
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::FFT_GPU)
        {
            return "Undetermined";
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_CPU)
        {
            return strFormat(
                "Total Pixels: %s\nPixels/Thread: %s\nEst. Memory: %s",
                strFromBigInteger(numPixels).c_str(),
                strFromBigInteger(numPixelsPerBlock).c_str(),
                strFromDataSize(ramUsage).c_str());
        }
        else if (m_params.methodInfo.method == ConvolutionMethod::NAIVE_GPU)
        {
            return strFormat(
                "Total Pixels: %s\nPixels/Chunk: %s\nEst. Memory: %s\nEst. VRAM: %s",
                strFromBigInteger(numPixels).c_str(),
                strFromBigInteger(numPixelsPerBlock).c_str(),
                strFromDataSize(ramUsage).c_str(),
                strFromDataSize(vramUsage).c_str());
        }

        return "";
    }

    std::array<float, 2> Convolution::getKernelOrigin(const ConvolutionParams& params)
    {
        if (params.useKernelTransformOrigin)
            return params.kernelTransformParams.transform.origin;
        else
            return { 0.5f, 0.5f };
    }

    void Convolution::convFftCPU(
        std::vector<float>& kernelBuffer,
        uint32_t kernelWidth,
        uint32_t kernelHeight,
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize)
    {
        try
        {
            // FFT
            ConvolutionFFT fftConv(
                m_capturedParams, inputBuffer.data(), inputWidth, inputHeight,
                kernelBuffer.data(), kernelWidth, kernelHeight
            );

            uint32_t numStages = 14;
            uint32_t currStage = 0;

            // Prepare padded buffers
            currStage++;
            m_status.setFftStage(strFormat("%u/%u Preparing", currStage, numStages));
            fftConv.pad();

            if (m_status.mustCancel()) throw std::exception();

            // Repeat for 3 color channels
            for (uint32_t i = 0; i < 3; i++)
            {
                // Input FFT
                currStage++;
                m_status.setFftStage(strFormat("%u/%u %s: Input FFT", currStage, numStages, strFromColorChannelID(i).c_str()));
                fftConv.inputFFT(i);

                if (m_status.mustCancel()) throw std::exception();

                // Kernel FFT
                currStage++;
                m_status.setFftStage(strFormat("%u/%u %s: Kernel FFT", currStage, numStages, strFromColorChannelID(i).c_str()));
                fftConv.kernelFFT(i);

                if (m_status.mustCancel()) throw std::exception();

                // Define the name of the arithmetic operation based on deconvolve
                std::string arithmeticName =
                    m_capturedParams.methodInfo.FFT_CPU_deconvolve
                    ? "Dividing"
                    : "Multiplying";

                // Multiply/Divide the Fourier transforms
                currStage++;
                m_status.setFftStage(strFormat("%u/%u %s: %s", currStage, numStages, strFromColorChannelID(i).c_str(), arithmeticName.c_str()));
                fftConv.multiplyOrDivide(i);

                if (m_status.mustCancel()) throw std::exception();

                // Inverse FFT
                currStage++;
                m_status.setFftStage(strFormat("%u/%u %s: Inverse FFT", currStage, numStages, strFromColorChannelID(i).c_str()));
                fftConv.inverse(i);

                if (m_status.mustCancel()) throw std::exception();
            }

            // Get the final output
            currStage++;
            m_status.setFftStage(strFormat("%u/%u Finalizing", currStage, numStages));
            fftConv.output();

            // Update the output image
            {
                std::scoped_lock lock(m_imgOutput);
                m_imgOutput.resize(inputWidth, inputHeight, false);
                float* convBuffer = m_imgOutput.getImageData();
                std::copy(
                    fftConv.getBuffer().data(),
                    fftConv.getBuffer().data() + fftConv.getBuffer().size(),
                    convBuffer
                );
            }
        }
        catch (const std::exception& e)
        {
            m_status.setError(e.what());
        }
    }

    void Convolution::convFftGPU(
        std::vector<float>& kernelBuffer,
        uint32_t kernelWidth,
        uint32_t kernelHeight,
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize)
    {
        // Create GpuHelper instance
        GpuHelper gpuHelper;
        std::string inpFilename = gpuHelper.getInpFilename();
        std::string outFilename = gpuHelper.getOutFilename();

        try
        {
            std::array<float, 2> kernelOrigin = getKernelOrigin(m_capturedParams);

            // Prepare input data
            BinaryConvFftGpuInput binInput;
            binInput.cp_kernelOriginX = kernelOrigin[0];
            binInput.cp_kernelOriginY = kernelOrigin[1];
            binInput.cp_convThreshold = m_capturedParams.threshold;
            binInput.cp_convKnee = m_capturedParams.knee;
            binInput.convMultiplier = CONV_MULTIPLIER;
            binInput.inputWidth = inputWidth;
            binInput.inputHeight = inputHeight;
            binInput.kernelWidth = kernelWidth;
            binInput.kernelHeight = kernelHeight;
            binInput.inputBuffer = inputBuffer;
            binInput.kernelBuffer = kernelBuffer;

            // Create the input file
            std::ofstream inpFile;
            inpFile.open(inpFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            if (!inpFile.is_open())
                throw std::exception(
                    strFormat("Input file \"%s\" could not be created/opened.", inpFilename.c_str()).c_str()
                );

            // Write operation type
            uint32_t opType = (uint32_t)GpuHelperOperationType::ConvFFT;
            stmWriteScalar(inpFile, opType);

            // Write input data
            binInput.writeTo(inpFile);

            // Close the input file
            inpFile.flush();
            inpFile.close();

            // Execute GPU Helper
            gpuHelper.run();

            // Wait for the output file to be created
            gpuHelper.waitForOutput([this]() { return m_status.mustCancel(); });

            // Wait for the GPU Helper to finish its job
            while (gpuHelper.isRunning())
            {
                if (m_status.mustCancel())
                    throw std::exception();
                std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP_SHORT));
            }

            // Open the output file
            std::ifstream outFile;
            outFile.open(outFilename, std::ifstream::in | std::ifstream::binary);
            if (!outFile.is_open())
                throw std::exception(
                    strFormat("Output file \"%s\" could not be opened.", outFilename.c_str()).c_str()
                );
            else
                outFile.seekg(std::ifstream::beg);

            // Parse the output file
            BinaryConvFftGpuOutput binOutput;
            binOutput.readFrom(outFile);
            if (binOutput.status == 1)
            {
                // Update the output image

                std::scoped_lock lock(m_imgOutput);
                m_imgOutput.resize(inputWidth, inputHeight, false);
                float* convBuffer = m_imgOutput.getImageData();
                uint32_t convBufferSize = m_imgOutput.getImageDataSize();

                if (convBufferSize == binOutput.buffer.size())
                {
                    std::copy(binOutput.buffer.data(), binOutput.buffer.data() + binOutput.buffer.size(), convBuffer);
                }
                else
                {
                    m_status.setError(strFormat(
                        "Output buffer size (%u) does not match the input buffer size (%u).",
                        binOutput.buffer.size(), convBufferSize
                    ));
                }
            }
            else
            {
                m_status.setError(binOutput.error);
            }

            // Close the output file
            outFile.close();
        }
        catch (const std::exception& e)
        {
            m_status.setError(e.what());
        }

        // Clean up
        gpuHelper.cleanUp();
    }

    void Convolution::convNaiveCPU(
        std::vector<float>& kernelBuffer,
        uint32_t kernelWidth,
        uint32_t kernelHeight,
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize)
    {
        // Clamp the number of threads
        uint32_t maxThreads = std::thread::hardware_concurrency();
        uint32_t numThreads = m_capturedParams.methodInfo.NAIVE_CPU_numThreads;
        if (numThreads > maxThreads) numThreads = maxThreads;
        if (numThreads < 1) numThreads = 1;
        m_capturedParams.methodInfo.NAIVE_CPU_numThreads = numThreads;

        // Create and prepare threads
        for (uint32_t i = 0; i < numThreads; i++)
        {
            std::shared_ptr<ConvolutionThread> ct = std::make_shared<ConvolutionThread>(
                numThreads, i, m_capturedParams,
                inputBuffer.data(), inputWidth, inputHeight,
                kernelBuffer.data(), kernelWidth, kernelHeight);

            std::vector<float>& threadBuffer = ct->getBuffer();
            threadBuffer.resize(inputBufferSize);
            for (uint32_t j = 0; j < inputBufferSize; j++)
                threadBuffer[j] = (j % 4 == 3) ? 1.0f : 0.0f;

            m_cpuThreads.push_back(ct);
        }

        // Start the threads
        for (auto& ct : m_cpuThreads)
        {
            std::shared_ptr<std::jthread> t = std::make_shared<std::jthread>(
                [ct]()
                {
                    ct->start();
                }
            );
            ct->setThread(t);
        }

        // Wait for the threads
        std::chrono::time_point<std::chrono::system_clock> lastProgTime = std::chrono::system_clock::now();
        while (true)
        {
            uint32_t numThreadsDone = 0;
            ConvolutionThread* ct;
            ConvolutionThreadStats* stats;

            for (auto& ct : m_cpuThreads)
            {
                stats = ct->getStats();
                if (stats->state == ConvolutionThreadState::Done)
                    numThreadsDone++;
            }

            // Keep in mind that the threads will be "Done" when mustCancel is true
            if (numThreadsDone >= numThreads)
                break;

            // Take a snapshot of the current progress

            bool mustUpdateProg =
                (!m_status.mustCancel())
                && (getElapsedMs(lastProgTime) > CONV_PROG_TIMESTEP)
                && (numThreadsDone < numThreads);

            if (mustUpdateProg)
            {
                std::vector<float> progBuffer;
                progBuffer.resize(inputBufferSize);
                for (uint32_t i = 0; i < inputBufferSize; i++)
                    progBuffer[i] = (i % 4 == 3) ? 1.0f : 0.0f;

                for (auto& ct : m_cpuThreads)
                {
                    std::vector<float>& currentBuffer = ct->getBuffer();
                    for (uint32_t y = 0; y < inputHeight; y++)
                    {
                        for (uint32_t x = 0; x < inputWidth; x++)
                        {
                            uint32_t redIndex = (y * inputWidth + x) * 4;
                            progBuffer[redIndex + 0] += currentBuffer[redIndex + 0] * CONV_MULTIPLIER;
                            progBuffer[redIndex + 1] += currentBuffer[redIndex + 1] * CONV_MULTIPLIER;
                            progBuffer[redIndex + 2] += currentBuffer[redIndex + 2] * CONV_MULTIPLIER;
                        }
                    }
                }

                // Update Conv. Result
                {
                    std::scoped_lock lock(*m_imgConvResult);
                    m_imgConvResult->resize(inputWidth, inputHeight, false);
                    float* convResultBuffer = m_imgConvResult->getImageData();
                    std::copy(progBuffer.data(), progBuffer.data() + inputBufferSize, convResultBuffer);
                }
                m_imgConvResult->moveToGPU();

                lastProgTime = std::chrono::system_clock::now();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP_SHORT));
        }

        // Join the threads
        for (auto& ct : m_cpuThreads)
            threadJoin(ct->getThread().get());

        // Update the output image
        {
            std::scoped_lock lock(m_imgOutput);
            m_imgOutput.resize(inputWidth, inputHeight, false);
            m_imgOutput.fill(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }, false);
            float* convBuffer = m_imgOutput.getImageData();

            // Add the buffers from each thread
            for (auto& ct : m_cpuThreads)
            {
                std::vector<float>& currentBuffer = ct->getBuffer();
                for (uint32_t y = 0; y < inputHeight; y++)
                {
                    for (uint32_t x = 0; x < inputWidth; x++)
                    {
                        uint32_t redIndex = (y * inputWidth + x) * 4;
                        convBuffer[redIndex + 0] += currentBuffer[redIndex + 0] * CONV_MULTIPLIER;
                        convBuffer[redIndex + 1] += currentBuffer[redIndex + 1] * CONV_MULTIPLIER;
                        convBuffer[redIndex + 2] += currentBuffer[redIndex + 2] * CONV_MULTIPLIER;
                    }
                }
            }
        }

        // Clean up
        clearVector(m_cpuThreads);
    }

    void Convolution::convNaiveGPU(
        std::vector<float>& kernelBuffer,
        uint32_t kernelWidth,
        uint32_t kernelHeight,
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize)
    {
        // Clamp numChunks

        if (m_capturedParams.methodInfo.NAIVE_GPU_numChunks < 1)
            m_capturedParams.methodInfo.NAIVE_GPU_numChunks = 1;

        if (m_capturedParams.methodInfo.NAIVE_GPU_numChunks > CONV_NAIVE_GPU_MAX_CHUNKS)
            m_capturedParams.methodInfo.NAIVE_GPU_numChunks = CONV_NAIVE_GPU_MAX_CHUNKS;

        uint32_t numChunks = m_capturedParams.methodInfo.NAIVE_GPU_numChunks;

        // Clamp chunkSleep

        if (m_capturedParams.methodInfo.NAIVE_GPU_chunkSleep > CONV_NAIVE_GPU_MAX_SLEEP)
            m_capturedParams.methodInfo.NAIVE_GPU_chunkSleep = CONV_NAIVE_GPU_MAX_SLEEP;

        uint32_t chunkSleep = m_capturedParams.methodInfo.NAIVE_GPU_chunkSleep;

        // Create GpuHelper instance
        GpuHelper gpuHelper;
        std::string inpFilename = gpuHelper.getInpFilename();
        std::string outFilename = gpuHelper.getOutFilename();
        std::string statFilename = inpFilename + "stat";
        HANDLE statMutex = NULL;

        try
        {
            std::array<float, 2> kernelOrigin = getKernelOrigin(m_capturedParams);

            // Prepare input data
            BinaryConvNaiveGpuInput binInput;
            binInput.numChunks = numChunks;
            binInput.chunkSleep = chunkSleep;
            binInput.cp_kernelOriginX = kernelOrigin[0];
            binInput.cp_kernelOriginY = kernelOrigin[1];
            binInput.cp_convThreshold = m_capturedParams.threshold;
            binInput.cp_convKnee = m_capturedParams.knee;
            binInput.convMultiplier = CONV_MULTIPLIER;
            binInput.inputWidth = inputWidth;
            binInput.inputHeight = inputHeight;
            binInput.kernelWidth = kernelWidth;
            binInput.kernelHeight = kernelHeight;
            binInput.inputBuffer = inputBuffer;
            binInput.kernelBuffer = kernelBuffer;

            // Mutex for IO operations on the stat file

            std::string statMutexName = "GlobalRbStatMutex" + std::to_string(gpuHelper.getRandomNumber());
            binInput.statMutexName = statMutexName;

            statMutex = createMutex(statMutexName);
            if (statMutex == NULL)
                throw std::exception(
                    strFormat("Mutex \"%s\" could not be created.", statMutexName.c_str()).c_str()
                );

            // Create the input file
            std::ofstream inpFile;
            inpFile.open(inpFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            if (!inpFile.is_open())
                throw std::exception(
                    strFormat("Input file \"%s\" could not be created/opened.", inpFilename.c_str()).c_str()
                );

            // Write operation type
            uint32_t opType = (uint32_t)GpuHelperOperationType::ConvNaive;
            stmWriteScalar(inpFile, opType);

            // Write input data
            binInput.writeTo(inpFile);

            // Close the input file
            inpFile.flush();
            inpFile.close();

            // Execute GPU Helper
            gpuHelper.run();

            // Wait for the output file to be created
            gpuHelper.waitForOutput([this]() { return m_status.mustCancel(); });

            // Wait for the GPU Helper to finish its job
            auto lastProgTime = std::chrono::system_clock::now();
            while (gpuHelper.isRunning())
            {
                if (m_status.mustCancel())
                    throw std::exception();

                // Read the stat file

                bool mustReadStat =
                    !(m_status.mustCancel())
                    && std::filesystem::exists(statFilename)
                    && (getElapsedMs(lastProgTime) > CONV_PROG_TIMESTEP);

                if (mustReadStat)
                {
                    BinaryConvNaiveGpuStat binStat;
                    waitForMutex(statMutex);

                    // Open and read the stat file
                    std::ifstream statFile;
                    statFile.open(statFilename, std::ifstream::in | std::ifstream::binary);
                    if (statFile.is_open())
                    {
                        statFile.seekg(std::ifstream::beg);
                        try
                        {
                            binStat.readFrom(statFile);

                            // If numChunksDone has increased
                            if (binStat.numChunksDone > m_status.getNumChunksDone())
                            {
                                m_status.setNumChunksDone(binStat.numChunksDone);

                                // Update Conv. Result
                                if (binStat.buffer.size() == (inputWidth * inputHeight * 4))
                                {
                                    std::scoped_lock lock(*m_imgConvResult);
                                    m_imgConvResult->resize(inputWidth, inputHeight, false);
                                    float* convResultBuffer = m_imgConvResult->getImageData();
                                    std::copy(binStat.buffer.data(), binStat.buffer.data() + binStat.buffer.size(), convResultBuffer);
                                }
                                m_imgConvResult->moveToGPU();
                            }
                        }
                        catch (const std::exception& e)
                        {
                            printError(__FUNCTION__, "stat", e.what());
                        }
                        statFile.close();
                    }
                    else
                    {
                        printError(__FUNCTION__, "stat", strFormat("Stat file \"%s\" could not be opened.", statFilename.c_str()));
                    }

                    releaseMutex(statMutex);
                    lastProgTime = std::chrono::system_clock::now();
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_TIMESTEP_SHORT));
            }

            // No longer need the mutex
            closeMutex(statMutex);

            // Open the output file
            std::ifstream outFile;
            outFile.open(outFilename, std::ifstream::in | std::ifstream::binary);
            if (!outFile.is_open())
                throw std::exception(
                    strFormat("Output file \"%s\" could not be opened.", outFilename.c_str()).c_str()
                );
            else
                outFile.seekg(std::ifstream::beg);

            // Parse the output file
            BinaryConvNaiveGpuOutput binOutput;
            binOutput.readFrom(outFile);
            if (binOutput.status == 1)
            {
                // Update the output image

                std::scoped_lock lock(m_imgOutput);
                m_imgOutput.resize(inputWidth, inputHeight, false);
                float* convBuffer = m_imgOutput.getImageData();
                uint32_t convBufferSize = m_imgOutput.getImageDataSize();

                if (convBufferSize == binOutput.buffer.size())
                {
                    std::copy(binOutput.buffer.data(), binOutput.buffer.data() + binOutput.buffer.size(), convBuffer);
                }
                else
                {
                    m_status.setError(strFormat(
                        "Output buffer size (%u) does not match the input buffer size (%u).",
                        binOutput.buffer.size(), convBufferSize
                    ));
                }
            }
            else
            {
                m_status.setError(binOutput.error);
            }

            // Close the output file
            outFile.close();
        }
        catch (const std::exception& e)
        {
            m_status.setError(e.what());
        }

        // Clean up
        gpuHelper.cleanUp();
        closeMutex(statMutex);
        if (GPU_HELPER_DELETE_TEMP)
        {
            deleteFile(statFilename);
        }
    }

    void drawRect(CmImage* image, int rx, int ry, int rw, int rh)
    {
        std::scoped_lock lock(*image);
        float* buffer = image->getImageData();
        uint32_t imageWidth = image->getWidth();
        uint32_t imageHeight = image->getHeight();

        int rx2 = rx + rw;
        int ry2 = ry + rh;

        uint32_t redIndex;
        for (int y = ry; y < ry2; y++)
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
        for (int x = rx; x < rx2; x++)
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
        std::scoped_lock lock(*image);
        float* buffer = image->getImageData();
        uint32_t imageWidth = image->getWidth();
        uint32_t imageHeight = image->getHeight();

        int rx2 = rx + rw;
        int ry2 = ry + rh;

        for (int y = ry; y < ry2; y++)
        {
            for (int x = rx; x < rx2; x++)
            {
                if (checkBounds(x, y, imageWidth, imageHeight))
                {
                    uint32_t redIndex = (y * imageWidth + x) * 4;
                    buffer[redIndex + 0] = 0;
                    buffer[redIndex + 1] = 0;
                    buffer[redIndex + 2] = 1;
                    buffer[redIndex + 3] = 1;
                }
            }
        }
    }

}
