#include <Math.h>
#include <Wire.h>
#include <Button.h>
#include <Timer.h>
// #include <RunningAverage.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#define BUTTON_PULLUP true
#define BUTTON_INVERT true
#define BUTTON_DEBOUNCE 5
#define CLICKER_PIN 7
#define CLICKS_PER_REVOLUTION 20
#define VELOCITY_SAMPLES 5
#define MAX_TIME_DELTA 1500
#define SPEED_PER_CLICK 0.05
#define DECAY 0.98
#define PI 3.141592653589793
#define TWOPI 6.28318530717959

#define NEOPIXELS_PIN 1
#define NUM_NEOPIXELS 5

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_NEOPIXELS, NEOPIXELS_PIN, NEO_GRB + NEO_KHZ800);

byte tickEvent = 0;
byte displayEvent = 0;
uint16_t clickerSpeed = 0;
uint32_t prevTime = 0;
uint16_t prevVelocity = 0;
uint16_t rainbowIndex = 0;

struct Colors {
  uint32_t white = strip.Color(255, 255, 255);
  uint32_t off = 0;
};

// Wheel states
enum {
  WHEEL_ACCELERATING,
  WHEEL_DECELERATING,
  WHEEL_STOPPED
};

struct WheelData {
  float position = 0; // Radians
  float decay = 0.98;
  float speed = 0; // Radians per second.
  float acceleration = 0; // Radians per second per second
  float distancePerClick = TWOPI / CLICKS_PER_REVOLUTION;
  byte wheelState = WHEEL_STOPPED;
  byte lightIndex = 0;
};

Button clickerBtn = Button(CLICKER_PIN, BUTTON_PULLUP, BUTTON_INVERT, BUTTON_DEBOUNCE);
Timer t;
Adafruit_24bargraph bar = Adafruit_24bargraph();
// RunningAverage timeDeltas(CLICKS_PER_REVOLUTION);
// RunningAverage speedDeltas(VELOCITY_SAMPLES);
WheelData wheelData;
Colors colors;

void setup() {
  Serial.begin(9600);

  // Setup digital pins.
  pinMode(CLICKER_PIN, INPUT_PULLUP);

  // Setup 24-bar LED.
  bar.begin(0x70);
  bar.setBrightness(15);
  for (byte b = 0; b < 24; b++) {
    bar.setBar(b, LED_YELLOW);
  }
  bar.writeDisplay();
  delay(500);

  // Setup timer event intervals.
  // The tick event runs much more frequently, at about 100Hz, for low-latency when reading buttons.
  tickEvent = t.every(20, tick, (void*)0);
  displayEvent = t.every(100, display, (void*)0);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  // Update timer.
  t.update();
}

void tick(void* context) {
  readClicker();
  updateMotion();
  if (wheelData.speed <= 0.01) {
    rainbowCycle();
  }
}

void display(void* context) {
  byte i;

  // float speedDeltasAverage = speedDeltas.getAverage();
  setBarPercentage(100 * wheelData.speed);
  Serial.println(wheelData.speed);
  Serial.println(wheelData.position);
  Serial.println(wheelData.lightIndex);
  Serial.println("---");
  // Light up the current light
  for(i = 0; i < strip.numPixels(); i++) {
    if (i == wheelData.lightIndex) {
      strip.setPixelColor(i, colors.white);
    }
    else {
      strip.setPixelColor(i, colors.off); 
    }
  }
  strip.show();

  // float acceleration = accelerationAverage.getAverage();
  // Serial.print("acceleration: ");
  // Serial.println(acceleration);
}

void click() {
  // increase speed by some amount
  wheelData.speed += SPEED_PER_CLICK;

  // if (wheelState == WHEEL_STOPPED) {
  //   Serial.println("accelerating");
  //   wheelState = WHEEL_ACCELERATING;
  //   // Seed the time deltas with the first time delta value.
  //   timeDeltas.fillValue(timeDelta, CLICKS_PER_REVOLUTION);
  // }
  // else {
  //   timeDeltas.addValue(timeDelta);
  // }
  // // Stopped?
  // if (timeDelta > MAX_TIME_DELTA) {
  //   Serial.println("stopping");
  //   Serial.println(timeDelta);
  //   wheelState = WHEEL_STOPPED;
  //   prevTime = 0;
  //   timeDeltas.fillValue(0, CLICKS_PER_REVOLUTION);
  //   speedDeltas.fillValue(0, VELOCITY_SAMPLES);
  //   return;
  // }
}

void updateMotion() {
  byte lightIndex;

  //apply 'friction' to speed
  wheelData.speed *= wheelData.decay;

  //increment position by speed
  wheelData.position += wheelData.speed;

  // Reset position after exceeding 2PI.
  if (wheelData.position >= TWOPI) {
    wheelData.position -= TWOPI;
  }

  //convert position to light index
  wheelData.lightIndex = floor(wheelData.position * NUM_NEOPIXELS / TWOPI);
}

void readClicker() {
  clickerBtn.read();

  if (clickerBtn.wasPressed()) {
    click();
  }
}

void setBarPercentage(byte value) {
  value = constrain(value, 0, 100);
  value = map(value, 0, 100, 0, 24);
  for (byte b = 0; b < 24; b++) {
    bar.setBar(b, b < value ? LED_GREEN : LED_OFF);
  }
  bar.writeDisplay();
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle() {
  uint16_t i;

  rainbowIndex++;
  if (rainbowIndex > 256 * 5) {
    rainbowIndex = 0;
  }

  // 5 cycles of all colors on wheel
  for(i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + rainbowIndex) & 255));
  }
  strip.show();
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}
