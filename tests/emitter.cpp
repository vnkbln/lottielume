#include <cmath>
#include <iostream>
#include <lottie/Emitter.hpp>
#include <stdexcept>
void check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
int main() {
    try {
        using E = lottie::Emitter;
        auto zero = E::Clock::time_point{};
        auto at = [&](int milliseconds) { return zero + std::chrono::milliseconds(milliseconds); };
        E e(zero);
        check(!e.trigger(zero), "no asset must not spawn");
        e.setDuration(2, zero);
        check(e.trigger(zero), "first trigger");
        e.update(at(500));
        check(e.trigger(at(500)), "overlap trigger");
        check(e.instances().size() == 2 && e.instances()[0].seconds == .5 &&
                  e.instances()[1].seconds == 0,
              "independent ages");
        const auto firstId = e.instances()[0].id;
        e.setSpeed(2, at(500));
        check(e.trigger(at(500)), "fast instance");
        e.update(at(750));
        check(e.instances()[0].seconds == .75 && e.instances()[1].seconds == .25 &&
                  e.instances()[2].seconds == .5,
              "different captured speeds");
        check(e.instances()[0].speed == 1 && e.instances()[2].speed == 2, "speed snapshot");
        e.setSpeed(0, at(750));
        check(e.trigger(at(750)), "frozen instance");
        e.update(at(1000));
        check(e.instances()[0].seconds == 1 && e.instances()[2].seconds == 1 &&
                  e.instances()[3].seconds == 0,
              "new speed zero must not freeze existing instances");
        e.setLimit(1, at(1000));
        check(!e.trigger(at(1000)) && e.instances().size() == 4,
              "limit preserves existing instances");
        e.setSpeed(4, at(1000));
        e.update(at(2000));
        check(e.instances().size() == 2 && e.instances()[0].id != firstId &&
                  e.instances()[0].seconds == 1.5 && e.instances()[1].speed == 0,
              "independent speed expiry");
        e.update(at(2500));
        check(e.instances().size() == 1 && e.instances()[0].seconds == 0,
              "frozen instance remains until clear");
        e.clear(at(2500));
        e.setSpeed(1, at(2500));
        check(e.trigger(at(2500)), "capacity after clear");
        e.update(at(2400));
        e.update(at(2600));
        check(std::abs(e.instances()[0].seconds - .1) < 1e-9, "backward timestamp");
        e.clear(at(6600));
        check(e.instances().empty(), "clear");
        e.setDuration(1, at(6600));
        e.setSpeed(.01, at(6600));
        e.trigger(at(6600));
        e.update(at(7600));
        check(std::abs(e.instances()[0].seconds - .01) < 1e-9, "slow motion fractional time");
        e.setDuration(2, at(7600));
        check(e.instances().empty(), "asset change clears states");
        E a(zero), b(zero);
        a.setDuration(2, zero);
        b.setDuration(2, zero);
        a.trigger(zero);
        a.update(at(1000));
        check(b.instances().empty(), "independent emitters");
        E fast(zero);
        fast.setDuration(2, zero);
        fast.setSpeed(10, zero);
        fast.trigger(zero);
        fast.update(at(100));
        check(fast.instances()[0].seconds == 1, "10x playback speed");
        fast.update(at(200));
        check(fast.instances().empty(), "10x one-shot expiry");
        E snapshots(zero);
        snapshots.setDuration(2, zero);
        lottie::Transform settings;
        settings.x = 12;
        settings.y = -34;
        settings.scale = 1.5f;
        settings.rotation = 90;
        settings.anchorX = 7;
        settings.anchorY = -8;
        settings.fit = lottie::Fit::Fill;
        settings.clipToCanvas = false;
        snapshots.trigger(zero, settings);
        settings.x = 100;
        settings.y = 200;
        settings.scale = .5f;
        settings.rotation = -45;
        settings.anchorX = -20;
        settings.anchorY = 30;
        settings.fit = lottie::Fit::Stretch;
        settings.clipToCanvas = true;
        snapshots.trigger(zero, settings);
        settings.x = 999;
        settings.scale = 0;
        const auto &old = snapshots.instances()[0].transform;
        const auto &recent = snapshots.instances()[1].transform;
        check(old.x == 12 && old.y == -34 && old.scale == 1.5f && old.rotation == 90 &&
                  old.anchorX == 7 && old.anchorY == -8 && old.fit == lottie::Fit::Fill &&
                  !old.clipToCanvas,
              "first transform snapshot");
        check(recent.x == 100 && recent.y == 200 && recent.scale == .5f && recent.rotation == -45 &&
                  recent.anchorX == -20 && recent.anchorY == 30 &&
                  recent.fit == lottie::Fit::Stretch && recent.clipToCanvas,
              "second transform snapshot independent of UI");
        snapshots.update(at(500));
        check(snapshots.instances()[0].transform.x == 12, "transform retained while advancing");
        E colors(zero);
        colors.setDuration(2, zero);
        lottie::Palette palette;
        palette.enabled = true;
        palette.colors[0] = {1, 0, 0};
        palette.alpha[0] = .5f;
        colors.trigger(zero, {}, palette);
        palette.colors[0] = {0, 1, 0};
        palette.alpha[0] = 1;
        colors.trigger(zero, {}, palette);
        palette.enabled = false;
        colors.trigger(zero, {}, palette);
        check(colors.instances()[0].palette.colors[0][0] == 1 &&
                  colors.instances()[0].palette.alpha[0] == .5f,
              "immutable first palette RGB and alpha");
        check(colors.instances()[1].palette.colors[0][1] == 1 &&
                  colors.instances()[1].palette.enabled,
              "second palette independent");
        check(!colors.instances()[2].palette.enabled, "original color mode captured");
        E opacity(zero);
        opacity.setDuration(2, zero);
        opacity.trigger(zero, {}, {}, 0, -1, .25f);
        opacity.trigger(zero, {}, {}, 0, -1, .75f);
        opacity.trigger(zero);
        check(opacity.instances()[0].opacity == .25f && opacity.instances()[1].opacity == .75f &&
                  opacity.instances()[2].opacity == 1,
              "independent opacity snapshots and default");
        std::cout << "Emitter clock/overlap/speed/limits/expiry tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
