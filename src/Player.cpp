#include <algorithm>
#include <cmath>
#include <lottie/Player.hpp>
#include <stdexcept>
namespace lottie {
double Player::time(Clock::time_point now) {
    double dt = std::max(0.0, std::chrono::duration<double>(now - last).count());
    last = now;
    if (!controls.manual && controls.play)
        cursor += dt * controls.speed;
    if (length <= 0)
        return 0;
    if (controls.loop) {
        cursor = std::fmod(cursor, length);
        if (cursor < 0)
            cursor += length;
    } else
        cursor = std::clamp(cursor, 0.0, length);
    return controls.manual ? controls.position * length : cursor;
}
void Player::configure(Controls c, Clock::time_point now) {
    if (!std::isfinite(c.speed) || !std::isfinite(c.position))
        throw std::runtime_error("Invalid playback control");
    double old = time(now);
    if (controls.manual && !c.manual)
        cursor = old;
    c.position = std::clamp(c.position, 0.0, 1.0);
    controls = c;
}
void Player::restart(Clock::time_point now) {
    cursor = 0;
    last = now;
}
void Player::duration(double d, Clock::time_point now) {
    length = std::max(0.0, d);
    restart(now);
}
} // namespace lottie
