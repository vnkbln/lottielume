// clang-format off
#include <windows.h>
// clang-format on
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ffgl/FFGL.h>
#include <filesystem>
#include <gl_load.h>
#include <iomanip>
#include <iostream>
#include <lottie/Core.hpp>
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
        check(argc >= 6, "Usage: emitter-palette-benchmark dll json width height frames");
        int width = std::stoi(argv[3]), height = std::stoi(argv[4]), frames = std::stoi(argv[5]);
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
            bool hasReload = entry(FF_GET_NUM_PARAMETERS, countQuery, nullptr).UIntValue >= 37;
            FFMixed opacityQuery{};
            opacityQuery.UIntValue = 4;
            auto opacityName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, opacityQuery, nullptr).PointerValue);
            bool hasOpacity = opacityName && std::string(opacityName) == "Opacity";
            FFMixed fitQuery{};
            fitQuery.UIntValue = 6 + (hasReload ? 1 : 0) + (hasOpacity ? 1 : 0);
            auto fitName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, fitQuery, nullptr).PointerValue);
            if (fitName && std::string(fitName, 8) == "Fit Mode") {
                if (parameter == 12)
                    parameter = 6;
                else if (parameter >= 6 && parameter < 12)
                    ++parameter;
            }
            FFMixed layoutQuery{};
            layoutQuery.UIntValue = 13 + (hasReload ? 1 : 0) + (hasOpacity ? 1 : 0);
            auto layoutName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, layoutQuery, nullptr).PointerValue);
            if (layoutName && std::string(layoutName, 14) == "Crop to Canvas") {
                if (parameter == 30)
                    parameter = 13;
                else if (parameter >= 13 && parameter < 30)
                    ++parameter;
            }
            FFMixed spawnQuery{};
            spawnQuery.UIntValue = 2;
            auto spawnName = static_cast<const char *>(
                entry(FF_GET_PARAMETER_NAME, spawnQuery, nullptr).PointerValue);
            bool newSpawnOrder = hasReload && spawnName && std::string(spawnName) == "Spawn";
            if (newSpawnOrder && parameter >= 1 && parameter <= 5) {
                if (parameter == 1)
                    parameter = 6;
            } else if (hasReload && parameter)
                ++parameter;
            if (hasOpacity && parameter >= 4)
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
        FFGLViewportStruct vp{0, 0, unsigned(width), unsigned(height)};
        FFMixed input{};
        input.PointerValue = &vp;
        auto instance = entry(FF_INSTANTIATE_GL, input, nullptr).PointerValue;
        check(instance && instance != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
              "instantiate emitter");
        setFloat(instance, 1, 0);
        setFloat(instance, 3, 0);
        setFloat(instance, 4, 64);
        SetParameterStruct file{};
        file.ParameterNumber = 0;
        file.NewParameterValue.PointerValue = argv[2];
        input.PointerValue = &file;
        check(entry(FF_SET_PARAMETER, input, instance).UIntValue == FF_SUCCESS, "file");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
        if (const char *rotation = std::getenv("LOTTIE_EMITTER_BENCH_ROTATION"))
            setFloat(instance, 9, float(std::atof(rotation)));
        if (const char *crop = std::getenv("LOTTIE_EMITTER_BENCH_CROP"))
            setFloat(instance, 30, std::atoi(crop) ? 1.f : 0.f);
        std::vector<unsigned char> readyPixels(size_t(width) * height * 4);
        bool ready = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            setFloat(instance, 2, 1);
            render(instance);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readyPixels.data());
            bool visible = false;
            for (size_t i = 3; i < readyPixels.size(); i += 4)
                if (readyPixels[i]) {
                    visible = true;
                    break;
                }
            if (visible) {
                ready = true;
                break;
            }
            Sleep(10);
        }
        check(ready, "emitter fixture ready");
        readyPixels.clear();
        readyPixels.shrink_to_fit();
        std::vector<int> counts{1, 8, 16};
        if (const char *count = std::getenv("LOTTIE_EMITTER_BENCH_COUNT"))
            counts = {std::atoi(count)};
        for (int count : counts)
            for (bool varied : {false, true}) {
                setFloat(instance, 5, 1);
                setFloat(instance, 13, 1);
                for (int n = 0; n < count; ++n) {
                    setFloat(instance, 14, varied ? float(n) / count : 0.f);
                    setFloat(instance, 15, 1);
                    setFloat(instance, 16, 1);
                    setFloat(instance, 17, 1);
                    setFloat(instance, 2, 1);
                }
                for (int warm = 0; warm < 12; ++warm) {
                    render(instance);
                    glFinish();
                }
                auto start = std::chrono::steady_clock::now();
                for (int frame = 0; frame < frames; ++frame) {
                    render(instance);
                    glFinish();
                }
                double ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count() /
                            frames;
                std::cout << std::fixed << std::setprecision(3) << "instances=" << count
                          << " varied=" << varied << " completed_frame_ms=" << ms << "\n";
            }
        check(entry(FF_DEINSTANTIATE_GL, empty, instance).UIntValue == FF_SUCCESS, "destroy");
        entry(FF_DEINITIALISE, empty, nullptr);
        FreeLibrary(lib);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
#include "../deps/ffgl/source/lib/glsdk_0_5_2/glload/source/gl_load.c"
