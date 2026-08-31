#pragma once

#include <atomic>

#include <spine/spine-sfml.h>

class WallpaperTextureLoader final : public spine::SFMLTextureLoader {
public:
    void setForcePremultiplied(bool value) noexcept {
        _forcePremultiplied.store(value, std::memory_order_relaxed);
    }

    bool forcePremultiplied() const noexcept {
        return _forcePremultiplied.load(std::memory_order_relaxed);
    }

    void load(spine::AtlasPage& page, const spine::String& path) override;
    void unload(void* texture) override;

private:
    std::atomic_bool _forcePremultiplied{true};
};
