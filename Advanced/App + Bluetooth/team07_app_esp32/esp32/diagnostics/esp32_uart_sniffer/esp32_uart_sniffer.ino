/*
 * esp32_uart_sniffer.ino  -  diagnostic: prints every byte arriving on GPIO16
 *
 * Every 3 s it switches between 9600 baud (ATmega at 2 MHz, correct) and
 * 76800 baud (ATmega accidentally running at 16 MHz, CKDIV8 fuse off),
 * and prints what it got at each speed in hex and as text.
 * Also reports the idle level of GPIO16 (should be HIGH).
 *
 * Wiring as for the echo test: ATmega PD1 -> 1.17k -> GPIO16, 2.18k GPIO16 -> GND,
 * GND common. Serial Monitor at 115200.
 */

#define RX_PIN 16
#define TX_PIN 17

HardwareSerial Mega(1);
const long bauds[] = { 9600, 76800 };
int which = 0;

void setup()
{
	Serial.begin(115200);
	delay(300);
	Serial.println();
	Serial.println("ESP32 UART sniffer");
}

void loop()
{
	long baud = bauds[which];
	which = (which + 1) % 2;

	/* idle level check with the UART off */
	Mega.end();
	pinMode(RX_PIN, INPUT);
	int highs = 0;
	for (int i = 0; i < 1000; i++) { highs += digitalRead(RX_PIN); delayMicroseconds(100); }

	Mega.begin(baud, SERIAL_8N1, RX_PIN, TX_PIN);
	unsigned long t0 = millis();
	int count = 0;
	String text;
	Serial.printf("\n--- %ld baud | GPIO16 high %d%% of the time ---\n", baud, highs / 10);
	while (millis() - t0 < 3000) {
		while (Mega.available()) {
			int c = Mega.read();
			count++;
			if (count <= 48) Serial.printf("%02X ", c);
			text += (c >= 32 && c < 127) ? (char)c : (c == '\n' ? '|' : '.');
		}
	}
	Serial.printf("\nbytes: %d\n", count);
	if (count) Serial.println("text:  " + text.substring(0, 120));
}
