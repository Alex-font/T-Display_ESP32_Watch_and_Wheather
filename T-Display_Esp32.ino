#include <TFT_eSPI.h>  // Library for working with T-Display ESP32 display
#include <WiFi.h>      // Library for working with Wi-Fi
#include <HTTPClient.h>// Library for sending HTTP requests
#include <ArduinoJson.h>// Library for working with JSON
#include <time.h>      // Library for working with time
#include "config.h"    // config.h file contains Wi-Fi configuration and API_KEY for OpenWeather

TFT_eSPI tft = TFT_eSPI(); // Initialize the display

const String apiUrl = "http://api.openweathermap.org/data/2.5/weather?q=Kyiv,UA&units=metric&appid=" + String(API_KEY);
// URL for getting weather data through the OpenWeather API for Kyiv, UA

// Variables for storing the received weather data
float currentTemperature = 0.0;    // Current temperature
float feelsLikeTemperature = 0.0;  // Temperature that the person feels
float minTemperature = 0.0;       // Minimum temperature for the day
float maxTemperature = 0.0;       // Maximum temperature for the day
float windSpeed = 0.0;            // Wind speed in m/s
String windDirection = "Unknown"; // Wind direction
float pressure = 0.0;             // Atmospheric pressure
const char* currentWeatherDescription = "Unknown"; // Description of the current weather
float precipitation = 0.0;        // Amount of precipitation
float humidity = 0.0;             // Humidity

unsigned long lastWeatherUpdate = 0; // Time of the last weather update
const unsigned long weatherUpdateInterval = 300000; // Interval between weather updates (5 minutes)

// Функція для виведення даних на дисплей T-Display ESP32
void displayDataOnTFT() {
    tft.fillScreen(TFT_BLACK);  // Clear the display

    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set white text color on black background
    tft.setTextSize(2);  // Text size

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {  // Check if the time is available
        const char* daysOfWeek[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        int dayIndex = (timeinfo.tm_wday + 6) % 7;  // Determine the day of the week
        tft.setCursor(0, 0);  // Set the cursor to the top left corner
        tft.printf("%02d:%02d:%02d %s %02d/%02d/%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, daysOfWeek[dayIndex], timeinfo.tm_mday, timeinfo.tm_mon + 1, (timeinfo.tm_year + 1900) % 100);
        tft.setCursor(0, 20);  // Set the cursor 30 pixels down
        tft.printf("Temp: %.1f C", currentTemperature);  // Display the current temperature
        tft.setCursor(0, 40);  // Set the cursor 50 pixels down
        tft.printf("Feels: %.1f C", feelsLikeTemperature);  // Display the feels-like temperature
        tft.setCursor(0, 60);  // Set the cursor 70 pixels down
        tft.printf("H: %.1f C L: %.1f C", maxTemperature, minTemperature);  // Minimum and maximum temperature
        tft.setCursor(0, 80);  // Set the cursor 90 pixels down
        tft.printf("Humidity: %.1f%%", humidity);  // Display the humidity
        tft.setCursor(0, 100);  // Set the cursor 110 pixels down
        tft.printf("Pressure: %.1f hPa", pressure);  // Display the pressure

        // Convert wind speed from m/s to km/h
        float windSpeedKmh = windSpeed * 3.6;  // Conversion
        tft.setCursor(0, 120);  // Set the cursor 130 pixels down
        tft.printf("Wind: %.1f km/h %s", windSpeedKmh, windDirection.c_str());  // Display the wind speed in km/h and direction
    } else {
        Serial.println("Using internal timer due to lack of connection.");
    }
}

// Функція для отримання даних про погоду
void fetchWeather() {
    if (WiFi.status() == WL_CONNECTED) {  // Check if there is a Wi-Fi connection
        HTTPClient http;  // Initialize the HTTP client
        http.begin(apiUrl);  // Set the connection to the API

        int httpCode = http.GET();  // Perform the GET request
        if (httpCode > 0) {  // If the response code is greater than 0, the request was successful
            String payload = http.getString();  // Get the response from the server
            Serial.println("Weather data: " + payload);  // Print the weather data

            DynamicJsonDocument doc(2048);  // Create a JSON document for parsing the response
            deserializeJson(doc, payload);  // Parse the JSON

            // Get the values from the JSON and store them in variables
            currentTemperature = doc["main"]["temp"];
            feelsLikeTemperature = doc["main"]["feels_like"];
            minTemperature = doc["main"]["temp_min"];
            maxTemperature = doc["main"]["temp_max"];
            pressure = doc["main"]["pressure"];
            windSpeed = doc["wind"]["speed"];
            humidity = doc["main"]["humidity"];

            // Determine the wind direction based on degrees
            float windDeg = doc["wind"]["deg"];
            if (windDeg >= 0 && windDeg < 45) windDirection = "North";
            else if (windDeg >= 45 && windDeg < 90) windDirection = "East";
            else if (windDeg >= 90 && windDeg < 135) windDirection = "S/E";
            else if (windDeg >= 135 && windDeg < 180) windDirection = "South";
            else if (windDeg >= 180 && windDeg < 225) windDirection = "S/W";
            else if (windDeg >= 225 && windDeg < 270) windDirection = "West";
            else if (windDeg >= 270 && windDeg < 315) windDirection = "N";
            else windDirection = "North";

            // Get the amount of precipitation, if any
            if (doc.containsKey("rain")) {
                precipitation = doc["rain"]["1h"];
            } else {
                precipitation = 0;
            }

            // Weather description
            const char* weatherId = doc["weather"][0]["description"];
            if (strcmp(weatherId, "few clouds") == 0) {
                currentWeatherDescription = "few clouds";
            } else if (strcmp(weatherId, "clear sky") == 0) {
                currentWeatherDescription = "clear sky";
            } else if (strcmp(weatherId, "overcast clouds") == 0) {
                currentWeatherDescription = "overcast clouds";
            } else if (strcmp(weatherId, "light rain") == 0) {
                currentWeatherDescription = "light rain";
            } else if (strcmp(weatherId, "moderate rain") == 0) {
                currentWeatherDescription = "moderate rain";
            } else if (strcmp(weatherId, "heavy rain") == 0) {
                currentWeatherDescription = "heavy rain";
            } else if (strcmp(weatherId, "scattered clouds") == 0) {
                currentWeatherDescription = "scattered clouds";
            } else if (strcmp(weatherId, "broken clouds") == 0) {
                currentWeatherDescription = "broken clouds";
            } else {
                currentWeatherDescription = "other";
            }
        } else {
            Serial.println("Failed to get weather data.");  // If the request failed
        }

        http.end();  // Close the HTTP connection
    }
}

void setup() {
    Serial.begin(115200);  // Initialize the serial monitor for output
    WiFi.begin(ssid, password);  // Connect to the Wi-Fi network using the settings from the config.h file

    // Wait until we connect to Wi-Fi
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500);
        Serial.print("."); // While not connected, print dots
    }
    Serial.println(" WiFi connected.");  // Print a message when the connection is established

    // Set the time using the NTP server
    configTime(2 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {  // Get the local time
        Serial.println("Failed to get time from the internet");
        return;
    }
    Serial.println("Time synchronized with NTP server");  // Message about successful time synchronization

    // Initialize the T-Display ESP32 display
    tft.init();
    tft.setRotation(1); // Rotate the screen 90 degrees
    tft.fillScreen(TFT_BLACK); // Fill the display with black color

    fetchWeather();  // Get the initial weather data
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastWeatherUpdate >= weatherUpdateInterval) {  // If 5 minutes have passed
        fetchWeather();  // Get the new weather data
        lastWeatherUpdate = currentMillis;  // Update the time of the last weather update
    }

    displayDataOnTFT();  // Display the data on the T-Display ESP32 display
    delay(1000);  // Delay for 1 second
}