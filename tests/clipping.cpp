#include <iostream>
#include <lottie/Core.hpp>
#include <stdexcept>
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        lottie::Renderer renderer(lottie::Asset::load(argv[1]));
        lottie::Transform t;
        t.fit = lottie::Fit::Original;
        auto alpha = [](const lottie::Frame &f, int x, int y) {
            return f.pixels[y * f.width + x] >> 24;
        };
        for (int n = 0; n < 3; ++n) {
            t.clipToCanvas = true;
            auto clipped = renderer.render(.5, 320, 180, t).pixels;
            check((clipped[60 * 320 + 230] >> 24) == 0, "root canvas clips overflow by default");
            t.clipToCanvas = false;
            const auto &open = renderer.render(.5, 320, 180, t);
            check(alpha(open, 230, 60) > 240, "root overflow visible");
            check(alpha(open, 230, 90) == 0, "authored mask still clips overflow");
            check(alpha(open, 230, 120) == 0 && alpha(open, 205, 120) > 240,
                  "precomp viewport preserved");
            lottie::Palette p;
            p.enabled = n != 1;
            p.colors[0] = {1, 0, 0};
            renderer.setPalette(p);
            check(alpha(renderer.render(.5, 320, 180, t), 230, 60) > 240,
                  "root overflow after slot rebuild");
        }
        t.clipToCanvas = true;
        check(alpha(renderer.render(.5, 320, 180, t), 230, 60) == 0, "clipping restored");
        t.clipToCanvas = false;
        t.x = 35;
        t.y = -15;
        check(alpha(renderer.render(.5, 320, 180, t), 265, 75) > 240,
              "translated overflow bounds update");
        t.x = 0;
        t.y = 0;
        t.rotation = 90;
        check(alpha(renderer.render(.5, 320, 180, t), 190, 160) > 240,
              "rotated overflow bounds update");
        t.rotation = 0;
        t.scale = 0;
        check(alpha(renderer.render(.5, 320, 180, t), 230, 60) == 0,
              "singular transform remains transparent");
        t.scale = 1;
        check(alpha(renderer.render(.5, 320, 180, t), 230, 60) > 240,
              "bounds recover after zero scale");
        t.fixedCropFrame = true;
        t.clipToCanvas = true;
        t.rotation = 35;
        check(alpha(renderer.render(.5, 320, 180, t), 208, 93) > 200 &&
                  alpha(renderer.render(.5, 320, 180, t), 214, 91) == 0,
              "fixed frame clips rotated content");
        t.rotation = 0;
        t.x = -150;
        check(alpha(renderer.render(.5, 320, 180, t), 55, 60) == 0, "fixed frame ignores Position");
        t.clipToCanvas = false;
        check(alpha(renderer.render(.5, 320, 180, t), 55, 60) > 200, "fixed frame disabled");
        t.x = 0;
        t.scale = 2;
        t.clipToCanvas = true;
        check(alpha(renderer.render(.5, 320, 180, t), 250, 30) == 0,
              "Scale changes content not frame");
        t.clipToCanvas = false;
        check(alpha(renderer.render(.5, 320, 180, t), 250, 30) > 200,
              "zoomed content outside frame recoverable");
        std::cout << "Root clipping toggle/mask/precomp/slot rebuild passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
