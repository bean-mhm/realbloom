#include "Dispersion.h"
#include "DispersionThread.h"

namespace RealBloom
{

    static constexpr uint32_t DISP_PROG_TIMESTEP = 33;

    Dispersion::Dispersion()
    {}

    DispersionParams* Dispersion::getParams()
    {
        return &m_params;
    }

    CmImage* Dispersion::getImgInputSrc()
    {
        return &m_imgInputSrc;
    }

    void Dispersion::setImgInput(CmImage* image)
    {
        m_imgInput = image;
    }

    void Dispersion::setImgDisp(CmImage* image)
    {
        m_imgDisp = image;
    }

    void Dispersion::previewCmf(CmfTable* table)
    {
        try
        {
            if (table == nullptr)
                throw std::exception("table was null.");

            uint32_t pWidth = std::max(2u, (uint32_t)table->getCount() * 2);
            uint32_t pHeight = std::max(2u, pWidth / 10);

            std::vector<float> buffer;
            buffer.resize(pWidth * pHeight * 4);

            // Get wavelength samples
            std::vector<float> samples;
            table->sampleRGB(pWidth, true, samples);

            // Copy to buffer
            uint32_t redIndex, smpIndex;
            for (uint32_t y = 0; y < pHeight; y++)
            {
                for (uint32_t x = 0; x < pWidth; x++)
                {
                    redIndex = (y * pWidth + x) * 4;
                    smpIndex = x * 3;

                    buffer[redIndex + 0] = samples[smpIndex + 0];
                    buffer[redIndex + 1] = samples[smpIndex + 1];
                    buffer[redIndex + 2] = samples[smpIndex + 2];
                    buffer[redIndex + 3] = 1.0f;
                }
            }

            // Copy to image
            {
                std::scoped_lock lock(*m_imgDisp);
                m_imgDisp->resize(pWidth, pHeight, false);
                float* imageBuffer = m_imgDisp->getImageData();
                std::copy(buffer.data(), buffer.data() + buffer.size(), imageBuffer);
            }
            m_imgDisp->moveToGPU();
        }
        catch (const std::exception& e)
        {
            throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
        }
    }

    void Dispersion::previewInput(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        processInputImage(previewMode, m_params.inputTransformParams, m_imgInputSrc, *m_imgInput, outBuffer, outWidth, outHeight);
    }

    void Dispersion::compute()
    {
        // If there's already a dispersion process going on
        cancel();

        // Capture the parameters to avoid changes during the process
        m_capturedParams = m_params;

        // Clamp the number of steps
        if (m_capturedParams.steps > DISP_MAX_STEPS) m_capturedParams.steps = DISP_MAX_STEPS;
        if (m_capturedParams.steps < 1) m_capturedParams.steps = 1;
        uint32_t dispSteps = m_capturedParams.steps;

        // Clamp the number of threads
        uint32_t maxThreads = std::thread::hardware_concurrency();
        if (m_capturedParams.methodInfo.numThreads > maxThreads) m_capturedParams.methodInfo.numThreads = maxThreads;
        if (m_capturedParams.methodInfo.numThreads < 1) m_capturedParams.methodInfo.numThreads = 1;

        // Reset the status
        m_status.reset();
        m_status.setWorking();

        // Start the thread
        m_thread = std::make_shared<std::jthread>([this, dispSteps]()
            {
                try
                {
                    CMS::ensureOK();

                    // CMF table
                    std::shared_ptr<CmfTable> table = CMF::getActiveTable();
                    if (table.get() == nullptr)
                        throw std::exception("An active CMF table is needed.");

                    // Input buffer
                    std::vector<float> inputBuffer;
                    uint32_t inputWidth = 0, inputHeight = 0;
                    previewInput(false, &inputBuffer, &inputWidth, &inputHeight);
                    uint32_t inputBufferSize = inputWidth * inputHeight * 4;

                    // Sample wavelengths
                    std::vector<float> cmfSamples;
                    table->sampleRGB(dispSteps, true, cmfSamples);
                    if (cmfSamples.size() < (dispSteps * 3))
                        throw std::exception("Invalid number of samples provided by CmfTable.");

                    // Call the appropriate function
                    switch (m_capturedParams.methodInfo.method)
                    {
                    case RealBloom::DispersionMethod::CPU:
                        dispCPU(
                            inputBuffer, inputWidth, inputHeight,
                            inputBufferSize, cmfSamples);
                        break;
                    case RealBloom::DispersionMethod::GPU:
                        dispGPU(
                            inputBuffer, inputWidth, inputHeight,
                            inputBufferSize, cmfSamples);
                        break;
                    default:
                        break;
                    }

                    // Update the dispersion image
                    m_imgDisp->moveToGPU();
                    Async::emitSignal("selImageID", m_imgDisp->getID());

                    m_status.setDone();
                }
                catch (const std::exception& e)
                {
                    m_status.setError(e.what());
                }
            });
    }

    void Dispersion::cancel()
    {
        if (!m_status.isWorking())
        {
            m_status.reset();
            return;
        }

        m_status.setMustCancel();

        if (m_capturedParams.methodInfo.method == DispersionMethod::CPU)
        {
            // Tell the sub-threads to stop
            for (auto& ct : m_threads)
                ct->stop();
        }

        // Wait for the main thread
        threadJoin(m_thread.get());
        m_thread = nullptr;

        m_status.reset();
    }

    const TimedWorkingStatus& Dispersion::getStatus() const
    {
        return m_status;
    }

    std::string Dispersion::getStatusText() const
    {
        if (m_status.mustCancel())
            return "";

        if (!m_status.isOK())
            return m_status.getError();

        if (m_status.isWorking())
        {
            float elapsedSec = m_status.getElapsedSec();

            if (m_capturedParams.methodInfo.method == DispersionMethod::CPU)
            {
                uint32_t numSteps = m_capturedParams.steps;
                uint32_t numDone = getNumStepsDoneCpu();

                float remainingSec = (elapsedSec * (float)(numSteps - numDone)) / fmaxf((float)(numDone), EPSILON);
                return strFormat(
                    "%u/%u steps\n%s / %s",
                    numDone,
                    numSteps,
                    strFromElapsed(elapsedSec).c_str(),
                    strFromElapsed(remainingSec).c_str());
            }
            else if (m_capturedParams.methodInfo.method == DispersionMethod::GPU)
            {
                return strFromElapsed(elapsedSec);
            }
        }
        else if (m_status.hasTimestamps())
        {
            float elapsedSec = m_status.getElapsedSec();
            return strFormat("Done (%s)", strFromDuration(elapsedSec).c_str());
        }

        return "";
    }

    uint32_t Dispersion::getNumStepsDoneCpu() const
    {
        if (!(m_status.isWorking() && m_status.isOK() && !m_status.mustCancel()))
            return 0;

        if (m_capturedParams.methodInfo.method != DispersionMethod::CPU)
            return 0;

        uint32_t numDone = 0;

        for (auto& ct : m_threads)
        {
            DispersionThreadStats* stats = ct->getStats();
            if (stats->state == DispersionThreadState::Working || stats->state == DispersionThreadState::Done)
                numDone += stats->numDone;
        }

        return numDone;
    }

    void Dispersion::dispCPU(
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize,
        std::vector<float>& cmfSamples)
    {
        // Parameters
        uint32_t numThreads = m_capturedParams.methodInfo.numThreads;

        try
        {
            // Create and prepare threads
            for (uint32_t i = 0; i < numThreads; i++)
            {
                std::shared_ptr<DispersionThread> ct = std::make_shared<DispersionThread>(
                    numThreads, i, m_capturedParams,
                    inputBuffer.data(), inputWidth, inputHeight,
                    cmfSamples.data()
                    );

                std::vector<float>& threadBuffer = ct->getBuffer();
                threadBuffer.resize(inputBufferSize);
                for (uint32_t j = 0; j < inputBufferSize; j++)
                    if (j % 4 == 3) threadBuffer[j] = 1.0f;
                    else threadBuffer[j] = 0.0f;

                m_threads.push_back(ct);
            }

            // Start the threads
            for (auto& ct : m_threads)
            {
                std::shared_ptr<std::jthread> t = std::make_shared<std::jthread>([ct]()
                    {
                        ct->start();
                    });
                ct->setThread(t);
            }

            // Wait for the threads
            {
                uint32_t lastNumDone = 0;
                while (true)
                {
                    uint32_t numThreadsDone = 0;

                    for (auto& ct : m_threads)
                    {
                        if (ct->getStats()->state == DispersionThreadState::Done)
                            numThreadsDone++;
                    }

                    // Keep in mind that the threads will be "Done" when mustCancel is true
                    if (numThreadsDone >= numThreads)
                        break;

                    uint32_t numDone = getNumStepsDoneCpu();

                    // Take a snapshot of the current progress
                    if ((!m_status.mustCancel()) && (numThreadsDone < numThreads) && ((numDone - lastNumDone) >= numThreads))
                    {
                        std::vector<float> progBuffer;
                        progBuffer.resize(inputBufferSize);
                        for (uint32_t i = 0; i < inputBufferSize; i++)
                            if (i % 4 == 3) progBuffer[i] = 1.0f;
                            else progBuffer[i] = 0.0f;

                        for (auto& ct : m_threads)
                        {
                            std::vector<float>& currentBuffer = ct->getBuffer();
                            uint32_t redIndex;
                            for (uint32_t y = 0; y < inputHeight; y++)
                            {
                                for (uint32_t x = 0; x < inputWidth; x++)
                                {
                                    redIndex = (y * inputWidth + x) * 4;
                                    progBuffer[redIndex + 0] += currentBuffer[redIndex + 0];
                                    progBuffer[redIndex + 1] += currentBuffer[redIndex + 1];
                                    progBuffer[redIndex + 2] += currentBuffer[redIndex + 2];
                                }
                            }
                        }

                        // Copy progBuffer into the dispersion image
                        {
                            std::scoped_lock lock(*m_imgDisp);
                            m_imgDisp->resize(inputWidth, inputHeight, false);
                            float* dispBuffer = m_imgDisp->getImageData();
                            std::copy(progBuffer.data(), progBuffer.data() + inputBufferSize, dispBuffer);
                        }
                        m_imgDisp->moveToGPU();

                        lastNumDone = numDone;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(DISP_PROG_TIMESTEP));
                }
            }

            // Join the threads
            for (auto& ct : m_threads)
                threadJoin(ct->getThread().get());

            if (m_status.mustCancel())
                throw std::exception();

            // Add the buffers from each thread
            {
                std::scoped_lock lock(*m_imgDisp);
                m_imgDisp->resize(inputWidth, inputHeight, false);
                m_imgDisp->fill(std::array<float, 4>{ 0, 0, 0, 1 }, false);
                float* dispBuffer = m_imgDisp->getImageData();

                for (auto& ct : m_threads)
                {
                    std::vector<float>& threadBuffer = ct->getBuffer();
                    uint32_t redIndex;
                    for (uint32_t y = 0; y < inputHeight; y++)
                    {
                        for (uint32_t x = 0; x < inputWidth; x++)
                        {
                            redIndex = (y * inputWidth + x) * 4;
                            dispBuffer[redIndex + 0] += threadBuffer[redIndex + 0];
                            dispBuffer[redIndex + 1] += threadBuffer[redIndex + 1];
                            dispBuffer[redIndex + 2] += threadBuffer[redIndex + 2];
                        }
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            m_status.setError(e.what());
        }

        // Clean up
        m_threads.clear();
    }

    void Dispersion::dispGPU(
        std::vector<float>& inputBuffer,
        uint32_t inputWidth,
        uint32_t inputHeight,
        uint32_t inputBufferSize,
        std::vector<float>& cmfSamples)
    {
        // Create GpuHelper instance
        GpuHelper gpuHelper;
        std::string inpFilename = gpuHelper.getInpFilename();
        std::string outFilename = gpuHelper.getOutFilename();

        try
        {
            // Prepare input data
            BinaryDispGpuInput binInput;
            binInput.dp_amount = m_capturedParams.amount;
            binInput.dp_steps = m_capturedParams.steps;
            binInput.inputWidth = inputWidth;
            binInput.inputHeight = inputHeight;
            binInput.inputBuffer = inputBuffer;
            binInput.cmfSamples = cmfSamples;

            // Create the input file
            std::ofstream inpFile;
            inpFile.open(inpFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            if (!inpFile.is_open())
                throw std::exception(
                    strFormat("Input file \"%s\" could not be created/opened.", inpFilename.c_str()).c_str()
                );

            // Write operation type
            uint32_t opType = (uint32_t)GpuHelperOperationType::Dispersion;
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
            BinaryDispGpuOutput binOutput;
            binOutput.readFrom(outFile);
            if (binOutput.status == 1)
            {
                // Update the output image

                std::scoped_lock lock(*m_imgDisp);
                m_imgDisp->resize(inputWidth, inputHeight, false);
                float* dispBuffer = m_imgDisp->getImageData();
                uint32_t dispBufferSize = m_imgDisp->getImageDataSize();

                if (dispBufferSize == binOutput.buffer.size())
                {
                    std::copy(binOutput.buffer.data(), binOutput.buffer.data() + binOutput.buffer.size(), dispBuffer);
                }
                else
                {
                    m_status.setError(strFormat(
                        "Output buffer size (%u) does not match the input buffer size (%u).",
                        binOutput.buffer.size(), dispBuffer
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

}
