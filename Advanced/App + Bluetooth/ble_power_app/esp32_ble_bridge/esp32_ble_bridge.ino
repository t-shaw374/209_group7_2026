/*
 * esp32_ble_bridge.ino  -  COMPSYS209 Team 07 energy monitor
 *
 * Transparent Bluetooth Low Energy <-> UART bridge.
 *
 * The ESP32 does NO calculations on the measurement data. It only:
 *   - forwards every byte the ATmega sends on its UART to the app (BLE notify)
 *   - forwards every byte the app writes to the ATmega's UART
 * All power/graph values are computed and formatted on the ATmega.
 *
 * BLE profile: Nordic UART Service (NUS), the de-facto standard "BLE serial".
 *   Service  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX char  6E400002-...  app  -> ESP32 -> ATmega   (write)
 *   TX char  6E400003-...  ATmega -> ESP32 -> app     (notify)
 *
 * Build: Arduino IDE, board package "esp32" by Espressif (2.x or 3.x),
 *        uses the BLE library that ships with the board package.
 *
 * Wiring (see README.md):
 *   ESP32 GPIO MEGA_RX_PIN  <-  P1 pin 4 (ATmega TX via R8/R10 divider, ~3.2 V)
 *   ESP32 GPIO MEGA_TX_PIN  ->  P1 pin 3 (ATmega RX, PD0)
 *   ESP32 GND               --  P1 pin 2 (GND)
 *   ESP32 power: its OWN USB cable, NOT the board's 5 V (78L05 is 100 mA max).
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------- user settings ----------
#define DEVICE_NAME  "EM-Team07"   // name shown when the app scans
#define MEGA_BAUD    9600          // must match the ATmega (F_CPU 2 MHz, UBRR 12)
#define MEGA_RX_PIN  16            // ESP32 pin receiving from the ATmega TX
#define MEGA_TX_PIN  17            // ESP32 pin sending to the ATmega RX
#define USB_DEBUG    1             // 1 = also echo ATmega data to the USB serial monitor
// ------------------------------------

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Default BLE packet payload is 20 bytes (MTU 23 - 3); safe on every phone/laptop.
#define CHUNK_SIZE   20
// Send a partial chunk if the UART has been quiet this long (ms).
#define FLUSH_MS     5

HardwareSerial MegaSerial(1);       // UART1 with custom pins: works on ESP32, C3, S3
BLECharacteristic *txChar = nullptr;
volatile bool appConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    appConnected = true;
  }
  void onDisconnect(BLEServer *server) override {
    appConnected = false;
    BLEDevice::startAdvertising();  // let the app reconnect
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    // App -> ATmega: pass bytes straight through, untouched.
    MegaSerial.write(c->getData(), c->getLength());
  }
};

void setup() {
  Serial.begin(115200);
  MegaSerial.begin(MEGA_BAUD, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(NUS_SERVICE_UUID);

  txChar = service->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  BLECharacteristic *rxChar = service->createCharacteristic(
      NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE bridge ready, advertising as " DEVICE_NAME);
}

void loop() {
  static uint8_t buf[CHUNK_SIZE];
  static size_t len = 0;
  static uint32_t lastByteMs = 0;

  // ATmega -> buffer
  while (MegaSerial.available() && len < CHUNK_SIZE) {
    buf[len++] = (uint8_t)MegaSerial.read();
    lastByteMs = millis();
  }

  // buffer -> app, when full or when the ATmega pauses
  if (len > 0 && (len == CHUNK_SIZE || millis() - lastByteMs >= FLUSH_MS)) {
    if (appConnected) {
      txChar->setValue(buf, len);
      txChar->notify();
    }
#if USB_DEBUG
    Serial.write(buf, len);
#endif
    len = 0;
  }
}
