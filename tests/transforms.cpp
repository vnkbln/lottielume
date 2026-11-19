#include <cmath>
#include <iostream>
#include <limits>
#include <lottie/Core.hpp>
#include <stdexcept>
void require(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
int main(int argc, char **argv) {
    try {
        lottie::Renderer r(lottie::Asset::load(argv[1]));
        lottie::Transform t;
        auto alpha = [](const lottie::Frame &f, int x, int y) {
            return f.pixels[y * f.width + x] >> 24;
        };
        auto baseline = r.render(1, 320, 180).pixels;
        require(baseline == r.render(1, 320, 180, t).pixels, "identity");
        t.x = 60;
        t.y = 30;
        auto moved = r.render(1, 320, 180, t);
        require(alpha(moved, 220, 60) > 0 && alpha(moved, 160, 90) == 0,
                "pixel translation / Y up");
        t = {};
        t.scale = 2;
        auto zoom = r.render(1, 320, 180, t);
        require(alpha(zoom, 210, 90) > 0 && alpha(zoom, 225, 90) == 0, "200% vector scale");
        t = {};
        t.scaleX = 2;
        t.scaleY = .5;
        auto axes = r.render(1, 320, 180, t);
        require(alpha(axes, 210, 90) > 0 && alpha(axes, 160, 110) == 0, "axis scale");
        t.rotation = 90;
        auto rotate = r.render(1, 320, 180, t);
        require(alpha(rotate, 160, 140) > 0 && alpha(rotate, 210, 90) == 0, "rotation");
        t = {};
        t.anchorX = 40;
        t.scale = 2;
        auto anchor = r.render(1, 320, 180, t);
        require(alpha(anchor, 80, 90) > 0 && alpha(anchor, 160, 90) == 0, "anchor placement");
        t = {};
        t.scale = 0;
        auto hidden = r.render(1, 320, 180, t);
        require(alpha(hidden, 160, 90) == 0, "zero scale");
        t = {};
        t.scaleX = -1;
        auto mirror = r.render(0, 320, 180, t);
        require(alpha(mirror, 260, 90) > 0 && alpha(mirror, 60, 90) == 0, "negative scale mirror");
        t = {};
        auto fit = r.render(1, 320, 320, t);
        require(alpha(fit, 160, 160) > 0 && alpha(fit, 160, 200) == 0, "fit");
        t.fit = lottie::Fit::Fill;
        auto fill = r.render(1, 320, 320, t);
        require(alpha(fill, 160, 200) > 0, "fill");
        t.fit = lottie::Fit::Stretch;
        auto stretch = r.render(1, 320, 320, t);
        require(alpha(stretch, 160, 200) > 0 && alpha(stretch, 200, 160) == 0, "stretch");
        t = {};
        t.fit = lottie::Fit::Original;
        auto original = r.render(1, 640, 360, t);
        require(alpha(original, 344, 180) > 0 && alpha(original, 360, 180) == 0,
                "original 60px shape on larger centered canvas");
        auto small = r.render(1, 160, 90, t);
        require(alpha(small, 104, 45) > 0 && alpha(small, 120, 45) == 0,
                "original keeps size on smaller output");
        t.scale = 2;
        auto originalZoom = r.render(1, 640, 360, t);
        require(alpha(originalZoom, 370, 180) > 0 && alpha(originalZoom, 390, 180) == 0,
                "original supports explicit scale");
        t = {};
        require(baseline == r.render(1, 320, 180, t).pixels, "reset after transforms/resize");
        t.x = std::numeric_limits<float>::quiet_NaN();
        bool failed = false;
        try {
            r.render(1, 320, 180, t);
        } catch (...) {
            failed = true;
        }
        require(failed, "NaN rejected");
        // A very small vector is enlarged before rasterization: new interior remains full coverage.
        t = {};
        t.scale = 8;
        auto large = r.render(1, 1920, 1080, t);
        require(alpha(large, 960, 540) > 120, "large vector zoom");
        std::cout << "Transform identity/translation/scale/rotation/anchor/fit/reset passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
