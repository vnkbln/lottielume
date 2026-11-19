#include <chrono>
#include <iostream>
#include <lottie/Core.hpp>
void savePng(const char *, const lottie::Frame &);
int main(int argc, char **argv) {
    try {
        if (argc < 4) {
            std::cerr << "Usage: lottie-render input.json output.png seconds [width height "
                         "[benchmark_frames]]\n";
            return 2;
        }
        lottie::Renderer r(lottie::Asset::load(argv[1]));
        auto m = r.metadata();
        std::cout << m.width << "x" << m.height << " duration=" << m.duration
                  << " frames=" << m.frames << "\n";
        unsigned w = argc > 4 ? std::stoul(argv[4]) : unsigned(m.width),
                 h = argc > 5 ? std::stoul(argv[5]) : unsigned(m.height);
        double t = std::stod(argv[3]);
        savePng(argv[2], r.render(t, w, h));
        if (argc > 6) {
            int count = std::stoi(argv[6]);
            if (count <= 0)
                throw std::runtime_error("Invalid benchmark count");
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < count; ++i)
                r.render(m.duration * i / count, w, h);
            std::cout << "CPU render mean ms="
                      << std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                                 .count() /
                             count
                      << "\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
