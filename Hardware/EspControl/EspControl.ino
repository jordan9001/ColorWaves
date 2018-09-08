#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "personal_info.h" // contains my ssid, password, servername, etc
#include "GradientAnimator.h"

#define USE_OTHER_LIB

#ifdef USE_OTHER_LIB
#include "WS2812_ESP_RMT.h"
#else
#include <Adafruit_NeoPixel.h>
#endif



#define PIN				22  // data pin
#define NUMPIXELS		150
#define DTIME			10 // ms
#define ERROR_DELAY		3000
#define PING_TIME		6000

#define USE_SERIAL		Serial

#define BUFSZ			0x1000
uint8_t recvbuf[BUFSZ];

#ifdef USE_OTHER_LIB
ESPRMTLED pixels = ESPRMTLED(NUMPIXELS, PIN, RMT_CHANNEL_0);
#else
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#endif

GradientAnimator ga = GradientAnimator();

void setup() {
	USE_SERIAL.begin(115200);

	#ifdef USE_OTHER_LIB
	#else
	pixels.begin();
	#endif
	

	USE_SERIAL.println();
	USE_SERIAL.println();
	USE_SERIAL.println();
	
	WiFi.begin(wfSSID, wfPASS);

	int i=0;
	while (WiFi.status() != WL_CONNECTED) {
		USE_SERIAL.printf("Connecting to Wifi %d...\n", i);
		USE_SERIAL.flush();
		delay(1000);
		i++;
	}

	USE_SERIAL.println("Connected!");
	USE_SERIAL.println(WiFi.localIP());
}

void loop() {
	// For a set of NeoPixels the first NeoPixel is 0, second is 1, all the way up to the count of pixels minus one.
	static uint32_t colors[NUMPIXELS];
	static int32_t t = 0;
	static uint32_t pingtimer = PING_TIME;

	if (pingtimer >= PING_TIME) {
		pingtimer = 0;
		// check for a new pattern
		if (!GetPattern()) {
			USE_SERIAL.println("GetPattern returned an error");
		}
	}
	
	if (!ga.getGradient(colors, NUMPIXELS, t)) {
		// error
		USE_SERIAL.println("getGradient error\n");
		goto exit;
	}

	

#ifdef USE_OTHER_LIB
	for (uint16_t i=0; i<NUMPIXELS; i++) {
		pixels.setPixelColor(i, colors[i]);
	}
	pixels.show();
#else
	for (uint16_t i=0; i<NUMPIXELS; i++) {
		pixels.setPixelColor(i, colors[i]);
	}
	// attempt to get rid of glitches by disabling interrupts
	// this helps, but I should probably implements pixels myself with rmt functionality in the esp32.
	portDISABLE_INTERRUPTS();
	pixels.show();
	portENABLE_INTERRUPTS(); 
#endif


	t = (t + DTIME) % ga.getLooptime();
	exit:
	pingtimer += DTIME;

	delay(DTIME);
}

bool GetPattern() {
	uint16_t bsz;
	int i;
	int amtread;
	WiFiClient client;

	USE_SERIAL.printf("Connecting to HomeBase\n");
	if (!client.connect(HOST, PORT)) {
		USE_SERIAL.printf("Connection failed\n");
		return false;
	}

	// receive size
	USE_SERIAL.printf("Waiting for response");
	while (client.available() < sizeof(bsz)) {
		delay(1);
	}
	USE_SERIAL.println("\nReading Response\n");

	i = client.read((uint8_t*)&bsz, sizeof(bsz));
	if (i != sizeof(bsz)) {
		USE_SERIAL.printf("Didn't read enough to get size! Read %d", i);
		client.flush();
		return false;
	} else if (bsz > BUFSZ) {
		USE_SERIAL.printf("Size of Message (0x%x) bigger than buffer (0x%x)\n", bsz, BUFSZ);
		client.flush();
		return false;
	}

	USE_SERIAL.printf("Got pattern of size 0x%x\n", bsz);

	if (bsz == 0) {
		return true;
	}

	bsz -= sizeof(bsz);
	i = 0;
	while (i < bsz) {
		amtread = client.read(recvbuf+i, bsz-i);
		if (amtread < 0) {
			USE_SERIAL.println("Connection dropped");
			return false;
		}
		i += amtread;
	}

	USE_SERIAL.println("Got update, parsing");

	return ga.parseBuffer(recvbuf, bsz);
}