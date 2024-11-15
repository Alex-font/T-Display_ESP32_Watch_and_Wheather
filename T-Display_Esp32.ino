/**
 * Підключення необхідних бібліотек
 */
#include <TFT_eSPI.h>  // Бібліотека для роботи з дисплеєм T-Display ESP32
#include <WiFi.h>      // Бібліотека для роботи з Wi-Fi
#include <HTTPClient.h>// Бібліотека для відправки HTTP запитів
#include <ArduinoJson.h>// Бібліотека для роботи з JSON
#include <time.h>      // Бібліотека для роботи з часом
#include "config.h"    // Файл config.h містить налаштування Wi-Fi та API_KEY для OpenWeather

// Ініціалізація об'єктів та констант
TFT_eSPI tft = TFT_eSPI(); // Ініціалізація дисплею

// Константи для роботи з батареєю
const float MAX_BATTERY_VOLTAGE = 4.15;  // Максимальна напруга батареї
const float MIN_BATTERY_VOLTAGE = 3.3;  // Мінімальна напруга батареї
const int BATTERY_PIN = 34;             // PIN для зчитування напруги батареї

// URL для отримання даних про погоду
const String apiUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + CITY_NAME + "," + COUNTRY_CODE + "&units=metric&appid=" + String(API_KEY);

// Змінні для збереження отриманих даних про погоду
float currentTemperature = 0.0;    // Поточна температура
float feelsLikeTemperature = 0.0;  // Температура, яку відчуває людина
float minTemperature = 0.0;       // Мінімальна температура дня
float maxTemperature = 0.0;       // Максимальна температура дня
float windSpeed = 0.0;            // Швидкість вітру в м/с
String windDirection = "Unknown"; // Напрямок вітру
float pressure = 0.0;             // Атмосферний тиск
float humidity = 0.0;             // Вологість

// Змінні для контролю оновлення даних
unsigned long lastWeatherUpdate = 0; // Час останнього оновлення погоди
const unsigned long weatherUpdateInterval = 300000; // Інтервал між оновленнями погоди (5 хвилин)
unsigned long lastBatteryUpdate = 0; // Час останнього оновлення батареї
const unsigned long batteryUpdateInterval = 5000; // Інтервал між оновленнями батареї (5 секунд)

// Змінні для збереження попередніх значень
float previousTemperature = -999.0;
float previousVoltage = -999.0;
float previousFeelsLikeTemperature = -999.0;
int previousBatteryPercentage = -1;
float previousMaxTemperature = -999.0;
float previousMinTemperature = -999.0;
float previousHumidity = -999.0;
float previousPressure = -999.0;
String previousWindDirection = "Unknown";
float previousWindSpeedKmh = -999.0;

/**
 * Функція отримання даних про погоду з OpenWeather API
 * Виконує HTTP запит та парсить отриману JSON відповідь
 * Оновлює глобальні змінні з даними про погоду
 */
void fetchWeather() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(apiUrl);

        int httpCode = http.GET();
        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("Weather data: " + payload);

            DynamicJsonDocument doc(2048);
            deserializeJson(doc, payload);

            // Оновлення змінних погоди
            currentTemperature = doc["main"]["temp"];
            feelsLikeTemperature = doc["main"]["feels_like"];
            minTemperature = doc["main"]["temp_min"];
            maxTemperature = doc["main"]["temp_max"];
            pressure = doc["main"]["pressure"];
            windSpeed = doc["wind"]["speed"];
            humidity = doc["main"]["humidity"];

            // Визначення напрямку вітру
            float windDeg = doc["wind"]["deg"];
            if (windDeg >= 0 && windDeg < 45) windDirection = "North";
            else if (windDeg >= 45 && windDeg < 90) windDirection = "East";
            else if (windDeg >= 90 && windDeg < 135) windDirection = "S/E";
            else if (windDeg >= 135 && windDeg < 180) windDirection = "South";
            else if (windDeg >= 180 && windDeg < 225) windDirection = "S/W";
            else if (windDeg >= 225 && windDeg < 270) windDirection = "West";
            else if (windDeg >= 270 && windDeg < 315) windDirection = "N/W";
            else windDirection = "North";
        } else {
            Serial.println("Failed to get weather data.");
        }

        http.end();
    }
}

/**
 * Функція отримання напруги батареї
 * @return float - Поточна напруга батареї у вольтах
 * 
 * Робить 10 вимірювань та усереднює їх для більшої точності
 * Враховує дільник напруги на платі (тому множимо на 2)
 */
float getBatteryVoltage() {
    float sum = 0;
    for(int i = 0; i < 10; i++) {
        sum += analogRead(BATTERY_PIN);
        delay(10);
    }
    float averageReading = sum / 10;
    float voltage = (averageReading / 4095.0) * 3.3 * 2;
    return voltage;
}

/**
 * Функція розрахунку відсотка заряду батареї
 * @param voltage - Поточна напруга батареї
 * @return int - Відсоток заряду (0-100%)
 * 
 * Використовує лінійну апроксимацію між мінімальною та максимальною напругою
 * Обмежує значення діапазоном 0-100%
 */
int getBatteryPercentage(float voltage) {
    int percentage = (voltage - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
    if(percentage > 100) percentage = 100;
    if(percentage < 0) percentage = 0;
    return percentage;
}

/**
 * Функція виведення всіх даних на дисплей
 * Відображає:
 * - Час та дату
 * - Температуру та стан батареї
 * - Відчутну температуру та індикатор заряду
 * - Інші метеорологічні дані
 */
void displayDataOnTFT() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        const char* daysOfWeek[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        int dayIndex = (timeinfo.tm_wday + 6) % 7;

        // Оновлення годинника кожну секунду
        tft.setCursor(0, 0);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setTextSize(2); // Задаємо розмір шрифта 2
        tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        tft.setCursor(100, 0);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("%s", daysOfWeek[dayIndex]);

        tft.setCursor(140, 0);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.printf("%02d/%02d/%02d\n", timeinfo.tm_mday, timeinfo.tm_mon + 1, (timeinfo.tm_year + 1900) % 100);

        // Оновлення даних про батарею кожні 5 секунд
        if (millis() - lastBatteryUpdate >= batteryUpdateInterval) {
            float voltage = getBatteryVoltage();
            int percentage = getBatteryPercentage(voltage);

            // Оновлення температури та напруги батареї
            if (currentTemperature != previousTemperature) {
                tft.fillRect(0, 20, 240, 20, TFT_BLACK);
                tft.setCursor(0, 20);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.printf("Temp:  %.0f C", currentTemperature);
                previousTemperature = currentTemperature; // Оновлюємо попереднє значення
            }
            if (voltage != previousVoltage) {
                tft.setCursor(160, 20);
                tft.setTextColor(TFT_OLIVE, TFT_BLACK);
                tft.printf("B:%.1fV", voltage);
                previousVoltage = voltage; // Оновлюємо попереднє значення
            }

            // Оновлення відчутної температури
            if (feelsLikeTemperature != previousFeelsLikeTemperature) {
                tft.fillRect(0, 40, 240, 20, TFT_BLACK);
                tft.setCursor(0, 40);
                tft.setTextColor(0xFFE0, TFT_BLACK);
                tft.printf("Feels: %.0f C", feelsLikeTemperature);
                previousFeelsLikeTemperature = feelsLikeTemperature; // Оновлюємо попереднє значення
            }

            // Малювання індикатора батареї
            int barX = 160;
            int barY = 40;
            int barWidth = 70;
            int barHeight = 15;

            uint16_t frameColor;
            if (percentage >= 99) {
                frameColor = TFT_RED;
            } else if (percentage >= 20) {
                frameColor = TFT_WHITE;
            } else {
                frameColor = TFT_BLUE;
            }

            tft.drawRect(barX, barY, barWidth, barHeight, frameColor);
            tft.fillRect(barX + barWidth, barY + 4, 3, 7, frameColor);

            int fillWidth = (percentage * (barWidth - 4)) / 100;
            uint16_t fillColor;
            if (percentage >= 99) {
                fillColor = TFT_GREEN;
            } else if (percentage >= 20) {
                fillColor = TFT_GREEN;
            } else {
                fillColor = TFT_RED;
            }
            tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, fillColor);

            previousBatteryPercentage = percentage; // Оновлюємо попереднє значення
            lastBatteryUpdate = millis();
        }

        // Оновлення погодних даних кожні 5 хвилин
        if (millis() - lastWeatherUpdate >= weatherUpdateInterval) {
            fetchWeather();
            lastWeatherUpdate = millis();
        }

        // Виведення інших метеорологічних даних

        if (maxTemperature != previousMaxTemperature) {

            tft.fillRect(0, 60, 240, 20, TFT_BLACK);

            tft.setCursor(0, 60);

            tft.setTextColor(TFT_BLUE, TFT_BLACK);

            tft.printf("Hi: %.0f C", maxTemperature);

            previousMaxTemperature = maxTemperature;

        } else {

            tft.setCursor(0, 60);

            tft.setTextColor(TFT_BLUE, TFT_BLACK);

            tft.printf("Hi: %.0f C", previousMaxTemperature);

        }


        if (minTemperature != previousMinTemperature) {

            tft.fillRect(120, 60, 120, 20, TFT_BLACK);

            tft.setCursor(120, 60);

            tft.setTextColor(TFT_RED, TFT_BLACK);

            tft.printf("Low: %.0f C", minTemperature);

            previousMinTemperature = minTemperature;

        } else {

            tft.setCursor(120, 60);

            tft.setTextColor(TFT_RED, TFT_BLACK);

            tft.printf("Low: %.0f C", previousMinTemperature);

        }


        if (humidity != previousHumidity) {

            tft.fillRect(0, 80, 240, 20, TFT_BLACK);

            tft.setCursor(0, 80);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);

            tft.printf("Humidity: %.0f%%", humidity);

            previousHumidity = humidity;

        } else {

            tft.setCursor(0, 80);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);

            tft.printf("Humidity: %.0f%%", previousHumidity);

        }


        if (pressure != previousPressure) {

            tft.fillRect(0, 100, 240, 20, TFT_BLACK);

            tft.setCursor(0, 100);

            tft.printf("Pressure: %.0f hPa", pressure);

            previousPressure = pressure;

        } else {

            tft.setCursor(0, 100);

            tft.printf("Pressure: %.0f hPa", previousPressure);

        }


        float windSpeedKmh = windSpeed * 3.6;

        if (windSpeedKmh != previousWindSpeedKmh) {

            tft.fillRect(0, 120, 240, 20, TFT_BLACK);

            tft.setCursor(0, 120);

            tft.setTextColor(0xB7E0, TFT_BLACK);

            tft.printf("Wind: %.1f km/h %s", windSpeedKmh, windDirection.c_str());

            previousWindSpeedKmh = windSpeedKmh;

        } else {

            tft.setCursor(0, 120);

            tft.setTextColor(0xB7E0, TFT_BLACK);

            tft.printf("Wind: %.1f km/h %s", previousWindSpeedKmh, windDirection.c_str());

        }

    } else {

        Serial.println("Using internal timer due to lack of connection.");

    }
    }

/**
 * Функція налаштування
 * Виконується один раз при старті
 * Ініціалізує всі компоненти та підключення
 */
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
 Serial.print(".");
    }
    Serial.println(" WiFi connected.");

    configTime(2 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to get time from the internet");
        return;
    }
    Serial.println("Time synchronized with NTP server");

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK); // Налаштування АЦП для вимірювання напруги батареї
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    
    fetchWeather();
    displayDataOnTFT(); // Відображення початкових даних на дисплеї
}

/**
 * Головний цикл програми
 * Оновлює дисплей кожну секунду
 */
void loop() {
    displayDataOnTFT();
    delay(1000);
}
