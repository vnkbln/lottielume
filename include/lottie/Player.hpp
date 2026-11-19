#pragma once
#include <chrono>
namespace lottie {
class Player {
  public:
    using Clock = std::chrono::steady_clock;
    struct Controls {
        bool play = true, loop = true, manual = false;
        double speed = 1, position = 0;
    };
    void configure(Controls controls, Clock::time_point now = Clock::now());
    void restart(Clock::time_point now = Clock::now());
    void duration(double seconds, Clock::time_point now = Clock::now());
    double time(Clock::time_point now = Clock::now());

  private:
    Controls controls;
    Clock::time_point last = Clock::now();
    double cursor = 0, length = 0;
};
} // namespace lottie
