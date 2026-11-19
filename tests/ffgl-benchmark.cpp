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
        check(argc >= 6, "Usage: ffgl-benchmark dll json width height frames");
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
        int maxInstances = std::getenv("LOTTIE_BENCH_INSTANCES")
                               ? std::atoi(std::getenv("LOTTIE_BENCH_INSTANCES"))
                               : 3;
        for (int count = 1; count <= maxInstances; ++count) {
            std::vector<void *> instances;
            for (int j = 0; j < count; ++j) {
                FFGLViewportStruct vp{0, 0, unsigned(width), unsigned(height)};
                FFMixed arg{};
                arg.PointerValue = &vp;
                auto instance = entry(FF_INSTANTIATE_GL, arg, nullptr).PointerValue;
                check(instance && instance != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
                      "instantiate");
                instances.push_back(instance);
                if (const char *rotation = std::getenv("LOTTIE_BENCH_ROTATION"))
                    setFloat(instance, 12, float(std::atof(rotation)));
                if (const char *crop = std::getenv("LOTTIE_BENCH_CROP")) {
                    check(entry(FF_GET_NUM_PARAMETERS, empty, nullptr).UIntValue > 33,
                          "Crop benchmark requires new Player");
                    setFloat(instance, 33, std::atoi(crop) ? 1.f : 0.f);
                }
                SetParameterStruct p{};
                p.ParameterNumber = 0;
                p.NewParameterValue.PointerValue = argv[2];
                arg.PointerValue = &p;
                check(entry(FF_SET_PARAMETER, arg, instance).UIntValue == FF_SUCCESS, "load JSON");
            }
            glCheck("instantiate");
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, width, height);
            // Warm loading, shader compilation and render-target allocation before timing.
            for (int frame = 0; frame < 60; ++frame) {
                for (auto instance : instances) {
                    setFloat(instance, 4, .5f);
                    render(instance);
                }
                glFinish();
                Sleep(10);
            }
            std::vector<unsigned char> pixels(size_t(width) * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            size_t visible = 0;
            for (size_t i = 3; i < pixels.size(); i += 4)
                visible += pixels[i] > 0;
            check(visible > 0, "no visible output after warmup");
            if (argc > 6 && count == 1) {
                lottie::Frame image;
                image.width = width;
                image.height = height;
                image.pixels.resize(size_t(width) * height);
                for (int y = 0; y < height; ++y)
                    std::memcpy(image.pixels.data() + size_t(y) * width,
                                pixels.data() + size_t(height - 1 - y) * width * 4,
                                size_t(width) * 4);
                savePng(argv[6], image);
            }
            pixels.clear();
            pixels.shrink_to_fit();
            auto start = std::chrono::steady_clock::now();
            double submission = 0;
            for (int frame = 0; frame < frames; ++frame) {
                auto begin = std::chrono::steady_clock::now();
                for (auto instance : instances) {
                    setFloat(instance, 4, float(frame % frames) / float(frames));
                    render(instance);
                }
                submission += std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - begin)
                                  .count();
                glFinish();
            }
            double wall =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                    .count() /
                frames;
            check(glGetError() == GL_NO_ERROR, "GL errors during benchmark");
            std::cout << std::fixed << std::setprecision(3) << "instances=" << count
                      << " completed_frame_ms=" << wall << " submission_ms=" << submission / frames
                      << " visible_pixels=" << visible << "\n";
            for (auto instance : instances)
                check(entry(FF_DEINSTANTIATE_GL, empty, instance).UIntValue == FF_SUCCESS,
                      "destroy");
        }
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
