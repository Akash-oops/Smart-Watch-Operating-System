#ifndef GALLERYSCREEN_H
#define GALLERYSCREEN_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

class GalleryScreen {
private:
    sf::Font font;
    sf::Text title;
    std::vector<sf::Texture> textures;
    std::vector<sf::Sprite> sprites;

public:
    GalleryScreen() {
        font.loadFromFile("assets/fonts/Roboto-Regular.ttf");
        title.setFont(font);
        title.setString("Gallery");
        title.setCharacterSize(24);
        title.setFillColor(sf::Color::White);
        title.setPosition(150, 20);

        // Load sample images
        loadImages();
    }

    void loadImages() {
        std::vector<std::string> imageFiles = {
            "assets/images/pic1.jpg",
            "assets/images/pic2.jpg",
            "assets/images/pic3.jpg"
        };

        for (auto &file : imageFiles) {
            sf::Texture texture;
            if (texture.loadFromFile(file)) {
                textures.push_back(texture);
                sf::Sprite sprite;
                sprite.setTexture(textures.back());
                sprite.setScale(0.3f, 0.3f);
                sprites.push_back(sprite);
            }
        }

        // Position images in a grid
        float startX = 60.f, startY = 80.f;
        int col = 0;
        for (auto &sprite : sprites) {
            sprite.setPosition(startX + (col * 150), startY);
            col++;
        }
    }

    void draw(sf::RenderWindow &window) {
        window.clear(sf::Color::Black);
        window.draw(title);
        for (auto &sprite : sprites)
            window.draw(sprite);
    }
};

#endif
