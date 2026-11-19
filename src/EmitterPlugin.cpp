// clang-format off
#include "NativeFilePicker.hpp"
// clang-format on
#include "GLStateGPU.hpp"
#define LOTTIE_EMITTER_THUMBNAIL
#include "EmbeddedDemo.hpp"
#include "PaletteControls.hpp"
#include "Playlist.hpp"
#include "Thumbnail.hpp"
#include "Version.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <lottie/Emitter.hpp>
#include <lottie/GpuRenderer.hpp>
#include <mutex>
#include <thread>

enum Param {
    File,
    Reload,
    Trigger,
    Speed,
    Opacity,
    Limit,
    Clear,
    AutoSpawn,
    Fitting,
    PosX,
    PosY,
    Scale,
    Rotation,
    AnchorX,
    AnchorY,
    ClipCanvas,
    PaletteStart,
    PlaylistEnabled = PaletteStart + 17,
    PlaylistRandom,
    PlaylistOnStart,
    SpawnOnNext,
    PlaylistNext,
    ChooseFile,
    RecoveryPath,
    PlaylistPrevious,
    Count
};
class LottieEmitterPlugin final : public CFFGLPlugin {
    struct Target {
        GLuint texture = 0, fbo = 0;
        unsigned width = 0, height = 0;
        void create() {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &fbo);
            if (!texture || !fbo)
                throw std::runtime_error("Emitter target allocation failed");
        }
        void resize(unsigned w, unsigned h) {
            if (w == width && h == height)
                return;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Emitter framebuffer incomplete");
            width = w;
            height = h;
        }
        void release() {
            if (fbo)
                glDeleteFramebuffers(1, &fbo);
            if (texture)
                glDeleteTextures(1, &texture);
            fbo = texture = width = height = 0;
        }
    } scratch, output;
    ffglex::FFGLShader shader;
    ffglex::FFGLScreenQuad quad;
    GLint image = -1, opacityUniform = -1;
    lottie::Emitter emitter;
    std::vector<lottie::Palette> livePalettes;
    enum class Kind { Trigger, Speed, Limit, Clear, Reset, Next, NextOnStart, Previous };
    struct Command {
        Kind kind;
        float value;
        lottie::Emitter::Clock::time_point time;
        lottie::Transform transform;
        lottie::Palette palette;
        float opacity = 1;
    };
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool stop = false, requested = false, ready = false;
    bool connected = false, startPending = false, reloadAtStart = false;
    uint64_t generation = 0, pendingGeneration = 0, loadedGeneration = 0;
    uint64_t reloadGeneration = 0, appliedGeneration = 0;
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

    std::array<float, Count> params{{0, 1, 0, 1, 16, 0, 0, 0, 100, 0, 0, 0, 0}};
    std::deque<Command> commands;
    std::shared_ptr<lottie::GpuRenderer> renderer, pending;
    struct Source {
        uint64_t id;
        std::string path;
        std::shared_ptr<lottie::GpuRenderer> renderer;
        bool failed = false;
    };
    std::vector<Source> sources;
    uint64_t selectedSource = 0, nextSource = 1;
    lottie::Playlist playlist;
    bool playlistDirty = true, playlistStart = false;
    void playlistVisibility(bool enabled) {
        for (unsigned i :
             {PlaylistNext, PlaylistPrevious, PlaylistRandom, PlaylistOnStart, SpawnOnNext}) {
            SetParamVisibility(i, enabled);
            RaiseParamEvent(i, FF_EVENT_FLAG_VISIBILITY);
        }
    }
    Source *findSource(uint64_t id) {
        for (auto &source : sources)
            if (source.id == id)
                return &source;
        return nullptr;
    }
    void releaseUnusedSources() {
        for (auto it = sources.begin(); it != sources.end();) {
            bool live = it->id == selectedSource;
            for (const auto &instance : emitter.instances())
                live = live || instance.source == it->id;
            if (!live) {
                it->renderer->releaseGL();
                it = sources.erase(it);
            } else
                ++it;
        }
    }
    bool selectPath(const std::string &path, bool fresh = false) {
        for (auto it = sources.rbegin(); !fresh && it != sources.rend(); ++it)
            if (const auto &source = *it; source.path == path) {
                if (source.failed)
                    return false;
                renderer = source.renderer;
                selectedSource = source.id;
                releaseUnusedSources();
                return true;
            }
        try {
            auto next = std::make_shared<lottie::GpuRenderer>(embedded_demo::load(path));
            selectedSource = nextSource++;
            sources.push_back({selectedSource, path, next});
            renderer = std::move(next);
            releaseUnusedSources();
            return true;
        } catch (const std::exception &e) {
            FFGLLog::LogToHost(e.what());
            return false;
        }
    }
    bool nextFile(uint64_t expectedGeneration, bool fresh = false, bool backwards = false) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (generation != expectedGeneration || loadedGeneration != generation)
                return false;
            if (fresh && !file.empty()) {
                try {
                    playlist.setFolder(std::filesystem::absolute(std::filesystem::u8path(file))
                                           .parent_path()
                                           .u8string(),
                                       true);
                } catch (const std::exception &e) {
                    FFGLLog::LogToHost(e.what());
                    return false;
                }
            }
        }
        for (size_t attempt = 0; attempt < std::max(size_t(1), playlist.size()); ++attempt) {
            auto path =
                playlist.size() ? (backwards ? playlist.previous() : playlist.next()) : file;
            if (selectPath(path, fresh)) {
                std::lock_guard<std::mutex> lock(mutex);
                // A concurrent explicit File selection takes priority over this
                // render-thread Next; do not overwrite its pending host value.
                if (generation == expectedGeneration && loadedGeneration == generation) {
                    bool changed = file != path;
                    file = path;
                    updateFilePresentation(changed);
                    return true;
                }
                return false;
            }
        }
        return false;
    }
    // Snapshot under the parameter mutex at the actual Trigger callback, not next render.
    lottie::Transform spawnTransform() const {
        lottie::Transform t;
        t.x = params[PosX];
        t.y = params[PosY];
        t.scale = params[Scale] / 100;
        t.rotation = params[Rotation];
        t.anchorX = params[AnchorX];
        t.anchorY = params[AnchorY];
        t.fit = static_cast<lottie::Fit>(int(params[Fitting]));
        t.clipToCanvas = params[ClipCanvas] > .5f;
        t.fixedCropFrame = true;
        return t;
    }
    // Called under mutex. Bound work accumulated while a source is inactive.
    void enqueue(Kind kind, float value) {
        if (commands.size() >= 512) {
            if (kind == Kind::Trigger)
                return;
            commands.pop_front();
        }
        commands.push_back({kind, value, lottie::Emitter::Clock::now(),
                            (kind == Kind::Trigger ||
                             ((kind == Kind::Next || kind == Kind::Previous) && value > .5f))
                                ? spawnTransform()
                                : lottie::Transform{},
                            (kind == Kind::Trigger ||
                             ((kind == Kind::Next || kind == Kind::Previous) && value > .5f))
                                ? palette_controls::read(params, PaletteStart)
                                : lottie::Palette{},
                            params[Opacity] / 100.f});
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
            std::unique_ptr<lottie::GpuRenderer> result;
            std::string error;
            try {
                result = std::make_unique<lottie::GpuRenderer>(embedded_demo::load(path));
            } catch (const std::exception &e) {
                error = e.what();
            } catch (...) {
                error = "Unknown emitter load error";
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
    void draw(GLuint texture, float opacity = 1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUseProgram(shader.GetGLID());
        glUniform1i(image, 0);
        glUniform1f(opacityUniform, opacity);
        quad.Draw();
    }

  public:
    LottieEmitterPlugin() {
        // The pinned SDK leaves this lifecycle flag uninitialized.
        m_isConnected = false;
        livePalettes.reserve(64);
        params.fill(0);
        params[AutoSpawn] = params[Speed] = 1;
        params[Limit] = 16;
        SetMinInputs(0);
        SetMaxInputs(0);
        SetTimeSupported(false);
        SetFileParamInfo(File, "File", {"json"}, "");
        SetParamInfo(Reload, "Reload", FF_TYPE_EVENT, 0.0f);
        SetParamVisibility(Reload, false);
        SetParamInfo(Trigger, "Spawn", FF_TYPE_EVENT, 0.0f);
        SetParamElementInfo(Trigger, 0, "Start", 1.0f);
        auto number = [&](unsigned index, const char *name, float initial, float low, float high,
                          unsigned type = FF_TYPE_STANDARD) {
            SetParamInfo(index, name, type, initial);
            SetParamRange(index, low, high);
            FindParamInfo(index)->defaultFloatVal = initial;
            params[index] = initial;
        };
        number(Speed, "Speed", 1, 0, 10);
        number(Opacity, "Opacity", 100, 0, 100);
        number(Limit, "Max Instances", 16, 1, 64, FF_TYPE_INTEGER);
        SetParamInfo(Clear, "Clear", FF_TYPE_EVENT, 0.0f);
        SetParamInfo(AutoSpawn, "Spawn on Start", FF_TYPE_BOOLEAN, true);
        for (unsigned i : {Speed, Opacity, Limit, AutoSpawn, Clear})
            SetParamGroup(i, "Spawn Options");
        SetOptionParamInfo(Fitting, "Fit Mode", 4, 0);
        SetParamElementInfo(Fitting, 0, "Fit", 0);
        SetParamElementInfo(Fitting, 1, "Fill", 1);
        SetParamElementInfo(Fitting, 2, "Stretch", 2);
        SetParamElementInfo(Fitting, 3, "Original", 3);
        number(PosX, "Position X", 0, -16384, 16384);
        number(PosY, "Position Y", 0, -16384, 16384);
        number(Scale, "Scale", 100, 0, 1000);
        number(Rotation, "Rotation", 0, -180, 180);
        number(AnchorX, "Anchor X", 0, -16384, 16384);
        number(AnchorY, "Anchor Y", 0, -16384, 16384);
        for (unsigned i = Fitting; i <= AnchorY; ++i)
            SetParamGroup(i, "Lottie Transform");
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
        SetParamInfo(SpawnOnNext, "Spawn on Change", FF_TYPE_BOOLEAN, false);
        SetParamInfo(PlaylistNext, "Next", FF_TYPE_EVENT, 0.0f);
        SetParamInfo(PlaylistPrevious, "Previous", FF_TYPE_EVENT, 0.0f);
        SetParamInfo(ChooseFile, "Replace File", FF_TYPE_EVENT, 0.0f);
        SetParamVisibility(ChooseFile, false); // Retained only for older assignments.
        SetParamInfo(RecoveryPath, "Recovery Path", FF_TYPE_TEXT, "");
        SetParamVisibility(RecoveryPath, false);
        playlistVisibility(false);
        requestFile("", true);
    }
    ~LottieEmitterPlugin() override {
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
            startPending = params[AutoSpawn] > .5f;
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
        reloadAtStart = false;
        startPending = false;
        playlistStart = false;
        // Lifecycle callbacks may run outside the GL thread. Discard stale
        // triggers and clear instances on the next render before a new Spawn.
        commands.clear();
        enqueue(Kind::Clear, 0);
        return FF_SUCCESS;
    }
    const char *GetShortName() override { return "Emitter GPU"; }
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
                playlistStart = false;
                startPending = !forceReload && connected && params[AutoSpawn] > .5f;
                ++generation;
                if (forceReload)
                    reloadGeneration = generation;
                loadedGeneration = 0;
                ready = false;
                requested = true;
                if ((!forceReload && params[PlaylistEnabled] <= .5f) || file.empty())
                    enqueue(Kind::Reset, 0);
                if (!worker.joinable())
                    worker = std::thread(&LottieEmitterPlugin::loadLoop, this);
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
        try {
            std::lock_guard<std::mutex> lock(mutex);
            if (index == PlaylistNext || index == PlaylistPrevious) {
                if (value > .5f && params[PlaylistEnabled] > .5f) {
                    enqueue(index == PlaylistPrevious ? Kind::Previous : Kind::Next,
                            params[SpawnOnNext]);
                    if (params[SpawnOnNext] > .5f)
                        startPending = false;
                    playlistStart = false;
                }
                return FF_SUCCESS;
            }
            if (index == PlaylistEnabled && (value > .5f) != (params[index] > .5f)) {
                playlistDirty = true;
                playlistStart = false;
                playlistVisibility(value > .5f);
            }
            if (index == PlaylistOnStart && value <= .5f)
                playlistStart = false;
            if (index == Trigger || index == Clear) {
                if (value > .5f) {
                    startPending = false;
                    if (index == Clear)
                        enqueue(Kind::Clear, 0);
                    else if (loadedGeneration && loadedGeneration == generation)
                        enqueue(Kind::Trigger, 0);
                }
                params[index] = 0;
                return FF_SUCCESS;
            }
            if (index >= PaletteStart || index == ClipCanvas)
                value = std::clamp(value, 0.0f, 1.0f);
            else if (index == AutoSpawn) {
                value = value > .5f ? 1.0f : 0.0f;
                if (!value)
                    startPending = false;
            } else if (index == Speed)
                value = std::clamp(value, 0.0f, 10.0f);
            else if (index == Opacity)
                value = std::clamp(value, 0.0f, 100.0f);
            else if (index == Limit)
                value = std::round(std::clamp(value, 1.0f, 64.0f));
            else if (index == Scale)
                value = std::clamp(value, 0.0f, 1000.0f);
            else if (index == Rotation)
                value = std::clamp(value, -180.0f, 180.0f);
            else if (index == Fitting)
                value = std::round(std::clamp(value, 0.0f, 3.0f));
            else
                value = std::clamp(value, -16384.0f, 16384.0f);
            if (value != params[index]) {
                if (index == Speed)
                    enqueue(Kind::Speed, value);
                if (index == Limit)
                    enqueue(Kind::Limit, value);
            }
            params[index] = value;
            return FF_SUCCESS;
        } catch (...) {
            return FF_FAIL;
        }
    }
    float GetFloatParameter(unsigned index) override {
        std::lock_guard<std::mutex> lock(mutex);
        return index < Count ? params[index] : 0;
    }
    char *GetParameterDisplay(unsigned index) override {
        if (index == Opacity) {
            thread_local char opacityText[32];
            std::snprintf(opacityText, sizeof(opacityText), "%.1f %%", GetFloatParameter(index));
            return opacityText;
        }
        if (index != Speed && index < PosX)
            return CFFGLPlugin::GetParameterDisplay(index);
        if (index == Fitting || index > AnchorY)
            return CFFGLPlugin::GetParameterDisplay(index);
        thread_local char text[32];
        auto value = GetFloatParameter(index);
        std::snprintf(text, sizeof(text),
                      index == Speed      ? "%.3fx"
                      : index == Scale    ? "%.1f%%"
                      : index == Rotation ? "%.1f deg"
                                          : "%.1f px",
                      value);
        return text;
    }
    FFResult InitGL(const FFGLViewportStruct *vp) override {
        GLStateGPU state;
        try {
            const char *vs =
                "#version 410 core\nlayout(location=0)in vec4 vPosition;layout(location=1)in vec2 "
                "vUV;out vec2 uv;void main(){gl_Position=vPosition;uv=vUV;}";
            const char *fs =
                "#version 410 core\nin vec2 uv;out vec4 fragColor;uniform sampler2D "
                "image;uniform float opacity;void main(){fragColor=texture(image,uv)*opacity;}";
            if (!shader.Compile(vs, fs) || !quad.Initialise(false))
                throw std::runtime_error("Emitter shader initialization failed");
            image = shader.FindUniform("image");
            opacityUniform = shader.FindUniform("opacity");
            scratch.create();
            output.create();
            return CFFGLPlugin::InitGL(vp);
        } catch (const std::exception &e) {
            FFGLLog::LogToHost(e.what());
            DeInitGL();
            return FF_FAIL;
        }
    }
    FFResult ProcessOpenGL(ProcessOpenGLStruct *) override {
        try {
            refreshFileReference();
            GLStateGPU state;
            state.prepare();
            std::array<float, Count> values;
            std::deque<Command> work;
            std::shared_ptr<lottie::GpuRenderer> next;
            std::string committedPath;
            uint64_t processingGeneration = 0;
            std::string error;
            bool replace = false, reloaded = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                values = params;
                processingGeneration = generation;
                work.swap(commands);
                if (ready && pendingGeneration == generation) {
                    next = std::move(pending);
                    error = std::move(pendingError);
                    ready = false;
                    replace = true;
                    appliedGeneration = pendingGeneration;
                    reloaded = pendingGeneration == reloadGeneration;
                    committedPath = file;
                    loadedGeneration = next ? generation : 0;
                }
                if (playlistStart && loadedGeneration && loadedGeneration == generation) {
                    if (!playlistDirty)
                        work.push_front({Kind::NextOnStart, 0, lottie::Emitter::Clock::now(), {}});
                    playlistStart = false;
                }
                if (startPending && connected && params[AutoSpawn] > .5f && loadedGeneration &&
                    loadedGeneration == generation) {
                    work.push_back({Kind::Speed, params[Speed], lottie::Emitter::Clock::now(), {}});
                    work.push_back({Kind::Trigger, 0, lottie::Emitter::Clock::now(),
                                    spawnTransform(), palette_controls::read(params, PaletteStart),
                                    params[Opacity] / 100.f});
                    startPending = false;
                }
            }
            if (replace) {
                const bool preserve =
                    (values[PlaylistEnabled] > .5f || reloaded) && !committedPath.empty();
                if (!preserve) {
                    for (auto &source : sources)
                        source.renderer->releaseGL();
                    sources.clear();
                }
                selectedSource = 0;
                renderer = std::move(next);
                if (renderer) {
                    for (auto it = sources.rbegin(); it != sources.rend(); ++it)
                        if (const auto &source = *it;
                            !reloaded && source.path == committedPath && !source.failed) {
                            selectedSource = source.id;
                            renderer = source.renderer;
                            break;
                        }
                    if (!selectedSource) {
                        selectedSource = nextSource++;
                        sources.push_back({selectedSource, committedPath, renderer});
                    }
                }
                auto now = lottie::Emitter::Clock::now();
                if (!preserve)
                    emitter.setDuration(renderer ? renderer->metadata().duration : 0, now);
                emitter.setSpeed(values[Speed], now);
                emitter.setLimit(unsigned(values[Limit]), now);
                if (!error.empty())
                    FFGLLog::LogToHost(error.c_str());
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (reloadAtStart && appliedGeneration == generation) {
                    reloadAtStart = false;
                    if (selectPath(file, true)) {
                        loadedGeneration = generation;
                        updateFilePresentation();
                    }
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
                if (playlistDirty && loadedGeneration == generation) {
                    playlistDirty = false;
                    try {
                        playlist.setFolder("");
                        if (params[PlaylistEnabled] > .5f && !file.empty()) {
                            playlist.setFolder(
                                std::filesystem::absolute(std::filesystem::u8path(file))
                                    .parent_path()
                                    .u8string());
                            playlist.select(file);
                        }
                        if (!file.empty())
                            selectPath(file);
                    } catch (const std::exception &e) {
                        FFGLLog::LogToHost(e.what());
                    }
                }
                playlist.setRandom(params[PlaylistRandom] > .5f);
            }
            for (const auto &command : work) {
                switch (command.kind) {
                case Kind::Reset:
                case Kind::Clear:
                    emitter.clear(command.time);
                    break;
                case Kind::Speed:
                    emitter.setSpeed(command.value, command.time);
                    break;
                case Kind::Limit:
                    emitter.setLimit(unsigned(command.value), command.time);
                    break;
                case Kind::NextOnStart:
                case Kind::Previous:
                case Kind::Next:
                    if (values[PlaylistEnabled] > .5f &&
                        nextFile(processingGeneration, command.kind == Kind::NextOnStart,
                                 command.kind == Kind::Previous) &&
                        command.value > .5f) {
                        auto source = findSource(selectedSource);
                        if (source && !source->failed)
                            emitter.trigger(command.time, command.transform, command.palette,
                                            source->id, source->renderer->metadata().duration,
                                            command.opacity);
                    }
                    break;
                case Kind::Trigger: {
                    auto source = findSource(selectedSource);
                    if (source && !source->failed)
                        emitter.trigger(command.time, command.transform, command.palette,
                                        source->id, source->renderer->metadata().duration,
                                        command.opacity);
                    break;
                }
                }
            }
            emitter.update();
            for (auto it = sources.begin(); it != sources.end();) {
                bool live = it->id == selectedSource;
                for (const auto &instance : emitter.instances())
                    live = live || instance.source == it->id;
                if (!live) {
                    it->renderer->releaseGL();
                    it = sources.erase(it);
                    continue;
                }
                livePalettes.clear();
                for (const auto &instance : emitter.instances())
                    if (instance.source == it->id &&
                        std::find(livePalettes.begin(), livePalettes.end(), instance.palette) ==
                            livePalettes.end())
                        livePalettes.push_back(instance.palette);
                it->renderer->retainPalettes(livePalettes);
                ++it;
            }
            const unsigned w = unsigned(state.viewport[2]), h = unsigned(state.viewport[3]);
            if (!w || !h)
                return FF_SUCCESS;
            if (w > 8192 || h > 8192)
                throw std::runtime_error("Emitter output exceeds 8192");
            state.prepare();
            scratch.resize(w, h);
            output.resize(w, h);
            glBindFramebuffer(GL_FRAMEBUFFER, output.fbo);
            glViewport(0, 0, w, h);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            for (const auto &instance : emitter.instances()) {
                auto source = findSource(instance.source);
                if (!source || source->failed || instance.opacity <= 0)
                    continue;
                auto sampleRenderer = source->renderer;
                try {
                    sampleRenderer->setPalette(instance.palette);
                    // Crop the assembled sample during compositing, not each
                    // nested ThorVG paint/mask. Keep authored clips intact.
                    auto contentTransform = instance.transform;
                    contentTransform.clipToCanvas = false;
                    sampleRenderer->render(wglGetCurrentContext(), scratch.fbo, instance.seconds, w,
                                           h, contentTransform);
                } catch (const std::exception &e) {
                    // A lazy asset error must not retain an old host frame or log every frame.
                    FFGLLog::LogToHost(e.what());
                    source->failed = true;
                    continue;
                }
                state.prepare();
                glBindFramebuffer(GL_FRAMEBUFFER, output.fbo);
                glViewport(0, 0, w, h);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                if (instance.transform.clipToCanvas) {
                    const auto &info = sampleRenderer->metadata();
                    double sx = w / double(info.width), sy = h / double(info.height);
                    const auto fit = instance.transform.fit;
                    if (fit == lottie::Fit::Original)
                        sx = sy = 1;
                    else if (fit != lottie::Fit::Stretch)
                        sx = sy = fit == lottie::Fit::Fill ? std::max(sx, sy) : std::min(sx, sy);
                    // Scissor uses framebuffer pixels, with boundaries rounded
                    // to the nearest pixel edge. The centered frame is Y-symmetric.
                    const auto edge = [](double value, unsigned limit) {
                        return GLint(std::clamp(std::floor(value + .5), 0.0, double(limit)));
                    };
                    const GLint left = edge((w - sx * info.width) * .5, w);
                    const GLint bottom = edge((h - sy * info.height) * .5, h);
                    const GLint right = edge((w + sx * info.width) * .5, w);
                    const GLint top = edge((h + sy * info.height) * .5, h);
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(left, bottom, right - left, top - bottom);
                }
                draw(scratch.texture, instance.opacity);
            }
            state.prepare();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.drawFbo);
            glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
            draw(output.texture);
            return FF_SUCCESS;
        } catch (const std::exception &e) {
            FFGLLog::LogToHost(e.what());
            return FF_FAIL;
        } catch (...) {
            return FF_FAIL;
        }
    }
    FFResult DeInitGL() override {
        GLStateGPU state;
        for (auto &source : sources)
            source.renderer->releaseGL();
        scratch.release();
        output.release();
        quad.Release();
        shader.FreeGLResources();
        return FF_SUCCESS;
    }
};
static CFFGLPluginInfo info(PluginFactory<LottieEmitterPlugin>, "LT02", "Lottie Emitter", 2, 2,
                            lottie::release::major, lottie::release::minor, FF_SOURCE,
                            "One-shot overlapping Lottie GPU instances", "Lottie Resolume");
