#include "texture_loader.h"

#include <cstddef>
#include <vector>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace {

sf::Uint8 premultiply(sf::Uint8 channel, sf::Uint8 alpha) {
    return static_cast<sf::Uint8>((static_cast<unsigned int>(channel) * alpha + 127u) / 255u);
}

bool isMipMapFilter(spine::TextureFilter filter) {
    return filter == spine::TextureFilter_MipMap ||
           filter == spine::TextureFilter_MipMapNearestNearest ||
           filter == spine::TextureFilter_MipMapLinearNearest ||
           filter == spine::TextureFilter_MipMapNearestLinear ||
           filter == spine::TextureFilter_MipMapLinearLinear;
}

bool isLinearFilter(spine::TextureFilter filter) {
    return filter == spine::TextureFilter_Linear ||
           filter == spine::TextureFilter_MipMapLinearNearest ||
           filter == spine::TextureFilter_MipMapNearestLinear ||
           filter == spine::TextureFilter_MipMapLinearLinear;
}

} // namespace

void WallpaperTextureLoader::load(spine::AtlasPage& page, const spine::String& path) {
    sf::Image image;
    if (!image.loadFromFile(path.buffer()))
        return;

    const sf::Vector2u size = image.getSize();
    const sf::Uint8* source = image.getPixelsPtr();
    const std::size_t byteCount = static_cast<std::size_t>(size.x) * size.y * 4u;

    sf::Texture* texture = new sf::Texture();
    bool loaded = false;
    if (forcePremultiplied() && source != nullptr) {
        std::vector<sf::Uint8> pixels(source, source + byteCount);
        for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
            const sf::Uint8 alpha = pixels[i + 3];
            pixels[i] = premultiply(pixels[i], alpha);
            pixels[i + 1] = premultiply(pixels[i + 1], alpha);
            pixels[i + 2] = premultiply(pixels[i + 2], alpha);
        }

        sf::Image premultiplied;
        premultiplied.create(size.x, size.y, pixels.data());
        loaded = texture->loadFromImage(premultiplied);
    } else {
        loaded = texture->loadFromImage(image);
    }

    if (!loaded) {
        delete texture;
        return;
    }

    texture->setSmooth(isLinearFilter(page.magFilter));
    texture->setRepeated(page.uWrap == spine::TextureWrap_Repeat &&
                          page.vWrap == spine::TextureWrap_Repeat);
    if (isMipMapFilter(page.minFilter))
        texture->generateMipmap();

    page.setRendererObject(texture);
    if (page.width <= 0 || page.height <= 0) {
        page.width = static_cast<int>(size.x);
        page.height = static_cast<int>(size.y);
    }
}

void WallpaperTextureLoader::unload(void* texture) {
    delete static_cast<sf::Texture*>(texture);
}
