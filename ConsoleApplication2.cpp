#include <SFML/Graphics.hpp>
#include <noise/noise.h>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>
#include <cmath>
#include <vector>

// Константы окна
const int WIDTH = 1024;
const int HEIGHT = 800;
const double SCALE = 0.05;

// Структура для хранения параметров фрактала
struct FractalParams {
    double frequency;
    int octaves;
    double amplitude;
    double lacunarity;
    double persistence;
    int seed;
    sf::Vector3f colorBase; // Базовый цвет в формате RGB (0-1)
};

// Класс для генерации и управления фракталами
class FractalGenerator {
public:
    FractalGenerator() {
        perlinModule.SetSeed(std::rand());
        resetParams();
    }

    void resetParams() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.5, 5.0);
        std::uniform_int_distribution<> intDis(2, 8);
        std::uniform_real_distribution<> colorDis(0.0, 1.0);

        params.frequency = dis(gen);
        params.octaves = intDis(gen);
        params.amplitude = dis(gen) * 0.5;
        params.lacunarity = dis(gen);
        params.persistence = dis(gen) * 0.3;
        params.seed = std::rand();
        params.colorBase = {
            static_cast<float>(colorDis(gen)),
            static_cast<float>(colorDis(gen)),
            static_cast<float>(colorDis(gen))
        };

        setPerlinParams();
    }

    void setParams(const FractalParams& newParams) {
        params = newParams;
        setPerlinParams();
    }

    void generate(sf::Uint8* pixels, double time, float rotation, float zoom) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                // Центрируем координаты
                double nx = (x - WIDTH / 2.0) * SCALE / zoom;
                double ny = (y - HEIGHT / 2.0) * SCALE / zoom;

                // Применяем вращение
                double rx = nx * cos(rotation) - ny * sin(rotation);
                double ry = nx * sin(rotation) + ny * cos(rotation);

                // Генерируем шум
                double value = perlinModule.GetValue(rx, ry, time);
                value = (value + 1.0) / 2.0; // Нормализация в [0,1]

                // Создаем цвет с вариациями
                float breathing = 0.5f + 0.5f * static_cast<float>(sin(time * 2.0)); // "Дыхание" яркости
                float fValue = static_cast<float>(value);
                sf::Vector3f color = {
                    static_cast<float>(params.colorBase.x * fValue + (1.0f - fValue) * breathing),
                    static_cast<float>(params.colorBase.y * fValue + (1.0f - fValue) * breathing * 0.8f),
                    static_cast<float>(params.colorBase.z * fValue + (1.0f - fValue) * breathing * 0.6f)
                };

                int index = (x + y * WIDTH) * 4;
                pixels[index + 0] = static_cast<sf::Uint8>(std::min(color.x, 1.0f) * 255); // R
                pixels[index + 1] = static_cast<sf::Uint8>(std::min(color.y, 1.0f) * 255); // G
                pixels[index + 2] = static_cast<sf::Uint8>(std::min(color.z, 1.0f) * 255); // B
                pixels[index + 3] = 255; // A
            }
        }
    }

private:
    noise::module::Perlin perlinModule;
    FractalParams params;

    void setPerlinParams() {
        perlinModule.SetFrequency(params.frequency);
        perlinModule.SetOctaveCount(params.octaves);
        perlinModule.SetPersistence(params.persistence);
        perlinModule.SetLacunarity(params.lacunarity);
        perlinModule.SetSeed(params.seed);
    }
};

int main() {
    // Инициализация окна
    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Fractal Dreamscape");
    window.setFramerateLimit(60);

    // Инициализация текстуры и спрайта
    sf::Texture texture;
    texture.create(WIDTH, HEIGHT);
    sf::Sprite sprite(texture);
    sf::Uint8* pixels = new sf::Uint8[WIDTH * HEIGHT * 4];

    // Генератор фракталов
    FractalGenerator generator;
    std::mutex noiseMutex;
    bool newFractalReady = false;
    FractalParams nextParams;

    // Параметры анимации
    double time = 0.0;
    float rotation = 0.0f;
    float zoom = 1.0f;
    float zoomSpeed = 0.001f;
    double nextSwitchTime = 10.0 + (std::rand() % 31); // 10-40 секунд

    // Фоновый поток для генерации следующих параметров фрактала
    std::thread noiseThread([&]() {
        while (window.isOpen()) {
            FractalParams newParams;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.5, 5.0);
            std::uniform_int_distribution<> intDis(2, 8);
            std::uniform_real_distribution<> colorDis(0.0, 1.0);

            newParams.frequency = dis(gen);
            newParams.octaves = intDis(gen);
            newParams.amplitude = dis(gen) * 0.5;
            newParams.lacunarity = dis(gen);
            newParams.persistence = dis(gen) * 0.3;
            newParams.seed = std::rand();
            newParams.colorBase = {
                static_cast<float>(colorDis(gen)),
                static_cast<float>(colorDis(gen)),
                static_cast<float>(colorDis(gen))
            };

            {
                std::lock_guard<std::mutex> lock(noiseMutex);
                nextParams = newParams;
                newFractalReady = true;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Даем время основному потоку
        }
        });

    // Основной цикл
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::G) {
                generator.resetParams(); // Мгновенная генерация нового фрактала
                time = 0.0; // Сброс времени для плавного перехода
            }
        }

        // Обновление параметров анимации
        time += 0.016; // Примерно 60 FPS
        rotation += static_cast<float>(0.01f * sin(time * 0.5));
        zoom += static_cast<float>(zoomSpeed * cos(time * 0.3));
        if (zoom > 2.0f || zoom < 0.5f) zoomSpeed = -zoomSpeed; // Пульсация масштаба

        // Проверка времени для смены фрактала
        if (time > nextSwitchTime && newFractalReady) {
            std::lock_guard<std::mutex> lock(noiseMutex);
            generator.setParams(nextParams); // Обновляем параметры
            time = 0.0;
            nextSwitchTime = 10.0 + (std::rand() % 31); // Новый интервал
            newFractalReady = false;
        }

        // Генерация изображения
        generator.generate(pixels, time, rotation, zoom);
        texture.update(pixels);

        // Отрисовка
        window.clear();
        window.draw(sprite);
        window.display();
    }

    // Очистка
    noiseThread.join();
    delete[] pixels;
    return 0;
}