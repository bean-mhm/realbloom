#include "ColorSystem.h"


// NOTE: THE CONTENTS OF THIS FILE IS A SLIGHTLY MODIFIED VERSION OF ANOTHER PERSON'S CODE.
// SOURCE:
// https://github.com/TomCrypto/fraunhofer/blob/master/src/main.cpp


void ColorSystem::XYZtoRGB(double x, double y, double z, double& outR, double& outG, double& outB) const
{
    // Decode the color system.
    double xr = xRed;   double yr = yRed;   double zr = 1 - (xr + yr);
    double xg = xGreen; double yg = yGreen; double zg = 1 - (xg + yg);
    double xb = xBlue;  double yb = yBlue;  double zb = 1 - (xb + yb);
    double xw = xWhite; double yw = yWhite; double zw = 1 - (xw + yw);

    // Compute the XYZ to RGB matrix.
    double rx = (yg * zb) - (yb * zg);
    double ry = (xb * zg) - (xg * zb);
    double rz = (xg * yb) - (xb * yg);
    double gx = (yb * zr) - (yr * zb);
    double gy = (xr * zb) - (xb * zr);
    double gz = (xb * yr) - (xr * yb);
    double bx = (yr * zg) - (yg * zr);
    double by = (xg * zr) - (xr * zg);
    double bz = (xr * yg) - (xg * yr);

    // Compute the RGB luminance scaling factor.
    double rw = ((rx * xw) + (ry * yw) + (rz * zw)) / yw;
    double gw = ((gx * xw) + (gy * yw) + (gz * zw)) / yw;
    double bw = ((bx * xw) + (by * yw) + (bz * zw)) / yw;

    // Scale the XYZ to RGB matrix to white.
    rx = rx / rw;  ry = ry / rw;  rz = rz / rw;
    gx = gx / gw;  gy = gy / gw;  gz = gz / gw;
    bx = bx / bw;  by = by / bw;  bz = bz / bw;

    // Calculate the desired RGB.
    outR = (rx * x) + (ry * y) + (rz * z);
    outG = (gx * x) + (gy * y) + (gz * z);
    outB = (bx * x) + (by * y) + (bz * z);

    // Constrain the RGB color within the RGB gamut.
    double w = fminf(0.0f, fminf(outR, fminf(outG, outB)));
    outR -= w; outG -= w; outB -= w;
}