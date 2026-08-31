#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>

#include "settings_types.h"
#include "texture_loader.h"

struct InteractionTransform final {
    float offsetX = 0.f;
    float offsetY = 0.f;
    float scale = 1.f;
};

class WallpaperRenderer final {
public:
    static constexpr unsigned MatchDisplayRefreshRate = 241;

    explicit WallpaperRenderer(
        std::filesystem::path spinePath,
        std::string animationName = "idle",
        bool scanDirectory = true);
    ~WallpaperRenderer();

    WallpaperRenderer(const WallpaperRenderer&) = delete;
    WallpaperRenderer& operator=(const WallpaperRenderer&) = delete;

    bool start();
    void stop();

    bool usePremultipliedAlpha() const noexcept;
    void setUsePremultipliedAlpha(bool value);

    bool forcePremultipliedTexture() const noexcept;
    void setForcePremultipliedTexture(bool value);

    void setPlaybackConfiguration(std::filesystem::path spinePath,
                                  std::string animationName);
    void setPlaybackLoadFailureHandler(std::function<void()> handler);

    unsigned maxFps() const noexcept;
    void setMaxFps(unsigned value);
    void setInteractionTransform(float offsetX, float offsetY,
                                 float scale);
    InteractionTransform interactionTransform() const;
    void resetInteractionPosition();

    // Desktop clicks can be delivered to Explorer instead of the wallpaper
    // child window. The tray thread forwards those input events here.
    void postDesktopMouseButton(bool pressed, int x, int y);
    void postDesktopMouseMove(int x, int y);
    void postDesktopMouseWheel(float delta);

    void setFullscreenBehavior(FullscreenBehavior behavior) noexcept;

private:
    struct Playback;

    enum class InteractionMode : unsigned char {
        Locked,
        Move,
        Scale,
    };

    struct InteractionState final {
        InteractionMode mode = InteractionMode::Locked;
        bool dragging = false;
        int lastMouseX = 0;
        int lastMouseY = 0;
        float offsetX = 0.f;
        float offsetY = 0.f;
        float scale = 1.f;
    };

    enum class InteractionInputType : unsigned char {
        ButtonPressed,
        ButtonReleased,
        Moved,
        WheelScrolled,
    };

    struct InteractionInput final {
        InteractionInputType type;
        int x = 0;
        int y = 0;
        float delta = 0.f;
    };

    void renderLoop();
    void playOnce(sf::RenderWindow& window, Playback& playback);
    std::unique_ptr<Playback> tryLoadRandom(
        const std::vector<std::filesystem::path>& files,
        const std::filesystem::path& previousFile,
        const std::string& animationName);

    void requestReload();
    void waitForSignal();
    void signalInitialLoadResult(bool succeeded);
    void notifyPlaybackLoadFailure();

    std::filesystem::path _spinePath;
    std::string _animationName;
    bool _scanDirectory;
    WallpaperTextureLoader _textureLoader;
    mutable std::mutex _playbackMutex;
    std::atomic_bool _stopRequested{false};
    std::atomic_bool _reloadRequested{false};
    std::atomic_bool _loadFailureReported{false};
    std::atomic_bool _resetInteractionPositionRequested{false};
    std::atomic_bool _usePma{true};
    std::atomic_uint _maxFps{30};
    std::atomic<FullscreenBehavior> _fullscreenBehavior{
        FullscreenBehavior::DoNothing};
    InteractionState _interaction;
    mutable std::mutex _interactionTransformMutex;
    InteractionTransform _savedInteractionTransform;
    std::mutex _interactionInputMutex;
    std::vector<InteractionInput> _interactionInputQueue;
    std::thread _thread;
    std::mutex _signalMutex;
    std::condition_variable _signal;
    std::mutex _initialLoadMutex;
    std::condition_variable _initialLoadSignal;
    bool _initialLoadCompleted = false;
    bool _initialLoadSucceeded = false;
    std::mutex _loadFailureHandlerMutex;
    std::function<void()> _loadFailureHandler;
};
