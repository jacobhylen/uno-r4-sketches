
#include "Arduino_LED_Matrix.h"

int UPDATE_INTERVAL = 100;

unsigned long lastTickTime;
unsigned long msNow;

ArduinoLEDMatrix matrix;

//Sine wave
uint8_t sine[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0 },
  { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },
  { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },
  { 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

//Triangle wave
uint8_t triangle[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0 },
  { 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0 },
  { 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1 },
  { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

//Sawtooth wave
uint8_t sawtooth[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 },
  { 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 },
  { 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0 },
  { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

//Square wave
uint8_t square[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0 },
  { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },
  { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },
  { 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};


// Set the DAC output pin to A0
const int dac_pin = A0;

// Define constants for the wave generator
float frequency = 1000;          // Hz
const float amplitude = 4095;    // 12-bit DAC resolution
const float offset = 2047;       // 12-bit DAC midpoint
const float pi = 3.14159265359;  // Pi

int switch_val = 0;

// Define variables for the wave generator
float phase = 0;
float increment = (2 * pi * frequency) / 100000;  // Sampling rate of 100 kHz


void setup() {
  Serial.begin(9600);
  // Initialize the DAC output pin
  analogWriteResolution(12);
  matrix.begin();
}

void loop() {
  // Read the switch input
  UPDATE_INTERVAL = 200 - frequency / 12;

  if (digitalRead(2) == HIGH) {
    switch_val++;
    if (switch_val > 3) {
      switch_val = 0;
    }
    Serial.print(switch_val);
    delay(200);
  }
  frequency = map(analogRead(A5), 0, 1024, 500, 2000);
  increment = (2 * pi * frequency) / 100000;

  float output;
  // Generate the appropriate waveform based on the switch value
  switch (switch_val) {

    case 0:  // Sine wave
      msNow = millis();
      if (msNow - lastTickTime > UPDATE_INTERVAL) {
        scrollFrame(sine);
        lastTickTime = msNow;
      }
      //for (int i = 0; i < 100000; i++) {
      output = amplitude * sin(phase) + offset;
      analogWrite(dac_pin, output);  // Scale output to 8-bit resolution
      phase += increment;
      if (phase >= 2 * pi) {
        phase -= 2 * pi;
      }
      // }
      break;
    case 1:  // Triangle wave
      msNow = millis();
      if (msNow - lastTickTime > UPDATE_INTERVAL) {
        scrollFrame(triangle);
        lastTickTime = msNow;
      }
      //for (int i = 0; i < 100000; i++) {
      output = (2 * amplitude / pi) * asin(sin(phase)) + offset;
      analogWrite(dac_pin, output);  // Scale output to 8-bit resolution
      phase += increment;
      if (phase >= 2 * pi) {
        phase -= 2 * pi;
      }
      // }
      break;
    case 2:  // Sawtooth wave
      msNow = millis();
      if (msNow - lastTickTime > UPDATE_INTERVAL) {
        scrollFrame(sawtooth);
        lastTickTime = msNow;
      }
      //for (int i = 0; i < 100000; i++) {
      output = (amplitude / pi) * phase + offset;
      analogWrite(dac_pin, output);  // Scale output to 8-bit resolution
      phase += increment;
      if (phase >= 2 * pi) {
        phase -= 2 * pi;
      }
      //}
      break;
    case 3:  // Square wave
      msNow = millis();
      if (msNow - lastTickTime > UPDATE_INTERVAL) {
        scrollFrame(square);
        lastTickTime = msNow;
      }
      //for (int i = 0; i < 100000; i++) {
      output = amplitude * sign(sin(phase)) + offset;
      analogWrite(dac_pin, output);  // Scale output to 8-bit resolution
      phase += increment;
      if (phase >= 2 * pi) {
        phase -= 2 * pi;
      }
      //}
      break;
    default:
      break;
  }
}

// Helper function to return the sign of a value
int sign(float x) {
  if (x > 0) {
    return 1;
  } else if (x < 0) {
    return -1;
  } else {
    return 0;
  }
}

void scrollFrame(uint8_t frame[8][12]) {
  for (int x = 0; x < 8; x++) {
    int temp = frame[x][0];  // Store the first element in a temporary variable

    for (int i = 0; i < 12 - 1; i++) {
      frame[x][i] = frame[x][i + 1];  // Shift each element one position to the left
    }

    frame[x][11] = temp;  // Set the last element to the temporary value
  }
  matrix.renderBitmap(frame, 8, 12);
}
