#include "renderer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>

#include <SFML/Graphics/View.hpp>
#include <SFML/Window/Event.hpp>

#include <spine/AnimationStateData.h>
#include <spine/Atlas.h>
#include <spine/SkeletonBinary.h>
#include <spine/SkeletonData.h>
#include <spine/SkeletonJson.h>
#include <spine/Skin.h>
#include <spine/spine-sfml.h>

namespace {

constexpr UINT WM_SPAWN_WORKER = 0x052C;
constexpr int GWL_STYLE_INDEX = -16;
constexpr int GWL_EXSTYLE_INDEX = -20;
constexpr LONG_PTR WS_POPUP_STYLE = static_cast<LONG_PTR>(0x80000000L);
constexpr LONG_PTR WS_EX_TOOLWINDOW_STYLE = 0x00000080L;
constexpr LONG_PTR WS_EX_LAYERED_STYLE = 0x00080000L;

enum class LogLevel {
    Info,
    Warning,
    Error,
};

void writeLog(LogLevel level, const std::string& message) {
    if (level == LogLevel::Info)
        return;

    try {
        wchar_t modulePath[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        const auto coreDirectory = length == 0
            ? std::filesystem::current_path()
            : std::filesystem::path(modulePath).parent_path();
        const auto logDirectory = coreDirectory.parent_path() / L"logs";

        std::error_code error;
        std::filesystem::create_directories(logDirectory, error);
        if (error)
            return;

        std::ofstream log(logDirectory / L"SpineWallpaperPlayer.log", std::ios::app);
        if (!log)
            return;

        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm localTime{};
        localtime_s(&localTime, &now);
        log << '[' << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "] "
            << message << '\n';
    } catch (...) {
        // Logging must not bring down a wallpaper process.
    }
}

std::string pathString(const std::filesystem::path& path) {
    return path.string();
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool endsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isSpineFile(const std::filesystem::path& path) {
    const std::string name = lowerAscii(path.filename().string());
    return endsWith(name, ".json") || endsWith(name, ".skel") || endsWith(name, ".skel.bytes");
}

std::filesystem::path atlasFor(const std::filesystem::path& skeletonPath) {
    const std::string value = skeletonPath.string();
    const std::string lower = lowerAscii(value);
    if (endsWith(lower, ".skel.bytes"))
        return std::filesystem::path(value.substr(0, value.size() - 11) + ".atlas.txt");
    if (endsWith(lower, ".skel"))
        return std::filesystem::path(value.substr(0, value.size() - 5) + ".atlas");
    if (endsWith(lower, ".json"))
        return std::filesystem::path(value.substr(0, value.size() - 5) + ".atlas");
    return {};
}

std::vector<std::filesystem::path> scanAssets(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
        return files;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (!error && entry.is_regular_file(error) && isSpineFile(entry.path()))
                files.push_back(entry.path());
            error.clear();
        }
    } catch (const std::exception& exception) {
        writeLog(LogLevel::Warning,
                 "扫描 SPINE 目录失败: " + pathString(directory) + "\n" + exception.what());
    }

    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return lowerAscii(left.string()) < lowerAscii(right.string());
    });
    return files;
}

HWND getWorkerWindow() {
    const HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman)
        return nullptr;

    DWORD_PTR result = 0;
    SendMessageTimeoutW(progman, WM_SPAWN_WORKER, 0xD, 0x1, SMTO_NORMAL, 1000, &result);

    HWND worker = nullptr;
    EnumWindows([](HWND top, LPARAM parameter) -> BOOL {
        HWND shellView = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
        if (shellView)
            *reinterpret_cast<HWND*>(parameter) = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&worker));

    return worker ? worker : FindWindowExW(progman, nullptr, L"WorkerW", nullptr);
}

std::pair<unsigned, unsigned> primaryScreenSize() {
    const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        const LONG width = std::max<LONG>(1, info.rcMonitor.right - info.rcMonitor.left);
        const LONG height = std::max<LONG>(1, info.rcMonitor.bottom - info.rcMonitor.top);
        return {static_cast<unsigned>(width), static_cast<unsigned>(height)};
    }
    return {1, 1};
}

unsigned primaryScreenRefreshRate() {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) != FALSE &&
        mode.dmDisplayFrequency > 0 && mode.dmDisplayFrequency <= 1000) {
        return static_cast<unsigned>(mode.dmDisplayFrequency);
    }
    return 30;
}

bool isOtherWindowMaximizedOrFullscreen(HWND foregroundWindow) {
    if (!foregroundWindow || !IsWindow(foregroundWindow))
        return false;

    const HWND window = GetAncestor(foregroundWindow, GA_ROOT);
    if (!window || window == GetDesktopWindow() || window == GetShellWindow())
        return false;

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0 || processId == GetCurrentProcessId())
        return false;
    if (!IsWindowVisible(window) || IsIconic(window))
        return false;

    if (IsZoomed(window))
        return true;

    RECT windowRect{};
    if (!GetWindowRect(window, &windowRect))
        return false;

    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
        return false;

    const RECT& monitorRect = monitorInfo.rcMonitor;
    return windowRect.left <= monitorRect.left &&
           windowRect.top <= monitorRect.top &&
           windowRect.right >= monitorRect.right &&
           windowRect.bottom >= monitorRect.bottom;
}

bool isOtherWindowMaximizedOrFullscreen() {
    return isOtherWindowMaximizedOrFullscreen(GetForegroundWindow());
}

void prepareWindowStyle(HWND handle) {
    LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE_INDEX);
    LONG_PTR exStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE_INDEX);
    style |= WS_POPUP_STYLE;
    exStyle |= WS_EX_TOOLWINDOW_STYLE | WS_EX_LAYERED_STYLE;
    SetWindowLongPtrW(handle, GWL_STYLE_INDEX, style);
    SetWindowLongPtrW(handle, GWL_EXSTYLE_INDEX, exStyle);
}

void attachToDesktop(sf::RenderWindow& window, unsigned width, unsigned height) {
    const HWND worker = getWorkerWindow();
    if (!worker)
        throw std::runtime_error("找不到桌面 WorkerW 窗口。");

    const HWND handle = window.getSystemHandle();
    const HWND previousParent = SetParent(handle, worker);
    SetLayeredWindowAttributes(handle, 0, 255, LWA_ALPHA);

    SetWindowPos(handle, HWND_BOTTOM, 0, 0, static_cast<int>(width), static_cast<int>(height),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    window.setPosition(sf::Vector2i(0, 0));
    window.setSize(sf::Vector2u(width, height));
    window.setVisible(true);
    ShowWindow(handle, SW_SHOWNOACTIVATE);

    std::ostringstream message;
    message << "桌面窗口已挂载，WorkerW 渲染尺寸: " << width << 'x' << height
            << ", 原父窗口: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(previousParent);
    writeLog(LogLevel::Info, message.str());
}

void resizeDesktopWindow(sf::RenderWindow& window, unsigned width, unsigned height) {
    window.setPosition(sf::Vector2i(0, 0));
    window.setSize(sf::Vector2u(width, height));
    // The Spine SFML adapter already uses a top-left/Y-down coordinate system.
    // A negative view height mirrors the finished image vertically, so keep
    // the normal positive SFML view here.
    window.setView(sf::View(sf::Vector2f(0.f, 0.f), sf::Vector2f(
        static_cast<float>(width), static_cast<float>(height))));

    std::ostringstream message;
    message << "桌面分辨率已更新，渲染尺寸: " << width << 'x' << height;
    writeLog(LogLevel::Info, message.str());
}

std::string spineError(const spine::String& error) {
    return error.buffer() == nullptr ? std::string("未知 Spine 解析错误") : error.buffer();
}

} // namespace

struct WallpaperRenderer::Playback {
    std::filesystem::path filePath;
    std::unique_ptr<spine::Atlas> atlas;
    std::unique_ptr<spine::SkeletonData> data;
    std::unique_ptr<spine::SkeletonDrawable> drawable;
};

WallpaperRenderer::WallpaperRenderer(std::filesystem::path spinePath,
                                     std::string animationName,
                                     bool scanDirectory)
    : _spinePath(std::move(spinePath)),
      _animationName(animationName.empty() ? "idle" : std::move(animationName)),
      _scanDirectory(scanDirectory) {
}

WallpaperRenderer::~WallpaperRenderer() {
    stop();
}

void WallpaperRenderer::start() {
    if (_thread.joinable())
        return;

    _stopRequested.store(false, std::memory_order_relaxed);
    _thread = std::thread(&WallpaperRenderer::renderLoop, this);
}

void WallpaperRenderer::stop() {
    _stopRequested.store(true, std::memory_order_relaxed);
    _reloadRequested.store(true, std::memory_order_relaxed);
    _signal.notify_all();
    if (_thread.joinable())
        _thread.join();
}

bool WallpaperRenderer::usePremultipliedAlpha() const noexcept {
    return _usePma.load(std::memory_order_relaxed);
}

void WallpaperRenderer::setUsePremultipliedAlpha(bool value) {
    if (_usePma.exchange(value, std::memory_order_relaxed) != value)
        requestReload();
}

bool WallpaperRenderer::forcePremultipliedTexture() const noexcept {
    return _textureLoader.forcePremultiplied();
}

void WallpaperRenderer::setForcePremultipliedTexture(bool value) {
    if (_textureLoader.forcePremultiplied() == value)
        return;
    _textureLoader.setForcePremultiplied(value);
    requestReload();
}

void WallpaperRenderer::setPlaybackConfiguration(std::filesystem::path spinePath,
                                                  std::string animationName) {
    if (animationName.empty())
        animationName = "idle";

    {
        std::lock_guard lock(_playbackMutex);
        _spinePath = std::move(spinePath);
        _animationName = std::move(animationName);
    }
    requestReload();
}

unsigned WallpaperRenderer::maxFps() const noexcept {
    return _maxFps.load(std::memory_order_relaxed);
}

void WallpaperRenderer::setMaxFps(unsigned value) {
    _maxFps.store(value == MatchDisplayRefreshRate ? value : std::min(value, 240u),
                  std::memory_order_relaxed);
}

void WallpaperRenderer::setFullscreenBehavior(FullscreenBehavior behavior) noexcept {
    _fullscreenBehavior.store(behavior, std::memory_order_relaxed);
    _signal.notify_all();
}

void WallpaperRenderer::requestReload() {
    _reloadRequested.store(true, std::memory_order_relaxed);
    _signal.notify_all();
}

void WallpaperRenderer::waitForSignal() {
    std::unique_lock lock(_signalMutex);
    _signal.wait_for(lock, std::chrono::seconds(3), [this] {
        return _stopRequested.load(std::memory_order_relaxed) ||
               _reloadRequested.load(std::memory_order_relaxed);
    });
}

std::unique_ptr<WallpaperRenderer::Playback> WallpaperRenderer::tryLoadRandom(
    const std::vector<std::filesystem::path>& files,
    const std::filesystem::path& previousFile,
    const std::string& animationName) {
    std::vector<std::filesystem::path> candidates;
    candidates.reserve(files.size());
    for (const auto& file : files) {
        if (previousFile.empty() || files.size() == 1 || file != previousFile)
            candidates.push_back(file);
    }

    static thread_local std::mt19937 random(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), random);

    for (const auto& filePath : candidates) {
        try {
            const auto atlasPath = atlasFor(filePath);
            if (atlasPath.empty() || !std::filesystem::is_regular_file(atlasPath)) {
                writeLog(LogLevel::Warning,
                         "跳过缺少 atlas 的 Spine 文件: " + pathString(filePath));
                continue;
            }

            const std::string atlasName = pathString(atlasPath);
            auto atlas = std::make_unique<spine::Atlas>(atlasName.c_str(), &_textureLoader);
            if (atlas->getPages().size() == 0) {
                writeLog(LogLevel::Warning,
                         "跳过无法加载 atlas 的 Spine 文件: " + pathString(filePath));
                continue;
            }

            const std::string skeletonName = pathString(filePath);
            std::unique_ptr<spine::SkeletonData> data;
            const std::string lowerName = lowerAscii(skeletonName);
            if (endsWith(lowerName, ".json")) {
                spine::SkeletonJson json(atlas.get());
                data.reset(json.readSkeletonDataFile(skeletonName.c_str()));
                if (!data)
                    throw std::runtime_error(spineError(json.getError()));
            } else {
                spine::SkeletonBinary binary(atlas.get());
                data.reset(binary.readSkeletonDataFile(skeletonName.c_str()));
                if (!data)
                    throw std::runtime_error(spineError(binary.getError()));
            }

            auto drawable = std::make_unique<spine::SkeletonDrawable>(data.get());
            drawable->setUsePremultipliedAlpha(usePremultipliedAlpha());
            drawable->state->getData()->setDefaultMix(0.f);

            auto& skins = data->getSkins();
            if (skins.size() > 0) {
                const std::size_t selected = static_cast<std::size_t>(
                    random() % static_cast<unsigned int>(skins.size()));
                drawable->skeleton->setSkin(skins[selected]->getName());
                drawable->skeleton->setSlotsToSetupPose();
            } else {
                drawable->skeleton->setToSetupPose();
            }

            if (data->findAnimation(animationName.c_str()) == nullptr) {
                writeLog(LogLevel::Warning,
                         "跳过没有 " + animationName + " 动画的 Spine 文件: " +
                             pathString(filePath));
                continue;
            }
            drawable->state->setAnimation(0, animationName.c_str(), true);

            auto playback = std::make_unique<Playback>();
            playback->filePath = filePath;
            playback->atlas = std::move(atlas);
            playback->data = std::move(data);
            playback->drawable = std::move(drawable);
            return playback;
        } catch (const std::exception& exception) {
            writeLog(LogLevel::Warning,
                     "跳过无法加载的 Spine 文件: " + pathString(filePath) + "\n" +
                         exception.what());
        } catch (...) {
            writeLog(LogLevel::Warning,
                     "跳过无法加载的 Spine 文件: " + pathString(filePath) + "\n未知异常");
        }
    }

    return nullptr;
}

void WallpaperRenderer::playOnce(sf::RenderWindow& window, Playback& playback) {
    sf::Clock clock;
    float elapsed = 0.f;
    const float endTime = std::numeric_limits<float>::infinity();
    unsigned appliedFps = std::numeric_limits<unsigned>::max();
    auto lastSizeCheck = std::chrono::steady_clock::now();
    auto lastRefreshRateCheck = lastSizeCheck;
    unsigned displayRefreshRate = 30;

    while (!_stopRequested.load(std::memory_order_relaxed) &&
           window.isOpen() && elapsed < endTime &&
           !_reloadRequested.load(std::memory_order_relaxed)) {
        const FullscreenBehavior behavior =
            _fullscreenBehavior.load(std::memory_order_relaxed);
        const bool otherWindowActive =
            behavior != FullscreenBehavior::DoNothing &&
            isOtherWindowMaximizedOrFullscreen();
        if (behavior == FullscreenBehavior::StopPlayback && otherWindowActive) {
            return;
        }
        if (behavior == FullscreenBehavior::PausePlayback && otherWindowActive) {
            clock.restart();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        const auto now = std::chrono::steady_clock::now();
        const unsigned configuredFps = maxFps();
        if (configuredFps == MatchDisplayRefreshRate &&
            (appliedFps == std::numeric_limits<unsigned>::max() ||
             now - lastRefreshRateCheck >= std::chrono::milliseconds(500))) {
            displayRefreshRate = primaryScreenRefreshRate();
            lastRefreshRateCheck = now;
        }
        const unsigned fps = configuredFps == MatchDisplayRefreshRate
            ? displayRefreshRate
            : configuredFps;
        if (fps != appliedFps) {
            window.setFramerateLimit(fps);
            appliedFps = fps;
            if (configuredFps == MatchDisplayRefreshRate) {
                writeLog(LogLevel::Info,
                         "最大帧率: 符合显示器 (" + std::to_string(fps) + " FPS)");
            } else {
                writeLog(LogLevel::Info,
                         fps == 0 ? "最大帧率: 不限制" :
                             "最大帧率: " + std::to_string(fps) + " FPS");
            }
        }

        if (now - lastSizeCheck >= std::chrono::milliseconds(500)) {
            const auto [width, height] = primaryScreenSize();
            if (window.getSize().x != width || window.getSize().y != height)
                resizeDesktopWindow(window, width, height);
            lastSizeCheck = now;
        }

        float delta = clock.restart().asSeconds();
        delta = std::clamp(delta, 0.f, 0.1f);
        if (delta <= 0.f)
            delta = 0.001f;

        playback.drawable->update(delta);
        window.clear(sf::Color::Black);
        window.draw(*playback.drawable);
        window.display();
        elapsed += delta;
    }
}

void WallpaperRenderer::renderLoop() {
    try {
        const auto [width, height] = primaryScreenSize();
        sf::RenderWindow window(sf::VideoMode(1, 1), "SpineWallpaper", sf::Style::None);
        window.setActive(false);
        window.setVisible(false);
        prepareWindowStyle(window.getSystemHandle());
        attachToDesktop(window, width, height);
        resizeDesktopWindow(window, width, height);
        window.setActive(true);
        window.setVerticalSyncEnabled(false);

        std::filesystem::path previousFile;
        bool windowHidden = false;
        while (!_stopRequested.load(std::memory_order_relaxed) && window.isOpen()) {
            std::filesystem::path spinePath;
            std::string animationName;
            {
                std::lock_guard lock(_playbackMutex);
                spinePath = _spinePath;
                animationName = _animationName;
            }

            const FullscreenBehavior behavior =
                _fullscreenBehavior.load(std::memory_order_relaxed);
            if (behavior == FullscreenBehavior::StopPlayback &&
                isOtherWindowMaximizedOrFullscreen()) {
                if (!windowHidden) {
                    window.setVisible(false);
                    windowHidden = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (windowHidden) {
                window.setVisible(true);
                windowHidden = false;
            }

            std::vector<std::filesystem::path> files;
            std::error_code error;
            if (std::filesystem::is_regular_file(spinePath, error)) {
                files.push_back(spinePath);
            } else if (_scanDirectory && std::filesystem::is_directory(spinePath, error)) {
                files = scanAssets(spinePath);
            }
            if (files.empty()) {
                waitForSignal();
                continue;
            }

            _reloadRequested.store(false, std::memory_order_relaxed);
            auto playback = tryLoadRandom(files, previousFile, animationName);
            if (!playback) {
                waitForSignal();
                continue;
            }

            previousFile = playback->filePath;
            writeLog(LogLevel::Info, "开始播放: " + pathString(previousFile));
            playOnce(window, *playback);

            if (_reloadRequested.exchange(false, std::memory_order_relaxed))
                previousFile.clear();
        }
    } catch (const std::exception& exception) {
        writeLog(LogLevel::Error, std::string("渲染线程退出: ") + exception.what());
    } catch (...) {
        writeLog(LogLevel::Error, "渲染线程退出: 未知异常");
    }
}
