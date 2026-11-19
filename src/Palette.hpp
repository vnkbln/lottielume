#pragma once
#include <algorithm>
#include <cmath>
#include <lottie/Core.hpp>
#include <nlohmann/json.hpp>

namespace lottie {
// Private slot preparation: source bytes stay untouched. Only typed color properties
// are inspected; gradient offsets and trailing opacity stops are never remapped.
struct PaletteBindings {
    using Json = nlohmann::ordered_json;
    struct Binding {
        std::string id;
        Json property;
        bool gradient;
        int stops;
        int opacityColor = -1;
    };
    std::vector<Binding> bindings;
    static int match(const Json &values, size_t offset) {
        if (!values.is_array() || values.size() < offset + 3)
            return -1;
        const Palette base;
        for (int c = 0; c < 4; ++c) {
            bool equal = true;
            for (int k = 0; k < 3; ++k)
                if (!values[offset + k].is_number() ||
                    std::abs(values[offset + k].get<double>() - base.colors[c][k]) > 0.0001)
                    equal = false;
            if (equal)
                return c;
        }
        return -1;
    }
    static bool remapArray(Json &values, int stops, const Palette &palette) {
        bool found = false;
        if (!values.is_array())
            return false;
        const auto original = values;
        std::vector<std::pair<float, float>> factors, opacity;
        bool changeAlpha = false;
        for (int n = 0; n < stops; ++n) {
            size_t offset = size_t(n) * 4 + 1;
            if (values.size() < offset + 3)
                return found;
            int role = match(original, offset);
            float factor = role >= 0 ? palette.alpha[role] : 1.f;
            factors.emplace_back(original[offset - 1].get<float>(), factor);
            changeAlpha = changeAlpha || factor != 1;
            if (int color = match(values, offset); color >= 0) {
                for (int k = 0; k < 3; ++k)
                    values[offset + k] = palette.colors[color][k];
                found = true;
            }
        }
        if (changeAlpha) {
            for (size_t n = size_t(stops) * 4; n + 1 < original.size(); n += 2)
                opacity.emplace_back(original[n].get<float>(), original[n + 1].get<float>());
            if (opacity.empty())
                opacity = {{0, 1}, {1, 1}};
            auto interpolate = [](const auto &points, float x) {
                if (x <= points.front().first)
                    return points.front().second;
                for (size_t n = 1; n < points.size(); ++n)
                    if (x <= points[n].first) {
                        float span = points[n].first - points[n - 1].first;
                        float t = span > 0 ? (x - points[n - 1].first) / span : 1;
                        return points[n - 1].second * (1 - t) + points[n].second * t;
                    }
                return points.back().second;
            };
            std::vector<float> positions;
            for (auto stop : factors)
                positions.push_back(stop.first);
            for (auto stop : opacity)
                positions.push_back(stop.first);
            std::sort(positions.begin(), positions.end());
            positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
            values.erase(values.begin() + stops * 4, values.end());
            for (float x : positions) {
                values.push_back(x);
                values.push_back(interpolate(opacity, x) * interpolate(factors, x));
            }
        }
        return found;
    }
    static void scaleOpacity(Json &prop, float factor) {
        auto &value = prop["k"];
        if (prop.value("a", 0) == 0 && value.is_number())
            value = value.get<float>() * factor;
        else if (value.is_array())
            for (auto &key : value)
                if (key.is_object())
                    for (const char *field : {"s", "e"})
                        if (key.contains(field) && key[field].is_array())
                            for (auto &v : key[field])
                                if (v.is_number())
                                    v = v.get<float>() * factor;
    }
    static bool remap(Json &property, bool gradient, int stops, const Palette &palette) {
        Json &p = gradient ? property["k"] : property;
        if (!p.is_object() || p.contains("x") || !p.contains("k"))
            return false;
        auto array = [&](Json &values) {
            if (gradient)
                return remapArray(values, stops, palette);
            int c = match(values, 0);
            if (c < 0)
                return false;
            for (int k = 0; k < 3; ++k)
                values[k] = palette.colors[c][k];
            return true;
        };
        if (p.value("a", 0) == 0)
            return array(p["k"]);
        // Keep gradient keyframes, stop positions, alpha and easing; only map RGB.
        if (!gradient || !p["k"].is_array())
            return false;
        bool found = false;
        for (auto &key : p["k"])
            if (key.is_object())
                for (const char *field : {"s", "e"})
                    if (key.contains(field))
                        found = array(key[field]) || found;
        return found;
    }
    void prepare(Json &document) {
        // Use IDs that do not collide with any original slot or sid string.
        const auto original = document.dump();
        std::string prefix = "__resolume_palette_";
        while (original.find(prefix) != std::string::npos)
            prefix += "_";
        std::vector<Json *> pending{&document};
        while (!pending.empty()) {
            auto &item = *pending.back();
            pending.pop_back();
            if (item.is_object()) {
                const auto ty = item.value("ty", Json());
                // AE layer Fill is effect 21; its color and opacity are
                // properties 2 and 6 in the Bodymovin effect parameter array.
                if (ty == 21 && item.contains("ef") && item["ef"].is_array() &&
                    item["ef"].size() >= 7 && item["ef"][2].is_object() &&
                    item["ef"][6].is_object() && item["ef"][2].contains("v") &&
                    item["ef"][6].contains("v")) {
                    auto &color = item["ef"][2]["v"];
                    auto &opacity = item["ef"][6]["v"];
                    auto test = color;
                    if (color.is_object() && !color.contains("sid") &&
                        remap(test, false, 0, Palette{})) {
                        int role = match(color["k"], 0);
                        auto id = prefix + std::to_string(bindings.size());
                        bindings.push_back({id, color, false, 0});
                        color["sid"] = id;
                        if (role >= 0 && opacity.is_object() && !opacity.contains("sid") &&
                            !opacity.contains("x")) {
                            auto opacityId = prefix + std::to_string(bindings.size());
                            bindings.push_back({opacityId, opacity, false, 0, role});
                            opacity["sid"] = opacityId;
                        }
                    }
                }
                bool gradient = ty == "gf" || ty == "gs";
                const char *field = gradient ? "g" : "c";
                if ((gradient || ty == "fl" || ty == "st") && item.contains(field) &&
                    item[field].is_object() && !item[field].contains("sid") &&
                    !(item[field].contains("k") && item[field]["k"].is_object() &&
                      item[field]["k"].contains("sid"))) {
                    int stops = gradient ? item[field].value("p", 0) : 0;
                    auto test = item[field];
                    if ((!gradient || (stops > 0 && stops <= 4096)) &&
                        remap(test, gradient, stops, Palette{})) {
                        auto id = prefix + std::to_string(bindings.size());
                        bindings.push_back({id, item[field], gradient, stops});
                        item[field]["sid"] = id;
                        // Palette alpha multiplies author opacity, including its keyframes.
                        if (!gradient) {
                            int role = match(item[field]["k"], 0);
                            if (!item.contains("o"))
                                item["o"] = {{"a", 0}, {"k", 100}};
                            if (role >= 0 && item["o"].is_object() && !item["o"].contains("sid") &&
                                !item["o"].contains("x")) {
                                auto opacityId = prefix + std::to_string(bindings.size());
                                bindings.push_back({opacityId, item["o"], false, 0, role});
                                item["o"]["sid"] = opacityId;
                            }
                        }
                    }
                }
                for (auto &child : item)
                    if (child.is_structured())
                        pending.push_back(&child);
            } else if (item.is_array())
                for (auto &child : item)
                    if (child.is_structured())
                        pending.push_back(&child);
        }
    }
    std::string overrides(const Palette &palette) const {
        Json result = Json::object();
        for (const auto &binding : bindings) {
            auto prop = binding.property;
            if (binding.opacityColor >= 0)
                scaleOpacity(prop, palette.alpha[binding.opacityColor]);
            else
                remap(prop, binding.gradient, binding.stops, palette);
            result[binding.id] = {{"p", std::move(prop)}};
        }
        return result.dump();
    }
};
} // namespace lottie
