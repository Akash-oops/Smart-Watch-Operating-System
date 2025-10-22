#ifndef SMARTWATCHOS_H
#define SMARTWATCHOS_H

#include <SFML/Graphics.hpp>
#include "HomeScreen.h"
#include "GalleryScreen.h"
#include "WatchFacesScreen.h"
#include "StepTrackerScreen.h"
#include "SettingsScreen.h"

enum class ScreenType {
    HOME,
    GALLERY,
    WATCHFACES,
    STEPTRACKER,
    SETTINGS
};

class SmartWatchOS {
private:
    sf::RenderWindow window;
    ScreenType currentScreen = ScreenType::HOME;
    sf::CircleShape homeButton;

    HomeScreen homeScreen;
    GalleryScreen galleryScreen;
    WatchFacesScreen facesScreen;
    StepTrackerScreen stepTrackerScreen;
    SettingsScreen settingsScreen;

public:
    SmartWatchOS() : window(sf::VideoMode(400, 400), "Smartwatch OS - v2") {
        window.setFramerateLimit(60);

        // Home button setup
        homeButton.setRadius(25.f);
        homeButton.setFillColor(sf::Color(100, 100, 255));
        homeButton.setPosition(400/2 - 25, 340);
    }

    void run() {
        sf::Clock clock;

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();

                handleInput(event);
            }

            // update clock each second
            if (clock.getElapsedTime().asSeconds() >= 1.f) {
                homeScreen.updateClock();
                clock.restart();
            }

            draw();
        }
    }

    void handleInput(sf::Event &event) {
        if (event.type == sf::Event::MouseButtonPressed) {
            sf::Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);
            if (homeButton.getGlobalBounds().contains(mousePos)) {
                currentScreen = ScreenType::HOME;
            }
        }

        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Right)
                nextScreen();
            else if (event.key.code == sf::Keyboard::Left)
                previousScreen();
        }
    }

    void nextScreen() {
        if (currentScreen == ScreenType::HOME) currentScreen = ScreenType::GALLERY;
        else if (currentScreen == ScreenType::GALLERY) currentScreen = ScreenType::WATCHFACES;
        else if (currentScreen == ScreenType::WATCHFACES) currentScreen = ScreenType::STEPTRACKER;
        else if (currentScreen == ScreenType::STEPTRACKER) currentScreen = ScreenType::SETTINGS;
    }

    void previousScreen() {
        if (currentScreen == ScreenType::SETTINGS) currentScreen = ScreenType::STEPTRACKER;
        else if (currentScreen == ScreenType::STEPTRACKER) currentScreen = ScreenType::WATCHFACES;
        else if (currentScreen == ScreenType::WATCHFACES) currentScreen = ScreenType::GALLERY;
        else if (currentScreen == ScreenType::GALLERY) currentScreen = ScreenType::HOME;
    }

    void draw() {
        window.clear();

        switch (currentScreen) {
            case ScreenType::HOME: homeScreen.draw(window); break;
            case ScreenType::GALLERY: galleryScreen.draw(window); break;
            case ScreenType::WATCHFACES: facesScreen.draw(window); break;
            case ScreenType::STEPTRACKER: stepTrackerScreen.draw(window); break;
            case ScreenType::SETTINGS: settingsScreen.draw(window); break;
        }

        // Draw home button
        window.draw(homeButton);
        window.display();
    }
};

#endif
