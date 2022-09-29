#include "PngUtils.h"

using namespace std;

inline uint8_t transPNG(float v)
{
    return (unsigned char)roundf(linearToSrgb(v) * 255.0f);
}

inline float revTransPNG(uint8_t v)
{
    return srgbToLinear((float)v / 255.0f);
}

bool loadPNG(std::string filename, std::vector<float>& outBuffer, uint32_t& outWidth, uint32_t& outHeight, std::string& outError)
{
    outError = "";

    std::vector<uint8_t> buffer;
    unsigned int error = lodepng::decode(buffer, outWidth, outHeight, filename);
    if (error)
    {
        outError = lodepng_error_text(error);
        return false;
    } else
    {
        uint32_t bufferSize = outWidth * outHeight * 4;
        outBuffer.resize(bufferSize);

        for (uint32_t i = 0; i < bufferSize; i++)
        {
            outBuffer[i] = revTransPNG(buffer[i]);
        }
    }
    return true;
}

bool savePNG(std::string filename, float* buffer, bool isRGBA, bool FlipY, uint32_t width, uint32_t height, std::string& outError)
{
    outError = "";

    uint32_t bufferSize = width * height * 4;
    std::vector<uint8_t> buffer8bit;
    buffer8bit.resize(bufferSize);

    uint32_t numChannels = isRGBA ? 4 : 3;

    uint32_t redIndexIn, redIndexOut;
    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            redIndexIn = FlipY ? ((height - 1 - y) * width + x) * numChannels : (y * width + x) * numChannels;
            redIndexOut = (y * width + x) * 4;

            buffer8bit[redIndexOut + 0] = transPNG(buffer[redIndexIn + 0]);
            buffer8bit[redIndexOut + 1] = transPNG(buffer[redIndexIn + 1]);
            buffer8bit[redIndexOut + 2] = transPNG(buffer[redIndexIn + 2]);

            if (isRGBA)
                buffer8bit[redIndexOut + 3] = buffer[redIndexIn + 3];
            else buffer8bit[redIndexOut + 3] = 255;
        }
    }

    unsigned int error = lodepng::encode(filename, buffer8bit, width, height);

    if (error)
    {
        outError = lodepng_error_text(error);
        return false;
    }
    return true;
}