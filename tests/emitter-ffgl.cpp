// clang-format off
#include <windows.h>
// clang-format on
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ffgl/FFGL.h>
#include <filesystem>
#include <gl_load.h>
#include <iostream>
#include <stdexcept>
#include <vector>

using Entry = FFMixed(__stdcall *)(FFUInt32, FFMixed, FFInstanceID);
void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
int main(int argc, char **argv) {
    try {
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
        std::cout << "GL: " << glGetString(GL_VERSION) << "\n";
        GLuint hostTexture = 0, hostFbo = 0;
        glGenTextures(1, &hostTexture);
        glBindTexture(GL_TEXTURE_2D, hostTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 320, 180, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenFramebuffers(1, &hostFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, hostFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hostTexture, 0);
        check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "host FBO");

        check(argc >= 3, "Usage: emitter-ffgl dll fixture.json");
        HMODULE lib = LoadLibraryA(argv[1]);
        check(lib, "load emitter DLL");
        auto entry = reinterpret_cast<Entry>(GetProcAddress(lib, "plugMain"));
        check(entry, "plugMain");
        FFMixed empty{};
        check(entry(FF_INITIALISE_V2, empty, nullptr).UIntValue == FF_SUCCESS, "initialise");
        FFGLViewportStruct vp{0, 0, 320, 180};
        FFMixed arg{};
        arg.PointerValue = &vp;
        auto id = entry(FF_INSTANTIATE_GL, arg, nullptr).PointerValue;
        check(id && id != reinterpret_cast<void *>(uintptr_t(FF_FAIL)), "instantiate");
        auto set = [&](void *instance, unsigned parameter, float value) {
            SetParameterStruct p{};
            if (parameter == 12)
                parameter = 6;
            else if (parameter >= 6 && parameter < 12)
                ++parameter;
            if (parameter == 30)
                parameter = 13;
            else if (parameter >= 13 && parameter < 30)
                ++parameter;
            if (parameter == 1)
                parameter = 6; // Spawn on Start follows Clear.
            else if (parameter > 5)
                ++parameter; // Other controls keep their legacy indices.
            if (parameter >= 4)
                ++parameter;
            p.ParameterNumber = parameter;
            std::memcpy(&p.NewParameterValue.UIntValue, &value, 4);
            FFMixed a{};
            a.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, a, instance).UIntValue == FF_SUCCESS, "set parameter");
        };
        auto file = [&](void *instance, const char *path) {
            SetParameterStruct p{};
            p.ParameterNumber = 0;
            p.NewParameterValue.PointerValue = const_cast<char *>(path);
            FFMixed a{};
            a.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, a, instance).UIntValue == FF_SUCCESS, "set file");
        };
        auto render = [&](void *instance) {
            ProcessOpenGLStruct p{};
            p.HostFBO = hostFbo;
            FFMixed a{};
            a.PointerValue = &p;
            check(entry(FF_PROCESS_OPENGL, a, instance).UIntValue == FF_SUCCESS, "process");
        };
        auto sample = [&](int x, int y) {
            std::array<unsigned char, 4> pixel{};
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            return pixel;
        };
        auto mass = [&]() {
            std::vector<unsigned char> pixels(320 * 180 * 4);
            glReadPixels(0, 0, 320, 180, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            double total = 0, weighted = 0;
            for (int y = 0; y < 180; ++y)
                for (int x = 0; x < 320; ++x) {
                    auto a = pixels[(y * 320 + x) * 4 + 3];
                    total += a;
                    weighted += a * x;
                }
            return std::pair<double, double>(total, total ? weighted / total : 0);
        };
        glViewport(0, 0, 320, 180);
        check(entry(FF_GET_NUM_PARAMETERS, empty, nullptr).UIntValue == 41, "parameter count");
        for (unsigned p : {2u, 6u}) {
            FFMixed n{};
            n.UIntValue = p;
            check(entry(FF_GET_PARAMETER_TYPE, n, nullptr).UIntValue == FF_TYPE_EVENT,
                  "native Event parameter");
        }
        // Opt out before first Process/implicit Connect: a fresh source has an
        // embedded demo and Spawn on Start is enabled by default.
        set(id, 1, 0);
        render(id);
        check(mass().first == 0, "empty emitter must be transparent");
        set(id, 3, 0);
        file(id, argv[2]);
        bool loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            set(id, 2, 1);
            render(id);
            if (sample(60, 90)[3] > 0) {
                loaded = true;
                break;
            }
            Sleep(10);
        }
        check(loaded, "load/trigger timeout");
        set(id, 5, 1);
        render(id);
        check(mass().first == 0, "clear");
        auto opacity = [&](float percent) {
            SetParameterStruct p{};
            p.ParameterNumber = 4;
            std::memcpy(&p.NewParameterValue.UIntValue, &percent, 4);
            FFMixed value{};
            value.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, value, id).UIntValue == FF_SUCCESS, "set Opacity");
        };
        opacity(50);
        set(id, 2, 1);
        opacity(0); // Changing before render must not change the queued spawn.
        render(id);
        auto half = sample(60, 90);
        check(half[3] >= 63 && half[3] <= 65 && half[0] == half[3],
              "instance opacity multiplies authored half alpha and premultiplied RGB");
        set(id, 2, 1); // Invisible instance must not alter the existing one.
        render(id);
        check(sample(60, 90) == half, "zero opacity and later changes preserve existing instance");
        opacity(100);
        set(id, 2, 1);
        render(id);
        auto mixed = sample(60, 90);
        check(mixed[3] >= 158 && mixed[3] <= 162 && mixed[0] == mixed[3],
              "different opacity snapshots composite independently");
        set(id, 5, 1);
        render(id);
        set(id, 2, 1);
        render(id);
        auto first = sample(60, 90);
        check(first[3] >= 126 && first[3] <= 129 && first[0] >= 126 && first[0] <= 129,
              "first instance premultiplied alpha");
        Sleep(50);
        render(id);
        check(sample(60, 90) == first, "no automatic emission at Speed 0");
        set(id, 2, 1);
        render(id);
        auto second = sample(60, 90);
        check(second[3] >= 189 && second[3] <= 193 && second[0] == second[3],
              "overlap alpha compositing");
        set(id, 4, 2);
        set(id, 2, 1);
        render(id);
        check(sample(60, 90) == second, "instance limit drops new triggers");
        set(id, 4, 1);
        set(id, 2, 1);
        render(id);
        check(sample(60, 90) == second, "reducing limit preserves existing instances");
        // Host GL state must survive all internal rendering/compositing passes.
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO);
        glColorMask(false, true, false, true);
        render(id);
        GLint state = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &state);
        check(state == GL_SRC_ALPHA && glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST),
              "GL blend/depth restore");
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state);
        check(state == GLint(hostFbo), "host FBO restore");
        GLboolean mask[4];
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        check(!mask[0] && mask[1] && !mask[2] && mask[3], "color mask restore");
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_ONE, GL_ZERO);
        glColorMask(true, true, true, true);
        set(id, 5, 1);
        set(id, 4, 16);
        set(id, 3, 1);
        set(id, 2, 1);
        render(id);
        auto start = mass();
        Sleep(200);
        render(id);
        auto advanced = mass();
        check(advanced.second > start.second + 15, "monotonic playback movement");
        set(id, 2, 1);
        render(id);
        check(sample(105, 90)[3] > 0 && sample(40, 90)[3] > 0 && sample(75, 90)[3] > 180,
              "independent staggered instances");
        set(id, 3, 0);
        render(id);
        auto frozen = mass();
        Sleep(150);
        render(id);
        check(mass().second > frozen.second + 10,
              "Speed change must not freeze existing instances");
        set(id, 5, 1);
        set(id, 3, .5f);
        set(id, 2, 1);
        set(id, 3, 2);
        set(id, 2, 1);
        render(id);
        Sleep(250);
        render(id);
        check(sample(50, 90)[3] > 0 && sample(130, 90)[3] > 0,
              "slow and fast instances must coexist");
        set(id, 5, 1);
        set(id, 3, 0);
        set(id, 6, 20);
        set(id, 8, 100);
        set(id, 2, 1);
        set(id, 6, 110);
        set(id, 8, 50);
        set(id, 9, 90);
        set(id, 10, -100);
        set(id, 2, 1);
        // Change UI again before either queued Trigger is rendered.
        set(id, 6, -160);
        set(id, 8, 10);
        set(id, 9, -90);
        set(id, 10, 100);
        render(id);
        check(sample(80, 90)[3] > 120 && sample(270, 90)[3] > 120 && sample(160, 90)[3] == 0,
              "queued triggers retain individual position/scale/rotation/anchor");
        set(id, 5, 1);
        set(id, 6, 0);
        set(id, 8, 100);
        set(id, 9, 0);
        set(id, 10, 0);
        set(id, 3, 4);
        set(id, 2, 1);
        render(id);
        check(mass().first > 0, "one-shot start");
        auto began = std::chrono::steady_clock::now();
        bool finished = false;
        for (int attempt = 0; attempt < 200; ++attempt) {
            render(id);
            if (mass().first == 0) {
                finished = true;
                break;
            }
            Sleep(10);
        }
        check(finished && std::chrono::steady_clock::now() - began > std::chrono::milliseconds(350),
              "one shot expiry at speed 4");
        file(id, "missing-emitter-asset.json");
        set(id, 2, 1);
        render(id);
        check(mass().first == 0, "missing/new file must clear active states");
        set(id, 3, 0);
        file(id, argv[2]);
        loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            set(id, 2, 1);
            render(id);
            if (sample(60, 90)[3] > 0) {
                loaded = true;
                break;
            }
            Sleep(10);
        }
        check(loaded, "reload recovery");
        arg.PointerValue = &vp;
        auto other = entry(FF_INSTANTIATE_GL, arg, nullptr).PointerValue;
        check(other && other != reinterpret_cast<void *>(uintptr_t(FF_FAIL)), "second emitter");
        render(other);
        check(mass().first == 0, "independent emitter remains empty");
        render(id);
        check(sample(60, 90)[3] > 0, "first emitter survives second");
        set(other, 3, 0);
        file(other, argv[2]);
        bool autoVisible = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            render(other);
            if (sample(60, 90)[3] > 0) {
                autoVisible = true;
                break;
            }
            Sleep(10);
        }
        check(autoVisible, "default automatic spawn after async file load");
        render(other);
        check(sample(60, 90)[3] >= 126 && sample(60, 90)[3] <= 129, "auto spawns only once");
        set(other, 5, 1);
        render(other);
        check(mass().first == 0, "Clear does not auto respawn");
        entry(FF_DISCONNECT, empty, other);
        entry(FF_CONNECT, empty, other);
        render(other);
        check(sample(60, 90)[3] > 120, "reconnect auto spawn");
        set(other, 5, 1);
        set(other, 1, 0);
        entry(FF_DISCONNECT, empty, other);
        entry(FF_CONNECT, empty, other);
        render(other);
        check(mass().first == 0, "auto spawn opt out");
        set(other, 1, 1);
        render(other);
        check(mass().first == 0, "enabling option does not spawn immediately");
        set(other, 6, 100);
        set(other, 12, 3);
        set(other, 2, 1);
        set(other, 6, 0);
        set(other, 12, 0);
        glViewport(0, 0, 160, 90);
        render(other);
        check(sample(104, 45)[3] > 120 && sample(120, 45)[3] == 0,
              "Emitter snapshots Original fit and native-size transform");
        glViewport(0, 0, 320, 180);
        check(entry(FF_DEINSTANTIATE_GL, empty, other).UIntValue == FF_SUCCESS, "second destroy");
        if (argc > 3) {
            set(id, 3, 0);
            file(id, argv[3]);
            for (int frame = 0; frame < 25; ++frame) {
                render(id);
                check(mass().first == 0, "file load must not auto-emit");
                Sleep(10);
            }
            set(id, 3, 1);
            set(id, 2, 1);
            render(id);
            Sleep(180);
            render(id);
            check(sample(160, 90)[2] > 120 && sample(160, 90)[0] < 5,
                  "older instance becomes blue");
            set(id, 3, 0);
            set(id, 2, 1);
            render(id);
            auto composed = sample(160, 90);
            check(composed[0] >= 126 && composed[0] <= 129 && composed[2] >= 62 &&
                      composed[2] <= 65 && composed[3] >= 189 && composed[3] <= 193,
                  "new red instance must composite over older blue");
        }
        if (argc > 4) {
            file(id, argv[4]);
            for (int frame = 0; frame < 25; ++frame) {
                render(id);
                Sleep(10);
            }
            set(id, 2, 1);
            render(id);
            check(mass().first == 0, "lazy missing asset must produce transparent output");
            set(id, 2, 1);
            render(id);
            check(mass().first == 0, "failed source must stay inactive");
        }
        set(id, 3, 0);
        auto paletteFile =
            (std::filesystem::u8path(argv[2]).parent_path() / "features" / "palette.json")
                .u8string();
        file(id, paletteFile.c_str());
        bool paletteLoaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            set(id, 2, 1);
            render(id);
            if (sample(80, 90)[3] > 0) {
                paletteLoaded = true;
                break;
            }
            Sleep(10);
        }
        check(paletteLoaded, "emitter palette fixture loaded");
        set(id, 5, 1);
        set(id, 14, 0);
        set(id, 15, 1);
        set(id, 16, 1);
        set(id, 17, .5f);
        set(id, 6, -40);
        set(id, 2, 1);
        set(id, 14, 1.f / 3);
        set(id, 17, 1);
        set(id, 6, 40);
        set(id, 2, 1);
        // Change again before either queued spawn is rendered.
        set(id, 14, 2.f / 3);
        set(id, 6, 0);
        for (int frame = 0; frame < 3; ++frame) {
            render(id);
            check(sample(40, 90)[0] >= 126 && sample(40, 90)[0] <= 129 && sample(40, 90)[1] == 0,
                  "first spawn captures red and half alpha at callback");
            check(sample(120, 90)[0] == 0 && sample(120, 90)[1] > 250,
                  "second spawn captures green independently");
        }
        for (unsigned k = 0; k < 16; ++k) {
            FFMixed query{};
            query.UIntValue = 17 + k;
            auto original = entry(FF_GET_PARAMETER_DEFAULT, query, nullptr);
            float value;
            std::memcpy(&value, &original.UIntValue, sizeof(value));
            set(id, 14 + k, value);
        }
        render(id);
        check(sample(120, 90)[1] > 250, "resetting colors affects only subsequent spawns");
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(80, 90)[0] > 230 && sample(80, 90)[2] > 220,
              "new spawn captures original palette mode");
        auto fillEffectFile =
            (std::filesystem::u8path(argv[2]).parent_path() / "features" / "fill-effect.json")
                .u8string();
        file(id, fillEffectFile.c_str());
        bool fillEffectLoaded = false;
        for (int n = 0; n < 500; ++n) {
            set(id, 2, 1);
            render(id);
            if (sample(80, 90)[3] > 240) {
                fillEffectLoaded = true;
                break;
            }
            Sleep(10);
        }
        check(fillEffectLoaded, "Emitter layer Fill fixture loaded");
        set(id, 5, 1);
        set(id, 14, 0);
        set(id, 15, 1);
        set(id, 16, 1);
        set(id, 17, .5f);
        set(id, 6, -40);
        set(id, 2, 1);
        set(id, 14, 1.f / 3);
        set(id, 17, 1);
        set(id, 6, 40);
        set(id, 2, 1);
        render(id);
        check(sample(40, 90)[0] >= 126 && sample(40, 90)[0] <= 129 && sample(40, 90)[1] == 0 &&
                  sample(40, 90)[3] == sample(40, 90)[0],
              "first layer Fill captures red half alpha");
        check(sample(120, 90)[0] == 0 && sample(120, 90)[1] > 250,
              "second layer Fill captures green independently");
        for (unsigned k = 0; k < 4; ++k) {
            FFMixed query{};
            query.UIntValue = 17 + k;
            auto original = entry(FF_GET_PARAMETER_DEFAULT, query, nullptr);
            float value;
            std::memcpy(&value, &original.UIntValue, 4);
            set(id, 14 + k, value);
        }
        set(id, 6, 0);
        auto overflowFile =
            (std::filesystem::u8path(argv[2]).parent_path() / "features" / "canvas-overflow.json")
                .u8string();
        set(id, 12, 3);
        set(id, 30, 0);
        file(id, overflowFile.c_str());
        bool overflowReady = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            set(id, 2, 1);
            render(id);
            if (sample(230, 120)[3] > 240) {
                overflowReady = true;
                break;
            }
            Sleep(10);
        }
        check(overflowReady, "emitter unclipped spawn");
        set(id, 30, 1);
        render(id);
        check(sample(230, 120)[3] > 240, "existing spawn keeps unclipped state");
        set(id, 2, 1);
        render(id);
        render(id);
        check(sample(230, 120)[3] > 240, "mixed clipped/unclipped instances remain independent");
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(205, 120)[3] > 240 && sample(230, 120)[3] == 0,
              "new spawn captures clipped state");
        set(id, 9, 35);
        set(id, 30, 1);
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(208, 87)[3] > 200 && sample(214, 89)[3] == 0,
              "fixed canvas crops rotated content without rotating the frame");
        set(id, 30, 0);
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(235, 74)[3] > 200, "rotated crop mask removed for uncropped instance");
        set(id, 9, 0);
        set(id, 30, 1);
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(230, 120)[3] == 0, "axis-aligned crop restored after masked rotation");
        set(id, 6, -150);
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(55, 120)[3] == 0, "Position does not move fixed crop frame");
        set(id, 30, 0);
        set(id, 5, 1);
        set(id, 2, 1);
        render(id);
        check(sample(55, 120)[3] > 200, "Crop off permits content outside fixed frame");
        auto cropFile =
            (std::filesystem::u8path(argv[2]).parent_path() / "features" / "crop-window.json")
                .u8string();
        set(id, 6, 0);
        file(id, cropFile.c_str());
        bool cropReady = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            set(id, 2, 1);
            render(id);
            if (sample(0, 0)[3] > 240) {
                cropReady = true;
                break;
            }
            Sleep(10);
        }
        check(cropReady, "crop window fixture ready");
        set(id, 30, 1);
        for (int fit = 0; fit < 4; ++fit) {
            set(id, 12, float(fit));
            set(id, 5, 1);
            set(id, 2, 1);
            render(id);
            check(sample(160, 90)[3] > 240, "crop preserves center in every Fit Mode");
            if (fit == 3) {
                check(sample(110, 65)[3] > 240 && sample(209, 114)[3] > 240 &&
                          sample(109, 90)[3] == 0 && sample(210, 90)[3] == 0 &&
                          sample(160, 64)[3] == 0 && sample(160, 115)[3] == 0,
                      "Original crop is exactly the native 100x50 window");
            } else if (fit == 0) {
                check(sample(0, 10)[3] > 240 && sample(319, 169)[3] > 240 &&
                          sample(160, 9)[3] == 0 && sample(160, 170)[3] == 0,
                      "Fit crop follows centered aspect ratio");
            } else {
                check(sample(0, 0)[3] > 240 && sample(319, 179)[3] > 240,
                      "Fill and Stretch crop cover output");
            }
        }
        // An unclipped spawn must not inherit the previous sample's scissor.
        set(id, 30, 0);
        set(id, 2, 1);
        glEnable(GL_SCISSOR_TEST);
        glScissor(3, 7, 11, 13);
        render(id);
        GLint scissor[4]{};
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        check(glIsEnabled(GL_SCISSOR_TEST) && scissor[0] == 3 && scissor[1] == 7 &&
                  scissor[2] == 11 && scissor[3] == 13,
              "host scissor enable and rectangle restored");
        check(sample(0, 0)[3] > 240 && sample(319, 179)[3] > 240,
              "mixed crop modes do not leak scissor into later samples or host copy");
        glDisable(GL_SCISSOR_TEST);
        check(glGetError() == GL_NO_ERROR, "GL error");
        check(entry(FF_DEINSTANTIATE_GL, empty, id).UIntValue == FF_SUCCESS, "destroy");
        entry(FF_DEINITIALISE, empty, nullptr);
        FreeLibrary(lib);
        glDeleteFramebuffers(1, &hostFbo);
        glDeleteTextures(1, &hostTexture);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        std::cout << "Emitter FFGL overlap/alpha/speed/expiry/cap/reload/state tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
#include "../deps/ffgl/source/lib/glsdk_0_5_2/glload/source/gl_load.c"
