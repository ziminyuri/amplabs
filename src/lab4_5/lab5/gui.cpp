#include "gui.h"

void GUI::handle_input(const sf::Event::KeyEvent& evt) {
	if (evt.code == sf::Keyboard::Space)
		paused = !paused;
	else if (evt.code == sf::Keyboard::Escape)
		window.close();
}

void GUI::handle_mouse() {
	const auto isLeftPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
	const auto isRightPressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
	if (!(isLeftPressed || isRightPressed)) return;
	const auto mousePosition = sf::Mouse::getPosition(window);
	const auto coords = window.mapCoordsToPixel(sf::Vector2f(
		static_cast<float>(mousePosition.x),
		static_cast<float>(mousePosition.y)
	));
	if (coords.x >= WINDOW_SIZE || coords.x < 0 || coords.y >= WINDOW_SIZE || coords.y < 0)
		return;
	const auto local_coords = sf::Vector2i(coords.x / PIXEL_SIZE, coords.y / PIXEL_SIZE);
	const auto field_idx = local_coords.x + local_coords.y * FIELD_SIZE;
	if (isLeftPressed)
		(*heat)[field_idx] = 36;
	else if (isRightPressed)
		(*heat)[field_idx] = 0;
}

void GUI::update() {
	services::textured(*input, *output, *heat, FIELD_SIZE, 5, 1);
	std::swap(input, output);
}

void GUI::render() {
	window.clear();
	const auto& pixels = (*output);
	int x_idx = 0;
	int y_idx = 0;
	for (size_t i = 0; i < pixels.size(); i++)
	{
		if (pixels[i] != 0) {
			brush.setPosition(
				static_cast<float>(x_idx * PIXEL_SIZE),
				static_cast<float>(y_idx * PIXEL_SIZE)
			);
			brush.setFillColor(sf::Color(pixels[i], 0, 0));
			window.draw(brush);
		}
		x_idx++;
		if (x_idx >= FIELD_SIZE) {
			x_idx = 0;
			y_idx++;
		}
	}
	window.display();
}

GUI::GUI() :
	paused(true),
	window(
		sf::VideoMode(WINDOW_SIZE, WINDOW_SIZE),
		"Lab 5",
		sf::Style::Titlebar | sf::Style::Close),
	brush(sf::Vector2f(PIXEL_SIZE, PIXEL_SIZE)),
	input(new std::vector<PixelData>(FIELD_SIZE * FIELD_SIZE)),
	output(new std::vector<PixelData>(FIELD_SIZE * FIELD_SIZE)),
	heat(new std::vector<PixelData>(FIELD_SIZE * FIELD_SIZE))
{
	brush.setFillColor(sf::Color::Red);
	brush.setOutlineThickness(1.0f);
	brush.setOutlineColor(sf::Color::Yellow);
}

void GUI::start() {
	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::KeyPressed)
				handle_input(event.key);
		}
		handle_mouse();
		if (!paused)
			update();
		render();
	}
}
