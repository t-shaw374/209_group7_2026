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
 * Wiring, SMT board header P5 (see README.md); the TXS0108E does the 3.3/5 V shifting:
 *   P5-2  ESP_UART_RX -> GPIO MEGA_RX_PIN (16)     P5-3  ESP_UART_TX -> GPIO MEGA_TX_PIN (17)
 *   P5-9  5Vdc        -> VIN / 5V  (ESP32 power from the board's 5 V LD1117)
 *   P5-8  3.3VdcESP   -> 3V3       (ESP32's 3.3 V output powers the shifter's 3.3 V side)
 *   P5-1/10 GND       -> GND
 *   P5-4..7 (ATmega RESET/MOSI/MISO/SCK) -> GPIO 4/23/19/18 for SPI flashing.
 *   Those pins stay inputs (and RESET released) except while the app is flashing.
 *
 * Flashing over Bluetooth: a third characteristic (ISP, 6E400010-...) takes
 * programmer commands from the app's "Flash ATmega" panel; avr_isp.h does the
 * AVR serial programming over SPI. The UART bridge keeps working meanwhile
 * (the ATmega is simply held in reset while it is being programmed).
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
// ===== SPI programmer (avr_isp.h, included here so this is one file) =====
/*
 * avr_isp.h  -  ESP32 as an SPI (ISP) programmer for the ATmega328P / 328PB
 *
 * Uses the chip's "Serial Downloading" mode (ATmega328P datasheet, Memory
 * Programming chapter): hold RESET low, then send 4-byte instructions on
 * MOSI/SCK and read the answers on MISO. No bootloader is needed.
 *
 * The pins are bit-banged (not the SPI peripheral) so the clock can be slowed
 * right down: the ATmega needs SCK below F_CPU / 4, and it runs at 2 MHz
 * (16 MHz crystal / CKDIV8), so SCK must stay under 500 kHz. ISP_FAST_HALF_US
 * = 2 gives about 150-200 kHz; if the chip doesn't answer, it retries at about
 * 20 kHz (ISP_SLOW_HALF_US) in case it runs from a slower clock.
 *
 * RESET is never driven high: the RESET pin needs 0.9 * VCC = 4.5 V to count
 * as high, which a 3.3 V output can't give. The ESP32 pulls it to 0 V, and to
 * release it switches the pin to an input so the board's pull-up takes it to 5 V.
 *
 * Commands (from the app, first byte = opcode) and replies (opcode, status, ...):
 *   'E'                         enter programming mode -> 'E' st sig0 sig1 sig2 half_us
 *   'F'                         read fuses             -> 'F' st low high ext lock
 *   'X'                         chip erase             -> 'X' st
 *   'P' alo ahi d0..dn (n<=128) program + verify a page -> 'P' st alo ahi
 *   'L' off d0..dn              load part of a page into the ESP32's buffer -> 'L' st
 *   'W' alo ahi                 program + verify the buffered page          -> 'W' st alo ahi
 *   'R' alo ahi n (n<=128)      read n bytes of flash (backup)              -> 'R' st alo ahi d0..dn
 *   'Q'                         leave: release RESET so the ATmega runs     -> 'Q' st
 * addr = byte address in flash (little-endian), page-aligned (128 bytes).
 * st: 0 ok, 1 no answer / not in programming mode, 2 verify failed,
 *     3 bad request, 4 chip stayed busy.
 */

#ifndef AVR_ISP_H_
#define AVR_ISP_H_

#ifndef ISP_RESET_PIN
#define ISP_RESET_PIN 4
#endif
#ifndef ISP_MOSI_PIN
#define ISP_MOSI_PIN  23
#endif
#ifndef ISP_MISO_PIN
#define ISP_MISO_PIN  19
#endif
#ifndef ISP_SCK_PIN
#define ISP_SCK_PIN   18
#endif

#define ISP_FAST_HALF_US 2
#define ISP_SLOW_HALF_US 25
#define ISP_PAGE_BYTES   128        /* ATmega328P and 328PB: 64 words */
#define ISP_FLASH_BYTES  32768

enum { ISP_OK = 0, ISP_NO_SYNC = 1, ISP_VERIFY = 2, ISP_BAD = 3, ISP_BUSY = 4 };

static int     isp_half   = ISP_FAST_HALF_US;
static bool    isp_active = false;
static uint8_t isp_page[ISP_PAGE_BYTES];

static uint8_t isp_xfer(uint8_t out)
{
  uint8_t in = 0;
  for (int i = 7; i >= 0; i--) {
    digitalWrite(ISP_MOSI_PIN, (out >> i) & 1);  /* ATmega reads MOSI on the rising edge */
    delayMicroseconds(isp_half);
    digitalWrite(ISP_SCK_PIN, HIGH);
    delayMicroseconds(isp_half);
    in = (uint8_t)((in << 1) | (digitalRead(ISP_MISO_PIN) ? 1 : 0));
    digitalWrite(ISP_SCK_PIN, LOW);              /* ATmega changes MISO on the falling edge */
  }
  return in;
}

/* one 4-byte instruction; returns the 4th byte, *third gets the 3rd */
static uint8_t isp_cmd(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t *third = nullptr)
{
  isp_xfer(a);
  isp_xfer(b);
  uint8_t r3 = isp_xfer(c);
  if (third) *third = r3;
  return isp_xfer(d);
}

static void isp_reset_low(void)
{
  digitalWrite(ISP_RESET_PIN, LOW);
  pinMode(ISP_RESET_PIN, OUTPUT);
}

static void isp_reset_release(void)
{
  pinMode(ISP_RESET_PIN, INPUT);                 /* board pull-up -> 5 V */
}

/* all pins back to inputs: the ATmega uses PB3..PB5 for its own things */
static void isp_end(void)
{
  pinMode(ISP_MOSI_PIN, INPUT);
  pinMode(ISP_SCK_PIN, INPUT);
  pinMode(ISP_MISO_PIN, INPUT);
  isp_reset_release();
  isp_active = false;
}

static bool isp_wait_ready(uint32_t timeout_ms)
{
  uint32_t t0 = millis();
  do {
    if ((isp_cmd(0xF0, 0x00, 0x00, 0x00) & 1) == 0) return true;   /* RDY/BSY poll */
  } while (millis() - t0 < timeout_ms);
  return false;
}

static bool isp_sync(void)
{
  uint8_t echo = 0;
  isp_cmd(0xAC, 0x53, 0x00, 0x00, &echo);        /* Programming Enable */
  return echo == 0x53;
}

static bool isp_start(void)
{
  pinMode(ISP_MISO_PIN, INPUT);
  digitalWrite(ISP_SCK_PIN, LOW);
  pinMode(ISP_SCK_PIN, OUTPUT);
  digitalWrite(ISP_MOSI_PIN, LOW);
  pinMode(ISP_MOSI_PIN, OUTPUT);

  const int speeds[2] = { ISP_FAST_HALF_US, ISP_SLOW_HALF_US };
  for (int s = 0; s < 2; s++) {
    isp_half = speeds[s];
    for (int tries = 0; tries < 6; tries++) {
      /* datasheet: SCK low, then a positive pulse on RESET, then wait >= 20 ms */
      isp_reset_release();
      delay(2);
      isp_reset_low();
      delay(25);
      if (isp_sync()) { isp_active = true; return true; }
    }
  }
  isp_end();
  return false;
}

/* program the page in isp_page at byte address addr, then read it back */
static uint8_t isp_write_page(uint16_t addr)
{
  uint16_t base = addr >> 1;                     /* word address */
  for (uint16_t i = 0; i < ISP_PAGE_BYTES / 2; i++) {
    uint8_t w = (uint8_t)((base + i) & 0xFF);
    isp_cmd(0x40, 0x00, w, isp_page[2 * i]);       /* load low byte  */
    isp_cmd(0x48, 0x00, w, isp_page[2 * i + 1]);   /* load high byte */
  }
  isp_cmd(0x4C, (uint8_t)(base >> 8), (uint8_t)(base & 0xFF), 0x00);   /* write page */
  if (!isp_wait_ready(50)) return ISP_BUSY;

  for (uint16_t i = 0; i < ISP_PAGE_BYTES / 2; i++) {
    uint16_t w = base + i;
    if (isp_cmd(0x20, (uint8_t)(w >> 8), (uint8_t)(w & 0xFF), 0) != isp_page[2 * i])     return ISP_VERIFY;
    if (isp_cmd(0x28, (uint8_t)(w >> 8), (uint8_t)(w & 0xFF), 0) != isp_page[2 * i + 1]) return ISP_VERIFY;
  }
  return ISP_OK;
}

/* Handles one command from the app; fills resp (ISP_RESP_MAX bytes), returns its length. */
#define ISP_RESP_MAX (4 + ISP_PAGE_BYTES)
static size_t isp_handle(const uint8_t *cmd, size_t n, uint8_t *resp)
{
  if (n == 0) return 0;
  uint8_t op = cmd[0];
  resp[0] = op;
  resp[1] = ISP_OK;

  if (op == 'E') {
    if (!isp_start()) { resp[1] = ISP_NO_SYNC; return 2; }
    for (uint8_t i = 0; i < 3; i++) resp[2 + i] = isp_cmd(0x30, 0x00, i, 0x00);  /* signature */
    resp[5] = (uint8_t)isp_half;
    return 6;
  }
  if (op == 'Q') {
    isp_end();
    return 2;
  }
  if (!isp_active) { resp[1] = ISP_NO_SYNC; return 2; }

  switch (op) {
    case 'F':
      resp[2] = isp_cmd(0x50, 0x00, 0x00, 0x00);   /* low fuse      */
      resp[3] = isp_cmd(0x58, 0x08, 0x00, 0x00);   /* high fuse     */
      resp[4] = isp_cmd(0x50, 0x08, 0x00, 0x00);   /* extended fuse */
      resp[5] = isp_cmd(0x58, 0x00, 0x00, 0x00);   /* lock bits     */
      return 6;

    case 'X':
      isp_cmd(0xAC, 0x80, 0x00, 0x00);             /* chip erase */
      if (!isp_wait_ready(100)) resp[1] = ISP_BUSY;
      memset(isp_page, 0xFF, sizeof isp_page);
      return 2;

    case 'R': {                                     /* read flash, for backups */
      if (n < 4) { resp[1] = ISP_BAD; return 2; }
      uint16_t addr = (uint16_t)(cmd[1] | (cmd[2] << 8));
      size_t len = cmd[3];
      resp[2] = cmd[1];
      resp[3] = cmd[2];
      if (len > ISP_PAGE_BYTES || (uint32_t)addr + len > ISP_FLASH_BYTES) { resp[1] = ISP_BAD; return 4; }
      for (size_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)(addr + i), w = a >> 1;
        resp[4 + i] = isp_cmd((a & 1) ? 0x28 : 0x20, (uint8_t)(w >> 8), (uint8_t)(w & 0xFF), 0);
      }
      return 4 + len;
    }

    case 'L': {
      if (n < 2) { resp[1] = ISP_BAD; return 2; }
      size_t off = cmd[1], len = n - 2;
      if (off + len > ISP_PAGE_BYTES) { resp[1] = ISP_BAD; return 2; }
      memcpy(isp_page + off, cmd + 2, len);
      return 2;
    }

    case 'P':
    case 'W': {
      if (n < 3) { resp[1] = ISP_BAD; return 2; }
      uint16_t addr = (uint16_t)(cmd[1] | (cmd[2] << 8));
      resp[2] = cmd[1];
      resp[3] = cmd[2];
      size_t len = n - 3;
      if ((addr % ISP_PAGE_BYTES) != 0 || addr >= ISP_FLASH_BYTES ||
          len > ISP_PAGE_BYTES || (op == 'W' && len != 0)) { resp[1] = ISP_BAD; return 4; }
      if (op == 'P') {
        memset(isp_page, 0xFF, sizeof isp_page);
        memcpy(isp_page, cmd + 3, len);
      }
      resp[1] = isp_write_page(addr);
      memset(isp_page, 0xFF, sizeof isp_page);
      return 4;
    }
  }
  resp[1] = ISP_BAD;
  return 2;
}

#endif /* AVR_ISP_H_ */
// ===== end of SPI programmer =====

// ---------- user settings ----------
#define DEVICE_NAME  "EM-Team07"   // name shown when the app scans
#define MEGA_BAUD    9600          // must match the ATmega (F_CPU 2 MHz, UBRR 12)
#define MEGA_RX_PIN  16            // ESP32 pin receiving from the ATmega TX
#define MEGA_TX_PIN  17            // ESP32 pin sending to the ATmega RX
#define USB_DEBUG    1             // 1 = also echo ATmega data to the USB serial monitor
#define LOW_POWER    1             // 1 = 80 MHz CPU, low TX power, slow radio intervals
// ------------------------------------

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define ISP_UUID         "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"   // programmer (write + notify)

// Default BLE packet payload is 20 bytes (MTU 23 - 3); safe on every phone/laptop.
#define CHUNK_SIZE   20
// Send a partial chunk if the UART has been quiet this long (ms).
#define FLUSH_MS     5

HardwareSerial MegaSerial(1);       // UART1 with custom pins: works on ESP32, C3, S3
BLECharacteristic *txChar = nullptr;
BLECharacteristic *ispChar = nullptr;
BLEServer *bleServer = nullptr;
volatile bool appConnected = false;
esp_bd_addr_t appAddr;

// One programmer command at a time: the app waits for each reply before sending
// the next. The BLE callback only copies it; loop() does the slow SPI work.
static uint8_t ispCmd[ISP_PAGE_BYTES + 8];
static volatile size_t ispCmdLen = 0;
static volatile bool ispPending = false;

// Connection speed: slow (saves power) normally, fast while flashing.
// Units: interval 1.25 ms, timeout 10 ms.
void setLinkFast(bool fast) {
  if (!appConnected || !bleServer) return;
  if (fast) bleServer->updateConnParams(appAddr, 6, 12, 0, 400);      // 7.5-15 ms
#if LOW_POWER
  else      bleServer->updateConnParams(appAddr, 80, 160, 4, 600);    // 100-200 ms
#endif
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server, esp_ble_gatts_cb_param_t *param) override {
    appConnected = true;
    memcpy(appAddr, param->connect.remote_bda, sizeof appAddr);
#if LOW_POWER
    // Ask for a slow connection: radio wakes every 100-200 ms and may skip up to
    // 4 empty events (~1 s). Plenty for 1 reading/s; the phone may adjust it.
    // Units: interval 1.25 ms, timeout 10 ms.
    server->updateConnParams(param->connect.remote_bda, 80, 160, 4, 600);
#endif
  }
  void onDisconnect(BLEServer *server) override {
    appConnected = false;
    BLEDevice::startAdvertising();  // let the app reconnect
  }
};

class IspCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    size_t n = c->getLength();
    if (ispPending || n == 0 || n > sizeof ispCmd) return;   // app waits, so never busy
    memcpy(ispCmd, c->getData(), n);
    ispCmdLen = n;
    ispPending = true;
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    // App -> ATmega: pass bytes straight through, untouched.
    MegaSerial.write(c->getData(), c->getLength());
  }
};

void setup() {
#if LOW_POWER
  setCpuFrequencyMhz(80);           // lowest speed the Bluetooth stack allows
#endif
  Serial.begin(115200);
  MegaSerial.begin(MEGA_BAUD, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(247);           // lets a whole 128-byte flash page fit in one packet
#if LOW_POWER
  // -12 dBm transmit power: still a few metres of range on a bench.
  BLEDevice::setPower(ESP_PWR_LVL_N12, ESP_BLE_PWR_TYPE_DEFAULT);
  BLEDevice::setPower(ESP_PWR_LVL_N12, ESP_BLE_PWR_TYPE_ADV);
#endif
  BLEServer *server = BLEDevice::createServer();
  bleServer = server;
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(NUS_SERVICE_UUID);

  txChar = service->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  BLECharacteristic *rxChar = service->createCharacteristic(
      NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  ispChar = service->createCharacteristic(
      ISP_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ispChar->addDescriptor(new BLE2902());
  ispChar->setCallbacks(new IspCallbacks());
  isp_end();                        // programmer pins idle: inputs, RESET released

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
#if LOW_POWER
  adv->setMinInterval(800);         // advertise every 0.5-1 s while waiting (units 0.625 ms)
  adv->setMaxInterval(1600);
#endif
  BLEDevice::startAdvertising();

  Serial.println("BLE bridge ready, advertising as " DEVICE_NAME);
}

void loop() {
  static uint8_t buf[CHUNK_SIZE];
  static size_t len = 0;
  static uint32_t lastByteMs = 0;

  // ---- programmer command from the app ----
  if (ispPending) {
    static uint8_t resp[ISP_RESP_MAX];
    uint8_t op = ispCmd[0];
    if (op == 'E') setLinkFast(true);
    size_t n = isp_handle(ispCmd, ispCmdLen, resp);
    if (op == 'Q') setLinkFast(false);
#if USB_DEBUG
    if (op != 'P' && op != 'L' && op != 'W' && op != 'R')
      Serial.printf("[isp] %c -> status %u\n", op, resp[1]);
    else if (resp[1] != ISP_OK)
      Serial.printf("[isp] page %02X%02X failed, status %u\n", resp[3], resp[2], resp[1]);
#endif
    if (appConnected) {
      ispChar->setValue(resp, n);
      ispChar->notify();
    }
    ispPending = false;
  }
  if (isp_active && !appConnected) isp_end();   // app went away mid-flash: let the ATmega go

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

  // Let the CPU idle between checks instead of spinning flat out.
  // UART hardware buffers incoming bytes meanwhile (9600 baud = ~1 byte/ms).
  if (!isp_active) delay(2);
}
