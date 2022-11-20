#include "ConvolutionFFT.h"

#include <iostream>
#include "../Utils/Misc.h"

RealBloom::ConvolutionFFT::ConvolutionFFT(ConvolutionParams& convParams, float* inputBuffer, uint32_t inputWidth, uint32_t inputHeight, float* kernelBuffer, uint32_t kernelWidth, uint32_t kernelHeight)
    : m_inputBuffer(inputBuffer), m_inputWidth(inputWidth), m_inputHeight(inputHeight),
    m_kernelBuffer(kernelBuffer), m_kernelWidth(kernelWidth), m_kernelHeight(kernelHeight)
{}

RealBloom::ConvolutionFFT::~ConvolutionFFT()
{

}

uint32_t upperPowerOf2(uint32_t v)
{
    return (uint32_t)floor(pow(2, ceil(log(v) / log(2))));
}

void RealBloom::ConvolutionFFT::pad()
{
    uint32_t totalWidth = m_inputWidth + m_kernelWidth;
    uint32_t totalHeight = m_inputHeight + m_kernelHeight;
    m_paddedSize = upperPowerOf2(std::max(totalWidth, totalHeight));

    std::cout << strFormat(
        "Input:   %u x %u\n"
        "Kernel:  %u x %u\n"
        "Total:   %u x %u\n"
        "Final:   %u x %u\n\n",
        m_inputWidth,
        m_inputHeight,
        m_kernelWidth,
        m_kernelHeight,
        totalWidth,
        totalHeight,
        m_paddedSize,
        m_paddedSize
    );
}

void RealBloom::ConvolutionFFT::inputFFT()
{

}

void RealBloom::ConvolutionFFT::kernelFFT()
{

}

void RealBloom::ConvolutionFFT::multiply()
{

}

void RealBloom::ConvolutionFFT::iFFT()
{

}

std::vector<float>& RealBloom::ConvolutionFFT::getBuffer()
{
    return m_outputBuffer;
}
