#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>  // built into Teensyduino
#include <Bounce.h>   // for debouncing the push-button

// --- pins -------------------------------------------------
const int POT0_PIN = A0;   // analog 0  (Teensy pin 14) – volume
const int POT1_PIN = A1;   // analog 1  (Teensy pin 15) – frequency
const int ENC_A_PIN = 41;  // encoder CLK
const int ENC_B_PIN = 40;  // encoder DT
const int ENC_SW = 9;      // encoder push-button
const int KEY_PIN = 2;     // CW keyer (active-LOW)
const int LED_PIN = 13;    // Onboard LED for status indication

// --- audio constants -------------------------------------
const float MIN_FREQ = 300.0f;   // Hz
const float MAX_FREQ = 1200.0f;  // Hz

// --- CW decoding constants -------------------------------
const unsigned int DIT_DURATION_MAX = 25;  // ms – press shorter than this = dit
const unsigned int CHAR_SPACE_MIN = 100;   // ms silence to mark end of character
const unsigned int WORD_SPACE_MIN = 250;   // ms silence to mark space


// --- OLED display ---------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- audio objects ---------------------------------------
AudioSynthWaveform wave1;                       // simple sine wave generator
AudioOutputI2S i2s1;                            // I2S output (to MAX98357A)
AudioConnection patchCord1(wave1, 0, i2s1, 0);  // left channel
AudioConnection patchCord2(wave1, 0, i2s1, 1);  // right channel

// --- other objects ---------------------------------------
Encoder knob(ENC_A_PIN, ENC_B_PIN);
Bounce button(ENC_SW, 20);  // 20 ms debounce

// --- globals ---------------------------------------------
bool lastKeyState = HIGH;  // previous state of KEY_PIN
bool tonePlaying = false;  // true while sidetone is sounding
// --- serial reporting cache ------------------------------

// CW decoding state
String currentSymbol = "";      // collects '.' and '-'
unsigned long toneOnTime = 0;   // when key went down
unsigned long toneOffTime = 0;  // last time key went up
float lastFreqReport = -1;
int lastVolReport = -1;
bool lastKeyReport = HIGH;
char lastDecodedChar = ' ';
unsigned long lastDisplayUpdate = 0;
bool menuActive = false;
const char* menuItems[] = { "< Back", "Play Callsign", "Waveform", "Koch", "Mode", "Delay" };
const int MENU_COUNT = 6;
int menuIndex = 0;
long lastEncoderPos = 0;
const int ENCODER_STEP = 4;  // steps per detent for this encoder/Teensy
unsigned long buttonPressTime = 0;

// --- playback speed (WPM) ---------------------------------
int playbackWpm = 20;  // words per minute, 5..40
const int WPM_MIN = 5;
const int WPM_MAX = 40;

// --- adaptive CW timing ----------------------------------
const uint8_t UNIT_SAMPLE_MAX = 10;
uint16_t unitSamples[UNIT_SAMPLE_MAX] = { 0 };
uint8_t unitPos = 0;
uint16_t unitLen = 60;  // ms, initial guess (~20 WPM)

void addUnitSample(uint16_t v) {
  unitSamples[unitPos++] = v;
  if (unitPos >= UNIT_SAMPLE_MAX) unitPos = 0;

  // compute mean of non-zero samples
  uint32_t sum = 0;
  uint8_t n = 0;
  for (uint8_t i = 0; i < UNIT_SAMPLE_MAX; i++) {
    if (unitSamples[i]) {
      sum += unitSamples[i];
      n++;
    }
  }
  if (n) unitLen = sum / n;
}

const char* waveformNames[] = { "Sine", "Square", "Sawtooth", "Triangle" };
const uint8_t waveformTypes[] = { WAVEFORM_SINE, WAVEFORM_SQUARE, WAVEFORM_SAWTOOTH, WAVEFORM_TRIANGLE };
int currentWaveformIndex = 0;
bool waveformMenuActive = false;
int waveformMenuIndex = 0;

// Mode submenu
bool modeMenuActive = false;
int modeMenuIndex = 0;  // 0 = Audio First, 1 = Audio After

// --- Koch training state ---------------------------------
bool trainingActive = false;
unsigned long trainingNextTime = 0;  // timestamp for next prompt
int trainingLevel = 0;               // current Koch lesson level

// --- training modes --------------------------------------
enum TrainingMode { MODE_AUDIO_FIRST,
                    MODE_AUDIO_AFTER };
TrainingMode trainingMode = MODE_AUDIO_FIRST;
char trainingChar = 0;      // current character being trained
uint8_t trainingStage = 0;  // 0=prompt shown, 1=delay/after stage
// --- training timing & counters ---------------------------
int trainingDelay = 3000;  // ms; user adjustable
const int TRAINING_DELAY_MIN = 0;
const int TRAINING_DELAY_MAX = 5000;
const int TRAINING_DELAY_STEP = 250;
int trainingCount = 0;  // letters presented so far

// Delay submenu state
bool delayMenuActive = false;
int delayMenuCursor = 1;  // 0 = < Back , 1 = delay value
bool delayEditing = false;   // true while user is adjusting the value

// Koch submenu cursor (0 = < Back, 1..KOCH_LEVEL_COUNT)
int kochMenuCursor = 0;

// Koch method character progression
String kochLessons[] = {
  "KM", "KMR", "KMRS", "KMRSU", "KMRSUA", "KMRSUAP", "KMRSUAPT", "KMRSUAPTL",
  "KMRSUAPTLO", "KMRSUAPTLOW", "KMRSUAPTLOWI", "KMRSUAPTLOWIN", "KMRSUAPTLOWING",
  "KMRSUAPTLOWINGD", "KMRSUAPTLOWINGDK", "KMRSUAPTLOWINGDKG", "KMRSUAPTLOWINGDKGO",
  "KMRSUAPTLOWINGDKGOH", "KMRSUAPTLOWINGDKGOHV", "KMRSUAPTLOWINGDKGOHVF",
  "KMRSUAPTLOWINGDKGOHVFU", "KMRSUAPTLOWINGDKGOHVFUJ", "KMRSUAPTLOWINGDKGOHVFUJE",
  "KMRSUAPTLOWINGDKGOHVFUJEL", "KMRSUAPTLOWINGDKGOHVFUJELB", "KMRSUAPTLOWINGDKGOHVFUJELBY",
  "KMRSUAPTLOWINGDKGOHVFUJELBYC", "KMRSUAPTLOWINGDKGOHVFUJELBYCK", "KMRSUAPTLOWINGDKGOHVFUJELBYCKX",
  "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQ", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ5",
  "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ54", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ543", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ5432",
  "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ54321", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ543210", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ5432109",
  "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ54321098", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ543210987", "KMRSUAPTLOWINGDKGOHVFUJELBYCKXQZ5432109876"
};

// --- Koch level menu state ------------------------------
const int KOCH_LEVEL_COUNT = sizeof(kochLessons) / sizeof(String);
bool kochLevelMenuActive = false;
int kochLevelIndex = 0;
int kochLevelViewOffset = 0;  // for scrolling display

// --- Morse table -----------------------------------------
struct MorseEntry {
  const char* code;
  char ch;
};
const MorseEntry MORSE_TABLE[] = {
  { ".-", 'A' }, { "-...", 'B' }, { "-.-.", 'C' }, { "-..", 'D' }, { ".", 'E' }, { "..-.", 'F' }, { "--.", 'G' }, { "....", 'H' }, { "..", 'I' }, { ".---", 'J' }, { "-.-", 'K' }, { ".-..", 'L' }, { "--", 'M' }, { "-.", 'N' }, { "---", 'O' }, { ".--.", 'P' }, { "--.-", 'Q' }, { ".-.", 'R' }, { "...", 'S' }, { "-", 'T' }, { "..-", 'U' }, { "...-", 'V' }, { ".--", 'W' }, { "-..-", 'X' }, { "-.--", 'Y' }, { "--..", 'Z' }, { ".----", '1' }, { "..---", '2' }, { "...--", '3' }, { "....-", '4' }, { ".....", '5' }, { "-....", '6' }, { "--...", '7' }, { "---..", '8' }, { "----.", '9' }, { "-----", '0' }, { nullptr, 0 }
};

char decodeMorse(const String& sym) {
  for (int i = 0; MORSE_TABLE[i].code != nullptr; i++) {
    if (sym.equals(MORSE_TABLE[i].code)) return MORSE_TABLE[i].ch;
  }
  return '?';  // unknown
}

// --- Character to Morse lookup ---------------------------
const char* charToMorse(char c) {
  if (c == ' ')
    return nullptr;  // space handled separately
  // convert to uppercase
  if (c >= 'a' && c <= 'z') c -= 32;
  for (int i = 0; MORSE_TABLE[i].code != nullptr; i++) {
    if (MORSE_TABLE[i].ch == c) return MORSE_TABLE[i].code;
  }
  return nullptr;  // unknown
}

// --- Play a string as Morse --------------------------------
// Plays the input string using CW audio according to current unitLen.
// Gaps:
//   element gap: 1 unit (handled inside loop)
//   character gap: 3 units (1 already added, plus 2 after each char)
//   word gap: 7 units
// Amplitude derives from volume potentiometer (POT0_PIN) each element.
void playMorseString(const String& msg) {
  uint16_t ditLen = 1200 / playbackWpm;  // standard dit timing
  for (size_t idx = 0; idx < msg.length(); idx++) {
    char ch = msg.charAt(idx);
    if (ch == ' ') {
      wave1.amplitude(0);
      delay(ditLen * 7);
      continue;
    }
    const char* code = charToMorse(ch);
    if (!code) continue;  // skip unsupported chars

    for (int i = 0; code[i] != '\0'; i++) {
      char symbol = code[i];
      float amp = constrain(analogRead(POT0_PIN) / 4095.0f, 0.0f, 1.0f);
      wave1.amplitude(amp);
      if (symbol == '.') {
        delay(ditLen);
      } else {  // '-'
        delay(ditLen * 3);
      }
      wave1.amplitude(0);
      // inter-element gap (1 unit) except after last element handled anyway
      delay(ditLen);
    }
    // already waited 1 unit; add 2 more to make 3-unit character gap
    delay(ditLen * 2);
  }
}


void decodeSymbol(const String& sym) {
  char c = decodeMorse(sym);
  lastDecodedChar = c;
  //Serial.print(c);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(115200);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("CW Trainer");
    display.display();
  }

  // GPIO setup
  pinMode(ENC_SW, INPUT_PULLUP);   // button is active-LOW
  pinMode(KEY_PIN, INPUT_PULLUP);  // keyer is active-LOW
  analogReadResolution(12);        // 0-4095

  // Audio system setup
  AudioMemory(8);  // allocate audio blocks for Audio library
  wave1.begin(0, 600, waveformTypes[currentWaveformIndex]);
  wave1.amplitude(0);  // start silent
  randomSeed(analogRead(A0));
}

void loop() {
  // --- Button handling ------------------------------------
  button.update();

  // --- Key input handling -----------------------------------
  bool keyState = digitalReadFast(KEY_PIN);

  // Edge detection for key transitions
  if (keyState == LOW && !tonePlaying) {  // key just went down
    tonePlaying = true;
    toneOnTime = millis();
    // Fetch pots once to minimise latency
    int freqRaw = analogRead(POT1_PIN);
    int volRaw  = analogRead(POT0_PIN);
    float freq = MIN_FREQ + ((MAX_FREQ - MIN_FREQ) * freqRaw / 4095.0f);
    float amp  = constrain(volRaw / 4095.0f, 0.0f, 1.0f);
    wave1.frequency(freq);
    wave1.amplitude(amp);
    digitalWriteFast(LED_PIN, LOW);
  } else if (keyState == HIGH && tonePlaying) {  // key just released
    tonePlaying = false;
    toneOffTime = millis();
    wave1.amplitude(0);
    digitalWriteFast(LED_PIN, HIGH);

    unsigned long pressDur = toneOffTime - toneOnTime;
    if (pressDur < DIT_DURATION_MAX) {
      currentSymbol += '.';
    } else {
      currentSymbol += '-';
    }
    addUnitSample(pressDur);
  }

  lastKeyState = keyState;

  // If key is currently pressed, skip the remainder of the loop to keep
  // the cycle time extremely short (avoids display I²C transfer etc.)
  if (tonePlaying) return;

  // End-of-symbol / character / word detection while key is up
  if (lastKeyState == HIGH && currentSymbol.length() > 0) {
    unsigned long gap = millis() - toneOffTime;
    if (gap > WORD_SPACE_MIN) {
      decodeSymbol(currentSymbol);
      currentSymbol = "";
      lastDecodedChar = ' ';
    } else if (gap > CHAR_SPACE_MIN) {
      decodeSymbol(currentSymbol);
      currentSymbol = "";
    }
  }

  // --- Koch Training Mode -------------------------------
  if (trainingActive) {
    // exit training on button press
    if (button.risingEdge()) {
      trainingActive = false;
      return;  // back to normal loop
    }

    if (millis() >= trainingNextTime) {
      if (trainingStage == 0) {
        // select new character
        String lesson = kochLessons[trainingLevel];
        trainingCount++;  // increment counter
        trainingChar = lesson.charAt(random(lesson.length()));
        // Display character
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("#");
        display.println(trainingCount);
        display.setTextSize(3);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 20);
        display.println(trainingChar);

        // Draw bottom-row status (frequency, volume, WPM) in small font
        int volRaw = analogRead(POT0_PIN);
        int freqRaw = analogRead(POT1_PIN);
        float amplitude = constrain(volRaw / 4095.0f, 0.0f, 1.0f);
        int volPct = (int)(amplitude * 100 + 0.5f);
        float freq = MIN_FREQ + ((MAX_FREQ - MIN_FREQ) * freqRaw / 4095.0f);

        display.setTextSize(1);
        display.setCursor(0, 56);  // last 8-pixel row on 64-pixel display
        display.print("F:");
        display.print((int)freq);
        display.print("Hz ");
        display.print("V:");
        display.print(volPct);
        display.print("% ");
        display.print(playbackWpm);
        display.print("wpm");
        display.display();

        if (trainingMode == MODE_AUDIO_FIRST) {
          playMorseString(String(trainingChar));
        }
        trainingStage = 1;
        trainingNextTime = millis() + trainingDelay;
      } else {
        // after delay period
        if (trainingMode == MODE_AUDIO_AFTER) {
          playMorseString(String(trainingChar));
        }
        trainingStage = 0;            // start next round
        trainingNextTime = millis();  // + trainingDelay;
      }
    }
  }
  if (button.fallingEdge()) {
    buttonPressTime = millis();
  } else if (button.risingEdge()) {
    unsigned long pressDur = millis() - buttonPressTime;

    if (menuActive) {
      // Short press selects current menu item
      if (waveformMenuActive) {
        if (waveformMenuIndex == 0) {
          waveformMenuActive = false;  // < Back
        } else {
          currentWaveformIndex = waveformMenuIndex - 1;
          wave1.begin(0, 600, waveformTypes[currentWaveformIndex]);
        }
      } else if (delayMenuActive) {
         if (delayMenuCursor == 0) {
           // < Back selected
           delayMenuActive = false;
           delayEditing = false;
         } else { // cursor on value row
           // toggle edit mode
           delayEditing = !delayEditing;
         }

      } else if (kochLevelMenuActive) {
        if (kochMenuCursor == 0) {
          kochLevelMenuActive = false;
        } else {
          trainingLevel = kochMenuCursor - 1;
          trainingActive = true;
          trainingCount = 0;
          kochLevelMenuActive = false;
          menuActive = false;
          trainingNextTime = 0;
        }
      } else if (modeMenuActive) {
        if (modeMenuIndex == 0) {
          modeMenuActive = false;
        } else {
          trainingMode = (modeMenuIndex == 1) ? MODE_AUDIO_FIRST : MODE_AUDIO_AFTER;
        }
      } else if (menuIndex == 0) {  // < Back
        menuActive = false;
      } else if (menuIndex == 1) {  // Play Callsign
        menuActive = false;
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("KG5CKI");
        display.display();
        playMorseString("KG5CKI");
        lastDisplayUpdate = millis();
      } else if (menuIndex == 2) {  // Waveform
        waveformMenuActive = true;
        waveformMenuIndex = 0;
      } else if (menuIndex == 3) {  // Koch level submenu
        kochLevelMenuActive = true;
        kochMenuCursor = trainingLevel + 1;  // row with current level
        kochLevelIndex = trainingLevel;
        kochLevelViewOffset = (kochLevelIndex / 6) * 6;  // page containing current level
      } else if (menuIndex == 4) {  // Mode submenu
        modeMenuActive = true;
        modeMenuIndex = (trainingMode == MODE_AUDIO_FIRST) ? 1 : 2;
      } else if (menuIndex == 5) {  // Delay submenu
        delayMenuActive = true;
        delayMenuCursor = 1;
        delayEditing = false;
      }
    } else {
      // Menu not active: short press opens menu
      if (pressDur < 1000) {
        menuActive = true;
        menuIndex = 1;  // first actionable item
        lastEncoderPos = knob.read();
      }
    }
  }
// --- Encoder / navigation --------------------------------
if (menuActive) {
  long pos = knob.read();
  // handle multiple steps and avoid large jumps
  while (pos - lastEncoderPos >= ENCODER_STEP) {
    if (waveformMenuActive) {
      waveformMenuIndex = (waveformMenuIndex + 1) % 5;
    } else if (kochLevelMenuActive) {
      kochMenuCursor = (kochMenuCursor + 1) % (KOCH_LEVEL_COUNT + 1);
      if (kochMenuCursor > 0) {
        kochLevelIndex = kochMenuCursor - 1;
        if (kochLevelIndex >= kochLevelViewOffset + 6) kochLevelViewOffset += 6;
      }
    } else if (modeMenuActive) {
      modeMenuIndex = (modeMenuIndex + 1) % 3;
    } else if (delayMenuActive) {
       if (delayEditing) {
         // editing value – increase
         trainingDelay = min(trainingDelay + TRAINING_DELAY_STEP, TRAINING_DELAY_MAX);
       } else {
         // navigating between rows
         delayMenuCursor = (delayMenuCursor + 1) % 2;  // toggle 0↔1
       }
     } else {
      menuIndex = (menuIndex + 1) % MENU_COUNT;
    }
    lastEncoderPos += ENCODER_STEP;
  }
  while (pos - lastEncoderPos <= -ENCODER_STEP) {
    if (waveformMenuActive) {
      waveformMenuIndex = (waveformMenuIndex - 1 + 5) % 5;
    } else if (kochLevelMenuActive) {
      kochMenuCursor = (kochMenuCursor - 1 + (KOCH_LEVEL_COUNT + 1)) % (KOCH_LEVEL_COUNT + 1);
      if (kochMenuCursor > 0) {
        kochLevelIndex = kochMenuCursor - 1;
        if (kochLevelIndex < kochLevelViewOffset) kochLevelViewOffset = max(0, kochLevelViewOffset - 6);
      }
    } else if (modeMenuActive) {
      modeMenuIndex = (modeMenuIndex - 1 + 3) % 3;
    } else if (delayMenuActive) {
       if (delayEditing) {
         // editing value – decrease
         trainingDelay = max(trainingDelay - TRAINING_DELAY_STEP, TRAINING_DELAY_MIN);
       } else {
         // navigating rows
         delayMenuCursor = (delayMenuCursor - 1 + 2) % 2;
       }
     } else {
      menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
    }
    lastEncoderPos -= ENCODER_STEP;
  }
}
// --- OLED display update --------------------------------
// Skip regular status refresh during active training to keep character visible
if (millis() - lastDisplayUpdate > 200 && (!trainingActive || menuActive)) {
  display.clearDisplay();
  if (menuActive) {
    if (waveformMenuActive) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      // row 0 = < Back
      display.setCursor(0, 0);
      display.print(waveformMenuIndex == 0 ? "> " : "  ");
      display.println("< Back");
      for (int i = 0; i < 4; i++) {
        display.setCursor(0, (i + 1) * 10);
        int drawIndex = i + 1;
        display.print(drawIndex == waveformMenuIndex ? "> " : "  ");
        display.print(waveformNames[i]);
        if (i == currentWaveformIndex) display.print(" *");
        display.println();
      }
    } else if (kochLevelMenuActive) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      // row 0 Back
      display.setCursor(0, 0);
      display.print(kochMenuCursor == 0 ? "> " : "  ");
      display.println("< Back");
      // list levels paged 6 per screen starting from viewOffset
      for (int i = 0; i < 6 && (kochLevelViewOffset + i) < KOCH_LEVEL_COUNT; i++) {
        int idx = kochLevelViewOffset + i;  // actual level index
        int drawRow = i + 1;                // row 1..6
        display.setCursor(0, drawRow * 10);
        int cursorCmp = (kochMenuCursor == (idx + 1));
        display.print(cursorCmp ? "> " : "  ");
        display.print("L");
        display.println(idx + 1);
      }
    } else if (modeMenuActive) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      const char* modeNames[2] = { "Audio First", "Audio After" };
      // row 0 < Back
      display.setCursor(0, 0);
      display.print(modeMenuIndex == 0 ? "> " : "  ");
      display.println("< Back");
      for (int i = 0; i < 2; i++) {
        display.setCursor(0, (i + 1) * 10);
        int drawIndex = i + 1;  // 1 or 2
        display.print(drawIndex == modeMenuIndex ? "> " : "  ");
        display.print(modeNames[i]);
        if ((i == 0 && trainingMode == MODE_AUDIO_FIRST) || (i == 1 && trainingMode == MODE_AUDIO_AFTER))
          display.print(" *");
        display.println();
      }
    } else if (delayMenuActive) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      // row 0 < Back
      display.setCursor(0, 0);
      display.print(delayMenuCursor == 0 ? "> " : "  ");
      display.println("< Back");
      // row1 header
      display.setCursor(0, 10);
      display.println("Delay (ms)");
      // value row highlight indicator
      display.setTextSize(2);
      display.setCursor(0, 24);
      if (delayMenuCursor == 1) display.print("> ");
      else display.print("  ");
      display.print(trainingDelay);
    } else {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      for (int i = 0; i < MENU_COUNT; i++) {
        display.setCursor(0, i * 10);  // size-1 font ~8px tall
        display.print(i == menuIndex ? "> " : "  ");
        display.println(menuItems[i]);
      }
    }
  } else {
     // Gather live frequency and volume for status display
     int volRaw = analogRead(POT0_PIN);
     int freqRaw = analogRead(POT1_PIN);
     float amplitude = constrain(volRaw / 4095.0f, 0.0f, 1.0f);
     float freq = MIN_FREQ + ((MAX_FREQ - MIN_FREQ) * freqRaw / 4095.0f);

     display.setTextSize(2);
     display.setTextColor(SSD1306_WHITE);
     display.setCursor(0, 0);
     display.print("Frq:");
     display.print((int)freq);
     display.println("Hz");
     display.print("Vol:");
     display.print((int)(amplitude * 100 + 0.5));
     display.println("%");
     display.print(playbackWpm);
     display.println("wpm");
   }
   display.display();
   lastDisplayUpdate = millis();
 }
}
