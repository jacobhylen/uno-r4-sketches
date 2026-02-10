/*

           /^\/^\
         _|__|  O|
\/     /~     \_/ \
 \____|__________/  \
        \_______      \
                `\     \                 \
                  |     |                  \
                 /      /                    \
                /     /                       \\
              /      /                         \ \
             /     /                            \  \
           /     /             _----_            \   \
          /     /           _-~      ~-_         |   |
         (      (        _-~    _--_    ~-_     _/   |
          \      ~-____-~    _-~    ~-_    ~-_-~    /
            ~-_           _-~          ~-_       _-~
               ~--______-~                ~-___-~

Play snake on the Arduino UNO R4 WiFi's built in LED Matrix!

Sketch written by Jacob Hylén & Ubi de Feo.

How to play:
- connect pushbuttons with 10kOhm pull-down resistors to D2, D3, D4, & D5.
- Upload the sketch.
- Enjoy!

Your score will be printed to the Serial Monitor on death.
*/

#include "Arduino_LED_Matrix.h"
#include "math.h"

ArduinoLEDMatrix matrix;

uint8_t frame[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

unsigned long lastTickTime, lastGameTickTime, lastSnakeMove;
#define UPDATE_INTERVAL 100
#define GAME_UPDATE_INTERVAL 66

#define ROWS 8
#define COLUMNS 12

#define down 5
#define up 4
#define left 2
#define right 3
#define BUZZER 11

uint8_t direction = 2;
int speed = 400;

uint8_t pointX = 0, pointY = 0;
uint8_t charX = 2, charY = 2;
uint8_t oldcharX = 4, oldcharY = 2;

//tail stuff
int snaketailX[96]{

};
int snaketailY[96]{

};

int tailLength = 2;

uint8_t foodX = 5, foodY = 6;

uint8_t score = 0;

// Track previous button states for edge detection
bool lastUp = false, lastDown = false, lastLeft = false, lastRight = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1500);
  matrix.begin();
  lastGameTickTime = lastTickTime = millis();

  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(4, INPUT);
  pinMode(5, INPUT);
  pinMode(BUZZER, OUTPUT);

  gameStartJingle();
}

void loop() {
  //delay(1000);
  unsigned long msNow = millis();
  if (msNow - lastGameTickTime > GAME_UPDATE_INTERVAL) {
    bool currUp = digitalRead(up) == HIGH;
    bool currDown = digitalRead(down) == HIGH;
    bool currLeft = digitalRead(left) == HIGH;
    bool currRight = digitalRead(right) == HIGH;

    if (currUp && !lastUp) {
      if (direction != 2) {
        direction = 3;
        tone(BUZZER, 800, 30);
      }
    }
    if (currDown && !lastDown) {
      if (direction != 3) {
        direction = 2;
        tone(BUZZER, 800, 30);
      }
    }
    if (currLeft && !lastLeft) {
      if (direction != 0) {
        direction = 1;
        tone(BUZZER, 800, 30);
      }
    }
    if (currRight && !lastRight) {
      if (direction != 1) {
        direction = 0;
        tone(BUZZER, 800, 30);
      }
    }

    lastUp = currUp;
    lastDown = currDown;
    lastLeft = currLeft;
    lastRight = currRight;

    oldcharX = charX;
    oldcharY = charY;

    lastGameTickTime = msNow;
  }
  if (msNow - lastTickTime > UPDATE_INTERVAL) {
    matrix.renderBitmap(frame, 8, 12);
    lastTickTime = msNow;
  }

  // set the direction for the snake
  if (msNow - lastSnakeMove > speed) {
    if (direction == 0) {
      charY--;
    }
    if (direction == 1) {
      charY++;
    }
    if (direction == 2) {
      charX++;
    }
    if (direction == 3) {
      charX--;
    }

    if (charY == 255 || charY == 8 || charX == 255 || charX == 12) {
      gameOver();
    }

    if (foodX == charX && foodY == charY) {
      score++;
      tailLength = score + 2;


      generateFood();
    }

    for (int i = tailLength; i > 0; i--) {
      snaketailX[i] = snaketailX[i - 1];
      snaketailY[i] = snaketailY[i - 1];
    }
    snaketailX[0] = charX;
    snaketailY[0] = charY;

    frame[snaketailY[tailLength - 1]][snaketailX[tailLength - 1]] = 0;

    if (frame[charY][charX] == 1) {
      gameOver();
    }
    frame[charY][charX] = 1;
    frame[foodY][foodX] = 1;

    speed = 66 + (400 - 66) * exp(-score * 3 / 100.0);
    lastSnakeMove = msNow;
  }
}

void generateFood() {
  frame[foodY][foodX] = 0;
  foodX = random(12);
  foodY = random(8);
  if (frame[foodY][foodX] == 1){
    generateFood();
  }
}

// 3x5 digit font (0-9)
const uint8_t digits[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void displayScore(uint8_t s) {
  // Clear frame
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 12; x++) {
      frame[y][x] = 0;
    }
  }

  int tens = s / 10;
  int ones = s % 10;

  // Draw tens digit at column 1, ones digit at column 6
  // Vertically centered (start at row 1)
  for (int row = 0; row < 5; row++) {
    if (tens > 0) {
      for (int col = 0; col < 3; col++) {
        if (digits[tens][row] & (0b100 >> col)) {
          frame[row + 1][col + 1] = 1;
        }
      }
    }
    for (int col = 0; col < 3; col++) {
      if (digits[ones][row] & (0b100 >> col)) {
        frame[row + 1][col + (tens > 0 ? 6 : 4)] = 1;
      }
    }
  }

  matrix.renderBitmap(frame, 8, 12);
}

void rippleAnimation() {
  // Center of the matrix
  float centerX = 5.5;
  float centerY = 3.5;
  
  // Calculate max distance from center to corner
  float maxDist = sqrt(centerX * centerX + centerY * centerY);

  // Pre-calculate the score frame
  uint8_t scoreFrame[8][12] = {0};
  int tens = score / 10;
  int ones = score % 10;
  for (int row = 0; row < 5; row++) {
    if (tens > 0) {
      for (int col = 0; col < 3; col++) {
        if (digits[tens][row] & (0b100 >> col)) {
          scoreFrame[row + 1][col + 1] = 1;
        }
      }
    }
    for (int col = 0; col < 3; col++) {
      if (digits[ones][row] & (0b100 >> col)) {
        scoreFrame[row + 1][col + (tens > 0 ? 6 : 4)] = 1;
      }
    }
  }
  
  // Fill outward from center
  for (float radius = 0; radius <= maxDist + 1; radius += 0.8) {
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 12; x++) {
        float dist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
        if (dist <= radius) {
          frame[y][x] = 1;
        }
      }
    }
    matrix.renderBitmap(frame, 8, 12);
    delay(40);
  }
  
  delay(200);
  
  // Clear outward from center, revealing score underneath
  for (float radius = 0; radius <= maxDist + 1; radius += 0.8) {
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 12; x++) {
        float dist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
        if (dist <= radius) {
          frame[y][x] = scoreFrame[y][x];
        }
      }
    }
    matrix.renderBitmap(frame, 8, 12);
    delay(40);
  }
}

void gameStartJingle() {
  tone(BUZZER, 262, 150);  // C4
  delay(160);
  tone(BUZZER, 330, 150);  // E4
  delay(160);
  tone(BUZZER, 392, 150);  // G4
  delay(160);
  tone(BUZZER, 523, 400);  // C5
  delay(450);
}

void gameOverJingle() {
  tone(BUZZER, 392, 150);  // G4
  delay(160);
  tone(BUZZER, 349, 150);  // F4
  delay(160);
  tone(BUZZER, 330, 150);  // E4
  delay(160);
  tone(BUZZER, 262, 400);  // C4
  delay(450);
}

void gameOver() {
  Serial.println("GAME OVER");
  Serial.print("Your score was: ");
  Serial.println(score);

  gameOverJingle();
  rippleAnimation();
  displayScore(score);

  while (1) {}
}
