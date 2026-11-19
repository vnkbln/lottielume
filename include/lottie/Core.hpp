#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace lottie {
// Immutable source bytes may be shared; each renderer owns its ThorVG animation.
struct Asset {
    std::string json, resourcePath;
    static std::shared_ptr<const Asset> load(const std::string &path);
};
struct Metadata {
    float width, height;
    double duration;
    float frames;
};
// RGBA8, premultiplied alpha, top row first; little-endian host required.
struct Frame {
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
enum class Fit { Fit, Fill, Stretch, Original };
// Output-pixel units; positive Y is up. Anchor is relative to fitted canvas center.
struct Transform {
    float x = 0, y = 0, scale = 1, scaleX = 1, scaleY = 1, rotation = 0, anchorX = 0, anchorY = 0;
    Fit fit = Fit::Fit;
    bool clipToCanvas = true;
    bool fixedCropFrame = false; // Crop the fitted frame; transform only its content.
};
struct Palette {
    bool enabled = false;
    std::array<float, 4> alpha{{1, 1, 1, 1}};
    std::array<std::array<float, 3>, 4> colors{{{{243.f / 255, 24.f / 255, 232.f / 255}},
                                                {{156.f / 255, 50.f / 255, 254.f / 255}},
                                                {{39.f / 255, 127.f / 255, 244.f / 255}},
                                                {{0, 243.f / 255, 233.f / 255}}}};
    bool operator==(const Palette &other) const {
        return enabled == other.enabled &&
               (!enabled || (colors == other.colors && alpha == other.alpha));
    }
};
class Renderer {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit Renderer(std::shared_ptr<const Asset> asset);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Metadata metadata() const;
    void setPalette(const Palette &palette);
    void retainPalettes(const std::vector<Palette> &palettes);
    const Frame &render(double seconds, uint32_t width, uint32_t height,
                        const Transform &transform = {});
};
} // namespace lottie
