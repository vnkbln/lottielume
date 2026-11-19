#pragma once
#include "tvgPicture.h"
#include "tvgShape.h"
#include <array>
#include <cmath>
#include <limits>

namespace lottie {
// Conservative current-frame bounds in output coordinates, before GPU preparation.
// Do not use prepared stroke bounds from a previous frame. Unknown/effect-expanded
// content falls back to the full output rather than risk cutting pixels.
struct RenderBounds {
    struct Box {
        float left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
        bool empty() const { return right <= left || bottom <= top; }
        void add(const Box &b) {
            if (b.empty())
                return;
            left = std::min(left, b.left);
            top = std::min(top, b.top);
            right = std::max(right, b.right);
            bottom = std::max(bottom, b.bottom);
        }
        void intersect(const Box &b) {
            left = std::max(left, b.left);
            top = std::max(top, b.top);
            right = std::min(right, b.right);
            bottom = std::min(bottom, b.bottom);
        }
    };
    Box output;
    Box rectangle(float x, float y, float w, float h, const tvg::Matrix &matrix) const {
        Box b;
        for (auto p : {tvg::Point{x, y}, tvg::Point{x + w, y}, tvg::Point{x + w, y + h},
                       tvg::Point{x, y + h}}) {
            p *= matrix;
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return output;
            b.left = std::min(b.left, p.x);
            b.top = std::min(b.top, p.y);
            b.right = std::max(b.right, p.x);
            b.bottom = std::max(b.bottom, p.y);
        }
        return b;
    }
    Box shape(tvg::Shape *paint, const tvg::Matrix &matrix) const {
        auto &geometry = tvg::to<tvg::ShapeImpl>(paint)->rs;
        tvg::BBox bounds = {{FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX}};
        if (!geometry.path.bounds(&matrix, bounds))
            return {};
        // Frobenius norm bounds any axis stretch, and the miter limit bounds
        // stroke joins. This deliberately overestimates bevel/round strokes.
        float norm = std::sqrt(matrix.e11 * matrix.e11 + matrix.e12 * matrix.e12 +
                               matrix.e21 * matrix.e21 + matrix.e22 * matrix.e22);
        float pad = geometry.strokeWidth() * std::max(1.f, geometry.strokeMiterlimit()) * norm;
        Box b{bounds.min.x - pad, bounds.min.y - pad, bounds.max.x + pad, bounds.max.y + pad};
        if (!std::isfinite(b.left) || !std::isfinite(b.top) || !std::isfinite(b.right) ||
            !std::isfinite(b.bottom))
            return output;
        return b;
    }
    Box visit(tvg::Paint *paint, const tvg::Matrix &parent, bool root = false) const {
        if (!paint->opacity())
            return {};
        auto matrix = parent * paint->transform();
        Box b;
        if (paint->type() == tvg::Type::Scene) {
            auto scene = tvg::to<SceneImpl>(paint);
            if (scene->effects && scene->effects->count)
                return output;
            for (auto child : scene->paints)
                b.add(visit(child, matrix));
        } else if (paint->type() == tvg::Type::Shape) {
            b = shape(static_cast<tvg::Shape *>(paint), matrix);
        } else if (paint->type() == tvg::Type::Picture) {
            auto picture = tvg::to<tvg::PictureImpl>(paint);
            if (picture->vector || picture->w <= 0 || picture->h <= 0)
                return output;
            b = rectangle(-picture->origin.x * picture->w, -picture->origin.y * picture->h,
                          picture->w, picture->h, matrix);
        } else
            return output;
        // Clipper transforms use the parent's matrix, not the paint's own.
        // Ignore our previous root rectangle to avoid feedback between frames.
        if (!root && paint->clip())
            b.intersect(shape(paint->clip(), parent * paint->clip()->transform()));
        return b;
    }
    std::array<float, 4> get(tvg::Paint *root, const tvg::Matrix &parent) const {
        auto b = visit(root, parent, true);
        // Keep AA/filter coverage. Empty/unknown bounds use the safe full target.
        if (b.empty())
            return {output.left, output.top, output.right, output.bottom};
        b.left = std::floor(b.left - 2);
        b.top = std::floor(b.top - 2);
        b.right = std::ceil(b.right + 2);
        b.bottom = std::ceil(b.bottom + 2);
        b.intersect(output);
        if (b.empty())
            return {output.left, output.top, output.right, output.bottom};
        return {b.left, b.top, b.right, b.bottom};
    }
};
} // namespace lottie
