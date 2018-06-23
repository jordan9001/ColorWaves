#include <Adafruit_NeoPixel.h>
#include "GradientAnimator.cpp"

#define PIN            6  // data pin
#define NUMPIXELS      150
#define DTIME          100 // ms
#define ERROR_DELAY    3000

struct serialPoint {
  uint32_t color;
  int32_t msoff;
  uint16_t index;
};

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
GradientAnimator ga = GradientAnimator();

void setup() {
  Serial.begin(115200);

  pixels.begin();
}

void loop() {
  // For a set of NeoPixels the first NeoPixel is 0, second is 1, all the way up to the count of pixels minus one.
  static uint32_t colors[NUMPIXELS];
  static int32_t t = 0;
  if (!ga.getGradient(colors, NUMPIXELS, t)) {
    // error
    delay(ERROR_DELAY);
    return;
  }

  for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, colors[i]);
  }
  pixels.show();

  t = (t + DTIME) % ga.getLooptime();

  delay(DTIME);
}

// Serial callback
//
// uin16_t bytes_in_message
// int32_t looptime
//  (for each node)
//    uint16_t number of points
//      (for each point)
//        uint32_t color
//        int32_t msoff
//        uint16_t index
//TODO move this to the lib

void serialEvent() {
  static uint16_t bytes_to_read = 0;
  static uint16_t points_to_read = 0;
  static uint8_t node = 0;
  static char buf[0x400];

  struct serialPoint pt;
  int32_t loopms = 0;

  if (bytes_to_read == 0) {
    // at the start of a message
    if (Serial.available() < sizeof(bytes_to_read) + sizeof(loopms)) {
      // wait for more
      return;
    }
    Serial.readBytes(&bytes_to_read, sizeof(bytes_to_read));
    // clear old pattern
    ga.clear();
    node = (uint8_t)-1;
    points_to_read = 0;
    // get the time
    Serial.readBytes(&loopms, sizeof(loopms));
    // fall through
  }
  while (bytes_to_read > 0) {
    if (points_to_read == 0) {
      // at the start of a node
      if (Serial.available() < sizeof(points_to_read)) {
        return;
      }
      bytes_to_read -= Serial.readBytes(&points_to_read, sizeof(points_to_read));
      node += 1;
    }
    // middle of a node
    if (Serial.available() < sizeof(pt)) {
      return;
    }

    bytes_to_read -= Serial.readBytes(&pt, sizeof(pt));
    points_to_read -= 1;

    // add the point!
    ga.addPoint(node, pt.color, pt.index, pt.msoff);
  }
}
