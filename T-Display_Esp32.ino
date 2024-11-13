#include <TFT_eSPI.h>  // Бібліотека для роботи з дисплеєм T-Display ESP32
#include <WiFi.h>      // Бібліотека для роботи з Wi-Fi
#include <HTTPClient.h>// Бібліотека для відправки HTTP запитів
#include <ArduinoJson.h>// Бібліотека для роботи з JSON
#include <time.h>      // Бібліотека для роботи з часом
#include "config.h"    // Файл config.h містить налаштування Wi-Fi та API_KEY для OpenWeather

TFT_eSPI tft = TFT_eSPI(); // Ініціалізація дисплею

const String apiUrl = "http://api.openweathermap.org/data/2.5/weather?q=Kyiv,UA&units=metric&appid=" + String(API_KEY);
// URL для отримання даних про погоду через OpenWeather API для Києва, Україна

// Змінні для збереження отриманих даних про погоду
float currentTemperature = 0.0;    // Поточна температура
float feelsLikeTemperature = 0.0;  // Температура, яку відчуває людина
float minTemperature = 0.0;       // Мінімальна температура дня
float maxTemperature = 0.0;       // Максимальна температура дня
float windSpeed = 0.0;            // Швидкість вітру в м/с
String windDirection = "Unknown"; // Напрямок вітру
float pressure = 0.0;             // Атмосферний тиск
const char* currentWeatherDescription = "Unknown"; // Опис поточної погоди
float precipitation = 0.0;        // Кількість опадів
float humidity = 0.0;             // Вологість

unsigned long lastWeatherUpdate = 0; // Час останнього оновлення погоди
const unsigned long weatherUpdateInterval = 300000; // Інтервал між оновленнями погоди (5 хвилин)

// Функція для виведення даних на дисплей T-Display ESP32
void displayDataOnTFT() {
    tft.fillScreen(TFT_BLACK);  // Очищаємо екран дисплею

    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Встановлюємо білий колір тексту на чорному фоні
    tft.setTextSize(2);  // Розмір тексту

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {  // Перевіряємо, чи доступний час
        const char* daysOfWeek[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        int dayIndex = (timeinfo.tm_wday + 6) % 7;  // Визначаємо день тижня
        tft.setCursor(0, 0);  // Встановлюємо курсор у верхній лівий кут
        tft.printf("%02d:%02d:%02d %s %02d/%02d/%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, daysOfWeek[dayIndex], timeinfo.tm_mday, timeinfo.tm_mon + 1, (timeinfo.tm_year + 1900) % 100);
        tft.setCursor(0, 20);  // Встановлюємо курсор на 30 пікселів нижче
        tft.printf("Temp: %.1f C", currentTemperature);  // Виводимо поточну температуру
        tft.setCursor(0, 40);  // Встановлюємо курсор на 50 пікселів нижче
        tft.printf("Feels: %.1f C", feelsLikeTemperature);  // Виводимо температуру, яку відчуває людина
        tft.setCursor(0, 60);  // Встановлюємо курсор на 70 пікселів нижче
        tft.printf("H: %.1f C L: %.1f C", maxTemperature, minTemperature);  // Мінімальна та максимальна температура
        tft.setCursor(0, 80);  // Встановлюємо курсор на 90 пікселів нижче
        tft.printf("Humidity: %.1f%%", humidity);  // Виводимо вологість
        tft.setCursor(0, 100);  // Встановлюємо курсор на 110 пікселів нижче
        tft.printf("Pressure: %.1f hPa", pressure);  // Виводимо тиск

        // Перетворюємо швидкість вітру з м/с в км/год
        float windSpeedKmh = windSpeed * 3.6;  // Переведення
        tft.setCursor(0, 120);  // Встановлюємо курсор на 130 пікселів нижче
        tft.printf("Wind: %.1f km/h %s", windSpeedKmh, windDirection.c_str());  // Виводимо швидкість вітру в км/год та напрямок
    } else {
        Serial.println("Using internal timer due to lack of connection.");
    }
}

// Функція для отримання даних про погоду
void fetchWeather() {
    if (WiFi.status() == WL_CONNECTED) {  // Перевіряємо, чи є Wi-Fi підключення
        HTTPClient http;  // Ініціалізуємо HTTP клієнт
        http.begin(apiUrl);  // Встановлюємо з'єднання з API

        int httpCode = http.GET();  // Виконуємо GET запит
        if (httpCode > 0) {  // Якщо код відповіді більший за 0, запит був успішний
            String payload = http.getString();  // Отримуємо відповідь від сервера
            Serial.println("Weather data: " + payload);  // Виводимо дані погоди

            DynamicJsonDocument doc(2048);  // Створюємо JSON документ для парсингу відповіді
            deserializeJson(doc, payload);  // Парсимо JSON

            // Отримуємо значення з JSON та зберігаємо їх у змінні
            currentTemperature = doc["main"]["temp"];
            feelsLikeTemperature = doc["main"]["feels_like"];
            minTemperature = doc["main"]["temp_min"];
            maxTemperature = doc["main"]["temp_max"];
            pressure = doc["main"]["pressure"];
            windSpeed = doc["wind"]["speed"];
            humidity = doc["main"]["humidity"];

            // Визначаємо напрямок вітру за допомогою градусів
            float windDeg = doc["wind"]["deg"];
            if (windDeg >= 0 && windDeg < 45) windDirection = "North";
            else if (windDeg >= 45 && windDeg < 90) windDirection = "East";
            else if (windDeg >= 90 && windDeg < 135) windDirection = "S/E";
            else if (windDeg >= 135 && windDeg < 180) windDirection = "South";
            else if (windDeg >= 180 && windDeg < 225) windDirection = "S/W";
            else if (windDeg >= 225 && windDeg < 270) windDirection = "West";
            else if (windDeg >= 270 && windDeg < 315) windDirection = "N";
            else windDirection = "North";
        } else {
            Serial.println("Failed to get weather data.");  // Якщо запит не вдалося виконати
        }

        http.end();  // Закриваємо HTTP з'єднання
    }
}

void setup() {
    Serial.begin(115200);  // Ініціалізація серійного монітора для виведення інформації
    WiFi.begin(ssid, password);  // Підключаємося до Wi-Fi мережі за налаштуваннями з файлу config.h

    // Чекаємо, поки підключимося до Wi-Fi
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500);
        Serial.print("."); // Поки не підключено, виводимо крапки
    }
    Serial.println(" WiFi connected.");  // Повідомлення про успішне підключення

    // Налаштовуємо час через NTP сервер
    configTime(2 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {  // Отримуємо локальний час
        Serial.println("Failed to get time from the internet");
        return;
    }
    Serial.println("Time synchronized with NTP server");  // Повідомлення про синхронізацію часу

    // Ініціалізація дисплею T-Display ESP32
    tft.init();
    tft.setRotation(1); // Повертаємо екран на 90 градусів
    tft.fillScreen(TFT_BLACK); // Заповнюємо екран чорним кольором

    fetchWeather();  // Отримуємо початкові дані погоди
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastWeatherUpdate >= weatherUpdateInterval) {  // Якщо пройшло 5 хвилин
        fetchWeather();  // Отримуємо нові дані про погоду
        lastWeatherUpdate = currentMillis;  // Оновлюємо час останнього оновлення погоди
    }

    displayDataOnTFT();  // Виводимо дані на дисплей T-Display ESP32
    delay(1000);  // Затримка 1 секунда
}
