#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// Codex helped write this firmware, including the encoder decoder. but I did most of the heavy lifting.
// Encoder direction and LED behaviour still need checking on the assembled PCB.

// Arduino-Pico uses RP2040 GPIO numbers, not the numbers printed after D.
constexpr uint8_t LED_PINS[] = {26, 27, 28, 29, 6, 7}; // D0–D5, groups 1–6
constexpr uint8_t ENCODER_A_PIN = 0;                  // D6
constexpr uint8_t ENCODER_B_PIN = 1;                  // D7
constexpr uint8_t ENCODER_BUTTON_PIN = 2;             // D8
constexpr int8_t ENCODER_DIRECTION = 1;
// EC11E18244A5: 36 dtents / 18 pulses, two transitions per detent.
constexpr int8_t ENCODER_TRANSITIONS_PER_STEP = 2;
constexpr uint8_t GROUP_COUNT = 6;
constexpr uint8_t PATTERN_COUNT = 5;
constexpr uint8_t BRIGHTNESS_STEP = 8;
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
  // Speed changes restart the effect at a predictabel phase.
  patternStartedAt = now;
  statusPending = true;
}

void nextPattern(uint32_t now) {
  pattern = pattern + 1;
  if (pattern >= PATTERN_COUNT) {
    pattern = 0;
  }
  patternStartedAt = now;
  testMode = false;
  statusPending = true;
}

void toggleTestMode(uint32_t now) {
  testMode = !testMode;
  testGroup = 0;
  patternStartedAt = now;
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
  if (helpPending) {
    snprintf(reply, sizeof(reply), "s=status t=test n=next 1-6=group +/-=bright </>=speed\n");
  } else if (statusPending) {
    snprintf(reply, sizeof(reply), "mode=%u bright=%u speed=%u%% test=%u group=%u\n",
             unsigned(pattern + 1), unsigned(brightness), unsigned(speedPercent),
             unsigned(testMode), unsigned(testGroup + 1));
  } else {
    return;
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
    float output = level;
    output = output * brightness;
    output = output + 127;
    output = output / 255;
    analogWrite(LED_PINS[group], int(output));
  }
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

  // External 10 kΩ pull-ups already connect the encoder inputs to 3.3 V.
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_BUTTON_PIN, INPUT);
  encoderState = readEncoder();
  buttonRaw = digitalRead(ENCODER_BUTTON_PIN) == LOW;
  buttonStable = buttonRaw;
  buttonUsedForAdjustment = buttonStable;
  buttonChangedAt = millis();
  patternStartedAt = buttonChangedAt;
  render(0);
}

void loop() {
  const uint32_t now = millis();
  pollEncoder(now);
  pollButton(now);
  pollSerial(now);
  // Unsigned subtraction also works when millis() wraps around. took me 10min just to figure that out.
  const uint64_t elapsed = uint32_t(now - patternStartedAt);
  render(uint32_t(elapsed * speedPercent / 100));
}
