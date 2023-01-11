#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include <string>
#include <vector>
#include <stdint.h>

#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryDispGpu.h"

#include "Utils/OpenGL/GlUtils.h"
#include "Utils/OpenGL/GlFrameBuffer.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"

namespace RealBloom
{

    class DispGpu;

    struct DispGpuData
    {
        BinaryDispGpuInput* binInput;
        std::vector<float> outputBuffer;

        bool done = false;
        bool success = false;
        std::string error = "";

        void reset();
        void setError(const std::string& err);
    };

    class DispGpu
    {
    private:
        DispGpuData* m_data;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // OpenGL Variables

        GLuint m_vao = 0;
        GLuint m_vbo = 0;

        GLuint m_vertexShader = 0;
        GLuint m_fragmentShader = 0;
        GLuint m_geometryShader = 0;
        GLuint m_shaderProgram = 0;

        GLuint m_texInput = 0;

        void makeInputTexture(float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight);
        void makeProgram();
        void definePoints(GLfloat* points, uint32_t size);
        void useProgram();
        void bindInputTexture();
        void setUniforms();
        void specifyLayout();
        void drawScene(uint32_t numPoints);

    public:
        DispGpu(DispGpuData* data);
        void process();

    };

}
