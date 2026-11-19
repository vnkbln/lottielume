#include <algorithm>
#include <cmath>
#include <lottie/Emitter.hpp>
#include <stdexcept>
namespace lottie {
Emitter::Emitter(Clock::time_point now) : last(now) { active.reserve(64); }
void Emitter::update(Clock::time_point now) {
    if (now < last)
        now = last;
    double elapsed = std::chrono::duration<double>(now - last).count();
    last = now;
    for (auto &instance : active)
        instance.seconds += elapsed * instance.speed;
    active.erase(std::remove_if(active.begin(), active.end(),
                                [&](const Instance &i) { return i.seconds >= i.duration; }),
                 active.end());
}
void Emitter::setDuration(double value, Clock::time_point now) {
    if (!std::isfinite(value) || value < 0)
        throw std::runtime_error("Invalid emitter duration");
    clear(now);
    duration = value;
}
void Emitter::setSpeed(double value, Clock::time_point now) {
    if (!std::isfinite(value) || value < 0 || value > 10)
        throw std::runtime_error("Emitter Speed must be 0..10");
    update(now);
    speed = value;
}
void Emitter::setLimit(unsigned value, Clock::time_point now) {
    if (value < 1 || value > 64)
        throw std::runtime_error("Emitter limit must be 1..64");
    update(now);
    limit = value;
}
bool Emitter::trigger(Clock::time_point now, const Transform &transform, const Palette &palette,
                      uint64_t source, double instanceDuration, float opacity) {
    if (!std::isfinite(opacity) || opacity < 0 || opacity > 1)
        throw std::runtime_error("Invalid emitter opacity");
    update(now);
    double length = instanceDuration < 0 ? duration : instanceDuration;
    if (!std::isfinite(length) || length <= 0 || active.size() >= limit)
        return false;
    active.push_back({next++, 0, speed, transform, palette, source, length, opacity});
    return true;
}
void Emitter::clear(Clock::time_point now) {
    update(now);
    active.clear();
}
} // namespace lottie
