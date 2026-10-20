/*
 * esp32_uart_echo.ino  -  ESP32 side of the UART round-trip test
 *
 * - "PING ..." lines from the ATmega are sent straight back (the round trip).
 * - Every other line (the ATmega's "RESULT ..." lines) is only printed.
 * Everything is printed to the USB serial port with a timestamp, so the laptop
 * can save it to a file (see README / PuTTY session logging).
 *
 * Wiring (328P test board): ATmega PD1 -> 1.17k -> GPIO16, 2.18k GPIO16 -> GND,
 * GPIO17 -> ATmega PD0, GND <-> GND, 5 V -> 5V pin (or USB).
 */

#define MEGA_BAUD   9600
#define MEGA_RX_PIN 16
#define MEGA_TX_PIN 17

HardwareSerial Mega(1);
String line;
unsigned long pings = 0;

void logLine(const char *what, const char *text) {
  char ts[16];
  unsigned long ms = millis();
  snprintf(ts, sizeof ts, "%6lu.%03lu ", ms / 1000, ms % 1000);
  Serial.print(ts);
  Serial.print(what);
  Serial.print(text);                     // text already ends in '\n'
}

void setup() {
  Serial.begin(115200);
  Mega.begin(MEGA_BAUD, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);
  Serial.println();
  Serial.println("time_s     event");
  Serial.println("ESP32 UART echo test ready (9600 baud on GPIO16/17)");
}

void loop() {
  while (Mega.available()) {
    char c = (char)Mega.read();
    if (c == '\r') continue;
    line += c;
    if (c == '\n') {
      if (line.startsWith("PING")) {
        Mega.print(line);                 // round trip: send it back
        pings++;
        logLine("echoed     ", line.c_str());
      } else {
        logLine("ATmega:    ", line.c_str());     // RESULT lines: print only
      }
      line = "";
    } else if (line.length() > 64) {
      line += "\n";
      logLine("GARBAGE (wrong baud or wiring?): ", line.c_str());
      line = "";
    }
  }
}
