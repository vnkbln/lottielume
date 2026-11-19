#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <lottie/Core.hpp>
#include <stdexcept>
void require(bool ok, const char *what) {
    if (!ok)
        throw std::runtime_error(what);
}
int main(int argc, char **argv) {
    try {
        const auto root = std::filesystem::u8path(argv[1]);
        auto load = [&](const char *name) {
            return std::make_unique<lottie::Renderer>(
                lottie::Asset::load((root / name).u8string()));
        };
        for (const auto &ext : {"png", "jpg", "webp"}) {
            auto external = load((std::string("image-") + ext + "-external.json").c_str());
            auto embedded = load((std::string("image-") + ext + "-embedded.json").c_str());
            auto frame = external->render(0, 320, 180).pixels;
            require(frame == embedded->render(0, 320, 180).pixels,
                    "image external/embedded mismatch");
            auto top = frame[75 * 320 + 160], bottom = frame[105 * 320 + 160];
            bool jpeg = std::string(ext) == "jpg";
            require((top & 255) > (jpeg ? 240 : 120) && ((top >> 16) & 255) < 5,
                    "image red channel");
            require(((bottom >> 16) & 255) > 240 && (bottom & 255) < 5,
                    "image blue channel / vertical orientation");
            require((top >> 24) >= (jpeg ? 255 : 126) && (top >> 24) <= (jpeg ? 255 : 128),
                    "image alpha");
            require(frame[0] == 0, "image transparent composition border");
        }
        {
            auto unicode = load("image-unicode.json"), ascii = load("image-png-external.json");
            require(unicode->render(0, 320, 180).pixels == ascii->render(0, 320, 180).pixels,
                    "Unicode image path");
        }
        for (const auto &ext : {"ttf", "otf"}) {
            auto external = load((std::string("font-") + ext + "-external.json").c_str());
            auto embedded = load((std::string("font-") + ext + "-embedded.json").c_str());
            auto frame = external->render(0, 320, 180).pixels;
            require(frame == embedded->render(0, 320, 180).pixels,
                    "font external/embedded mismatch");
            require(std::count_if(
                        frame.begin(), frame.end(),
                        [](uint32_t p) { return (p >> 24) > 200 && ((p >> 8) & 255) > 200; }) > 400,
                    "font glyphs absent");
            embedded.reset();
            require(frame == external->render(1, 320, 180).pixels, "font independent lifetime");
        }
        {
            auto original = load("font-ttf-embedded.json");
            auto expected = original->render(0, 320, 180).pixels;
            auto alternate = load("font-same-name-alternate.json");
            require(expected != alternate->render(0, 320, 180).pixels,
                    "same-name font conflict was not isolated");
            require(expected == original->render(1, 320, 180).pixels,
                    "other instance changed existing font");
            alternate.reset();
            require(expected == original->render(1, 320, 180).pixels,
                    "other font destruction changed instance");
            auto named = load("font-name-lookup.json");
            require(named->render(0, 320, 180).pixels == expected, "sidecar font name lookup");
        }
        auto position = load("expression-position.json");
        require((position->render(0, 320, 180).pixels[90 * 320 + 60] >> 24) > 0,
                "expression position start");
        auto changed = position->render(1, 320, 180).pixels;
        require((changed[90 * 320 + 160] >> 24) > 0 && (changed[90 * 320 + 60] >> 24) == 0,
                "time expression not evaluated");
        require(changed == position->render(1, 320, 180).pixels,
                "expression fixed time determinism");
        auto opacity = load("expression-opacity.json");
        require(std::abs(int(opacity->render(0, 320, 180).pixels[90 * 320 + 160] >> 24) - 51) <= 1,
                "expression opacity start");
        require(std::abs(int(opacity->render(1, 320, 180).pixels[90 * 320 + 160] >> 24) - 153) <= 1,
                "expression opacity time");
        require(changed == position->render(1, 320, 180).pixels, "independent expressions context");
        auto invalid = load("expression-invalid.json");
        require((invalid->render(1, 320, 180).pixels[90 * 320 + 160] >> 24) == 255,
                "invalid expression fallback");
        auto broken = std::make_shared<lottie::Asset>(
            *lottie::Asset::load((root / "image-png-external.json").u8string()));
        broken->resourcePath = (root / "does-not-exist").u8string();
        bool failed = false;
        try {
            lottie::Renderer r(broken);
        } catch (const std::exception &) {
            failed = true;
        }
        require(failed, "missing image was not reported");
        broken = std::make_shared<lottie::Asset>(
            *lottie::Asset::load((root / "font-ttf-external.json").u8string()));
        broken->resourcePath = (root / "does-not-exist").u8string();
        failed = false;
        try {
            lottie::Renderer r(broken);
        } catch (const std::exception &) {
            failed = true;
        }
        require(failed, "missing font was not reported");
        std::cout << "Images PNG/JPEG/WebP, TTF/OTF fonts, expressions, missing resources passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
