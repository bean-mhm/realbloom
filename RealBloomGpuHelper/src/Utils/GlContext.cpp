#include "GlContext.h"

HGLRC realContext = NULL;
void* g_data = nullptr;
std::function<void(void*)> g_job;
std::function<void(std::string)> g_errHandler;

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool oglOneTimeContext(uint32_t width, uint32_t height, std::function<void(void*)> job, void* data, std::function<void(std::string)> errHandler, std::string& outError)
{
    outError = "";

    g_data = data;
    g_job = job;
    g_errHandler = errHandler;

    std::wstring className = L"oglcontext" + std::to_wstring(RandomNumber::nextInt());

    MSG msg = { 0 };
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = MyWndProc;
    wc.hInstance = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszClassName = className.c_str();
    wc.style = CS_OWNDC;

    if (!RegisterClass(&wc))
    {
        outError = "RegisterClass failed: " + toHexStr(GetLastError());
        return false;
    }

    CreateWindowW(wc.lpszClassName, L"oglcontext", WS_POPUP, 0, 0, width, height, 0, 0, NULL, 0);

    while (GetMessage(&msg, NULL, 0, 0) > 0)
        DispatchMessage(&msg);

    outError = "Success";
    return true;

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
            // Real context
            static const int ctxAttribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 2,
                WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            realContext = wglCreateContextAttribsARB(devContext, nullptr, ctxAttribs);

            if (realContext)
            {
                if (wglMakeCurrent(devContext, realContext))
                {
                    //MessageBoxA(0, (char*)glGetString(GL_VERSION), "OPENGL VERSION", 0);
                    g_job(g_data);
                } else
                {
                    g_errHandler("Failed to activate OpenGL 3.2 rendering context.");
                }

                // Delete the rendering context
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(realContext);
            } else
            {
                g_errHandler("Failed to create OpenGL 3.2 context.");
            }
        } else
        {
            g_errHandler("GLEW was not initialized.");
        }

        // Delete the device context
        ReleaseDC(hWnd, devContext);
        PostQuitMessage(0);
        DestroyWindow(hWnd);
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;

}