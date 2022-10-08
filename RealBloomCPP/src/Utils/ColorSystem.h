#pragma once

#include <math.h>


// NOTE: THE CONTENTS OF THIS FILE IS A SLIGHTLY MODIFIED VERSION OF ANOTHER PERSON'S CODE.
// SOURCE:
// https://github.com/TomCrypto/fraunhofer/blob/master/src/main.cpp


struct ColorSystem
{
public:
    double xRed, yRed;	    	    // Red x, y
    double xGreen, yGreen;  	    // Green x, y
    double xBlue, yBlue;     	    // Blue x, y
    double xWhite, yWhite;  	    // White point x, y
    double gamma;   	    	    // Gamma correction for system

    void XYZtoRGB(double x, double y, double z, double& outR, double& outG, double& outB) const;
};

namespace ColorSystems
{
    // These are some relatively common illuminants (white points).
#define IlluminantC     0.3101, 0.3162	    	// For NTSC television
#define IlluminantD65   0.3127, 0.3291	    	// For EBU and SMPTE
#define IlluminantE 	0.33333333, 0.33333333  // CIE equal-energy illuminant

// 0 represents a special gamma function.
#define GAMMA_REC709 0

// These are some standard color systems.
    const ColorSystem // xRed    yRed    xGreen  yGreen  xBlue  yBlue    White point        Gamma
        EBU = { 0.64,   0.33,   0.29,   0.60,   0.15,   0.06,   IlluminantD65,  GAMMA_REC709 },
        SMPTE = { 0.630,  0.340,  0.310,  0.595,  0.155,  0.070,  IlluminantD65,  GAMMA_REC709 },
        HDTV = { 0.670,  0.330,  0.210,  0.710,  0.150,  0.060,  IlluminantD65,  GAMMA_REC709 },
        Rec709 = { 0.64,   0.33,   0.30,   0.60,   0.15,   0.06,   IlluminantD65,  GAMMA_REC709 },
        NTSC = { 0.67,   0.33,   0.21,   0.71,   0.14,   0.08,   IlluminantC,    GAMMA_REC709 },
        CIE = { 0.7355, 0.2645, 0.2658, 0.7243, 0.1669, 0.0085, IlluminantE,    GAMMA_REC709 };
}