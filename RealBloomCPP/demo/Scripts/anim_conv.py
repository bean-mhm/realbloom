# This is a script for using RealBloom's CLI to apply convolutional bloom on a sequence of frames.
# https://github.com/bean-mhm/realbloom

import subprocess
import time

# Paths
rb_path = "path to RealBloom.exe"
frames_dir = "path to the folder containing the frames, must end with a (back)slash"
kernel_path = "path to the kernel"
output_dir = "path to the folder containing the output frames, must end with a (back)slash"

# Frames
frame_start = 1
frame_end = 50

# Example: frame0001.exr
frame_prefix = "frame"
frame_length = 4
frame_suffix = ".exr"

# Color Spaces (w = working space)
frames_cs = "w"
kernel_cs = "w"
output_cs = "w"

# Additional Arguments
arguments = "--kernel-contrast 0.05 --kernel-crop 0.8 --autoexp --mix 0.15"

"""
Arguments:
  --input, -i             * Input filename
  --input-space, -a       * Input color space
  --kernel, -k            * Kernel filename
  --kernel-space, -b      * Kernel color space
  --output, -o            * Output filename
  --output-space, -c      * Output color space
  --kernel-exposure, -e   Kernel exposure [def: 0]
  --kernel-contrast, -f   Kernel contrast [def: 0]
  --kernel-rotation, -r   Kernel rotation [def: 0]
  --kernel-scale, -s      Kernel scale [def: 1]
  --kernel-crop, -q       Kernel crop [def: 1]
  --kernel-center, -u     Kernel center [def: 0.5,0.5]
  --threshold, -t         Threshold [def: 0]
  --knee, -w              Threshold knee [def: 0]
  --autoexp, -n           Auto-Exposure
  --input-mix, -x         Input mix (additive blending)
  --conv-mix, -y          Output mix (additive blending)
  --mix, -m               Blend between input and output (normal blending) [def: 1]
  --conv-exposure, -z     Conv. output exposure [def: 0]
  --verbose, -v           Enable verbose logging
"""

for i in range(frame_start, frame_end + 1):
    filename = frame_prefix + str(i).zfill(frame_length) + frame_suffix
    command = "\"{}\" conv -i \"{}\" -a \"{}\" -k \"{}\" -b \"{}\" -o \"{}\" -c \"{}\" {}"
    command = command.format(
        rb_path,
        frames_dir + filename,
        frames_cs,
        kernel_path,
        kernel_cs,
        output_dir + filename,
        output_cs,
        arguments
    )
    start = time.time()
    subprocess.call(command)
    end = time.time()
    print(filename + " done in {:.1f} ms".format((end - start) * 1000.0))
