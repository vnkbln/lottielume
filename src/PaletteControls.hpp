#pragma once
#include <algorithm>
#include <cmath>
#include <lottie/Core.hpp>
namespace palette_controls {
inline std::array<float, 3> hsv(const std::array<float, 3> &rgb) {
    float high = std::max({rgb[0], rgb[1], rgb[2]}), low = std::min({rgb[0], rgb[1], rgb[2]});
    float delta = high - low, hue = 0;
    if (delta > 0) {
        if (high == rgb[0])
            hue = std::fmod((rgb[1] - rgb[2]) / delta, 6.f);
        else if (high == rgb[1])
            hue = (rgb[2] - rgb[0]) / delta + 2;
        else
            hue = (rgb[0] - rgb[1]) / delta + 4;
        hue /= 6;
        if (hue < 0)
            hue += 1;
    }
    return {hue, high > 0 ? delta / high : 0, high};
}
inline std::array<float, 3> rgb(float h, float s, float v) {
    h = (h - std::floor(h)) * 6;
    float c = v * s, x = c * (1 - std::abs(std::fmod(h, 2.f) - 1)), m = v - c;
    std::array<float, 3> result;
    if (h < 1)
        result = {c, x, 0};
    else if (h < 2)
        result = {x, c, 0};
    else if (h < 3)
        result = {0, c, x};
    else if (h < 4)
        result = {0, x, c};
    else if (h < 5)
        result = {x, 0, c};
    else
        result = {c, 0, x};
    for (auto &channel : result)
        channel += m;
    return result;
}
template <class Set> void setup(unsigned first, Set set) {
    const lottie::Palette defaults;
    set(first, "Use Palette", FF_TYPE_BOOLEAN, 0.f);
    const char *names[] = {"Color 1", "Color 2", "Color 3", "Color 4"};
    const unsigned types[] = {FF_TYPE_HUE, FF_TYPE_SATURATION, FF_TYPE_BRIGHTNESS, FF_TYPE_ALPHA};
    for (unsigned c = 0; c < 4; ++c) {
        auto channels = hsv(defaults.colors[c]);
        for (unsigned k = 0; k < 3; ++k)
            set(first + 1 + c * 4 + k, names[c], types[k], channels[k]);
        set(first + 4 + c * 4, names[c], FF_TYPE_ALPHA, 1.f);
    }
}
template <class Values> lottie::Palette read(const Values &values, unsigned first) {
    const lottie::Palette defaults;
    lottie::Palette p;
    for (unsigned c = 0; c < 4; ++c) {
        unsigned i = first + 1 + c * 4;
        p.colors[c] = rgb(values[i], values[i + 1], values[i + 2]);
        p.alpha[c] = values[i + 3];
        for (unsigned k = 0; k < 3; ++k)
            p.enabled = p.enabled || std::abs(p.colors[c][k] - defaults.colors[c][k]) > 1e-5f;
        p.enabled = p.enabled || std::abs(p.alpha[c] - defaults.alpha[c]) > 1e-5f;
    }
    return p.enabled ? p : defaults;
}
} // namespace palette_controls
