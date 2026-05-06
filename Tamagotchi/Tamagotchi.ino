/*
 * Learning Tamagotchi for Arduino UNO R4 WiFi
 * ------------------------------------------------
 * - Cycles between HAPPY and NEEDY states.
 * - When NEEDY, a random electronics "need" is generated
 *   (button press, potentiometer sweep, LED loopback, etc.)
 *   targeting a random pin from a safe whitelist.
 * - A tiny WiFi web page shows the current instructions,
 *   the target pin, and a countdown. The LED matrix only
 *   carries emotion: happy face, sad face, dead skull,
 *   and a heart/sparkle when a need is satisfied.
 * - If the deadline passes, the pet dies (permadeath until reset).
 *
 * Wire up whatever the device asks for. Safe pins only; the
 * sketch reconfigures pinMode() at runtime.
 */

#include "Arduino_LED_Matrix.h"
#include <WiFiS3.h>
#include "faces.h"
#include "secrets.h"   // defines WIFI_SSID and WIFI_PASS

// ---------------- Config ---------------------------------------------------

// Pins the game is allowed to commandeer. Avoid matrix/I2C/serial pins.
static const uint8_t SAFE_PINS[] = {2, 3, 4, 5, 6, 7, A0, A1, A2, A3};
static const uint8_t SAFE_PIN_COUNT = sizeof(SAFE_PINS) / sizeof(SAFE_PINS[0]);

static const uint32_t NEED_DEADLINE_MS    = 60UL * 60UL * 1000UL;  // 1 hour
static const uint32_t HAPPY_INTERVAL_MIN  = 2UL  * 60UL * 1000UL;  // 2 min
static const uint32_t HAPPY_INTERVAL_MAX  = 10UL * 60UL * 1000UL;  // 10 min
static const uint32_t DEBOUNCE_MS         = 20;

// Analog stability detection. A floating pin wanders; a real resistor
// network (pot, photoresistor, etc.) is steady. We require STABILITY_WINDOW
// consecutive samples, taken ~SAMPLE_INTERVAL_MS apart, with max-min
// spread at or below STABILITY_MAX_SPREAD ADC counts (1023 full-scale).
static const uint8_t  STABILITY_WINDOW     = 10;
static const int16_t  STABILITY_MAX_SPREAD = 30;
static const uint32_t SAMPLE_INTERVAL_MS   = 20;

// ---------------- Types ----------------------------------------------------

enum PetState : uint8_t { STATE_HAPPY, STATE_NEEDY, STATE_CELEBRATING, STATE_DEAD };

enum NeedType : uint8_t {
  NEED_BUTTON_COUNT,   // press a button N times
  NEED_ANALOG_RANGE,   // drive an analog pin into [lo, hi]
  NEED_ANALOG_CHANGE,  // cause a large change on an analog pin
  NEED_LED_LOOPBACK,   // light an LED whose other leg feeds an input pin
  NEED_TOGGLE          // toggle a digital input HIGH/LOW/HIGH/LOW...
};

struct Need {
  NeedType type;
  uint8_t  primaryPin;
  uint8_t  secondaryPin;  // used by loopback
  int32_t  target;        // press count, toggle count, etc.
  int32_t  progress;
  int32_t  param1;        // range lo, baseline, etc.
  int32_t  param2;        // range hi, etc.
  uint32_t startedAt;
  const char* title;
  const char* instructions;
};

// ---------------- Globals --------------------------------------------------

ArduinoLEDMatrix matrix;
WiFiServer       server(80);

PetState state = STATE_HAPPY;
Need     currentNeed;
uint32_t nextNeedAt    = 0;
uint32_t lastFaceFrame = 0;
uint8_t  faceFrame     = 0;
uint32_t celebrateUntil = 0;

// debounce / edge tracking
uint8_t  lastRead  = HIGH;
uint32_t lastEdge  = 0;

// Analog stability window.
int16_t  sampleBuf[STABILITY_WINDOW];
uint8_t  sampleCount   = 0;
uint32_t lastSampleMs  = 0;

// ---------------- Helpers --------------------------------------------------

static uint8_t pickSafePin(uint8_t exclude = 255) {
  uint8_t p;
  do { p = SAFE_PINS[random(SAFE_PIN_COUNT)]; } while (p == exclude);
  return p;
}

static bool pinIsAnalog(uint8_t p) { return p >= A0 && p <= A5; }

static const char* pinLabel(uint8_t p) {
  switch (p) {
    case A0: return "A0"; case A1: return "A1"; case A2: return "A2";
    case A3: return "A3"; case A4: return "A4"; case A5: return "A5";
    default:
      static char buf[4];
      snprintf(buf, sizeof(buf), "D%u", p);
      return buf;
  }
}

static void showFrame(const uint8_t frame[8][12]) {
  matrix.renderBitmap(frame, 8, 12);
}

// ---- Analog stability helpers -------------------------------------------

static void resetStability() {
  sampleCount  = 0;
  lastSampleMs = 0;
}

// Adds a throttled sample to the ring. Returns true once the window is full.
static bool addSample(int16_t v) {
  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return false;
  lastSampleMs = now;

  if (sampleCount < STABILITY_WINDOW) {
    sampleBuf[sampleCount++] = v;
  } else {
    for (uint8_t i = 0; i < STABILITY_WINDOW - 1; i++)
      sampleBuf[i] = sampleBuf[i + 1];
    sampleBuf[STABILITY_WINDOW - 1] = v;
  }
  return sampleCount == STABILITY_WINDOW;
}

static int16_t sampleSpread() {
  int16_t mn = sampleBuf[0], mx = sampleBuf[0];
  for (uint8_t i = 1; i < sampleCount; i++) {
    if (sampleBuf[i] < mn) mn = sampleBuf[i];
    if (sampleBuf[i] > mx) mx = sampleBuf[i];
  }
  return mx - mn;
}

static bool allInRange(int16_t lo, int16_t hi) {
  for (uint8_t i = 0; i < sampleCount; i++) {
    if (sampleBuf[i] < lo || sampleBuf[i] > hi) return false;
  }
  return true;
}

// ---------------- Need generation -----------------------------------------

static void generateNeed() {
  currentNeed = Need{};
  currentNeed.startedAt = millis();
  currentNeed.progress  = 0;
  resetStability();

  uint8_t roll = random(5);
  switch (roll) {
    case 0: {
      currentNeed.type = NEED_BUTTON_COUNT;
      currentNeed.primaryPin = pickSafePin();
      currentNeed.target = 3 + random(6); // 3..8
      currentNeed.title = "Feed me clicks!";
      currentNeed.instructions =
        "Wire a momentary pushbutton between the target pin and GND. "
        "Press it the required number of times.";
      pinMode(currentNeed.primaryPin, INPUT_PULLUP);
      lastRead = digitalRead(currentNeed.primaryPin);
      break;
    }
    case 1: {
      // force analog
      uint8_t p;
      do { p = pickSafePin(); } while (!pinIsAnalog(p));
      currentNeed.type = NEED_ANALOG_RANGE;
      currentNeed.primaryPin = p;
      currentNeed.param1 = 400; // ~33%
      currentNeed.param2 = 700; // ~57%
      currentNeed.target = 1;
      currentNeed.title = "Tune me in!";
      currentNeed.instructions =
        "Wire a 10k potentiometer: outer legs to 5V and GND, wiper to the target pin. "
        "Turn the wiper until the reading lands in the target window.";
      break;
    }
    case 2: {
      uint8_t p;
      do { p = pickSafePin(); } while (!pinIsAnalog(p));
      currentNeed.type = NEED_ANALOG_CHANGE;
      currentNeed.primaryPin = p;
      // Baseline captured later, once a stable reading proves the user has
      // actually wired something. progress==0 means baseline not yet taken.
      currentNeed.param1 = 0;
      currentNeed.param2 = 200; // delta threshold
      currentNeed.target = 1;
      currentNeed.title = "Wake my senses!";
      currentNeed.instructions =
        "Wire a photoresistor (or any analog sensor) to the target pin with a 10k pull-down. "
        "Cover it, shine a light on it, or otherwise change its reading noticeably.";
      break;
    }
    case 3: {
      currentNeed.type = NEED_LED_LOOPBACK;
      currentNeed.primaryPin   = pickSafePin();                        // LED driver (OUTPUT... from user side)
      currentNeed.secondaryPin = pickSafePin(currentNeed.primaryPin);  // sense line
      currentNeed.target = 1;
      currentNeed.title = "Light me up!";
      currentNeed.instructions =
        "Wire an LED: anode (long leg) to 5V through a 220 ohm resistor, cathode (short leg) "
        "to the sense pin. Also wire the sense pin to where current can flow. "
        "Simpler: just bridge the two target pins with a wire so the sense pin reads LOW.";
      pinMode(currentNeed.primaryPin,   OUTPUT);
      digitalWrite(currentNeed.primaryPin, LOW);
      pinMode(currentNeed.secondaryPin, INPUT_PULLUP);
      break;
    }
    default: {
      currentNeed.type = NEED_TOGGLE;
      currentNeed.primaryPin = pickSafePin();
      currentNeed.target = 4 + random(5); // 4..8 edges
      currentNeed.title = "Tickle me!";
      currentNeed.instructions =
        "Repeatedly connect and disconnect the target pin from GND. "
        "A wire you touch to ground works; a switch works too.";
      pinMode(currentNeed.primaryPin, INPUT_PULLUP);
      lastRead = digitalRead(currentNeed.primaryPin);
      break;
    }
  }

  state = STATE_NEEDY;
  Serial.print("NEW NEED: "); Serial.println(currentNeed.title);
  Serial.print("  pin: "); Serial.println(pinLabel(currentNeed.primaryPin));
}

// ---------------- Need evaluation -----------------------------------------

static bool evaluateNeed() {
  uint32_t now = millis();
  switch (currentNeed.type) {

    case NEED_BUTTON_COUNT:
    case NEED_TOGGLE: {
      uint8_t r = digitalRead(currentNeed.primaryPin);
      if (r != lastRead && (now - lastEdge) > DEBOUNCE_MS) {
        lastEdge = now;
        // count falling edge for button, both edges for toggle
        if (currentNeed.type == NEED_BUTTON_COUNT) {
          if (r == LOW) currentNeed.progress++;
        } else {
          currentNeed.progress++;
        }
        lastRead = r;
      }
      return currentNeed.progress >= currentNeed.target;
    }

    case NEED_ANALOG_RANGE: {
      int v = analogRead(currentNeed.primaryPin);
      if (!addSample(v)) return false;
      if (sampleSpread() > STABILITY_MAX_SPREAD) return false;
      return allInRange(currentNeed.param1, currentNeed.param2);
    }

    case NEED_ANALOG_CHANGE: {
      int v = analogRead(currentNeed.primaryPin);
      if (!addSample(v)) return false;
      if (sampleSpread() > STABILITY_MAX_SPREAD) return false;

      if (currentNeed.progress == 0) {
        // First stable reading after wiring -> baseline.
        currentNeed.param1   = v;
        currentNeed.progress = 1;
        resetStability();
        Serial.print("Baseline established: "); Serial.println(v);
        return false;
      }
      // Require a stable deflection from baseline.
      return abs(v - currentNeed.param1) >= currentNeed.param2;
    }

    case NEED_LED_LOOPBACK: {
      // Drive HIGH, expect sense to rise; drive LOW, expect sense to fall.
      digitalWrite(currentNeed.primaryPin, HIGH);
      delayMicroseconds(200);
      uint8_t hi = digitalRead(currentNeed.secondaryPin);
      digitalWrite(currentNeed.primaryPin, LOW);
      delayMicroseconds(200);
      uint8_t lo = digitalRead(currentNeed.secondaryPin);
      return (hi == HIGH && lo == LOW);
    }
  }
  return false;
}

// ---------------- State transitions ---------------------------------------

static void scheduleNextNeed() {
  uint32_t span = HAPPY_INTERVAL_MAX - HAPPY_INTERVAL_MIN;
  nextNeedAt = millis() + HAPPY_INTERVAL_MIN + random(span);
}

static void releaseNeedPins() {
  // Return pins to a safe input state after a need ends.
  pinMode(currentNeed.primaryPin, INPUT);
  if (currentNeed.type == NEED_LED_LOOPBACK) {
    pinMode(currentNeed.secondaryPin, INPUT);
  }
}

static void satisfyNeed() {
  releaseNeedPins();
  state = STATE_CELEBRATING;
  celebrateUntil = millis() + 4000;
  showFrame(FACE_HEART);
  Serial.println("Need satisfied! <3");
}

static void killPet() {
  releaseNeedPins();
  state = STATE_DEAD;
  showFrame(FACE_DEAD);
  Serial.println("Your pet has died. Reset to try again.");
}

// ---------------- Faces (animation) ---------------------------------------

static void animateFace() {
  uint32_t now = millis();
  if (now - lastFaceFrame < 600) return;
  lastFaceFrame = now;

  switch (state) {
    case STATE_HAPPY:
      showFrame(faceFrame++ & 1 ? FACE_HAPPY : FACE_HAPPY_BLINK);
      break;
    case STATE_NEEDY:
      showFrame(faceFrame++ & 1 ? FACE_SAD : FACE_SAD_CRY);
      break;
    case STATE_CELEBRATING:
      showFrame(faceFrame++ & 1 ? FACE_HEART : FACE_HAPPY);
      if (now >= celebrateUntil) {
        state = STATE_HAPPY;
        scheduleNextNeed();
      }
      break;
    case STATE_DEAD:
      showFrame(FACE_DEAD);
      break;
  }
}

// ---------------- WiFi / Web UI -------------------------------------------

static void startWiFi() {
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait for association.
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi failed (status="); Serial.print(WiFi.status());
    Serial.println("); continuing offline.");
    return;
  }

  // Wait for DHCP to actually hand us an IP.
  uint32_t ipStart = millis();
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - ipStart < 10000) {
    delay(200); Serial.print("+");
  }
  Serial.println();

  IPAddress ip = WiFi.localIP();
  if (ip == IPAddress(0, 0, 0, 0)) {
    Serial.println("DHCP did not assign an IP; continuing offline.");
    return;
  }

  Serial.print("IP: ");        Serial.println(ip);
  Serial.print("RSSI: ");      Serial.println(WiFi.RSSI());
  Serial.print("Firmware: ");  Serial.println(WiFi.firmwareVersion());
  server.begin();
}

static const char* stateName() {
  switch (state) {
    case STATE_HAPPY:       return "happy";
    case STATE_NEEDY:       return "needy";
    case STATE_CELEBRATING: return "celebrating";
    case STATE_DEAD:        return "dead";
  }
  return "?";
}

static void sendStatusPage(WiFiClient& c) {
  uint32_t remaining = 0;
  if (state == STATE_NEEDY) {
    uint32_t elapsed = millis() - currentNeed.startedAt;
    remaining = (elapsed < NEED_DEADLINE_MS) ? (NEED_DEADLINE_MS - elapsed) / 1000 : 0;
  }

  c.println(F("HTTP/1.1 200 OK"));
  c.println(F("Content-Type: text/html"));
  c.println(F("Connection: close"));
  c.println();
  c.println(F("<!doctype html><html><head><meta charset=utf-8>"
               "<meta http-equiv=refresh content=5>"
               "<title>Tamagotchi</title>"
               "<style>body{font-family:system-ui;max-width:40em;margin:2em auto;padding:0 1em}"
               ".card{border:1px solid #ccc;border-radius:12px;padding:1em;margin:1em 0}"
               ".pin{font-family:monospace;background:#eef;padding:.1em .4em;border-radius:4px}"
               "</style></head><body>"));
  c.print(F("<h1>Tamagotchi (")); c.print(stateName()); c.println(F(")</h1>"));

  if (state == STATE_NEEDY) {
    c.println(F("<div class=card>"));
    c.print(F("<h2>")); c.print(currentNeed.title); c.println(F("</h2>"));
    c.print(F("<p>")); c.print(currentNeed.instructions); c.println(F("</p>"));
    c.print(F("<p>Target pin: <span class=pin>"));
    c.print(pinLabel(currentNeed.primaryPin)); c.println(F("</span></p>"));
    if (currentNeed.type == NEED_LED_LOOPBACK) {
      c.print(F("<p>Sense pin: <span class=pin>"));
      c.print(pinLabel(currentNeed.secondaryPin)); c.println(F("</span></p>"));
    }
    if (currentNeed.type == NEED_BUTTON_COUNT || currentNeed.type == NEED_TOGGLE) {
      c.print(F("<p>Progress: "));
      c.print(currentNeed.progress); c.print(F(" / "));
      c.print(currentNeed.target);   c.println(F("</p>"));
    }
    c.print(F("<p>Time remaining: ")); c.print(remaining); c.println(F("s</p>"));
    c.println(F("</div>"));
  } else if (state == STATE_HAPPY) {
    c.println(F("<p>Your pet is content. It will want something soon.</p>"));
  } else if (state == STATE_CELEBRATING) {
    c.println(F("<p>Yay! Thanks for the care.</p>"));
  } else {
    c.println(F("<p>Your pet has died. Press reset on the board to start over.</p>"));
  }

  if (state != STATE_DEAD) {
    c.println(F("<p><a href=\"/trigger\">Force a new need</a></p>"));
  }
  c.println(F("</body></html>"));
}

static void forceNewNeed() {
  // If a need is already in flight, release its pins first so we don't
  // leak OUTPUT state when the new need reassigns a different pinMode.
  if (state == STATE_NEEDY) releaseNeedPins();
  generateNeed();
}

static void sendRedirect(WiFiClient& c) {
  c.println(F("HTTP/1.1 303 See Other"));
  c.println(F("Location: /"));
  c.println(F("Connection: close"));
  c.println();
}

static void serviceWeb() {
  WiFiClient client = server.available();
  if (!client) return;

  // Parse the request line: "GET /path HTTP/1.1\r\n"
  char line[64];
  size_t len = 0;
  uint32_t tmo = millis() + 1000;
  while (client.connected() && millis() < tmo) {
    if (client.available()) {
      char ch = client.read();
      if (ch == '\n') break;
      if (ch != '\r' && len < sizeof(line) - 1) line[len++] = ch;
    }
  }
  line[len] = 0;

  // Drain the rest of the headers.
  while (client.available()) client.read();

  // Extract path: second space-delimited token.
  const char* path = "/";
  char* firstSpace = strchr(line, ' ');
  char* secondSpace = firstSpace ? strchr(firstSpace + 1, ' ') : nullptr;
  if (firstSpace && secondSpace) {
    *secondSpace = 0;
    path = firstSpace + 1;
  }

  if (strcmp(path, "/trigger") == 0 && state != STATE_DEAD) {
    forceNewNeed();
    sendRedirect(client);
  } else {
    sendStatusPage(client);
  }
  client.stop();
}

// ---------------- Arduino entry points ------------------------------------

void setup() {
  Serial.begin(115200);
  matrix.begin();
  randomSeed(analogRead(A5)); // floating-pin noise
  showFrame(FACE_HAPPY);
  startWiFi();
  Serial.print("WiFi.status() = "); Serial.println(WiFi.status());
  Serial.print("Firmware: ");        Serial.println(WiFi.firmwareVersion());
  scheduleNextNeed();
}

void loop() {
  uint32_t now = millis();

  // State transitions that aren't triggered by web traffic.
  switch (state) {
    case STATE_HAPPY:
      if (now >= nextNeedAt) generateNeed();
      break;

    case STATE_NEEDY:
      if (evaluateNeed())                           satisfyNeed();
      else if (now - currentNeed.startedAt >=
               NEED_DEADLINE_MS)                    killPet();
      break;

    case STATE_CELEBRATING:
    case STATE_DEAD:
      break;
  }

  animateFace();
  serviceWeb();
}

