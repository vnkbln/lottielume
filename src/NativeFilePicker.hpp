#pragma once
// Include before FFGLSDK: its Windows feature macros omit dialog declarations.
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <commdlg.h>
// clang-format on
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
namespace lottie_ui {
// Official master FF_EVENT_FLAG_VALUE ABI, supported by Resolume >= 7.4.
// https://github.com/resolume/ffgl/blob/master/source/lib/ffgl/FFGL.h
// Pinned v2.2 already transports arbitrary event bits; do not use its commented 0x02.
constexpr unsigned long long valueChanged = 0x04;
inline bool missingFile(const std::string &path) {
    if (path.empty())
        return false;
    std::error_code error;
    bool exists = std::filesystem::exists(std::filesystem::u8path(path), error);
    return !exists && !error;
}
inline std::string pickJson(const std::string &previous) {
    std::vector<wchar_t> buffer(32768, 0);
    std::wstring directory;
    if (!previous.empty()) {
        auto path = std::filesystem::u8path(previous);
        std::error_code ec;
        if (std::filesystem::is_directory(path.parent_path(), ec))
            directory = path.parent_path().wstring();
        auto name = path.filename().wstring();
        if (name.size() < buffer.size())
            std::copy(name.begin(), name.end(), buffer.begin());
    }
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrTitle = L"Replace Lottie JSON";
    dialog.lpstrFilter = L"Lottie JSON (*.json)\0*.json\0\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = DWORD(buffer.size());
    dialog.lpstrInitialDir = directory.empty() ? nullptr : directory.c_str();
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   OFN_DONTADDTORECENT;
    if (GetOpenFileNameW(&dialog))
        return std::filesystem::path(buffer.data()).u8string();
    if (CommDlgExtendedError())
        throw std::runtime_error("Cannot open Lottie file selection dialog");
    return {};
}
} // namespace lottie_ui
