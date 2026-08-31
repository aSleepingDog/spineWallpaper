#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>

#include "settings_types.h"
#include "texture_loader.h"

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

    void start();
    void stop();

    bool usePremultipliedAlpha() const noexcept;
    void setUsePremultipliedAlpha(bool value);

    bool forcePremultipliedTexture() const noexcept;
    void setForcePremultipliedTexture(bool value);

    void setPlaybackConfiguration(std::filesystem::path spinePath,
                                  std::string animationName);

    unsigned maxFps() const noexcept;
    void setMaxFps(unsigned value);

    void setFullscreenBehavior(FullscreenBehavior behavior) noexcept;

private:
    struct Playback;

    void renderLoop();
    void playOnce(sf::RenderWindow& window, Playback& playback);
    std::unique_ptr<Playback> tryLoadRandom(
        const std::vector<std::filesystem::path>& files,
        const std::filesystem::path& previousFile,
        const std::string& animationName);

    void requestReload();
    void waitForSignal();

    std::filesystem::path _spinePath;
    std::string _animationName;
    bool _scanDirectory;
    WallpaperTextureLoader _textureLoader;
    mutable std::mutex _playbackMutex;
    std::atomic_bool _stopRequested{false};
    std::atomic_bool _reloadRequested{false};
    std::atomic_bool _usePma{true};
    std::atomic_uint _maxFps{30};
    std::atomic<FullscreenBehavior> _fullscreenBehavior{
        FullscreenBehavior::DoNothing};
    std::thread _thread;
    std::mutex _signalMutex;
    std::condition_variable _signal;
};
