#ifndef SETTINGSSCREEN_H
#define SETTINGSSCREEN_H

#include <SFML/Graphics.hpp>

class SettingsScreen {
private:
    sf::Font font;
    sf::Text title;
    sf::Text infoText;

public:
    SettingsScreen() {
        font.loadFromFile("assets/fonts/Roboto-Regular.ttf");
        title.setFont(font);
        title.setString("Settings");
        title.setCharacterSize(28);
        title.setFillColor(sf::Color::White);
        title.setPosition(130, 120);

        infoText.setFont(font);
        infoText.setCharacterSize(20);
        infoText.setFillColor(sf::Color::Yellow);
        infoText.setPosition(80, 180);
        infoText.setString("Press T to toggle theme\n(coming soon)");
    }

    void draw(sf::RenderWindow &window) {
        window.clear(sf::Color(50, 50, 60));
        window.draw(title);
        window.draw(infoText);
    }
};

#endif
