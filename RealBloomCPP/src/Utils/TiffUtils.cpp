#include "TiffUtils.h"

//bool loadPNG(std::string filename, std::vector<uint8_t>& outBuffer, uint32_t& outWidth, uint32_t& outHeight, std::string& outError);
//bool savePNG(std::string filename, std::vector<uint8_t>& buffer, uint32_t width, uint32_t height, std::string& outError);

#define FAIL(ERR) \
outError = ERR; \
TIFFClose(tif); \
return false;

bool loadTIFF(std::string filename, std::vector<float>& outBuffer, uint32_t& outWidth, uint32_t& outHeight, std::string& outError)
{
    outError = "";

    TIFF* tif = TIFFOpen(filename.c_str(), "r");
    if (tif)
    {
        uint32_t channelsPerPixel = 4;

        // Read dimensions of image
        if (TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &outWidth) != 1)
        {
            FAIL("Failed to read width.");
        }
        if (TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &outHeight) != 1)
        {
            FAIL("Failed to read height.");
        }
        if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channelsPerPixel) != 1)
        {
            FAIL("Failed to read samples per pixel.");
        }

        if (channelsPerPixel < 3)
        {
            FAIL("At least 3 color channels are required.");
        }

        uint32_t sampleFormat = 0;
        uint32_t bitsPerSample = 0;
        TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
        TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
        if (sampleFormat != SAMPLEFORMAT_IEEEFP || bitsPerSample != 32)
        {
            FAIL("Input image must be in 32-bit floating point format.");
        }

        uint32_t planarConfig = 0;
        TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
        if (planarConfig != PLANARCONFIG_CONTIG)
        {
            FAIL("The component values must be stored contiguously (RGBRGB).");
        }

        uint32_t scanSizeBytes = TIFFScanlineSize(tif);
        uint32_t rowSizeBytes = outWidth * channelsPerPixel * (bitsPerSample / 8);
        if (scanSizeBytes != rowSizeBytes)
        {
            FAIL("Unexpected scanline size");
        }

        // Make space for image in memory
        float* rowBuffer = new float[outWidth * channelsPerPixel]; // scanLength / bytesPerSample
        outBuffer.resize(outWidth * outHeight * 4);

        // Read image data
        uint32_t redIndexRow, redIndexOut;
        for (uint32_t y = 0; y < outHeight; y++)
        {
            TIFFReadScanline(tif, rowBuffer, y);
            for (uint32_t x = 0; x < outWidth; x++)
            {
                redIndexRow = x * channelsPerPixel;
                redIndexOut = (y * outWidth + x) * 4;

                outBuffer[redIndexOut + 0] = rowBuffer[redIndexRow + 0];
                outBuffer[redIndexOut + 1] = rowBuffer[redIndexRow + 1];
                outBuffer[redIndexOut + 2] = rowBuffer[redIndexRow + 2];
                outBuffer[redIndexOut + 3] = 1.0f;
            }
        }

        TIFFClose(tif);
    } else
    {
        outError = "File couldn't be opened.";
        return false;
    }
    return true;
}

bool saveTIFF(std::string filename, float* buffer, uint32_t width, uint32_t height, std::string& outError)
{
    outError = "";

    const uint32_t channelsPerPixel_Input = 4;
    const uint32_t channelsPerPixel = 3;
    const uint32_t bytesPerChannel = 4;
    uint32_t rowSizeBytes = 0;
    float* rowBuffer = nullptr;

    {
        TIFF* tif = TIFFOpen(filename.c_str(), "w");

        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);

        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, channelsPerPixel);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 32);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);

        TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);

        rowSizeBytes = TIFFScanlineSize(tif); // width * channels * bytes per channel
        if (rowSizeBytes != (width * channelsPerPixel * bytesPerChannel))
        {
            FAIL("Unexpected scanline size");
        }

        rowBuffer = new float[rowSizeBytes / bytesPerChannel];

        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, width * channelsPerPixel * bytesPerChannel));

        uint32_t redIndexInp, redIndexRow;
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                redIndexRow = x * channelsPerPixel;
                redIndexInp = (y * width + x) * channelsPerPixel_Input;

                rowBuffer[redIndexRow + 0] = buffer[redIndexInp + 0];
                rowBuffer[redIndexRow + 1] = buffer[redIndexInp + 1];
                rowBuffer[redIndexRow + 2] = buffer[redIndexInp + 2];
            }

            if (TIFFWriteScanline(tif, rowBuffer, y, 0) < 0)
                break;
        }

        TIFFClose(tif);
    }

    if (rowBuffer)
        delete[] rowBuffer;

    return true;
}