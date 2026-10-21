#include <SFML/Graphics.hpp>

#include <cstdlib>

int main()
{
    // SFML 3: VideoMode takes a size, and pollEvent returns an optional.
    sf::RenderWindow app(sf::VideoMode({800, 600}), "SFML window");

    sf::Texture texture;
    if (!texture.loadFromFile("cb.bmp"))
        return EXIT_FAILURE;
    sf::Sprite sprite(texture);

    // Local points are centered on the origin, so setPosition puts the
    // triangle's middle at the middle of the window. Point 0 is the top.
    sf::ConvexShape triangle(3);
    triangle.setPoint(0, {0.f, -80.f});
    triangle.setPoint(1, {70.f, 40.f});
    triangle.setPoint(2, {-70.f, 40.f});
    triangle.setFillColor(sf::Color::Blue);
    triangle.setOutlineColor(sf::Color::Green);
    triangle.setOutlineThickness(4.f);
    triangle.setPosition({400.f, 300.f});

    while (app.isOpen())
    {
        while (const std::optional event = app.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                app.close();

            // Each arrow key is one event. move() adds that offset to the
            // triangle's current position.
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                const float step = 12.f;
                if (key->code == sf::Keyboard::Key::Left)
                    triangle.move({-step, 0.f});
                else if (key->code == sf::Keyboard::Key::Right)
                    triangle.move({step, 0.f});
                else if (key->code == sf::Keyboard::Key::Up)
                    triangle.move({0.f, -step});
                else if (key->code == sf::Keyboard::Key::Down)
                    triangle.move({0.f, step});
            }
        }

        // The position is the triangle's center. Keep that point inside
        // the window; the corners may still cross the edge.
        sf::Vector2f position = triangle.getPosition();
        const sf::Vector2u size = app.getSize();
        const float limitX = static_cast<float>(size.x);
        const float limitY = static_cast<float>(size.y);
        if (position.x < 0.f)
            position.x = 0.f;
        else if (position.x > limitX)
            position.x = limitX;
        if (position.y < 0.f)
            position.y = 0.f;
        else if (position.y > limitY)
            position.y = limitY;
        triangle.setPosition(position);

        app.clear();
        app.draw(sprite);
        app.draw(triangle);
        app.display();
    }

    return EXIT_SUCCESS;
}
