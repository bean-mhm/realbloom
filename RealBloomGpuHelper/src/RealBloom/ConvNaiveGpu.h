#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include "../Utils/GlContext.h"
#include "Utils/GlUtils.h"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <stdint.h>

#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryConvNaiveGpu.h"

#include "Utils/NumberHelpers.h"
#include "Utils/Misc.h"
#include "Utils/GlFrameBuffer.h"

namespace RealBloom
{

    class ConvNaiveGpu;

    struct ConvNaiveGpuData
    {
        BinaryConvNaiveGpuInput* binInput;
        uint32_t numPoints = 0;
        std::vector<float> points; // Each point consists of 2 elements for position and 3 for color: x, y, r, g ,b
        std::vector<float> outputBuffer;

        bool done = false;
        bool success = false;
        std::string error = "";
        std::string gpuName = "Unknown";

        void reset();
        void setError(std::string err);
    };

    class ConvNaiveGpu
    {
    private:
        ConvNaiveGpuData* m_data;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // OpenGL Variables
        GLuint m_vao = 0;
        GLuint m_vbo = 0;

        GLuint m_vertexShader = 0;
        GLuint m_fragmentShader = 0;
        GLuint m_geometryShader = 0;
        GLuint m_shaderProgram = 0;

        GLuint m_texKernel = 0;

        void makeKernelTexture(float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight);
        void makeProgram();
        void definePoints(GLfloat* points, uint32_t numPoints, uint32_t numAttribs);
        void useProgram();
        void bindKernelTexture();
        void setUniforms(uint32_t kernelWidth, uint32_t kernelHeight, float* kernelTopLeft, float* kernelSize);
        void specifyLayout();
        void drawScene(uint32_t numPoints);

    public:
        ConvNaiveGpu(ConvNaiveGpuData* data);
        void prepare();
        void process(uint32_t chunkIndex);
        void cleanUp();
    };

}