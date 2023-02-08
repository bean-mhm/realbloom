# This is a script for using RealBloom's CLI to apply convolutional bloom on a sequence of frames.
# https://github.com/bean-mhm/realbloom

import subprocess
import time

# Paths
rb_path = 'path to RealBloom.exe'
frames_dir = 'path to the folder containing the frames, must end with a (back)slash'
kernel_path = 'path to the kernel'
output_dir = 'path to the folder containing the output frames, must end with a (back)slash'

# Frames
frame_start = 1
frame_end = 50

# Example: frame0001.exr
frame_prefix = 'frame'
frame_length = 4
frame_suffix = '.exr'

# Color spaces (w = working space)
frames_cs = 'w'
kernel_cs = 'w'
output_cs = 'w'

# Additional arguments
arguments = '--kernel-contrast 0.05 --kernel-crop 0.8 --autoexp --mix 0.15'

'''
Arguments:
  --input, -i             * Input filename
  --input-space, -a       * Input color space
  --kernel, -k            * Kernel filename
  --kernel-space, -b      * Kernel color space
  --output, -o            * Output filename
  --output-space, -p      Output color space (no view transform)
  --display, -h           Display name for view transform
  --view, -j              View name for view transform
  --look, -l              Look name for view transform
  --view-exp              Exposure for view transform [def: 0]
  --kernel-exposure, -e   Kernel exposure [def: 0]
  --kernel-contrast, -c   Kernel contrast [def: 0]
  --kernel-color, -f      Kernel color [def: 1,1,1]
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
'''

# Print the command line output from RealBloom
def print_stdout(proc: subprocess.Popen):
    s = ''
    last_char = '\n'
    
    while True:
        char = proc.stdout.read(1).decode()
        
        if len(char) < 1:
            break
        
        if last_char == '\n' and char == '>':
            break
        
        last_char = char
        
        s += char
    
    s = s.strip()
    if (s != ''):
        # Left padding
        s = '  ' + s
        s = s.replace('\n', '\n  ')
        
        # Vertical padding
        if '\n' in s:
            print(f'\n{s}\n')
        else:
            print(s)

# Create subprocess
proc = subprocess.Popen(
    [rb_path, 'cli'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE)
print_stdout(proc)

# Process the frames
for i in range(frame_start, frame_end + 1):
    filename = frame_prefix + str(i).zfill(frame_length) + frame_suffix
    
    command = f'conv -i "{frames_dir + filename}" -a "{frames_cs}" -k "{kernel_path}" -b "{kernel_cs}" '
    command += f'-o "{output_dir + filename}" -p "{output_cs}" {arguments}'
    
    # Run
    start = time.time()
    proc.stdin.write(f'{command}\n'.encode())
    proc.stdin.flush()
    print_stdout(proc)
    end = time.time()
    
    print(f'{filename} done in {(end - start) * 1000.0:.1f} ms')

print('')

# Quit

proc.stdin.write('quit\n'.encode())
proc.stdin.flush()
print_stdout(proc)

proc.stdin.close()
proc.terminate()
proc.wait(timeout=5)

print('All frames done')
