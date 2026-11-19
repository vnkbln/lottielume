// clang-format off
#include <windows.h>
// clang-format on
#include "../src/Playlist.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ffgl/FFGL.h>
#include <filesystem>
#include <fstream>
#include <gl_load.h>
#include <iostream>
#include <set>
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
            bool emitterLayout = argc > 3 && std::string(argv[3]) == "emitter";
            unsigned fit = emitterLayout ? 12 : 15, start = emitterLayout ? 6 : 7;
            if (parameter == fit)
                parameter = start;
            else if (parameter >= start && parameter < fit)
                ++parameter;
            if (emitterLayout && parameter >= 1 && parameter <= 5) {
                if (parameter == 1)
                    parameter = 6;
            } else if (parameter)
                ++parameter;
            if (emitterLayout && parameter >= 4)
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

        check(argc >= 4, "Usage: integrated-playlists dll moving-square.json player|emitter");
        bool isEmitter = std::string(argv[3]) == "emitter";
        FFMixed reloadVisibility{};
        reloadVisibility.UIntValue = 1;
        check(entry(FF_GET_PRAMETER_VISIBILITY, reloadVisibility, id).UIntValue == FF_FALSE,
              "Reload is hidden without changing its legacy index");

        auto reload = [&] {
            SetParameterStruct p{};
            p.ParameterNumber = 1;
            float value = 1;
            std::memcpy(&p.NewParameterValue.UIntValue, &value, 4);
            FFMixed arg{};
            arg.PointerValue = &p;
            check(entry(FF_SET_PARAMETER, arg, id).UIntValue == FF_SUCCESS, "Reload event");
        };

        auto root = std::filesystem::current_path() / "Testing" /
                    ("integrated-" + std::to_string(GetCurrentProcessId()) + "-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        auto anchor = (root / "A.json").u8string();
        auto makeBlue = [&] {
            std::ifstream input(std::filesystem::u8path(anchor));
            std::string json((std::istreambuf_iterator<char>(input)), {});
            auto pos = json.find("[1,0.25,0,1]");
            check(pos != std::string::npos, "find fixture color");
            json.replace(pos, std::string("[1,0.25,0,1]").size(), "[0,0,1,1]");
            std::ofstream(std::filesystem::u8path(anchor)) << json;
        };

        std::filesystem::copy_file(std::filesystem::u8path(argv[2]), root / "A.json");
        std::ofstream(root / "B.json")
            << R"({"v":"5.7.4","fr":30,"ip":0,"op":15,"w":320,"h":180,"ddd":0,"assets":[],"layers":[{"ddd":0,"ind":1,"ty":1,"nm":"Green","sr":1,"ks":{"o":{"a":0,"k":100},"r":{"a":0,"k":0},"p":{"a":0,"k":[260,90,0]},"a":{"a":0,"k":[30,30,0]},"s":{"a":0,"k":[100,100,100]}},"sw":60,"sh":60,"sc":"#00ff00","ip":0,"op":15,"st":0,"bm":0}]})";
        unsigned enabled = isEmitter ? 31 : 34, next = enabled + (isEmitter ? 4 : 3);
        auto connect = [&](bool enabled) {
            check(entry(enabled ? FF_CONNECT : FF_DISCONNECT, empty, id).UIntValue == FF_SUCCESS,
                  "connection");
        };
        // Embedded demo must render without any filesystem File reference.
        if (isEmitter) {
            set(id, 1, 0);
            set(id, 3, 1);
        } else
            set(id, 4, .5f);
        bool demoVisible = false;
        for (int n = 0; n < 500; ++n) {
            if (isEmitter)
                set(id, 2, 1);
            render(id);
            if (mass().first > 100) {
                demoVisible = true;
                break;
            }
            Sleep(10);
        }
        check(demoVisible, "embedded LottieLogo renders with empty File");
        if (!isEmitter)
            set(id, 4, 0);
        else
            set(id, 3, 0);
        file(id, anchor.c_str());
        bool loaded = false;
        for (int n = 0; n < 500; ++n) {
            if (isEmitter)
                set(id, 2, 1);
            render(id);
            if (sample(60, 90)[3] > 100) {
                loaded = true;
                break;
            }
            Sleep(10);
        }
        check(loaded, "initial asynchronous file load");
        set(id, enabled, 1);
        render(id);
        auto drainFileEvents = [&] {
            ParamEventStruct events[64]{};
            GetParamEventsStruct query{};
            query.numEvents = 64;
            query.events = events;
            FFMixed arg{};
            arg.PointerValue = &query;
            check(entry(FF_GET_PARAMETER_EVENTS, arg, id).UIntValue == FF_SUCCESS,
                  "read File events");
            bool changed = false;
            for (unsigned n = 0; n < query.numEvents; ++n)
                changed =
                    changed || (events[n].ParameterNumber == 0 && (events[n].eventFlags & 0x04));
            return changed;
        };
        drainFileEvents(); // Discard initial File and visibility events before Next.
        if (isEmitter) {
            set(id, 5, 1);
            set(id, 2, 1); // A
            set(id, next, 1);
            set(id, 2, 1); // B in the same command batch
            render(id);
            check(sample(60, 90)[0] > 100 && sample(260, 90)[1] > 250,
                  "A and B coexist with distinct source snapshots");
            check(drainFileEvents(), "Emitter Next emits File VALUE event");
            render(id);
            check(!drainFileEvents(), "idle Emitter frames do not repeat File notifications");
            FFMixed selectedQuery{};
            selectedQuery.UIntValue = 0;
            auto selectedFile = std::string(
                static_cast<char *>(entry(FF_GET_PARAMETER, selectedQuery, id).PointerValue));
            check(selectedFile == (root / "B.json").u8string(),
                  "Emitter File shows B selected for future spawns");
            file(id, selectedFile.c_str()); // Simulate the host applying a value-event echo.
            render(id);
            check(sample(60, 90)[0] > 100 && sample(260, 90)[1] > 250,
                  "Emitter File echo does not reload or clear live instances");

            set(id, next, 1);
            render(id); // select A, without spawning
            check(sample(60, 90)[3] > 100 && sample(260, 90)[1] > 250,
                  "Next alone leaves both running sources unchanged");
            set(id, 6, 80);
            set(id, 2, 1);
            render(id);
            check(sample(140, 90)[0] > 100 && sample(260, 90)[1] > 250,
                  "cached A scene reused with independent transform while B remains");
            set(id, enabled, 0);
            render(id);
            check(sample(260, 90)[1] > 250, "disabling playlist preserves running other files");
            set(id, 5, 1);
            render(id);
            check(mass().first == 0, "Clear removes all files");
            set(id, 6, 0);
            set(id, 3, 1);
            set(id, enabled, 1);
            render(id);
            set(id, next, 1);
            set(id, 2, 1); // B: duration .5s
            set(id, next, 1);
            set(id, 2, 1); // A: duration 2s
            render(id);
            Sleep(650);
            render(id);
            check(sample(260, 90)[3] == 0 && sample(120, 90)[3] > 100,
                  "instances expire by their own file duration");
            // A bad new resource must not clear existing healthy instances.
            set(id, 3, 0);
            set(id, 5, 1);
            set(id, 2, 1);
            render(id);
            auto bad = std::filesystem::u8path(argv[2]).parent_path() / "features" /
                       "emitter-missing-image.json";
            std::filesystem::copy_file(bad, root / "C.json");
            set(id, enabled, 0);
            set(id, enabled, 1);
            render(id); // rescan, anchored to A
            set(id, next, 1);
            set(id, next, 1);
            set(id, 2, 1);
            render(id);
            check(sample(60, 90)[3] > 100, "failed C sample does not erase A");
            render(id);
            check(sample(60, 90)[3] > 100, "failed source remains isolated on later frames");
            set(id, 5, 1);
            render(id);
            std::filesystem::remove(root / "C.json");
            file(id, anchor.c_str());
            for (int attempt = 0; attempt < 500; ++attempt) {
                set(id, 2, 1);
                render(id);
                if (sample(60, 90)[0] > 100)
                    break;
                Sleep(10);
            }
            set(id, 5, 1);
            render(id);
            set(id, 1, 1);
            connect(false);
            connect(true);
            render(id);
            check(sample(260, 90)[1] > 250 && sample(60, 90)[3] == 0,
                  "Next on Start selects B before automatic Spawn captures source");
            set(id, 1, 0);
            set(id, 5, 1);
            render(id);
            connect(false);
            connect(true);
            render(id); // selects A without spawning
            check(mass().first == 0, "Next on Start does not imply Spawn when auto spawn disabled");
            set(id, 2, 1);
            render(id);
            check(sample(60, 90)[3] > 100, "manual Spawn uses newly selected A");
            set(id, 5, 1);
            set(id, 4, 1);
            set(id, 2, 1);
            set(id, next, 1);
            set(id, 2, 1);
            render(id);
            check(sample(60, 90)[3] > 100 && sample(260, 90)[3] == 0,
                  "global instance limit applies across files");
            set(id, 4, 16);
            set(id, 5, 1);
            render(id);
            file(id, anchor.c_str());
            for (int n = 0; n < 500; ++n) {
                set(id, 2, 1);
                render(id);
                if (sample(60, 90)[3] > 100)
                    break;
                Sleep(10);
            }
            check(sample(60, 90)[3] > 100, "prepare live A for anchor replacement");
            file(id, (root / "B.json").u8string().c_str());
            for (int n = 0; n < 500; ++n) {
                set(id, 2, 1);
                render(id);
                if (sample(260, 90)[1] > 250)
                    break;
                Sleep(10);
            }
            check(sample(60, 90)[3] > 100 && sample(260, 90)[1] > 250,
                  "choosing a different File in playlist mode also preserves live sources");
            set(id, 5, 1);
            render(id);
            set(id, 34, 1); // Spawn on Next (new native index 35).
            set(id, next, 1);
            render(id); // B -> A, automatic Spawn.
            check(sample(60, 90)[0] > 100,
                  "Spawn on Next launches selected A without separate Trigger");
            set(id, next, 1);
            render(id);
            check(sample(60, 90)[0] > 100 && sample(260, 90)[1] > 250,
                  "Spawn on Next launches B and preserves A");
            set(id, 34, 0);
            set(id, 5, 1);
            render(id);
            file(id, anchor.c_str());
            for (int n = 0; n < 500; ++n) {
                set(id, 2, 1);
                render(id);
                if (sample(60, 90)[0] > 100)
                    break;
                Sleep(10);
            }
            makeBlue();
            set(id, 33, 0); // Disable Next on Start so reactivation refreshes current A.
            {
                std::ifstream input(root / "B.json");
                std::string json((std::istreambuf_iterator<char>(input)), {});
                auto color = json.find("#00ff00");
                check(color != std::string::npos, "find second fixture color");
                json.replace(color, 7, "#ffff00");
                std::ofstream(root / "C.json") << json;
            }
            connect(false);
            connect(true);
            set(id, 6, 80);
            bool refreshed = false;
            for (int n = 0; n < 500; ++n) {
                set(id, 2, 1);
                render(id);
                if (sample(140, 90)[2] > 100) {
                    refreshed = true;
                    break;
                }
                Sleep(10);
            }
            check(refreshed && sample(60, 90)[3] == 0,
                  "reactivation clears old instances and refreshes JSON for new spawns");
            set(id, 6, 0);
            set(id, 34, 1);
            set(id, next, 1);
            set(id, next, 1);
            render(id);
            check(sample(260, 90)[0] > 250 && sample(260, 90)[1] > 250,
                  "reactivation rescans folder so newly added C can be selected and spawned");

        } else {
            set(id, 4, .5f);
            set(id, next, 1);
            render(id);
            check(sample(260, 90)[1] > 250 && sample(60, 90)[3] == 0, "Player Next selects B");
            set(id, 4, .73f); // Host Timeline ticks before it has read the pending event.
            FFMixed positionQuery{};
            positionQuery.UIntValue = 5;
            auto positionValue = entry(FF_GET_PARAMETER, positionQuery, id);
            float phase = -1;
            std::memcpy(&phase, &positionValue.UIntValue, 4);
            check(phase == 0, "pending reset survives a host write before its first read");
            ParamEventStruct positionEvents[64]{};
            GetParamEventsStruct positionEventQuery{};
            positionEventQuery.numEvents = 64;
            positionEventQuery.events = positionEvents;
            FFMixed positionEventArg{};
            positionEventArg.PointerValue = &positionEventQuery;
            check(entry(FF_GET_PARAMETER_EVENTS, positionEventArg, id).UIntValue == FF_SUCCESS,
                  "read Position events");
            bool resetEvent = false, fileEvent = false;
            for (unsigned n = 0; n < positionEventQuery.numEvents; ++n) {
                resetEvent = resetEvent || (positionEvents[n].ParameterNumber == 5 &&
                                            (positionEvents[n].eventFlags & 0x04));
                fileEvent = fileEvent || (positionEvents[n].ParameterNumber == 0 &&
                                          (positionEvents[n].eventFlags & 0x04));
            }
            check(fileEvent, "Player Next emits File VALUE event alongside Position reset");
            check(resetEvent, "Next notifies host of Position reset through VALUE event");
            set(id, 4, .25f);
            render(id);
            positionValue = entry(FF_GET_PARAMETER, positionQuery, id);
            std::memcpy(&phase, &positionValue.UIntValue, 4);
            check(phase == .25f, "subsequent host Position updates are not suppressed");
            set(id, next, 1);
            render(id);
            set(id, 4, .42f); // No read/acknowledgement: deadline must release the latch.
            Sleep(300);
            render(id);
            positionValue = entry(FF_GET_PARAMETER, positionQuery, id);
            std::memcpy(&phase, &positionValue.UIntValue, 4);
            check(phase == .42f, "unread reset expires and restores the latest host phase");
            set(id, next, 1);
            render(id); // Restore selected B for following assertions.
            entry(FF_GET_PARAMETER, positionQuery, id);

            set(id, 4, 0);
            FFMixed value{};
            value.UIntValue = 0;
            check(
                std::string(static_cast<char *>(entry(FF_GET_PARAMETER, value, id).PointerValue)) ==
                    (root / "B.json").u8string(),
                "host File follows selected B");
            file(id, (root / "B.json").u8string().c_str()); // Host echo must not enqueue a reload.
            set(id, next, 1);
            render(id);
            entry(FF_GET_PARAMETER, positionQuery, id);
            check(sample(60, 90)[0] > 100,
                  "File echo preserves sequence for immediate Next back to A");
            set(id, enabled, 0);
            render(id);
            check(sample(60, 90)[0] > 100 && sample(260, 90)[3] == 0,
                  "Player off returns to selected A");
            entry(FF_GET_PARAMETER, positionQuery, id);
            set(id, 4, .5f);
            render(id);
            check(sample(160, 90)[3] > 100, "Player Position phase preserved");
            set(id, enabled, 1);
            render(id);
            connect(false);
            connect(true);
            render(id);
            check(sample(260, 90)[1] > 250, "Player Next on Start advances on reconnect");
            connect(true);
            render(id);
            check(sample(260, 90)[1] > 250, "duplicate Connect does not advance Player");
            set(id, enabled, 0);
            render(id); // Keep current B, phase zero after automatic Next.
            entry(FF_GET_PARAMETER, positionQuery, id);
            set(id, 4, .5f);
            render(id);
            {
                std::ifstream input(root / "B.json");
                std::string json((std::istreambuf_iterator<char>(input)), {});
                auto color = json.find("#00ff00");
                check(color != std::string::npos, "find current B color");
                json.replace(color, 7, "#0000ff");
                std::ofstream(root / "B.json") << json;
            }
            connect(false);
            connect(true);
            bool refreshed = false;
            for (int n = 0; n < 500; ++n) {
                render(id);
                if (sample(260, 90)[2] > 250) {
                    refreshed = true;
                    break;
                }
                Sleep(10);
            }
            check(refreshed, "Player reactivation rereads currently displayed B");
            positionValue = entry(FF_GET_PARAMETER, positionQuery, id);
            std::memcpy(&phase, &positionValue.UIntValue, 4);
            check(phase == .5f, "reactivation preserves Position on current file");
        }
        auto consumeEvents = [&] {
            std::vector<ParamEventStruct> events(64);
            GetParamEventsStruct query{};
            query.numEvents = unsigned(events.size());
            query.events = events.data();
            FFMixed arg{};
            arg.PointerValue = &query;
            check(entry(FF_GET_PARAMETER_EVENTS, arg, id).UIntValue == FF_SUCCESS,
                  "consume events");
            events.resize(query.numEvents);
            return events;
        };
        consumeEvents();
        auto relocated =
            (root / std::filesystem::u8path(u8"relocated-\u0442\u0435\u0441\u0442.json"))
                .u8string();
        {
            std::ifstream input(root / "B.json");
            std::string json((std::istreambuf_iterator<char>(input)), {});
            auto blue = json.find("#0000ff");
            if (blue != std::string::npos)
                json.replace(blue, 7, "#00ff00");
            std::ofstream(std::filesystem::u8path(relocated)) << json;
        }
        file(id, relocated.c_str()); // Same request path used by the native Replace File dialog.
        auto events = consumeEvents();
        bool valueChanged = false;
        for (const auto &event : events)
            valueChanged =
                valueChanged || (event.ParameterNumber == 0 && (event.eventFlags & 0x04));
        check(valueChanged, "File value-change event reaches host with official 0x04 ABI");
        FFMixed query{};
        query.UIntValue = 0;
        auto savedPath =
            std::string(static_cast<char *>(entry(FF_GET_PARAMETER, query, id).PointerValue));
        check(savedPath == relocated, "host reads new Unicode File path for serialization");
        file(id, savedPath.c_str());
        events = consumeEvents();
        for (const auto &event : events)
            check(event.ParameterNumber != 0 || !(event.eventFlags & 0x04),
                  "same path does not create notification loop");
        FFGLViewportStruct restoreViewport{0, 0, 320, 180};
        FFMixed restoreArgument{};
        restoreArgument.PointerValue = &restoreViewport;
        auto restored = entry(FF_INSTANTIATE_GL, restoreArgument, nullptr).PointerValue;
        check(restored && restored != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
              "instantiate restored source");
        if (isEmitter) {
            set(restored, 1, 0);
            set(restored, 3, 0);
        }
        file(restored, savedPath.c_str());
        bool restoredVisible = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            if (isEmitter)
                set(restored, 2, 1);
            render(restored);
            if (sample(260, 90)[1] > 250) {
                restoredVisible = true;
                break;
            }
            Sleep(10);
        }
        check(restoredVisible, "saved replacement path restores playback in a fresh source");
        check(entry(FF_DEINSTANTIATE_GL, empty, restored).UIntValue == FF_SUCCESS,
              "destroy restored source");
        const unsigned recoveryIndex = isEmitter ? 39 : 40;
        auto readText = [&](void *target, unsigned index) {
            FFMixed arg{};
            arg.UIntValue = index;
            return std::string(
                static_cast<char *>(entry(FF_GET_PARAMETER, arg, target).PointerValue));
        };
        auto writeRecovery = [&](void *target, const std::string &path) {
            SetParameterStruct parameter{};
            parameter.ParameterNumber = recoveryIndex;
            parameter.NewParameterValue.PointerValue = const_cast<char *>(path.c_str());
            FFMixed arg{};
            arg.PointerValue = &parameter;
            check(entry(FF_SET_PARAMETER, arg, target).UIntValue == FF_SUCCESS,
                  "restore recovery text");
        };
        auto missingPath = (root / "missing-source.json").u8string();
        file(id, missingPath.c_str());
        check(readText(id, 0).empty() && readText(id, recoveryIndex) == missingPath,
              "missing File becomes empty native Load field while hidden path persists");
        file(id, "");
        check(readText(id, recoveryIndex) == missingPath,
              "empty host acknowledgement preserves missing path");
        for (bool hiddenFirst : {false, true}) {
            auto target = entry(FF_INSTANTIATE_GL, restoreArgument, nullptr).PointerValue;
            check(target && target != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
                  "restore missing source");
            if (isEmitter) {
                set(target, 1, 0);
                set(target, 3, 0);
            }
            if (hiddenFirst)
                writeRecovery(target, missingPath);
            file(target, "");
            if (!hiddenFirst)
                writeRecovery(target, missingPath);
            render(target);
            check(readText(target, 0).empty() && readText(target, recoveryIndex) == missingPath,
                  "missing reference survives either parameter restoration order");
            file(target, savedPath.c_str());
            writeRecovery(target,
                          missingPath); // stale hidden value restored after explicit valid File
            check(readText(target, 0) == savedPath && readText(target, recoveryIndex).empty(),
                  "valid replacement wins over stale hidden restoration");
            check(entry(FF_DEINSTANTIATE_GL, empty, target).UIntValue == FF_SUCCESS,
                  "destroy missing source");
        }
        // A saved missing reference can become available again before composition restore.
        std::filesystem::copy_file(std::filesystem::u8path(relocated),
                                   std::filesystem::u8path(missingPath));
        auto target = entry(FF_INSTANTIATE_GL, restoreArgument, nullptr).PointerValue;
        check(target && target != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
              "restore available reference");
        writeRecovery(target, missingPath);
        file(target, "");
        render(target);
        check(readText(target, 0) == missingPath && readText(target, recoveryIndex).empty(),
              "reappeared hidden reference restores native File without losing path");
        bool sourceLoaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            render(target);
            if (sample(260, 90)[1] > 250) {
                sourceLoaded = true;
                break;
            }
            Sleep(10);
        }
        check(sourceLoaded, "restored file finishes loading before runtime move");
        std::filesystem::rename(std::filesystem::u8path(missingPath), root / "moved-source.json");
        Sleep(1100);
        render(target);
        check(readText(target, 0).empty() && readText(target, recoveryIndex) == missingPath,
              "runtime file disappearance resets native field on periodic check");
        check(entry(FF_DEINSTANTIATE_GL, empty, target).UIntValue == FF_SUCCESS,
              "destroy available reference");
        auto orderFolder = root / "order-check";
        std::filesystem::create_directories(orderFolder);
        for (const char *name : {"A.json", "B.json", "C.json"})
            std::ofstream(orderFolder / name) << "{}";
        lottie::Playlist order(42);
        order.setFolder(orderFolder.u8string());
        order.setRandom(true);
        std::set<std::string> visited;
        for (int n = 0; n < 3; ++n) {
            order.setFolder(orderFolder.u8string(), true);
            visited.insert(order.next());
        }
        check(visited.size() == 3,
              "activation rescan preserves shuffle bag when folder is unchanged");
        order.setRandom(false);
        auto firstPath = (orderFolder / "A.json").u8string();
        auto lastPath = (orderFolder / "C.json").u8string();
        order.select(firstPath);
        check(order.previous() == lastPath, "sequential Previous wraps to last file");
        check(order.next() == firstPath, "sequential Next wraps to first file");
        order.setRandom(true);
        auto randomB = order.next();
        auto randomC = order.next();
        check(order.previous() == randomB, "random Previous follows actual history");
        check(order.previous() == firstPath, "random Previous reaches selected anchor");
        check(order.previous() == firstPath, "random Previous stops at history beginning");
        check(order.next() == randomB && order.next() == randomC,
              "random Next retraces history after Previous");
        auto singleFolder = root / "single";
        std::filesystem::create_directories(singleFolder);
        auto singlePath = (singleFolder / "A.json").u8string();
        std::filesystem::copy_file(root / "A.json", singleFolder / "A.json");
        order.setFolder(singleFolder.u8string());
        check(order.next() == singlePath && order.previous() == singlePath,
              "single-item navigation returns the same asset");
        // Exercise real adapter reset/source selection, including the empty-File demo.
        for (const auto &path : {singlePath, std::string{}}) {
            set(id, enabled, 0);
            file(id, path.c_str());
            for (int n = 0; n < 100; ++n) {
                render(id);
                Sleep(5);
            }
            set(id, enabled, 1);
            render(id);
            for (unsigned actual : {isEmitter ? 37u : 38u, isEmitter ? 40u : 41u}) {
                SetParameterStruct navigation{};
                navigation.ParameterNumber = actual;
                float one = 1;
                std::memcpy(&navigation.NewParameterValue.UIntValue, &one, 4);
                FFMixed nav{};
                nav.PointerValue = &navigation;
                if (!isEmitter)
                    set(id, 4, .6f);
                check(entry(FF_SET_PARAMETER, nav, id).UIntValue == FF_SUCCESS, "navigation");
                render(id);
                if (!isEmitter) {
                    FFMixed query{};
                    query.UIntValue = 5;
                    auto value = entry(FF_GET_PARAMETER, query, id);
                    float phase;
                    std::memcpy(&phase, &value.UIntValue, 4);
                    check(phase == 0, "Next/Previous reset singleton and embedded demo");
                }
            }
            check(readText(id, 0) == path, "singleton/demo navigation retains File reference");
        }
        if (isEmitter) {
            FFMixed instantiate{};
            instantiate.PointerValue = &vp;
            auto singleton = entry(FF_INSTANTIATE_GL, instantiate, nullptr).PointerValue;
            check(singleton && singleton != reinterpret_cast<void *>(uintptr_t(FF_FAIL)),
                  "singleton instantiate");
            set(singleton, 1, 0); // No automatic spawn.
            set(singleton, 3, 0); // Freeze instances at their visible first frame.
            file(singleton, singlePath.c_str());
            for (int n = 0; n < 100; ++n) {
                render(singleton);
                Sleep(5);
            }
            set(singleton, enabled, 1);
            set(singleton, 34, 1); // Spawn on Change.
            render(singleton);
            auto singletonOpacity = [&](float percent) {
                SetParameterStruct p{};
                p.ParameterNumber = 4;
                std::memcpy(&p.NewParameterValue.UIntValue, &percent, 4);
                FFMixed value{};
                value.PointerValue = &p;
                check(entry(FF_SET_PARAMETER, value, singleton).UIntValue == FF_SUCCESS,
                      "singleton Opacity");
            };
            singletonOpacity(50);
            set(singleton, next, 1);
            singletonOpacity(100);
            render(singleton);
            check(sample(60, 90)[3] >= 63 && sample(60, 90)[3] <= 65,
                  "Spawn on Change captures Opacity at Next callback");
            set(singleton, 5, 1);
            render(singleton);
            check(sample(60, 90)[3] == 0, "Clear removes singleton instances");
            set(singleton, 38, 1); // Appended Previous parameter (actual 39).
            render(singleton);
            check(sample(60, 90)[3] > 100, "single-file Previous spawns an instance");
            // A short disconnect must clear live and queued instances, even
            // with speed zero (they cannot expire naturally).
            FFMixed lifecycle{};
            check(entry(FF_CONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "connect singleton");
            render(singleton);
            set(singleton, 2, 1); // Unprocessed Spawn must also be discarded.
            check(entry(FF_DISCONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "disconnect singleton");
            Sleep(1000);
            set(singleton, 33, 0); // Keep the current file on reactivation.
            check(entry(FF_CONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "reconnect singleton");
            render(singleton);
            check(mass().first == 0,
                  "short reactivation with auto spawn off clears old and queued instances");
            set(singleton, 1, 1);
            check(entry(FF_DISCONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "disconnect for auto spawn");
            check(entry(FF_CONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "reconnect for auto spawn");
            render(singleton);
            check(sample(60, 90)[3] > 100,
                  "reactivation with auto spawn on launches a fresh instance");
            check(entry(FF_CONNECT, lifecycle, singleton).UIntValue == FF_SUCCESS,
                  "duplicate connect");
            render(singleton);
            set(singleton, 5, 1);
            render(singleton);
            set(singleton, 34, 0);
            set(singleton, 38, 1);
            render(singleton);
            check(sample(60, 90)[3] == 0, "Spawn on Change off prevents navigation spawn");
            check(entry(FF_DEINSTANTIATE_GL, empty, singleton).UIntValue == FF_SUCCESS,
                  "singleton destroy");
        }
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
        std::cout << "Integrated playlists and independent source lifetimes passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
#include "../deps/ffgl/source/lib/glsdk_0_5_2/glload/source/gl_load.c"
