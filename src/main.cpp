#include <iostream>
#include <spine/spine-sfml.h>
#include <spine/Debug.h>
#include <SFML/Graphics.hpp>
#include <format>
#include "json.h"
#include <filesystem>
#include <fstream>

#include <windows.h>

bool is_effect_spine_json(const std::string& file, const std::string& atlas)
{
    spine::SFMLTextureLoader textureLoader;
	auto atlas_p = std::make_unique<spine::Atlas>(atlas.c_str(), &textureLoader);
    spine::SkeletonJson json(atlas_p.get());
    std::unique_ptr<spine::SkeletonData> skeletonData;
    skeletonData.reset(json.readSkeletonDataFile(file.c_str()));
	if (!skeletonData) {
        throw std::runtime_error(json.getError().buffer());
	}
}

std::unique_ptr<sf::RenderWindow> window = nullptr;
json::Value config;
int main()
{
    HDC hdc = GetDC(NULL);
    int realWidth = GetDeviceCaps(hdc, HORZRES);
    int realHeight = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(NULL, hdc);

    if (!std::filesystem::exists("./config.json"))
    {
        MessageBox(NULL, R"(no config.json)", "error", MB_OK);
        return 1;
    }
    std::string config_str = "";
    std::ifstream configFile("./config.json");
    if (!configFile.is_open())
    {
        MessageBox(NULL, R"(can not open config.json)", "error", MB_OK);
        return 2;
    }
    while (!configFile.eof())
    {
        config_str += configFile.get();
    }
    std::cout << config_str << std::endl;
    try
    {
        config = json::Value::parse(config_str);
        std::cout << std::boolalpha << config.to_object().contains("width") << std::endl;
        if (!json::check<json::Param<json::Type::Object, json::ObjectParams<
            json::ObjectParam<"width", json::Param<json::Type::Number>>,
            json::ObjectParam<"height", json::Param<json::Type::Number>>,
            json::ObjectParam<"x", json::Param<json::Type::Number>>,
            json::ObjectParam<"y", json::Param<json::Type::Number>>,
            json::ObjectParam<"scale", json::Param<json::Type::Number>>,
            json::ObjectParam<"spine_skeleton", json::Param<json::Type::String>>,
            json::ObjectParam<"spine_atlas", json::Param<json::Type::String>>
            >>>(config))
        {
            MessageBox(NULL, R"(config.json wrong format)", "error", MB_OK);
            return 3;
        }
        is_effect_spine_json(
            config.to_object()["spine_skeleton"].to_string(),
            config.to_object()["spine_atlas"].to_string()
        );
    }
    catch (const std::exception& e)
    {
        MessageBox(NULL, e.what(), "error", MB_OK);
        return 4;
    }
    uint64_t width = config.to_object()["width"].to_number();
    uint64_t height = config.to_object()["height"].to_number();
    uint64_t x = config.to_object()["x"].to_number();
    uint64_t y = config.to_object()["y"].to_number();
    uint64_t scale = config.to_object()["scale"].to_number();
    std::string spine_skeleton = config.to_object()["spine_skeleton"].to_string();
    std::string spine_atlas = config.to_object()["spine_atlas"].to_string();

    spine::SFMLTextureLoader textureLoader;
    auto spine_atlas_p = std::make_unique<spine::Atlas>(spine_atlas.c_str(), &textureLoader);
    spine::SkeletonJson spine_json(spine_atlas_p.get());
    spine_json.setScale(scale * 1.0 / 100);
    auto skeletonData = spine_json.readSkeletonDataFile(spine_skeleton.c_str());
    
    spine::SkeletonDrawable drawable(skeletonData);
    drawable.timeScale = 1;
    drawable.setUsePremultipliedAlpha(false);

    spine::Skeleton* skeleton = drawable.skeleton;
    skeleton->setPosition(x, y);
    skeleton->updateWorldTransform();

    if (drawable.state->getData()->getSkeletonData()->findAnimation("idle") == nullptr)
    {
        MessageBox(NULL, "unsupport another animations except idle", "error", MB_OK);
        return 4;
    }
    drawable.state->setAnimation(0, "idle", true);
    window = std::make_unique<sf::RenderWindow>(sf::VideoMode(width, height), "Spine SFML player");
    window->setFramerateLimit(0);
    window->setVerticalSyncEnabled(false);
    window->setFramerateLimit(0);
    sf::Event event;
    sf::Clock deltaClock;

    sf::RectangleShape rect;
    rect.setSize(sf::Vector2f(width, height));
    rect.setPosition(0, 0);
    rect.setFillColor(sf::Color(0, 0, 0, 128));

    uint64_t mouse_stage = 0;//0 normal 1 drug
    sf::Vector2i lastPos;

    uint64_t stage = 0;//0 lock 1 move 2 scale

    while (window->isOpen())
    {
        while (window->pollEvent(event)) 
        {
            if (event.type == sf::Event::Closed) window->close();
            else if (event.type == sf::Event::KeyPressed) 
            {
                if(stage == 1)
                {
                    float moveSpeed = 1;
                    switch (event.key.code)
                    {
                    case sf::Keyboard::Left:
                        x -= moveSpeed;
                        break;
                    case sf::Keyboard::Right:
                        x += moveSpeed;
                        break;
                    case sf::Keyboard::Up:
                        y -= moveSpeed;
                        break;
                    case sf::Keyboard::Down:
                        y += moveSpeed;
                        break;
                    }

                    drawable.skeleton->setPosition(x, y);
                    drawable.skeleton->updateWorldTransform();
                }
            }
            else if (event.type == sf::Event::MouseWheelScrolled && stage == 2)
            {
                uint64_t zoomStep = 1;
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) 
                {
                    if (event.mouseWheelScroll.delta > 0)
                    {
                        scale += zoomStep;
                    }
                    else if (event.mouseWheelScroll.delta < 0 && scale > zoomStep)
                    {
                        scale -= zoomStep;
                    }
                    drawable.skeleton->setScaleX(scale * 1.0 / 100);
                    drawable.skeleton->setScaleY(scale * 1.0 / 100);
                    skeleton->updateWorldTransform();
                }
            }
            else if (event.type == sf::Event::MouseButtonPressed && 
                event.mouseButton.button == sf::Mouse::Left)
            {
                if(event.mouseButton.x<10 && event.mouseButton.y<10)
                {
                    stage = (stage + 1) % 3;
                    if (stage == 1) rect.setFillColor(sf::Color(0x66, 0xCC, 0xFF, 128));
                    if (stage == 2) rect.setFillColor(sf::Color(0x39, 0xC5, 0xBB, 128));
                    std::cout << "stage:" << stage << std::endl;
                    if (stage == 0)
                    {
                        config.to_object()["x"].to_number() = x;
                        config.to_object()["y"].to_number() = y;
                        config.to_object()["scale"].to_number() = scale;
                        std::ofstream configoFile("./config.json");
                        std::string out = config.to_json_string();
                        configoFile << out;
                    }
                }
                std::cout << "start drug" << std::endl;
                lastPos.x = event.mouseButton.x;
                lastPos.y = event.mouseButton.y;
                if (mouse_stage == 0) mouse_stage = 1;
            }
            else if (event.type == sf::Event::MouseButtonReleased &&
                event.mouseButton.button == sf::Mouse::Left)
            {
                std::cout << "end drug" << std::endl;
                if (mouse_stage == 1) mouse_stage = 0;
            }
            else if (mouse_stage == 1 && event.type == sf::Event::MouseMoved)
            {
                sf::Vector2i curPos(event.mouseMove.x, event.mouseMove.y);
                sf::Vector2i delta = curPos - lastPos;
                lastPos = curPos;
                std::cout << "X+:" << delta.x << " Y+:" << delta.y << std::endl;
                if(stage == 1)
                {
                    x += delta.x;
                    y += delta.y;
                    drawable.skeleton->setPosition(x, y);
                    drawable.skeleton->updateWorldTransform();
                }
                if (stage == 2)
                {
                    uint64_t zoomStep = 1;
                    if (delta.y > 0) scale += zoomStep;
                    else if (delta.y < 0 && scale > zoomStep) scale -= zoomStep;
                    drawable.skeleton->setScaleX(scale * 1.0 / 100);
                    drawable.skeleton->setScaleY(scale * 1.0 / 100);
                    skeleton->updateWorldTransform();
                }
            }
        }

        float delta = deltaClock.getElapsedTime().asSeconds();
        deltaClock.restart();

        drawable.update(delta);

        window->clear();
        window->draw(drawable);
        if (stage != 0)window->draw(rect);
        window->display();
    }
    window.reset();

    return 0;
}