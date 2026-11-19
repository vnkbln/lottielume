#include "Palette.hpp"
#include "RenderBounds.hpp"
#include "tvgCanvas.h"
#include "tvgScene.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <lottie/Core.hpp>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thorvg.h>
#include <thorvg_lottie.h>
#include <unordered_map>
#ifdef LOTTIE_OPENGL_RENDERER
#include "tvgGlProgram.h"
#include <lottie/GpuRenderer.hpp>
// Compile the shared scene/asset/transform implementation for a separate GL library.
// CPU LottieCore is still linked only against its CPU-only ThorVG archive.
#define Renderer GpuRenderer
#endif
namespace lottie {
// Conservative serialization for upstream global loader/runtime state.
static std::recursive_mutex &engineMutex() {
    static std::recursive_mutex m;
    return m;
}
static void check(tvg::Result r, const char *op) {
    if (r != tvg::Result::Success)
        throw std::runtime_error(std::string(op) + ": ThorVG result " + std::to_string(int(r)));
}
std::shared_ptr<const Asset> Asset::load(const std::string &path) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Cannot open: " + path);
    auto size = f.tellg();
    if (size <= 0 || size > 64 * 1024 * 1024)
        throw std::runtime_error("Empty or oversized resource (64 MiB limit)");
    auto a = std::make_shared<Asset>();
    a->json.resize(size_t(size));
    f.seekg(0);
    if (!f.read(a->json.data(), size))
        throw std::runtime_error("Cannot read: " + path);
    a->resourcePath =
        std::filesystem::absolute(std::filesystem::u8path(path)).parent_path().u8string() + "/";
    return a;
}
struct Renderer::Impl {
    struct Runtime {
        Runtime() { check(tvg::Initializer::init(0), "init"); }
        ~Runtime() { tvg::Initializer::term(); }
    };
    static Runtime &sharedRuntime() {
        static Runtime runtime;
        return runtime;
    }
    Runtime &runtime;
    std::shared_ptr<const Asset> asset;
    std::unique_ptr<tvg::LottieAnimation> animation;
    std::unique_ptr<tvg::Accessor> accessor{tvg::Accessor::gen()};
    PaletteBindings paletteBindings;
    Palette palette;
    uint32_t paletteSlot = 0;
    struct CachedPalette {
        Palette palette;
        std::string json;
        uint32_t slot;
    };
    std::vector<CachedPalette> paletteCache;
    void retainPalettes(const std::vector<Palette> &live) {
        std::vector<uint32_t> removed;
        paletteCache.erase(
            std::remove_if(paletteCache.begin(), paletteCache.end(),
                           [&](const auto &entry) {
                               if (std::find(live.begin(), live.end(), entry.palette) != live.end())
                                   return false;
                               removed.push_back(entry.slot);
                               return true;
                           }),
            paletteCache.end());
        std::sort(removed.begin(), removed.end());
        removed.erase(std::unique(removed.begin(), removed.end()), removed.end());
        removed.erase(std::remove_if(removed.begin(), removed.end(),
                                     [&](uint32_t id) {
                                         return std::any_of(
                                             paletteCache.begin(), paletteCache.end(),
                                             [&](const auto &entry) { return entry.slot == id; });
                                     }),
                      removed.end());
        if (removed.empty())
            return;
        // Pinned del resets its target properties even for an inactive handle.
        // Reset the active slot first and force reapplication before the next sample.
        check(animation->apply(0), "reset cached palettes");
        paletteSlot = 0;
        palette.enabled = false;
        for (uint32_t id : removed)
            check(animation->del(id), "delete cached palette");
    }
#ifdef LOTTIE_OPENGL_RENDERER
    std::unique_ptr<tvg::GlCanvas> canvas;
    void *context = nullptr;
    uint32_t target = 0;
#else
    std::unique_ptr<tvg::SwCanvas> canvas;
#endif
    std::unordered_map<std::string, std::string> fonts, fontNames;
    std::string fontScope;
    std::string prepareJson() {
        auto document = nlohmann::ordered_json::parse(asset->json);
        if (!document.is_object())
            throw std::runtime_error("Lottie JSON root must be an object");
        std::unordered_map<std::string, std::string> names;
        static uint64_t nextInstance = 0; // protected by engineMutex
        const auto prefix = "lottie-instance-" + std::to_string(++nextInstance) + "-";
        fontScope = prefix;
        if (document.contains("fonts") && document["fonts"].is_object() &&
            document["fonts"].contains("list") && document["fonts"]["list"].is_array()) {
            for (const auto &font : document["fonts"]["list"]) {
                if (font.is_object() && font.contains("fName") && font["fName"].is_string()) {
                    auto original = font["fName"].get<std::string>();
                    auto scoped = prefix + original;
                    names[original] = scoped;
                    fontNames[scoped] = original;
                }
            }
        }
        paletteBindings.prepare(document);
        if (names.empty())
            return document.dump();
        // Rename definitions and text-document references, including nested precomps.
        // ThorVG's font registry is global; private names prevent cross-instance substitution.
        std::vector<nlohmann::ordered_json *> pending{&document};
        while (!pending.empty()) {
            auto *item = pending.back();
            pending.pop_back();
            if (item->is_object())
                for (auto it = item->begin(); it != item->end(); ++it) {
                    if ((it.key() == "fName" || it.key() == "f") && it->is_string()) {
                        auto found = names.find(it->get<std::string>());
                        if (found != names.end())
                            *it = found->second;
                    } else if (it->is_structured())
                        pending.push_back(&it.value());
                }
            else if (item->is_array())
                for (auto &child : *item)
                    if (child.is_structured())
                        pending.push_back(&child);
        }
        return document.dump();
    }
    std::string resourceError;
    bool resolve(tvg::Paint *paint, const char *source) noexcept {
        try {
            std::string src = source ? source : "";
            if (src.find("://") != std::string::npos)
                throw std::runtime_error("Remote assets are unsupported: " + src);
            if (paint->type() == tvg::Type::Text) {
                auto found = fonts.find(src);
                if (found == fonts.end()) {
                    std::filesystem::path path;
                    if (src.rfind("name:", 0) == 0) {
                        auto name = src.substr(5);
                        if (auto original = fontNames.find(name); original != fontNames.end())
                            name = original->second;
                        for (const auto &candidate :
                             {"fonts/" + name + ".ttf", "fonts/" + name + ".otf", name + ".ttf",
                              name + ".otf"}) {
                            auto test = std::filesystem::u8path(asset->resourcePath) /
                                        std::filesystem::u8path(candidate);
                            if (std::filesystem::is_regular_file(test)) {
                                path = test;
                                break;
                            }
                        }
                        if (path.empty())
                            throw std::runtime_error("Font not found: " + name +
                                                     " (provide fonts/<fName>.ttf or .otf)");
                    } else {
                        path = std::filesystem::u8path(src);
                        if (path.is_relative())
                            path = std::filesystem::u8path(asset->resourcePath) / path;
                    }
                    auto bytes = Asset::load(path.u8string())->json;
                    auto alias = fontScope + "resource-" + std::to_string(fonts.size());
                    auto extension = path.extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                                   [](unsigned char c) { return char(std::tolower(c)); });
                    if (extension != ".ttf" && extension != ".otf")
                        throw std::runtime_error("Only TTF/OTF font files are supported: " +
                                                 path.u8string());
                    check(tvg::Text::load(alias.c_str(), bytes.data(), uint32_t(bytes.size()),
                                          extension.c_str() + 1, true),
                          "font load");
                    found = fonts.emplace(src, std::move(alias)).first;
                }
                check(static_cast<tvg::Text *>(paint)->font(found->second.c_str()), "font select");
                return true;
            }
            if (paint->type() == tvg::Type::Picture) {
                auto path = std::filesystem::u8path(src);
                auto bytes = Asset::load(path.u8string())->json;
                auto extension = path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" &&
                    extension != ".webp")
                    throw std::runtime_error("Unsupported image asset: " + src);
                check(static_cast<tvg::Picture *>(paint)->load(bytes.data(), uint32_t(bytes.size()),
                                                               extension.c_str() + 1, nullptr,
                                                               true),
                      "image asset");
                return true;
            }
        } catch (const std::exception &e) {
            resourceError = e.what();
        } catch (...) {
            resourceError = "Resource resolution failed";
        }
        return false;
    }
    Metadata info{};
    Frame frame;
    std::array<float, 6> lastMatrix{};
    tvg::Paint *clipRoot = nullptr; // identity only; reacquire before use
    bool cropApplied = true, cropMasked = false, cropFixed = false;
    std::array<float, 4> clipBounds{}, frameBounds{};
    std::array<float, 6> clipMatrix{};
    Impl(std::shared_ptr<const Asset> a)
        : runtime(sharedRuntime()), asset(std::move(a)), animation(tvg::LottieAnimation::gen()),
          canvas(
#ifdef LOTTIE_OPENGL_RENDERER
              nullptr
#else
              tvg::SwCanvas::gen()
#endif
          ) {
        const uint32_t endian = 1;
        if (*reinterpret_cast<const uint8_t *>(&endian) != 1)
            throw std::runtime_error("Requires little-endian pixels");
        if (!asset || asset->json.empty() || asset->json.size() > 64 * 1024 * 1024 || !animation
#ifndef LOTTIE_OPENGL_RENDERER
            || !canvas
#endif
        )
            throw std::runtime_error("Renderer allocation failed");
        check(animation->picture()->resolver(
                  [](tvg::Paint *paint, const char *src, void *user) {
                      return static_cast<Impl *>(user)->resolve(paint, src);
                  },
                  this),
              "asset resolver");
        const auto normalized = prepareJson();
        check(animation->picture()->load(normalized.data(), uint32_t(normalized.size()), "lottie",
                                         asset->resourcePath.c_str(), true),
              "load");
        check(animation->picture()->size(&info.width, &info.height), "size");
        info.duration = animation->duration();
        info.frames = animation->totalFrame();
        if (!(std::isfinite(info.width) && std::isfinite(info.height) &&
              std::isfinite(info.duration) && std::isfinite(info.frames) && info.width > 0 &&
              info.height > 0 && info.duration > 0 && info.frames > 0))
            throw std::runtime_error("Invalid animation metadata");
    }
    ~Impl() {
        if (canvas) {
            canvas->sync();
            canvas.reset();
        }
        animation.reset();
        for (const auto &entry : fonts)
            tvg::Text::unload(entry.second.c_str());
    }
};
Renderer::Renderer(std::shared_ptr<const Asset> a) {
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    impl = std::make_unique<Impl>(std::move(a));
#ifndef LOTTIE_OPENGL_RENDERER
    // Warm initially visible fonts/assets on the loading thread. Later layers may resolve lazily.
    try {
        render(0, 64, 64);
    } catch (...) {
        impl.reset();
        throw;
    }
#endif
}
void Renderer::setPalette(const Palette &palette) {
    for (const auto &color : palette.colors)
        for (float channel : color)
            if (!std::isfinite(channel) || channel < 0 || channel > 1)
                throw std::runtime_error("Invalid palette color");
    for (float alpha : palette.alpha)
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
            throw std::runtime_error("Invalid palette alpha");
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    auto &i = *impl;
    if (palette == i.palette)
        return;
    if (i.paletteBindings.bindings.empty()) {
        i.palette = palette;
        return;
    }
    if (!palette.enabled) {
        if (i.paletteSlot)
            check(i.animation->apply(0), "reset palette");
        i.paletteSlot = 0;
        i.palette = palette;
        return;
    }
    auto found = std::find_if(i.paletteCache.begin(), i.paletteCache.end(),
                              [&](const auto &entry) { return entry.palette == palette; });
    uint32_t id = 0;
    if (found != i.paletteCache.end())
        id = found->slot;
    else {
        // Player automation also has bounded memory. Emitter prunes to its live
        // (at most 64) palette snapshots before sampling, so this is a fallback.
        if (i.paletteCache.size() >= 64)
            i.retainPalettes({palette});
        auto json = i.paletteBindings.overrides(palette);
        auto same = std::find_if(i.paletteCache.begin(), i.paletteCache.end(),
                                 [&](const auto &entry) { return entry.json == json; });
        if (same != i.paletteCache.end())
            id = same->slot;
        else
            id = i.animation->gen(json.c_str());
        if (!id)
            throw std::runtime_error("Palette slot generation failed");
        i.paletteCache.push_back({palette, std::move(json), id});
    }
    if (id != i.paletteSlot)
        check(i.animation->apply(id), "apply cached palette");
    i.paletteSlot = id;
    i.palette = palette;
}
void Renderer::retainPalettes(const std::vector<Palette> &palettes) {
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    impl->retainPalettes(palettes);
}
Renderer::~Renderer() {
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    impl.reset();
}
Metadata Renderer::metadata() const { return impl->info; }
#ifdef LOTTIE_OPENGL_RENDERER
void Renderer::releaseGL() {
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    if (impl->canvas) {
        impl->canvas->sync();
        impl->canvas.reset();
    }
    impl->frame.width = impl->frame.height = 0;
    impl->context = nullptr;
}
void Renderer::render(void *context, uint32_t target, double seconds, uint32_t w, uint32_t h,
                      const Transform &t) {
#else
const Frame &Renderer::render(double seconds, uint32_t w, uint32_t h, const Transform &t) {
#endif
    std::lock_guard<std::recursive_mutex> lock(engineMutex());
    if (!std::isfinite(seconds) || !w || !h || w > 8192 || h > 8192)
        throw std::runtime_error("Invalid render time or size (max 8192)");
    auto &i = *impl;
    auto &f = i.frame;
    for (float value : {t.x, t.y, t.scale, t.scaleX, t.scaleY, t.rotation, t.anchorX, t.anchorY})
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite transform");
    if (std::abs(t.x) > 32768 || std::abs(t.y) > 32768 || std::abs(t.anchorX) > 32768 ||
        std::abs(t.anchorY) > 32768 || t.scale < 0 || t.scale > 40 || std::abs(t.scaleX) > 40 ||
        std::abs(t.scaleY) > 40 || std::abs(t.rotation) > 3600)
        throw std::runtime_error("Transform outside supported range");
    if (t.fit != Fit::Fit && t.fit != Fit::Fill && t.fit != Fit::Stretch && t.fit != Fit::Original)
        throw std::runtime_error("Invalid fit mode");
#ifdef LOTTIE_OPENGL_RENDERER
    // Pinned 1.0.7 GL backend caches shader bindings across frames, while the host
    // changes them. Its confirmed internal hook invalidates the cache at this boundary.
    GlProgram::unload();
    if (!context)
        throw std::runtime_error("No current host GL context");
    if (i.context && i.context != context)
        throw std::runtime_error("GL context changed without releaseGL");
    if (!i.canvas)
        i.canvas.reset(tvg::GlCanvas::gen());
    if (!i.canvas)
        throw std::runtime_error("ThorVG GL canvas unavailable");
    const bool resized =
        f.width != w || f.height != h || i.target != target || i.context != context;
#else
    const bool resized = f.width != w || f.height != h;
#endif
    if (resized) {
        if (f.width)
            check(i.canvas->remove(), "remove");
#ifndef LOTTIE_OPENGL_RENDERER
        f.pixels.resize(size_t(w) * h);
#endif
        f.width = w;
        f.height = h;
#ifdef LOTTIE_OPENGL_RENDERER
        check(i.canvas->target(nullptr, nullptr, context, int32_t(target), w, h,
                               tvg::ColorSpace::ABGR8888S),
              "GL target");
        i.context = context;
        i.target = target;
#else
        check(i.canvas->target(f.pixels.data(), w, w, h, tvg::ColorSpace::ABGR8888), "target");
#endif
    }
    float bx = w / i.info.width, by = h / i.info.height;
    if (t.fit == Fit::Original)
        bx = by = 1;
    else if (t.fit != Fit::Stretch)
        bx = by = t.fit == Fit::Fill ? std::max(bx, by) : std::min(bx, by);
    const double radians = t.rotation * 3.14159265358979323846 / 180.0;
    const float cs = float(std::cos(radians)), sn = float(std::sin(radians));
    const float sx = t.scale * t.scaleX, sy = t.scale * t.scaleY;
    const float a = cs * sx, b = -sn * sy, c = sn * sx, d = cs * sy;
    const float px = bx * i.info.width * .5f + t.anchorX, py = by * i.info.height * .5f - t.anchorY;
    std::array<float, 6> matrix{a * bx, b * by, w * .5f + t.x - a * px - b * py,
                                c * bx, d * by, h * .5f - t.y - c * px - d * py};
    if (resized || matrix != i.lastMatrix) {
        tvg::Matrix m{matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], 0, 0, 1};
        check(i.animation->picture()->transform(m), "transform");
        i.lastMatrix = matrix;
    }
    if (resized)
        check(i.canvas->add(i.animation->picture()), "add");
    float frame = float(std::clamp(seconds / i.info.duration, 0.0, 1.0) * i.info.frames);
    frame = std::min(frame, std::nextafter(i.info.frames, 0.0f));
    auto r = i.animation->frame(frame);
    if (r != tvg::Result::InsufficientCondition)
        check(r, "frame");
    // Accessor visits Picture, then its root scene. Stop there: never touch
    // authored layer masks or precomposition clips. Slots may rebuild this root,
    // so do not retain a scene/clip pointer across renders.
    tvg::Paint *root = nullptr;
    check(i.accessor->set(
              i.animation->picture(),
              [](const tvg::Paint *paint, void *data) {
                  if (paint->type() == tvg::Type::Scene) {
                      *static_cast<tvg::Paint **>(data) = const_cast<tvg::Paint *>(paint);
                      return false;
                  }
                  return true;
              },
              &root),
          "root accessor");
    if (!root)
        throw std::runtime_error("Lottie root scene unavailable");
    auto pictureMatrix = i.animation->picture()->transform();
    std::array<float, 4> bounds{};
    tvg::Matrix inverse;
    bool invertible = tvg::inverse(&pictureMatrix, &inverse);
    if (invertible)
        for (float v :
             {inverse.e11, inverse.e12, inverse.e13, inverse.e21, inverse.e22, inverse.e23})
            if (!std::isfinite(v))
                invertible = false;
    const bool fixedCrop = t.clipToCanvas && t.fixedCropFrame;
    bool maskedCrop = false;
#ifdef LOTTIE_OPENGL_RENDERER
    maskedCrop = t.clipToCanvas && !fixedCrop && invertible &&
                 (!tvg::rightAngle(pictureMatrix) || tvg::skewed(pictureMatrix));
    if (maskedCrop) {
        // The original rotated clip is already fast when it contains the whole
        // viewport (e.g. a zoomed-in canvas). Preserve that no-composite path.
        bool coversViewport = true;
        for (auto corner : {tvg::Point{0, 0}, tvg::Point{float(w), 0},
                            tvg::Point{float(w), float(h)}, tvg::Point{0, float(h)}}) {
            corner *= inverse;
            if (corner.x <= .001f || corner.y <= .001f || corner.x >= i.info.width - .001f ||
                corner.y >= i.info.height - .001f)
                coversViewport = false;
        }
        if (coversViewport)
            maskedCrop = false;
    }
#endif
    if (maskedCrop) {
        RenderBounds limits{{0, 0, float(w), float(h)}};
        auto box = limits.rectangle(0, 0, i.info.width, i.info.height, pictureMatrix);
        box.left = std::max(0.f, std::floor(box.left - 2));
        box.top = std::max(0.f, std::floor(box.top - 2));
        box.right = std::min(float(w), std::ceil(box.right + 2));
        box.bottom = std::min(float(h), std::ceil(box.bottom + 2));
        bounds = box.empty() ? std::array<float, 4>{0, 0, float(w), float(h)}
                             : std::array<float, 4>{box.left, box.top, box.right, box.bottom};
    }
    // Emitter's stationary frame belongs above the transformed Picture. Keeping
    // it in output coordinates avoids rotated clipping and inverse-rounding drift.
    auto canvasFrame = i.canvas->pImpl->scene;
    if (fixedCrop) {
        const float frameW = bx * i.info.width, frameH = by * i.info.height;
        std::array<float, 4> frameBounds{
            std::max(0.f, (w - frameW) * .5f), std::max(0.f, (h - frameH) * .5f),
            std::min(float(w), (w + frameW) * .5f), std::min(float(h), (h + frameH) * .5f)};
        if (!canvasFrame->clip() || i.frameBounds != frameBounds) {
            auto clip = tvg::Shape::gen();
            check(clip->appendRect(frameBounds[0], frameBounds[1], frameBounds[2] - frameBounds[0],
                                   frameBounds[3] - frameBounds[1]),
                  "fixed frame rectangle");
            check(canvasFrame->clip(clip), "fixed frame clipping");
            check(canvasFrame->transform(canvasFrame->transform()), "invalidate fixed frame");
            i.frameBounds = frameBounds;
        }
    } else if (canvasFrame->clip()) {
        check(canvasFrame->clip(nullptr), "remove fixed frame");
        check(canvasFrame->transform(canvasFrame->transform()), "invalidate fixed frame");
    }
    if (!t.clipToCanvas && invertible)
        bounds = RenderBounds{{0, 0, float(w), float(h)}}.get(root, pictureMatrix);
    bool screenCrop = !t.clipToCanvas || maskedCrop;
    bool needsClip = !fixedCrop && (screenCrop ? invertible : t.clipToCanvas);
    if (i.clipRoot != root || i.cropApplied != t.clipToCanvas || i.cropMasked != maskedCrop ||
        i.cropFixed != fixedCrop ||
        (screenCrop && (i.clipBounds != bounds || i.clipMatrix != matrix)) ||
        bool(root->clip()) != needsClip) {
        tvg::Shape *clip = nullptr;
        if (needsClip) {
            clip = tvg::Shape::gen();
            if (!screenCrop) {
                check(clip->appendRect(0, 0, i.info.width, i.info.height), "canvas rectangle");
            } else {
                check(clip->appendRect(bounds[0], bounds[1], bounds[2] - bounds[0],
                                       bounds[3] - bounds[1]),
                      "content rectangle");
                // Clipper lives below Picture; cancel that transform so the
                // conservative screen-space rectangle stays axis-aligned.
                check(clip->transform(inverse), "content clip transform");
            }
        }
        if (maskedCrop) {
            if (i.clipRoot != root || !i.cropMasked ||
                root->mask(nullptr) == tvg::MaskMethod::None) {
                auto mask = tvg::Shape::gen();
                check(mask->appendRect(0, 0, i.info.width, i.info.height), "crop mask rectangle");
                check(mask->fill(255, 255, 255, 255), "crop mask color");
                check(root->mask(mask, tvg::MaskMethod::Alpha), "crop scene mask");
            }
        } else if (i.cropMasked)
            check(root->mask(nullptr, tvg::MaskMethod::None), "remove crop mask");
        check(root->clip(clip), "canvas clipping");
        tvg::to<SceneImpl>(root)->size(
            !screenCrop && !fixedCrop ? tvg::Point{i.info.width, i.info.height} : tvg::Point{0, 0});
        check(root->transform(root->transform()), "invalidate clipping");
        check(i.animation->picture()->transform(pictureMatrix), "invalidate picture clipping");
        i.clipRoot = root;
        i.cropApplied = t.clipToCanvas;
        i.cropMasked = maskedCrop;
        i.cropFixed = fixedCrop;
        i.clipBounds = bounds;
        i.clipMatrix = matrix;
    }
    check(i.canvas->update(), "update");
    if (maskedCrop) {
        // Keep the intermediate MSAA sample grid aligned to the host target.
        // A tight offscreen origin can create isolated coverage seams in fills.
        auto scene = tvg::to<SceneImpl>(root);
        scene->vport = {{0, 0}, {int32_t(w), int32_t(h)}};
        scene->vdirty = false;
    }
    check(i.canvas->draw(true), "draw");
    check(i.canvas->sync(), "sync");
    if (!i.resourceError.empty())
        throw std::runtime_error(i.resourceError);
#ifndef LOTTIE_OPENGL_RENDERER
    return f;
#endif
}
} // namespace lottie
