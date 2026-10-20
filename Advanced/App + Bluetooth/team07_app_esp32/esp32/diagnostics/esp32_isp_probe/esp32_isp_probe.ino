/*
 * esp32_isp_probe.ino  -  diagnostic for the SPI (ISP) wiring to the ATmega
 *
 * Every 2 s it:
 *   1. reads the line levels with everything released,
 *   2. holds RESET low and sends "Programming Enable" (AC 53 00 00) a few times,
 *   3. prints the 4 bytes that came back on MISO, and the signature if it synced,
 *   4. releases RESET so the ATmega runs again.
 * The 3rd byte must come back as 53 when everything is right.
 *
 * Pins as in esp32_ble_bridge: GPIO4 RESET, 23 MOSI, 19 MISO (via divider), 18 SCK.
 * Serial Monitor at 115200.
 */

#define RST  4
#define MOSI_PIN 23
#define MISO_PIN 19
#define SCK_PIN  18
int halfUs = 2;

uint8_t xfer(uint8_t out) {
  uint8_t in = 0;
  for (int i = 7; i >= 0; i--) {
    digitalWrite(MOSI_PIN, (out >> i) & 1);
    delayMicroseconds(halfUs);
    digitalWrite(SCK_PIN, HIGH);
    delayMicroseconds(halfUs);
    in = (in << 1) | (digitalRead(MISO_PIN) ? 1 : 0);
    digitalWrite(SCK_PIN, LOW);
  }
  return in;
}

void releaseAll() {
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(MISO_PIN, INPUT);
  pinMode(RST, INPUT);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  releaseAll();
  Serial.println("\nESP32 ISP probe");
}

void loop() {
  releaseAll();
  delay(50);
  Serial.printf("\nidle levels (released): RESET=%d MISO=%d SCK=%d MOSI=%d\n",
                digitalRead(RST), digitalRead(MISO_PIN), digitalRead(SCK_PIN), digitalRead(MOSI_PIN));

  pinMode(MISO_PIN, INPUT);
  digitalWrite(SCK_PIN, LOW);  pinMode(SCK_PIN, OUTPUT);
  digitalWrite(MOSI_PIN, LOW); pinMode(MOSI_PIN, OUTPUT);
  digitalWrite(RST, LOW);      pinMode(RST, OUTPUT);
  delay(30);
  Serial.printf("RESET held low, MISO=%d\n", digitalRead(MISO_PIN));

  bool synced = false;
  const int speeds[2] = { 2, 25 };
  for (int s = 0; s < 2 && !synced; s++) {
    halfUs = speeds[s];
    for (int t = 0; t < 4 && !synced; t++) {
      uint8_t r[4];
      r[0] = xfer(0xAC); r[1] = xfer(0x53); r[2] = xfer(0x00); r[3] = xfer(0x00);
      Serial.printf("  %2d us  AC 53 00 00 -> %02X %02X %02X %02X  %s\n", halfUs,
                    r[0], r[1], r[2], r[3], r[2] == 0x53 ? "<- SYNC OK" : "");
      if (r[2] == 0x53) { synced = true; break; }
      pinMode(RST, INPUT); delay(2);                 // positive pulse on RESET, retry
      digitalWrite(RST, LOW); pinMode(RST, OUTPUT); delay(25);
    }
  }
  if (synced) {
    uint8_t sig[3];
    for (int i = 0; i < 3; i++) { xfer(0x30); xfer(0x00); xfer(i); sig[i] = xfer(0x00); }
    Serial.printf("  signature %02X %02X %02X  (1E 95 0F = ATmega328P, 1E 95 16 = 328PB)\n", sig[0], sig[1], sig[2]);
  }
  releaseAll();
  delay(2000);
}
