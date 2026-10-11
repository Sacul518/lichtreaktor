#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// Codex helped write this firmware, including the encoder decoder. but I did most of the heavy lifting.
// Encoder direction and LED behaviour still need checking on the assembled PCB.

// Arduino-Pico uses RP2040 GPIO numbers, not the numbers printed after D.
constexpr uint8_t LED_PINS[] = {26, 27, 28, 29, 6, 7}; // D0–D5, groups 1–6
constexpr uint8_t STATUS_LED_PINS[] = {PIN_LED_R, PIN_LED_G, PIN_LED_B};
constexpr uint8_t ENCODER_A_PIN = 0;                  // D6
constexpr uint8_t ENCODER_B_PIN = 1;                  // D7
constexpr uint8_t ENCODER_BUTTON_PIN = 2;             // D8
constexpr int8_t ENCODER_DIRECTION = 1;
// EC11E18244A5: 36 detents / 18 pulses, two transitions per detent.
constexpr int8_t ENCODER_TRANSITIONS_PER_STEP = 2;
constexpr uint8_t GROUP_COUNT = 6;
constexpr uint8_t PATTERN_COUNT = 10;
constexpr uint8_t BRIGHTNESS_STEP = 8;
constexpr uint32_t FRAME_INTERVAL_MS = 10;
constexpr uint32_t AUTO_PATTERN_MS = 10000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 25;
constexpr uint32_t LONG_PRESS_MS = 1200;
constexpr uint16_t MIN_SPEED_PERCENT = 25;
constexpr uint16_t MAX_SPEED_PERCENT = 300;
constexpr uint16_t SPEED_STEP = 25;

uint8_t brightness = 80;
uint8_t pattern = 0;
uint8_t encoderState = 0;
int8_t encoderTransitions = 0;
bool buttonRaw = false;
bool buttonStable = false;
uint32_t buttonChangedAt = 0;
uint32_t patternStartedAt = 0;
uint16_t speedPercent = 100;
bool testMode = false;
uint8_t testGroup = 0;
bool buttonUsedForAdjustment = false;
bool buttonLongHandled = false;
uint32_t buttonPressedAt = 0;
bool statusPending = true;
bool helpPending = false;
bool serialWasConnected = false;
bool autoMode = false;
bool paused = false;
uint32_t pausedAt = 0;
uint32_t autoChangedAt = 0;
uint32_t lastFrameAt = 0;
uint8_t lastOutput[GROUP_COUNT] = {};
uint8_t lastIndicator = 255;

void restartPattern(uint32_t now) {
  patternStartedAt = now;
  autoChangedAt = now;
  pausedAt = now;
}

void toggleAutoMode(uint32_t now) {
  autoMode = !autoMode;
  autoChangedAt = paused ? pausedAt : now;
  statusPending = true;
}

void togglePause(uint32_t now) {
  if (paused) {
    const uint32_t pauseDuration = now - pausedAt;
    patternStartedAt += pauseDuration;
    autoChangedAt += pauseDuration;
  } else {
    pausedAt = now;
  }
  paused = !paused;
  statusPending = true;
}

void changeBrightness(int direction) {
  const int value = brightness + direction * BRIGHTNESS_STEP;
  if (value < 0) {
    brightness = 0;
  } else if (value > 255) {
    brightness = 255;
  } else {
    brightness = value;
  }
  statusPending = true;
}

void changeSpeed(int direction, uint32_t now) {
  const int value = speedPercent + direction * SPEED_STEP;
  if (value < MIN_SPEED_PERCENT) {
    speedPercent = MIN_SPEED_PERCENT;
  } else if (value > MAX_SPEED_PERCENT) {
    speedPercent = MAX_SPEED_PERCENT;
  } else {
    speedPercent = value;
  }
  // Speed changes restart the effect at a predictable phase.
  restartPattern(now);
  statusPending = true;
}

void nextPattern(uint32_t now) {
  pattern = pattern + 1;
  if (pattern >= PATTERN_COUNT) {
    pattern = 0;
  }
  restartPattern(now);
  testMode = false;
  statusPending = true;
}

void toggleTestMode(uint32_t now) {
  testMode = !testMode;
  testGroup = 0;
  restartPattern(now);
  statusPending = true;
}

void encoderStep(int direction, uint32_t now) {
  if (buttonStable) buttonUsedForAdjustment = true;
  if (testMode) {
    int newGroup = testGroup + direction;
    if (newGroup < 0) {
      newGroup = GROUP_COUNT - 1;
    }
    if (newGroup >= GROUP_COUNT) {
      newGroup = 0;
    }
    testGroup = newGroup;
    statusPending = true;
  } else if (buttonStable) {
    changeSpeed(direction, now);
  } else {
    changeBrightness(direction);
  }
}

uint8_t readEncoder() {
  int state = 0;
  if (digitalRead(ENCODER_A_PIN) == HIGH) {
    state = state + 2;
  }
  if (digitalRead(ENCODER_B_PIN) == HIGH) {
    state = state + 1;
  }
  return state;
}

void pollEncoder(uint32_t now) {
  const uint8_t next = readEncoder();
  if (next == encoderState) return;

  if ((encoderState == 0 && next == 3) ||
      (encoderState == 3 && next == 0) ||
      (encoderState == 1 && next == 2) ||
      (encoderState == 2 && next == 1)) {
    // Both signals changed: a transition was missed. Start afresh.
    encoderTransitions = 0;
  } else {
    if ((encoderState == 0 && next == 2) ||
        (encoderState == 2 && next == 3) ||
        (encoderState == 3 && next == 1) ||
        (encoderState == 1 && next == 0)) {
      encoderTransitions++;
    } else {
      encoderTransitions--;
    }
    if (encoderTransitions >= ENCODER_TRANSITIONS_PER_STEP ||
        encoderTransitions <= -ENCODER_TRANSITIONS_PER_STEP) {
      int direction = -1;
      if (encoderTransitions > 0) {
        direction = 1;
      }
      encoderStep(direction * ENCODER_DIRECTION, now);
      encoderTransitions = 0;
    }
  }
  encoderState = next;
}

void pollButton(uint32_t now) {
  const bool pressed = digitalRead(ENCODER_BUTTON_PIN) == LOW;
  if (pressed != buttonRaw) {
    buttonRaw = pressed;
    buttonChangedAt = now;
  }
  if (buttonRaw != buttonStable &&
      uint32_t(now - buttonChangedAt) >= BUTTON_DEBOUNCE_MS) {
    buttonStable = buttonRaw;
    if (buttonStable) {
      buttonPressedAt = now;
      buttonUsedForAdjustment = false;
      buttonLongHandled = false;
    } else if (!buttonUsedForAdjustment && !buttonLongHandled) {
      // Wait for release so a speed adjustment does not also change the pattern.
      if (testMode) toggleTestMode(now);
      else nextPattern(now);
    }
  }
  if (buttonStable && buttonRaw && !buttonUsedForAdjustment && !buttonLongHandled &&
      uint32_t(now - buttonPressedAt) >= LONG_PRESS_MS) {
    toggleTestMode(now);
    buttonLongHandled = true;
  }
}

void pollSerial(uint32_t now) {
  const bool connected = bool(Serial);
  if (connected && !serialWasConnected) statusPending = true;
  serialWasConnected = connected;
  if (!connected) return;

  // Limits USB work per loop so pasted commands cannot monopolise encoder polling.
  for (uint8_t count = 0; count < 8 && Serial.available(); ++count) {
    const int command = Serial.read();
    if (command == 's') {
      statusPending = true;
    } else if (command == '?') {
      helpPending = true;
    } else if (command == 't') {
      toggleTestMode(now);
    } else if (command == 'n') {
      nextPattern(now);
    } else if (command == 'a') {
      toggleAutoMode(now);
    } else if (command == 'p') {
      togglePause(now);
    } else if (command == '+') {
      changeBrightness(1);
    } else if (command == '-') {
      changeBrightness(-1);
    } else if (command == '>') {
      changeSpeed(1, now);
    } else if (command == '<') {
      changeSpeed(-1, now);
    } else if (command >= '1' && command <= '6') {
      testMode = true;
      testGroup = command - '1';
      statusPending = true;
    }
  }

  // Keep replies short and wait for USB buffer space rather than blocking LEDs.
  char reply[64];
  if (!helpPending && !statusPending) return;
  if (Serial.availableForWrite() < int(sizeof(reply) - 1)) return;
  if (helpPending) {
    snprintf(reply, sizeof(reply), "s:info n:next a:auto p:pause t:test 1-6:group +/-:dim <>:speed\n");
  } else if (statusPending) {
    snprintf(reply, sizeof(reply), "mode=%u b=%u speed=%u%% test=%u g=%u auto=%u pause=%u\n",
             unsigned(pattern + 1), unsigned(brightness), unsigned(speedPercent),
             unsigned(testMode), unsigned(testGroup + 1), unsigned(autoMode), unsigned(paused));
  }
  const size_t length = strlen(reply);
  if (Serial.availableForWrite() >= int(length)) {
    Serial.write(reinterpret_cast<const uint8_t *>(reply), length);
    if (helpPending) helpPending = false;
    else statusPending = false;
  }
}

uint8_t patternLevel(uint8_t selected, uint8_t group, uint32_t elapsed) {
  if (selected == 0) {
    return 255;
  }

  if (selected == 1) {
    uint32_t phase = elapsed % 2400;
    uint32_t ramp = 0;
    if (phase < 1200) {
      ramp = phase;
    } else {
      ramp = 2400 - phase;
    }
    uint32_t level = ramp * 255;
    level = level / 1200;
    level = level * level / 255;
    return level;
  }

  if (selected == 2) {
    int head = (elapsed / 160) % GROUP_COUNT;
    int behind = head - group;
    if (behind < 0) {
      behind = behind + GROUP_COUNT;
    }
    if (behind == 0) return 255;
    if (behind == 1) return 70;
    if (behind == 2) return 15;
    return 0;
  }

  if (selected == 3) {
    int groupSide = group % 2;
    int activeSide = (elapsed / 400) % 2;
    if (groupSide == activeSide) {
      return 255;
    }
    return 0;
  }

  if (selected == 4) {
    uint32_t phase = elapsed % 1400;
    if (phase < 120) {
      return 255 - phase * 255 / 120;
    }
    if (phase >= 230 && phase < 400) {
      return (400 - phase) * 255 / 170;
    }
    return 0;
  }

  if (selected == 5) {
    int head = (elapsed / 130) % (2 * (GROUP_COUNT - 1));
    if (head >= GROUP_COUNT) head = 2 * (GROUP_COUNT - 1) - head;
    int distance = head - group;
    if (distance < 0) distance = -distance;
    if (distance == 0) return 255;
    if (distance == 1) return 60;
    return 0;
  }

  if (selected == 6) {
    uint32_t phase = (elapsed % 1800 + group * 300) % 1800;
    uint32_t ramp = phase;
    if (phase > 900) ramp = 1800 - phase;
    return ramp * 255 / 900;
  }

  if (selected == 7) {
    int litGroups = (elapsed / 180) % (GROUP_COUNT * 2);
    if (litGroups > GROUP_COUNT) litGroups = GROUP_COUNT * 2 - litGroups;
    if (group < litGroups) return 255;
    return 0;
  }

  if (selected == 8) {
    const uint8_t sparkGroups[] = {0, 3, 1, 5, 2, 4, 1, 4, 0, 2, 5, 3};
    uint8_t step = (elapsed / 110) % sizeof(sparkGroups);
    if (group == sparkGroups[step]) return 255 - (elapsed % 110) * 255 / 110;
    return 8;
  }

  if (selected == 9) {
    int clockwise = (elapsed / 160) % GROUP_COUNT;
    int counterclockwise = (GROUP_COUNT - clockwise) % GROUP_COUNT;
    int behindClockwise = (clockwise + GROUP_COUNT - group) % GROUP_COUNT;
    int behindCounterclockwise = (group + GROUP_COUNT - counterclockwise) % GROUP_COUNT;
    if (behindClockwise == 0 || behindCounterclockwise == 0) return 255;
    if (behindClockwise == 1 || behindCounterclockwise == 1) return 60;
    return 0;
  }

  return 0;
}

void render(uint32_t elapsed) {
  for (uint8_t group = 0; group < GROUP_COUNT; ++group) {
    uint16_t level = 0;
    if (testMode) {
      if (group == testGroup) level = 255;
    } else {
      level = patternLevel(pattern, group, elapsed);
    }
    const uint8_t output = (level * brightness + 127) / 255;
    if (output != lastOutput[group]) {
      analogWrite(LED_PINS[group], output);
      lastOutput[group] = output;
    }
  }
}

void updateIndicator() {
  uint8_t mode = 0;
  if (testMode) mode = 3;
  else if (paused) mode = 2;
  else if (autoMode) mode = 1;
  if (mode == lastIndicator) return;

  // The XIAO's built-in red, green and blue LEDs are active low.
  digitalWrite(PIN_LED_R, (mode == 2 || mode == 3) ? LOW : HIGH);
  digitalWrite(PIN_LED_G, (mode == 1 || mode == 2) ? LOW : HIGH);
  digitalWrite(PIN_LED_B, mode == 0 ? LOW : HIGH);
  lastIndicator = mode;
}

void setup() {
  Serial.begin(115200);
  // Do not wait for Serial: the reactor must also start from a USB power supply.
  for (uint8_t pin : LED_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  analogWriteFreq(2000);
  analogWriteRange(255);
  for (uint8_t pin : STATUS_LED_PINS) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
  }

  // External 10 kΩ pull-ups already connect the encoder inputs to 3.3 V.
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_BUTTON_PIN, INPUT);
  encoderState = readEncoder();
  buttonRaw = digitalRead(ENCODER_BUTTON_PIN) == LOW;
  buttonStable = buttonRaw;
  buttonUsedForAdjustment = buttonStable;
  buttonChangedAt = millis();
  restartPattern(buttonChangedAt);
  lastFrameAt = buttonChangedAt;
  render(0);
  updateIndicator();
}

void loop() {
  const uint32_t now = millis();
  pollEncoder(now);
  pollButton(now);
  pollSerial(now);
  if (autoMode && !paused && !testMode &&
      uint32_t(now - autoChangedAt) >= AUTO_PATTERN_MS) {
    nextPattern(now);
  }
  updateIndicator();
  if (uint32_t(now - lastFrameAt) < FRAME_INTERVAL_MS) return;
  lastFrameAt = now;
  // Unsigned subtraction also works when millis() wraps around.
  const uint32_t effectNow = paused ? pausedAt : now;
  const uint64_t elapsed = uint32_t(effectNow - patternStartedAt);
  render(uint32_t(elapsed * speedPercent / 100));
}
