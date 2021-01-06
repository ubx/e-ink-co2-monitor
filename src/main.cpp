#include <Arduino.h>
#include <Wire.h>

#include <GxGDEH0213B72/GxGDEH0213B72.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxEPD.h>

#include "SPI.h"
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Adafruit_BMP280.h>
#include <DHT12.h>
#include <Tone32.h>

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18

#define ELINK_SS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

//#define PRINT_VALUES
//#define FORCE_RECALIBRATION

typedef enum {
    RIGHT_ALIGNMENT = 0,
    LEFT_ALIGNMENT,
    CENTER_ALIGNMENT,
} Text_alignment;

const uint8_t vbatPin = 35;
const uint8_t tonePin = 25;

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ ELINK_RESET, /*BUSY=*/ ELINK_BUSY);

const uint8_t Whiteboard[1700] = {0x00};

// sensors
SCD30 scd30;
Adafruit_BMP280 bmp280;
DHT12 dht12(&Wire);

void displayText(const String &str, uint16_t y, uint8_t alignment) {
    int16_t x = 0;
    int16_t x1, y1;
    uint16_t w, h;
    display.setCursor(x, y);
    display.getTextBounds(str, x, y, &x1, &y1, &w, &h);

    switch (alignment) {
        case RIGHT_ALIGNMENT:
            //display.setCursor(display.width() - w - x1, y);
            display.setCursor(180, y);
            break;
        case LEFT_ALIGNMENT:
            display.setCursor(30, y);
            break;
        case CENTER_ALIGNMENT:
            display.setCursor(display.width() / 2 - ((w + x1) / 2), y);
            break;
        default:
            break;
    }
    display.println(str);
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("setup...");

    // display setup
    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
    display.init(); // enable diagnostic output on Serial

    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.fillScreen(GxEPD_WHITE);
    display.update();

    // sensor setup
    Serial.println("SCD30 setup...");
    Wire.begin();
    if (!scd30.begin()) {
        Serial.println("fail!");
        Serial.println("SCD30 not detected. Please check wiring. Freezing...");
        while (true);
    }
    scd30.setAutoSelfCalibration(false);
    //airSensor.setForcedRecalibrationFactor(400);

    Serial.println("BMP280 setup...");
    if (!bmp280.begin(0x76)) {
        Serial.println("BMP280 not detected. Please check wiring. Freezing...");
        while (true);
    }

    Serial.println("DHT12 setup...");
    dht12.begin();
    if (dht12.read() != 0) {
        Serial.println("DHT12 not detected. Please check wiring. Freezing...");
        while (true);
    }
    Serial.println("...done");
}

int cnt = 0;
bool bat = true;
float newTempOffset = 0.0;

void loop() {
    sleep(1);
    // get sensor data
    auto bmp280_pres = bmp280.readPressure() / 100.0;
    auto scd30_temp = scd30.getTemperature();
    auto scd30_humi = scd30.getHumidity();
    dht12.read();

    float oldTempOffset = newTempOffset;
    newTempOffset = (scd30_temp - dht12.getTemperature()) + oldTempOffset;
    if (newTempOffset < 0.0) newTempOffset = 0.0;
    scd30.setTemperatureOffset(newTempOffset);

    if (cnt % 60 == 0) {
        scd30.setAmbientPressure(bmp280_pres);
    }

    auto scd30_co2 = scd30.getCO2();
    display.setFont(&FreeMonoBold24pt7b);
    displayText(String(scd30_co2), 40, LEFT_ALIGNMENT);
    display.setFont(&FreeMonoBold12pt7b);
    displayText(String(bmp280_pres, 1), 70, LEFT_ALIGNMENT);
    displayText(String(dht12.getTemperature(), 1), 90, LEFT_ALIGNMENT);
    displayText(String(scd30_humi, 1), 110, LEFT_ALIGNMENT);

    display.setFont(&FreeMono9pt7b);

    if (bat) {
        // Battery Voltage
        auto vbat = (float) (analogRead(vbatPin)) / 4095 * 2 * 3.3 * 1.1;
        displayText(String(vbat, 2), 110, RIGHT_ALIGNMENT);
    } else {
        displayText(String("----"), 110, RIGHT_ALIGNMENT);
    }
    bat = !bat;

    display.updateWindow(0, 0, 222, 125, true);
    display.fillScreen(GxEPD_WHITE);

    if (scd30_co2 > 1100) {
        tone(tonePin, 4000, 500, 0);
    }

#ifdef PRINT_VALUES
    Serial.print("co2(ppm):");
    Serial.print(scd30_co2);

    Serial.print(" temp_scd30(C):");
    Serial.print(scd30_temp, 1);

    Serial.print(" humidity_scd30(%):");
    Serial.print(scd30_humi, 1);

    Serial.print(" pressure(hPa):");
    Serial.print(bmp280_pres, 1);

    Serial.print(" temp_bme(C):");
    Serial.print(bmp280.readTemperature(), 1);

    Serial.print(" temp_dht12(C):");
    Serial.print(dht12.getTemperature(), 1);

    Serial.print(" humidity_dht12(%):");
    Serial.print(dht12.getHumidity(), 1);

    Serial.print(" tempOffset(C):");
    Serial.print(oldTempOffset, 2);
    Serial.print(" / ");
    Serial.print(newTempOffset, 2);
    Serial.print(" / ");
    Serial.print(scd30.getTemperatureOffset(), 2);

    Serial.println();
#endif

    cnt++;

#ifdef FORCE_RECALIBRATION
    Serial.println("scd30 sensor FRC waiting...");
    // initilal FRC
    if (cnt == 1800) {
        scd30.setForcedRecalibrationFactor(400);
        Serial.println("scd30 sensor FRC is active!!");
    }
#endif
}