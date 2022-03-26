#include <Arduino.h>
#include <DHT.h>
#include <RCSwitch.h>
#include "main.h"
#include <LedControl.h>

// todo:
// Set state to unknown every hour or so
// Add display for temperature
// Add potentiometer for setting target temp

#define N_DIGITS 8
const int warmButtonPin = 4;
const int coldButtonPin = 7;
DHT dht(2, DHT11);
RCSwitch mySwitch = RCSwitch();
LedControl lc = LedControl(12, 13, 11, 1);

const char *onCode = "010011000100110101101111";
const char *offCode = "010011000100110101101110";
const float temperatureMargin = 1;
enum PowerState {
    on, off, unknown
};
PowerState state = PowerState::unknown;

void setup() {
    Serial.begin(9600);
    Serial.println("Started Thermostaat!");

    pinMode(warmButtonPin, INPUT_PULLUP);
    pinMode(coldButtonPin, INPUT_PULLUP);

    mySwitch.enableTransmit(10);
    mySwitch.setPulseLength(309);
    mySwitch.setProtocol(1);
    mySwitch.setRepeatTransmit(3);

    // we have to do a wakeup call
    lc.shutdown(0, false);
    lc.setIntensity(0, 8);
    lc.clearDisplay(0);

    dht.begin();
}

void setDegreeSymbol(int digit) {
    lc.setLed(0, digit, 1, true);
    lc.setLed(0, digit, 2, true);
    lc.setLed(0, digit, 6, true);
    lc.setLed(0, digit, 7, true);
    lc.setLed(0, digit, 0, false);
    lc.setLed(0, digit, 3, false);
    lc.setLed(0, digit, 4, false);
    lc.setLed(0, digit, 5, false);
}

void setDisplay(const String &values) {
    for (int i = 0; i < N_DIGITS; i++) {
        int digitIndex = N_DIGITS - 1 - i;
        if (i < values.length()) {
            auto character = values.charAt(i);
            if (character == '*') {
                setDegreeSymbol(digitIndex);
            } else {
                lc.setChar(0, digitIndex, values.charAt(i), false);
            }
        } else {
            lc.setChar(0, digitIndex, ' ', false);
        }
    }
}

float targetTemperature = 24;
float humidity = 0;
float temperature = 0;
float heatIndex = 0;
String targetTempString = "";
String temperatureString = "";
String displayString = "";
unsigned int loopIndex = 0;
const int showTargetTicks = 300;
unsigned int showUserChangeUntil = 0;
unsigned int noButtonPressUntil = 0;

void readTemperature() {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    temperatureString = String(heatIndex, 1) + "*C";

    if (heatIndex < targetTemperature - temperatureMargin && state != PowerState::on) {
        Serial.println("Turning on heater!");
        mySwitch.send(onCode);
        state = PowerState::on;
    }
    if (heatIndex > targetTemperature + temperatureMargin && state != PowerState::off) {
        Serial.println("Turning off heater!");
        mySwitch.send(offCode);
        state = PowerState::off;
    }
}

void loop() {
    delay(10);
    loopIndex++;

    if (loopIndex < showUserChangeUntil) {
        if (displayString != targetTempString) {
            displayString = targetTempString;
            setDisplay(displayString);
        }
    } else {
        if (displayString != temperatureString) {
            displayString = temperatureString;
            setDisplay(displayString);
        }
    }

    // Invert HIGH/LOW button logic because of internal pull up resistor
    bool userChanged = false;
    auto warmButtonValue = digitalRead(warmButtonPin);
    auto coldButtonValue = digitalRead(coldButtonPin);
    if (warmButtonValue == LOW && loopIndex >= noButtonPressUntil) {
        if (loopIndex < showUserChangeUntil)
            targetTemperature += .5;
        userChanged = true;
    }
    if (coldButtonValue == LOW && loopIndex >= noButtonPressUntil) {
        if (loopIndex < showUserChangeUntil)
            targetTemperature -= .5;
        userChanged = true;
    }
    // if both button not pressed
    if (warmButtonValue == HIGH && coldButtonValue == HIGH) {
        noButtonPressUntil = loopIndex;
    }

    if (userChanged) {
        noButtonPressUntil = loopIndex + 25;
        showUserChangeUntil = loopIndex + showTargetTicks;
        targetTempString = "  " + String(targetTemperature, 1) + "*C";
        Serial.println("User changed target temp: " + targetTempString);
    }

    if (loopIndex % 200 == 0)
        readTemperature();
}