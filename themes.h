#ifndef THEMES_H
#define THEMES_H

#include <SFML/Graphics.hpp>

struct Theme {
    sf::Color background;
    sf::Color text;
};

static Theme lightTheme = {sf::Color(250, 250, 250), sf::Color::Black};
static Theme darkTheme  = {sf::Color::Black, sf::Color::White};

#endif
