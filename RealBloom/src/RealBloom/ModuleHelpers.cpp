#include "ModuleHelpers.h"

namespace RealBloom
{

    void processInputImage(
        bool previewMode,
        ImageTransformParams& transformParams,
        CmImage& imgSrc,
        CmImage& imgPreview,
        std::vector<float>* outBuffer,
        uint32_t* outWidth,
        uint32_t* outHeight)
    {
        // Get the input buffer
        std::vector<float> inputBuffer;
        uint32_t inputBufferSize = 0;
        uint32_t inputWidth, inputHeight;
        {
            std::scoped_lock lock(imgSrc);
            float* srcBuffer = imgSrc.getImageData();
            inputBufferSize = imgSrc.getImageDataSize();
            inputWidth = imgSrc.getWidth();
            inputHeight = imgSrc.getHeight();

            inputBuffer.resize(inputBufferSize);
            std::copy(srcBuffer, srcBuffer + inputBufferSize, inputBuffer.data());
        }

        // Transform
        std::vector<float> transBuffer;
        uint32_t transWidth, transHeight;
        ImageTransform::apply(
            transformParams,
            inputBuffer,
            inputWidth,
            inputHeight,
            transBuffer,
            transWidth,
            transHeight,
            previewMode);

        clearVector(inputBuffer);

        // Copy to outBuffer if requested by another function
        bool outerRequest = (!previewMode && outBuffer && outWidth && outHeight);
        if (outerRequest)
        {
            *outWidth = transWidth;
            *outHeight = transHeight;
            outBuffer->resize(transBuffer.size());
            *outBuffer = transBuffer;
        }

        // Copy to the preview image
        {
            std::scoped_lock lock(imgPreview);
            imgPreview.resize(transWidth, transHeight, false);
            float* prevBuffer = imgPreview.getImageData();
            std::copy(transBuffer.data(), transBuffer.data() + transBuffer.size(), prevBuffer);
        }

        imgPreview.moveToGPU();
        imgPreview.setSourceName(imgSrc.getSourceName());
    }

}
