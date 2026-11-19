#include <fstream>
#include <lottie/Core.hpp>
#include <stdexcept>
#include <vector>
namespace {
using Bytes = std::vector<unsigned char>;
void u32(Bytes &b, uint32_t n) {
    for (int s = 24; s >= 0; s -= 8)
        b.push_back((n >> s) & 255);
}
void chunk(Bytes &out, const char *type, const Bytes &data) {
    u32(out, uint32_t(data.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t crc = ~0u;
    for (size_t i = start; i < out.size(); ++i) {
        crc ^= out[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320u & uint32_t(-int(crc & 1)));
    }
    u32(out, ~crc);
}
} // namespace
// Minimal standards-compliant PNG encoder, stored DEFLATE blocks, straight RGBA.
void savePng(const char *path, const lottie::Frame &f) {
    Bytes raw;
    raw.reserve((size_t(f.width) * 4 + 1) * f.height);
    for (uint32_t y = 0; y < f.height; ++y) {
        raw.push_back(0);
        for (uint32_t x = 0; x < f.width; ++x) {
            uint32_t p = f.pixels[size_t(y) * f.width + x], a = p >> 24;
            for (int s = 0; s < 24; s += 8)
                raw.push_back(a ? static_cast<unsigned char>(
                                      std::min(255u, (((p >> s) & 255) * 255 + a / 2) / a))
                                : 0);
            raw.push_back(a);
        }
    }
    Bytes z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (auto c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t pos = 0; pos < raw.size();) {
        unsigned n = unsigned(std::min(size_t(65535), raw.size() - pos));
        z.push_back(pos + n == raw.size() ? 1 : 0);
        z.push_back(n & 255);
        z.push_back(n >> 8);
        z.push_back((~n) & 255);
        z.push_back(((~n) >> 8) & 255);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    }
    u32(z, (b << 16) | a);
    Bytes png{137, 80, 78, 71, 13, 10, 26, 10}, header;
    u32(header, f.width);
    u32(header, f.height);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    chunk(png, "IHDR", header);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<char *>(png.data()), png.size());
    if (!file)
        throw std::runtime_error("PNG write failed");
}
