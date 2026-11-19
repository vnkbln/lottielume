#include <filesystem>
#include <iostream>
#include <lottie/Core.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
using Json = nlohmann::ordered_json;
void check(bool b, const char *message) {
    if (!b)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        auto asset = lottie::Asset::load(argv[1]);
        lottie::Renderer renderer(asset);
        auto original = renderer.render(.5, 320, 180).pixels;
        lottie::Palette palette;
        palette.enabled = true;
        palette.colors = {{{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}, {{1, 1, 0}}}};
        auto doc = Json::parse(asset->json);
        doc["layers"][0]["shapes"][1]["c"]["k"] = {1, 0, 0, 1};
        doc["layers"][0]["shapes"][2]["c"]["k"] = {1, 1, 0, 1};
        doc["layers"][1]["shapes"][1]["g"]["k"]["k"] = {0, 0, 1, 0, 1, 0, 0, 1, 0, .5, 1, .5};
        auto reference = std::make_shared<lottie::Asset>();
        reference->json = doc.dump();
        lottie::Renderer expected(reference);
        auto expectedPixels = expected.render(.5, 320, 180).pixels;
        for (int i = 0; i < 20; ++i) {
            renderer.setPalette(palette);
            check(renderer.render(.5, 320, 180).pixels == expectedPixels,
                  "fill/stroke/gradient slots match directly authored colors and opacity");
            renderer.setPalette(palette);
            check(renderer.render(.5, 320, 180).pixels == expectedPixels, "unchanged palette");
            auto off = palette;
            off.enabled = false;
            renderer.setPalette(off);
            check(renderer.render(.5, 320, 180).pixels == original, "original palette restored");
        }
        auto alphaDoc = doc;
        alphaDoc["layers"][0]["shapes"][1]["o"]["k"] = 50;
        alphaDoc["layers"][0]["shapes"][2]["o"]["k"] = 25;
        alphaDoc["layers"][1]["shapes"][1]["g"]["k"]["k"] = {0, 0, 1, 0,   1, 0,
                                                             0, 1, 0, .25, 1, .25};
        auto alphaAsset = std::make_shared<lottie::Asset>();
        alphaAsset->json = alphaDoc.dump();
        lottie::Renderer alphaExpected(alphaAsset);
        palette.alpha = {.5f, .5f, .5f, .25f};
        renderer.setPalette(palette);
        check(renderer.render(.5, 320, 180).pixels == alphaExpected.render(.5, 320, 180).pixels,
              "palette alpha multiplies fill stroke and original gradient opacity");
        palette.alpha = {1, 1, 1, 1};
        lottie::Palette second = palette;
        second.colors[0] = {0, 0, 1};
        for (int n = 0; n < 8; ++n) {
            renderer.retainPalettes({palette, second});
            renderer.setPalette(palette);
            check(renderer.render(.5, 320, 180).pixels == expectedPixels,
                  "cached original slot reused");
            renderer.setPalette(second);
            check((renderer.render(.5, 320, 180).pixels[90 * 320 + 80] & 0xff) == 0,
                  "cached alternate slot");
        }
        renderer.retainPalettes({palette});
        renderer.setPalette(palette);
        check(renderer.render(.5, 320, 180).pixels == expectedPixels,
              "pruning restores retained slot");
        renderer.retainPalettes({});
        renderer.setPalette(palette);
        check(renderer.render(.5, 320, 180).pixels == expectedPixels,
              "palette regenerated after clear");
        // Different UI palettes can produce identical overrides when an unused
        // color changes. Their shared slot must survive pruning either alias.
        auto onlyFirst = Json::parse(asset->json);
        onlyFirst["layers"].erase(onlyFirst["layers"].begin() + 1);
        onlyFirst["layers"][0]["shapes"].erase(onlyFirst["layers"][0]["shapes"].begin() + 2);
        auto aliasAsset = std::make_shared<lottie::Asset>();
        aliasAsset->json = onlyFirst.dump();
        lottie::Renderer aliases(aliasAsset);
        auto alias = palette;
        alias.colors[2] = {1, 1, 1};
        aliases.setPalette(palette);
        auto aliasPixels = aliases.render(.5, 320, 180).pixels;
        aliases.setPalette(alias);
        aliases.retainPalettes({alias});
        aliases.setPalette(alias);
        check(aliases.render(.5, 320, 180).pixels == aliasPixels,
              "shared slot survives alias pruning");
        aliases.retainPalettes({});
        for (int n = 0; n < 80; ++n) {
            auto changing = palette;
            changing.colors[0][1] = float(n) / 100;
            aliases.setPalette(changing);
            aliases.render(.5, 320, 180);
        }
        aliases.setPalette(palette);
        check(aliases.render(.5, 320, 180).pixels == aliasPixels,
              "bounded cache eviction restores colors");
        // Animated gradient stops are remapped without changing timing or opacity.
        auto animate = [](Json &document, const Json &endColors) {
            auto &property = document["layers"][1]["shapes"][1]["g"]["k"];
            auto start = property["k"];
            property = {{"a", 1},
                        {"k", Json::array({{{"t", 0},
                                            {"s", start},
                                            {"e", endColors},
                                            {"o", {{"x", .33}, {"y", .33}}},
                                            {"i", {{"x", .67}, {"y", .67}}}},
                                           {{"t", 60}, {"s", endColors}}})}};
        };
        auto animatedDoc = Json::parse(asset->json);
        const auto base = lottie::Palette{};
        Json endColors = {0,
                          base.colors[0][0],
                          base.colors[0][1],
                          base.colors[0][2],
                          1,
                          base.colors[3][0],
                          base.colors[3][1],
                          base.colors[3][2],
                          0,
                          .5,
                          1,
                          .5};
        animate(animatedDoc, endColors);
        animate(doc, Json::array({0, 1, 0, 0, 1, 1, 1, 0, 0, .5, 1, .5}));
        auto animatedAsset = std::make_shared<lottie::Asset>();
        animatedAsset->json = animatedDoc.dump();
        reference = std::make_shared<lottie::Asset>();
        reference->json = doc.dump();
        lottie::Renderer animated(animatedAsset), animatedExpected(reference);
        animated.setPalette(palette);
        for (double time : {.3, 1.2, 1.9, 0.}) {
            check(animated.render(time, 320, 180).pixels ==
                      animatedExpected.render(time, 320, 180).pixels,
                  "animated gradient palette preserves keyframes/easing/opacity");
        }
        auto alternateDoc = doc;
        alternateDoc["layers"][0]["shapes"][1]["c"]["k"] = {0, 0, 1, 1};
        auto &keys = alternateDoc["layers"][1]["shapes"][1]["g"]["k"]["k"];
        for (auto *values : {&keys[0]["e"], &keys[1]["s"]}) {
            (*values)[1] = 0;
            (*values)[2] = 0;
            (*values)[3] = 1;
        }
        auto alternateAsset = std::make_shared<lottie::Asset>();
        alternateAsset->json = alternateDoc.dump();
        lottie::Renderer alternateExpected(alternateAsset);
        auto alternatePalette = palette;
        alternatePalette.colors[0] = {0, 0, 1};
        for (double time : {.3, 1.2, 1.9, 0.}) {
            animated.retainPalettes({palette, alternatePalette});
            animated.setPalette(alternatePalette);
            check(animated.render(time, 320, 180).pixels ==
                      alternateExpected.render(time, 320, 180).pixels,
                  "cached alternate animated gradient");
            animated.setPalette(palette);
            check(animated.render(time, 320, 180).pixels ==
                      animatedExpected.render(time, 320, 180).pixels,
                  "cached original animated gradient");
        }
        lottie::Palette off;
        animated.setPalette(off);
        lottie::Renderer animatedOriginal(animatedAsset);
        check(animated.render(.7, 320, 180).pixels == animatedOriginal.render(.7, 320, 180).pixels,
              "animated original gradient restored");
        check(Json::parse(asset->json)["layers"][0]["shapes"][1]["c"]["k"][0] != 1,
              "immutable source");
        auto fillAsset = lottie::Asset::load(
            (std::filesystem::u8path(argv[1]).parent_path() / "fill-effect.json").u8string());
        auto fillDoc = Json::parse(fillAsset->json);
        // Keep authored opacity keyframes; palette alpha must multiply them.
        auto &effectOpacity = fillDoc["layers"][0]["ef"][0]["ef"][6]["v"];
        effectOpacity = {{"a", 1},
                         {"k", Json::array({{{"t", 0},
                                             {"s", {.2}},
                                             {"e", {1.}},
                                             {"i", {{"x", {1.}}, {"y", {1.}}}},
                                             {"o", {{"x", {0.}}, {"y", {0.}}}}},
                                            {{"t", 60}, {"s", {1.}}}})}};
        auto animatedFillAsset = std::make_shared<lottie::Asset>();
        animatedFillAsset->json = fillDoc.dump();
        lottie::Renderer fillRenderer(animatedFillAsset);
        auto expectedFillDoc = fillDoc;
        expectedFillDoc["layers"][0]["ef"][0]["ef"][2]["v"]["k"] = {0, 1, 0, 1};
        auto &expectedOpacity = expectedFillDoc["layers"][0]["ef"][0]["ef"][6]["v"]["k"];
        expectedOpacity[0]["s"] = {.1};
        expectedOpacity[0]["e"] = {.5};
        expectedOpacity[1]["s"] = {.5};
        auto expectedFillAsset = std::make_shared<lottie::Asset>();
        expectedFillAsset->json = expectedFillDoc.dump();
        lottie::Renderer expectedFill(expectedFillAsset);
        auto effectPalette = lottie::Palette{};
        effectPalette.enabled = true;
        effectPalette.colors[0] = {0, 1, 0};
        effectPalette.alpha[0] = .5f;
        for (int n = 0; n < 4; ++n) {
            for (double time : {0., .5, 1.5}) {
                auto before = fillRenderer.render(time, 320, 180).pixels;
                check((before[90 * 320 + 80] >> 24) > 0, "Fill fixture is visible");
                fillRenderer.setPalette(effectPalette);
                auto actual = fillRenderer.render(time, 320, 180).pixels;
                check(actual == expectedFill.render(time, 320, 180).pixels,
                      "layer Fill recolor and animated opacity match authored reference");
                check(actual != before, "Fill palette visibly changes pixels");
                fillRenderer.setPalette(lottie::Palette{});
                check(fillRenderer.render(time, 320, 180).pixels == before,
                      "Fill original color and animated opacity restored");
            }
        }
        // The shipped demo must use the actual palette RGBs, not visually
        // similar authoring/export shades that cannot bind to color slots.
        check(argc > 2, "demo path required");
        lottie::Renderer demo(lottie::Asset::load(argv[2]));
        lottie::Palette black;
        black.enabled = true;
        for (auto &color : black.colors)
            color = {0, 0, 0};
        for (double time : {1., 2.5, 4.}) {
            demo.setPalette(lottie::Palette{});
            const auto originalDemo = demo.render(time, 320, 320).pixels;
            bool colored = false;
            for (auto pixel : originalDemo)
                colored = colored || (pixel & 0x00ffffffu) != 0;
            check(colored, "demo is visible before recolor");
            demo.setPalette(black);
            const auto recolored = demo.render(time, 320, 320).pixels;
            for (size_t i = 0; i < recolored.size(); ++i) {
                check((recolored[i] & 0x00ffffffu) == 0, "all demo colors bind to palette slots");
                check((recolored[i] >> 24) == (originalDemo[i] >> 24),
                      "demo recolor preserves authored alpha");
            }
        }
        std::cout << "Palette fills/strokes/gradients/opacity/reset/demo passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
