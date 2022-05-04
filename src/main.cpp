#include <Arduino.h>
#include <SSD1306.h>
#include <BLEDevice.h>

#include "uuid.h"

#define PIN_OLED_SDA 4
#define PIN_OLED_SCL 15
#define PIN_OLED_RST 16

#define PUMP_BLE_NAME "Insulin Pump"

#define BOLUS_DISPLAY_TIME 5

SSD1306 display(0x3c, PIN_OLED_SDA, PIN_OLED_SCL);

BLEServer *pumpServer;
BLEService *pumpService;
BLECharacteristic *basalCharacteristic;
BLECharacteristic *bolusCharacteristic;
BLECharacteristic *insulinTimeCharacteristic;

bool newBasal = false;
bool newBolus = false;
int32_t simulationTime = 0;
int32_t basalValue = 0;
int32_t bolusValue = 0;
int32_t timeSinceStart = 0;
int32_t bolusLastTime = 0;

char serialBuffer[32];

class PumpServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
    }

    void onDisconnect(BLEServer *pServer) {
      pServer->getAdvertising()->start();
    }
};

class BasalCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      newBasal = true;
    }
};

class BolusCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      newBolus = true;
    }
};

void drawScreen() {
  char screenBuffer[128];

  display.clear();

  display.setFont(ArialMT_Plain_16);
  sprintf(screenBuffer, "Basal: %.2f", basalValue / 100.0);
  display.drawString(64, 2, screenBuffer);
  if ((timeSinceStart - bolusLastTime) < 5) {
    sprintf(screenBuffer, "Bolus: %.2f", bolusValue / 100.0);
    display.drawString(64, 20, screenBuffer);
  }

  display.setFont(ArialMT_Plain_10);
  sprintf(screenBuffer, "Simulation time: %02d:%02d", (simulationTime % 86400) / 3600, ((simulationTime % 86400) % 3600) / 60);
  display.drawString(64, 52, screenBuffer);

  display.display();
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(PIN_OLED_RST, OUTPUT);
  digitalWrite(PIN_OLED_RST, LOW);
  delay(50);
  digitalWrite(PIN_OLED_RST, HIGH);
  
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  BLEDevice::init(PUMP_BLE_NAME);

  pumpServer = BLEDevice::createServer();
  pumpServer->setCallbacks(new PumpServerCallbacks());

  pumpService = pumpServer->createService(PUMP_SERVICE_UUID);

  basalCharacteristic = new BLECharacteristic(BASAL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  basalCharacteristic->setCallbacks(new BasalCharacteristicCallbacks());
  bolusCharacteristic = new BLECharacteristic(BOLUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  bolusCharacteristic->setCallbacks(new BolusCharacteristicCallbacks());
  insulinTimeCharacteristic = new BLECharacteristic(INSULIN_TIME_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);

  pumpService->addCharacteristic(basalCharacteristic);
  pumpService->addCharacteristic(bolusCharacteristic);
  pumpService->addCharacteristic(insulinTimeCharacteristic);
  pumpService->start();

  pumpServer->getAdvertising()->start();

  display.clear();
  display.drawStringMaxWidth(64, 22, 128, "Device started...");
  display.display();
  delay(1500);
}

void loop() {
  if (newBasal) {
    simulationTime = atoi(insulinTimeCharacteristic->getValue().c_str());
    basalValue = atoi(basalCharacteristic->getValue().c_str());
    sprintf(serialBuffer, "SET_BASAL;%d;%d", simulationTime, basalValue);
    Serial.println(serialBuffer);
    newBasal = false;
  }
  if (newBolus) {
    simulationTime = atoi(insulinTimeCharacteristic->getValue().c_str());
    bolusValue = atoi(bolusCharacteristic->getValue().c_str());
    sprintf(serialBuffer, "SET_BOLUS;%d;%d", simulationTime, bolusValue);
    Serial.println(serialBuffer);

    bolusLastTime = timeSinceStart;
    newBolus = false;
  }

  drawScreen();
  
  timeSinceStart++;
  delay(1000);
}