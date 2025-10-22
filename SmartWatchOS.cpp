#include <SFML/Graphics.hpp>
#include "HomeScreen.h"
#include "GalleryScreen.h"

int main() {
    sf::RenderWindow window(sf::VideoMode(400, 400), "Smartwatch OS");
    window.setFramerateLimit(60);

    HomeScreen homeScreen;
    GalleryScreen galleryScreen; 

    sf::Clock clock; // for updating time every second

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            homeScreen.handleInput(event);
        }

        // Update clock once per second
        if (clock.getElapsedTime().asSeconds() >= 1.f) {
            homeScreen.updateClock();
            clock.restart();
        }

        homeScreen.updateSwipe();

        homeScreen.draw(window);

        window.display();
    }

    return 0;
}
