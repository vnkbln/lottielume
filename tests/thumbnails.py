"""Check the actual FFGL thumbnail ABI against source PNG pixels on Windows."""
import ctypes as c
import argparse
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', default='build')
args = parser.parse_args()
build = root / args.build_dir
class Thumbnail(c.Structure):
    _fields_ = [('width', c.c_uint32), ('height', c.c_uint32), ('buffer', c.c_void_p)]
for relative, kind in [('LottiePlayer.dll', 'Player'), ('LottieEmitter.dll', 'Emitter'), ('cpu/LottiePlayer.dll', 'Player')]:
    dll = c.WinDLL(str(build / 'Release' / relative))
    entry = dll.plugMain
    entry.argtypes = [c.c_uint32, c.c_void_p, c.c_void_p]
    entry.restype = c.c_size_t
    info = Thumbnail()
    assert entry(42, c.byref(info), None) == 0
    assert (info.width, info.height) == (160, 120)
    pixels = (c.c_uint32 * (info.width * info.height))()
    info.buffer = c.addressof(pixels)
    assert entry(42, c.byref(info), None) == 0
    im = Image.open(root / 'assets' / 'icons' / f'Lottie_{kind}.png').convert('RGBA')
    expected = [int.from_bytes(bytes(p), 'big') for p in im.get_flattened_data()]
    assert list(pixels) == expected, relative
    print(f'{relative}: 160x120, all RGBA pixels match source PNG')
