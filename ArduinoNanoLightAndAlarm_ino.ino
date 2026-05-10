const uint8_t PIXEL_PIN = 2;     // D2 -> onboard RGB LEDs data
const uint8_t LED_COUNT = 3;
const uint8_t SPEAKER_PIN = A1;  // passive piezo / speaker
const uint8_t BUILTIN_LED = LED_BUILTIN;

struct RGB {
  uint8_t r, g, b;
};

RGB leds[LED_COUNT];

String inputLine;
bool alarmActive = false;
unsigned long lastStepMs = 0;
int stepIndex = 0;

struct AlarmStep {
  uint16_t freq;
  uint16_t durationMs;
  uint8_t r, g, b;
};

AlarmStep pattern[] = {
  {1319, 140,  0,  0,  0},
  {1047, 220, 12, 12, 12},
  {0,    180,  0,  0,  0},
};

const int PATTERN_LEN = sizeof(pattern) / sizeof(pattern[0]);

static inline void sendBit1() {
  PORTD |= _BV(PD2);
  __asm__ __volatile__(
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\nnop\n"
  );
  PORTD &= ~_BV(PD2);
  __asm__ __volatile__(
    "nop\nnop\nnop\n"
  );
}

static inline void sendBit0() {
  PORTD |= _BV(PD2);
  __asm__ __volatile__(
    "nop\nnop\nnop\n"
  );
  PORTD &= ~_BV(PD2);
  __asm__ __volatile__(
    "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
    "nop\nnop\n"
  );
}

void sendByte(uint8_t b) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    if (b & mask) sendBit1();
    else          sendBit0();
  }
}

void showLeds() {
  noInterrupts();

  for (uint8_t i = 0; i < LED_COUNT; i++) {
    // WS2812 usually expects GRB order
    sendByte(leds[i].g);
    sendByte(leds[i].r);
    sendByte(leds[i].b);
  }

  interrupts();
  delayMicroseconds(80); // latch
}

void setAllLeds(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    leds[i].r = r;
    leds[i].g = g;
    leds[i].b = b;
  }
}

void applyStep(const AlarmStep& s) {
  if (s.freq > 0) tone(SPEAKER_PIN, s.freq);
  else            noTone(SPEAKER_PIN);

  setAllLeds(s.r, s.g, s.b);
  showLeds();
}

void startAlarm() {
  alarmActive = true;
  stepIndex = 0;
  lastStepMs = 0;
  applyStep(pattern[0]);
}

void stopAlarm() {
  alarmActive = false;
  noTone(SPEAKER_PIN);
  digitalWrite(BUILTIN_LED, LOW);
  setAllLeds(0, 0, 0);
  showLeds();
}

void runAlarm() {
  unsigned long now = millis();

  if (lastStepMs == 0) {
    lastStepMs = now;
    return;
  }

  if (now - lastStepMs >= pattern[stepIndex].durationMs) {
    lastStepMs = now;
    stepIndex++;
    if (stepIndex >= PATTERN_LEN) stepIndex = 0;
    applyStep(pattern[stepIndex]);
  }
}

void handleCommand(const String& cmdRaw) {
  String cmd = cmdRaw;
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "ALARM") {
    startAlarm();
    Serial.println("OK ALARM");
  } else if (cmd == "STOP") {
    stopAlarm();
    Serial.println("OK STOP");
  } else if (cmd == "PING") {
    Serial.println("PONG");
  } else if (cmd.length() > 0) {
    Serial.print("ERR UNKNOWN: ");
    Serial.println(cmd);
  }
}

void setup() {
  DDRD |= _BV(PD2);      // D2 output for WS2812
  PORTD &= ~_BV(PD2);

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  pinMode(SPEAKER_PIN, OUTPUT);
  noTone(SPEAKER_PIN);

  setAllLeds(0, 0, 0);
  showLeds();

  Serial.begin(115200);
  inputLine.reserve(64);
  Serial.println("READY");
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputLine.length() > 0) {
        handleCommand(inputLine);
        inputLine = "";
      }
    } else {
      inputLine += c;
    }
  }

  if (alarmActive) {
    runAlarm();
  }
}