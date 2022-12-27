#include "Dispersion.h"
#include "DispersionThread.h"

namespace RealBloom
{

    constexpr uint32_t DISP_PROG_TIMESTEP = 33;

    Dispersion::Dispersion() : m_imgInputSrc("", "")
    {}

    DispersionParams* Dispersion::getParams()
    {
        return &m_params;
    }

    void Dispersion::setImgInput(CmImage* image)
    {
        m_imgInput = image;
    }

    void Dispersion::setImgDisp(CmImage* image)
    {
        m_imgDisp = image;
    }

    CmImage* Dispersion::getImgInputSrc()
    {
        return &m_imgInputSrc;
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
            table->sampleRGB(pWidth, samples);

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
                std::lock_guard<CmImage> lock(*m_imgDisp);
                m_imgDisp->resize(pWidth, pHeight, false);
                float* imageBuffer = m_imgDisp->getImageData();
                std::copy(buffer.data(), buffer.data() + buffer.size(), imageBuffer);
            }
            m_imgDisp->moveToGPU();
        }
        catch (const std::exception& e)
        {
            throw std::exception(printErr(__FUNCTION__, "", e.what()).c_str());
        }
    }

    void Dispersion::previewInput(bool previewMode, std::vector<float>* outBuffer, uint32_t* outWidth, uint32_t* outHeight)
    {
        std::vector<float> inputBuffer;
        uint32_t inputBufferSize = 0;
        uint32_t inputWidth = 0, inputHeight = 0;
        {
            std::lock_guard<CmImage> lock(m_imgInputSrc);
            float* inputSrcBuffer = m_imgInputSrc.getImageData();
            inputBufferSize = m_imgInputSrc.getImageDataSize();
            inputWidth = m_imgInputSrc.getWidth();
            inputHeight = m_imgInputSrc.getHeight();

            inputBuffer.resize(inputBufferSize);
            std::copy(inputSrcBuffer, inputSrcBuffer + inputBufferSize, inputBuffer.data());
        }

        float expMul = applyExposure(m_params.exposure);
        float contrast = m_params.contrast;

        std::array<float, 3> color = m_params.color;

        // Apply contrast and exposure
        {
            // Get the brightest value
            float v = 0;
            float maxV = EPSILON;
            for (uint32_t i = 0; i < inputBufferSize; i++)
            {
                v = fmaxf(0.0f, inputBuffer[i]);
                if ((v > maxV) && (i % 4 != 3))
                    maxV = v;
            }

            uint32_t redIndex;
            for (uint32_t y = 0; y < inputHeight; y++)
            {
                for (uint32_t x = 0; x < inputWidth; x++)
                {
                    redIndex = (y * inputWidth + x) * 4;

                    // Normalize
                    inputBuffer[redIndex + 0] /= maxV;
                    inputBuffer[redIndex + 1] /= maxV;
                    inputBuffer[redIndex + 2] /= maxV;

                    // Calculate the grayscale value
                    float grayscale = rgbToGrayscale(inputBuffer[redIndex + 0], inputBuffer[redIndex + 1], inputBuffer[redIndex + 2]);

                    // Apply contrast, de-normalize, colorize
                    if (grayscale > 0.0f)
                    {
                        float mul = expMul * maxV * (contrastCurve(grayscale, contrast) / grayscale);
                        inputBuffer[redIndex + 0] *= mul * color[0];
                        inputBuffer[redIndex + 1] *= mul * color[1];
                        inputBuffer[redIndex + 2] *= mul * color[2];
                    }
                }
            }
        }

        // Copy to outBuffer if it's requested by another function
        if (!previewMode && (outBuffer != nullptr))
        {
            *outWidth = inputWidth;
            *outHeight = inputHeight;
            outBuffer->resize(inputBufferSize);
            *outBuffer = inputBuffer;
        }

        // Copy to input image
        {
            std::lock_guard<CmImage> lock(*m_imgInput);
            m_imgInput->resize(inputWidth, inputHeight, false);
            float* prevBuffer = m_imgInput->getImageData();
            std::copy(inputBuffer.data(), inputBuffer.data() + inputBufferSize, prevBuffer);
        }

        m_imgInput->moveToGPU();
        m_imgInput->setSourceName(m_imgInputSrc.getSourceName());
    }

    void Dispersion::compute()
    {
        cancel();

        uint32_t dispSteps = m_params.steps;
        if (dispSteps > DISP_MAX_STEPS) dispSteps = DISP_MAX_STEPS;
        if (dispSteps < 1) dispSteps = 1;
        m_params.steps = dispSteps;

        m_state.working = true;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.error = "";
        m_state.timeStart = std::chrono::system_clock::now();
        m_state.hasTimestamps = false;
        m_state.numSteps = dispSteps;

        // Clamp the number of threads
        uint32_t maxThreads = std::thread::hardware_concurrency();
        uint32_t numThreads = m_params.numThreads;
        if (numThreads > maxThreads) numThreads = maxThreads;
        if (numThreads < 1) numThreads = 1;

        // Start the thread
        m_thread = new std::thread([this, dispSteps, numThreads]()
            {
                try
                {
                    CMS::ensureOK();

                    // CMF table
                    std::shared_ptr<CmfTable> table = CMF::getActiveTable();
                    if (table.get() == nullptr)
                        throw std::exception("An active CMF table is needed.");

                    // Input Buffer
                    std::vector<float> inputBuffer;
                    uint32_t inputWidth = 0, inputHeight = 0;
                    previewInput(false, &inputBuffer, &inputWidth, &inputHeight);
                    uint32_t inputBufferSize = inputWidth * inputHeight * 4;

                    // Sample wavelengths
                    std::vector<float> cmfSamples;
                    table->sampleRGB(dispSteps, cmfSamples);
                    if (cmfSamples.size() < (dispSteps * 3))
                        throw std::exception("Invalid number of samples provided by CmfTable.");

                    // Normalize the samples
                    {
                        // Calculate the total perceived luminance
                        float luminanceSum = EPSILON;
                        uint32_t smpIndex;
                        for (uint32_t i = 0; i < dispSteps; i++)
                        {
                            smpIndex = i * 3;
                            luminanceSum += rgbToGrayscale(
                                cmfSamples[smpIndex + 0],
                                cmfSamples[smpIndex + 1],
                                cmfSamples[smpIndex + 2]
                            );
                        }

                        // Divide by the sum
                        for (uint32_t i = 0; i < dispSteps; i++)
                        {
                            smpIndex = i * 3;
                            cmfSamples[smpIndex + 0] /= luminanceSum;
                            cmfSamples[smpIndex + 1] /= luminanceSum;
                            cmfSamples[smpIndex + 2] /= luminanceSum;
                        }
                    }

                    // Create and prepare threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        DispersionThread* ct = new DispersionThread(
                            numThreads, i, m_params,
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
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        DispersionThread* ct = m_threads[i];
                        std::shared_ptr<std::thread> t = std::make_shared<std::thread>([ct]()
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
                            DispersionThread* ct;
                            DispersionThreadStats* stats;
                            for (uint32_t i = 0; i < numThreads; i++)
                            {
                                if (m_threads[i])
                                {
                                    ct = m_threads[i];
                                    stats = ct->getStats();
                                    if (stats->state == DispersionThreadState::Done)
                                        numThreadsDone++;
                                }
                            }

                            // Keep in mind that the threads will be "Done" when mustCancel is true
                            if (numThreadsDone >= numThreads)
                                break;

                            uint32_t numDone = getNumDone();

                            // Take a snapshot of the current progress
                            if ((!m_state.mustCancel) && (numThreadsDone < numThreads) && ((numDone - lastNumDone) >= numThreads))
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
                                                progBuffer[redIndex + 0] += currentBuffer[redIndex + 0];
                                                progBuffer[redIndex + 1] += currentBuffer[redIndex + 1];
                                                progBuffer[redIndex + 2] += currentBuffer[redIndex + 2];
                                            }
                                        }
                                    }
                                }

                                // Copy progBuffer into the dispersion image
                                {
                                    std::lock_guard<CmImage> lock(*m_imgDisp);
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
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        if (m_threads[i])
                            threadJoin(m_threads[i]->getThread().get());
                    }

                    if (m_state.mustCancel)
                        throw std::exception();

                    // Add the buffers from each thread
                    {
                        std::lock_guard<CmImage> lock(*m_imgDisp);
                        m_imgDisp->resize(inputWidth, inputHeight, false);
                        m_imgDisp->fill(std::array<float, 4>{ 0, 0, 0, 1 }, false);
                        float* dispBuffer = m_imgDisp->getImageData();

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
                                            dispBuffer[redIndex + 0] += threadBuffer[redIndex + 0];
                                            dispBuffer[redIndex + 1] += threadBuffer[redIndex + 1];
                                            dispBuffer[redIndex + 2] += threadBuffer[redIndex + 2];
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Update the dispersion image
                    m_imgDisp->moveToGPU();
                    Async::schedule([this]()
                        {
                            std::string* pSelImageID = (std::string*)Async::getShared("selImageID");
                            if (pSelImageID != nullptr) *pSelImageID = m_imgDisp->getID();
                        });

                    m_state.timeEnd = std::chrono::system_clock::now();
                    m_state.hasTimestamps = true;
                }
                catch (const std::exception& e)
                {
                    if (!m_state.mustCancel)
                    {
                        m_state.failed = true;
                        m_state.error = e.what();
                    }
                }

                // Clean up
                for (uint32_t i = 0; i < m_threads.size(); i++)
                    DELPTR(m_threads[i]);
                m_threads.clear();

                m_state.working = false;
            });
    }

    void Dispersion::cancel()
    {
        if (m_state.working)
        {
            m_state.mustCancel = true;

            // Tell the sub-threads to stop
            DispersionThread* ct;
            for (uint32_t i = 0; i < m_threads.size(); i++)
            {
                ct = m_threads[i];
                if (ct)
                {
                    ct->stop();
                }
            }

            // Wait for the thread
            threadJoin(m_thread);
            DELPTR(m_thread);
        }

        m_state.working = false;
        m_state.mustCancel = false;
        m_state.failed = false;
        m_state.hasTimestamps = false;
    }

    bool Dispersion::isWorking() const
    {
        return m_state.working;
    }

    bool Dispersion::hasFailed() const
    {
        return m_state.failed;
    }

    std::string Dispersion::getError() const
    {
        return m_state.error;
    }

    uint32_t Dispersion::getNumDone() const
    {
        if (!(m_state.working && !m_state.failed && !m_state.mustCancel))
            return 0;

        uint32_t numDone = 0;
        for (uint32_t i = 0; i < m_threads.size(); i++)
        {
            if (m_threads[i])
            {
                DispersionThreadStats* stats = m_threads[i]->getStats();
                if (stats->state == DispersionThreadState::Working || stats->state == DispersionThreadState::Done)
                {
                    numDone += stats->numDone;
                }
            }
        }
        return numDone;
    }

    std::string Dispersion::getStats() const
    {
        if (m_state.mustCancel)
            return "";

        if (m_state.failed)
            return m_state.error;

        if (m_state.working)
        {
            float elapsedSec = (float)getElapsedMs(m_state.timeStart) / 1000.0f;

            uint32_t numSteps = m_state.numSteps;
            uint32_t numDone = getNumDone();

            float remainingSec = (elapsedSec * (float)(numSteps - numDone)) / fmaxf((float)(numDone), EPSILON);
            return strFormat(
                "%u/%u steps\n%s / %s",
                numDone,
                numSteps,
                strFromElapsed(elapsedSec).c_str(),
                strFromElapsed(remainingSec).c_str());
        }
        else if (m_state.hasTimestamps)
        {
            std::chrono::milliseconds elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(m_state.timeEnd - m_state.timeStart);
            float elapsedSec = (float)elapsedMs.count() / 1000.0f;
            return strFormat("Done (%s)", strFromDuration(elapsedSec).c_str());
        }

        return "";
    }

}