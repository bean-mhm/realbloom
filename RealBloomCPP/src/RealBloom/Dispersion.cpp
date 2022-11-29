#include "Dispersion.h"
#include "DispersionThread.h"

namespace RealBloom
{

    constexpr uint32_t DISP_WAIT_TIMESTEP = 33;

    Dispersion::Dispersion()
    {}

    DispersionParams* Dispersion::getParams()
    {
        return &m_params;
    }

    void Dispersion::setDiffPatternImage(CmImage* image)
    {
        m_imageDP = image;
    }

    void Dispersion::setDispersionImage(CmImage* image)
    {
        m_imageDisp = image;
    }

    void Dispersion::setNumThreads(uint32_t numThreads)
    {
        m_numThreads = numThreads;
    }

    void Dispersion::previewCmf()
    {
        std::shared_ptr<CmfTable> table = CMF::getActiveTable();
        if (table.get() == nullptr)
            return;

        uint32_t pWidth = std::max(2u, (uint32_t)table->getCount() * 2);
        uint32_t pHeight = std::max(2u, (uint32_t)pWidth / 10);

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
            std::lock_guard<CmImage> lock(*m_imageDisp);
            m_imageDisp->resize(pWidth, pHeight, false);
            float* imageBuffer = m_imageDisp->getImageData();
            std::copy(buffer.data(), buffer.data() + buffer.size(), imageBuffer);
        }
        m_imageDisp->moveToGPU();
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
        uint32_t numThreads = m_numThreads;
        if (numThreads > maxThreads) numThreads = maxThreads;
        if (numThreads < 1) numThreads = 1;

        // Start the thread
        m_thread = new std::thread([this, dispSteps, numThreads]()
            {
                try
                {
                    // CMF table
                    std::shared_ptr<CmfTable> table = CMF::getActiveTable();
                    if (table.get() == nullptr)
                        throw std::exception("An active CMF table is needed.");

                    // Input buffer
                    std::vector<float> inputBuffer;
                    uint32_t inputWidth = 0, inputHeight = 0;
                    uint32_t inputBufferSize = 0;
                    {
                        // Buffer from diff pattern image
                        std::lock_guard<CmImage> lock(*m_imageDP);
                        float* dpImageData = m_imageDP->getImageData();
                        inputWidth = m_imageDP->getWidth();
                        inputHeight = m_imageDP->getHeight();

                        // Copy the buffer so the image doesn't stay locked throughout the process
                        inputBufferSize = m_imageDP->getImageDataSize();
                        inputBuffer.resize(inputBufferSize);
                        std::copy(dpImageData, dpImageData + inputBufferSize, inputBuffer.data());
                    }

                    // Sample wavelengths
                    std::vector<float> cmfSamples;
                    table->sampleRGB(dispSteps, cmfSamples);
                    if (cmfSamples.size() < (dispSteps * 3))
                        throw std::exception("Invalid number of samples provided by CmfTable.");

                    // Calculate the perceived luminance for the sum of the samples
                    float luminanceSum = 0.0f, luminanceMul = 0.0f;
                    {
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
                        luminanceMul = 1.0f / luminanceSum;
                    }

                    // Create and prepare threads
                    for (uint32_t i = 0; i < numThreads; i++)
                    {
                        DispersionThread* ct = new DispersionThread(
                            numThreads, i, m_params,
                            inputBuffer.data(), inputWidth, inputHeight,
                            cmfSamples.data(), luminanceMul
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
                                    std::lock_guard<CmImage> lock(*m_imageDisp);
                                    m_imageDisp->resize(inputWidth, inputHeight, false);
                                    float* dispBuffer = m_imageDisp->getImageData();

                                    std::copy(progBuffer.data(), progBuffer.data() + inputBufferSize, dispBuffer);
                                }
                                m_imageDisp->moveToGPU();

                                lastNumDone = numDone;
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(DISP_WAIT_TIMESTEP));
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
                        std::lock_guard<CmImage> lock(*m_imageDisp);
                        m_imageDisp->resize(inputWidth, inputHeight, false);
                        m_imageDisp->fill(color_t{ 0, 0, 0, 1 }, false);
                        float* dispBuffer = m_imageDisp->getImageData();

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
                    m_imageDisp->moveToGPU();
                    Async::schedule([this]()
                        {
                            int* pSelImageIndex = (int*)Async::getShared("selImageIndex");
                            if (pSelImageIndex != nullptr) *pSelImageIndex = 2;
                        });

                    m_state.timeEnd = std::chrono::system_clock::now();
                    m_state.hasTimestamps = true;
                } catch (const std::exception& e)
                {
                    if (!m_state.mustCancel)
                    {
                        m_state.failed = true;
                        m_state.error = e.what();
                    }
                }

                // Clean up
                for (uint32_t i = 0; i < numThreads; i++)
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