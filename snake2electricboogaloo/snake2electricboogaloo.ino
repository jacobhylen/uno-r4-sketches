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

#define down 2
#define up 3
#define left 4
#define right 5

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
}

void loop() {
  //delay(1000);
  unsigned long msNow = millis();
  if (msNow - lastGameTickTime > GAME_UPDATE_INTERVAL) {
    if (digitalRead(up) == HIGH) {
      // Serial.println("up");
      if (direction == 2) {
        return;
      }
      direction = 3;
      // charX--;
    }
    if (digitalRead(down) == HIGH) {
      // Serial.println("down");
      if (direction == 3) {
        return;
      }
      direction = 2;
      // charX++;
    }
    if (digitalRead(left) == HIGH) {
      // Serial.println("left");
      if (direction == 0) {
        return;
      }
      direction = 1;
    }
    if (digitalRead(right) == HIGH) {
      // Serial.println("right");
      if (direction == 1) {
        return;
      }
      direction = 0;
    }

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

    if (charY == 255) {
      Serial.println("GAME OVER");
      Serial.print("Your score was: ");
      Serial.print(score);
      while (1) {}
    }
    if (charY == 8) {
      Serial.println("GAME OVER");
      Serial.print("Your score was: ");
      Serial.print(score);
      while (1) {}
    }
    if (charX == 255) {
      Serial.println("GAME OVER");
      Serial.print("Your score was: ");
      Serial.print(score);
      while (1) {}
    }
    if (charX == 12) {
      Serial.println("GAME OVER");
      Serial.print("Your score was: ");
      Serial.print(score);
      while (1) {}
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
      Serial.println("");
      Serial.println("GAME OVER");
      Serial.print("Your score was: ");
      Serial.print(score);
      while (1) {}
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