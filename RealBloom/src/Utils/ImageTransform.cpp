#include "ImageTransform.h"

void ImageTransformParams::CropResizeParams::reset()
{
    crop = { 1.0f, 1.0f };
    resize = { 1.0f, 1.0f };
    origin = { 0.5f, 0.5f };
    previewOrigin = false;
}

void ImageTransformParams::TransformParams::reset()
{
    scale = { 1.0f, 1.0f };
    rotate = 0.0f;
    translate = { 0.0f, 0.0f };
    origin = { 0.5f, 0.5f };
    previewOrigin = false;
}

void ImageTransformParams::ColorParams::reset()
{
    exposure = 0.0f;
    contrast = 0.0f;
    filter = { 1.0f, 1.0f, 1.0f };
    mono = false;
    monoMethod = RgbToMonoMethod::Luminance;
}

void ImageTransformParams::reset()
{
    cropResize.reset();
    transform.reset();
    color.reset();
}

bool ImageTransform::USE_GPU = false;

void ImageTransform::getOutputDimensions(
    const ImageTransformParams& params,
    uint32_t inputWidth,
    uint32_t inputHeight,
    uint32_t& outCroppedWidth,
    uint32_t& outCroppedHeight,
    float& outCropX,
    float& outCropY,
    uint32_t& outResizedWidth,
    uint32_t& outResizedHeight,
    float& outResizeX,
    float& outResizeY)
{
    outCroppedWidth = (uint32_t)fmaxf(1.0f, floorf(params.cropResize.crop[0] * (float)inputWidth));
    outCroppedHeight = (uint32_t)fmaxf(1.0f, floorf(params.cropResize.crop[1] * (float)inputHeight));

    outCropX = (float)outCroppedWidth / (float)inputWidth;
    outCropY = (float)outCroppedHeight / (float)inputHeight;

    outResizedWidth = (uint32_t)fmaxf(1.0f, floorf(params.cropResize.resize[0] * (float)outCroppedWidth));
    outResizedHeight = (uint32_t)fmaxf(1.0f, floorf(params.cropResize.resize[1] * (float)outCroppedHeight));

    outResizeX = (float)outResizedWidth / (float)outCroppedWidth;
    outResizeY = (float)outResizedHeight / (float)outCroppedHeight;
}

void ImageTransform::getOutputDimensions(
    const ImageTransformParams& params,
    uint32_t inputWidth,
    uint32_t inputHeight,
    uint32_t& outputWidth,
    uint32_t& outputHeight)
{
    uint32_t croppedWidth, croppedHeight;
    float cropX, cropY, resizeX, resizeY;
    getOutputDimensions(
        params,
        inputWidth,
        inputHeight,
        croppedWidth,
        croppedHeight,
        cropX,
        cropY,
        outputWidth,
        outputHeight,
        resizeX,
        resizeY);
}

void ImageTransform::apply(
    const ImageTransformParams& params,
    const std::vector<float>& inputBuffer,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<float>& outputBuffer,
    uint32_t& outputWidth,
    uint32_t& outputHeight)
{
    if (USE_GPU)
        applyGPU(params, inputBuffer, inputWidth, inputHeight, outputBuffer, outputWidth, outputHeight);
    else
        applyCPU(params, inputBuffer, inputWidth, inputHeight, outputBuffer, outputWidth, outputHeight);
}

void ImageTransform::applyCPU(
    const ImageTransformParams& params,
    const std::vector<float>& inputBuffer,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<float>& outputBuffer,
    uint32_t& outputWidth,
    uint32_t& outputHeight)
{
    // Verify the input size
    uint32_t inputBufferSize = inputWidth * inputHeight * 4;
    if ((inputBuffer.size() != inputBufferSize) || (inputBuffer.size() < 1))
        throw std::exception(makeError(__FUNCTION__, "", "Invalid input buffer size").c_str());

    // Clear the output buffer
    clearVector(outputBuffer);

    // Get dimensions
    uint32_t croppedWidth, croppedHeight, resizedWidth, resizedHeight;
    float cropX, cropY, resizeX, resizeY;
    getOutputDimensions(
        params,
        inputWidth,
        inputHeight,
        croppedWidth,
        croppedHeight,
        cropX,
        cropY,
        resizedWidth,
        resizedHeight,
        resizeX,
        resizeY);

    // Define the input buffer for later transforms
    const std::vector<float>* lastBuffer = &inputBuffer;

    // Crop
    std::vector<float> croppedBuffer;
    if ((cropX != 1.0f) || (cropY != 1.0f))
    {
        lastBuffer = &croppedBuffer;

        uint32_t croppedBufferSize = croppedWidth * croppedHeight * 4;
        croppedBuffer.resize(croppedBufferSize);

        float cropMaxOffsetX = inputWidth - croppedWidth;
        float cropMaxOffsetY = inputHeight - croppedHeight;

        uint32_t cropStartX = (uint32_t)floorf(params.cropResize.origin[0] * cropMaxOffsetX);
        uint32_t cropStartY = (uint32_t)floorf(params.cropResize.origin[1] * cropMaxOffsetY);

        for (uint32_t y = 0; y < croppedHeight; y++)
        {
            for (uint32_t x = 0; x < croppedWidth; x++)
            {
                uint32_t redIndexCropped = 4 * (y * croppedWidth + x);
                uint32_t redIndexInput = 4 * (((y + cropStartY) * inputWidth) + x + cropStartX);

                croppedBuffer[redIndexCropped + 0] = inputBuffer[redIndexInput + 0];
                croppedBuffer[redIndexCropped + 1] = inputBuffer[redIndexInput + 1];
                croppedBuffer[redIndexCropped + 2] = inputBuffer[redIndexInput + 2];
                croppedBuffer[redIndexCropped + 3] = inputBuffer[redIndexInput + 3];
            }
        }
    }

    // Prepare the output buffer
    uint32_t outputBufferSize = resizedWidth * resizedHeight * 4;
    outputBuffer.resize(outputBufferSize);

    // Transform origin
    float transformOriginX = params.transform.origin[0] * resizedWidth;
    float transformOriginY = params.transform.origin[1] * resizedHeight;

    // Scale (non-zero)
    float scaleX = params.transform.scale[0];
    float scaleY = params.transform.scale[1];
    if (scaleX == 0.0f) scaleX == EPSILON;
    if (scaleY == 0.0f) scaleY == EPSILON;

    // Color multiplier
    float expMul = getExposureMul(params.color.exposure);
    float colorMulR = expMul * params.color.filter[0];
    float colorMulG = expMul * params.color.filter[1];
    float colorMulB = expMul * params.color.filter[2];

    // Resize, Scale, Rotate, Translate, Color Transforms
    {
        uint32_t redIndexInput, redIndexOutput;
        float transX, transY;
        float targetColor[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
        Bilinear bil;
        for (uint32_t y = 0; y < resizedHeight; y++)
        {
            for (uint32_t x = 0; x < resizedWidth; x++)
            {
                // Resize
                transX = (x + 0.5f) / resizeX;
                transY = (y + 0.5f) / resizeY;

                // Scale
                transX = ((transX - transformOriginX) / scaleX) + transformOriginX;
                transY = ((transY - transformOriginY) / scaleY) + transformOriginY;

                // Rotate
                rotatePointInPlace(transX, transY, transformOriginX, transformOriginY, -params.transform.rotate);

                // Translate
                transX += params.transform.translate[0] * resizedWidth;
                transY += params.transform.translate[1] * resizedHeight;

                // Interpolate

                bil.calc(transX, transY);

                for (auto& v : targetColor)
                    v = 0.0f;

                if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], croppedWidth, croppedHeight))
                {
                    redIndexInput = (bil.topLeftPos[1] * croppedWidth + bil.topLeftPos[0]) * 4;
                    blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topLeftWeight);
                }
                if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], croppedWidth, croppedHeight))
                {
                    redIndexInput = (bil.topRightPos[1] * croppedWidth + bil.topRightPos[0]) * 4;
                    blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topRightWeight);
                }
                if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], croppedWidth, croppedHeight))
                {
                    redIndexInput = (bil.bottomLeftPos[1] * croppedWidth + bil.bottomLeftPos[0]) * 4;
                    blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomLeftWeight);
                }
                if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], croppedWidth, croppedHeight))
                {
                    redIndexInput = (bil.bottomRightPos[1] * croppedWidth + bil.bottomRightPos[0]) * 4;
                    blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomRightWeight);
                }

                // Exposure, Filter
                targetColor[0] *= colorMulR;
                targetColor[1] *= colorMulG;
                targetColor[2] *= colorMulB;

                // Get the monotonic value for contrast
                float mono = rgbToMono(targetColor, RgbToMonoMethod::Magnitude);

                // Contrast
                if (mono > 0.0f)
                {
                    float mul = applyContrast(mono, params.color.contrast) / mono;
                    targetColor[0] *= mul;
                    targetColor[1] *= mul;
                    targetColor[2] *= mul;
                }

                // Mono
                if (params.color.mono)
                {
                    targetColor[0] = rgbToMono(targetColor, params.color.monoMethod);
                    targetColor[1] = targetColor[0];
                    targetColor[2] = targetColor[0];
                }

                // Put tagetColor in the output buffer
                redIndexOutput = (y * resizedWidth + x) * 4;
                std::copy(targetColor, targetColor + 4, &(outputBuffer[redIndexOutput]));
            }
        }
    }

    // Preview origins
    if (params.cropResize.previewOrigin || params.transform.previewOrigin)
    {
        float cropOriginX = params.cropResize.origin[0] * resizedWidth;
        float cropOriginY = params.cropResize.origin[1] * resizedHeight;

        // Draw
        for (uint32_t y = 0; y < resizedHeight; y++)
        {
            for (uint32_t x = 0; x < resizedWidth; x++)
            {
                uint32_t redIndex = 4 * (y * resizedWidth + x);

                float fX = x + 0.5f;
                float fY = y + 0.5f;

                // Pixel value, goes from source pixel to filled from 0 to 1
                float v = 0.0f;

                // Crop origin
                if (params.cropResize.previewOrigin)
                {
                    constexpr std::array<float, 4> fillColor{ 0.0f, 1.0f, 0.0f, 1.0f };
                    v = getPreviewMarkValue(cropOriginX, cropOriginY, fX, fY, resizedWidth, resizedHeight);
                    for (uint32_t i = 0; i < 4; i++)
                        outputBuffer[redIndex + i] = lerp(outputBuffer[redIndex + i], fillColor[i], v);
                }

                // Transform origin
                if (params.transform.previewOrigin)
                {
                    constexpr std::array<float, 4> fillColor{ 0.0f, 0.0f, 1.0f, 1.0f };
                    v = getPreviewMarkValue(transformOriginX, transformOriginY, fX, fY, resizedWidth, resizedHeight);
                    for (uint32_t i = 0; i < 4; i++)
                        outputBuffer[redIndex + i] = lerp(outputBuffer[redIndex + i], fillColor[i], v);
                }
            }
        }
    }

    clearVector(croppedBuffer);
}

void ImageTransform::applyGPU(
    const ImageTransformParams& params,
    const std::vector<float>& inputBuffer,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<float>& outputBuffer,
    uint32_t& outputWidth,
    uint32_t& outputHeight)
{
    // To be implemented
}

float ImageTransform::getPreviewMarkValue(float originX, float originY, float x, float y, uint32_t bufferWidth, uint32_t bufferHeight)
{
    // Parameters
    constexpr float softness = 0.5f;
    constexpr float outlineRadius = 1.0f;
    constexpr float dotRadius = 1.5f;
    float squareRadius = 0.15f * fminf(bufferWidth, bufferHeight);

    // Chebyshev distance
    float distSquare = fmaxf(fabsf(x - originX), fabsf(y - originY));

    // Square
    float v = fminf(fmaxf(fabsf(distSquare - squareRadius) - outlineRadius, 0.0f) / softness, 1.0f);

    // Dot
    float distDot = sqrtf(powf(x - originX, 2.0f) + powf(y - originY, 2.0f));
    v *= fminf(fmaxf(distDot - dotRadius, 0.0f) / softness, 1.0f);

    return 1.0f - v;
}
