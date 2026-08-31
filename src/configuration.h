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
    std::optional<float> interactionOffsetX;
    std::optional<float> interactionOffsetY;
    std::optional<float> interactionScale;
};

struct SavedSettings final {
    TextureSettingMode forcePremultipliedTextureMode =
        TextureSettingMode::UseConfiguration;
    TextureSettingMode usePremultipliedAlphaMode =
        TextureSettingMode::UseConfiguration;
    FullscreenBehavior fullscreenBehavior = FullscreenBehavior::DoNothing;
    unsigned maxFps = 30;
    bool launchAtStartup = false;
    float interactionOffsetX = 0.f;
    float interactionOffsetY = 0.f;
    float interactionScale = 1.f;
    std::string playbackConfigSignature;
};

std::optional<PlaybackConfiguration> loadPlaybackConfiguration(
    const std::filesystem::path& path);
SavedSettings loadSavedSettings(const std::filesystem::path& path);
bool saveSavedSettings(const std::filesystem::path& path, const SavedSettings& settings);
std::optional<std::string> playbackConfigurationSignature(
    const std::filesystem::path& path);
