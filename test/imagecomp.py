import sys
import os
import warnings

#You may have to install SSIM_PIL from
#https://github.com/mmertama/SSIM-PIL.git
#as that wont show they annoying warning
#about pyopencl windows (also on OSX)
#even the GPU=FALSE as below

from SSIM_PIL import compare_ssim
from PIL import Image

if __name__ == '__main__':
    p1 = sys.argv[1]
    p2 = sys.argv[2]
    if not os.path.exists(p1):
        sys.exit("No found: " + p1)
    if not os.path.exists(p2):
        sys.exit("No found: " + p2)
    image1 = Image.open(p1)
    image2 = Image.open(p2)
    value = compare_ssim(image1, image2, GPU=False)
    print(value)

