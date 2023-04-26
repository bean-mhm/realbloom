#include "ImageTransform.h"

#include <omp.h>

#pragma region Shaders
static const char* fragmentSource = R"glsl(
    #version 150 core

    #define DEG_TO_RAD 0.0174532925199432944

    #define CONTRAST_PIVOT 0.5

    #define SQRT3 1.7320508075688772935274463415059

    in vec2 vTexUV;

    out vec4 outColor;

    uniform sampler2D img;

    uniform vec2 resizedSize;

    uniform vec2 scale;
    uniform float rotate;
    uniform vec2 translate;

    uniform vec2 transformOrigin;
    uniform vec2 cropOrigin;

    uniform vec4 colorMul;
    uniform float contrast;
    uniform int contrastGrayscaleType;
    uniform int grayscaleType;
    uniform float grayscaleMix;

    uniform int transparency;

    uniform int previewOriginCropResize;
    uniform int previewOriginTransform;

    uniform vec4 previewMarksCropColor;
    uniform vec4 previewMarksTransformColor;

    uniform float previewMarksSquareRadius;
    uniform float previewMarksSquareOutlineRadius;
    uniform float previewMarksDotRadius;

    uniform float previewMarksSoftness;

    float lerp(float a, float b, float t)
    {
        return a + ((b - a) * t);
    }

    float applyContrast(float v, float contrast)
    {
        if (contrast == 0.0)
            return v;

        // v0.5.2-dev
        float c =
            (contrast >= 0.0) ?
            (2.0 * contrast + 1.0) :
            (1.0 / (-2.0 * contrast + 1.0));
        return pow(v / CONTRAST_PIVOT, c) * CONTRAST_PIVOT;
    }

    vec2 rotatePoint(float x, float y, float pivotX, float pivotY, float angle)
    {
        float s = sin(angle * DEG_TO_RAD);
        float c = cos(angle * DEG_TO_RAD);

        return vec2(
            ((x - pivotX) * c - (y - pivotY) * s) + pivotX,
            ((x - pivotX) * s + (y - pivotY) * c) + pivotY
        );
    }

    float rgbaToGrayscale(vec4 rgba, int type)
    {
        switch (type)
        {

        // None
        case 0:
            return 0.0;
            break;

        // Luminance
        case 1:
            return (rgba.x * 0.2126) + (rgba.y * 0.7152) + (rgba.z * 0.0722);
            break;

        // Average
        case 2:
            return (rgba.x + rgba.y + rgba.z) / 3.0;
            break;

        // Sum
        case 3:
            return rgba.x + rgba.y + rgba.z;
            break;

        // Maximum
        case 4:
            return max(max(rgba.x, rgba.y), rgba.z);
            break;

        // Minimum
        case 5:
            return min(min(rgba.x, rgba.y), rgba.z);
            break;

        // Magnitude
        case 6:
            return sqrt((rgba.x * rgba.x) + (rgba.y * rgba.y) + (rgba.z * rgba.z));
            break;

        // MagOverSqrt3
        case 7:
            return sqrt((rgba.x * rgba.x) + (rgba.y * rgba.y) + (rgba.z * rgba.z)) / SQRT3;
            break;

        // Red
        case 8:
            return rgba.x;
            break;

        // Green
        case 9:
            return rgba.y;
            break;

        // Blue
        case 10:
            return rgba.z;
            break;

        // Alpha
        case 11:
            return rgba.w;
            break;

        default:
            break;

        }
    
        return rgba.x;
    }

    float getPreviewMarkValue(vec2 uv, vec2 origin)
    {
        // Chebyshev distance
        float distSquare = max(abs(uv.x - origin.x), abs(uv.y - origin.y));
    
        // Square
        float v = min(max(abs(distSquare - previewMarksSquareRadius) - previewMarksSquareOutlineRadius, 0.0) / previewMarksSoftness, 1.0);
    
        // Dot
        float distDot = distance(uv, origin);
        v *= min(max(distDot - previewMarksDotRadius, 0.0) / previewMarksSoftness, 1.0);
    
        return 1.0 - v;
    }

    void main()
    {
        vec2 uv = vTexUV * resizedSize;

        // Translate
        vec2 transUV = uv - translate;

        // Rotate
        transUV = rotatePoint(transUV.x, transUV.y, transformOrigin.x, transformOrigin.y, -rotate);

        // Scale
        transUV = ((transUV - transformOrigin) / scale) + transformOrigin;

        // Sample the color and multiply by colorMul
        outColor = texture(img, transUV / resizedSize) * colorMul;

        // Remove alpha if there's no transparency
        if (transparency == 0)
            outColor = vec4(outColor.xyz, 1.0);

        // Get the monotonic value for contrast
        float mono = rgbaToGrayscale(outColor, contrastGrayscaleType);
        
        // Contrast
        if (mono > 0.0)
        {
            float mul = applyContrast(mono, contrast) / mono;
            outColor = vec4(outColor.xyz * mul, outColor.w);
        }
        
        // Grayscale
        if (grayscaleType != 0)
        {
            float v = rgbaToGrayscale(outColor, grayscaleType);

            if (grayscaleMix == 1.0)
            {
                outColor = vec4(v, v, v, 1.0);
            }
            else
            {
                outColor = vec4(lerp(outColor.x, v, grayscaleMix), lerp(outColor.y, v, grayscaleMix), lerp(outColor.z, v, grayscaleMix), 1.0);
            }
        }

        // Preview marks
        if (previewOriginCropResize != 0)
        {
            outColor += getPreviewMarkValue(uv, cropOrigin) * (previewMarksCropColor - outColor);
        }
        if (previewOriginTransform != 0)
        {
            outColor += getPreviewMarkValue(uv, transformOrigin) * (previewMarksTransformColor - outColor);
        }

    }
)glsl";
#pragma endregion

#pragma region Constants

static constexpr std::array<float, 4> previewMarksCropColor{ 1.0f, 0.0f, 0.0f, 1.0f };
static constexpr std::array<float, 4> previewMarksTransformColor{ 0.0f, 0.0f, 1.0f, 1.0f };

static constexpr float previewMarksSquareRadiusRatio = 0.06f;
static constexpr float previewMarksSquareOutlineRadius = 1.0f;
static constexpr float previewMarksDotRadius = 1.5f;

static constexpr float previewMarksSoftness = 1.0f;

#pragma endregion


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
    filter = { 1.0f, 1.0f, 1.0f };
    exposure = 0.0f;
    contrast = 0.0f;
    contrastGrayscaleType = GrayscaleType::MagOverSqrt3;
    grayscaleType = GrayscaleType::None;
    grayscaleMix = 1.0f;
}

void ImageTransformParams::reset()
{
    cropResize.reset();
    transform.reset();
    color.reset();
    transparency = false;
}

bool ImageTransform::S_USE_GPU = false;

GLuint ImageTransform::s_vertShader = 0;
GLuint ImageTransform::s_fragShader = 0;
GLuint ImageTransform::s_program = 0;

bool ImageTransform::s_gpuInitialized = false;

void ImageTransform::ensureInitGPU()
{
    if (s_gpuInitialized)
        return;
    else
        s_gpuInitialized = true;

    try
    {
        std::string shaderLog;

        // Create and compile the vertex shader
        if (!createShader(GL_VERTEX_SHADER, GL_BASE_VERTEX_SOURCE, s_vertShader, shaderLog))
            throw std::exception(
                strFormat("Vertex shader compilation error: %s", shaderLog.c_str()).c_str()
            );

        // Create and compile the fragment shader
        if (!createShader(GL_FRAGMENT_SHADER, fragmentSource, s_fragShader, shaderLog))
            throw std::exception(
                strFormat("Fragment shader compilation error: %s", shaderLog.c_str()).c_str()
            );

        // Create the program
        s_program = glCreateProgram();
        checkGlStatus("", "glCreateProgram");

        glAttachShader(s_program, s_vertShader);
        checkGlStatus("", "glAttachShader(m_program, m_vertShader)");

        glAttachShader(s_program, s_fragShader);
        checkGlStatus("", "glAttachShader(m_program, m_fragShader)");

        glBindFragDataLocation(s_program, 0, "outColor");
        checkGlStatus("", "glBindFragDataLocation");

        glLinkProgram(s_program);
        checkGlStatus("", "glLinkProgram");

        glUseProgram(s_program);
        checkGlStatus("", "glUseProgram");
    }
    catch (const std::exception& e)
    {
        throw std::exception(makeError(__FUNCTION__, "", e.what()).c_str());
    }
}

void ImageTransform::applyNoCropCPU(
    const ImageTransformParams& params,
    const std::vector<float>* lastBuffer,
    uint32_t lastBufferWidth,
    uint32_t lastBufferHeight,
    std::vector<float>& outputBuffer,
    uint32_t resizedWidth,
    uint32_t resizedHeight,
    float resizeX,
    float resizeY,
    bool previewMode)
{
    // Prepare the output buffer
    uint32_t outputBufferSize = resizedWidth * resizedHeight * 4;
    outputBuffer.resize(outputBufferSize);

    // Transform origin
    const float transformOriginX = params.transform.origin[0] * resizedWidth;
    const float transformOriginY = params.transform.origin[1] * resizedHeight;

    // Crop origin
    const float cropOriginX = params.cropResize.origin[0] * resizedWidth;
    const float cropOriginY = params.cropResize.origin[1] * resizedHeight;

    // Scale (non-zero)
    float scaleX = params.transform.scale[0];
    float scaleY = params.transform.scale[1];
    if (scaleX == 0.0f) scaleX == EPSILON;
    if (scaleY == 0.0f) scaleY == EPSILON;

    // Color multiplier
    const float expMul = getExposureMul(params.color.exposure);
    const float colorMulR = expMul * params.color.filter[0];
    const float colorMulG = expMul * params.color.filter[1];
    const float colorMulB = expMul * params.color.filter[2];

    const bool grayscale = (params.color.grayscaleType != GrayscaleType::None);
    const bool grayscaleMixing = (params.color.grayscaleMix != 1.0f);

    const bool transparency = params.transparency;

    // Check if we'll need to draw preview marks for crop and transform origins
    const bool previewOrigins = (params.cropResize.previewOrigin || params.transform.previewOrigin) && previewMode;

    // Check if there's no need for any transforms
    bool noTrans =
        (resizeX == 1.0f)
        && (resizeY == 1.0f)
        && (scaleX == 1.0f)
        && (scaleY == 1.0f)
        && (params.transform.rotate == 0.0f)
        && (params.transform.translate[0] == 0.0f)
        && (params.transform.translate[1] == 0.0f)
        && (!previewOrigins)
        && (params.color.filter[0] == 1.0f)
        && (params.color.filter[1] == 1.0f)
        && (params.color.filter[2] == 1.0f)
        && (params.color.exposure == 0.0f)
        && (params.color.contrast == 0.0f)
        && (!grayscale);

    // Resize, Scale, Rotate, Translate, Color Transforms
    if (noTrans)
    {
        outputBuffer = *lastBuffer;

        // Reset the alpha channel if there's no transparency
        if (!params.transparency)
        {
            for (uint32_t i = 0; i < outputBuffer.size(); i++)
                if (i % 4 == 3) outputBuffer[i] = 1.0f;
        }
    }
    else
    {
#pragma omp parallel for
        for (int y = 0; y < resizedHeight; y++)
        {
            uint32_t redIndexInput, redIndexOutput;
            float transX, transY;
            float targetColor[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
            Bilinear bil;

            for (int x = 0; x < resizedWidth; x++)
            {
                // Translate
                transX = (x + 0.5f) - (params.transform.translate[0] * resizedWidth);
                transY = (y + 0.5f) - (params.transform.translate[1] * resizedHeight);

                // Rotate
                rotatePointInPlace(transX, transY, transformOriginX, transformOriginY, -params.transform.rotate);

                // Scale
                transX = ((transX - transformOriginX) / scaleX) + transformOriginX;
                transY = ((transY - transformOriginY) / scaleY) + transformOriginY;

                // Resize
                transX /= resizeX;
                transY /= resizeY;

                // Interpolate
                bil.calc(transX, transY);
                for (auto& v : targetColor) v = 0.0f;
                if (params.transparency)
                {
                    if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.topLeftPos[1] * lastBufferWidth + bil.topLeftPos[0]) * 4;
                        blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topLeftWeight);
                    }
                    if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.topRightPos[1] * lastBufferWidth + bil.topRightPos[0]) * 4;
                        blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topRightWeight);
                    }
                    if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.bottomLeftPos[1] * lastBufferWidth + bil.bottomLeftPos[0]) * 4;
                        blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomLeftWeight);
                    }
                    if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.bottomRightPos[1] * lastBufferWidth + bil.bottomRightPos[0]) * 4;
                        blendAddRGBA(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomRightWeight);
                    }
                }
                else
                {
                    targetColor[3] = 1.0f;
                    if (checkBounds(bil.topLeftPos[0], bil.topLeftPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.topLeftPos[1] * lastBufferWidth + bil.topLeftPos[0]) * 4;
                        blendAddRGB(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topLeftWeight);
                    }
                    if (checkBounds(bil.topRightPos[0], bil.topRightPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.topRightPos[1] * lastBufferWidth + bil.topRightPos[0]) * 4;
                        blendAddRGB(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.topRightWeight);
                    }
                    if (checkBounds(bil.bottomLeftPos[0], bil.bottomLeftPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.bottomLeftPos[1] * lastBufferWidth + bil.bottomLeftPos[0]) * 4;
                        blendAddRGB(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomLeftWeight);
                    }
                    if (checkBounds(bil.bottomRightPos[0], bil.bottomRightPos[1], lastBufferWidth, lastBufferHeight))
                    {
                        redIndexInput = (bil.bottomRightPos[1] * lastBufferWidth + bil.bottomRightPos[0]) * 4;
                        blendAddRGB(targetColor, 0, (*lastBuffer).data(), redIndexInput, bil.bottomRightWeight);
                    }
                }

                // Filter, Exposure
                targetColor[0] *= colorMulR;
                targetColor[1] *= colorMulG;
                targetColor[2] *= colorMulB;

                // Get the monotonic value for contrast
                float mono = rgbaToGrayscale(targetColor, params.color.contrastGrayscaleType);

                // Contrast
                if (mono > 0.0f)
                {
                    float mul = applyContrast(mono, params.color.contrast) / mono;
                    targetColor[0] *= mul;
                    targetColor[1] *= mul;
                    targetColor[2] *= mul;
                }

                // Grayscale
                if (grayscale)
                {
                    if (grayscaleMixing)
                    {
                        float v = rgbaToGrayscale(targetColor, params.color.grayscaleType);

                        targetColor[0] = lerp(targetColor[0], v, params.color.grayscaleMix);
                        targetColor[1] = lerp(targetColor[1], v, params.color.grayscaleMix);
                        targetColor[2] = lerp(targetColor[2], v, params.color.grayscaleMix);
                        targetColor[3] = 1.0f;
                    }
                    else
                    {
                        targetColor[0] = rgbaToGrayscale(targetColor, params.color.grayscaleType);
                        targetColor[1] = targetColor[0];
                        targetColor[2] = targetColor[0];
                        targetColor[3] = 1.0f;
                    }
                }

                // Put tagetColor in the output buffer
                redIndexOutput = (y * resizedWidth + x) * 4;
                std::copy(targetColor, targetColor + 4, &(outputBuffer[redIndexOutput]));
            }
        }
    }

    // Preview origins
    if (previewOrigins)
    {
        const float squareRadius = previewMarksSquareRadiusRatio * fminf(resizedWidth, resizedHeight);

        // Draw
#pragma omp parallel for
        for (int y = 0; y < resizedHeight; y++)
        {
            for (int x = 0; x < resizedWidth; x++)
            {
                uint32_t redIndex = 4 * (y * resizedWidth + x);

                float fX = x + 0.5f;
                float fY = y + 0.5f;

                // Pixel value, goes from source pixel to filled from 0 to 1
                float v = 0.0f;

                // Crop origin
                if (params.cropResize.previewOrigin)
                {
                    v = getPreviewMarkValue(fX, fY, cropOriginX, cropOriginY, squareRadius);
                    for (uint32_t i = 0; i < 4; i++)
                        outputBuffer[redIndex + i] = lerp(outputBuffer[redIndex + i], previewMarksCropColor[i], v);
                }

                // Transform origin
                if (params.transform.previewOrigin)
                {
                    v = getPreviewMarkValue(fX, fY, transformOriginX, transformOriginY, squareRadius);
                    for (uint32_t i = 0; i < 4; i++)
                        outputBuffer[redIndex + i] = lerp(outputBuffer[redIndex + i], previewMarksTransformColor[i], v);
                }
            }
        }
    }
}

void ImageTransform::applyNoCropGPU(
    const ImageTransformParams& params,
    const std::vector<float>* lastBuffer,
    uint32_t lastBufferWidth,
    uint32_t lastBufferHeight,
    std::vector<float>& outputBuffer,
    uint32_t resizedWidth,
    uint32_t resizedHeight,
    float resizeX,
    float resizeY,
    bool previewMode)
{
    try
    {
        ensureInitGPU();

        // Create a texture
        GlTexture texture(lastBufferWidth, lastBufferHeight, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_LINEAR, GL_RGBA32F);

        // Set the background color
        if (params.transparency)
            texture.setBorderColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        else
            texture.setBorderColor({ 0.0f, 0.0f, 0.0f, 1.0f });

        // Upload the input buffer
        texture.upload(lastBuffer->data());

        // Create a frame buffer
        GlFrameBuffer frameBuffer(resizedWidth, resizedHeight);

        // Bind the frame buffer so we can render the transformed image into it
        frameBuffer.bind();

        // Set the viewport
        frameBuffer.viewport();

        // Use the shader program
        glUseProgram(s_program);
        checkGlStatus("", "glUseProgram");

        // Configure the input textures and uniforms
        {
            texture.bind(GL_TEXTURE0);

            findAndSetUniform1i(s_program, "img", 0);

            // Scale (non-zero)
            float scaleX = params.transform.scale[0];
            float scaleY = params.transform.scale[1];
            if (scaleX == 0.0f) scaleX == EPSILON;
            if (scaleY == 0.0f) scaleY == EPSILON;

            findAndSetUniform2f(s_program, "resizedSize", resizedWidth, resizedHeight);
            findAndSetUniform2f(s_program, "scale", scaleX, scaleY);
            findAndSetUniform1f(s_program, "rotate", params.transform.rotate);

            findAndSetUniform2f(
                s_program,
                "translate",
                params.transform.translate[0] * resizedWidth,
                params.transform.translate[1] * resizedHeight);

            findAndSetUniform2f(
                s_program,
                "transformOrigin",
                params.transform.origin[0] * resizedWidth,
                params.transform.origin[1] * resizedHeight);

            findAndSetUniform2f(
                s_program,
                "cropOrigin",
                params.cropResize.origin[0] * resizedWidth,
                params.cropResize.origin[1] * resizedHeight);

            float expMul = getExposureMul(params.color.exposure);

            findAndSetUniform4f(
                s_program,
                "colorMul",
                expMul * params.color.filter[0],
                expMul * params.color.filter[1],
                expMul * params.color.filter[2],
                1.0f);

            findAndSetUniform1f(s_program, "contrast", params.color.contrast);
            findAndSetUniform1i(s_program, "contrastGrayscaleType", (int)params.color.contrastGrayscaleType);
            findAndSetUniform1i(s_program, "grayscaleType", (int)params.color.grayscaleType);
            findAndSetUniform1f(s_program, "grayscaleMix", params.color.grayscaleMix);
            findAndSetUniform1i(s_program, "transparency", params.transparency ? 1 : 0);

            findAndSetUniform1i(s_program, "previewOriginCropResize", params.cropResize.previewOrigin ? 1 : 0);
            findAndSetUniform1i(s_program, "previewOriginTransform", params.transform.previewOrigin ? 1 : 0);

            findAndSetUniform4f(
                s_program,
                "previewMarksCropColor",
                previewMarksCropColor[0],
                previewMarksCropColor[1],
                previewMarksCropColor[2],
                previewMarksCropColor[3]);

            findAndSetUniform4f(
                s_program,
                "previewMarksTransformColor",
                previewMarksTransformColor[0],
                previewMarksTransformColor[1],
                previewMarksTransformColor[2],
                previewMarksTransformColor[3]);

            float previewMarksSquareRadius = previewMarksSquareRadiusRatio * fminf(resizedWidth, resizedHeight);

            findAndSetUniform1f(s_program, "previewMarksSquareRadius", previewMarksSquareRadius);
            findAndSetUniform1f(s_program, "previewMarksSquareOutlineRadius", previewMarksSquareOutlineRadius);
            findAndSetUniform1f(s_program, "previewMarksDotRadius", previewMarksDotRadius);

            findAndSetUniform1f(s_program, "previewMarksSoftness", previewMarksSoftness);
        }

        // Clear the buffer
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        checkGlStatus("", "Clear");

        // Draw
        {
            GlFullPlaneVertices::enable(s_program);

            // Draw
            glDrawArrays(GL_TRIANGLES, 0, 6);
            checkGlStatus("", "glDrawArrays");

            GlFullPlaneVertices::disable(s_program);
        }

        // Unbind the frame buffer
        frameBuffer.unbind();

        // Prepare the output buffer
        outputBuffer.resize(frameBuffer.getWidth() * frameBuffer.getHeight() * 4);

        // Read back

        glBindTexture(GL_TEXTURE_2D, frameBuffer.getColorBuffer());
        checkGlStatus("", "Read back: glBindTexture");

        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, outputBuffer.data());
        checkGlStatus("", "Read back: glGetTexImage");
    }
    catch (const std::exception& e)
    {
        printError(__FUNCTION__, "", e.what());
    }
}

float ImageTransform::getPreviewMarkValue(float x, float y, float originX, float originY, float squareRadius)
{
    // Chebyshev distance
    float distSquare = fmaxf(fabsf(x - originX), fabsf(y - originY));

    // Square
    float v = fminf(fmaxf(fabsf(distSquare - squareRadius) - previewMarksSquareOutlineRadius, 0.0f) / previewMarksSoftness, 1.0f);

    // Dot
    float distDot = sqrtf(powf(x - originX, 2.0f) + powf(y - originY, 2.0f));
    v *= fminf(fmaxf(distDot - previewMarksDotRadius, 0.0f) / previewMarksSoftness, 1.0f);

    return 1.0f - v;
}

void ImageTransform::cleanUp()
{
    if (!s_gpuInitialized)
        return;

    glDeleteProgram(s_program);
    glDeleteShader(s_fragShader);
    glDeleteShader(s_vertShader);
    clearGlStatus();
}

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
    uint32_t& outputHeight,
    bool previewMode)
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

    outputWidth = resizedWidth;
    outputHeight = resizedHeight;

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

#pragma omp parallel for
        for (int y = 0; y < croppedHeight; y++)
        {
            for (int x = 0; x < croppedWidth; x++)
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

    // Call the appropraite function
    if (S_USE_GPU)
    {
        applyNoCropGPU(
            params,
            lastBuffer,
            croppedWidth,
            croppedHeight,
            outputBuffer,
            resizedWidth,
            resizedHeight,
            resizeX,
            resizeY,
            previewMode);
    }
    else
    {
        applyNoCropCPU(
            params,
            lastBuffer,
            croppedWidth,
            croppedHeight,
            outputBuffer,
            resizedWidth,
            resizedHeight,
            resizeX,
            resizeY,
            previewMode);
    }
}
