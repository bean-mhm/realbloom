#include "GlContext.h"

HGLRC realContext = NULL;

GlContextVersion g_glVersion;
std::function<void()> g_job;
bool g_success = false;
std::string* g_outError = nullptr;

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool oglOneTimeContext(GlContextVersion glVersion, std::function<void()> job, std::string& outError)
{
    outError = "";

    g_glVersion = glVersion;
    g_job = job;
    g_success = false;
    g_outError = &outError;

    std::string className = "RealBloomOglContext";

    WNDCLASSEXA classInfo = { 0 };
    bool alreadyRegistered = GetClassInfoExA(NULL, className.c_str(), &classInfo);

    if (!alreadyRegistered)
    {
        WNDCLASSA wc = { 0 };
        wc.lpfnWndProc = MyWndProc;
        wc.hInstance = NULL;
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
        wc.lpszClassName = className.c_str();
        wc.style = CS_OWNDC;

        if (!RegisterClassA(&wc))
        {
            outError = "RegisterClass failed: " + toHexStr(GetLastError());
            return false;
        }
    }

    CreateWindowA(className.c_str(), className.c_str(), WS_POPUP, 0, 0, 128, 128, 0, 0, NULL, 0);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return g_success;
}

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
            PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
            32,                   // Colordepth of the framebuffer.
            0, 0, 0, 0, 0, 0,
            0,
            0,
            0,
            0, 0, 0, 0,
            24,                   // Number of bits for the depthbuffer
            8,                    // Number of bits for the stencilbuffer
            0,                    // Number of Aux buffers in the framebuffer.
            PFD_MAIN_PLANE,
            0,
            0, 0, 0
        };

        HDC devContext = GetDC(hWnd);
        int pixelFormat;
        pixelFormat = ChoosePixelFormat(devContext, &pfd);
        SetPixelFormat(devContext, pixelFormat, &pfd);

        // Dummy context
        HGLRC tempContext = wglCreateContext(devContext);
        wglMakeCurrent(devContext, tempContext);

        // Initialize GLEW
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        wglewInit();
        wglDeleteContext(tempContext);

        bool glewReady = (err == GLEW_OK);
        if (glewReady)
        {
            // Version

            int versionMajor, versionMinor;
            if (g_glVersion == GlContextVersion::GL_3_2)
            {
                versionMajor = 3;
                versionMinor = 2;
            }
            else if (g_glVersion == GlContextVersion::GL_4_3)
            {
                versionMajor = 4;
                versionMinor = 3;
            }
            else
            {
                versionMajor = 3;
                versionMinor = 2;
            }

            // Real context
            static const int ctxAttribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, versionMajor,
                WGL_CONTEXT_MINOR_VERSION_ARB, versionMinor,
                WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            realContext = wglCreateContextAttribsARB(devContext, nullptr, ctxAttribs);

            if (realContext)
            {
                if (wglMakeCurrent(devContext, realContext))
                {
                    //MessageBoxA(0, (char*)glGetString(GL_VERSION), "OPENGL VERSION", 0);
                    g_success = true;
                    g_job();
                } else
                {
                    *g_outError = strFormat("Failed to activate OpenGL %d.%d rendering context.", versionMajor, versionMinor);
                }

                // Delete the rendering context
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(realContext);
            } else
            {
                *g_outError = strFormat("Failed to create OpenGL %d.%d context.", versionMajor, versionMinor);
            }
        } else
        {
            *g_outError = "GLEW was not initialized.";
        }

        // Delete the device context
        ReleaseDC(hWnd, devContext);
        DestroyWindow(hWnd);
        PostQuitMessage(0);
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
