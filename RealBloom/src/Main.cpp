#include "Main.h"

// GUI

static int glVersionMajor = 3;
static int glVersionMinor = 2;
static const char* glslVersion = "#version 150";

static GLFWwindow* window = nullptr;
static ImGuiIO* io = nullptr;
static bool appRunning = true;

static bool showRendererName = true;
static bool showFPS = true;
static bool showDebugPanel = true;

// Fonts
static ImFont* fontRoboto = nullptr;
static ImFont* fontRobotoBold = nullptr;
static ImFont* fontMono = nullptr;

// Colors
static const ImVec4 colorBackground{ 0.13f, 0.13f, 0.13f, 1.00f };
static const ImVec4 colorImageBorder{ 0.05f, 0.05f, 0.05f, 1.0f };
static const ImVec4 colorInfoText{ 0.328f, 0.735f, 0.910f, 1.0f };
static const ImVec4 colorWarningText{ 0.940f, 0.578f, 0.282f, 1.0f };
static const ImVec4 colorErrorText{ 0.950f, 0.300f, 0.228f, 1.0f };

// Image slots
static std::vector<std::shared_ptr<CmImage>> images;
static std::vector<std::string> imageNames;
static int selImageIndex = 0;
static std::string selImageID = "";
static float imageZoom = 1.0f;

// Map slot IDs to internal source images in modules
static std::unordered_map<std::string, ImageSourceInfo> imageSourceInfoMap;

// RealBloom modules
static RealBloom::Diffraction diff;
static RealBloom::Dispersion disp;
static RealBloom::Convolution conv;

// Variables for modules

static bool convInputTransformParamsChanged = false;
static bool convKernelTransformParamsChanged = false;

static bool convThresholdChanged = false;
static bool convThresholdSwitchImage = false;
static bool convBlendParamsChanged = false;

static bool diffInputTransformParamsChanged = false;
static bool dispInputTransformParamsChanged = false;

static std::string convResUsage = "";

// Constants
static constexpr float EXPOSURE_RANGE = 10.0f;

// Dialog Params
DialogParams_MoveTo dialogParams_MoveTo;

int main(int argc, char** argv)
{
    // Set Locale
    if (!std::setlocale(LC_ALL, Config::APP_LOCALE))
        std::cout << "Couldn't set locale to \"" << Config::APP_LOCALE << "\".\n";

    // Load config
    Config::load();

    // CLI
    CLI::Interface::init(argc, argv);

    // GUI-specific
    if (!CLI::Interface::active())
    {
        // Change the working directory so ImGui can load its
        // config properly
        SetCurrentDirectoryA(getExecDir().c_str());

        // Setup GLFW
        if (!setupGLFW())
        {
            std::cout << "Failed to initialize GLFW.\n";
            return 1;
        }

        // GLEW for loading OpenGL extensions
        glewExperimental = GL_TRUE;
        GLenum glewInitResult = glewInit();
        if (glewInitResult != GLEW_OK)
        {
            std::cout << "Failed to initialize GLEW: " << glewInitResult << "\n";
            return 1;
        }

        // Setup ImGui
        if (!setupImGui())
        {
            std::cout << "Failed to initialize ImGui.\n";
            return 1;
        }
    }

    // Color Management System
    if (!CMS::init())
        return 1;

    // Color Matching Functions
    CMF::init();

    // XYZ Utility
    CmXYZ::init();

    // Color Managed Image IO
    CmImageIO::init();

    // GUI-specific
    if (!CLI::Interface::active())
    {
        // Minimize the console window
        PostMessage(GetConsoleWindow(), WM_SYSCOMMAND, SC_MINIMIZE, 0);

        // Native File Dialog Extended
        NFD_Init();

        // Create images to be used throughout the program
        addImage("diff-input", "Diffraction Input");
        addImage("diff-result", "Diffraction Result");
        addImage("disp-input", "Dispersion Input");
        addImage("disp-result", "Dispersion Result");
        addImage("cv-input", "Conv. Input");
        addImage("cv-kernel", "Conv. Kernel");
        addImage("cv-prev", "Conv. Preview");
        addImage("cv-result", "Conv. Result");

        // Set up images for diffraction
        diff.setImgInput(getImageByID("diff-input"));
        diff.setImgDiff(getImageByID("diff-result"));

        // Set up images for dispersion
        disp.setImgInput(getImageByID("disp-input"));
        disp.setImgDisp(getImageByID("disp-result"));

        // Set up images for convolution
        conv.setImgInput(getImageByID("cv-input"));
        conv.setImgKernel(getImageByID("cv-kernel"));
        conv.setImgConvPreview(getImageByID("cv-prev"));
        conv.setImgConvResult(getImageByID("cv-result"));

        // Map slot IDs to internal source images in modules
        imageSourceInfoMap["diff-input"] = { diff.getImgInputSrc(), &diffInputTransformParamsChanged };
        imageSourceInfoMap["disp-input"] = { disp.getImgInputSrc(), &dispInputTransformParamsChanged };
        imageSourceInfoMap["cv-input"] = { conv.getImgInputSrc(), &convInputTransformParamsChanged };
        imageSourceInfoMap["cv-kernel"] = { conv.getImgKernelSrc(), &convKernelTransformParamsChanged };
    }

    // To kill child processes when the parent dies
    // https://stackoverflow.com/a/53214/18049911
    HANDLE hJobObject = CreateJobObjectA(NULL, NULL);
    if (hJobObject != NULL)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        Async::emitSignal("hJobObject", (void*)hJobObject);
    }

    // Update resource usage info for convolution (GUI)
    std::shared_ptr<std::jthread> convResUsageThread = nullptr;
    if (!CLI::Interface::active())
    {
        convResUsageThread = std::make_shared<std::jthread>([]()
            {
                auto lastTime = std::chrono::system_clock::now();
                while (appRunning)
                {
                    if (getElapsedMs(lastTime) > 500)
                    {
                        convResUsage = conv.getResourceInfo();
                        lastTime = std::chrono::system_clock::now();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
    }

    // Main loop (GUI)
    if (!CLI::Interface::active())
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

            // Process scheduled jobs
            Async::processJobs("Main");

            // UI Layout
            ImGui::PushFont(fontRoboto);
            layoutAll();
            ImGui::PopFont();

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(colorBackground.x * colorBackground.w, colorBackground.y * colorBackground.w, colorBackground.z * colorBackground.w, colorBackground.w);
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

    // CLI-specific
    if (CLI::Interface::active())
    {
        // Create OpenGL context

        std::string ctxError;
        bool ctxSuccess = oglOneTimeContext(
            glVersionMajor, glVersionMinor,
            []()
            {
                // Start the CLI
                CLI::Interface::proceed();
            },
            ctxError);

        if (!ctxSuccess)
            printError(__FUNCTION__, "", strFormat("OpenGL context initialization error: %s", ctxError.c_str()));
    }

    // Quit

    Config::save();

    if (!CLI::Interface::active())
    {
        convResUsageThread->join();
        convResUsageThread = nullptr;
    }

    disp.cancel();
    conv.cancel();

    cleanUp();

    return 0;
}

void layoutAll()
{
    layoutImagePanels();
    layoutColorManagement();
    layoutMisc();
    layoutConvolution();
    layoutDiffraction();
    layoutDebug();
}

void layoutImagePanels()
{
    // Selected image

    {
        SignalValue v;
        if (Async::readSignalLast("selImageID", v))
            selImageID = std::get<std::string>(v);
    }

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

    // Image List
    {
        ImGui::Begin("Image List");

        imGuiBold("SLOTS");

        ImGui::PushItemWidth(-1);
        ImGui::ListBox(
            "##ImageList_Slots",
            &selImageIndex,
            [](void* data, int index, const char** outText)
            {
                *outText = imageNames[index].c_str();
                return true;
            },
            nullptr, images.size(), images.size());
        ImGui::PopItemWidth();

        // Compare input and result slots
        if ((selImage.getID() == "disp-input") || (selImage.getID() == "disp-result"))
        {
            if (ImGui::Button("Compare##ImageList_0", btnSize()))
            {
                if (selImage.getID() == "disp-input") selImageID = "disp-result";
                else selImageID = "disp-input";
            }
        }
        else if ((selImage.getID() == "cv-input") || (selImage.getID() == "cv-result"))
        {
            if (ImGui::Button("Compare##ImageList_1", btnSize()))
            {
                if (selImage.getID() == "cv-input") selImageID = "cv-result";
                else selImageID = "cv-input";
            }
        }
        else if ((selImage.getID() == "diff-input") || (selImage.getID() == "diff-result"))
        {
            if (ImGui::Button("Compare##ImageList_2", btnSize()))
            {
                if (selImage.getID() == "diff-input") selImageID = "diff-result";
                else selImageID = "diff-input";
            }
        }

        // Reset
        if (ImGui::Button("Reset##ImageList", btnSize()))
        {
            selImage.reset(true);

            if (imageSourceInfoMap.contains(selImage.getID()))
            {
                auto& srcInfo = imageSourceInfoMap[selImage.getID()];
                srcInfo.imageSource->reset(true);
                srcInfo.updateParamsChanged();
            }
        }

        // Move To
        if (ImGui::Button("Move To##ImageList", btnSize()))
        {
            dialogParams_MoveTo.selSourceImage = selImageIndex;
            dialogParams_MoveTo.selDestImage = (selImageIndex + 1) % (int)images.size();

            ImGui::OpenPopup(DIALOG_TITLE_MOVETO);
        }

        // Save
        {
            static std::string saveError = "";
            if (ImGui::Button("Save##ImageList", btnSize()))
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
            ImVec2((float)imageWidth * imageZoom, (float)imageHeight * imageZoom),
            { 0, 0 },
            { 1,1 },
            { 1, 1, 1, 1 },
            colorImageBorder);

        ImGui::End();
    }
}

void layoutColorManagement()
{
    const std::vector<std::string>& internalSpaces = CMS::getInternalColorSpaces();
    const std::vector<std::string>& userSpaces = CMS::getColorSpaces();

    static bool cmsParamsChanged = false;

    ImGui::Begin("Color Management");

    imGuiBold("VIEW");

    {

        // Exposure
        float cmsExposure = CMS::getExposure();
        if (ImGui::SliderFloat("Exposure##CMS", &cmsExposure, -EXPOSURE_RANGE, EXPOSURE_RANGE))
        {
            CMS::setExposure(cmsExposure);
            cmsParamsChanged = true;
        }

        const std::vector<std::string>& displays = CMS::getDisplays();
        const std::vector<std::string>& views = CMS::getViews();
        const std::vector<std::string>& looks = CMS::getLooks();
        const std::string& activeDisplay = CMS::getActiveDisplay();
        const std::string& activeView = CMS::getActiveView();
        const std::string& activeLook = CMS::getActiveLook();

        int selDisplay = findIndex(displays, activeDisplay);
        int selView = findIndex(views, activeView);
        int selLook = findIndex(looks, activeLook);

        if (activeLook.empty()) selLook = 0;

        // Display
        if (imGuiCombo("Display##CMS", displays, &selDisplay, false))
        {
            CMS::setActiveDisplay(displays[selDisplay]);
            CMS::updateProcessors();
            cmsParamsChanged = true;

            // Update selView
            ptrdiff_t activeViewIndex = std::distance(views.begin(), std::find(views.begin(), views.end(), activeView));
            if (activeViewIndex < (int)(views.size()))
                selView = activeViewIndex;
        }

        // View
        if (imGuiCombo("View##CMS", views, &selView, false))
        {
            CMS::setActiveView(views[selView]);
            CMS::updateProcessors();
            cmsParamsChanged = true;
        }

        // Look
        if (imGuiCombo("Look##CMS", looks, &selLook, false))
        {
            CMS::setActiveLook(looks[selLook]);
            CMS::updateProcessors();
            cmsParamsChanged = true;
        }

        // Update
        if (cmsParamsChanged)
        {
            cmsParamsChanged = false;
            for (auto& img : images)
                img->moveToGPU();
        }

    }

    imGuiDiv();
    imGuiBold("INFO");

    {

        // Working Space
        static std::string workingSpace = CMS::getWorkingSpace();
        static std::string workingSpaceDesc = CMS::getWorkingSpaceDesc();
        ImGui::TextWrapped("Working Space: %s", workingSpace.c_str());
        if (ImGui::IsItemHovered() && !workingSpaceDesc.empty())
            ImGui::SetTooltip(workingSpaceDesc.c_str());

        // CMS Error
        if (!CMS::getStatus().isOK())
            imGuiText(CMS::getStatus().getError(), true, false);

    }

    imGuiDiv();
    imGuiBold("IMAGE IO");

    {

        int selInputSpace = findIndex(userSpaces, CmImageIO::getInputSpace());
        int selOutputSpace = findIndex(userSpaces, CmImageIO::getOutputSpace());
        int selNonLinearSpace = findIndex(userSpaces, CmImageIO::getNonLinearSpace());

        // Input Color Space
        if (imGuiCombo("Input##IIO", userSpaces, &selInputSpace, false))
            CmImageIO::setInputSpace(userSpaces[selInputSpace]);
        if (ImGui::IsItemHovered() && selInputSpace >= 0)
        {
            std::string desc = CMS::getColorSpaceDesc(CMS::getConfig(), userSpaces[selInputSpace]);
            if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
        }

        // Output Color Space
        if (imGuiCombo("Output##IIO", userSpaces, &selOutputSpace, false))
            CmImageIO::setOutputSpace(userSpaces[selOutputSpace]);
        if (ImGui::IsItemHovered() && selOutputSpace >= 0)
        {
            std::string desc = CMS::getColorSpaceDesc(CMS::getConfig(), userSpaces[selOutputSpace]);
            if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
        }

        // Non-Linear Color Space
        if (imGuiCombo("Non-Linear##IIO", userSpaces, &selNonLinearSpace, false))
            CmImageIO::setNonLinearSpace(userSpaces[selNonLinearSpace]);
        if (ImGui::IsItemHovered() && selNonLinearSpace >= 0)
        {
            std::string desc = CMS::getColorSpaceDesc(CMS::getConfig(), userSpaces[selNonLinearSpace]);
            if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
        }

        // Auto-Detect
        bool autoDetect = CmImageIO::getAutoDetect();
        if (ImGui::Checkbox("Auto-Detect##IIO", &autoDetect))
            CmImageIO::setAutoDetect(autoDetect);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Attempt to detect the input color space for images exported by RealBloom");

        // Apply View Transform
        bool applyViewTransform = CmImageIO::getApplyViewTransform();
        if (ImGui::Checkbox("Apply View Transform##IIO", &applyViewTransform))
            CmImageIO::setApplyViewTransform(applyViewTransform);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Apply view transform on linear images");

    }

    imGuiDiv();
    imGuiBold("COLOR MATCHING");

    {

        // Get the available CMF Tables
        std::vector<CmfTableInfo> cmfTables = CMF::getTables();

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
        if (imGuiCombo("CMF##CMF", cmfTableNames, &selCmfTable, false))
        {
            CMF::setActiveTable(cmfTables[selCmfTable]);
            tableChanged = true;
        }

        // CMF Details
        std::string cmfTableDetails = CMF::getActiveTableDetails();
        if (ImGui::IsItemHovered() && CMF::hasTable() && !cmfTableDetails.empty())
            ImGui::SetTooltip(cmfTableDetails.c_str());

        // CMF Error
        if (!CMF::getStatus().isOK())
            imGuiText(CMF::getStatus().getError(), true, false);

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

    imGuiDiv();
    imGuiBold("XYZ CONVERSION");

    {

        XyzConversionInfo currentInfo = CmXYZ::getConversionInfo();

        int selUserSpace = findIndex(userSpaces, currentInfo.userSpace);
        int selCommonInternal = findIndex(internalSpaces, currentInfo.commonInternal);
        int selCommonUser = findIndex(userSpaces, currentInfo.commonUser);
        int selMethod = (int)currentInfo.method;

        static bool xcParamsChanged = false;

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
        if (imGuiCombo("Method##XC", methodNames, &selMethod, false))
            xcParamsChanged = true;

        // Color spaces
        if (selMethod == (int)XyzConversionMethod::UserConfig)
        {
            // XYZ space
            if (imGuiCombo("XYZ I-E##XC", userSpaces, &selUserSpace, false))
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
            if (imGuiCombo("Internal##XC", internalSpaces, &selCommonInternal, false))
                xcParamsChanged = true;
            if (ImGui::IsItemHovered() && selCommonInternal >= 0)
            {
                std::string desc = CMS::getColorSpaceDesc(CMS::getInternalConfig(), internalSpaces[selCommonInternal]);
                if (!desc.empty()) ImGui::SetTooltip(desc.c_str());
            }

            // User space
            if (imGuiCombo("User##XC", userSpaces, &selCommonUser, false))
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
        if (!CmXYZ::getStatus().isOK())
            imGuiText(CmXYZ::getStatus().getError(), true, false);

    }

    ImGui::NewLine();
    imGuiDialogs();
    ImGui::End();
}

void layoutMisc()
{
    ImGui::Begin("Misc");

    imGuiBold("INTERFACE");

    if (ImGui::InputFloat("Scale", &Config::UI_SCALE, 0.125f, 0.125f, "%.3f"))
    {
        Config::UI_SCALE = fminf(fmaxf(Config::UI_SCALE, Config::UI_MIN_SCALE), Config::UI_MAX_SCALE);
        io->FontGlobalScale = Config::UI_SCALE / Config::UI_MAX_SCALE;
    }

    // UI Renderer
    if (showRendererName)
    {
        static std::string uiRenderer = strFormat("Renderer: %s", (const char*)glGetString(GL_RENDERER));
        ImGui::TextWrapped(uiRenderer.c_str());
    }

    // FPS
    if (showFPS)
    {
        ImGui::TextWrapped(
            "%.3f ms (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    }

    imGuiDiv();
    imGuiBold("INFO");

    // Version
    ImGui::TextWrapped("%s v%s", Config::APP_TITLE, Config::APP_VERSION);

    // GitHub
    if (ImGui::Button("GitHub", btnSize()))
        openURL(Config::GITHUB_URL);
    if (ImGui::Button("Tutorial", btnSize()))
        openURL(Config::DOCS_URL);

    ImGui::End();
}

void layoutConvolution()
{
    CmImage& imgConvInput = *getImageByID("cv-input");
    CmImage& imgConvKernel = *getImageByID("cv-kernel");
    CmImage& imgConvResult = *getImageByID("cv-result");

    RealBloom::ConvolutionParams* convParams = conv.getParams();

    ImGui::Begin("Convolution");

    imGuiBold("INPUT");

    {
        static std::string loadError;
        if (ImGui::Button("Browse Input##Conv", btnSize()))
        {
            auto& srcInfo = imageSourceInfoMap["cv-input"];

            if (openImage(*srcInfo.imageSource, imgConvInput.getID(), loadError))
            {
                srcInfo.updateParamsChanged();
                convThresholdChanged = true;
            }
        }
        imGuiText(loadError, true, false);
    }

    if (layoutImageTransformParams("Input", "ConvInput", convParams->inputTransformParams))
        convInputTransformParamsChanged = true;

    if (convInputTransformParamsChanged)
    {
        convInputTransformParamsChanged = false;
        conv.previewInput();
        selImageID = "cv-input";
    }

    imGuiDiv();
    imGuiBold("KERNEL");

    {
        static std::string loadError;
        if (ImGui::Button("Browse Kernel##Conv", btnSize()))
        {
            auto& srcInfo = imageSourceInfoMap["cv-kernel"];

            if (openImage(*srcInfo.imageSource, imgConvKernel.getID(), loadError))
                srcInfo.updateParamsChanged();
        }
        imGuiText(loadError, true, false);
    }

    if (layoutImageTransformParams("Kernel", "ConvKernel", convParams->kernelTransformParams))
        convKernelTransformParamsChanged = true;

    if (convKernelTransformParamsChanged)
    {
        convKernelTransformParamsChanged = false;
        conv.previewKernel();
        selImageID = "cv-kernel";
    }

    ImGui::Checkbox("Use Transform Origin##Conv", &convParams->useKernelTransformOrigin);

    imGuiDiv();
    imGuiBold("CONVOLUTION");

    const char* const convMethodItems[]{ "FFT CPU", "FFT GPU (Experimental)", "Naive CPU", "Naive GPU" };
    if (ImGui::Combo("Method##Conv", (int*)(&convParams->methodInfo.method), convMethodItems, RealBloom::ConvolutionMethod_EnumSize))
        conv.cancel();

    if (convParams->methodInfo.method == RealBloom::ConvolutionMethod::FFT_CPU)
    {
        // Deconvolve
        ImGui::Checkbox("Deconvolve##Conv", &convParams->methodInfo.FFT_CPU_deconvolve);
    }
    else if (convParams->methodInfo.method == RealBloom::ConvolutionMethod::NAIVE_CPU)
    {
        // Threads
        if (imGuiSliderUInt("Threads##Conv", &convParams->methodInfo.NAIVE_CPU_numThreads, 1, getMaxNumThreads()))
            convParams->methodInfo.NAIVE_CPU_numThreads = std::clamp(convParams->methodInfo.NAIVE_CPU_numThreads, 1u, getMaxNumThreads());
    }
    else if (convParams->methodInfo.method == RealBloom::ConvolutionMethod::NAIVE_GPU)
    {
        // Chunks
        if (imGuiInputUInt("Chunks##Conv", &convParams->methodInfo.NAIVE_GPU_numChunks))
            convParams->methodInfo.NAIVE_GPU_numChunks = std::clamp(convParams->methodInfo.NAIVE_GPU_numChunks, 1u, RealBloom::CONV_NAIVE_GPU_MAX_CHUNKS);

        // Chunk Sleep Time
        if (imGuiInputUInt("Sleep (ms)##Conv", &convParams->methodInfo.NAIVE_GPU_chunkSleep))
            convParams->methodInfo.NAIVE_GPU_chunkSleep = std::clamp(convParams->methodInfo.NAIVE_GPU_chunkSleep, 0u, RealBloom::CONV_NAIVE_GPU_MAX_SLEEP);
    }

    if (ImGui::SliderFloat("Threshold##Conv", &convParams->threshold, 0.0f, 2.0f))
    {
        convParams->threshold = std::max(convParams->threshold, 0.0f);
        convThresholdChanged = true;
        convThresholdSwitchImage = true;
    }

    if (ImGui::SliderFloat("Knee##Conv", &convParams->knee, 0.0f, 2.0f))
    {
        convParams->knee = std::max(convParams->knee, 0.0f);
        convThresholdChanged = true;
        convThresholdSwitchImage = true;
    }

    if (convThresholdChanged)
    {
        convThresholdChanged = false;
        conv.previewThreshold();
        if (convThresholdSwitchImage)
        {
            convThresholdSwitchImage = false;
            selImageID = "cv-prev";
        }
    }

    ImGui::Checkbox("Auto-Exposure##Conv", &convParams->autoExposure);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Adjust the exposure to preserve the overall brightness");

    if (ImGui::Button("Convolve##Conv", btnSize()))
    {
        imgConvResult.resize(imgConvInput.getWidth(), imgConvInput.getHeight(), true);
        imgConvResult.fill(std::array<float, 4>{ 0, 0, 0, 1 }, true);
        imgConvResult.moveToGPU();
        selImageID = "cv-result";

        conv.convolve();
    }

    if (ImGui::IsItemHovered() && !convResUsage.empty())
        ImGui::SetTooltip(convResUsage.c_str());

    if (conv.getStatus().isWorking())
    {
        if (ImGui::Button("Cancel##Conv", btnSize()))
            conv.cancel();
    }

    std::string convStatus, convMessage;
    uint32_t convMessageType = 1;
    conv.getStatusText(convStatus, convMessage, convMessageType);

    if (!convStatus.empty())
        ImGui::Text(convStatus.c_str());

    if (!convMessage.empty())
    {
        if (convMessageType > 0)
        {
            const ImVec4* textColor = &colorErrorText;
            if (convMessageType == 1)
                textColor = &colorInfoText;
            else if (convMessageType == 2)
                textColor = &colorWarningText;
            else if (convMessageType == 3)
                textColor = &colorErrorText;

            ImGui::PushStyleColor(ImGuiCol_Text, *textColor);
            ImGui::TextWrapped(convMessage.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::Text(convMessage.c_str());
        }
    }

    imGuiDiv();
    imGuiBold("BLENDING");

    if (ImGui::Checkbox("Additive##Conv", &convParams->blendAdditive))
        convBlendParamsChanged = true;

    if (convParams->blendAdditive)
    {
        if (ImGui::SliderFloat("Input##Conv", &convParams->blendInput, 0.0f, 1.0f))
        {
            convParams->blendInput = std::max(convParams->blendInput, 0.0f);
            convBlendParamsChanged = true;
        }

        if (ImGui::SliderFloat("Conv.##Conv", &convParams->blendConv, 0.0f, 1.0f))
        {
            convParams->blendConv = std::max(convParams->blendConv, 0.0f);
            convBlendParamsChanged = true;
        }
    }
    else
    {
        if (ImGui::SliderFloat("Mix##Conv", &convParams->blendMix, 0.0f, 1.0f))
        {
            convParams->blendMix = std::clamp(convParams->blendMix, 0.0f, 1.0f);
            convBlendParamsChanged = true;
        }
    }

    if (ImGui::SliderFloat("Exposure##Conv", &convParams->blendExposure, -EXPOSURE_RANGE, EXPOSURE_RANGE))
        convBlendParamsChanged = true;

    if (ImGui::Button("Show Conv. Layer##Conv", btnSize()))
    {
        convParams->blendAdditive = false;
        convParams->blendMix = 1.0f;
        convParams->blendExposure = 0.0f;
        convBlendParamsChanged = true;
    }

    {
        SignalValue v;
        convBlendParamsChanged |= Async::readSignalLast("convBlendParamsChanged", v);
    }


    if (convBlendParamsChanged)
    {
        convBlendParamsChanged = false;
        conv.blend();
        selImageID = "cv-result";
    }

    ImGui::NewLine();
    imGuiDialogs();
    ImGui::End();
}

void layoutDiffraction()
{
    CmImage& imgDiffInput = *getImageByID("diff-input");
    CmImage& imgDispInput = *getImageByID("disp-input");
    CmImage& imgDispResult = *getImageByID("disp-result");

    RealBloom::DiffractionParams* diffParams = diff.getParams();
    RealBloom::DispersionParams* dispParams = disp.getParams();

    ImGui::Begin("Diffraction");

    imGuiBold("DIFFRACTION");

    {
        static std::string loadError = "";
        if (ImGui::Button("Browse Input##Diff", btnSize()))
        {
            auto& srcInfo = imageSourceInfoMap["diff-input"];

            if (openImage(*srcInfo.imageSource, imgDiffInput.getID(), loadError))
                srcInfo.updateParamsChanged();
        }
        imGuiText(loadError, true, false);
    }

    if (layoutImageTransformParams("Input", "DiffInput", diffParams->inputTransformParams))
        diffInputTransformParamsChanged = true;

    if (diffInputTransformParamsChanged)
    {
        diffInputTransformParamsChanged = false;
        diff.previewInput();
        selImageID = "diff-input";
    }

    if (ImGui::Button("Compute##Diff", btnSize()))
    {
        selImageID = "diff-result";
        diff.compute();
    }

    if (!diff.getStatus().isOK())
    {
        std::string dpError = diff.getStatus().getError();
        imGuiText(dpError, true, false);
    }

    imGuiDiv();
    imGuiBold("DISPERSION");

    {
        static std::string loadError;
        if (ImGui::Button("Browse Input##Disp", btnSize()))
        {
            auto& srcInfo = imageSourceInfoMap["disp-input"];

            if (openImage(*srcInfo.imageSource, imgDispInput.getID(), loadError))
                srcInfo.updateParamsChanged();
        }
        imGuiText(loadError, true, false);
    }

    if (layoutImageTransformParams("Input", "DispInput", dispParams->inputTransformParams))
        dispInputTransformParamsChanged = true;

    if (dispInputTransformParamsChanged)
    {
        dispInputTransformParamsChanged = false;
        disp.previewInput();
        selImageID = "disp-input";
    }

    if (ImGui::SliderFloat("Amount##Disp", &dispParams->amount, 0.0f, 1.0f))
        dispParams->amount = std::clamp(dispParams->amount, 0.0f, 1.0f);

    if (imGuiSliderUInt("Steps##Disp", &dispParams->steps, 32, 1024))
        dispParams->steps = std::clamp(dispParams->steps, 1u, RealBloom::DISP_MAX_STEPS);

    const char* const dispMethodItems[]{ "CPU", "GPU" };
    if (ImGui::Combo("Method##Disp", (int*)(&dispParams->methodInfo.method), dispMethodItems, RealBloom::DispersionMethod_EnumSize))
        disp.cancel();

    if (dispParams->methodInfo.method == RealBloom::DispersionMethod::CPU)
    {
        if (imGuiSliderUInt("Threads##Disp", &dispParams->methodInfo.numThreads, 1, getMaxNumThreads()))
            dispParams->methodInfo.numThreads = std::clamp(dispParams->methodInfo.numThreads, 1u, getMaxNumThreads());
    }

    if (ImGui::Button("Apply Dispersion##Disp", btnSize()))
    {
        imgDispResult.resize(imgDispInput.getWidth(), imgDispInput.getHeight(), true);
        imgDispResult.fill(std::array<float, 4>{ 0, 0, 0, 1 }, true);
        imgDispResult.moveToGPU();
        selImageID = "disp-result";

        disp.compute();
    }

    if (disp.getStatus().isWorking())
    {
        if (ImGui::Button("Cancel##Disp", btnSize()))
            disp.cancel();
    }

    std::string dispStats = disp.getStatusText();
    imGuiText(dispStats, !disp.getStatus().isOK(), false);

    ImGui::NewLine();
    imGuiDialogs();
    ImGui::End();
}

void layoutDebug()
{
    if (!showDebugPanel)
        return;

    ImGui::Begin("Debug");

    ImGui::Checkbox("Image Transform: Use GPU##Debug", &ImageTransform::S_USE_GPU);

    ImGui::End();
}

bool layoutImageTransformParams(const std::string& imageName, const std::string& imGuiID, ImageTransformParams& params)
{
    bool changed = false;

    static std::unordered_map<std::string, bool> lockCrop;
    if (!lockCrop.contains(imGuiID))
        lockCrop[imGuiID] = true;

    static std::unordered_map<std::string, bool> lockResize;
    if (!lockResize.contains(imGuiID))
        lockResize[imGuiID] = true;

    static std::unordered_map<std::string, bool> lockScale;
    if (!lockScale.contains(imGuiID))
        lockScale[imGuiID] = true;

    ImGui::PushID(imGuiID.c_str());
    if (ImGui::CollapsingHeader(strFormat("%s Transform", imageName.c_str()).c_str()))
    {
        ImGui::Indent();

        imGuiBold("CROP & RESIZE");

        {

            // Crop
            if (ImGui::Checkbox("Lock Crop##CropResize", &lockCrop[imGuiID]))
            {
                if (lockCrop[imGuiID])
                {
                    params.cropResize.crop[0] = (params.cropResize.crop[0] + params.cropResize.crop[1]) / 2.0f;
                    params.cropResize.crop[1] = params.cropResize.crop[0];
                }
                changed = true;
            }
            if (lockCrop[imGuiID])
            {
                if (ImGui::SliderFloat("Crop##CropResize", &params.cropResize.crop[0], 0.01f, 1.0f))
                {
                    params.cropResize.crop[0] = std::clamp(params.cropResize.crop[0], 0.01f, 1.0f);
                    params.cropResize.crop[1] = params.cropResize.crop[0];
                    changed = true;
                }
            }
            else
            {
                if (ImGui::SliderFloat2("Crop##CropResize", &params.cropResize.crop[0], 0.01f, 1.0f))
                {
                    params.cropResize.crop[0] = std::clamp(params.cropResize.crop[0], 0.01f, 1.0f);
                    params.cropResize.crop[1] = std::clamp(params.cropResize.crop[1], 0.01f, 1.0f);
                    changed = true;
                }
            }

            // Resize
            if (ImGui::Checkbox("Lock Resize##CropResize", &lockResize[imGuiID]))
            {
                if (lockResize[imGuiID])
                {
                    params.cropResize.resize[0] = (params.cropResize.resize[0] + params.cropResize.resize[1]) / 2.0f;
                    params.cropResize.resize[1] = params.cropResize.resize[0];
                }
                changed = true;
            }
            if (lockResize[imGuiID])
            {
                if (ImGui::SliderFloat("Resize##CropResize", &params.cropResize.resize[0], 0.01f, 2.0f))
                {
                    params.cropResize.resize[0] = std::max(params.cropResize.resize[0], 0.01f);
                    params.cropResize.resize[1] = params.cropResize.resize[0];
                    changed = true;
                }
            }
            else
            {
                if (ImGui::SliderFloat2("Resize##CropResize", &params.cropResize.resize[0], 0.01f, 2.0f))
                {
                    params.cropResize.resize[0] = std::max(params.cropResize.resize[0], 0.01f);
                    params.cropResize.resize[1] = std::max(params.cropResize.resize[1], 0.01f);
                    changed = true;
                }
            }

            // Origin
            if (ImGui::SliderFloat2("Origin##CropResize", params.cropResize.origin.data(), 0.0f, 1.0f))
            {
                params.cropResize.origin[0] = std::clamp(params.cropResize.origin[0], 0.0f, 1.0f);
                params.cropResize.origin[1] = std::clamp(params.cropResize.origin[1], 0.0f, 1.0f);
                changed = true;
            }
            if (ImGui::Checkbox("Preview Origin##CropResize", &params.cropResize.previewOrigin))
                changed = true;

            // Reset
            if (ImGui::Button("Reset##CropResize", btnSize()))
            {
                params.cropResize.reset();
                lockCrop[imGuiID] = true;
                lockResize[imGuiID] = true;
                changed = true;
            }

        }

        imGuiDiv();
        imGuiBold("TRANSFORM");

        {

            // Scale
            if (ImGui::Checkbox("Lock Scale##Transform", &lockScale[imGuiID]))
            {
                if (lockScale[imGuiID])
                {
                    params.transform.scale[0] = (params.transform.scale[0] + params.transform.scale[1]) / 2.0f;
                    params.transform.scale[1] = params.transform.scale[0];
                }
                changed = true;
            }
            if (lockScale[imGuiID])
            {
                if (ImGui::SliderFloat("Scale##Transform", &params.transform.scale[0], 0.01f, 2.0f))
                {
                    params.transform.scale[1] = params.transform.scale[0];
                    changed = true;
                }
            }
            else
            {
                if (ImGui::SliderFloat2("Scale##Transform", &params.transform.scale[0], 0.01f, 2.0f))
                {
                    changed = true;
                }
            }

            // Rotate
            if (ImGui::SliderFloat("Rotate##Transform", &params.transform.rotate, -180.0f, 180.0f))
                changed = true;

            // Translate
            if (ImGui::SliderFloat2("Translate##Transform", params.transform.translate.data(), -1.0f, 1.0f))
                changed = true;

            // Origin
            if (ImGui::SliderFloat2("Origin##Transform", params.transform.origin.data(), 0.0f, 1.0f))
            {
                params.transform.origin[0] = std::clamp(params.transform.origin[0], 0.0f, 1.0f);
                params.transform.origin[1] = std::clamp(params.transform.origin[1], 0.0f, 1.0f);
                changed = true;
            }
            if (ImGui::Checkbox("Preview Origin##Transform", &params.transform.previewOrigin))
                changed = true;

            // Reset
            if (ImGui::Button("Reset##Transform", btnSize()))
            {
                params.transform.reset();
                lockScale[imGuiID] = true;
                changed = true;
            }

        }

        imGuiDiv();
        imGuiBold("COLOR");

        {

            // Filter
            if (ImGui::ColorEdit3("Filter##Color", params.color.filter.data(),
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha))
            {
                changed = true;
            }

            // Exposure
            if (ImGui::SliderFloat("Exposure##Color", &params.color.exposure, -EXPOSURE_RANGE, EXPOSURE_RANGE))
                changed = true;

            // Contrast
            if (ImGui::SliderFloat("Contrast##Color", &params.color.contrast, -1.0f, 1.0f))
                changed = true;

            // Grayscale
            {
                const char* const grayscaleItems[]{ "None", "Luminance", "Average", "Maximum", "Magnitude", "Red", "Green", "Blue", "Alpha" };
                int selIndex = params.color.grayscale ? ((int)params.color.grayscaleType + 1) : 0;
                if (ImGui::Combo("Grayscale##Color", &selIndex, grayscaleItems, sizeof(grayscaleItems) / sizeof(grayscaleItems[0])))
                {
                    if (selIndex > 0)
                    {
                        params.color.grayscale = true;
                        params.color.grayscaleType = (GrayscaleType)(selIndex - 1);
                    }
                    else
                    {
                        params.color.grayscale = false;
                    }
                    changed = true;
                }
            }

            // Reset
            if (ImGui::Button("Reset##Color", btnSize()))
            {
                params.color.reset();
                changed = true;
            }

        }

        imGuiDiv();
        imGuiBold("GENERAL");

        {

            // Transparency
            if (ImGui::Checkbox("Transparency##General", &params.transparency))
                changed = true;

            // Reset all
            if (ImGui::Button("Reset All##General", btnSize()))
            {
                params.reset();
                lockCrop[imGuiID] = true;
                lockResize[imGuiID] = true;
                lockScale[imGuiID] = true;
                changed = true;
            }

        }

        ImGui::NewLine();
        ImGui::Unindent();
    }
    ImGui::PopID();

    return changed;
}

void imGuiDiv()
{
    ImGui::NewLine();
}

void imGuiBold(const std::string& s)
{
    ImGui::PushFont(fontRobotoBold);
    ImGui::Text(s.c_str());
    ImGui::PopFont();
}

void imGuiText(const std::string& s, bool isError, bool newLine)
{
    if (!s.empty())
    {
        if (isError)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, colorErrorText);
            ImGui::TextWrapped(s.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextWrapped(s.c_str());
        }
        if (newLine)
            ImGui::NewLine();
    }
}

bool imGuiSliderUInt(const std::string& label, uint32_t* v, uint32_t min, uint32_t max)
{
    int vInt = u32ToI32(*v);
    if (ImGui::SliderInt(label.c_str(), &vInt, u32ToI32(min), u32ToI32(max)))
    {
        *v = i32ToU32(vInt);
        return true;
    }
    return false;
}

bool imGuiInputUInt(const std::string& label, uint32_t* v)
{
    int vInt = u32ToI32(*v);
    if (ImGui::InputInt(label.c_str(), &vInt))
    {
        *v = i32ToU32(vInt);
        return true;
    }
    return false;
}

bool imGuiCombo(const std::string& label, const std::vector<std::string>& items, int* selectedIndex, bool fullWidth)
{
    static std::vector<std::string> activeComboList;

    activeComboList = items;
    bool result = false;

    if (fullWidth)
        ImGui::PushItemWidth(-1);

    result = ImGui::Combo(
        label.c_str(),
        selectedIndex,
        [](void* data, int index, const char** outText)
        {
            *outText = activeComboList[index].c_str();
            return true;
        },
        nullptr, activeComboList.size());

    if (fullWidth)
        ImGui::PopItemWidth();

    return result;
}

void imGuiDialogs()
{
    // Move To
    if (ImGui::BeginPopupModal(DIALOG_TITLE_MOVETO, 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Source:");
        imGuiCombo("##Dialog_MoveTo_SourceImage", imageNames, &dialogParams_MoveTo.selSourceImage, true);

        ImGui::Text("Destination:");
        imGuiCombo("##Dialog_MoveTo_DestImage", imageNames, &dialogParams_MoveTo.selDestImage, true);

        ImGui::Checkbox("Preserve Original##Dialog_MoveTo", &dialogParams_MoveTo.preserveOriginal);

        if (ImGui::Button("OK##Dialog_MoveTo", dlgBtnSize()))
        {
            bool validParams =
                dialogParams_MoveTo.selSourceImage >= 0
                && dialogParams_MoveTo.selDestImage >= 0;

            validParams &= (dialogParams_MoveTo.selSourceImage != dialogParams_MoveTo.selDestImage);

            if (validParams)
            {
                std::shared_ptr<CmImage> sourceImage = images[dialogParams_MoveTo.selSourceImage];
                std::shared_ptr<CmImage> destImage = images[dialogParams_MoveTo.selDestImage];

                if (imageSourceInfoMap.contains(destImage->getID()))
                {
                    auto& srcInfo = imageSourceInfoMap[destImage->getID()];
                    sourceImage->moveContent(*srcInfo.imageSource, dialogParams_MoveTo.preserveOriginal);
                    srcInfo.updateParamsChanged();
                }
                else
                {
                    sourceImage->moveContent(*destImage, dialogParams_MoveTo.preserveOriginal);
                }

                selImageID = destImage->getID();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel##Dialog_MoveTo", dlgBtnSize()))
            ImGui::CloseCurrentPopup();

        ImGui::NewLine();
    }

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

bool openImage(CmImage& image, const std::string& dlgID, std::string& outError)
{
    std::string filename;
    if (FileDialogs::openDialog(dlgID, CmImageIO::getOpenFilterList(), filename))
    {
        outError = "";
        try
        {
            CmImageIO::readImage(image, filename);
            selImageID = image.getID();
            return true;
        }
        catch (const std::exception& e)
        {
            outError = e.what();
        }
    }
    return false;
}

bool saveImage(CmImage& image, const std::string& dlgID, std::string& outError)
{
    std::string filename;
    if (FileDialogs::saveDialog(dlgID, CmImageIO::getSaveFilterList(), CmImageIO::getDefaultFilename(), filename))
    {
        outError = "";
        try
        {
            CmImageIO::writeImage(image, filename);
            return true;
        }
        catch (const std::exception& e)
        {
            outError = e.what();
        }
    }
    return false;
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glVersionMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glVersionMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create window with graphics context
    window = glfwCreateWindow(Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, Config::APP_TITLE, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create the main window.\n";
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

    io->FontGlobalScale = Config::UI_SCALE / Config::UI_MAX_SCALE;

    fontRoboto = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/RobotoCondensed-Regular.ttf").c_str(),
        18.0f * Config::UI_MAX_SCALE);

    fontRobotoBold = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/RobotoCondensed-Bold.ttf").c_str(),
        19.0f * Config::UI_MAX_SCALE);

    fontMono = io->Fonts->AddFontFromFileTTF(
        getLocalPath("assets/fonts/mono/RobotoMono-Regular.ttf").c_str(),
        18.0f * Config::UI_MAX_SCALE);

    if (fontRoboto && fontRobotoBold && fontMono)
        return true;
    else
    {
        std::cout << "The required fonts could not be loaded.\n";
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
    ImageTransform::cleanUp();

    if (!CLI::Interface::active())
    {
        clearVector(images);
        imgui_widgets_cleanup_cmimages();
    }

    CmImage::cleanUp();
    CmImageIO::cleanUp();
    CMF::cleanUp();
    CMS::cleanUp();

    GlFullPlaneVertices::cleanUp();

    if (!CLI::Interface::active())
    {
        NFD_Quit();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    CLI::Interface::cleanUp();
}

void ImageSourceInfo::updateParamsChanged()
{
    if (paramsChanged != nullptr)
        *paramsChanged = true;
}
