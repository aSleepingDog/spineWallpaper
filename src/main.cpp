#include "renderer.h"
#include "configuration.h"
#include "startup_registration.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>
#include <ctime>
#include <utility>

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr UINT WM_TRAY_MESSAGE = WM_APP + 1;
constexpr UINT ID_TRAY_SETTINGS = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;
constexpr UINT ID_TRAY_RELOAD = 1003;
constexpr UINT ID_SETTINGS_OK = 2001;
constexpr UINT ID_SETTINGS_CANCEL = 2002;
constexpr UINT ID_SETTINGS_RESET = 2003;
constexpr UINT ID_FORCE_PREMUL_CONFIG = 2101;
constexpr UINT ID_FORCE_PREMUL_ON = 2102;
constexpr UINT ID_FORCE_PREMUL_OFF = 2103;
constexpr UINT ID_USE_PMA_CONFIG = 2111;
constexpr UINT ID_USE_PMA_ON = 2112;
constexpr UINT ID_USE_PMA_OFF = 2113;
constexpr UINT ID_MAX_FPS = 2120;
constexpr UINT ID_LAUNCH_AT_STARTUP = 2121;
constexpr UINT ID_FULLSCREEN_BEHAVIOR_NONE = 2131;
constexpr UINT ID_FULLSCREEN_BEHAVIOR_PAUSE = 2132;
constexpr UINT ID_FULLSCREEN_BEHAVIOR_STOP = 2133;
constexpr UINT ID_SETTINGS_SEPARATOR_1 = 2201;
constexpr UINT ID_SETTINGS_SEPARATOR_2 = 2202;
constexpr UINT ID_SETTINGS_SEPARATOR_3 = 2203;
constexpr wchar_t kPlaybackConfigFileName[] = L"SpineWallpaper.ini";
constexpr wchar_t kSavedSettingsFileName[] = L"SpineWallpaper.settings.ini";
constexpr wchar_t kLauncherFileName[] = L"SpineWallpaper.exe";

HINSTANCE moduleInstance() {
    return GetModuleHandleW(nullptr);
}

bool resolveTextureSetting(TextureSettingMode mode,
                           const std::optional<bool>& configuredValue) {
    switch (mode) {
    case TextureSettingMode::Enabled:
        return true;
    case TextureSettingMode::Disabled:
        return false;
    case TextureSettingMode::UseConfiguration:
    default:
        return configuredValue.value_or(false);
    }
}

SavedSettings currentSettings(const WallpaperRenderer& renderer,
                              const SavedSettings& savedSettings,
                              bool launchAtStartup) {
    SavedSettings settings = savedSettings;
    settings.maxFps = renderer.maxFps();
    settings.launchAtStartup = launchAtStartup;
    return settings;
}

enum class PreferredAppMode : int {
    Default = 0,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4,
};

using SetPreferredAppModeFunction = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFunction = void(WINAPI*)();
using ShouldAppsUseDarkModeFunction = bool(WINAPI*)();
using RefreshImmersiveColorPolicyStateFunction = void(WINAPI*)();

struct NativeMenuThemeApi final {
    HMODULE module = nullptr;
    SetPreferredAppModeFunction setPreferredAppMode = nullptr;
    FlushMenuThemesFunction flushMenuThemes = nullptr;
    ShouldAppsUseDarkModeFunction shouldAppsUseDarkMode = nullptr;
    RefreshImmersiveColorPolicyStateFunction refreshColorPolicy = nullptr;
};

NativeMenuThemeApi& nativeMenuThemeApi() {
    static NativeMenuThemeApi api = [] {
        NativeMenuThemeApi result{};
        result.module = LoadLibraryW(L"uxtheme.dll");
        if (!result.module)
            return result;

        // These menu theme entry points are not part of the public UXTheme API.
        result.setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFunction>(
            GetProcAddress(result.module, MAKEINTRESOURCEA(135)));
        result.flushMenuThemes = reinterpret_cast<FlushMenuThemesFunction>(
            GetProcAddress(result.module, MAKEINTRESOURCEA(136)));
        // This private UXTheme entry point is used to detect the current
        // system app theme for the native tray menu.
        result.shouldAppsUseDarkMode =
            reinterpret_cast<ShouldAppsUseDarkModeFunction>(
                GetProcAddress(result.module, MAKEINTRESOURCEA(132)));
        result.refreshColorPolicy =
            reinterpret_cast<RefreshImmersiveColorPolicyStateFunction>(
                GetProcAddress(result.module, MAKEINTRESOURCEA(104)));
        return result;
    }();
    return api;
}

bool readLightThemeSetting(const wchar_t* name, bool& usesLightTheme) {
    HKEY key = nullptr;
    constexpr wchar_t kPersonalizeKey[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalizeKey, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 0;
    DWORD type = 0;
    DWORD valueSize = sizeof(value);
    const LONG result = RegQueryValueExW(
        key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &valueSize);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || type != REG_DWORD || valueSize != sizeof(value))
        return false;

    usesLightTheme = value != 0;
    return true;
}

bool darkThemeEnabled() {
    // The private ShouldAppsUseDarkMode export reflects the current app
    // preference after ForceDark/ForceLight has been applied. Read the
    // system preference first so a later theme switch can always be seen.
    bool usesLightTheme = true;
    if (readLightThemeSetting(L"AppsUseLightTheme", usesLightTheme) ||
        readLightThemeSetting(L"SystemUsesLightTheme", usesLightTheme)) {
        return !usesLightTheme;
    }

    auto& api = nativeMenuThemeApi();
    if (api.shouldAppsUseDarkMode)
        return api.shouldAppsUseDarkMode();

    const COLORREF fallback = GetSysColor(COLOR_WINDOW);
    const unsigned brightness =
        5u * GetRValue(fallback) + 2u * GetGValue(fallback) + GetBValue(fallback);
    return brightness <= 8u * 128u;
}

void refreshNativeMenuTheme() {
    auto& api = nativeMenuThemeApi();
    if (api.refreshColorPolicy)
        api.refreshColorPolicy();

    if (api.setPreferredAppMode) {
        api.setPreferredAppMode(darkThemeEnabled()
                                    ? PreferredAppMode::ForceDark
                                    : PreferredAppMode::ForceLight);
    }
    if (api.flushMenuThemes)
        api.flushMenuThemes();
}

void writeCrashLog(EXCEPTION_POINTERS* exception) noexcept {
    try {
        wchar_t modulePath[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, modulePath,
                                                ARRAYSIZE(modulePath));
        if (length == 0)
            return;

        const auto logDirectory =
            std::filesystem::path(modulePath).parent_path().parent_path() /
            L"logs";
        std::error_code error;
        std::filesystem::create_directories(logDirectory, error);
        if (error)
            return;

        std::ofstream log(logDirectory / L"SpineWallpaperPlayer.log",
                          std::ios::app);
        if (!log)
            return;

        const auto now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm localTime{};
        localtime_s(&localTime, &now);
        log << '[' << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
            << "] [ERROR] 未处理异常";
        if (exception && exception->ExceptionRecord) {
            log << ": code=0x" << std::hex
                << exception->ExceptionRecord->ExceptionCode
                << ", address=0x"
                << reinterpret_cast<std::uintptr_t>(
                       exception->ExceptionRecord->ExceptionAddress)
                << std::dec;
        }
        log << '\n';
    } catch (...) {
        // A crash handler must never throw or mask the original crash.
    }
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exception) noexcept {
    writeCrashLog(exception);
    return EXCEPTION_CONTINUE_SEARCH;
}

class SettingsDialog final {
public:
    static void show(HWND owner, WallpaperRenderer& renderer,
                     SavedSettings& settings,
                     const std::filesystem::path& settingsPath,
                     const std::filesystem::path& startupPath,
                     const std::optional<PlaybackConfiguration>& playbackConfiguration) {
        if (existingWindow() && IsWindow(existingWindow())) {
            ShowWindow(existingWindow(), SW_SHOWNORMAL);
            SetForegroundWindow(existingWindow());
            return;
        }

        registerClass();
        auto* state = new State{
            &renderer,
            &settings,
            settingsPath,
            startupPath,
            playbackConfiguration ? playbackConfiguration->forcePremultipliedTexture
                                  : std::nullopt,
            playbackConfiguration ? playbackConfiguration->usePremultipliedAlpha
                                  : std::nullopt,
            settings,
            startup_registration::isRegisteredFor(startupPath)};
        state->originalSettings.maxFps = renderer.maxFps();
        state->originalSettings.launchAtStartup = state->originalLaunchAtStartup;
        HWND dialog = CreateWindowExW(
            0,
            className(),
            L"Spine 壁纸播放器设置",
            WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 500, 200,
            owner,
            nullptr,
            moduleInstance(),
            state);
        if (!dialog) {
            delete state;
            return;
        }

        existingWindow() = dialog;
        center(dialog);
        ShowWindow(dialog, SW_SHOWNORMAL);
        UpdateWindow(dialog);
    }

private:
    struct State {
        WallpaperRenderer* renderer;
        SavedSettings* settings;
        std::filesystem::path settingsPath;
        std::filesystem::path startupPath;
        std::optional<bool> configuredForcePremul;
        std::optional<bool> configuredUsePma;
        SavedSettings originalSettings;
        bool originalLaunchAtStartup = false;
        std::array<HWND, 3> forcePremul{};
        std::array<HWND, 3> usePma{};
        std::array<HWND, 3> fullscreenBehavior{};
        HWND maxFps = nullptr;
        HWND launchAtStartup = nullptr;
        bool accepted = false;
        bool restored = false;
    };

    static LPCWSTR className() {
        return L"SpineWallpaperSettings";
    }

    static HWND& existingWindow() {
        static HWND value = nullptr;
        return value;
    }

    static void registerClass() {
        static ATOM registered = 0;
        if (registered)
            return;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.hInstance = moduleInstance();
        wc.lpfnWndProc = &windowProcedure;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className();
        registered = RegisterClassExW(&wc);
    }

    static void applyWindowChrome(HWND window) {
        const DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE,
                              &preference, sizeof(preference));
    }

    static bool isSeparator(UINT id) {
        return id == ID_SETTINGS_SEPARATOR_1 ||
               id == ID_SETTINGS_SEPARATOR_2 ||
               id == ID_SETTINGS_SEPARATOR_3;
    }

    static bool drawSeparator(const DRAWITEMSTRUCT& draw) {
        if (draw.CtlType != ODT_STATIC || !isSeparator(draw.CtlID))
            return false;

        FillRect(draw.hDC, &draw.rcItem, GetSysColorBrush(COLOR_WINDOW));

        RECT separator = draw.rcItem;
        const int dpi = GetDeviceCaps(draw.hDC, LOGPIXELSX);
        const int inset = std::max(1, MulDiv(8, dpi > 0 ? dpi : 96, 96));
        separator.left += inset;
        separator.right -= inset;
        separator.top = (separator.top + separator.bottom) / 2;
        separator.bottom = separator.top + std::max(1, MulDiv(1, dpi > 0 ? dpi : 96, 96));
        HBRUSH separatorBrush = CreateSolidBrush(GetSysColor(COLOR_3DSHADOW));
        FillRect(draw.hDC, &separator, separatorBrush);
        DeleteObject(separatorBrush);
        return true;
    }

    static void center(HWND window) {
        RECT windowRect{};
        RECT workArea{};
        GetWindowRect(window, &windowRect);
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        const int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
        const int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
        SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    static void fitToContent(HWND window) {
        struct ContentBounds {
            HWND parent;
            int right = 0;
            int bottom = 0;
        } bounds{window};

        EnumChildWindows(window, [](HWND child, LPARAM parameter) -> BOOL {
            auto* bounds = reinterpret_cast<ContentBounds*>(parameter);
            RECT childRect{};
            if (!GetWindowRect(child, &childRect))
                return TRUE;

            POINT bottomRight{childRect.right, childRect.bottom};
            ScreenToClient(bounds->parent, &bottomRight);
            bounds->right = std::max(bounds->right, static_cast<int>(bottomRight.x));
            bounds->bottom = std::max(bounds->bottom, static_cast<int>(bottomRight.y));
            return TRUE;
        }, reinterpret_cast<LPARAM>(&bounds));

        constexpr int kMinimumClientWidth = 500;
        constexpr int kRightPadding = 20;
        constexpr int kBottomPadding = 16;
        const int clientWidth = std::max(kMinimumClientWidth,
                                         bounds.right + kRightPadding);
        const int clientHeight = bounds.bottom + kBottomPadding;

        RECT frame{0, 0, clientWidth, clientHeight};
        const DWORD style = static_cast<DWORD>(
            GetWindowLongPtrW(window, GWL_STYLE));
        const DWORD extendedStyle = static_cast<DWORD>(
            GetWindowLongPtrW(window, GWL_EXSTYLE));
        if (!AdjustWindowRectEx(&frame, style, FALSE, extendedStyle))
            return;

        SetWindowPos(window, nullptr, 0, 0,
                     frame.right - frame.left, frame.bottom - frame.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    static HFONT defaultFont() {
        static HFONT font = [] {
            NONCLIENTMETRICSW metrics{};
            metrics.cbSize = sizeof(metrics);
            if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                                       &metrics, 0)) {
                return static_cast<HFONT>(nullptr);
            }

            lstrcpynW(metrics.lfMessageFont.lfFaceName, L"Segoe UI",
                      LF_FACESIZE);
            metrics.lfMessageFont.lfWeight = FW_NORMAL;
            return CreateFontIndirectW(&metrics.lfMessageFont);
        }();
        return font;
    }

    static HWND control(
        HWND parent,
        LPCWSTR type,
        LPCWSTR text,
        DWORD style,
        int x,
        int y,
        int width,
        int height,
        HMENU id = nullptr) {
        HWND child = CreateWindowExW(
            0, type, text, WS_CHILD | WS_VISIBLE | style,
            x, y, width, height, parent, id, moduleInstance(), nullptr);
        if (child) {
            const HFONT font = defaultFont();
            if (font)
                SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return child;
    }

    static HMENU controlId(UINT id) {
        return reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id));
    }

    static HFONT sectionFont() {
        static HFONT font = CreateFontW(
            -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        return font;
    }

    static void setDefaultFont(HWND window) {
        const HFONT font = defaultFont();
        if (font)
            SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        EnumChildWindows(window, [](HWND child, LPARAM fontValue) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(fontValue), TRUE);
            return TRUE;
        }, reinterpret_cast<LPARAM>(font));
    }

    static void setSectionFont(HWND window) {
        const HFONT font = sectionFont();
        if (font)
            SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    static void selectMode(const std::array<HWND, 3>& controls,
                           TextureSettingMode mode) {
        const int selected = static_cast<int>(mode);
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            SendMessageW(controls[i], BM_SETCHECK,
                         i == selected ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    static TextureSettingMode selectedMode(const std::array<HWND, 3>& controls) {
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (SendMessageW(controls[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                return static_cast<TextureSettingMode>(i);
        }
        return TextureSettingMode::UseConfiguration;
    }

    static void selectFullscreenBehavior(
        const std::array<HWND, 3>& controls, FullscreenBehavior behavior) {
        const int selected = static_cast<int>(behavior);
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            SendMessageW(controls[i], BM_SETCHECK,
                         i == selected ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    static FullscreenBehavior selectedFullscreenBehavior(
        const std::array<HWND, 3>& controls) {
        for (int i = 0; i < static_cast<int>(controls.size()); ++i) {
            if (SendMessageW(controls[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                return static_cast<FullscreenBehavior>(i);
        }
        return FullscreenBehavior::DoNothing;
    }

    static bool selectRadioById(const std::array<HWND, 3>& controls, UINT id) {
        for (HWND controlWindow : controls) {
            if (GetDlgCtrlID(controlWindow) != static_cast<int>(id))
                continue;
            for (HWND radio : controls) {
                SendMessageW(radio, BM_SETCHECK,
                             radio == controlWindow ? BST_CHECKED
                                                     : BST_UNCHECKED,
                             0);
            }
            return true;
        }
        return false;
    }

    static unsigned selectedMaxFps(HWND combo) {
        const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (index < 0)
            return 30u;
        return static_cast<unsigned>(SendMessageW(
            combo, CB_GETITEMDATA, index, 0));
    }

    static void selectMaxFps(HWND combo, unsigned fps) {
        const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
        for (LRESULT index = 0; index < count; ++index) {
            if (static_cast<unsigned>(SendMessageW(
                    combo, CB_GETITEMDATA, index, 0)) == fps) {
                SendMessageW(combo, CB_SETCURSEL, index, 0);
                InvalidateRect(combo, nullptr, TRUE);
                return;
            }
        }
    }

    static SavedSettings settingsFromControls(const State& state) {
        SavedSettings settings = *state.settings;
        settings.forcePremultipliedTextureMode = selectedMode(state.forcePremul);
        settings.usePremultipliedAlphaMode = selectedMode(state.usePma);
        settings.fullscreenBehavior =
            selectedFullscreenBehavior(state.fullscreenBehavior);
        settings.maxFps = selectedMaxFps(state.maxFps);
        settings.launchAtStartup = SendMessageW(
            state.launchAtStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
        return settings;
    }

    static void applyRuntime(State& state, const SavedSettings& settings) {
        state.renderer->setForcePremultipliedTexture(resolveTextureSetting(
            settings.forcePremultipliedTextureMode,
            state.configuredForcePremul));
        state.renderer->setUsePremultipliedAlpha(resolveTextureSetting(
            settings.usePremultipliedAlphaMode,
            state.configuredUsePma));
        state.renderer->setMaxFps(settings.maxFps);
        state.renderer->setFullscreenBehavior(settings.fullscreenBehavior);
    }

    static bool setStartupRegistration(HWND window, State& state, bool enabled) {
        std::wstring startupError;
        if (startup_registration::setRegistered(
                state.startupPath, enabled, &startupError)) {
            return true;
        }

        const std::wstring message =
            L"无法修改开机自动启动设置。\n" + startupError;
        MessageBoxW(window, message.c_str(), L"Spine 壁纸播放器",
                    MB_ICONERROR | MB_OK);
        return false;
    }

    static void applyCurrentRuntime(State& state) {
        applyRuntime(state, settingsFromControls(state));
    }

    static void restoreOriginal(HWND window, State& state) {
        if (state.restored)
            return;
        state.restored = true;
        applyRuntime(state, state.originalSettings);
        setStartupRegistration(window, state, state.originalLaunchAtStartup);
    }

    static void resetToDefaults(HWND window, State& state) {
        if (!setStartupRegistration(window, state, false))
            return;

        const SavedSettings defaults{};
        selectMode(state.forcePremul, defaults.forcePremultipliedTextureMode);
        selectMode(state.usePma, defaults.usePremultipliedAlphaMode);
        selectFullscreenBehavior(state.fullscreenBehavior,
                                 defaults.fullscreenBehavior);
        selectMaxFps(state.maxFps, defaults.maxFps);
        SendMessageW(state.launchAtStartup, BM_SETCHECK, BST_UNCHECKED, 0);
        applyRuntime(state, defaults);
    }

    static void createControls(HWND window, State& state) {
        setSectionFont(control(window, L"STATIC", L"纹理兼容性", SS_LEFT,
                               20, 20, 464, 24));
        control(window, L"STATIC",
                L"每项可按配置文件、强制开启或强制关闭。",
                SS_LEFT, 22, 44, 460, 20);

        control(window, L"STATIC", L"强制预乘通道", SS_LEFT,
                36, 74, 118, 24);
        state.forcePremul = {{
            control(window, L"BUTTON", L"按配置",
                    BS_AUTORADIOBUTTON | WS_GROUP,
                    162, 70, 72, 24,
                    controlId(ID_FORCE_PREMUL_CONFIG)),
            control(window, L"BUTTON", L"开",
                    BS_AUTORADIOBUTTON, 238, 70, 44, 24,
                    controlId(ID_FORCE_PREMUL_ON)),
            control(window, L"BUTTON", L"关",
                    BS_AUTORADIOBUTTON, 286, 70, 44, 24,
                    controlId(ID_FORCE_PREMUL_OFF))}};

        control(window, L"STATIC", L"预乘 Alpha 通道", SS_LEFT,
                36, 108, 118, 24);
        state.usePma = {{
            control(window, L"BUTTON", L"按配置",
                    BS_AUTORADIOBUTTON | WS_GROUP,
                    162, 104, 72, 24,
                    controlId(ID_USE_PMA_CONFIG)),
            control(window, L"BUTTON", L"开",
                    BS_AUTORADIOBUTTON, 238, 104, 44, 24,
                    controlId(ID_USE_PMA_ON)),
            control(window, L"BUTTON", L"关",
                    BS_AUTORADIOBUTTON, 286, 104, 44, 24,
                    controlId(ID_USE_PMA_OFF))}};
        control(window, L"STATIC", L"", SS_OWNERDRAW, 20, 140, 464, 2,
                controlId(ID_SETTINGS_SEPARATOR_1));

        setSectionFont(control(window, L"STATIC", L"性能", SS_LEFT,
                               20, 154, 464, 24));
        control(window, L"STATIC", L"最大帧率", SS_LEFT, 36, 188, 100, 24);
        state.maxFps = control(window, L"COMBOBOX", L"",
                               CBS_DROPDOWNLIST | WS_VSCROLL,
                               152, 184, 180, 150,
                               controlId(ID_MAX_FPS));

        const std::array<std::pair<unsigned, LPCWSTR>, 8> options{{
            {30, L"30 FPS"}, {60, L"60 FPS"}, {120, L"120 FPS"},
            {144, L"144 FPS"}, {165, L"165 FPS"}, {240, L"240 FPS"},
            {WallpaperRenderer::MatchDisplayRefreshRate, L"符合显示器"},
            {0, L"不限制"}}};
        int selected = 0;
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            SendMessageW(state.maxFps, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(options[i].second));
            SendMessageW(state.maxFps, CB_SETITEMDATA, i, options[i].first);
            if (options[i].first == state.renderer->maxFps())
                selected = i;
        }
        SendMessageW(state.maxFps, CB_SETCURSEL, selected, 0);
        control(window, L"STATIC", L"", SS_OWNERDRAW, 20, 224, 464, 2,
                controlId(ID_SETTINGS_SEPARATOR_2));

        setSectionFont(control(window, L"STATIC",
                               L"其他软件最大化/全屏时", SS_LEFT,
                               20, 238, 464, 24));
        state.fullscreenBehavior = {{
            control(window, L"BUTTON", L"无操作",
                    BS_AUTORADIOBUTTON | WS_GROUP,
                    36, 270, 84, 24,
                    controlId(ID_FULLSCREEN_BEHAVIOR_NONE)),
            control(window, L"BUTTON", L"暂停播放",
                    BS_AUTORADIOBUTTON, 128, 270, 100, 24,
                    controlId(ID_FULLSCREEN_BEHAVIOR_PAUSE)),
            control(window, L"BUTTON", L"停止播放",
                    BS_AUTORADIOBUTTON, 236, 270, 100, 24,
                    controlId(ID_FULLSCREEN_BEHAVIOR_STOP))}};
        control(window, L"STATIC", L"", SS_OWNERDRAW, 20, 302, 464, 2,
                controlId(ID_SETTINGS_SEPARATOR_3));

        setSectionFont(control(window, L"STATIC", L"启动", SS_LEFT,
                               20, 316, 464, 24));
        state.launchAtStartup = control(window, L"BUTTON", L"开机自动启动",
                                        BS_AUTOCHECKBOX,
                                        36, 348, 210, 24,
                                        controlId(ID_LAUNCH_AT_STARTUP));

        control(window, L"BUTTON", L"重置", BS_PUSHBUTTON,
                208, 380, 84, 30, controlId(ID_SETTINGS_RESET));
        control(window, L"BUTTON", L"取消", BS_PUSHBUTTON,
                304, 380, 84, 30, controlId(ID_SETTINGS_CANCEL));
        control(window, L"BUTTON", L"确定", BS_DEFPUSHBUTTON,
                400, 380, 84, 30, controlId(ID_SETTINGS_OK));

        selectMode(state.forcePremul,
                   state.settings->forcePremultipliedTextureMode);
        selectMode(state.usePma,
                   state.settings->usePremultipliedAlphaMode);
        selectFullscreenBehavior(state.fullscreenBehavior,
                                 state.settings->fullscreenBehavior);
        SendMessageW(state.launchAtStartup, BM_SETCHECK,
                     startup_registration::isRegisteredFor(state.startupPath)
                         ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    static void accept(HWND window, State& state) {
        const SavedSettings settings = settingsFromControls(state);
        if (!setStartupRegistration(window, state, settings.launchAtStartup))
            return;

        applyRuntime(state, settings);
        if (!saveSavedSettings(state.settingsPath, settings)) {
            MessageBoxW(window, L"无法保存设置文件。", L"Spine 壁纸播放器",
                        MB_ICONERROR | MB_OK);
            return;
        }

        *state.settings = settings;
        state.accepted = true;
        DestroyWindow(window);
    }

    static void cancel(HWND window, State& state) {
        if (!state.restored)
            restoreOriginal(window, state);
        DestroyWindow(window);
    }

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<State*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (message) {
        case WM_CREATE:
            setDefaultFont(window);
            applyWindowChrome(window);
            createControls(window, *state);
            fitToContent(window);
            return 0;
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client,
                     GetSysColorBrush(COLOR_WINDOW));
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetBkMode(deviceContext, TRANSPARENT);
            SetTextColor(deviceContext, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(deviceContext, GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case WM_DRAWITEM: {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw && drawSeparator(*draw))
                return TRUE;
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                const UINT id = LOWORD(wParam);
                if (selectRadioById(state->forcePremul, id) ||
                    selectRadioById(state->usePma, id) ||
                    selectRadioById(state->fullscreenBehavior, id)) {
                    applyCurrentRuntime(*state);
                    return 0;
                }
                if (id == ID_LAUNCH_AT_STARTUP) {
                    const bool shouldBeChecked = SendMessageW(
                        state->launchAtStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    if (!setStartupRegistration(window, *state, shouldBeChecked)) {
                        SendMessageW(state->launchAtStartup, BM_SETCHECK,
                                     shouldBeChecked ? BST_UNCHECKED
                                                     : BST_CHECKED,
                                     0);
                    }
                    return 0;
                }
                if (id == ID_SETTINGS_RESET) {
                    resetToDefaults(window, *state);
                    return 0;
                }
            }
            if (LOWORD(wParam) == ID_MAX_FPS &&
                (HIWORD(wParam) == CBN_SELCHANGE ||
                 HIWORD(wParam) == CBN_SELENDOK)) {
                applyCurrentRuntime(*state);
                return 0;
            }
            if (LOWORD(wParam) == ID_SETTINGS_OK) {
                accept(window, *state);
                return 0;
            }
            if (LOWORD(wParam) == ID_SETTINGS_CANCEL) {
                cancel(window, *state);
                return 0;
            }
            break;
        case WM_CLOSE:
            cancel(window, *state);
            return 0;
        case WM_DESTROY:
            if (!state->accepted && !state->restored)
                restoreOriginal(window, *state);
            existingWindow() = nullptr;
            delete state;
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
};

class TrayApplication final {
public:
    TrayApplication(WallpaperRenderer& renderer,
                    SavedSettings& settings,
                    std::filesystem::path settingsPath,
                    std::filesystem::path startupPath,
                    std::filesystem::path playbackConfigPath,
                    std::optional<PlaybackConfiguration> playbackConfiguration)
        : _renderer(renderer),
          _settings(settings),
          _settingsPath(std::move(settingsPath)),
          _startupPath(std::move(startupPath)),
          _playbackConfigPath(std::move(playbackConfigPath)),
          _playbackConfiguration(std::move(playbackConfiguration)) {
    }

    ~TrayApplication() {
        if (_window)
            DestroyWindow(_window);
    }

    bool create() {
        registerClass();
        _window = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            className(),
            L"SpineWallpaperTray",
            0,
            0, 0, 0, 0,
            nullptr, nullptr, moduleInstance(), this);
        if (!_window)
            return false;

        NOTIFYICONDATAW icon{};
        icon.cbSize = sizeof(icon);
        icon.hWnd = _window;
        icon.uID = 1;
        icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        icon.uCallbackMessage = WM_TRAY_MESSAGE;
        icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        lstrcpynW(icon.szTip, L"Spine 壁纸播放器", ARRAYSIZE(icon.szTip));
        if (!Shell_NotifyIconW(NIM_ADD, &icon)) {
            DestroyWindow(_window);
            _window = nullptr;
            return false;
        }
        _iconAdded = true;
        return true;
    }

private:
    static LPCWSTR className() {
        return L"SpineWallpaperTrayHost";
    }

    void registerClass() {
        static ATOM registered = 0;
        if (registered)
            return;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.hInstance = moduleInstance();
        wc.lpfnWndProc = &windowProcedure;
        wc.lpszClassName = className();
        registered = RegisterClassExW(&wc);
    }

    void showMenu() {
        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(_window);

        HMENU menu = CreatePopupMenu();
        if (!menu)
            return;

        AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
        AppendMenuW(menu, MF_STRING, ID_TRAY_RELOAD, L"重载配置");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");

        const UINT command = TrackPopupMenu(
            menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
            point.x, point.y, 0, _window, nullptr);
        DestroyMenu(menu);

        if (command == ID_TRAY_SETTINGS)
            SettingsDialog::show(_window, _renderer, _settings,
                                 _settingsPath, _startupPath,
                                 _playbackConfiguration);
        else if (command == ID_TRAY_RELOAD)
            reloadConfiguration();
        else if (command == ID_TRAY_EXIT)
            PostQuitMessage(0);
    }

    void reloadConfiguration() {
        const SavedSettings settings = loadSavedSettings(_settingsPath);
        std::optional<PlaybackConfiguration> playbackConfiguration =
            _playbackConfiguration;

#if defined(SPINEWALLPAPER_RELEASE)
        playbackConfiguration = loadPlaybackConfiguration(_playbackConfigPath);
#endif

        std::wstring startupError;
        if (!startup_registration::setRegistered(
                _startupPath, settings.launchAtStartup, &startupError)) {
            const std::wstring message =
                L"无法应用开机自动启动设置。\n" + startupError;
            MessageBoxW(_window, message.c_str(), L"Spine 壁纸播放器",
                        MB_ICONERROR | MB_OK);
            return;
        }

        std::filesystem::path spinePath;
        std::string animationName = "idle";
#if defined(SPINEWALLPAPER_RELEASE)
        if (playbackConfiguration) {
            spinePath = playbackConfiguration->spineFilePath;
            animationName = playbackConfiguration->animationName;
        }
#endif

        _renderer.setForcePremultipliedTexture(
            resolveTextureSetting(
                settings.forcePremultipliedTextureMode,
                playbackConfiguration
                    ? playbackConfiguration->forcePremultipliedTexture
                    : std::nullopt));
        _renderer.setUsePremultipliedAlpha(
            resolveTextureSetting(
                settings.usePremultipliedAlphaMode,
                playbackConfiguration
                    ? playbackConfiguration->usePremultipliedAlpha
                    : std::nullopt));
        _renderer.setMaxFps(settings.maxFps);
        _renderer.setFullscreenBehavior(settings.fullscreenBehavior);
#if defined(SPINEWALLPAPER_RELEASE)
        _renderer.setPlaybackConfiguration(std::move(spinePath),
                                            std::move(animationName));
#endif

        _settings = settings;
        _playbackConfiguration = std::move(playbackConfiguration);

#if defined(SPINEWALLPAPER_RELEASE)
        if (!_playbackConfiguration) {
            MessageBoxW(_window,
                        L"未找到启动器旁边的 SpineWallpaper.ini，当前没有可播放的 Spine 资源。",
                        L"Spine 壁纸播放器", MB_ICONWARNING | MB_OK);
        }
#endif
    }

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* app = reinterpret_cast<TrayApplication*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<TrayApplication*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }

        if (app && message == WM_TRAY_MESSAGE) {
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
                app->showMenu();
            else if (lParam == WM_LBUTTONDBLCLK)
                SettingsDialog::show(window, app->_renderer, app->_settings,
                                     app->_settingsPath, app->_startupPath,
                                     app->_playbackConfiguration);
            return 0;
        }

        if (app && message == WM_SETTINGCHANGE) {
            refreshNativeMenuTheme();
            return 0;
        }

        if (app && message == WM_COMMAND) {
            if (LOWORD(wParam) == ID_TRAY_SETTINGS)
                SettingsDialog::show(window, app->_renderer, app->_settings,
                                     app->_settingsPath, app->_startupPath,
                                     app->_playbackConfiguration);
            else if (LOWORD(wParam) == ID_TRAY_RELOAD)
                app->reloadConfiguration();
            else if (LOWORD(wParam) == ID_TRAY_EXIT)
                PostQuitMessage(0);
            return 0;
        }

        if (message == WM_DESTROY) {
            if (app && app->_iconAdded) {
                NOTIFYICONDATAW icon{};
                icon.cbSize = sizeof(icon);
                icon.hWnd = window;
                icon.uID = 1;
                Shell_NotifyIconW(NIM_DELETE, &icon);
                app->_iconAdded = false;
            }
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    WallpaperRenderer& _renderer;
    SavedSettings& _settings;
    std::filesystem::path _settingsPath;
    std::filesystem::path _startupPath;
    std::filesystem::path _playbackConfigPath;
    std::optional<PlaybackConfiguration> _playbackConfiguration;
    HWND _window = nullptr;
    bool _iconAdded = false;
};

std::filesystem::path executableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, ARRAYSIZE(buffer));
    if (length == 0)
        return std::filesystem::current_path();
    return std::filesystem::path(buffer, buffer + length).parent_path();
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(&unhandledExceptionFilter);
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&commonControls);
    refreshNativeMenuTheme();

    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SpineWallpaper");
    if (!instanceMutex)
        return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instanceMutex);
        return 0;
    }

    const auto executableDir = executableDirectory();
    const auto launcherDir = executableDir.parent_path();
    const auto settingsPath = launcherDir / kSavedSettingsFileName;
    SavedSettings savedSettings = loadSavedSettings(settingsPath);
#if defined(SPINEWALLPAPER_RELEASE)
    const auto startupPath = launcherDir / kLauncherFileName;
#else
    const auto startupPath = executableDir / L"SpineWallpaperCore.exe";
#endif
    bool launchAtStartup = startup_registration::isRegisteredFor(startupPath);
    if (!launchAtStartup && savedSettings.launchAtStartup) {
        launchAtStartup = startup_registration::setRegistered(startupPath, true);
    }

    std::filesystem::path spinePath;
    std::string animationName = "idle";
    std::filesystem::path playbackConfigPath;
    std::optional<PlaybackConfiguration> playbackConfiguration;
#if defined(SPINEWALLPAPER_RELEASE)
    playbackConfigPath = launcherDir / kPlaybackConfigFileName;
    playbackConfiguration = loadPlaybackConfiguration(playbackConfigPath);
    if (!playbackConfiguration) {
        MessageBoxW(nullptr,
                    L"未找到启动器旁边的 SpineWallpaper.ini，当前没有可播放的 Spine 资源。",
                    L"Spine 壁纸播放器", MB_ICONWARNING | MB_OK);
    } else {
        spinePath = playbackConfiguration->spineFilePath;
        animationName = playbackConfiguration->animationName;
    }
#else
    spinePath = launcherDir / L"SPINE";
#endif

    const bool scanSpineDirectory =
#if defined(SPINEWALLPAPER_RELEASE)
        false;
#else
        true;
#endif
    WallpaperRenderer renderer(std::move(spinePath), std::move(animationName),
                               scanSpineDirectory);
    renderer.setForcePremultipliedTexture(
        resolveTextureSetting(
            savedSettings.forcePremultipliedTextureMode,
            playbackConfiguration
                ? playbackConfiguration->forcePremultipliedTexture
                : std::nullopt));
    renderer.setUsePremultipliedAlpha(
        resolveTextureSetting(
            savedSettings.usePremultipliedAlphaMode,
            playbackConfiguration
                ? playbackConfiguration->usePremultipliedAlpha
                : std::nullopt));
    renderer.setMaxFps(savedSettings.maxFps);
    renderer.setFullscreenBehavior(savedSettings.fullscreenBehavior);
    renderer.start();

    TrayApplication tray(renderer, savedSettings, settingsPath, startupPath,
                         std::move(playbackConfigPath),
                         std::move(playbackConfiguration));
    if (!tray.create()) {
        renderer.stop();
        saveSavedSettings(settingsPath,
                          currentSettings(renderer, savedSettings,
                                          startup_registration::isRegisteredFor(startupPath)));
        MessageBoxW(nullptr, L"无法创建系统托盘图标。", L"Spine 壁纸播放器", MB_ICONERROR | MB_OK);
        ReleaseMutex(instanceMutex);
        CloseHandle(instanceMutex);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    renderer.stop();
    saveSavedSettings(settingsPath,
                      currentSettings(renderer, savedSettings,
                                      startup_registration::isRegisteredFor(startupPath)));
    ReleaseMutex(instanceMutex);
    CloseHandle(instanceMutex);
    return 0;
}
