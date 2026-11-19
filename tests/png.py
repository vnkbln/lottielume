"""Independent PNG decoder checks, using only the Python standard library."""
import pathlib, struct, subprocess, sys, tempfile, zlib
exe, fixture = sys.argv[1:]
with tempfile.TemporaryDirectory() as temp:
    for seconds, center in [(0, 60), (1, 160), (2, 260)]:
        output = pathlib.Path(temp) / f'frame-{seconds}.png'
        subprocess.run([exe, fixture, str(output), str(seconds)], check=True)
        data = output.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n'
        offset, compressed, dimensions = 8, b'', None
        while offset < len(data):
            size = struct.unpack('>I', data[offset:offset+4])[0]
            tag = data[offset+4:offset+8]
            payload = data[offset+8:offset+8+size]
            crc = struct.unpack('>I', data[offset+8+size:offset+12+size])[0]
            assert zlib.crc32(tag + payload) == crc, 'PNG CRC'
            if tag == b'IHDR':
                dimensions = struct.unpack('>IIBBBBB', payload)
            elif tag == b'IDAT':
                compressed += payload
            offset += size+12
        assert dimensions == (320, 180, 8, 6, 0, 0, 0)
        raw = zlib.decompress(compressed)
        stride = 320*4+1
        assert len(raw) == stride*180
        assert all(raw[y*stride] == 0 for y in range(180)), 'PNG filter'
        pixel = raw[90*stride+1+center*4:90*stride+1+center*4+4]
        assert pixel[0] == 255 and 60 <= pixel[1] <= 67 and pixel[2] == 0 and 126 <= pixel[3] <= 128, pixel
        assert raw[1:5] == b'\0\0\0\0', 'Transparent corner'
print('PNG CRC, DEFLATE, straight RGBA and 3 positions passed')
