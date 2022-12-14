#include "Main.h"

#define IMGUI_DIV ImGui::NewLine()

#define IMGUI_BOLD(TEXT) ImGui::PushFont(fontRobotoBold); \
ImGui::Text(TEXT); \
ImGui::PopFont();

// GUI
const char* glslVersion = "#version 130";
GLFWwindow* window = nullptr;
ImGuiIO* io = nullptr;
bool appRunning = true;

// Fonts
ImFont* fontRoboto = nullptr;
ImFont* fontRobotoBold = nullptr;
ImFont* fontMono = nullptr;

// Colors
const ImVec4 bgColor{ 0.13f, 0.13f, 0.13f, 1.00f };
const ImVec4 colorInfoText{ 0.328f, 0.735f, 0.910f, 1.0f };
const ImVec4 colorWarningText{ 0.940f, 0.578f, 0.282f, 1.0f };
const ImVec4 colorErrorText{ 0.950f, 0.300f, 0.228f, 1.0f };

// Used with ImGui::Combo
std::vector<std::string> activeComboList;

// Image slots
std::vector<std::shared_ptr<CmImage>> images;

std::vector<std::string> imageNames;
int selImageIndex = 0;
std::string selImageID = "";

float imageZoom = 1.0f;

// RealBloom modules
RealBloom::DiffractionPattern diff;
RealBloom::Dispersion disp;
RealBloom::Convolution conv;

// Variables used in controls and sliders
UiVars vars;
std::string convResUsage = "";

// Color Space Dialog
constexpr const char* DIALOG_COLORSPACE = "Color Space";
bool dlgParam_colorSpace_init = false;
bool dlgParam_colorSpace_writing = false;
std::string dlgParam_colorSpace_readSpace = "";
std::packaged_task<void()> dialogAction_colorSpace;
std::string dialogResult_colorSpace = "";

int main(int argc, char** argv)
{
    // Set Locale
    if (!std::setlocale(LC_ALL, Config::S_APP_LOCALE))
        std::cout << "Couldn't set locale to \"" << Config::S_APP_LOCALE << "\".\n";

    // Load config
    Config::load();
    Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::S_UI_MIN_SCALE), Config::S_UI_MAX_SCALE);

    // CLI
    CLI::init(argc, argv);

    // No GUI if there is command line input
    if (!CLI::hasCommands())
    {
        // Change the working directory so ImGui can load its
        // config properly
        SetCurrentDirectoryA(getExecDir().c_str());

        // Setup GLFW and ImGui
        if (!setupGLFW())
            return 1;
        if (!setupImGui())
            return 1;

        // GLEW for loading OpenGL extensions
        glewInit();
    }

    // Color Management System
    if (!CMS::init())
        return 1;

    // Color Matching Functions
    CMF::init();

    // XYZ Utility
    CmXYZ::init();

    // GUI-specific
    if (!CLI::hasCommands())
    {
        // Hide the console window
        ShowWindow(GetConsoleWindow(), SW_HIDE);

        // Create images to be used throughout the program
        addImage("aperture", "Aperture");
        addImage("disp-input", "Dispersion Input");
        addImage("disp-result", "Dispersion Result");
        addImage("cv-input", "Conv. Input");
        addImage("cv-kernel", "Conv. Kernel");
        addImage("cv-prev", "Conv. Preview");
        addImage("cv-result", "Conv. Result");

        // Set up images for diffraction pattern
        diff.setImgAperture(getImageByID("aperture"));
        diff.setImgDiffPattern(disp.getImgInputSrc());

        // Set up images for dispersion
        disp.setImgInput(getImageByID("disp-input"));
        disp.setImgDisp(getImageByID("disp-result"));

        // Set up images for convolution
        conv.setImgInput(getImageByID("cv-input"));
        conv.setImgKernel(getImageByID("cv-kernel"));
        conv.setImgConvPreview(getImageByID("cv-prev"));
        conv.setImgConvMix(getImageByID("cv-result"));

        // Will be shared with Convolution and Dispersion
        Async::putShared("convMixParamsChanged", &(vars.convMixParamsChanged));
        Async::putShared("selImageID", &selImageID);
    }

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
    std::shared_ptr<std::thread> convResUsageThread = nullptr;
    if (!CLI::hasCommands())
    {
        convResUsageThread = std::make_shared<std::thread>([]()
            {
                auto lastTime = std::chrono::system_clock::now();
                while (appRunning)
                {
                    if (getElapsedMs(lastTime) > 500)
                    {
                        updateConvParams();
                        convResUsage = conv.getResourceInfo();
                        lastTime = std::chrono::system_clock::now();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
    }

    // Main loop
    if (!CLI::hasCommands())
    {
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
                std::unique_lock<std::mutex> lock(Async::S_TASKS_MUTEX);
                while (!Async::S_TASKS.empty())
                {
                    auto task(std::move(Async::S_TASKS.front()));
                    Async::S_TASKS.pop_front();

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
    }

    // Process the command line input
    if (CLI::hasCommands())
    {
        disablePrintErr();
        CLI::proceed();
    }

    // Quit
    Config::save();
    if (!CLI::hasCommands())
        convResUsageThread->join();
    disp.cancel();
    conv.cancelConv();
    cleanUp();
    return 0;
}

void layout()
{
    // Selected image

    if (!selImageID.empty())
    {
        for (size_t i = 0; i < images.size(); i++)
        {
            if (images[i]->getID() == selImageID)
            {
                selImageIndex = i;
                break;
            }
        }
        selImageID = "";
    }

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
        if ((selImage.getID() == "cv-input") || (selImage.getID() == "cv-result"))
        {
            if (ImGui::Button("Compare", btnSize()))
            {
                if (selImage.getID() == "cv-input") selImageID = "cv-result";
                else selImageID = "cv-input";
            }
        }

        // Save
        {
            static std::string saveError = "";
            if (ImGui::Button("Save", btnSize()))
                saveImage(selImage, selImage.getID(), saveError);
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

        std::string imageName;
        if (selImage.getSourceName().empty())
            imageName = selImage.getName();
        else
            imageName = strFormat("%s (%s)", selImage.getName().c_str(), selImage.getSourceName().c_str());

        ImGui::PushFont(fontRobotoBold);
        ImGui::TextWrapped(imageName.c_str());
        ImGui::PopFont();

        ImGui::PushFont(fontMono);

        // Size
        uint32_t imageWidth = selImage.getWidth();
        uint32_t imageHeight = selImage.getHeight();
        ImGui::Text("%dx%d", imageWidth, imageHeight);

        // Zoom
        {
            ImGui::Text(strRightPadding(std::to_string((int)roundf(imageZoom * 100.0f)) + "%%", 5, false).c_str());

            ImGui::SameLine();
            if (ImGui::SmallButton("+"))
                imageZoom = fminf(fmaxf(imageZoom + 0.125f, 0.25f), 2.0f);

            ImGui::SameLine();
            if (ImGui::SmallButton("-"))
                imageZoom = fminf(fmaxf(imageZoom - 0.125f, 0.25f), 2.0f);

            ImGui::SameLine();
            if (ImGui::SmallButton("R"))
                imageZoom = 1.0f;
        }

        ImGui::PopFont();

        ImGui::Image(
            (void*)(intptr_t)(selImage.getGlTexture()),
            ImVec2(
                (float)imageWidth * imageZoom,
                (float)imageHeight * imageZoom
            ));

        ImGui::End();
    }

    // Controls
    CmImage& imgAperture = *getImageByID("aperture");
    CmImage& imgDispInput = *getImageByID("disp-input");
    CmImage& imgDispResult = *getImageByID("disp-result");
    CmImage& imgConvInput = *getImageByID("cv-input");
    CmImage& imgConvKernel = *getImageByID("cv-kernel");
    CmImage& imgConvResult = *getImageByID("cv-result");
    {
        ImGui::Begin("Diffraction Pattern");

        IMGUI_BOLD("APERTURE");

        {
            static std::string loadError;
            if (ImGui::Button("Browse Aperture##DP", btnSize()))
                loadImage(imgAperture, imgAperture.getID(), []() {}, loadError);
            imGuiText(loadError, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("DIFFRACTION");

        ImGui::Checkbox("Grayscale##DP", &(vars.dp_grayscale));

        if (ImGui::Button("Compute##DP", btnSize()))
        {
            updateDiffParams();
            diff.compute();
            vars.dispParamsChanged = true;
        }

        if (!diff.success())
        {
            std::string dpError = diff.getError();
            imGuiText(dpError, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("DISPERSION");

        {
            static std::string loadError;
            if (ImGui::Button("Browse Input##Disp", btnSize()))
            {
                CmImage* inputSrc = disp.getImgInputSrc();
                loadImage(*inputSrc, imgDispInput.getID(), []() { vars.dispParamsChanged = true; }, loadError);
            }
            imGuiText(loadError, true, false);
        }

        if (ImGui::SliderFloat("Exposure##Disp", &(vars.ds_exposure), -10, 10))
            vars.dispParamsChanged = true;
        if (ImGui::SliderFloat("Contrast##Disp", &(vars.ds_contrast), -1, 1))
            vars.dispParamsChanged = true;

        if (vars.dispParamsChanged)
        {
            vars.dispParamsChanged = false;
            updateDispParams();
            disp.previewInput();
            selImageID = "disp-input";
        }

        ImGui::SliderInt("Steps##Disp", &(vars.ds_steps), 32, 1024);
        ImGui::SliderFloat("Amount##Disp", &(vars.ds_amount), 0, 1);
        ImGui::ColorEdit3("Color##Disp", vars.ds_col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha);
        ImGui::SliderInt("Threads##Disp", &(vars.ds_numThreads), 1, getMaxNumThreads());

        if (ImGui::Button("Apply Dispersion##Disp", btnSize()))
        {
            imgDispResult.resize(imgDispInput.getWidth(), imgDispInput.getHeight(), true);
            imgDispResult.fill(std::array<float, 4>{ 0, 0, 0, 1 }, true);
            imgDispResult.moveToGPU();
            selImageID = "disp-result";

            updateDispParams();
            disp.compute();
        }

        if (disp.isWorking())
        {
            if (ImGui::Button("Cancel##Disp", btnSize()))
                disp.cancel();
        }

        std::string dispStats = disp.getStats();
        imGuiText(dispStats, disp.hasFailed(), false);

        ImGui::NewLine();
        imGuiDialogs();
        ImGui::End(); // Diffraction Pattern

        ImGui::Begin("Convolution");

        IMGUI_BOLD("INPUT");

        {
            static std::string loadError;
            if (ImGui::Button("Browse Input##Conv", btnSize()))
                loadImage(imgConvInput, imgConvInput.getID(), []() { vars.convThresholdChanged = true; }, loadError);
            imGuiText(loadError, true, false);
        }

        {
            static std::string loadError;
            if (ImGui::Button("Browse Kernel##Conv", btnSize()))
            {
                CmImage* kernelSrc = conv.getImgKernelSrc();
                loadImage(*kernelSrc, imgConvKernel.getID(), []() { vars.convParamsChanged = true; }, loadError);
            }
            imGuiText(loadError, true, false);
        }

        IMGUI_DIV;
        IMGUI_BOLD("KERNEL");

        if (ImGui::SliderFloat("Exposure##Kernel", &(vars.cv_kernelExposure), -10, 10))
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
            }
            else
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
            }
            else
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
            selImageID = "cv-kernel";
        }

        IMGUI_DIV;
        IMGUI_BOLD("CONVOLUTION");

        const char* const convMethodItems[]{ "FFT (CPU)", "Naive (CPU)", "Naive (GPU)" };
        ImGui::Combo("Method##Conv", &(vars.cv_method), convMethodItems, 3);

        if (vars.cv_method == (int)RealBloom::ConvolutionMethod::FFT_CPU)
        {
            // No parameters yet
        }
        else if (vars.cv_method == (int)RealBloom::ConvolutionMethod::NAIVE_CPU)
        {
            // Threads
            ImGui::SliderInt("Threads##Conv", &(vars.cv_numThreads), 1, getMaxNumThreads());

            // Warning Text
            if (vars.cv_numThreads > std::max(1u, getMaxNumThreads() / 2))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colorWarningText);
                if (vars.cv_numThreads == getMaxNumThreads())
                    ImGui::TextWrapped(
                        "Maximizing the number of threads might result in slowdowns and "
                        "potential crashes.");
                else
                    ImGui::TextWrapped(
                        "Using over half the number of concurrent threads supported "
                        "by the hardware is not recommended.");
                ImGui::PopStyleColor();
            }
        }
        else if (vars.cv_method == (int)RealBloom::ConvolutionMethod::NAIVE_GPU)
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
                selImageID = "cv-prev";
                vars.convThresholdSwitchImage = false;
            }
            vars.convThresholdChanged = false;
        }

        ImGui::Checkbox("Auto-Exposure##Kernel", &(vars.cv_kernelAutoExposure));

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Adjust the exposure to match the brightness of the convolution layer to that of the input.");

        if (ImGui::Button("Convolve##Conv", btnSize()))
        {
            imgConvResult.resize(imgConvInput.getWidth(), imgConvInput.getHeight(), true);
            imgConvResult.fill(std::array<float, 4>{ 0, 0, 0, 1 }, true);
            imgConvResult.moveToGPU();
            selImageID = "cv-result";

            updateConvParams();
            conv.convolve();
        }

        if (ImGui::IsItemHovered() && !convResUsage.empty())
            ImGui::SetTooltip(convResUsage.c_str());

        if (conv.isWorking())
        {
            if (ImGui::Button("Cancel##Conv", btnSize()))
                conv.cancelConv();
        }

        std::string convTime, convStatus;
        uint32_t convStatType = 1;
        conv.getConvStats(convTime, convStatus, convStatType);

        if (!convTime.empty())
            ImGui::Text(convTime.c_str());

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
            }
            else
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
            if (ImGui::SliderFloat("Input##Conv", &(vars.cm_inputMix), 0, 1))
                vars.convMixParamsChanged = true;
            if (ImGui::SliderFloat("Conv.##Conv", &(vars.cm_convMix), 0, 1))
                vars.convMixParamsChanged = true;
            if (ImGui::SliderFloat("Exposure##Conv", &(vars.cm_convExposure), -10, 10))
                vars.convMixParamsChanged = true;
        }
        else
        {
            if (ImGui::SliderFloat("Mix##Conv", &(vars.cm_mix), 0, 1))
                vars.convMixParamsChanged = true;
            if (ImGui::SliderFloat("Exposure##Conv", &(vars.cm_convExposure), -10, 10))
                vars.convMixParamsChanged = true;
        }

        if (ImGui::Button("Show Conv. Layer##Conv", btnSize()))
        {
            vars.cm_additive = false;
            vars.cm_mix = 1.0f;
            vars.cm_convExposure = 0.0f;
            vars.convMixParamsChanged = true;
        }

        if (vars.convMixParamsChanged)
        {
            vars.convMixParamsChanged = false;
            conv.mixConv(vars.cm_additive, vars.cm_inputMix, vars.cm_convMix, vars.cm_mix, vars.cm_convExposure);
            selImageID = "cv-result";
        }

        ImGui::NewLine();
        imGuiDialogs();
        ImGui::End(); // Convolution
    }

    // Color Management
    {
        ImGui::Begin("Color Management");

        IMGUI_BOLD("VIEW");

        {

            // Exposure
            if (ImGui::SliderFloat("Exposure##CMS", &(vars.cms_exposure), -10, 10))
            {
                CMS::setExposure(vars.cms_exposure);
                vars.cmsParamsChanged = true;
            }

            const std::vector<std::string>& displays = CMS::getAvailableDisplays();
            const std::vector<std::string>& views = CMS::getAvailableViews();
            const std::vector<std::string>& looks = CMS::getAvailableLooks();
            const std::string& activeDisplay = CMS::getActiveDisplay();
            const std::string& activeView = CMS::getActiveView();
            const std::string& activeLook = CMS::getActiveLook();

            static int selDisplay = -1;
            static int selView = -1;
            static int selLook = -1;

            // selDisplay
            for (size_t i = 0; i < displays.size(); i++)
                if (activeDisplay == displays[i])
                {
                    selDisplay = i;
                    break;
                }

            // selView
            for (size_t i = 0; i < views.size(); i++)
                if (activeView == views[i])
                {
                    selView = i;
                    break;
                }

            // selLook
            for (size_t i = 0; i < looks.size(); i++)
                if (activeLook == looks[i])
                {
                    selLook = i;
                    break;
                }
            if (activeLook.empty()) selLook = 0;

            // Display
            if (imguiCombo("Display##CMS", displays, &selDisplay, false))
            {
                CMS::setActiveDisplay(displays[selDisplay]);
                vars.cmsParamsChanged = true;

                // Update selView
                ptrdiff_t activeViewIndex = std::distance(views.begin(), std::find(views.begin(), views.end(), activeView));
                if (activeViewIndex < views.size())
                    selView = activeViewIndex;
            }

            // View
            if (imguiCombo("View##CMS", views, &selView, false))
            {
                CMS::setActiveView(views[selView]);
                vars.cmsParamsChanged = true;
            }

            // Look
            if (imguiCombo("Look##CMS", looks, &selLook, false))
            {
                CMS::setActiveLook(looks[selLook]);
                vars.cmsParamsChanged = true;
            }

            // Update
            if (vars.cmsParamsChanged)
            {
                vars.cmsParamsChanged = false;
                for (auto& img : images)
                    img->moveToGPU();
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

        IMGUI_DIV;
        IMGUI_BOLD("COLOR MATCHING");

        {

            // Get available CMF Tables
            std::vector<CmfTableInfo> cmfTables = CMF::getAvailableTables();

            // Convert to a string vector
            std::vector<std::string> cmfTableNames;
            for (const auto& tbl : cmfTables)
                cmfTableNames.push_back(tbl.name);

            // Get the active table index
            static int selCmfTable = -1;
            if (CMF::hasTable())
            {
                CmfTableInfo activeTable = CMF::getActiveTableInfo();
                for (size_t i = 0; i < cmfTables.size(); i++)
                    if ((cmfTables[i].name == activeTable.name) && (cmfTables[i].path == activeTable.path))
                    {
                        selCmfTable = i;
                        break;
                    }
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
            static std::string cmfPreviewError = "";
            if (ImGui::Button("Preview##CMF", btnSize()) || tableChanged)
            {
                tableChanged = false;
                cmfPreviewError = "";
                selImageID = "disp-result";

                disp.cancel();
                try
                {
                    disp.previewCmf(CMF::getActiveTable().get());
                }
                catch (const std::exception& e)
                {
                    cmfPreviewError = e.what();
                }
            }
            imGuiText(cmfPreviewError, true, false);

        }

        IMGUI_DIV;
        IMGUI_BOLD("XYZ CONVERSION");

        {

            const std::vector<std::string>& internalSpaces = CMS::getInternalColorSpaces();
            const std::vector<std::string>& userSpaces = CMS::getAvailableColorSpaces();
            XyzConversionInfo currentInfo = CmXYZ::getConversionInfo();

            static int selUserSpace = -1;
            static int selCommonInternal = -1;
            static int selCommonUser = -1;
            static int selMethod = 0;
            static bool xcParamsChanged = false;

            selMethod = (int)currentInfo.method;

            // selUserSpace
            for (size_t i = 0; i < userSpaces.size(); i++)
                if (currentInfo.userSpace == userSpaces[i])
                {
                    selUserSpace = i;
                    break;
                }

            // selCommonInternal
            for (size_t i = 0; i < internalSpaces.size(); i++)
                if (currentInfo.commonInternal == internalSpaces[i])
                {
                    selCommonInternal = i;
                    break;
                }

            // selCommonUser
            for (size_t i = 0; i < userSpaces.size(); i++)
                if (currentInfo.commonUser == userSpaces[i])
                {
                    selCommonUser = i;
                    break;
                }

            // Method
            static std::vector<std::string> methodNames;
            {
                static bool methodNamesInit = false;
                if (!methodNamesInit)
                {
                    methodNamesInit = true;
                    methodNames.push_back("None");
                    methodNames.push_back("User Config");
                    methodNames.push_back("Common Space");
                }
            }
            if (imguiCombo("Method##XC", methodNames, &selMethod, false))
                xcParamsChanged = true;

            // Color spaces
            if (selMethod == (int)XyzConversionMethod::UserConfig)
            {
                // XYZ space
                if (imguiCombo("XYZ I-E##XC", userSpaces, &selUserSpace, false))
                    xcParamsChanged = true;
                if (ImGui::IsItemHovered() && selUserSpace >= 0)
                {
                    std::string desc = CMS::getColorSpaceDesc(CMS::getConfig(), userSpaces[selUserSpace]);
                    if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
                }
            }
            else if (selMethod == (int)XyzConversionMethod::CommonSpace)
            {
                // Internal space
                if (imguiCombo("Internal##XC", internalSpaces, &selCommonInternal, false))
                    xcParamsChanged = true;
                if (ImGui::IsItemHovered() && selCommonInternal >= 0)
                {
                    std::string desc = CMS::getColorSpaceDesc(CMS::getInternalConfig(), internalSpaces[selCommonInternal]);
                    if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
                }

                // User space
                if (imguiCombo("User##XC", userSpaces, &selCommonUser, false))
                    xcParamsChanged = true;
                if (ImGui::IsItemHovered() && selCommonUser >= 0)
                {
                    std::string desc = CMS::getColorSpaceDesc(CMS::getConfig(), userSpaces[selCommonUser]);
                    if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
                }
            }

            // Update
            if (xcParamsChanged)
            {
                xcParamsChanged = false;

                XyzConversionInfo info;
                info.method = (XyzConversionMethod)selMethod;

                if (selUserSpace >= 0)
                    info.userSpace = userSpaces[selUserSpace];

                if (selCommonInternal >= 0)
                    info.commonInternal = internalSpaces[selCommonInternal];

                if (selCommonUser >= 0)
                    info.commonUser = userSpaces[selCommonUser];

                CmXYZ::setConversionInfo(info);
            }

            // CmXYZ Error
            if (!CmXYZ::ok())
                imGuiText(CmXYZ::getError(), true, false);

        }

        ImGui::NewLine();
        imGuiDialogs();
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
        static std::string uiRenderer = strFormat("UI Renderer:\n%s", (const char*)glGetString(GL_RENDERER));

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
        if (ImGui::Button("GitHub", btnSize()))
            openURL(Config::S_GITHUB_URL);
        if (ImGui::Button("Tutorial", btnSize()))
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
        }
        else
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

void addImage(const std::string& id, const std::string& name)
{
    std::shared_ptr<CmImage> img = std::make_shared<CmImage>(id, name);
    images.push_back(img);
    imageNames.push_back(name);
}

CmImage* getImageByID(const std::string& id)
{
    for (auto& img : images)
        if (img->getID() == id)
            return img.get();
    return nullptr;
}

void loadImage(CmImage& image, const std::string& dlgID, std::function<void()> onLoad, std::string& outError)
{
    std::string filename;
    if (FileDialogs::openDialog(dlgID, filename))
    {
        dialogAction_colorSpace = std::packaged_task<void()>(
            [&image, onLoad, filename, &outError]()
            {
                outError = "";
                try
                {
                    CmImageIO::readImage(image, filename, dialogResult_colorSpace);
                    selImageID = image.getID();
                    onLoad();
                }
                catch (const std::exception& e)
                {
                    outError = e.what();
                }
            });

        dlgParam_colorSpace_init = true;
        dlgParam_colorSpace_writing = false;
        CmImageIO::readImageColorSpace(filename, dlgParam_colorSpace_readSpace);
        ImGui::OpenPopup(DIALOG_COLORSPACE);
    }
}

void saveImage(CmImage& image, const std::string& dlgID, std::string& outError)
{
    std::string filename;
    if (FileDialogs::saveDialog(dlgID, "exr", filename))
    {
        dialogAction_colorSpace = std::packaged_task<void()>(
            [&image, filename, &outError]()
            {
                outError = "";
                try
                {
                    CmImageIO::writeImage(image, filename, dialogResult_colorSpace);
                }
                catch (const std::exception& e)
                {
                    outError = e.what();
                }
            });

        dlgParam_colorSpace_init = true;
        dlgParam_colorSpace_writing = true;
        dlgParam_colorSpace_readSpace = "";
        ImGui::OpenPopup(DIALOG_COLORSPACE);
    }
}

void imGuiDialogs()
{
    const std::vector<std::string>& userSpaces = CMS::getAvailableColorSpaces();

    if (ImGui::BeginPopupModal(DIALOG_COLORSPACE, 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Color space of the file:");

        static int selReadSpace = 0;
        static int selWriteSpace = 0;

        if (dlgParam_colorSpace_init)
        {
            dlgParam_colorSpace_init = false;
            if (!dlgParam_colorSpace_writing)
            {
                if (!(dlgParam_colorSpace_readSpace.empty()))
                {
                    for (size_t i = 0; i < userSpaces.size(); i++)
                        if (userSpaces[i] == dlgParam_colorSpace_readSpace)
                        {
                            selReadSpace = i;
                            break;
                        }
                }
            }
        }

        int* selSpace = (dlgParam_colorSpace_writing ? (&selWriteSpace) : (&selReadSpace));

        if (dlgParam_colorSpace_writing)
            imguiCombo("##WriteColorSpace", userSpaces, selSpace, true);
        else
            imguiCombo("##ReadColorSpace", userSpaces, selSpace, true);

        if (ImGui::IsItemHovered() && *selSpace >= 0)
        {
            std::string colorSpaceDesc = CMS::getConfig()
                ->getColorSpace(userSpaces[*selSpace].c_str())
                ->getDescription();
            ImGui::SetTooltip(colorSpaceDesc.c_str());
        }

        if (ImGui::Button("OK", dlgBtnSize()))
        {
            dialogResult_colorSpace = userSpaces[*selSpace];
            dialogAction_colorSpace();
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel", dlgBtnSize()))
            ImGui::CloseCurrentPopup();
        ImGui::NewLine();
    }
}

void updateDiffParams()
{
    RealBloom::DiffractionPatternParams* params = diff.getParams();
    params->grayscale = vars.dp_grayscale;
}

void updateDispParams()
{
    RealBloom::DispersionParams* params = disp.getParams();
    params->exposure = vars.ds_exposure;
    params->contrast = vars.ds_contrast;
    params->steps = vars.ds_steps;
    params->amount = vars.ds_amount;
    params->color = std::array<float, 3>{ vars.ds_col[0], vars.ds_col[1], vars.ds_col[2] };
    params->numThreads = vars.ds_numThreads;
}

void updateConvParams()
{
    RealBloom::ConvolutionParams* params = conv.getParams();
    params->methodInfo.method = (RealBloom::ConvolutionMethod)(vars.cv_method);
    params->methodInfo.numThreads = vars.cv_numThreads;
    params->methodInfo.numChunks = vars.cv_numChunks;
    params->methodInfo.chunkSleep = vars.cv_chunkSleep;
    params->kernelExposure = vars.cv_kernelExposure;
    params->kernelContrast = vars.cv_kernelContrast;
    params->kernelRotation = vars.cv_kernelRotation;
    params->kernelScaleX = vars.cv_kernelScale[0];
    params->kernelScaleY = vars.cv_kernelScale[1];
    params->kernelCropX = vars.cv_kernelCrop[0];
    params->kernelCropY = vars.cv_kernelCrop[1];
    params->kernelPreviewCenter = vars.cv_kernelPreviewCenter;
    params->kernelCenterX = vars.cv_kernelCenter[0];
    params->kernelCenterY = vars.cv_kernelCenter[1];
    params->convThreshold = vars.cv_convThreshold;
    params->convKnee = vars.cv_convKnee;
    params->kernelAutoExposure = vars.cv_kernelAutoExposure;
}

ImVec2 btnSize()
{
    return { -1.0f, 28.0f * Config::UI_SCALE };
}

ImVec2 dlgBtnSize()
{
    return { 300.0f * Config::UI_SCALE, 28.0f * Config::UI_SCALE };
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

    fontRoboto = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/RobotoCondensed-Regular.ttf").c_str(),
        18.0f * Config::S_UI_MAX_SCALE);

    fontRobotoBold = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/RobotoCondensed-Bold.ttf").c_str(),
        19.0f * Config::S_UI_MAX_SCALE);

    fontMono = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/mono/RobotoMono-Regular.ttf").c_str(),
        18.0f * Config::S_UI_MAX_SCALE);

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
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.1098039224743843, 0.1098039224743843, 0.1098039224743843, 1.0);
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
    if (!CLI::hasCommands())
    {
        clearVector(images);
        imgui_widgets_cleanup_cmimages();
    }

    CmImage::cleanUp();
    CMF::cleanUp();
    CMS::cleanUp();

    if (!CLI::hasCommands())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    CLI::cleanUp();
}
