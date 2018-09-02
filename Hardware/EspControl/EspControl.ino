#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_NeoPixel.h>
#include "personal_info.h" // contains my ssid, password, servername, etc
#include "GradientAnimator.h"

#define LOOPTIME_T		int32_t
#define POINTCOUNT_T	uint16_t
#define PT_COLOR_T		uint32_t
#define PT_MSOFF_T		int32_t
#define PT_INDEX_T		uint16_t

#define POINT_LEN       (sizeof(PT_COLOR_T) + sizeof(PT_MSOFF_T) + sizeof(PT_INDEX_T))
#define MIN_BUFFER_LEN  (sizeof(LOOPTIME_T) + sizeof(POINTCOUNT_T) + POINT_LEN)

#define USE_SERIAL     Serial
#define PIN            22  // data pin
#define NUMPIXELS      150
#define DTIME          1000 // ms
#define ERROR_DELAY    3000
#define PING_TIME      6000

#define BUFSZ       0x1000
uint8_t recvbuf[BUFSZ];

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
GradientAnimator ga = GradientAnimator();

void setup() {
	USE_SERIAL.begin(115200);

	pixels.begin();

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

	for(int i=0;i<NUMPIXELS;i++){
		USE_SERIAL.printf("%d: #%x\n", i, colors[i]); 
		//pixels.setPixelColor(i, colors[i]);
	}

	// attempt to get rid of glitches by disabling interrupts
	// this helps, but I should probably implements pixels myself with rmt functionality in the esp32.
	portDISABLE_INTERRUPTS();
	delay(1);
	pixels.show();
	portENABLE_INTERRUPTS(); 

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
		USE_SERIAL.printf("0x%x / 0x%x\n", i, bsz);
	}

	USE_SERIAL.println("Got update, parsing");

	return parseBuffer(recvbuf, bsz);
}

bool parseBuffer(uint8_t* buf, uint16_t sz) {
	if (sz < MIN_BUFFER_LEN) {
		USE_SERIAL.printf("Bad buffer size passed in, only 0x%x bytes\n", sz);
		return false;
	}
	ga.clear();

	uint8_t* cur;
	LOOPTIME_T lpt;
	POINTCOUNT_T pointcount;
	POINTCOUNT_T i;
	PT_COLOR_T p_c;
	PT_MSOFF_T p_m;
	PT_INDEX_T p_i;
	uint8_t node_i = 0;

	cur = buf;
	lpt = *((LOOPTIME_T*)cur);
	USE_SERIAL.printf("Got looptime as %d\n", lpt);
	ga.setLooptime(lpt);
	cur += sizeof(LOOPTIME_T);
	while (cur < (buf + sz)) {
		pointcount = *((POINTCOUNT_T*)cur);
		cur += sizeof(POINTCOUNT_T);

		if ((cur + (pointcount * POINT_LEN)) > (buf + sz)) {
			USE_SERIAL.printf("Pointcount would send up past end of buffer. 0x%x\n", pointcount);
			return false;
		}

		USE_SERIAL.printf("Node %d has %d points\n", node_i, pointcount);

		for (i=0; i<pointcount; i++) {
			p_c = *((PT_COLOR_T*)cur);
			cur += sizeof(PT_COLOR_T);
			p_m = *((PT_MSOFF_T*)cur);
			cur += sizeof(PT_MSOFF_T);
			p_i = *((PT_INDEX_T*)cur);
			cur += sizeof(PT_INDEX_T);

			if (!ga.addPoint(node_i, p_c, p_i, p_m)) {
				USE_SERIAL.println("Used up too many points!");
				return false;
			}
		}
		node_i++;
	}

	return true;
}