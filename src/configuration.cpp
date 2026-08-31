#include "configuration.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

#include "renderer.h"

namespace {

std::string trim(std::string value) {
    const auto isSpace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char character) {
        return !isSpace(static_cast<unsigned char>(character));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char character) {
        return !isSpace(static_cast<unsigned char>(character));
    }).base(), value.end());
    return value;
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return trim(std::move(value));
}

bool parseBool(const std::string& value, bool& result) {
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" ||
        normalized == "on" || normalized == "enabled") {
        result = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" ||
        normalized == "off" || normalized == "disabled") {
        result = false;
        return true;
    }
    return false;
}

bool parseTextureSettingMode(const std::string& value, TextureSettingMode& result) {
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "config" || normalized == "configuration" ||
        normalized == "auto" || normalized == "automatic" || normalized == "default") {
        result = TextureSettingMode::UseConfiguration;
        return true;
    }
    if (normalized == "on" || normalized == "enabled" || normalized == "true" ||
        normalized == "1" || normalized == "yes") {
        result = TextureSettingMode::Enabled;
        return true;
    }
    if (normalized == "off" || normalized == "disabled" || normalized == "false" ||
        normalized == "0" || normalized == "no") {
        result = TextureSettingMode::Disabled;
        return true;
    }
    return false;
}

const char* textureSettingModeName(TextureSettingMode mode) {
    switch (mode) {
    case TextureSettingMode::Enabled:
        return "on";
    case TextureSettingMode::Disabled:
        return "off";
    case TextureSettingMode::UseConfiguration:
    default:
        return "config";
    }
}

bool parseFullscreenBehavior(const std::string& value, FullscreenBehavior& result) {
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "none" || normalized == "no_action" ||
        normalized == "do_nothing" || normalized == "nothing") {
        result = FullscreenBehavior::DoNothing;
        return true;
    }
    if (normalized == "pause" || normalized == "pause_playback") {
        result = FullscreenBehavior::PausePlayback;
        return true;
    }
    if (normalized == "stop" || normalized == "stop_playback") {
        result = FullscreenBehavior::StopPlayback;
        return true;
    }
    return false;
}

const char* fullscreenBehaviorName(FullscreenBehavior behavior) {
    switch (behavior) {
    case FullscreenBehavior::PausePlayback:
        return "pause";
    case FullscreenBehavior::StopPlayback:
        return "stop";
    case FullscreenBehavior::DoNothing:
    default:
        return "none";
    }
}

bool isSupportedMaxFps(unsigned value) {
    return value == 0 || value == WallpaperRenderer::MatchDisplayRefreshRate ||
           value == 30 || value == 60 || value == 120 || value == 144 ||
           value == 165 || value == 240;
}

bool parseUnsigned(const std::string& value, unsigned& result) {
    const std::string normalized = trim(value);
    if (normalized.empty())
        return false;

    try {
        std::size_t parsed = 0;
        const unsigned long number = std::stoul(normalized, &parsed, 10);
        if (parsed != normalized.size() || number > std::numeric_limits<unsigned>::max())
            return false;
        result = static_cast<unsigned>(number);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseFloat(const std::string& value, float& result) {
    const std::string normalized = trim(value);
    if (normalized.empty())
        return false;

    try {
        std::size_t parsed = 0;
        const float number = std::stof(normalized, &parsed);
        if (parsed != normalized.size() || !std::isfinite(number))
            return false;
        result = number;
        return true;
    } catch (...) {
        return false;
    }
}

template <typename Callback>
bool readKeyValues(const std::filesystem::path& path, Callback&& callback) {
    std::ifstream input(path);
    if (!input)
        return false;

    std::string line;
    bool firstLine = true;
    while (std::getline(input, line)) {
        if (firstLine && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        firstLine = false;

        line = trim(std::move(line));
        if (line.empty() || line.front() == '#' || line.front() == ';' ||
            (line.front() == '[' && line.back() == ']')) {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        const std::string key = lowerAscii(trim(line.substr(0, separator)));
        const std::string value = unquote(line.substr(separator + 1));
        if (!key.empty())
            callback(key, value);
    }
    return true;
}

std::filesystem::path resolvePath(const std::filesystem::path& configPath,
                                  const std::string& value) {
    std::filesystem::path result = std::filesystem::u8path(value);
    if (result.is_relative())
        result = configPath.parent_path() / result;

    std::error_code error;
    const auto normalized = std::filesystem::weakly_canonical(result, error);
    return error ? result.lexically_normal() : normalized;
}

} // namespace

std::optional<PlaybackConfiguration> loadPlaybackConfiguration(
    const std::filesystem::path& path) {
    PlaybackConfiguration configuration;

    if (!readKeyValues(path, [&](const std::string& key, const std::string& value) {
            if (key == "spine_file" || key == "spine_path" || key == "spine") {
                if (!value.empty()) {
                    configuration.spineFilePath = resolvePath(path, value);
                }
            } else if (key == "animation" || key == "animation_name" || key == "action") {
                if (!value.empty())
                    configuration.animationName = value;
            } else if (key == "force_premultiplied_texture" ||
                       key == "force_premultiplied_channel" || key == "force_premul") {
                bool parsed = false;
                if (parseBool(value, parsed))
                    configuration.forcePremultipliedTexture = parsed;
            } else if (key == "use_premultiplied_alpha" ||
                       key == "force_premultiplied_alpha" || key == "use_pma") {
                bool parsed = false;
                if (parseBool(value, parsed))
                    configuration.usePremultipliedAlpha = parsed;
            } else if (key == "offset_x" || key == "interaction_offset_x") {
                float parsed = 0.f;
                if (parseFloat(value, parsed))
                    configuration.interactionOffsetX =
                        std::clamp(parsed, -100000.f, 100000.f);
            } else if (key == "offset_y" || key == "interaction_offset_y") {
                float parsed = 0.f;
                if (parseFloat(value, parsed))
                    configuration.interactionOffsetY =
                        std::clamp(parsed, -100000.f, 100000.f);
            } else if (key == "scale" || key == "interaction_scale") {
                float parsed = 0.f;
                if (parseFloat(value, parsed) && parsed > 0.f)
                    configuration.interactionScale =
                        std::clamp(parsed, 0.01f, 100.f);
            }
        })) {
        return std::nullopt;
    }

    // A config file without a spine_file key is still considered valid so the
    // caller can use it to override texture compatibility settings.
    return configuration;
}

SavedSettings loadSavedSettings(const std::filesystem::path& path) {
    SavedSettings settings;
    bool forceTextureModeSpecified = false;
    bool alphaModeSpecified = false;
    readKeyValues(path, [&](const std::string& key, const std::string& value) {
        if (key == "force_premultiplied_texture_mode") {
            TextureSettingMode parsed = settings.forcePremultipliedTextureMode;
            if (parseTextureSettingMode(value, parsed)) {
                settings.forcePremultipliedTextureMode = parsed;
                forceTextureModeSpecified = true;
            }
        } else if (key == "force_premultiplied_texture" || key == "force_premul") {
            bool parsed = false;
            if (!forceTextureModeSpecified && parseBool(value, parsed)) {
                settings.forcePremultipliedTextureMode = parsed
                    ? TextureSettingMode::Enabled : TextureSettingMode::Disabled;
            }
        } else if (key == "use_premultiplied_alpha_mode") {
            TextureSettingMode parsed = settings.usePremultipliedAlphaMode;
            if (parseTextureSettingMode(value, parsed)) {
                settings.usePremultipliedAlphaMode = parsed;
                alphaModeSpecified = true;
            }
        } else if (key == "use_premultiplied_alpha" || key == "use_pma") {
            bool parsed = false;
            if (!alphaModeSpecified && parseBool(value, parsed)) {
                settings.usePremultipliedAlphaMode = parsed
                    ? TextureSettingMode::Enabled : TextureSettingMode::Disabled;
            }
        } else if (key == "fullscreen_behavior" ||
                   key == "maximized_window_behavior") {
            FullscreenBehavior parsed = settings.fullscreenBehavior;
            if (parseFullscreenBehavior(value, parsed))
                settings.fullscreenBehavior = parsed;
        } else if (key == "launch_at_startup" || key == "auto_startup") {
            bool parsed = false;
            if (parseBool(value, parsed))
                settings.launchAtStartup = parsed;
        } else if (key == "max_fps" || key == "maximum_fps") {
            const std::string normalized = lowerAscii(trim(value));
            if (normalized == "display" || normalized == "monitor" || normalized == "match_display") {
                settings.maxFps = WallpaperRenderer::MatchDisplayRefreshRate;
            } else {
                unsigned parsed = settings.maxFps;
                if (parseUnsigned(normalized, parsed) && isSupportedMaxFps(parsed)) {
                    settings.maxFps = parsed;
                }
            }
        } else if (key == "interaction_offset_x") {
            float parsed = settings.interactionOffsetX;
            if (parseFloat(value, parsed))
                settings.interactionOffsetX = std::clamp(parsed, -100000.f, 100000.f);
        } else if (key == "interaction_offset_y") {
            float parsed = settings.interactionOffsetY;
            if (parseFloat(value, parsed))
                settings.interactionOffsetY = std::clamp(parsed, -100000.f, 100000.f);
        } else if (key == "interaction_scale") {
            float parsed = settings.interactionScale;
            if (parseFloat(value, parsed) && parsed > 0.f)
                settings.interactionScale = std::clamp(parsed, 0.01f, 100.f);
        } else if (key == "playback_config_signature") {
            settings.playbackConfigSignature = value;
        }
    });
    return settings;
}

bool saveSavedSettings(const std::filesystem::path& path, const SavedSettings& settings) {
    std::ofstream output(path, std::ios::trunc);
    if (!output)
        return false;

    output << "# SpineWallpaper current settings\n"
           << "force_premultiplied_texture_mode="
           << textureSettingModeName(settings.forcePremultipliedTextureMode) << '\n'
           << "use_premultiplied_alpha_mode="
           << textureSettingModeName(settings.usePremultipliedAlphaMode) << '\n'
           << "fullscreen_behavior="
           << fullscreenBehaviorName(settings.fullscreenBehavior) << '\n'
           << "launch_at_startup="
           << (settings.launchAtStartup ? "true" : "false") << '\n'
           << "max_fps=";
    if (settings.maxFps == WallpaperRenderer::MatchDisplayRefreshRate)
        output << "display\n";
    else
        output << std::min(settings.maxFps, 240u) << '\n';
    output << std::setprecision(9)
           << "interaction_offset_x=" << settings.interactionOffsetX << '\n'
           << "interaction_offset_y=" << settings.interactionOffsetY << '\n'
           << "interaction_scale=" << settings.interactionScale << '\n'
           << "playback_config_signature=" << settings.playbackConfigSignature << '\n';
    return output.good();
}

std::optional<std::string> playbackConfigurationSignature(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;

    constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    std::uint64_t hash = kFnvOffset;
    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= kFnvPrime;
        }
    }
    if (input.bad())
        return std::nullopt;

    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << hash;
    return result.str();
}
