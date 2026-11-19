// clang-format off
#include "NativeFilePicker.hpp"
// clang-format on
#include "EmbeddedDemo.hpp"
#include "GLState.hpp"
#include "PaletteControls.hpp"
#include "Playlist.hpp"
#include "Thumbnail.hpp"
#include "Version.hpp"
#include <lottie/Core.hpp>
#ifdef LOTTIE_GPU_PLUGIN
#include "GLStateGPU.hpp"
#include <lottie/GpuRenderer.hpp>
using ActiveRenderer = lottie::GpuRenderer;
#else
using ActiveRenderer = lottie::Renderer;
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <lottie/Core.hpp>
#include <mutex>
#include <thread>

enum Parameter {
    File,
    Reload,
    Play,
    Loop,
    Speed,
    Position,
    Restart,
    Mode,
    Fitting,
    PosX,
    PosY,
    Scale,
    ScaleX,
    ScaleY,
    Rotation,
    AnchorX,
    AnchorY,
    ClipCanvas,
    PaletteStart,
    PlaylistEnabled = PaletteStart + 17,
    PlaylistRandom,
    PlaylistOnStart,
    PlaylistNext,
    ChooseFile,
    RecoveryPath,
    PlaylistPrevious,
    Count
};
class LottiePlugin final : public CFFGLPlugin {
    ffglex::FFGLShader shader;
    ffglex::FFGLScreenQuad quad;
    GLuint texture = 0;
#ifdef LOTTIE_GPU_PLUGIN
    GLuint framebuffer = 0;
#endif
    GLint image = -1;
    uint32_t textureW = 0, textureH = 0;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool stop = false, requested = false, ready = false;
    uint64_t generation = 0, pendingGeneration = 0;
    std::string file, pendingError;
    std::string recoveryPath;
    bool fileMissing = false, restorePending = false;
    std::chrono::steady_clock::time_point nextFileCheck{};
    // Called with the parameter mutex held. Only the presentation is cleared;
    // the internal source anchor remains intact for Reload and live instances.
    void updateFilePresentation(bool notify = false) {
        bool missing = lottie_ui::missingFile(file);
        auto backup = missing ? file : std::string{};
        if (backup != recoveryPath) {
            recoveryPath = std::move(backup);
            RaiseParamEvent(RecoveryPath, lottie_ui::valueChanged);
        }
        if (missing != fileMissing || notify)
            RaiseParamEvent(File, lottie_ui::valueChanged);
        fileMissing = missing;
    }
    void refreshFileReference() {
        std::string restore;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (restorePending) {
                restorePending = false;
                if (file.empty())
                    restore = recoveryPath;
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= nextFileCheck) {
                nextFileCheck = now + std::chrono::seconds(1);
                if (restore.empty())
                    updateFilePresentation();
            }
        }
        if (!restore.empty())
            requestFile(restore.c_str(), false);
    }

    std::array<float, Count> params{{0, 1, 1, 1, 0, 0, 0, 0, 0, 100, 100, 100, 0, 0, 0, 0}};
    std::unique_ptr<ActiveRenderer> pending, renderer;
    lottie::Playlist playlist;
    bool playlistDirty = true, playlistStart = false, connected = false;
    std::deque<bool> playlistSteps;
    bool reloadAtStart = false;
    uint64_t appliedGeneration = 0;
    std::string currentPath;
    bool positionResetPending = false;
    float bufferedHostPosition = 0;
    std::chrono::steady_clock::time_point positionResetDeadline{};
    void expirePositionReset() {
        if (positionResetPending && std::chrono::steady_clock::now() >= positionResetDeadline) {
            positionResetPending = false;
            params[Position] = bufferedHostPosition;
        }
    }

    void playlistVisibility(bool enabled) {
        for (unsigned i : {PlaylistNext, PlaylistPrevious, PlaylistRandom, PlaylistOnStart}) {
            SetParamVisibility(i, enabled);
            RaiseParamEvent(i, FF_EVENT_FLAG_VISIBILITY);
        }
    }
    bool selectPath(const std::string &path) {
        try {
            auto next = std::make_unique<ActiveRenderer>(embedded_demo::load(path));
#ifdef LOTTIE_GPU_PLUGIN
            if (renderer)
                renderer->releaseGL();
#endif
            renderer = std::move(next);
            currentPath = path;
            return true;
        } catch (const std::exception &e) {
            FFGLLog::LogToHost(e.what());
            return false;
        }
    }
    void loadLoop() {
        for (;;) {
            std::string path;
            uint64_t version;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&] { return stop || requested; });
                if (stop)
                    return;
                path = file;
                version = generation;
                requested = false;
            }
            std::unique_ptr<ActiveRenderer> result;
            std::string error;
            try {
                result = std::make_unique<ActiveRenderer>(embedded_demo::load(path));
            } catch (const std::exception &e) {
                error = e.what();
            } catch (...) {
                error = "Unknown Lottie load error";
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (version == generation) {
                    pending = std::move(result);
                    pendingError = std::move(error);
                    pendingGeneration = version;
                    ready = true;
                }
            }
        }
    }

  public:
    LottiePlugin() {
        params.fill(0);
        params[Play] = params[Loop] = params[Speed] = 1;
        m_isConnected = false;
        SetMinInputs(0);
        SetMaxInputs(0);
        SetTimeSupported(false); // SetTime is not a clip playhead API.
        SetFileParamInfo(File, "File", {"json"}, "");
        SetParamInfo(Reload, "Reload", FF_TYPE_EVENT, 0.0f);
        SetParamVisibility(Reload, false);
        SetParamInfo(Play, "Play", FF_TYPE_BOOLEAN, true);
        SetParamInfo(Loop, "Loop", FF_TYPE_BOOLEAN, true);
        SetParamInfo(Speed, "Speed", FF_TYPE_STANDARD, 1.0f);
        SetParamRange(Speed, -4, 4);
        SetParamInfo(Position, "Position", FF_TYPE_STANDARD, 0.0f);
        SetParamInfo(Restart, "Restart", FF_TYPE_EVENT, 0.0f);
        SetOptionParamInfo(Mode, "Playback Mode", 2, 0);
        SetParamElementInfo(Mode, 0, "Auto", 0);
        SetParamElementInfo(Mode, 1, "Manual", 1);
        // Keep indices for compositions saved with v0.1. Playback now comes entirely
        // from Resolume's animation of Position; retired controls only round-trip.
        for (unsigned index : {Play, Loop, Speed, Restart, Mode})
            SetParamVisibility(index, false);
        auto parameter = [&](unsigned index, const char *name, float initial, float low,
                             float high) {
            SetParamInfo(index, name, FF_TYPE_STANDARD, initial);
            SetParamRange(index, low, high);
            // SDK v2.2 SetParamInfo clamps STANDARD defaults to [0,1], even for
            // custom ranges. Restore our actual-unit default via its protected metadata API.
            FindParamInfo(index)->defaultFloatVal = initial;
            params[index] = initial;
            SetParamGroup(index, "Lottie Transform");
        };
        SetOptionParamInfo(Fitting, "Fit Mode", 4, 0);
        SetParamElementInfo(Fitting, 0, "Fit", 0);
        SetParamElementInfo(Fitting, 1, "Fill", 1);
        SetParamElementInfo(Fitting, 2, "Stretch", 2);
        SetParamElementInfo(Fitting, 3, "Original", 3);
        SetParamGroup(Fitting, "Lottie Transform");
        parameter(PosX, "Position X", 0, -16384, 16384);
        parameter(PosY, "Position Y", 0, -16384, 16384);
        parameter(Scale, "Scale", 100, 0, 1000);
        parameter(ScaleX, "Scale X", 100, -4000, 4000);
        parameter(ScaleY, "Scale Y", 100, -4000, 4000);
        SetParamVisibility(ScaleX, false);
        SetParamVisibility(ScaleY, false);
        parameter(Rotation, "Rotation", 0, -180, 180);
        parameter(AnchorX, "Anchor X", 0, -16384, 16384);
        parameter(AnchorY, "Anchor Y", 0, -16384, 16384);
        SetParamInfo(ClipCanvas, "Crop to Canvas", FF_TYPE_BOOLEAN, false);
        SetParamGroup(ClipCanvas, "Lottie Transform");
        params[ClipCanvas] = 0;
        palette_controls::setup(PaletteStart,
                                [&](unsigned index, const char *name, unsigned type, float value) {
                                    SetParamInfo(index, name, type, value);
                                    SetParamGroup(index, "Lottie Colors");
                                    params[index] = value;
                                });
        SetParamVisibility(PaletteStart, false); // Retired toggle; color edits apply automatically.
        SetParamInfo(PlaylistEnabled, "Playlist", FF_TYPE_BOOLEAN, false);
        SetParamInfo(PlaylistRandom, "Random", FF_TYPE_BOOLEAN, false);
        SetParamInfo(PlaylistOnStart, "Next on Start", FF_TYPE_BOOLEAN, true);
        params[PlaylistOnStart] = 1;
        SetParamInfo(PlaylistNext, "Next", FF_TYPE_EVENT, 0.0f);
        SetParamInfo(PlaylistPrevious, "Previous", FF_TYPE_EVENT, 0.0f);
        SetParamInfo(ChooseFile, "Replace File", FF_TYPE_EVENT, 0.0f);
        SetParamVisibility(ChooseFile, false); // Retained only for older assignments.
        SetParamInfo(RecoveryPath, "Recovery Path", FF_TYPE_TEXT, "");
        SetParamVisibility(RecoveryPath, false);
        playlistVisibility(false);
        requestFile("", true);
    }
    ~LottiePlugin() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        cv.notify_one();
        if (worker.joinable())
            worker.join();
    }
    unsigned int Connect() override {
        std::lock_guard<std::mutex> lock(mutex);
        if (!connected) {
            playlistStart = params[PlaylistEnabled] > .5f && params[PlaylistOnStart] > .5f;
            reloadAtStart = !file.empty() && generation > 0 && appliedGeneration == generation &&
                            (!playlistStart || playlistDirty);
        }
        connected = true;
        return FF_SUCCESS;
    }
    unsigned int Disconnect() override {
        std::lock_guard<std::mutex> lock(mutex);
        connected = false;
        playlistStart = false;
        reloadAtStart = false;
        return FF_SUCCESS;
    }
    FFResult SetTextParameter(unsigned index, const char *value) override {
        if (index == RecoveryPath) {
            if (!value)
                return FF_FAIL;
            std::lock_guard<std::mutex> lock(mutex);
            if (!file.empty()) {
                if (recoveryPath != value)
                    RaiseParamEvent(RecoveryPath, lottie_ui::valueChanged);
                return FF_SUCCESS; // Explicit File wins over a stale hidden value.
            }
            recoveryPath = value;
            restorePending = !recoveryPath.empty();
            return FF_SUCCESS;
        }
        if (index != File)
            return FF_FAIL;
        return requestFile(value, false);
    }
    FFResult requestFile(const char *value, bool forceReload, bool notifyHost = false) {
        if (!value)
            return FF_FAIL;
        try {
            {
                std::lock_guard<std::mutex> lock(mutex);
                // Empty acknowledgements of our native-field reset must not
                // erase the saved missing reference during host restoration.
                if (!*value && (fileMissing || restorePending))
                    return FF_SUCCESS;
                // A VALUE notification may be echoed by the host. Reload is
                // the explicit force-load action; same-path echoes are inert.
                if (!forceReload && !notifyHost && !fileMissing && file == value)
                    return FF_SUCCESS;
                bool changed = file != value;
                file = value;
                restorePending = false;
                updateFilePresentation(changed || notifyHost);
                playlistDirty = true;
                reloadAtStart = false;
                playlistSteps.clear();
                playlistStart = false;
                ++generation;
                requested = true;
                ready = false;
                if (!worker.joinable())
                    worker = std::thread(&LottiePlugin::loadLoop, this);
            }
            cv.notify_one();
            return FF_SUCCESS;
        } catch (...) {
            return FF_FAIL;
        }
    }
    char *GetTextParameter(unsigned index) override {
        if (index != File && index != RecoveryPath)
            return nullptr;
        thread_local std::string copy;
        std::lock_guard<std::mutex> lock(mutex);
        if (index == File && !restorePending && std::chrono::steady_clock::now() >= nextFileCheck) {
            nextFileCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            updateFilePresentation();
        }
        copy = index == RecoveryPath ? recoveryPath : (fileMissing ? std::string{} : file);
        return copy.data();
    }
    FFResult SetFloatParameter(unsigned index, float value) override {
        if (index == ChooseFile) {
            if (!std::isfinite(value))
                return FF_FAIL;
            if (value <= .5f)
                return FF_SUCCESS;
            try {
                std::string previous;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    previous = file;
                }
                auto selected = lottie_ui::pickJson(previous);
                return selected.empty() ? FF_SUCCESS : requestFile(selected.c_str(), false, true);
            } catch (const std::exception &e) {
                FFGLLog::LogToHost(e.what());
                return FF_FAIL;
            }
        }
        if (index == Reload) {
            if (!std::isfinite(value))
                return FF_FAIL;
            if (value <= .5f)
                return FF_SUCCESS;
            std::string path;
            {
                std::lock_guard<std::mutex> lock(mutex);
                path = file;
            }
            return requestFile(path.c_str(), true);
        }
        if (index == File || index == RecoveryPath || index >= Count || !std::isfinite(value))
            return FF_FAIL;
        std::lock_guard<std::mutex> lock(mutex);
        if (index == Position) {
            expirePositionReset();
            if (positionResetPending) {
                bufferedHostPosition = std::clamp(value, 0.f, 1.f);
                return FF_SUCCESS;
            }
        }
        if (index == PlaylistNext || index == PlaylistPrevious) {
            if (value > .5f && params[PlaylistEnabled] > .5f) {
                if (playlistSteps.size() < 64)
                    playlistSteps.push_back(index == PlaylistPrevious);
                playlistStart = false;
            }
            return FF_SUCCESS;
        }
        if (index == PlaylistEnabled && (value > .5f) != (params[index] > .5f)) {
            playlistDirty = true;
            playlistSteps.clear();
            playlistStart = false;
            playlistVisibility(value > .5f);
        }
        if (index == PlaylistOnStart && value <= .5f)
            playlistStart = false;
        if (index == PosX || index == PosY || index == AnchorX || index == AnchorY)
            value = std::clamp(value, -16384.0f, 16384.0f);
        else if (index == Scale)
            value = std::clamp(value, 0.0f, 1000.0f);
        else if (index == ScaleX || index == ScaleY)
            value = std::clamp(value, -4000.0f, 4000.0f);
        else if (index == Rotation)
            value = std::clamp(value, -180.0f, 180.0f);
        else if (index == Fitting)
            value = std::round(std::clamp(value, 0.0f, 3.0f));
        else if (index == Speed)
            value = std::clamp(value, -4.0f, 4.0f);
        else
            value = std::clamp(value, 0.0f, 1.0f);
        params[index] = index == Restart ? 0 : value;
        return FF_SUCCESS;
    }
    float GetFloatParameter(unsigned index) override {
        std::lock_guard<std::mutex> lock(mutex);
        if (index == Position) {
            expirePositionReset();
            if (positionResetPending) {
                positionResetPending = false;
                return 0;
            }
        }
        return index < Count ? params[index] : 0;
    }
    char *GetParameterDisplay(unsigned index) override {
        if (index < PosX || index > AnchorY)
            return CFFGLPlugin::GetParameterDisplay(index);
        thread_local char buffer[32];
        const float value = GetFloatParameter(index);
        std::snprintf(buffer, sizeof(buffer),
                      index == Scale || index == ScaleX || index == ScaleY ? "%.1f%%"
                      : index == Rotation                                  ? "%.1f deg"
                                                                           : "%.1f px",
                      value);
        return buffer;
    }
    const char *GetShortName() override {
#ifdef LOTTIE_GPU_PLUGIN
        return "Lottie GPU";
#else
        return "Lottie CPU";
#endif
    }
    FFResult InitGL(const FFGLViewportStruct *vp) override {
#ifdef LOTTIE_GPU_PLUGIN
        GLStateGPU state;
#else
        GLState state;
#endif
        const char *vs =
            "#version 410 core\nlayout(location=0) in vec4 vPosition;layout(location=1) in vec2 "
            "vUV;out vec2 uv;void main(){gl_Position=vPosition;uv=vUV;}";
        const char *fs = "#version 410 core\nin vec2 uv;out vec4 fragColor;uniform sampler2D "
                         "image;void main(){fragColor=texture(image,uv);}";
        if (!shader.Compile(vs, fs) || !quad.Initialise(
#ifdef LOTTIE_GPU_PLUGIN
                                           false
#else
                                           true
#endif
                                           )) {
            DeInitGL();
            return FF_FAIL;
        }
        image = shader.FindUniform("image");
#ifdef LOTTIE_GPU_PLUGIN
        glGenFramebuffers(1, &framebuffer);
#endif
        glGenTextures(1, &texture);
        if (!texture) {
            DeInitGL();
            return FF_FAIL;
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return CFFGLPlugin::InitGL(vp);
    }
    FFResult ProcessOpenGL(ProcessOpenGLStruct *) override {
        try {
            refreshFileReference();
#ifdef LOTTIE_GPU_PLUGIN
            GLStateGPU gpuState;
            gpuState.prepare();
#endif
            std::array<float, Count> values;
            std::unique_ptr<ActiveRenderer> next;
            bool replace = false;
            std::string error;
            {
                std::lock_guard<std::mutex> lock(mutex);
                values = params;
                if (ready && pendingGeneration == generation) {
                    next = std::move(pending);
                    error = std::move(pendingError);
                    ready = false;
                    replace = true;
                    appliedGeneration = pendingGeneration;
                    currentPath = file;
                }
            }
            if (replace) {
                renderer = std::move(next);
                if (!error.empty())
                    FFGLLog::LogToHost(error.c_str());
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (appliedGeneration == generation) {
                    if (reloadAtStart) {
                        reloadAtStart = false;
                        if (selectPath(file))
                            updateFilePresentation();
                        if (params[PlaylistEnabled] > .5f && !file.empty()) {
                            try {
                                playlist.setFolder(
                                    std::filesystem::absolute(std::filesystem::u8path(file))
                                        .parent_path()
                                        .u8string(),
                                    true);
                            } catch (const std::exception &e) {
                                FFGLLog::LogToHost(e.what());
                            }
                        }
                    }
                    if (playlistDirty) {
                        playlistDirty = false;
                        playlistStart = false;
                        try {
                            playlist.setFolder("");
                            if (params[PlaylistEnabled] > .5f && !file.empty()) {
                                playlist.setFolder(
                                    std::filesystem::absolute(std::filesystem::u8path(file))
                                        .parent_path()
                                        .u8string());
                                playlist.select(file);
                            }
                            if (currentPath != file)
                                selectPath(file);
                        } catch (const std::exception &e) {
                            FFGLLog::LogToHost(e.what());
                        }
                    }
                    playlist.setRandom(params[PlaylistRandom] > .5f);
                    if (playlistStart) {
                        playlistStart = false;
                        try {
                            if (!file.empty())
                                playlist.setFolder(
                                    std::filesystem::absolute(std::filesystem::u8path(file))
                                        .parent_path()
                                        .u8string(),
                                    true);
                            if (playlistSteps.size() < 64)
                                playlistSteps.push_back(false);
                        } catch (const std::exception &e) {
                            FFGLLog::LogToHost(e.what());
                        }
                    }
                    if (!playlistSteps.empty() && params[PlaylistEnabled] > .5f) {
                        for (bool backwards : playlistSteps) {
                            for (size_t attempt = 0; attempt < std::max(size_t(1), playlist.size());
                                 ++attempt)
                                if (selectPath(playlist.size() ? (backwards ? playlist.previous()
                                                                            : playlist.next())
                                                               : file)) {
                                    bool fileChanged = file != currentPath;
                                    file = currentPath;
                                    updateFilePresentation(fileChanged);
                                    // Experiment: ask the host to pick up a new playhead
                                    // value. Whether Timeline rephases is host-owned.
                                    if (!positionResetPending)
                                        bufferedHostPosition = params[Position];
                                    positionResetPending = true;
                                    positionResetDeadline = std::chrono::steady_clock::now() +
                                                            std::chrono::milliseconds(250);
                                    params[Position] = 0;
                                    RaiseParamEvent(Position, lottie_ui::valueChanged);
                                    break;
                                }
                        }
                    }
                    playlistSteps.clear();
                }
                expirePositionReset();
                values = params;
            }
            if (renderer)
                renderer->setPalette(palette_controls::read(values, PaletteStart));
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT, vp);
            if (vp[2] <= 0 || vp[3] <= 0)
                return FF_SUCCESS;
            lottie::Transform transform;
            transform.x = values[PosX];
            transform.y = values[PosY];
            transform.scale = values[Scale] / 100;
            // Retired axis parameters round-trip old presets but no longer affect rendering.
            transform.rotation = values[Rotation];
            transform.anchorX = values[AnchorX];
            transform.anchorY = values[AnchorY];
            transform.fit = static_cast<lottie::Fit>(int(values[Fitting]));
            transform.clipToCanvas = values[ClipCanvas] > .5f;
#ifdef LOTTIE_GPU_PLUGIN
            unsigned w = uint32_t(vp[2]), h = uint32_t(vp[3]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            if (w != textureW || h != textureH) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             nullptr);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture,
                                       0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("GPU framebuffer incomplete");
                textureW = w;
                textureH = h;
            }
            if (renderer)
                renderer->render(wglGetCurrentContext(), framebuffer,
                                 values[Position] * renderer->metadata().duration, w, h, transform);
            else {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glViewport(0, 0, w, h);
                glDisable(GL_SCISSOR_TEST);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            gpuState.prepare();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gpuState.drawFbo);
            glViewport(vp[0], vp[1], vp[2], vp[3]);
            glBindTexture(GL_TEXTURE_2D, texture);
#else
            const lottie::Frame *frame =
                renderer ? &renderer->render(values[Position] * renderer->metadata().duration,
                                             uint32_t(vp[2]), uint32_t(vp[3]), transform)
                         : nullptr;
            uint32_t clear = 0;
            unsigned w = frame ? frame->width : 1, h = frame ? frame->height : 1;
            const void *pixels = frame ? frame->pixels.data() : &clear;
            GLState state;
            state.prepare();
            glBindTexture(GL_TEXTURE_2D, texture);
            if (w != textureW || h != textureH) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels);
                textureW = w;
                textureH = h;
            } else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#endif
            glUseProgram(shader.GetGLID());
            glUniform1i(image, 0);
            quad.Draw();
            return FF_SUCCESS;
        } catch (const std::exception &e) {
            FFGLLog::LogToHost(e.what());
            return FF_FAIL;
        } catch (...) {
            return FF_FAIL;
        }
    }
    FFResult DeInitGL() override {
#ifdef LOTTIE_GPU_PLUGIN
        GLStateGPU state;
        if (renderer)
            renderer->releaseGL();
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
#endif
        if (texture)
            glDeleteTextures(1, &texture);
        texture = 0;
        textureW = textureH = 0;
        quad.Release();
        shader.FreeGLResources();
        return FF_SUCCESS;
    }
};
#ifdef LOTTIE_GPU_PLUGIN
static const char *rendererDescription = "Lottie ThorVG OpenGL GPU renderer";
#else
static const char *rendererDescription = "Lottie ThorVG CPU renderer";
#endif
static CFFGLPluginInfo info(PluginFactory<LottiePlugin>, "LT01", "Lottie Player", 2, 2,
                            lottie::release::major, lottie::release::minor, FF_SOURCE,
                            rendererDescription, "Lottie Resolume");
