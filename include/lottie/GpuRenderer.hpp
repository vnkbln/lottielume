#pragma once
#include <lottie/Core.hpp>
namespace lottie {
// Separate GL adapter. Load on any thread; draw/release/destruction of a used renderer
// require its owning host context current. No context is created or switched here.
class GpuRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit GpuRenderer(std::shared_ptr<const Asset> asset);
    ~GpuRenderer();
    Metadata metadata() const;
    void setPalette(const Palette &palette);
    void retainPalettes(const std::vector<Palette> &palettes);
    void render(void *context, uint32_t fbo, double seconds, uint32_t width, uint32_t height,
                const Transform &transform = {});
    void releaseGL();
};
} // namespace lottie
