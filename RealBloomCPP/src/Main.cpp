#include "Main.h"

#define IMGUI_DIV ImGui::NewLine()

#define IMGUI_BOLD(TEXT) ImGui::PushFont(fontRobotoBold); \
ImGui::Text(TEXT); \
ImGui::PopFont();

const char* glslVersion = "#version 130";
GLFWwindow* window = nullptr;
ImGuiIO* io;
bool appRunning = true;

ImFont* fontRoboto = nullptr;
ImFont* fontRobotoBold = nullptr;
ImFont* fontMono = nullptr;

const ImVec4 bgColor{ 0.13f, 0.13f, 0.13f, 1.00f };
const ImVec4 colorInfoText{ 0.328f, 0.735f, 0.910f, 1.0f };
const ImVec4 colorWarningText{ 0.940f, 0.578f, 0.282f, 1.0f };
const ImVec4 colorErrorText{ 0.950f, 0.300f, 0.228f, 1.0f };

const ImVec2 buttonSize{ -1, 27 };
std::vector<std::string> activeComboList;

std::vector<CmImage*> images;
std::vector<std::string> imageNames;
int selImageIndex = 0;
float imageZoom = 1.0f;

RealBloom::DiffractionPattern diffPattern;
RealBloom::Dispersion dispersion;
RealBloom::Convolution conv;
UiVars vars;
std::string convResUsage = "";

constexpr const char* DIALOG_COLORSPACE = "Color Space";
std::packaged_task<void()> dialogAction_ColorSpace;
std::string dialogResult_ColorSpace;

int main()
{
    // Set Locale
    if (!std::setlocale(LC_ALL, Config::S_APP_LOCALE))
        std::cout << "Couldn't set locale to \"" << Config::S_APP_LOCALE << "\".\n";

    // Load config
    Config::load();
    Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);

    // Setup GLFW and ImGui
    if (!setupGLFW())
        return 1;
    if (!setupImGui())
        return 1;

    // GLEW for loading OpenGL extensions
    glewInit();

    // Color Management System
    if (!CMS::init())
        return 1;

    // Color Matching Functions
    CMF::init();

    // Hide the console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Create images to be used throughout the program
    images.push_back(new CmImage("aperture", "Aperture Shape"));
    images.push_back(new CmImage("diff", "Diffraction Pattern"));
    images.push_back(new CmImage("disp", "Dispersion"));
    images.push_back(new CmImage("cv-input", "Conv. Input"));
    images.push_back(new CmImage("cv-kernel", "Conv. Kernel"));
    images.push_back(new CmImage("cv-kernel-prev", "Conv. Kernel (Transformed)"));
    images.push_back(new CmImage("cv-prev", "Conv. Preview"));
    images.push_back(new CmImage("cv-layer", "Conv. Layer"));
    images.push_back(new CmImage("cv-result", "Conv. Result"));

    for (uint32_t i = 0; i < images.size(); i++)
        imageNames.push_back(images[i]->getName());

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

    // Quit
    Config::save();
    convResUsageThread.join();
    conv.cancelConv();
    cleanUp();
    return 0;
}

void layout()
{
    CmImage& selImage = *(images[selImageIndex]);

    static int lastSelImageIndex = selImageIndex;
    if (selImageIndex != lastSelImageIndex)
    {
        selImage.moveToGPU();
        lastSelImageIndex = selImageIndex;
    }

    // Image Selector
    {
        ImGui::Begin("Image List");

        IMGUI_BOLD("SLOTS");

        ImGui::PushItemWidth(-1);
        ImGui::ListBox("##Slots", &selImageIndex, &lb1ItemGetter, nullptr, images.size(), images.size());
        ImGui::PopItemWidth();

        // For comparing conv. input and result
        if (selImageIndex == 3 || selImageIndex == 8)
        {
            if (ImGui::Button("Compare", buttonSize))
            {
                if (selImageIndex == 3) selImageIndex = 8;
                else selImageIndex = 3;
            }
        }

        // Save
        {
            static std::string saveError = "";
            if (ImGui::Button("Save", buttonSize))
                saveImage(selImage, saveError);
            imGuiText(saveError, true, false);
        }

        ImGui::NewLine();
        imGuiDialogs();
        ImGui::End();
    }

    // Image Viewer
    {
        ImGui::Begin("Image Viewer", 0, ImGuiWindowFlags_HorizontalScrollbar);

        // Name
        ImGui::Text(selImage.getName().c_str());

        // Size
        uint32_t imageWidth = selImage.getWidth();
        uint32_t imageHeight = selImage.getHeight();
        ImGui::Text("Size: %dx%d", imageWidth, imageHeight);

        // Zoom
        {
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
        }

        ImGui::Image(
            (void*)(intptr_t)selImage.getGlTexture(),
            ImVec2(
                (float)imageWidth * imageZoom,
                (float)imageHeight * imageZoom
            ));

        ImGui::End();
    }

    // Controls
    CmImage& imgAperture = *getImage("aperture");
    CmImage& imgDiffPattern = *getImage("diff");
    CmImage& imgDispersion = *getImage("disp");
    CmImage& imgConvInput = *getImage("cv-input");
    CmImage& imgKernel = *getImage("cv-kernel");
    CmImage& imgConvLayer = *getImage("cv-layer");
    CmImage& imgConvResult = *getImage("cv-result");
    {
        ImGui::Begin("Diffraction Pattern");

        IMGUI_BOLD("APERTURE");

        {
            static std::string loadResult;
            if (ImGui::Button("Browse Aperture##DP", buttonSize))
                loadImage(imgAperture, 0, nullptr, loadResult);
            imGuiText(loadResult, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("DIFFRACTION");

        ImGui::Checkbox("Grayscale##DP", &(vars.dp_grayscale));
        if (ImGui::Button("Compute##DP", buttonSize))
        {
            {
                std::lock_guard<CmImage> lock(imgAperture);
                float* image1Buffer = imgAperture.getImageData();

                RealBloom::DiffractionPatternParams* dpParams = diffPattern.getParams();
                dpParams->width = imgAperture.getWidth();
                dpParams->height = imgAperture.getHeight();
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
        IMGUI_BOLD("DISPERSION");

        if (ImGui::SliderFloat("Intensity##Disp", &(vars.dp_multiplier), 0, 5))
            vars.dpParamsChanged = true;
        if (ImGui::SliderFloat("Contrast##Disp", &(vars.dp_contrast), -1, 1))
            vars.dpParamsChanged = true;

        if (vars.dpParamsChanged)
        {
            vars.dpParamsChanged = false;
            selImageIndex = 1;
            renderDiffPattern();
        }

        ImGui::SliderFloat("Amount##Disp", &(vars.ds_dispersionAmount), 0, 1);
        ImGui::SliderInt("Steps##Disp", &(vars.ds_dispersionSteps), 32, 1024);
        ImGui::ColorEdit3("Color##Disp", vars.ds_dispersionCol, ImGuiColorEditFlags_NoInputs);

        if (ImGui::Button("Apply##Disp", buttonSize))
        {
            RealBloom::DispersionParams* dispersionParams = dispersion.getParams();
            dispersionParams->amount = vars.ds_dispersionAmount;
            dispersionParams->steps = vars.ds_dispersionSteps;
            std::copy(vars.ds_dispersionCol, vars.ds_dispersionCol + 3, dispersionParams->color);

            selImageIndex = 2;
            dispersion.compute();
        }

        if (dispersion.isWorking())
        {
            if (ImGui::Button("Cancel##Disp", buttonSize))
                dispersion.cancel();
        }

        std::string dispStats = dispersion.getStats();
        imGuiText(dispStats, dispersion.hasFailed(), false);

        ImGui::NewLine();
        imGuiDialogs();
        ImGui::End(); // Diffraction Pattern

        ImGui::Begin("Convolution");

        IMGUI_BOLD("INPUT");

        {
            static std::string loadResult;
            if (ImGui::Button("Browse Input##Conv", buttonSize))
                loadImage(imgConvInput, 3, nullptr, loadResult);
            imGuiText(loadResult, true, false);
        }

        {
            static std::string loadResult;
            if (ImGui::Button("Browse Kernel##Conv", buttonSize))
                loadImage(imgKernel, 4, &(vars.convParamsChanged), loadResult);
            imGuiText(loadResult, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("KERNEL");

        if (ImGui::SliderFloat("Intensity##Kernel", &(vars.cv_kernelIntensity), 0, 5))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat("Contrast##Kernel", &(vars.cv_kernelContrast), -1, 1))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat("Rotation##Kernel", &(vars.cv_kernelRotation), -180.0f, 180.0f))
            vars.convParamsChanged = true;

        // Scale
        {
            if (ImGui::Checkbox("Lock Scale##Kernel", &(vars.cv_kernelLockScale)))
            {
                if (vars.cv_kernelLockScale)
                {
                    vars.cv_kernelScale[0] = (vars.cv_kernelScale[0] + vars.cv_kernelScale[1]) / 2.0f;
                    vars.cv_kernelScale[1] = vars.cv_kernelScale[0];
                }
                vars.convParamsChanged = true;
            }
            if (vars.cv_kernelLockScale)
            {
                if (ImGui::SliderFloat("Scale##Kernel", &(vars.cv_kernelScale[0]), 0.1f, 2))
                {
                    vars.cv_kernelScale[1] = vars.cv_kernelScale[0];
                    vars.convParamsChanged = true;
                }
            } else
            {
                if (ImGui::SliderFloat2("Scale##Kernel", vars.cv_kernelScale, 0.1f, 2))
                    vars.convParamsChanged = true;
            }
        }

        // Crop
        {
            if (ImGui::Checkbox("Lock Crop##Kernel", &(vars.cv_kernelLockCrop)))
            {
                if (vars.cv_kernelLockCrop)
                {
                    vars.cv_kernelCrop[0] = (vars.cv_kernelCrop[0] + vars.cv_kernelCrop[1]) / 2.0f;
                    vars.cv_kernelCrop[1] = vars.cv_kernelCrop[0];
                }
                vars.convParamsChanged = true;
            }
            if (vars.cv_kernelLockCrop)
            {
                if (ImGui::SliderFloat("Crop##Kernel", &(vars.cv_kernelCrop[0]), 0.1f, 1.0f))
                {
                    vars.cv_kernelCrop[1] = vars.cv_kernelCrop[0];
                    vars.convParamsChanged = true;
                }
            } else
            {
                if (ImGui::SliderFloat2("Crop##Kernel", vars.cv_kernelCrop, 0.1f, 1.0f))
                    vars.convParamsChanged = true;
            }
        }

        if (ImGui::Checkbox("Preview Center##Kernel", &(vars.cv_kernelPreviewCenter)))
            vars.convParamsChanged = true;

        if (ImGui::SliderFloat2("Center##Kernel", vars.cv_kernelCenter, 0, 1))
            vars.convParamsChanged = true;

        if (vars.convParamsChanged)
        {
            vars.convParamsChanged = false;
            updateConvParams();
            conv.kernel();
            selImageIndex = 5;
        }

        IMGUI_DIV;
        IMGUI_BOLD("CONVOLUTION");

        const char* const deviceTypeItems[]{ "CPU", "GPU" };
        ImGui::Combo("Device##Conv", &(vars.cv_deviceType), deviceTypeItems, 2);

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

        if (ImGui::Button("Convolve##Conv", buttonSize))
        {
            imgConvLayer.resize(imgConvInput.getWidth(), imgConvInput.getHeight(), true);
            imgConvLayer.fill(color_t{ 0, 0, 0, 1 }, true);
            imgConvLayer.moveToGPU();
            selImageIndex = 7;

            updateConvParams();
            conv.convolve();
        }

        if (ImGui::IsItemHovered() && !convResUsage.empty())
            ImGui::SetTooltip(convResUsage.c_str());

        if (conv.isWorking())
        {
            if (ImGui::Button("Cancel##Conv", buttonSize))
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
        IMGUI_BOLD("LAYERS");

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

        ImGui::NewLine();
        imGuiDialogs();
        ImGui::End(); // Convolution
    }

    // Color Management
    {
        ImGui::Begin("Color Management");

        IMGUI_BOLD("COLOR MATCHING");

        {

            static int selCmfTable = 0;
            static bool cmfLoaded = false;

            // Get available CMF Tables
            std::vector<CmfTableInfo> cmfTables = CMF::getAvailableTables();

            // Convert to a string vector
            std::vector<std::string> cmfTableNames;
            for (const auto& tbl : cmfTables)
                cmfTableNames.push_back(tbl.name);

            // Load the first available table by default
            if (!cmfLoaded)
            {
                cmfLoaded = true;
                if (cmfTables.size() > 0)
                    CMF::setActiveTable(cmfTables[0]);
            }

            // CMF Table
            static bool tableChanged = false;
            if (imguiCombo("CMF##CMF", cmfTableNames, &selCmfTable, false))
            {
                CMF::setActiveTable(cmfTables[selCmfTable]);
                tableChanged = true;
            }

            // CMF Details
            std::string cmfTableDetails = CMF::getActiveTableDetails();
            if (ImGui::IsItemHovered() && CMF::hasTable() && !cmfTableDetails.empty())
                ImGui::SetTooltip(cmfTableDetails.c_str());

            // CMF Error
            if (!CMF::ok())
                imGuiText(CMF::getError(), true, false);

            // CMF Preview
            if (ImGui::Button("Preview##CMF", buttonSize) || tableChanged)
            {
                tableChanged = false;
                selImageIndex = 2;
                dispersion.previewCmf();
            }

        }

        IMGUI_DIV;
        IMGUI_BOLD("VIEW");

        {

            // Exposure
            if (ImGui::SliderFloat("Exposure##CMS", &(vars.cms_exposure), -10, 10))
            {
                CMS::setExposure(vars.cms_exposure);
                vars.cmsParamsChanged = true;
            }

            static int selDisplay = 0;
            static int selView = 0;
            static int selLook = 0;

            // Display
            std::vector<std::string> displays = CMS::getAvailableDisplays();
            if (imguiCombo("Display##CMS", displays, &selDisplay, false))
            {
                CMS::setActiveDisplay(displays[selDisplay]);
                vars.cmsParamsChanged = true;

                // Update selView
                std::vector<std::string> views = CMS::getAvailableViews();
                std::string activeView = CMS::getActiveView();
                ptrdiff_t activeViewIndex = std::distance(views.begin(), std::find(views.begin(), views.end(), activeView));
                if (activeViewIndex < views.size())
                    selView = activeViewIndex;
            }

            // View
            std::vector<std::string> views = CMS::getAvailableViews();
            if (imguiCombo("View##CMS", views, &selView, false))
            {
                CMS::setActiveView(views[selView]);
                vars.cmsParamsChanged = true;
            }

            // Look
            std::vector<std::string> looks = CMS::getAvailableLooks();
            if (imguiCombo("Look##CMS", looks, &selLook, false))
            {
                CMS::setActiveLook(looks[selLook]);
                vars.cmsParamsChanged = true;
            }

            if (vars.cmsParamsChanged)
            {
                vars.cmsParamsChanged = false;
                for (CmImage* image : images)
                    image->moveToGPU();
            }

        }

        IMGUI_DIV;
        IMGUI_BOLD("INFO");

        {

            // Working Space
            static std::string workingSpace = CMS::getWorkingSpace();
            static std::string workingSpaceDesc = CMS::getWorkingSpaceDesc();
            ImGui::TextWrapped("Working Space: %s", workingSpace.c_str());
            if (ImGui::IsItemHovered() && !workingSpaceDesc.empty())
                ImGui::SetTooltip(workingSpaceDesc.c_str());

            // CMS Error
            if (!CMS::ok())
                imGuiText(CMS::getError(), true, false);

        }

        ImGui::NewLine();
        ImGui::End();
    }

    // Misc
    {
        ImGui::Begin("Misc");

        IMGUI_BOLD("INTERFACE");

        if (ImGui::InputFloat("Scale", &(Config::UI_SCALE), 0.125f, 0.125f, "%.3f"))
        {
            Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);
            io->FontGlobalScale = Config::UI_SCALE / Config::S_UI_MAX_SCALE;
        }

        // UI Renderer
        static std::string uiRenderer = formatStr("UI Renderer:\n%s", (const char*)glGetString(GL_RENDERER));

        // FPS
        ImGui::TextWrapped(
            "%.3f ms (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(uiRenderer.c_str());
        ImGui::NewLine();

        IMGUI_BOLD("INFO");

        // Version
        ImGui::TextWrapped("%s v%s", Config::S_APP_TITLE, Config::S_APP_VERSION);

        // GitHub
        if (ImGui::Button("GitHub", buttonSize))
            openURL(Config::S_GITHUB_URL);
        if (ImGui::Button("Tutorial", buttonSize))
            openURL(Config::S_DOCS_URL);

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
    *outText = imageNames[index].c_str();
    return true;
}

bool comboItemGetter(void* data, int index, const char** outText)
{
    *outText = activeComboList[index].c_str();
    return true;
}

bool imguiCombo(const std::string& label, const std::vector<std::string>& items, int* selectedIndex, bool fullWidth)
{
    activeComboList = items;
    bool result;

    if (fullWidth)
        ImGui::PushItemWidth(-1);
    result = ImGui::Combo(label.c_str(), selectedIndex, comboItemGetter, nullptr, activeComboList.size());
    if (fullWidth)
        ImGui::PopItemWidth();

    return result;
}

CmImage* getImage(const std::string& id)
{
    for (uint32_t i = 0; i < images.size(); i++)
    {
        if (images[i]->getID() == id)
            return images[i];
    }
    return nullptr;
}

bool openImageDialog(std::string& outFilename)
{
    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_OpenDialog("", NULL, &outPath);
    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        free(outPath);
        return true;
    }
    return false;
}

bool saveImageDialog(std::string& outFilename)
{
    const char* fileExtension = "exr";
    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_SaveDialog(fileExtension, NULL, &outPath);
    if (result == NFD_OKAY)
    {
        outFilename = outPath;
        free(outPath);

        bool hasExtension = false;
        size_t lastDotIndex = outFilename.find_last_of('.');
        if (lastDotIndex != std::string::npos)
            if (outFilename.substr(lastDotIndex + 1) == fileExtension)
                hasExtension = true;

        if (!hasExtension)
            outFilename += "." + std::string(fileExtension);

        return true;
    }
    return false;
}

void loadImage(CmImage& image, int imageIndex, bool* toSetTrue, std::string& outError)
{
    std::string filename;
    if (openImageDialog(filename))
    {
        dialogAction_ColorSpace = std::packaged_task<void()>(
            [&image, imageIndex, toSetTrue, filename, &outError]()
            {
                if (CmImageIO::readImage(image, filename, dialogResult_ColorSpace, outError))
                {
                    selImageIndex = imageIndex;
                    if (toSetTrue != nullptr)
                        *toSetTrue = true;
                }
            });
        ImGui::OpenPopup(DIALOG_COLORSPACE);
    }
}

void saveImage(CmImage& image, std::string& outError)
{
    std::string filename;
    if (saveImageDialog(filename))
    {
        dialogAction_ColorSpace = std::packaged_task<void()>(
            [&image, filename, &outError]()
            {
                CmImageIO::writeImage(image, filename, dialogResult_ColorSpace, outError);
            });
        ImGui::OpenPopup(DIALOG_COLORSPACE);
    }
}

void imGuiDialogs()
{
    if (ImGui::BeginPopupModal(DIALOG_COLORSPACE))
    {
        ImGui::Text("Color space of the file:");

        static int selColorSpace = 0;
        imguiCombo("##ColorSpace", CMS::getAvailableColorSpaces(), &selColorSpace, true);

        if (ImGui::IsItemHovered() && selColorSpace >= 0)
        {
            std::string colorSpaceDesc = CMS::getConfig()
                ->getColorSpace(CMS::getAvailableColorSpaces()[selColorSpace].c_str())
                ->getDescription();
            ImGui::SetTooltip(colorSpaceDesc.c_str());
        }

        if (ImGui::Button("OK", buttonSize))
        {
            dialogResult_ColorSpace = CMS::getAvailableColorSpaces()[selColorSpace];
            dialogAction_ColorSpace();
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", buttonSize))
            ImGui::CloseCurrentPopup();
    }
}

void renderDiffPattern()
{
    CmImage& imgDiffPattern = *getImage("diff");

    if (diffPattern.hasRawData())
    {
        RealBloom::DiffractionPatternParams* dpParams = diffPattern.getParams();
        dpParams->contrast = vars.dp_contrast;
        dpParams->multiplier = vars.dp_multiplier;

        std::vector<float> buffer;
        diffPattern.getRgbaOutput(buffer);

        uint32_t width = dpParams->width;
        uint32_t height = dpParams->height;
        {
            std::lock_guard<CmImage> lock(imgDiffPattern);
            imgDiffPattern.resize(width, height, false);
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
    {
        std::cerr << "Failed to create a window.\n";
        return false;
    }
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

    // Setup ImGui style
    applyStyle_RealBloomGray();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

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
    fontRobotoBold = io->Fonts->AddFontFromFileTTF("./assets/fonts/RobotoCondensed-Bold.ttf", 19.0f * Config::S_UI_MAX_SCALE);
    fontMono = io->Fonts->AddFontFromFileTTF("./assets/fonts/mono/RobotoMono-Regular.ttf", 18.0f * Config::S_UI_MAX_SCALE);

    if (fontRoboto && fontRobotoBold && fontMono)
        return true;
    else
    {
        std::cerr << "The required fonts could not be loaded.\n";
        return false;
    }
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

void applyStyle_RealBloomGray()
{
    // RealBloom v2 style from ImThemes
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
    style.GrabMinSize = 20.0;
    style.GrabRounding = 2.0;
    style.TabRounding = 3.0;
    style.TabBorderSize = 1.0;
    style.TabMinWidthForCloseButton = 4.0;
    style.ColorButtonPosition = ImGuiDir_Left;
    style.ButtonTextAlign = ImVec2(0.5, 0.5);
    style.SelectableTextAlign = ImVec2(0.0, 0.5);

    style.Colors[ImGuiCol_Text] = ImVec4(0.9019607901573181, 0.9019607901573181, 0.9019607901573181, 1.0);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.8627451062202454, 0.8627451062202454, 0.8627451062202454, 0.3137255012989044);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1294117718935013, 0.1294117718935013, 0.1294117718935013, 1.0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.196078434586525, 0.196078434586525, 0.196078434586525, 1.0);
    style.Colors[ImGuiCol_Border] = ImVec4(1.0, 0.9960784316062927, 1.0, 0.0);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0, 0.0, 0.0, 0.0);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1.0, 1.0, 1.0, 0.062745101749897);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0, 1.0, 1.0, 0.196078434586525);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1098039224743843, 0.1098039224743843, 0.1098039224743843, 1.0);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.1568627506494522, 0.1568627506494522, 0.1568627506494522, 1.0);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.1098039224743843, 0.1098039224743843, 0.1098039224743843, 1.0);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0, 1.0, 1.0, 0.062745101749897);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0, 0.0, 0.0, 0.1176470592617989);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0, 1.0, 1.0, 0.2352941185235977);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0, 1.0, 1.0, 0.3921568691730499);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.0, 1.0, 1.0, 0.5490196347236633);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.6781115531921387, 0.4635863304138184, 0.1396968215703964, 1.0);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.6784313917160034, 0.4627451002597809, 0.1411764770746231, 0.7843137383460999);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2918455004692078, 0.2177876979112625, 0.1127300262451172, 1.0);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0, 0.9882352948188782, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0, 0.9960784316062927, 1.0, 0.196078434586525);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0, 0.9960784316062927, 1.0, 0.294117659330368);
    style.Colors[ImGuiCol_Header] = ImVec4(0.0, 0.0, 0.0, 0.2352941185235977);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.9882352948188782, 0.9960784316062927, 1.0, 0.062745101749897);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_Separator] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 1.0);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.0, 1.0, 1.0, 0.2352941185235977);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0, 1.0, 1.0, 0.2352941185235977);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.3921568691730499);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 1.0);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.0, 1.0, 1.0, 0.4705882370471954);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0, 1.0, 1.0, 0.6274510025978088);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.0, 1.0, 1.0, 0.4705882370471954);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0, 1.0, 1.0, 0.6274510025978088);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0, 1.0, 1.0, 0.1176470592617989);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0, 0.0, 0.0, 0.0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0, 1.0, 1.0, 0.03529411926865578);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.0, 1.0, 1.0, 0.1568627506494522);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.6235294342041016, 0.4235294163227081, 0.1215686276555061, 0.9019607901573181);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.5450980663299561, 0.3764705955982208, 0.1372549086809158, 0.7843137383460999);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0, 1.0, 1.0, 0.7176470756530762);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.2000000029802322);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0, 0.0, 0.0, 0.4705882370471954);
}

void cleanUp()
{
    for (auto image : images)
        delete image;
    CmImage::cleanUp();
    CMF::cleanUp();
    CMS::cleanUp();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}