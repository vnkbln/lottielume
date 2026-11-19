#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace lottie {
// File names only: no animation is parsed or prefetched by the playlist.
class Playlist {
    std::vector<std::string> files;
    std::vector<size_t> bag, history;
    size_t historyPosition = 0;
    size_t current = size_t(-1);
    bool random = false;
    std::mt19937 rng;
    static std::string folded(std::string s) {
        for (auto &c : s)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

  public:
    explicit Playlist(unsigned seed = std::random_device{}()) : rng(seed) {}
    void setFolder(const std::string &folder, bool preserve = false) {
        std::vector<std::string> next;
        if (!folder.empty()) {
            for (const auto &entry :
                 std::filesystem::directory_iterator(std::filesystem::u8path(folder))) {
                if (entry.is_regular_file() &&
                    folded(entry.path().extension().u8string()) == ".json") {
                    if (next.size() == 4096)
                        throw std::runtime_error("Playlist folder exceeds 4096 JSON files");
                    next.push_back(entry.path().u8string());
                }
            }
            std::sort(next.begin(), next.end(), [](const auto &a, const auto &b) {
                auto x = folded(a), y = folded(b);
                return x == y ? a < b : x < y;
            });
        }
        if (preserve && next == files)
            return; // Keep shuffle progress on unchanged folders.
        auto previous = preserve && current < files.size() ? files[current] : std::string{};
        files = std::move(next);
        bag.clear();
        current = size_t(-1);
        history.clear();
        historyPosition = 0;
        if (!previous.empty())
            select(previous);
    }
    void setRandom(bool value) {
        if (random != value) {
            random = value;
            bag.clear();
            history.clear();
            if (current < files.size())
                history.push_back(current);
            historyPosition = 0;
        }
    }
    void select(const std::string &path) {
        auto wanted = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal();
        for (size_t n = 0; n < files.size(); ++n) {
            if (std::filesystem::absolute(std::filesystem::u8path(files[n])).lexically_normal() ==
                wanted) {
                current = n;
                bag.clear();
                history = {n};
                historyPosition = 0;
                return;
            }
        }
    }
    size_t size() const { return files.size(); }
    std::string next() {
        if (files.empty())
            return {};
        if (!random)
            current = current == size_t(-1) ? 0 : (current + 1) % files.size();
        else {
            if (!history.empty() && historyPosition + 1 < history.size()) {
                current = history[++historyPosition];
                return files[current];
            }
            if (bag.empty()) {
                bag.resize(files.size());
                std::iota(bag.begin(), bag.end(), size_t(0));
                std::shuffle(bag.begin(), bag.end(), rng);
                if (bag.size() > 1 && bag.back() == current)
                    std::swap(bag.front(), bag.back());
            }
            current = bag.back();
            bag.pop_back();
            history.push_back(current);
            if (history.size() > 4096)
                history.erase(history.begin());
            historyPosition = history.size() - 1;
        }
        return files[current];
    }
    std::string previous() {
        if (files.empty())
            return {};
        if (random) {
            if (!history.empty()) {
                if (historyPosition)
                    --historyPosition;
                current = history[historyPosition];
            } else if (current >= files.size())
                current = 0;
        } else
            current = current == size_t(-1) ? files.size() - 1
                                            : (current + files.size() - 1) % files.size();
        return files[current];
    }
};
} // namespace lottie
