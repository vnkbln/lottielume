#pragma once
#include <lottie/Core.hpp>

namespace embedded_demo {
inline std::shared_ptr<const lottie::Asset> asset() {
    static const auto value = [] {
        auto result = std::make_shared<lottie::Asset>();
        static const unsigned char bytes[] = {
#include "LottieLogo.json.inc"
        };
        result->json.assign(reinterpret_cast<const char *>(bytes), sizeof(bytes));
        return result;
    }();
    return value;
}
inline std::shared_ptr<const lottie::Asset> load(const std::string &path) {
    return path.empty() ? asset() : lottie::Asset::load(path);
}
} // namespace embedded_demo
