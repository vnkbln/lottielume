// clang-format off
#include <windows.h>
// clang-format on
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ffgl/FFGL.h>
#include <filesystem>
#include <fstream>
#include <gl_load.h>
#include <iomanip>
#include <iostream>
#include <lottie/Core.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

void savePng(const char *, const lottie::Frame &);
using Entry = FFMixed(__stdcall *)(FFUInt32, FFMixed, FFInstanceID);
void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
int main(int argc, char **argv) {
    try {
        check(argc >= 6, "Usage: overflow-parity new.dll json width height reference.dll");
        int width = std::stoi(argv[3]), height = std::stoi(argv[4]), frames = 20;
        check(width > 0 && height > 0 && frames > 0, "invalid benchmark arguments");
        WNDCLASSA wc{};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "LottieSmoke";
        RegisterClassA(&wc);
        HWND window = CreateWindowA(wc.lpszClassName, "", WS_POPUP, 0, 0, 320, 180, nullptr,
                                    nullptr, wc.hInstance, nullptr);
        check(window, "window");
        HDC dc = GetDC(window);
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        int format = ChoosePixelFormat(dc, &pfd);
        check(format && SetPixelFormat(dc, format, &pfd), "pixel format");
        HGLRC rc = wglCreateContext(dc);
        check(rc && wglMakeCurrent(dc, rc), "context");
        using CreateContext = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
        auto create =
            reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
        check(create != nullptr, "WGL core context extension");
        const int attributes[] = {0x2091, 4, 0x2092, 1, 0x9126, 1, 0};
        auto core = create(dc, nullptr, attributes);
        check(core, "OpenGL 4.1 core context");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        rc = core;
        check(wglMakeCurrent(dc, rc), "make core current");
        ogl_LoadFunctions();
        if (std::getenv("LOTTIE_GL_DEBUG")) {
            using Callback = void(APIENTRY *)(GLenum, GLenum, GLuint, GLenum, GLsizei, const char *,
                                              const void *);
            using SetCallback = void(APIENTRY *)(Callback, const void *);
            auto callback =
                reinterpret_cast<SetCallback>(wglGetProcAddress("glDebugMessageCallback"));
            if (callback) {
                glEnable(0x92E0);
                glEnable(0x8242);
                callback(
                    [](GLenum, GLenum type, GLuint, GLenum, GLsizei, const char *message,
                       const void *) {
                        if (type == 0x824C)
                            std::cerr << "GL DEBUG: " << message << "\n";
                    },
                    nullptr);
            }
        }
        std::cout << "GL: " << glGetString(GL_VERSION) << "\n";

        std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
        GLuint texture = 0, fbo = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "host FBO");
        HMODULE lib = LoadLibraryA(argv[1]);
        check(lib, "load DLL");
        auto entry = reinterpret_cast<Entry>(GetProcAddress(lib, "plugMain"));
        check(entry, "plugMain");
        FFMixed empty{};
        check(entry(FF_INITIALISE_V2, empty, nullptr).UIntValue == FF_SUCCESS, "initialise");
        std::cout << "Backend: "
                  << static_cast<const char *>(
                         entry(FF_GET_PLUGIN_SHORT_NAME, empty, nullptr).PointerValue)
                  << " size=" << width << "x" << height << " frames=" << frames << "\n";
        auto setFloat = [&](void *instance, unsigned parameter, float value) {
            SetParameterStruct p{};
            FFMixed countQuery{};
            bool hasReload = entry(FF_GET_NUM_PARAMETERS, countQuery, nullptr).UIntValue >= 39;
            FFMixed fitQuery{};
            fitQuery.UIntValue = 7 + (hasReload ? 1 : 0);
            auto fitName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, fitQuery, nullptr).PointerValue);
            if (fitName && std::string(fitName, 8) == "Fit Mode") {
                if (parameter == 15)
                    parameter = 7;
                else if (parameter >= 7 && parameter < 15)
                    ++parameter;
            }
            FFMixed layoutQuery{};
            layoutQuery.UIntValue = 16 + (hasReload ? 1 : 0);
            auto layoutName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, layoutQuery, nullptr).PointerValue);
            if (layoutName && std::string(layoutName, 14) == "Crop to Canvas") {
                if (parameter == 33)
                    parameter = 16;
                else if (parameter >= 16 && parameter < 33)
                    ++parameter;
            }
            if (hasReload && parameter)
                ++parameter;
            p.ParameterNumber = parameter;
            std::memcpy(&p.NewParameterValue.UIntValue, &value, 4);
            FFMixed arg{};
            arg.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, arg, instance).UIntValue == FF_SUCCESS, "set float");
        };
        auto glCheck = [](const char *stage) {
            auto error = glGetError();
            if (error != GL_NO_ERROR)
                throw std::runtime_error(std::string(stage) + " GL error=" + std::to_string(error));
        };
        auto render = [&](void *instance) {
            ProcessOpenGLStruct p{};
            p.HostFBO = fbo;
            FFMixed arg{};
            arg.PointerValue = &p;
            check(entry(FF_PROCESS_OPENGL, arg, instance).UIntValue == FF_SUCCESS, "process");
            glCheck("process");
        };
        bool crop =
            std::getenv("LOTTIE_PARITY_CROP") && std::atoi(std::getenv("LOTTIE_PARITY_CROP"));
        std::ifstream source(std::filesystem::u8path(argv[2]));
        auto document = nlohmann::json::parse(source);
        double sourceW = document.at("w").get<double>(), sourceH = document.at("h").get<double>();
        HMODULE refLib = LoadLibraryA(argv[5]);
        check(refLib, "reference DLL");
        auto refEntry = reinterpret_cast<Entry>(GetProcAddress(refLib, "plugMain"));
        check(refEntry, "reference entry");
        auto newEntry = entry;
        std::vector<std::pair<Entry, void *>> instances;
        for (auto api : {newEntry, refEntry}) {
            entry = api;
            if (api == refEntry)
                check(entry(FF_INITIALISE_V2, empty, nullptr).UIntValue == FF_SUCCESS,
                      "initialize reference");
            FFGLViewportStruct vp{0, 0, unsigned(width), unsigned(height)};
            FFMixed arg{};
            arg.PointerValue = &vp;
            auto instance = entry(FF_INSTANTIATE_GL, arg, nullptr).PointerValue;
            check(instance && instance != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
                  "instantiate");
            instances.emplace_back(api, instance);
            setFloat(instance, 33, crop ? 1.f : 0.f);
            SetParameterStruct p{};
            p.ParameterNumber = 0;
            p.NewParameterValue.PointerValue = argv[2];
            arg.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, arg, instance).UIntValue == FF_SUCCESS, "file");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
        for (int warm = 0; warm < 60; ++warm) {
            for (auto item : instances) {
                entry = item.first;
                setFloat(item.second, 4, .5f);
                render(item.second);
            }
            glFinish();
            Sleep(10);
        }
        for (auto item : instances) {
            entry = item.first;
            render(item.second);
            std::vector<unsigned char> pixels(size_t(width) * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            bool visible = false;
            for (size_t i = 3; i < pixels.size(); i += 4)
                if (pixels[i]) {
                    visible = true;
                    break;
                }
            check(visible, "fixture must produce visible output before parity checks");
        }
        unsigned cases = 0;
        for (unsigned variant = 0; variant < 4; ++variant) {
            for (auto item : instances) {
                entry = item.first;
                setFloat(item.second, 15, variant == 1 ? 3 : 0);
                setFloat(item.second, 9,
                         variant == 0   ? 100
                         : variant == 1 ? 85
                         : variant == 2 ? 60
                                        : 300);
                setFloat(item.second, 12,
                         variant == 0   ? 0
                         : variant == 1 ? 33
                         : variant == 2 ? -75
                                        : 35);
                setFloat(item.second, 7, variant == 0 ? 0 : 15);
                setFloat(item.second, 8, variant == 0 ? 0 : -10);
                setFloat(item.second, 13, variant == 2 ? 37 : 0);
                setFloat(item.second, 14, variant == 2 ? -19 : 0);
            }
            for (float phase : {0.f, .17f, .5f, .83f, .99f}) {
                std::vector<unsigned char> images[2];
                for (unsigned i = 0; i < 2; ++i) {
                    auto item = instances[i];
                    entry = item.first;
                    setFloat(item.second, 4, phase);
                    render(item.second);
                    images[i].resize(size_t(width) * height * 4);
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, images[i].data());
                }
                // A single post-composite mask can differ from per-shape MSAA
                // only at the source-canvas boundary. Interior pixels stay strict.
                double fit = variant == 1 ? 1 : std::min(width / sourceW, height / sourceH);
                double scale = variant == 0 ? 1 : variant == 1 ? .85 : variant == 2 ? .6 : 3.;
                double angle = (variant == 0   ? 0
                                : variant == 1 ? 33
                                : variant == 2 ? -75
                                               : 35) *
                               3.141592653589793 / 180;
                double aa = std::cos(angle) * scale * fit, bb = -std::sin(angle) * scale * fit;
                double cc = std::sin(angle) * scale * fit, dd = std::cos(angle) * scale * fit;
                double px = fit * sourceW / 2 + (variant == 2 ? 37 : 0),
                       py = fit * sourceH / 2 - (variant == 2 ? -19 : 0);
                double tx = width / 2. + (variant == 0 ? 0 : 15) - aa / fit * px - bb / fit * py;
                double ty = height / 2. - (variant == 0 ? 0 : -10) - cc / fit * px - dd / fit * py;
                double determinant = aa * dd - bb * cc;
                auto cropEdgeDistance = [&](size_t channel) {
                    double x = double(channel / 4 % width) + .5 - tx,
                           y = height - .5 - double(channel / 4 / width) - ty;
                    double u = (dd * x - bb * y) / determinant,
                           v = (-cc * x + aa * y) / determinant;
                    double dx = std::min(std::abs(u), std::abs(u - sourceW)) *
                                std::abs(determinant) / std::hypot(dd, bb);
                    double dy = std::min(std::abs(v), std::abs(v - sourceH)) *
                                std::abs(determinant) / std::hypot(cc, aa);
                    return std::min(dx, dy);
                };
                size_t differences = 0, boundaryDifferences = 0;
                unsigned maxInterior = 0;
                unsigned maxDifference = 0;
                for (size_t i = 0; i < images[0].size(); ++i) {
                    unsigned d = unsigned(std::abs(int(images[0][i]) - int(images[1][i])));
                    if (d)
                        ++differences;
                    maxDifference = std::max(maxDifference, d);
                    if (crop && d && cropEdgeDistance(i) <= 2)
                        ++boundaryDifferences;
                    else
                        maxInterior = std::max(maxInterior, d);
                }
                if (differences)
                    std::cout << "variant=" << variant << " phase=" << phase
                              << " changed_channels=" << differences
                              << " max_diff=" << maxDifference
                              << " boundary_channels=" << boundaryDifferences
                              << " max_interior=" << maxInterior << "\n";
                check(maxInterior <= 1, "overflow parity interior pixel mismatch");
                ++cases;
            }
        }
        for (auto item : instances) {
            entry = item.first;
            check(entry(FF_DEINSTANTIATE_GL, empty, item.second).UIntValue == FF_SUCCESS,
                  "destroy");
            entry(FF_DEINITIALISE, empty, nullptr);
        }
        FreeLibrary(refLib);
        FreeLibrary(lib);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        std::cout << "Overflow parity passed: " << cases << " frames\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
#include "../deps/ffgl/source/lib/glsdk_0_5_2/glload/source/gl_load.c"
