// clang-format off
#include <windows.h>
// clang-format on
#include <array>
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
        HMODULE lib = LoadLibraryA(argv[1]);
        check(lib, "LoadLibrary");
        auto entry = reinterpret_cast<Entry>(GetProcAddress(lib, "plugMain"));
        check(entry, "plugMain");
        FFMixed empty{};
        check(entry(FF_INITIALISE_V2, empty, nullptr).UIntValue == FF_SUCCESS, "initialise");
        FFGLViewportStruct vp{0, 0, 320, 180};
        FFMixed input{};
        input.PointerValue = &vp;
        auto id = entry(FF_INSTANTIATE_GL, input, nullptr).PointerValue;
        check(id && id != reinterpret_cast<void *>(uintptr_t(FF_FAIL)), "instantiate");

        auto setFloat = [&](void *instance, unsigned parameter, float value) {
            SetParameterStruct param{};
            if (parameter == 15)
                parameter = 7;
            else if (parameter >= 7 && parameter < 15)
                ++parameter;
            if (parameter == 33)
                parameter = 16;
            else if (parameter >= 16 && parameter < 33)
                ++parameter;
            if (parameter)
                ++parameter; // Reload inserted immediately after File.
            param.ParameterNumber = parameter;
            std::memcpy(&param.NewParameterValue.UIntValue, &value, sizeof(value));
            FFMixed v{};
            v.PointerValue = &param;
            check(entry(FF_SET_PARAMETER, v, instance).UIntValue == FF_SUCCESS, "float parameter");
        };
        auto setFile = [&](void *instance, const char *path) {
            SetParameterStruct param{};
            param.ParameterNumber = 0;
            param.NewParameterValue.PointerValue = const_cast<char *>(path);
            FFMixed v{};
            v.PointerValue = &param;
            check(entry(FF_SET_PARAMETER, v, instance).UIntValue == FF_SUCCESS, "file parameter");
        };
        check(argc >= 3, "Usage: ffgl-smoke dll fixture.json");
        check(entry(FF_GET_NUM_PARAMETERS, empty, nullptr).UIntValue == 42, "parameter count");
        FFMixed type{};
        type.UIntValue = 0;
        check(entry(FF_GET_PARAMETER_TYPE, type, nullptr).UIntValue == FF_TYPE_FILE, "file type");
        type.UIntValue = 6;
        check(entry(FF_GET_PARAMETER_TYPE, type, nullptr).UIntValue == FF_TYPE_EVENT, "event type");
        for (unsigned parameter = 0; parameter < 16; ++parameter) {
            FFMixed number{};
            number.UIntValue = parameter == 15
                                   ? 7
                                   : (parameter >= 7 && parameter < 15 ? parameter + 1 : parameter);
            if (number.UIntValue)
                ++number.UIntValue;
            check(entry(FF_GET_PRAMETER_VISIBILITY, number, id).UIntValue ==
                      ((parameter == 0 || parameter == 4 ||
                        (parameter >= 7 && parameter != 10 && parameter != 11))
                           ? FF_TRUE
                           : FF_FALSE),
                  "visible controls");
        }
        for (unsigned parameter : {9u, 10u, 11u}) {
            FFMixed number{};
            number.UIntValue = parameter == 15
                                   ? 7
                                   : (parameter >= 7 && parameter < 15 ? parameter + 1 : parameter);
            if (number.UIntValue)
                ++number.UIntValue;
            auto value = entry(FF_GET_PARAMETER_DEFAULT, number, nullptr);
            float actual;
            std::memcpy(&actual, &value.UIntValue, 4);
            check(actual == 100, "scale default must be 100 percent");
        }
        auto range = [&](unsigned index, float low, float high) {
            GetRangeStruct query{};
            query.parameterNumber = (index >= 7 && index < 15 ? index + 1 : index) + 1;
            FFMixed argument{};
            argument.PointerValue = &query;
            check(entry(FF_GET_RANGE, argument, id).UIntValue == FF_SUCCESS, "get range");
            check(query.range.min == low && query.range.max == high, "transform UI range");
            setFloat(id, index, high * 2);
            FFMixed number{};
            number.UIntValue = (index >= 7 && index < 15 ? index + 1 : index) + 1;
            auto result = entry(FF_GET_PARAMETER, number, id);
            float actual;
            std::memcpy(&actual, &result.UIntValue, 4);
            check(actual == high, "range clamp");
            setFloat(id, index, index == 9 ? 100 : 0);
        };
        range(7, -16384, 16384);
        range(8, -16384, 16384);
        range(9, 0, 1000);
        range(12, -180, 180);
        range(13, -16384, 16384);
        range(14, -16384, 16384);
        // v0.1 hidden values can still be restored without affecting playback.
        setFloat(id, 1, 0);
        setFloat(id, 2, 0);
        setFloat(id, 3, -4);
        setFloat(id, 6, 0);
        setFloat(id, 4, .5f);
        setFile(id, argv[2]);
        glViewport(0, 0, 320, 180);
        glDisable(GL_BLEND);
        ProcessOpenGLStruct process{};
        process.HostFBO = hostFbo;
        auto render = [&](void *instance) {
            FFMixed v{};
            v.PointerValue = &process;
            check(entry(FF_PROCESS_OPENGL, v, instance).UIntValue == FF_SUCCESS, "process");
        };
        std::vector<unsigned char> waitPixels(320 * 180 * 4);
        bool loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            render(id);
            glReadPixels(0, 0, 320, 180, GL_RGBA, GL_UNSIGNED_BYTE, waitPixels.data());
            if (waitPixels[(90 * 320 + 160) * 4 + 3] > 0) {
                loaded = true;
                break;
            }
            Sleep(10);
        }
        check(loaded, "asynchronous load timed out");
        auto sample = [&](int x, int y) {
            std::array<unsigned char, 4> rgba{};
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            return rgba;
        };
        auto premul = sample(160, 90);
        check(premul[0] >= 126 && premul[0] <= 129 && premul[2] == 0, "premultiplied output color");
        setFloat(id, 4, 0);
        render(id);
        check(sample(60, 90)[3] > 0 && sample(260, 90)[3] == 0, "Position start");
        auto still = sample(60, 90);
        Sleep(100);
        render(id);
        check(sample(60, 90) == still, "Position must not advance on its own");
        setFloat(id, 4, 1);
        render(id);
        check(sample(260, 90)[3] > 0 && sample(60, 90)[3] == 0, "Position end");
        setFloat(id, 4, .5f);
        render(id);
        setFile(id, "missing-lottie-123456.json");
        bool cleared = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            render(id);
            if (sample(160, 90)[3] == 0) {
                cleared = true;
                break;
            }
            Sleep(10);
        }
        check(cleared, "missing file must clear output");
        // Rapid consecutive requests must not publish stale failed results.
        setFile(id, "another-missing-lottie.json");
        setFile(id, argv[2]);
        loaded = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            render(id);
            if (sample(160, 90)[3] > 0) {
                loaded = true;
                break;
            }
            Sleep(10);
        }
        check(loaded, "reload recovery");
        setFloat(id, 15, 3);
        glViewport(0, 0, 160, 90);
        render(id);
        check(sample(104, 45)[3] > 120 && sample(120, 45)[3] == 0,
              "Original retains native shape size on smaller viewport");
        setFloat(id, 15, 0);
        render(id);
        check(sample(104, 45)[3] == 0, "Fit scales down on smaller viewport");
        glViewport(0, 0, 320, 180);
        // New controls must transform the vector scene before GPU upload.
        setFloat(id, 7, 60);
        setFloat(id, 8, 30);
        render(id);
        check(sample(220, 120)[3] > 0 && sample(160, 90)[3] == 0, "GPU vector translation");
        setFloat(id, 7, 0);
        setFloat(id, 8, 0);
        setFloat(id, 9, 200);
        render(id);
        check(sample(210, 90)[3] > 0, "GPU vector scale");
        setFloat(id, 10, 100);
        setFloat(id, 11, 25);
        render(id);
        check(sample(160, 140)[3] > 0, "retired axis scale ignored");
        setFloat(id, 4, .25f);
        setFloat(id, 12, 90);
        render(id);
        check(sample(160, 140)[3] > 0 && sample(80, 90)[3] == 0, "GPU rotation");
        setFloat(id, 4, .5f);
        setFloat(id, 12, 0);
        setFloat(id, 11, 100);
        setFloat(id, 13, 40);
        render(id);
        check(sample(80, 90)[3] > 0 && sample(160, 90)[3] == 0, "GPU anchor");
        setFloat(id, 13, 0);
        setFloat(id, 9, 100);
        render(id);
        // Exercise hostile-but-valid GL state; the adapter must restore every changed binding.
        GLuint testTexture = 0, pbo = 0, sampler = 0;
        glGenTextures(1, &testTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, testTexture);
        glGenSamplers(1, &sampler);
        glBindSampler(0, sampler);
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, nullptr, GL_STATIC_DRAW);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        GLuint ubo = 0;
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
        glBindBufferRange(GL_UNIFORM_BUFFER, 2, ubo, 0, 64);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, testTexture);
        glBindSampler(1, sampler);
        glActiveTexture(GL_TEXTURE3);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_SCISSOR_TEST);
        glColorMask(false, true, false, true);
        render(id);
        GLint value = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
        check(value == GL_TEXTURE3, "active unit restore");
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &value);
        check(value == GLint(pbo), "PBO restore");
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
        check(value == 7, "row length restore");
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &value);
        check(value == 8, "alignment restore");
        check(glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_SCISSOR_TEST) &&
                  glIsEnabled(GL_CULL_FACE),
              "enable restore");
        GLboolean mask[4];
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        check(!mask[0] && mask[1] && !mask[2] && mask[3], "mask restore");
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 2, &value);
        check(value == GLint(ubo), "uniform binding restore");
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
        check(value == GLint(hostFbo), "host draw FBO restore");
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
        check(value == GLint(hostFbo), "host read FBO restore");
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        check(value == GLint(testTexture), "texture unit 1 restore");
        glGetIntegerv(GL_SAMPLER_BINDING, &value);
        check(value == GLint(sampler), "sampler unit 1 restore");
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(1, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, 0);
        glDeleteBuffers(1, &ubo);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(true, true, true, true);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        check(value == GLint(testTexture), "texture restore");
        glGetIntegerv(GL_SAMPLER_BINDING, &value);
        check(value == GLint(sampler), "sampler restore");
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(0, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glDeleteTextures(1, &testTexture);
        glDeleteBuffers(1, &pbo);
        glDeleteSamplers(1, &sampler);
        render(id);
        // Second instance remains transparent while the first has its own source.
        input.PointerValue = &vp;
        auto second = entry(FF_INSTANTIATE_GL, input, nullptr).PointerValue;
        check(second && second != reinterpret_cast<void *>(uintptr_t(FF_FAIL)), "second instance");
        render(second);
        unsigned char blank[4];
        glReadPixels(160, 90, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, blank);
        check(blank[3] == 0, "instance isolation");
        check(entry(FF_DEINSTANTIATE_GL, empty, second).UIntValue == FF_SUCCESS, "second destroy");
        render(id);

        std::vector<unsigned char> pixels(320 * 180 * 4);
        glReadPixels(0, 0, 320, 180, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        unsigned transparent = 0, half = 0;
        for (size_t i = 3; i < pixels.size(); i += 4) {
            if (pixels[i] == 0)
                ++transparent;
            if (pixels[i] >= 126 && pixels[i] <= 129)
                ++half;
        }
        check(transparent && half, "alpha readback");
        check(glGetError() == GL_NO_ERROR, "GL error");
        if (argc > 3) {
            auto waitAlpha = [&](int x, int y, bool visible) {
                for (int attempt = 0; attempt < 500; ++attempt) {
                    render(id);
                    if ((sample(x, y)[3] > 0) == visible)
                        return;
                    Sleep(10);
                }
                throw std::runtime_error("resource load timeout");
            };
            for (const char *fixture :
                 {"image-png-external.json", "image-png-embedded.json", "image-jpg-external.json",
                  "image-jpg-embedded.json", "image-webp-external.json", "image-webp-embedded.json",
                  "font-ttf-external.json", "font-ttf-embedded.json", "font-otf-external.json",
                  "font-otf-embedded.json"}) {
                setFile(id, "__missing_for_transparent_reset__.json");
                waitAlpha(160, 90, false);
                auto path = (std::filesystem::u8path(argv[3]) / fixture).u8string();
                setFloat(id, 4, 0);
                setFile(id, path.c_str());
                bool font = std::string(fixture).rfind("font-", 0) == 0;
                waitAlpha(font ? 56 : 160, font ? 79 : 104, true);
                auto pixel = sample(font ? 56 : 160, font ? 79 : 104);
                if (font)
                    check(pixel[1] > 200 && pixel[0] == 0, "GPU font color");
                else {
                    check(pixel[0] > 120 && pixel[2] < 5, "GPU image top");
                    check(sample(160, 75)[2] > 240, "GPU image bottom / orientation");
                }
            }
            setFile(id, "__missing_for_transparent_reset__.json");
            waitAlpha(56, 79, false);
            setFloat(id, 4, .5f);
            auto emptyFirst =
                (std::filesystem::u8path(argv[3]) / "empty-first-frame.json").u8string();
            setFile(id, emptyFirst.c_str());
            waitAlpha(160, 90, true);
            setFloat(id, 4, 0);
            render(id);
            check(sample(160, 90)[3] == 0, "empty first frame");
            glUseProgram(0);
            render(id);
            check(glGetError() == GL_NO_ERROR, "shared-context shader cache regression");
            setFloat(id, 4, .5f);
            render(id);
            check(sample(160, 90)[3] > 0, "visible after empty frame");
            setFile(id, "__missing_for_transparent_reset__.json");
            waitAlpha(80, 90, false);
            auto paletteFile = (std::filesystem::u8path(argv[3]) / "palette.json").u8string();
            setFile(id, paletteFile.c_str());
            waitAlpha(80, 90, true);
            auto paletteOriginal = sample(80, 90);
            check(paletteOriginal[0] > 230 && paletteOriginal[1] < 35, "original neon palette");
            for (unsigned c = 0; c < 4; ++c) {
                setFloat(id, 17 + c * 4, c == 3 ? 1.f / 6 : c / 3.f);
                setFloat(id, 18 + c * 4, 1);
                setFloat(id, 19 + c * 4, 1);
            }
            render(id);
            auto paletteFill = sample(80, 90);
            check(paletteFill[0] > 250 && paletteFill[1] == 0 && paletteFill[2] == 0,
                  "live palette fill on GPU/CPU");
            auto paletteGradient = sample(240, 90);
            check(paletteGradient[0] == 0 && paletteGradient[1] > 40 && paletteGradient[2] > 40 &&
                      paletteGradient[3] >= 126 && paletteGradient[3] <= 129,
                  "live palette gradient preserves opacity");
            auto paletteStroke = sample(132, 90);
            check(paletteStroke[0] > 200 && paletteStroke[1] > 200 && paletteStroke[2] == 0,
                  "live palette stroke");
            for (unsigned k = 0; k < 16; ++k) {
                FFMixed query{};
                query.UIntValue = 19 + k;
                auto original = entry(FF_GET_PARAMETER_DEFAULT, query, nullptr);
                float value;
                std::memcpy(&value, &original.UIntValue, sizeof(value));
                setFloat(id, 17 + k, value);
            }
            render(id);
            check(sample(80, 90) == paletteOriginal, "palette reset restores original colors");
            setFile(id, "__missing_for_transparent_reset__.json");
            waitAlpha(80, 90, false);
            auto fillEffectFile =
                (std::filesystem::u8path(argv[3]) / "fill-effect.json").u8string();
            setFile(id, fillEffectFile.c_str());
            waitAlpha(80, 90, true);
            check(sample(80, 90)[0] > 230 && sample(80, 90)[2] > 220, "original layer Fill color");
            setFloat(id, 17, 1.f / 3);
            setFloat(id, 18, 1);
            setFloat(id, 19, 1);
            setFloat(id, 20, .5f);
            render(id);
            auto fillEffectPixel = sample(80, 90);
            std::cout << "Fill RGBA " << int(fillEffectPixel[0]) << "," << int(fillEffectPixel[1])
                      << "," << int(fillEffectPixel[2]) << "," << int(fillEffectPixel[3]) << "\n";
            check(fillEffectPixel[0] == 0 && fillEffectPixel[1] >= 126 &&
                      fillEffectPixel[1] <= 129 && fillEffectPixel[3] == fillEffectPixel[1],
                  "GPU/CPU layer Fill palette and alpha");
            for (unsigned k = 0; k < 4; ++k) {
                FFMixed query{};
                query.UIntValue = 19 + k;
                auto original = entry(FF_GET_PARAMETER_DEFAULT, query, nullptr);
                float value;
                std::memcpy(&value, &original.UIntValue, 4);
                setFloat(id, 17 + k, value);
            }
            render(id);
            check(sample(80, 90)[0] > 230 && sample(80, 90)[2] > 220, "layer Fill palette reset");
            setFile(id, "__missing_for_transparent_reset__.json");
            waitAlpha(80, 90, false);
            auto overflowFile =
                (std::filesystem::u8path(argv[3]) / "canvas-overflow.json").u8string();
            setFloat(id, 15, 3);
            setFile(id, overflowFile.c_str());
            waitAlpha(205, 60, true);
            check(sample(230, 120)[3] > 240, "canvas clipping defaults off");
            setFloat(id, 33, 1);
            render(id);
            check(sample(230, 120)[3] == 0, "explicit canvas clipping enabled");
            setFloat(id, 33, 0);
            render(id);
            check(sample(230, 120)[3] > 240, "GPU/CPU canvas overflow revealed");
            check(sample(230, 90)[3] == 0 && sample(230, 60)[3] == 0 && sample(205, 60)[3] > 240,
                  "GPU/CPU layer mask and nested precomp preserved");
            render(id);
            check(sample(230, 120)[3] > 240, "overflow survives palette scene rebuild");
            setFloat(id, 33, 1);
            render(id);
            check(sample(230, 120)[3] == 0, "GPU/CPU clipping restored");
            std::cout << "GPU resource fixtures passed\n";
        }
        check(entry(FF_DEINSTANTIATE_GL, empty, id).UIntValue == FF_SUCCESS, "deinstantiate");
        entry(FF_DEINITIALISE, empty, nullptr);
        FreeLibrary(lib);
        glDeleteFramebuffers(1, &hostFbo);
        glDeleteTextures(1, &hostTexture);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(rc);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        std::cout << "FFGL smoke passed: transparent=" << transparent << " half-alpha=" << half
                  << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << " Win32=" << GetLastError() << "\n";
        return 1;
    }
}

#include "../deps/ffgl/source/lib/glsdk_0_5_2/glload/source/gl_load.c"
