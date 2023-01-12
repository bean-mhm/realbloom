#pragma once

#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include <GL/glew.h>

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <stdint.h>

#include "RealBloom/Binary/BinaryData.h"
#include "RealBloom/Binary/BinaryConvNaiveGpu.h"

#include "Utils/OpenGL/GlUtils.h"
#include "Utils/OpenGL/GlFrameBuffer.h"
#include "Utils/NumberHelpers.h"
#include "Utils/Status.h"
#include "Utils/Misc.h"

namespace RealBloom
{

    class ConvNaiveGpu
    {
    private:
        BaseStatus m_status;
        BinaryConvNaiveGpuInput* m_binInput;

        std::vector<float> m_vertexData; // Each point consists of 2 elements for position and 3 for color: x, y, r, g ,b
        std::vector<float> m_outputBuffer;

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
        void definePoints(GLfloat* points, uint32_t size);
        void useProgram();
        void bindKernelTexture();
        void setUniforms(uint32_t kernelWidth, uint32_t kernelHeight, float* kernelTopLeft, float* kernelSize);
        void specifyLayout();
        void drawScene(uint32_t numPoints);

    public:
        ConvNaiveGpu(BinaryConvNaiveGpuInput* binInput);
        void prepare();
        void process(uint32_t chunkIndex);
        void cleanUp();

        uint32_t getNumVertices() const;
        const std::vector<float>& getBuffer() const;
        const BaseStatus& getStatus() const;

        static uint32_t getNumAttribs();

    };

}
