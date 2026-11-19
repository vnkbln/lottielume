#pragma once
#include <chrono>
#include <cstdint>
#include <lottie/Core.hpp>
#include <vector>
namespace lottie {
// Independent, one-shot playback states. No host or graphics API dependencies.
class Emitter {
  public:
    using Clock = std::chrono::steady_clock;
    struct Instance {
        uint64_t id;
        double seconds;
        double speed; // Captured at Trigger; independent of later control changes.
        Transform transform;
        Palette palette;
        uint64_t source = 0;
        double duration = 0;
        float opacity = 1; // Captured at Spawn; scales assembled premultiplied RGBA.
    };
    explicit Emitter(Clock::time_point now = Clock::now());
    void update(Clock::time_point now = Clock::now());
    void setDuration(double seconds, Clock::time_point now = Clock::now());
    void setSpeed(double speed, Clock::time_point now = Clock::now());
    void setLimit(unsigned limit, Clock::time_point now = Clock::now());
    bool trigger(Clock::time_point now = Clock::now(), const Transform &transform = {},
                 const Palette &palette = {}, uint64_t source = 0, double instanceDuration = -1,
                 float opacity = 1);
    void clear(Clock::time_point now = Clock::now());
    const std::vector<Instance> &instances() const { return active; }

  private:
    Clock::time_point last;
    double duration = 0, speed = 1;
    unsigned limit = 16;
    uint64_t next = 1;
    std::vector<Instance> active;
};
} // namespace lottie
