#include <cmath>
#include <future>
#include <iostream>
#include <lottie/Core.hpp>
#include <lottie/Player.hpp>
#include <stdexcept>
#include <vector>
void require(bool ok, const char *s) {
    if (!ok)
        throw std::runtime_error(s);
}
int main(int argc, char **argv) {
    try {
        auto a = lottie::Asset::load(argv[1]);
        lottie::Renderer r(a), r2(a);
        auto m = r.metadata();
        require(m.width == 320 && m.height == 180 && m.duration == 2, "metadata");
        auto first = r.render(0, 320, 180).pixels;
        auto middle = r.render(1, 320, 180).pixels;
        require(first != middle, "animation did not change");
        require(first == r2.render(0, 320, 180).pixels, "instance independence");
        auto pixel = first[90 * 320 + 60];
        require((pixel >> 24) >= 126 && (pixel >> 24) <= 128, "alpha");
        require((pixel & 255) >= 126 && (pixel & 255) <= 128, "premultiplied red");
        require(((pixel >> 8) & 255) >= 30 && ((pixel >> 8) & 255) <= 33 &&
                    ((pixel >> 16) & 255) == 0,
                "RGBA order");
        require(first[0] == 0, "transparent background");
        r.render(2, 640, 480);
        r.render(0, 320, 180);
        auto buffer = r.render(0, 320, 180).pixels.data();
        require(buffer == r.render(.5, 320, 180).pixels.data(), "frame buffer reuse");
        auto fit = r.render(0, 640, 480).pixels;
        require(fit[20 * 640 + 120] == 0 && (fit[240 * 640 + 120] >> 24) > 0,
                "centered aspect fit");
        std::vector<std::future<void>> tasks;
        for (int thread = 0; thread < 4; ++thread)
            tasks.push_back(std::async(std::launch::async, [a] {
                for (int j = 0; j < 25; ++j) {
                    lottie::Renderer instance(a);
                    instance.render(j * .03, 320, 180);
                }
            }));
        for (auto &task : tasks)
            task.get();
        auto bad = std::make_shared<lottie::Asset>();
        bad->json = "{broken";
        bool failed = false;
        try {
            lottie::Renderer b(bad);
        } catch (...) {
            failed = true;
        }
        require(failed, "broken JSON accepted");
        failed = false;
        try {
            lottie::Asset::load("missing-file-12345.json");
        } catch (...) {
            failed = true;
        }
        require(failed, "missing file accepted");
        lottie::Player p;
        auto t = lottie::Player::Clock::now();
        p.duration(2, t);
        auto at = [&](int ms) { return t + std::chrono::milliseconds(ms); };
        require(std::abs(p.time(at(500)) - .5) < 1e-6, "clock");
        lottie::Player::Controls c;
        c.play = false;
        p.configure(c, at(500));
        require(p.time(at(1500)) == .5, "pause");
        c.play = true;
        c.speed = 2;
        p.configure(c, at(1500));
        require(std::abs(p.time(at(2000)) - 1.5) < 1e-6, "speed");
        require(std::abs(p.time(at(2500)) - .5) < 1e-6, "loop");
        c.manual = true;
        c.position = 1;
        p.configure(c, at(2500));
        require(p.time(at(5000)) == 2, "manual endpoint");
        c.manual = false;
        c.loop = false;
        p.configure(c, at(5000));
        require(p.time(at(6000)) == 2, "end clamp");
        p.restart(at(6000));
        require(p.time(at(6000)) == 0, "restart");
        c.loop = true;
        c.speed = -1;
        p.configure(c, at(6000));
        require(std::abs(p.time(at(6500)) - 1.5) < 1e-6, "reverse loop");
        std::cout << "core + playback tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
