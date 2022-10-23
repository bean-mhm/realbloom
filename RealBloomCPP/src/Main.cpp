#include "Main.h"

#define IMGUI_DIV ImGui::NewLine()

#define IMGUI_BOLD(TEXT) ImGui::PushFont(fontRobotoBold); \
ImGui::Text(TEXT); \
ImGui::PopFont();

const char* glsl_version = "#version 130";
GLFWwindow* window = nullptr;
ImGuiIO* io;
bool appRunning = true;

ImFont* fontRoboto = nullptr;
ImFont* fontRobotoBold = nullptr;
ImFont* fontMono = nullptr;
ImVec4 bgColor{ 0.13f, 0.13f, 0.13f, 1.00f };

constexpr uint32_t defaultDims = 128;
std::vector<Image32Bit*> images;
std::vector<char*> imageNames;
int selImageIndex = 0;
float imageZoom = 1.0f;

const ImVec4 colorInfoText{ 0.328f, 0.735f, 0.910f, 1.0f };
const ImVec4 colorWarningText{ 0.940f, 0.578f, 0.282f, 1.0f };
const ImVec4 colorErrorText{ 0.950f, 0.300f, 0.228f, 1.0f };

RealBloom::DiffractionPattern diffPattern(defaultDims, defaultDims);
RealBloom::Dispersion dispersion;
RealBloom::Convolution conv;

UiVars vars;

std::string convResUsage = "";

int main()
{
    Config::load();
    Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);

    if (!setupGLFW())
        return 1;
    if (!setupImGui())
        return 1;
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Create images to be used throughout the program
    images.push_back(new Image32Bit(
        "aperture",
        "Aperture Shape", defaultDims));
    images.push_back(new Image32Bit(
        "diff",
        "Diffraction Pattern", defaultDims));
    images.push_back(new Image32Bit(
        "disp",
        "Dispersion", defaultDims));
    images.push_back(new Image32Bit(
        "cv-input",
        "Conv. Input", defaultDims));
    images.push_back(new Image32Bit(
        "cv-kernel",
        "Conv. Kernel", defaultDims));
    images.push_back(new Image32Bit(
        "cv-kernel-prev",
        "Conv. Kernel (Transformed)", defaultDims));
    images.push_back(new Image32Bit(
        "cv-prev",
        "Conv. Preview", defaultDims));
    images.push_back(new Image32Bit(
        "cv-layer",
        "Conv. Layer", defaultDims));
    images.push_back(new Image32Bit(
        "cv-result",
        "Conv. Result", defaultDims));

    for (uint32_t i = 0; i < images.size(); i++)
    {
        char* c = new char[128];
        std::string s = images[i]->getName();
        s.copy(c, 128);
        c[std::min((uint32_t)s.size(), (uint32_t)127)] = '\0';

        imageNames.push_back(c);
    }

    // Set up images for convolution
    conv.setInputImage(getImage("cv-input"));
    conv.setKernelImage(getImage("cv-kernel"));
    conv.setKernelPreviewImage(getImage("cv-kernel-prev"));
    conv.setConvPreviewImage(getImage("cv-prev"));
    conv.setConvLayerImage(getImage("cv-layer"));
    conv.setConvMixImage(getImage("cv-result"));

    // Set up images for dispersion
    dispersion.setDiffPatternImage(getImage("diff"));
    dispersion.setDispersionImage(getImage("disp"));

    // Will be shared with Convolution and Dispersion
    Async::putShared("convMixParamsChanged", &(vars.convMixParamsChanged));
    Async::putShared("selImageIndex", &selImageIndex);

    // To kill child processes when the parent dies
    // https://stackoverflow.com/a/53214/18049911
    HANDLE hJobObject = CreateJobObjectA(NULL, NULL);
    if (hJobObject != NULL)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        Async::putShared("hJobObject", hJobObject);
    }

    // Update resource usage info for convolution
    std::thread convResUsageThread([]()
        {
            auto lastTime = std::chrono::system_clock::now();
            while (appRunning)
            {
                if (getElapsedMs(lastTime) > 1000)
                {
                    updateConvParams();
                    convResUsage = conv.getResourceInfo();
                    lastTime = std::chrono::system_clock::now();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

    // Main loop
    appRunning = !glfwWindowShouldClose(window);
    while (appRunning)
    {
        appRunning = !glfwWindowShouldClose(window);

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        // Process our queued tasks
        {
            std::unique_lock<std::mutex> lock(Async::s_tasksMutex);
            while (!Async::s_tasks.empty())
            {
                auto task(std::move(Async::s_tasks.front()));
                Async::s_tasks.pop_front();

                // unlock during the task
                lock.unlock();
                task();
                lock.lock();
            }
        }

        // UI Layout
        ImGui::PushFont(fontRoboto);
        layout();
        ImGui::PopFont();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(bgColor.x * bgColor.w, bgColor.y * bgColor.w, bgColor.z * bgColor.w, bgColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    Config::save();
    convResUsageThread.join();
    conv.cancelConv();
    cleanUp();
    return 0;
}

void layout()
{
    // Image Selector
    {
        ImGui::Begin("Image List");
        ImGui::PushItemWidth(-1);
        ImGui::ListBox("##Images", &selImageIndex, &lb1ItemGetter, nullptr, images.size(), images.size());
        ImGui::PopItemWidth();

        // For comparing conv. input and result
        if (selImageIndex == 3 || selImageIndex == 8)
        {
            if (ImGui::Button("Compare"))
            {
                if (selImageIndex == 3)
                    selImageIndex = 8;
                else
                    selImageIndex = 3;
            }
        }

        ImGui::End();
    }

    // Image Viewer
    {
        Image32Bit& selImage = *(images[selImageIndex]);
        ImGui::Begin("Image Viewer", 0, ImGuiWindowFlags_HorizontalScrollbar);

        // Name
        ImGui::Text(selImage.getName().c_str());

        // Size
        uint32_t imageWidth = 0, imageHeight = 0;
        selImage.getDimensions(imageWidth, imageHeight);
        ImGui::Text("Size: %dx%d", imageWidth, imageHeight);

        // Zoom
        ImGui::Text("%d%%", (int)roundf(imageZoom * 100.0f));

        ImGui::SameLine();
        ImGui::PushFont(fontMono);
        if (ImGui::SmallButton("+"))
            imageZoom = fminf(fmaxf(imageZoom + 0.125f, 0.25f), 2.0f);

        ImGui::SameLine();
        if (ImGui::SmallButton("-"))
            imageZoom = fminf(fmaxf(imageZoom - 0.125f, 0.25f), 2.0f);

        ImGui::SameLine();
        if (ImGui::SmallButton("R"))
            imageZoom = 1.0f;
        ImGui::PopFont();

        ImGui::Image(
            (void*)(intptr_t)selImage.getGlTexture(),
            ImVec2(
                (float)imageWidth * imageZoom,
                (float)imageHeight * imageZoom
            ));
        ImGui::End();
    }

    // Controls
    Image32Bit& imgAperture = *getImage("aperture");
    Image32Bit& imgDiffPattern = *getImage("diff");
    Image32Bit& imgDispersion = *getImage("disp");
    Image32Bit& imgConvInput = *getImage("cv-input");
    Image32Bit& imgKernel = *getImage("cv-kernel");
    Image32Bit& imgConvLayer = *getImage("cv-layer");
    Image32Bit& imgConvResult = *getImage("cv-result");
    {
        ImGui::Begin("Diffraction Pattern");

        {
            static LoadImageResult pngResult;
            if (ImGui::Button("Browse Aperture##DP"))
            {
                if (loadImage("", ImageFormat::PNG, imgAperture, pngResult))
                    selImageIndex = 0;
            }
            imGuiText(pngResult.error, !pngResult.success, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("Diffraction Pattern");
        ImGui::Checkbox("Grayscale##DP", &(vars.dp_grayscale));
        if (ImGui::Button("Compute##DP"))
        {
            {
                std::lock_guard<Image32Bit> lock(imgAperture);
                float* image1Buffer = imgAperture.getImageData();

                uint32_t image1Width = 0, image1Height = 0;
                imgAperture.getDimensions(image1Width, image1Height);

                RealBloom::DiffractionPatternParams* dpParams = diffPattern.getParams();
                dpParams->width = image1Width;
                dpParams->height = image1Height;
                dpParams->grayscale = vars.dp_grayscale;

                diffPattern.compute(image1Buffer);
            }

            if (diffPattern.success())
            {
                renderDiffPattern();
                selImageIndex = 1;
            }
        }
        if (!diffPattern.success())
        {
            std::string dpError = diffPattern.getError();
            imGuiText(dpError, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("Dispersion");

        if (ImGui::SliderFloat("Intensity##Disp", &(vars.dp_multiplier), 0, 5))
            vars.dpParamsChanged = true;
        if (ImGui::SliderFloat("Contrast##Disp", &(vars.dp_contrast), -1, 1))
            vars.dpParamsChanged = true;

        if (vars.dpParamsChanged)
        {
            vars.dpParamsChanged = false;
            renderDiffPattern();
            selImageIndex = 1;
        }

        ImGui::SliderFloat("Amount##Disp", &(vars.ds_dispersionAmount), 0, 1);
        ImGui::SliderInt("Steps##Disp", &(vars.ds_dispersionSteps), 32, 1024);
        ImGui::ColorEdit3("Color##Disp", vars.ds_dispersionCol, ImGuiColorEditFlags_NoInputs);

        if (ImGui::Button("Apply##Disp"))
        {
            RealBloom::DispersionParams* dispersionParams = dispersion.getParams();
            dispersionParams->amount = vars.ds_dispersionAmount;
            dispersionParams->steps = vars.ds_dispersionSteps;
            std::copy(vars.ds_dispersionCol, vars.ds_dispersionCol + 3, dispersionParams->color);

            selImageIndex = 2;
            dispersion.compute();
        }

        ImGui::SameLine();
        if (dispersion.isWorking())
        {
            if (ImGui::Button("Cancel##Disp"))
                dispersion.cancel();
        } else
        {
            static std::string tiffError = "";
            if (ImGui::Button("Save##Disp"))
            {
                std::lock_guard<Image32Bit> lock(imgDispersion);

                float* image3Buffer = imgDispersion.getImageData();
                uint32_t image3Width = 0, image3Height = 0;
                imgDispersion.getDimensions(image3Width, image3Height);

                std::string filename;
                if (saveDialog("tif", filename))
                {
                    saveTIFF(filename, image3Buffer, image3Width, image3Height, tiffError);
                }
            }
            imGuiText(tiffError, true, false);
        }

        std::string dispStats = dispersion.getStats();
        if (!dispStats.empty())
            ImGui::Text(dispStats.c_str());

        IMGUI_DIV;
        ImGui::End(); // Diffraction Pattern

        ImGui::Begin("Convolution");

        {
            static LoadImageResult tiffResult;
            if (ImGui::Button("Browse Input##Conv"))
            {
                if (loadImage("", ImageFormat::TIFF, imgConvInput, tiffResult))
                {
                    selImageIndex = 3;
                    vars.convThresholdChanged = true;
                }
            }
            imGuiText(tiffResult.error, !tiffResult.success, true);
        }

        {
            static LoadImageResult tiffResult;
            if (ImGui::Button("Browse Kernel##Conv"))
            {
                if (loadImage("", ImageFormat::TIFF, imgKernel, tiffResult))
                {
                    selImageIndex = 4;
                    vars.convParamsChanged = true;
                }
            }
            imGuiText(tiffResult.error, !tiffResult.success, true);
        }

        IMGUI_DIV;
        IMGUI_BOLD("Kernel");

        if (ImGui::SliderFloat("Intensity##Kernel", &(vars.cv_kernelIntensity), 0, 5))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat("Contrast##Kernel", &(vars.cv_kernelContrast), -1, 1))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat("Rotation##Kernel", &(vars.cv_kernelRotation), -180.0f, 180.0f))
            vars.convParamsChanged = true;

        ImGui::SmallButton("L");
        ImGui::SameLine();
        if (ImGui::SliderFloat2("Scale##Kernel", vars.cv_kernelScale, 0.1f, 2))
            vars.convParamsChanged = true;

        ImGui::SmallButton("L");
        ImGui::SameLine();
        if (ImGui::SliderFloat2("Crop##Kernel", vars.cv_kernelCrop, 0.1f, 1.0f))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat2("Center##Kernel", vars.cv_kernelCenter, 0, 1))
            vars.convParamsChanged = true;

        if (ImGui::Checkbox("Preview Center##Kernel", &(vars.cv_kernelPreviewCenter)))
            vars.convParamsChanged = true;

        if (vars.convParamsChanged)
        {
            vars.convParamsChanged = false;
            updateConvParams();
            conv.kernel();
            selImageIndex = 5;
        }

        IMGUI_DIV;
        IMGUI_BOLD("Convolution");

        ImGui::Combo("Device##Conv", &(vars.cv_deviceType), cb1ItemGetter, nullptr, 2);

        if (vars.cv_deviceType == 0)
        {
            // Threads
            ImGui::SliderInt("Threads##Conv", &(vars.cv_numThreads), 1, vars.cv_maxThreads);

            // Warning Text
            if (vars.cv_numThreads > vars.cv_halfMaxThreads)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colorWarningText);
                if (vars.cv_numThreads == vars.cv_maxThreads)
                    ImGui::TextWrapped(
                        "Maximizing the number of threads might result in slowdowns and "
                        "potential crashes.");
                else
                    ImGui::TextWrapped(
                        "Using over half the number of concurrent threads supported "
                        "by the hardware is not recommended.");
                ImGui::PopStyleColor();
            }
        } else
        {
            // Chunks
            if (ImGui::InputInt("Chunks##Conv", &(vars.cv_numChunks)))
                vars.cv_numChunks = std::min(std::max(vars.cv_numChunks, 1), RealBloom::CONV_MAX_CHUNKS);

            // Chunk Sleep Time
            if (ImGui::InputInt("Sleep (ms)##Conv", &(vars.cv_chunkSleep)))
                vars.cv_chunkSleep = std::min(std::max(vars.cv_chunkSleep, 0), RealBloom::CONV_MAX_SLEEP);
        }

        if (ImGui::SliderFloat("Threshold##Conv", &(vars.cv_convThreshold), 0, 2))
        {
            vars.convThresholdChanged = true;
            vars.convThresholdSwitchImage = true;
        }

        if (ImGui::SliderFloat("Knee##Conv", &(vars.cv_convKnee), 0, 2))
        {
            vars.convThresholdChanged = true;
            vars.convThresholdSwitchImage = true;
        }

        if (vars.convThresholdChanged)
        {
            updateConvParams();
            conv.previewThreshold();
            if (vars.convThresholdSwitchImage)
            {
                selImageIndex = 6;
                vars.convThresholdSwitchImage = false;
            }
            vars.convThresholdChanged = false;
        }

        if (ImGui::Button("Convolve##Conv"))
        {
            imgConvLayer.resize(imgConvInput.getWidth(), imgConvInput.getHeight());
            imgConvLayer.fill({ 0, 0, 0, 1 });
            imgConvLayer.moveToGPU();
            selImageIndex = 7;

            updateConvParams();
            conv.convolve();
        }

        if (ImGui::IsItemHovered() && !convResUsage.empty())
            ImGui::SetTooltip(convResUsage.c_str());

        if (conv.isWorking())
        {
            ImGui::SameLine();
            if (ImGui::Button("Cancel##Conv"))
                conv.cancelConv();
        }

        std::string convTime, convStatus;
        uint32_t convStatType = 1;
        conv.getConvStats(convTime, convStatus, convStatType);
        if (!convTime.empty())
        {
            ImGui::Text(convTime.c_str());
        }
        if (!convStatus.empty())
        {
            if (convStatType > 0)
            {
                const ImVec4* textColor = &colorErrorText;
                if (convStatType == 1)
                    textColor = &colorInfoText;
                else if (convStatType == 2)
                    textColor = &colorWarningText;
                else if (convStatType == 3)
                    textColor = &colorErrorText;

                ImGui::PushStyleColor(ImGuiCol_Text, *textColor);
                ImGui::TextWrapped(convStatus.c_str());
                ImGui::PopStyleColor();
            } else
            {
                ImGui::Text(convStatus.c_str());
            }
        }

        IMGUI_DIV;
        IMGUI_BOLD("Layers");

        if (ImGui::Checkbox("Additive##Conv", &(vars.cm_additive)))
            vars.convMixParamsChanged = true;

        if (vars.cm_additive)
        {
            if (ImGui::SliderFloat("Input##Conv", &(vars.cm_inputMix), 0, 2))
                vars.convMixParamsChanged = true;
            if (ImGui::SliderFloat("Conv.##Conv", &(vars.cm_convMix), 0, 5))
                vars.convMixParamsChanged = true;
        } else
        {
            if (ImGui::SliderFloat("Mix##Conv", &(vars.cm_mix), 0, 1))
                vars.convMixParamsChanged = true;
            if (ImGui::SliderFloat("Intensity##Conv", &(vars.cm_convIntensity), 0, 5))
                vars.convMixParamsChanged = true;
        }

        if (vars.convMixParamsChanged)
        {
            vars.convMixParamsChanged = false;
            conv.mixConv(vars.cm_additive, vars.cm_inputMix, vars.cm_convMix, vars.cm_mix, vars.cm_convIntensity);
            selImageIndex = 8;
        }

        {
            static std::string tiffError = "";
            if (ImGui::Button("Save##Conv"))
            {
                std::lock_guard<Image32Bit> lock(imgConvResult);

                float* imageBuffer = imgConvResult.getImageData();
                uint32_t imageWidth = 0, imageHeight = 0;
                imgConvResult.getDimensions(imageWidth, imageHeight);

                std::string filename;
                if (saveDialog("tif", filename))
                {
                    saveTIFF(filename, imageBuffer, imageWidth, imageHeight, tiffError);
                }
            }
            imGuiText(tiffError, true, false);
        }

        IMGUI_DIV;
        ImGui::End(); // Convolution
    }

    // Info
    {
        ImGui::Begin("Info");

        if (ImGui::InputFloat("UI Scale", &(Config::UI_SCALE), 0.125f, 0.125f, "%.3f"))
        {
            Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);
            io->FontGlobalScale = Config::UI_SCALE / Config::S_UI_MAX_SCALE;
        }

        // FPS
        ImGui::NewLine();
        ImGui::TextWrapped(
            "%.3f ms (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);

        // Renderer
        static std::string uiRenderer = (const char*)glGetString(GL_RENDERER);
        ImGui::NewLine();
        ImGui::TextWrapped("UI Renderer: %s", uiRenderer.c_str());

        // Version
        ImGui::NewLine();
        ImGui::TextWrapped("%s v%s", Config::S_APP_TITLE, Config::S_APP_VERSION);

        // GitHub
        if (ImGui::Button("GitHub"))
            openURL(Config::S_GITHUB_URL);

        ImGui::End();
    }
}

void imGuiText(const std::string& text, bool isError, bool newLine)
{
    if (!text.empty())
    {
        if (isError)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, colorErrorText);
            ImGui::TextWrapped(text.c_str());
            ImGui::PopStyleColor();
        } else
        {
            ImGui::TextWrapped(text.c_str());
        }
        if (newLine)
            ImGui::NewLine();
    }
}

bool lb1ItemGetter(void* data, int index, const char** outText)
{
    *outText = imageNames[index];
    return true;
}

bool cb1ItemGetter(void* data, int index, const char** outText)
{
    *outText = (index == 0) ? "CPU" : "GPU";
    return true;
}

Image32Bit* getImage(const std::string& id)
{
    for (uint32_t i = 0; i < images.size(); i++)
    {
        if (images[i]->getID() == id)
            return images[i];
    }

    return nullptr;
}

bool loadImage(std::string filename, ImageFormat format, Image32Bit& image, LoadImageResult& outResult)
{
    outResult.success = true;
    outResult.error = "";

    bool haveFilename = false;
    if (filename.empty())
    {
        std::string filterList;
        if (format == ImageFormat::PNG)
            filterList = FILTER_LIST_PNG;
        else
            filterList = FILTER_LIST_TIFF;

        nfdchar_t* outPath = NULL;
        nfdresult_t result = NFD_OpenDialog(filterList.c_str(), NULL, &outPath);

        if (result == NFD_OKAY)
        {
            filename = outPath;
            free(outPath);
            haveFilename = true;
        }
    } else
    {
        haveFilename = true;
    }

    if (!haveFilename)
        return false;

    if (format == ImageFormat::PNG)
    {
        std::vector<float> pngBuffer;
        uint32_t pngWidth = 0, pngHeight = 0;

        if (loadPNG(filename.c_str(), pngBuffer, pngWidth, pngHeight, outResult.error))
        {
            image.resize(pngWidth, pngHeight);
            {
                std::lock_guard<Image32Bit> lock(image);
                float* imageBuffer = image.getImageData();

                uint32_t bufferSize = pngWidth * pngHeight * 4;
                for (uint32_t i = 0; i < bufferSize; i++)
                {
                    imageBuffer[i] = pngBuffer[i];
                }
            }
            image.moveToGPU();
        } else
        {
            outResult.success = false;
        }
    } else if (format == ImageFormat::TIFF)
    {
        std::vector<float> tiffBuffer;
        uint32_t tiffWidth = 0, tiffHeight = 0;

        if (loadTIFF(filename.c_str(), tiffBuffer, tiffWidth, tiffHeight, outResult.error))
        {
            image.resize(tiffWidth, tiffHeight);
            {
                std::lock_guard<Image32Bit> lock(image);
                float* imageBuffer = image.getImageData();

                uint32_t redIndex = 0;
                for (uint32_t y = 0; y < tiffHeight; y++)
                {
                    for (uint32_t x = 0; x < tiffWidth; x++)
                    {
                        redIndex = 4 * (y * tiffWidth + x);

                        imageBuffer[redIndex + 0] = tiffBuffer[redIndex + 0];
                        imageBuffer[redIndex + 1] = tiffBuffer[redIndex + 1];
                        imageBuffer[redIndex + 2] = tiffBuffer[redIndex + 2];
                        imageBuffer[redIndex + 3] = 1;
                    }
                }
            }
            image.moveToGPU();
        } else
        {
            outResult.success = false;
        }
    }

    return outResult.success;
}

bool saveDialog(std::string extension, std::string& outFilename)
{
    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_SaveDialog(extension.c_str(), NULL, &outPath);
    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        free(outPath);

        bool hasExtension = false;
        uint32_t lastDotIndex = outFilename.find_last_of('.');
        if (lastDotIndex >= 0)
            if (outFilename.substr(lastDotIndex + 1) == extension)
                hasExtension = true;

        if (!hasExtension)
            outFilename += "." + extension;

        return true;
    }
    return false;
}

void renderDiffPattern()
{
    Image32Bit& imgDiffPattern = *getImage("diff");

    if (diffPattern.hasRawData())
    {
        RealBloom::DiffractionPatternParams* dpParams = diffPattern.getParams();
        dpParams->contrast = vars.dp_contrast;
        dpParams->multiplier = vars.dp_multiplier;

        std::vector<float> buffer;
        diffPattern.getRgbaOutput(buffer);

        uint32_t width = dpParams->width;
        uint32_t height = dpParams->height;

        imgDiffPattern.resize(width, height);
        {
            std::lock_guard<Image32Bit> lock(imgDiffPattern);
            float* image2Buffer = imgDiffPattern.getImageData();

            uint32_t imageSize = width * height * 4;
            for (uint32_t i = 0; i < imageSize; i++)
            {
                image2Buffer[i] = buffer[i];
            }
        }
        imgDiffPattern.moveToGPU();
    }
}

void updateConvParams()
{
    RealBloom::ConvolutionParams* convParams = conv.getParams();
    convParams->device.deviceType =
        (vars.cv_deviceType == 0) ? RealBloom::ConvolutionDeviceType::CPU : RealBloom::ConvolutionDeviceType::GPU;
    convParams->device.numThreads = vars.cv_numThreads;
    convParams->device.numChunks = vars.cv_numChunks;
    convParams->device.chunkSleep = vars.cv_chunkSleep;
    convParams->kernelRotation = vars.cv_kernelRotation;
    convParams->kernelScaleW = vars.cv_kernelScale[0];
    convParams->kernelScaleH = vars.cv_kernelScale[1];
    convParams->kernelCropW = vars.cv_kernelCrop[0];
    convParams->kernelCropH = vars.cv_kernelCrop[1];
    convParams->kernelCenterX = vars.cv_kernelCenter[0];
    convParams->kernelCenterY = vars.cv_kernelCenter[1];
    convParams->kernelPreviewCenter = vars.cv_kernelPreviewCenter;
    convParams->kernelContrast = vars.cv_kernelContrast;
    convParams->kernelIntensity = vars.cv_kernelIntensity;
    convParams->convThreshold = vars.cv_convThreshold;
    convParams->convKnee = vars.cv_convKnee;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#include <GL/wglew.h>

bool setupGLFW()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    window = glfwCreateWindow(Config::S_WINDOW_WIDTH, Config::S_WINDOW_HEIGHT, Config::S_APP_TITLE, NULL, NULL);
    if (window == NULL)
        return false;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    return true;
}

bool setupImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io->ConfigViewportsNoAutoMerge = true;
    //io->ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    applyStyle_RealBloom();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
    io->FontGlobalScale = Config::UI_SCALE / Config::S_UI_MAX_SCALE;
    fontRoboto = io->Fonts->AddFontFromFileTTF("./assets/fonts/RobotoCondensed-Regular.ttf", 18.0f * Config::S_UI_MAX_SCALE);
    fontRobotoBold = io->Fonts->AddFontFromFileTTF("./assets/fonts/RobotoCondensed-Bold.ttf", 22.0f * Config::S_UI_MAX_SCALE);
    fontMono = io->Fonts->AddFontFromFileTTF("./assets/fonts/mono/RobotoMono-Regular.ttf", 18.0f * Config::S_UI_MAX_SCALE);
    if ((!fontRoboto) || (!fontRobotoBold) || (!fontMono))
        std::cout << "The required fonts could not be loaded.\n";

    return (fontRoboto && fontRobotoBold && fontMono);
}

static inline ImVec4 ImLerp(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static inline ImVec4 operator*(const ImVec4& lhs, const ImVec4& rhs)
{
    return ImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
}

void applyStyle_RealBloom()
{
    // RealBloom style from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0;
    style.DisabledAlpha = 0.5;
    style.WindowPadding = ImVec2(10.0, 6.0);
    style.WindowRounding = 0.0;
    style.WindowBorderSize = 1.0;
    style.WindowMinSize = ImVec2(32.0, 32.0);
    style.WindowTitleAlign = ImVec2(0.5, 0.5);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0;
    style.ChildBorderSize = 1.0;
    style.PopupRounding = 0.0;
    style.PopupBorderSize = 1.0;
    style.FramePadding = ImVec2(8.0, 3.0);
    style.FrameRounding = 3.0;
    style.FrameBorderSize = 1.0;
    style.ItemSpacing = ImVec2(4.0, 4.0);
    style.ItemInnerSpacing = ImVec2(4.0, 4.0);
    style.CellPadding = ImVec2(6.0, 5.0);
    style.IndentSpacing = 8.0;
    style.ColumnsMinSpacing = 6.0;
    style.ScrollbarSize = 13.0;
    style.ScrollbarRounding = 6.0;
    style.GrabMinSize = 16.0;
    style.GrabRounding = 2.0;
    style.TabRounding = 3.0;
    style.TabBorderSize = 1.0;
    style.TabMinWidthForCloseButton = 4.0;
    style.ColorButtonPosition = ImGuiDir_Left;
    style.ButtonTextAlign = ImVec2(0.5, 0.5);
    style.SelectableTextAlign = ImVec2(0.0, 0.5);

    style.Colors[ImGuiCol_Text] = ImVec4(0.9019607901573181, 0.9019607901573181, 0.9019607901573181, 1.0);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.8627451062202454, 0.8627451062202454, 0.8627451062202454, 0.3137255012989044);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0784313753247261, 0.0784313753247261, 0.0784313753247261, 1.0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0, 0.9960784316062927, 1.0, 0.0);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0, 0.0, 0.0, 0.0);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4470588266849518, 0.321568638086319, 0.1490196138620377, 0.5098039507865906);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4470588266849518, 0.321568638086319, 0.1490196138620377, 0.5882353186607361);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.294117659330368, 0.294117659330368, 0.294117659330368, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.3921568691730499, 0.3921568691730499, 0.3921568691730499, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5490196347236633, 0.5490196347236633, 0.5490196347236633, 1.0);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_Button] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.7843137383460999);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_Header] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.294117659330368, 0.294117659330368, 0.294117659330368, 1.0);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.3921568691730499, 0.3921568691730499, 0.3921568691730499, 1.0);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.294117659330368, 0.294117659330368, 0.294117659330368, 1.0);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.3921568691730499, 0.3921568691730499, 0.3921568691730499, 1.0);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.3921568691730499, 0.3921568691730499, 0.3921568691730499, 1.0);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.7843137383460999, 0.7843137383460999, 0.7843137383460999, 0.8627451062202454);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.9411764740943909, 0.9411764740943909, 0.9411764740943909, 1.0);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.7843137383460999);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.294117659330368, 0.294117659330368, 0.294117659330368, 1.0);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.294117659330368, 0.294117659330368, 0.294117659330368, 1.0);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0, 0.0, 0.0, 0.0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0, 1.0, 1.0, 0.03921568766236305);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 0.9019607901573181);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0, 1.0, 1.0, 0.7019608020782471);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.2000000029802322);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.3499999940395355);
}

void applyStyle_Blender()
{
    // Blender style from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0;
    style.DisabledAlpha = 0.5;
    style.WindowPadding = ImVec2(10.0, 6.0);
    style.WindowRounding = 0.0;
    style.WindowBorderSize = 1.0;
    style.WindowMinSize = ImVec2(32.0, 32.0);
    style.WindowTitleAlign = ImVec2(0.5, 0.5);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0;
    style.ChildBorderSize = 1.0;
    style.PopupRounding = 0.0;
    style.PopupBorderSize = 1.0;
    style.FramePadding = ImVec2(8.0, 3.0);
    style.FrameRounding = 3.0;
    style.FrameBorderSize = 1.0;
    style.ItemSpacing = ImVec2(4.0, 4.0);
    style.ItemInnerSpacing = ImVec2(4.0, 4.0);
    style.CellPadding = ImVec2(6.0, 5.0);
    style.IndentSpacing = 8.0;
    style.ColumnsMinSpacing = 6.0;
    style.ScrollbarSize = 13.0;
    style.ScrollbarRounding = 6.0;
    style.GrabMinSize = 16.0;
    style.GrabRounding = 2.0;
    style.TabRounding = 3.0;
    style.TabBorderSize = 1.0;
    style.TabMinWidthForCloseButton = 4.0;
    style.ColorButtonPosition = ImGuiDir_Left;
    style.ButtonTextAlign = ImVec2(0.5, 0.5);
    style.SelectableTextAlign = ImVec2(0.0, 0.5);

    style.Colors[ImGuiCol_Text] = ImVec4(0.9019607901573181, 0.9019607901573181, 0.9019607901573181, 1.0);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.8627451062202454, 0.8627451062202454, 0.8627451062202454, 0.3921568691730499);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1882352977991104, 0.1882352977991104, 0.1882352977991104, 1.0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.1490196138620377, 0.1490196138620377, 0.1490196138620377, 1.0);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.09411764889955521, 0.09411764889955521, 0.09411764889955521, 1.0);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0, 1.0, 1.0, 0.0470588244497776);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.0, 1.0, 1.0, 0.0);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.3294117748737335, 0.3294117748737335, 0.3294117748737335, 1.0);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4745098054409027, 0.4745098054409027, 0.4745098054409027, 1.0);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.1333333402872086, 0.1333333402872086, 0.1333333402872086, 1.0);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.1882352977991104, 0.1882352977991104, 0.1882352977991104, 1.0);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.1882352977991104, 0.1882352977991104, 0.1882352977991104, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.2431372553110123, 0.2431372553110123, 0.2431372553110123, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.3294117748737335, 0.3294117748737335, 0.3294117748737335, 1.0);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.3490196168422699, 0.3490196168422699, 0.3490196168422699, 1.0);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9019607901573181, 0.9019607901573181, 0.9019607901573181, 1.0);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3294117748737335, 0.3294117748737335, 0.3294117748737335, 1.0);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3960784375667572, 0.3960784375667572, 0.3960784375667572, 1.0);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_Header] = ImVec4(0.239215686917305, 0.239215686917305, 0.239215686917305, 1.0);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.08627451211214066, 0.08627451211214066, 0.08627451211214066, 1.0);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.08627451211214066, 0.08627451211214066, 0.08627451211214066, 1.0);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.08627451211214066, 0.08627451211214066, 0.08627451211214066, 1.0);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.5686274766921997, 0.5686274766921997, 0.5686274766921997, 1.0);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.6627451181411743, 0.6627451181411743, 0.6627451181411743, 1.0);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.6627451181411743, 0.6627451181411743, 0.6627451181411743, 1.0);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.239215686917305, 0.239215686917305, 0.239215686917305, 1.0);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.7843137383460999, 0.7843137383460999, 0.7843137383460999, 1.0);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.7843137383460999, 0.7843137383460999, 0.7843137383460999, 1.0);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.2000000029802322, 0.2000000029802322, 0.2000000029802322, 1.0);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.1137254908680916, 0.1137254908680916, 0.1137254908680916, 1.0);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.1490196138620377, 0.1490196138620377, 0.1490196138620377, 1.0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.168627455830574, 0.168627455830574, 0.168627455830574, 1.0);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2784313857555389, 0.4470588266849518, 0.7019608020782471, 1.0);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.239215686917305, 0.239215686917305, 0.239215686917305, 1.0);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.239215686917305, 0.239215686917305, 0.239215686917305, 1.0);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0, 1.0, 1.0, 0.7019608020782471);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.2000000029802322);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.3499999940395355);
}

void cleanUp()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}