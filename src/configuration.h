#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "settings_types.h"

struct PlaybackConfiguration final {
    std::filesystem::path spineFilePath;
    std::string animationName = "idle";
    std::optional<bool> forcePremultipliedTexture;
    std::optional<bool> usePremultipliedAlpha;
};

struct SavedSettings final {
    TextureSettingMode forcePremultipliedTextureMode =
        TextureSettingMode::UseConfiguration;
    TextureSettingMode usePremultipliedAlphaMode =
        TextureSettingMode::UseConfiguration;
    FullscreenBehavior fullscreenBehavior = FullscreenBehavior::DoNothing;
    unsigned maxFps = 30;
    bool launchAtStartup = false;
};

std::optional<PlaybackConfiguration> loadPlaybackConfiguration(
    const std::filesystem::path& path);
SavedSettings loadSavedSettings(const std::filesystem::path& path);
bool saveSavedSettings(const std::filesystem::path& path, const SavedSettings& settings);
