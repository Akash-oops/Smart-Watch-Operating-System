#ifndef HOMESCREEN_H
#define HOMESCREEN_H

#include <SFML/Graphics.hpp>
#include <ctime>
#include <vector>
#include <filesystem>
#include <string>
#include "themes.h"

namespace fs = std::filesystem;

class HomeScreen {
private:
    sf::Font font;
    Theme currentTheme = darkTheme;

    int currentPage = 0; // 0 = Digital, 1 = Analog, 2 = Gallery
    const int totalPages = 3;

    // Swipe
    float offsetX = 0.f;
    bool isSwiping = false;
    float swipeDirection = 0.f;
    float swipeSpeed = 30.f;

    // Text
    sf::Text titleText;
    sf::Text digitalClockText;

    // Analog clock parts
    sf::CircleShape clockFace;
    sf::RectangleShape hourHand, minuteHand, secondHand;

    // Gallery
    std::vector<sf::Texture> galleryTextures;
    std::vector<sf::Sprite> gallerySprites;
    int currentImage = 0;

public:
    HomeScreen() {
        font.loadFromFile("assets/fonts/Roboto-Regular.ttf");

        titleText.setFont(font);
        titleText.setCharacterSize(22);
        titleText.setPosition(120, 20);
        titleText.setFillColor(currentTheme.text);

        digitalClockText.setFont(font);
        digitalClockText.setCharacterSize(40);
        digitalClockText.setFillColor(currentTheme.text);
        digitalClockText.setPosition(100, 150);

        // Analog clock
        clockFace.setRadius(100.f);
        clockFace.setFillColor(sf::Color::Transparent);
        clockFace.setOutlineColor(currentTheme.text);
        clockFace.setOutlineThickness(3);
        clockFace.setPosition(100, 100);

        hourHand.setSize(sf::Vector2f(50, 4));
        hourHand.setOrigin(0, 2);
        hourHand.setFillColor(currentTheme.text);

        minuteHand.setSize(sf::Vector2f(70, 3));
        minuteHand.setOrigin(0, 1.5);
        minuteHand.setFillColor(currentTheme.text);

        secondHand.setSize(sf::Vector2f(90, 2));
        secondHand.setOrigin(0, 1);
        secondHand.setFillColor(sf::Color::Red);

        loadGalleryImages();
    }

    void loadGalleryImages() {
        galleryTextures.clear();
        gallerySprites.clear();

        for (const auto &entry : fs::directory_iterator("assets/gallery")) {
            if (entry.is_regular_file()) {
                sf::Texture texture;
                if (texture.loadFromFile(entry.path().string())) {
                    sf::Sprite sprite(texture);
                    sprite.setScale(0.5f, 0.5f);
                    sprite.setPosition(50, 80);
                    galleryTextures.push_back(texture);
                    gallerySprites.push_back(sprite);
                }
            }
        }
    }

    void toggleTheme() {
        currentTheme = (currentTheme.background == darkTheme.background) ? lightTheme : darkTheme;
        digitalClockText.setFillColor(currentTheme.text);
        titleText.setFillColor(currentTheme.text);
        clockFace.setOutlineColor(currentTheme.text);
        hourHand.setFillColor(currentTheme.text);
        minuteHand.setFillColor(currentTheme.text);
    }

    void handleInput(sf::Event &event) {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::T)
                toggleTheme();
            else if (event.key.code == sf::Keyboard::Right && !isSwiping) {
                if (currentPage == 2 && currentImage < (int)gallerySprites.size() - 1)
                    currentImage++;
                else if (currentPage < totalPages - 1) {
                    swipeDirection = -1.f;
                    isSwiping = true;
                }
            }
            else if (event.key.code == sf::Keyboard::Left && !isSwiping) {
                if (currentPage == 2 && currentImage > 0)
                    currentImage--;
                else if (currentPage > 0) {
                    swipeDirection = 1.f;
                    isSwiping = true;
                }
            }
        }
    }

    void updateClock() {
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        char buffer[10];
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", localTime);
        digitalClockText.setString(buffer);

        float hours = localTime->tm_hour % 12 + localTime->tm_min / 60.f;
        float minutes = localTime->tm_min + localTime->tm_sec / 60.f;
        float seconds = localTime->tm_sec;

        float hourAngle = hours * 30.f - 90.f;
        float minuteAngle = minutes * 6.f - 90.f;
        float secondAngle = seconds * 6.f - 90.f;

        hourHand.setRotation(hourAngle);
        minuteHand.setRotation(minuteAngle);
        secondHand.setRotation(secondAngle);
        hourHand.setPosition(200, 200);
        minuteHand.setPosition(200, 200);
        secondHand.setPosition(200, 200);
    }

    void updateSwipe() {
        if (isSwiping) {
            offsetX += swipeDirection * swipeSpeed;

            if (abs(offsetX) >= 400.f) {
                offsetX = 0.f;
                isSwiping = false;
                currentPage += (swipeDirection == -1.f) ? 1 : -1;
            }
        }
    }

    void draw(sf::RenderWindow &window) {
        window.clear(currentTheme.background);

        std::vector<std::string> titles = {"Digital Clock", "Analog Clock", "Gallery"};
        titleText.setString(titles[currentPage]);
        window.draw(titleText);

        sf::RenderStates states;
        states.transform.translate(offsetX, 0.f);

        if (currentPage == 0)
            window.draw(digitalClockText, states);
        else if (currentPage == 1)
            drawAnalog(window, states);
        else
            drawGallery(window, states);
    }

    void drawAnalog(sf::RenderWindow &window, sf::RenderStates states) {
        window.draw(clockFace, states);
        window.draw(hourHand, states);
        window.draw(minuteHand, states);
        window.draw(secondHand, states);
    }

    void drawGallery(sf::RenderWindow &window, sf::RenderStates states) {
        if (gallerySprites.empty()) {
            sf::Text placeholder("No Images Found", font, 24);
            placeholder.setFillColor(currentTheme.text);
            placeholder.setPosition(80, 160);
            window.draw(placeholder, states);
            return;
        }

        window.draw(gallerySprites[currentImage], states);

        // Image indicator
        sf::Text indicator(
            std::to_string(currentImage + 1) + " / " + std::to_string(gallerySprites.size()),
            font, 18);
        indicator.setFillColor(currentTheme.text);
        indicator.setPosition(150, 350);
        window.draw(indicator, states);
    }
};

#endif
