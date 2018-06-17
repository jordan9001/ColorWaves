#include <Adafruit_NeoPixel.h>
#include "GradientAnimator.cpp"

#define PIN            6  // data pin
#define NUMPIXELS      150

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
  uint32_t c = pixels.Color(0,150,0);
  Serial.print("Setting for ");
  Serial.print(NUMPIXELS);
  Serial.print(" to color ");
  Serial.println(c, HEX);
  for(int i=0;i<NUMPIXELS;i++){
    Serial.println(i);
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(0,150,0)); // Moderately bright green color.

    pixels.show(); // This sends the updated pixel color to the hardware.

    delay(100); // Delay for a period of time (in milliseconds).

  }
  delay(3000);
}

// Serial callback
//
// uin16_t bytes_in_message
//  (for each node)
//    uint16_t number of points
//      (for each point)
//        uint32_t color
//        int32_t msoff
//        uint16_t index

void serialEvent() {
  static uint16_t bytes_to_read = 0;
  static uint16_t points_to_read = 0;
  static uint8_t node = 0;
  static char buf[0x400];

  struct serialPoint pt;

  if (bytes_to_read == 0) {
    // at the start of a message
    if (Serial.available() < sizeof(bytes_to_read)) {
      // wait for more
      return;
    }
    Serial.readBytes(&bytes_to_read, sizeof(bytes_to_read));
    // clear old pattern
    ga.clear();
    node = (uint8_t)-1;
    points_to_read = 0;
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
