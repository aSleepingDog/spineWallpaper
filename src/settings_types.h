#pragma once

#include <cstdint>

enum class TextureSettingMode : unsigned char {
    UseConfiguration,
    Enabled,
    Disabled,
};

enum class FullscreenBehavior : unsigned char {
    DoNothing,
    PausePlayback,
    StopPlayback,
};
