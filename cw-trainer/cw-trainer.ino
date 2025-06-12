#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
// Optional: Uncomment these lines if you add WiFi via ESP32/ESP8266 module
// #include <WiFi.h>
// #include <WebServer.h>

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi and Web Server (Optional - requires external module)
// const char* ssid = "YourWiFiSSID";
// const char* password = "YourWiFiPassword";
// WebServer server(80);
bool wifiEnabled = false;  // Set to true if you add WiFi module

// Audio objects for generation
AudioSynthWaveformSine sine1;
AudioSynthWaveform waveform1;
AudioMixer4 mixer1;
AudioEffectEnvelope envelope1;

// Audio objects for decoding and input
AudioAnalyzeToneDetect toneDetect1;
AudioInputI2S audioInput;  // For radio input
AudioMixer4 decodeMixer;
AudioAnalyzeFFT1024 fft1024;  // For spectrum analysis
AudioEffectMultiply agc;      // Simple AGC

// Audio outputs
AudioOutputAnalog dac1;
AudioOutputI2S i2s1;

// Audio connections - Generation path
AudioConnection patchCord1(sine1, 0, mixer1, 0);
AudioConnection patchCord2(waveform1, 0, mixer1, 1);
AudioConnection patchCord3(mixer1, envelope1);
AudioConnection patchCord4(envelope1, 0, dac1, 0);
AudioConnection patchCord5(envelope1, 0, i2s1, 0);
AudioConnection patchCord6(envelope1, 0, i2s1, 1);

// Audio connections - Decoding path
AudioConnection patchCord7(envelope1, 0, decodeMixer, 0);   // Internal sidetone
AudioConnection patchCord8(audioInput, 0, decodeMixer, 1);  // External audio
AudioConnection patchCord9(decodeMixer, toneDetect1);
AudioConnection patchCord10(decodeMixer, 0, fft1024, 0);  // Spectrum analysis

AudioControlSGTL5000 sgtl5000_1;

// Pin definitions
const int KEY_PIN = 2;
const int FREQ_POT = A0;
const int VOLUME_POT = A1;
const int SPEED_POT = A2;  // Koch speed control
const int WAVEFORM_BTN = 3;
const int OUTPUT_SELECT_BTN = 4;
const int DECODER_TOGGLE_BTN = 5;
const int KOCH_MODE_BTN = 6;
const int KOCH_NEXT_BTN = 7;
const int INPUT_SELECT_BTN = 8;  // Toggle internal/external audio
const int MENU_BTN = 9;          // Menu navigation
const int SELECT_BTN = 10;       // Menu selection
const int AUDIO_INPUT_PIN = A3;  // External audio level

// Debounce objects
Bounce keyDebouncer = Bounce();
Bounce waveformButton = Bounce();
Bounce outputButton = Bounce();
Bounce decoderButton = Bounce();
Bounce kochModeButton = Bounce();
Bounce kochNextButton = Bounce();
Bounce inputSelectButton = Bounce();
Bounce menuButton = Bounce();
Bounce selectButton = Bounce();

// Configuration variables
float sidetoneFreq = 600.0;
float volume = 0.5;
int currentWaveform = 0;
bool useHeadphones = true;
bool keyPressed = false;
bool decoderEnabled = true;
bool kochModeEnabled = false;
bool useExternalAudio = false;  // false = sidetone, true = radio input
unsigned long lastFreqUpdate = 0;
unsigned long lastVolumeUpdate = 0;
unsigned long lastDisplayUpdate = 0;

// Menu system
enum MenuMode { MAIN_SCREEN,
                KOCH_MENU,
                PRACTICE_MENU,
                SETTINGS_MENU,
                STATS_MENU,
                QSO_MENU };
MenuMode currentMenu = MAIN_SCREEN;
int menuSelection = 0;
bool inMenu = false;

// Practice modes
enum PracticeMode { KOCH_TRAINING,
                    CALLSIGN_PRACTICE,
                    QSO_SIMULATION,
                    CONTEST_MODE,
                    CUSTOM_LESSON };
PracticeMode currentPracticeMode = KOCH_TRAINING;

// Envelope settings
const float ATTACK_TIME = 5.0;
const float DECAY_TIME = 0.0;
const float SUSTAIN_LEVEL = 1.0;
const float RELEASE_TIME = 8.0;

// CW Decoder variables
unsigned long keyDownTime = 0;
unsigned long keyUpTime = 0;
unsigned long lastKeyChange = 0;
bool lastToneState = false;
String currentCharacter = "";
String decodedText = "";
unsigned long lastCharacterTime = 0;
unsigned long lastWordTime = 0;

// Timing
float ditLength = 100;
float dahThreshold = 200;
float charSpaceThreshold = 300;
float wordSpaceThreshold = 700;
const float TONE_THRESHOLD = 0.1;

// Statistics (stored in EEPROM)
struct TrainingStats {
  unsigned long totalDits;
  unsigned long totalDahs;
  unsigned long charactersDecoded;
  unsigned long sessionsCompleted;
  float bestWPM;
  float totalTrainingMinutes;
  int highestLesson;
  unsigned long characterErrors[26];  // A-Z error counts
  float lessonAccuracy[40];           // Accuracy for each Koch lesson
  unsigned long lastSessionTime;
  int customLessonsCompleted;
  float averageAccuracy;
};

TrainingStats stats;
unsigned long sessionStartTime;
float currentWPM = 0;

// Koch Method Variables
int kochLesson = 1;
String kochCharSet = "";
int kochSpeed = 20;
int kochEffectiveSpeed = 13;
String kochSentText = "";
String kochReceivedText = "";
int kochCharIndex = 0;
unsigned long kochSendTimer = 0;
bool kochSending = false;
bool kochListening = false;
int kochCorrect = 0;
int kochTotal = 0;
float kochAccuracy = 0.0;

// Callsign practice
String callsignPrefixes[] = { "K", "W", "N", "AA", "AB", "AC", "AD", "AE", "AF", "AG", "AH", "AI", "AJ", "AK", "AL", "VE", "VK", "G", "DL", "JA", "HL", "UA" };
String callsignSuffixes[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };

// QSO simulation data
String qsoExchanges[] = { "CQ CQ DE ", " K", " TU 73", "599 ", "5NN ", "QTH ", "NAME ", "AGE ", "PWR ", "ANT " };
String contestExchanges[] = { "001 ", "002 ", "CA", "NY", "TX", "FL", "OH", "IL", "PA", "MI" };

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

// Morse code lookup table
struct MorseChar {
  char character;
  String code;
};

MorseChar morseTable[] = {
  { 'A', ".-" }, { 'B', "-..." }, { 'C', "-.-." }, { 'D', "-.." }, { 'E', "." }, { 'F', "..-." }, { 'G', "--." }, { 'H', "...." }, { 'I', ".." }, { 'J', ".---" }, { 'K', "-.-" }, { 'L', ".-.." }, { 'M', "--" }, { 'N', "-." }, { 'O', "---" }, { 'P', ".--." }, { 'Q', "--.-" }, { 'R', ".-." }, { 'S', "..." }, { 'T', "-" }, { 'U', "..-" }, { 'V', "...-" }, { 'W', ".--" }, { 'X', "-..-" }, { 'Y', "-.--" }, { 'Z', "--.." }, { '1', ".----" }, { '2', "..---" }, { '3', "...--" }, { '4', "....-" }, { '5', "....." }, { '6', "-...." }, { '7', "--..." }, { '8', "---.." }, { '9', "----." }, { '0', "-----" }, { '/', "-..-." }, { '?', "..--.." }, { ',', "--..--" }, { '.', ".-.-.-" }, { '=', "-...-" }, { '+', ".-.-." }, { '-', "-....-" }, { '(', "-.--." }, { ')', "-.--.-" }, { '"', ".-..-." }, { ':', "---..." }, { ';', "-.-.-." }, { '@', ".--.-." }, { '!', "-.-.--" }
};
const int morseTableSize = sizeof(morseTable) / sizeof(MorseChar);

const char* waveformNames[] = { "Sine", "Square", "Sawtooth", "Triangle" };
const char* practiceModeNames[] = { "Koch", "Callsign", "QSO", "Contest", "Custom" };

// EEPROM addresses
const int STATS_ADDR = 0;
const int KOCH_LESSON_ADDR = sizeof(TrainingStats);
const int SETTINGS_ADDR = KOCH_LESSON_ADDR + sizeof(int);

void setup() {
  Serial.begin(115200);

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CW Ultimate Trainer");
  display.println("v5.0 Loading...");
  display.display();

  Serial.println("==============================================");
  Serial.println("CW ULTIMATE TRAINING MACHINE v5.0");
  Serial.println("==============================================");
  Serial.println("Features: Koch Method, QSO Sim, Callsigns,");
  Serial.println("Contest Mode, OLED Display, Statistics");
  Serial.println("Note: WiFi requires external ESP32/ESP8266");
  Serial.println("==============================================");

  // Pin setup
  setupPins();

  // Load saved data from EEPROM
  loadSettings();

  // Audio setup
  AudioMemory(35);  // Increased for all features
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.lineInLevel(5);  // Adjustable line input level

  // Configure envelope
  envelope1.attack(ATTACK_TIME);
  envelope1.decay(DECAY_TIME);
  envelope1.sustain(SUSTAIN_LEVEL);
  envelope1.release(RELEASE_TIME);

  // Configure mixers
  decodeMixer.gain(0, 1.0);  // Internal sidetone
  decodeMixer.gain(1, 0.0);  // External audio (off initially)

  // Initialize WiFi if available (requires external module)
  if (wifiEnabled) {
    // initializeWiFi();  // Uncomment if you add WiFi module
  } else {
    Serial.println("WiFi disabled - use serial or OLED interface");
  }

  // Initial setup
  updateWaveform();
  updateFrequency();
  updateVolume();
  updateOutputRouting();
  updateToneDetector();
  initializeKoch();

  sessionStartTime = millis();

  Serial.println("Ready! Use buttons or serial commands.");
  updateDisplay();
}

void loop() {
  // Handle web server (if WiFi enabled)
  if (wifiEnabled) {
    // server.handleClient();  // Uncomment if you add WiFi
  }

  // Update debouncers
  updateDebouncers();

  // Handle serial commands
  handleSerialCommands();

  // Handle button inputs
  handleButtons();

  // Handle CW key (manual keying)
  if (!kochSending) {
    handleManualKeying();
  }

  // Handle decoder
  if (decoderEnabled && !kochSending) {
    processCWDecoder();
  }

  // Handle various training modes
  handleTrainingModes();

  // Update controls and display
  updateControls();

  // Update display periodically
  if (millis() - lastDisplayUpdate > 100) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Auto-save statistics every 5 minutes
  static unsigned long lastSave = 0;
  if (millis() - lastSave > 300000) {
    saveSettings();
    lastSave = millis();
  }
}

void setupPins() {
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(WAVEFORM_BTN, INPUT_PULLUP);
  pinMode(OUTPUT_SELECT_BTN, INPUT_PULLUP);
  pinMode(DECODER_TOGGLE_BTN, INPUT_PULLUP);
  pinMode(KOCH_MODE_BTN, INPUT_PULLUP);
  pinMode(KOCH_NEXT_BTN, INPUT_PULLUP);
  pinMode(INPUT_SELECT_BTN, INPUT_PULLUP);
  pinMode(MENU_BTN, INPUT_PULLUP);
  pinMode(SELECT_BTN, INPUT_PULLUP);

  // Setup debouncers
  keyDebouncer.attach(KEY_PIN);
  keyDebouncer.interval(5);
  waveformButton.attach(WAVEFORM_BTN);
  waveformButton.interval(50);
  outputButton.attach(OUTPUT_SELECT_BTN);
  outputButton.interval(50);
  decoderButton.attach(DECODER_TOGGLE_BTN);
  decoderButton.interval(50);
  kochModeButton.attach(KOCH_MODE_BTN);
  kochModeButton.interval(50);
  kochNextButton.attach(KOCH_NEXT_BTN);
  kochNextButton.interval(50);
  inputSelectButton.attach(INPUT_SELECT_BTN);
  inputSelectButton.interval(50);
  menuButton.attach(MENU_BTN);
  menuButton.interval(50);
  selectButton.attach(SELECT_BTN);
  selectButton.interval(50);
}

void updateDebouncers() {
  keyDebouncer.update();
  waveformButton.update();
  outputButton.update();
  decoderButton.update();
  kochModeButton.update();
  kochNextButton.update();
  inputSelectButton.update();
  menuButton.update();
  selectButton.update();
}

void handleButtons() {
  // Menu navigation
  if (menuButton.fell()) {
    if (!inMenu) {
      inMenu = true;
      currentMenu = KOCH_MENU;
      menuSelection = 0;
    } else {
      // Cycle through menus
      currentMenu = (MenuMode)((currentMenu + 1) % 6);
      menuSelection = 0;
    }
    updateDisplay();
  }

  if (selectButton.fell() && inMenu) {
    handleMenuSelection();
  }

  // Direct controls (when not in menu)
  if (!inMenu) {
    if (waveformButton.fell()) {
      currentWaveform = (currentWaveform + 1) % 4;
      updateWaveform();
    }

    if (outputButton.fell()) {
      useHeadphones = !useHeadphones;
      updateOutputRouting();
    }

    if (decoderButton.fell()) {
      decoderEnabled = !decoderEnabled;
      if (decoderEnabled) {
        currentCharacter = "";
        lastCharacterTime = millis();
        lastWordTime = millis();
      }
    }

    if (inputSelectButton.fell()) {
      useExternalAudio = !useExternalAudio;
      decodeMixer.gain(0, useExternalAudio ? 0.0 : 1.0);  // Internal sidetone
      decodeMixer.gain(1, useExternalAudio ? 1.0 : 0.0);  // External audio
    }

    if (kochModeButton.fell()) {
      kochModeEnabled = !kochModeEnabled;
      if (kochModeEnabled) {
        currentPracticeMode = KOCH_TRAINING;
        startKochLesson();
      } else {
        stopKochLesson();
      }
    }

    if (kochNextButton.fell()) {
      handlePracticeModeButton();
    }
  }
}

void handleMenuSelection() {
  switch (currentMenu) {
    case KOCH_MENU:
      if (menuSelection == 0) {  // Start Koch lesson
        kochModeEnabled = true;
        currentPracticeMode = KOCH_TRAINING;
        startKochLesson();
        inMenu = false;
      } else if (menuSelection == 1) {  // Set lesson
        // Implement lesson selection
        inMenu = false;
      }
      break;

    case PRACTICE_MENU:
      currentPracticeMode = (PracticeMode)menuSelection;
      startPracticeMode();
      inMenu = false;
      break;

    case SETTINGS_MENU:
      // Handle settings changes
      inMenu = false;
      break;

    case STATS_MENU:
      // Display detailed stats
      displayDetailedStats();
      break;

    case QSO_MENU:
      startQSOSimulation();
      inMenu = false;
      break;

    default:
      inMenu = false;
      break;
  }
}

void handlePracticeModeButton() {
  switch (currentPracticeMode) {
    case KOCH_TRAINING:
      if (kochSending || kochListening) {
        startKochLesson();  // Repeat
      } else {
        evaluateKochSession();
      }
      break;

    case CALLSIGN_PRACTICE:
      generateCallsignLesson();
      break;

    case QSO_SIMULATION:
      continueQSOSimulation();
      break;

    case CONTEST_MODE:
      generateContestExchange();
      break;

    case CUSTOM_LESSON:
      generateCustomLesson();
      break;
  }
}

void startPracticeMode() {
  switch (currentPracticeMode) {
    case KOCH_TRAINING:
      kochModeEnabled = true;
      startKochLesson();
      break;

    case CALLSIGN_PRACTICE:
      generateCallsignLesson();
      break;

    case QSO_SIMULATION:
      startQSOSimulation();
      break;

    case CONTEST_MODE:
      generateContestExchange();
      break;

    case CUSTOM_LESSON:
      generateCustomLesson();
      break;
  }
}

void generateCallsignLesson() {
  String lesson = "";
  for (int i = 0; i < 10; i++) {  // 10 callsigns
    String callsign = generateRandomCallsign();
    lesson += callsign + " ";
  }

  kochSentText = lesson;
  kochReceivedText = "";
  kochCharIndex = 0;
  kochSendTimer = millis();
  kochSending = true;
  kochListening = false;

  Serial.println("Callsign Practice:");
  Serial.println(lesson);
  updateDisplay();
}

String generateRandomCallsign() {
  String callsign = "";

  // Choose prefix
  int prefixIndex = random(sizeof(callsignPrefixes) / sizeof(callsignPrefixes[0]));
  callsign += callsignPrefixes[prefixIndex];

  // Add number
  callsign += String(random(0, 10));

  // Add suffix (1-3 letters)
  int suffixLength = random(1, 4);
  for (int i = 0; i < suffixLength; i++) {
    int suffixIndex = random(sizeof(callsignSuffixes) / sizeof(callsignSuffixes[0]));
    callsign += callsignSuffixes[suffixIndex];
  }

  return callsign;
}

void startQSOSimulation() {
  // Generate realistic QSO
  String qso = "CQ CQ DE " + generateRandomCallsign() + " " + generateRandomCallsign() + " K ";
  qso += generateRandomCallsign() + " DE " + generateRandomCallsign() + " ";
  qso += "599 " + generateRandomCallsign() + " TU 73 ";

  kochSentText = qso;
  kochReceivedText = "";
  kochCharIndex = 0;
  kochSendTimer = millis();
  kochSending = true;
  kochListening = false;

  Serial.println("QSO Simulation:");
  Serial.println(qso);
  updateDisplay();
}

void continueQSOSimulation() {
  // Continue with next part of QSO
  startQSOSimulation();  // For now, just start a new one
}

void generateContestExchange() {
  String exchange = "";

  // Generate contest number
  static int contestNumber = 1;
  if (contestNumber < 10) exchange += "00";
  else if (contestNumber < 100) exchange += "0";
  exchange += String(contestNumber++) + " ";

  // Add state/province
  int stateIndex = random(sizeof(contestExchanges) / sizeof(contestExchanges[0]));
  exchange += contestExchanges[stateIndex] + " ";

  // Repeat for multiple exchanges
  String lesson = "";
  for (int i = 0; i < 5; i++) {
    lesson += generateRandomCallsign() + " " + exchange;
  }

  kochSentText = lesson;
  kochReceivedText = "";
  kochCharIndex = 0;
  kochSendTimer = millis();
  kochSending = true;
  kochListening = false;

  Serial.println("Contest Practice:");
  Serial.println(lesson);
  updateDisplay();
}

void generateCustomLesson() {
  // Generate lesson based on user's weak characters
  String weakChars = findWeakCharacters();
  if (weakChars.length() == 0) weakChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  String lesson = "";
  for (int i = 0; i < 50; i++) {
    if (i % 6 == 5) {
      lesson += " ";
    } else {
      lesson += weakChars.charAt(random(weakChars.length()));
    }
  }

  kochSentText = lesson;
  kochReceivedText = "";
  kochCharIndex = 0;
  kochSendTimer = millis();
  kochSending = true;
  kochListening = false;

  Serial.println("Custom Lesson (Weak Characters):");
  Serial.println(lesson);
  updateDisplay();
}

String findWeakCharacters() {
  String weakChars = "";
  float threshold = stats.averageAccuracy * 0.8;  // Characters below 80% of average

  for (int i = 0; i < 26; i++) {
    if (stats.characterErrors[i] > threshold) {
      weakChars += char('A' + i);
    }
  }

  return weakChars;
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();

    if (command.startsWith("SPEED ")) {
      int speed = command.substring(6).toInt();
      if (speed >= 5 && speed <= 50) {
        kochSpeed = speed;
        calculateKochTiming();
        Serial.println("Speed set to " + String(speed) + " WPM");
      }
    } else if (command.startsWith("FARNSWORTH ")) {
      int farnsworth = command.substring(11).toInt();
      if (farnsworth >= 5 && farnsworth <= kochSpeed) {
        kochEffectiveSpeed = farnsworth;
        calculateKochTiming();
        Serial.println("Farnsworth speed set to " + String(farnsworth) + " WPM");
      }
    } else if (command.startsWith("LESSON ")) {
      int lesson = command.substring(7).toInt();
      if (lesson >= 1 && lesson <= 40) {
        kochLesson = lesson;
        initializeKoch();
        Serial.println("Jumped to lesson " + String(lesson));
      }
    } else if (command == "STATS") {
      displayDetailedStats();
    } else if (command == "RESET") {
      resetAllStats();
      Serial.println("All statistics reset");
    } else if (command.startsWith("FREQ ")) {
      int freq = command.substring(5).toInt();
      if (freq >= 300 && freq <= 1200) {
        sidetoneFreq = freq;
        updateFrequency();
        updateToneDetector();
        Serial.println("Frequency set to " + String(freq) + " Hz");
      }
    } else if (command == "HELP") {
      printHelp();
    }
  }
}

void printHelp() {
  Serial.println("\n=== COMMAND REFERENCE ===");
  Serial.println("SPEED [5-50]     - Set character speed");
  Serial.println("FARNSWORTH [5-50] - Set effective speed");
  Serial.println("LESSON [1-40]    - Jump to Koch lesson");
  Serial.println("FREQ [300-1200]  - Set sidetone frequency");
  Serial.println("STATS            - Show detailed statistics");
  Serial.println("RESET            - Reset all statistics");
  Serial.println("HELP             - Show this help");
  Serial.println("========================\n");
}

void handleManualKeying() {
  bool currentKeyState = !keyDebouncer.read();
  if (currentKeyState != keyPressed) {
    keyPressed = currentKeyState;
    if (keyPressed) {
      envelope1.noteOn();
      keyDownTime = millis();
    } else {
      envelope1.noteOff();
      keyUpTime = millis();
    }
    lastKeyChange = millis();
  }
}

void handleTrainingModes() {
  if (kochModeEnabled) {
    processKochSending();
  }
}

// [Previous Koch method functions remain the same]
void initializeKoch() {
  kochCharSet = kochLessons[kochLesson - 1];
  calculateKochTiming();
}

void startKochLesson() {
  kochSentText = generateKochText(50);
  kochReceivedText = "";
  kochCharIndex = 0;
  kochSendTimer = millis();
  kochSending = true;
  kochListening = false;
  kochCorrect = 0;
  kochTotal = 0;

  Serial.println("\n=== KOCH LESSON " + String(kochLesson) + " ===");
  Serial.println("Characters: " + kochCharSet);
  Serial.println("Speed: " + String(kochSpeed) + "/" + String(kochEffectiveSpeed) + " WPM");
  Serial.println("Text: " + kochSentText);
  Serial.println("Sending...");

  decodedText = "";
  updateDisplay();
}

void stopKochLesson() {
  kochSending = false;
  kochListening = false;
  envelope1.noteOff();
  Serial.println("Training stopped.");
  updateDisplay();
}

String generateKochText(int length) {
  String text = "";
  for (int i = 0; i < length; i++) {
    if (i % 6 == 5) {
      text += " ";
    } else {
      int charIndex = random(kochCharSet.length());
      text += kochCharSet.charAt(charIndex);
    }
  }
  return text;
}

void processKochSending() {
  if (!kochSending || kochCharIndex >= kochSentText.length()) {
    if (kochSending) {
      kochSending = false;
      kochListening = true;
      envelope1.noteOff();
      Serial.println("\nSending complete. Copy received:");
      updateDisplay();
    }
    return;
  }

  unsigned long currentTime = millis();
  char currentChar = kochSentText.charAt(kochCharIndex);

  if (currentChar == ' ') {
    if (currentTime - kochSendTimer >= wordSpaceThreshold) {
      kochCharIndex++;
      kochSendTimer = currentTime;
    }
  } else {
    String morseCode = getMorseCode(currentChar);
    if (sendMorseCharacter(morseCode, currentTime)) {
      kochCharIndex++;
      kochSendTimer = currentTime + charSpaceThreshold;
    }
  }
}

bool sendMorseCharacter(String morseCode, unsigned long currentTime) {
  static int elementIndex = 0;
  static unsigned long elementTimer = 0;
  static bool elementState = false;

  if (kochCharIndex == 0 || kochSendTimer <= currentTime) {
    elementIndex = 0;
    elementTimer = currentTime;
    elementState = true;
    envelope1.noteOn();
    return false;
  }

  if (elementIndex >= morseCode.length()) {
    envelope1.noteOff();
    elementIndex = 0;
    return true;
  }

  char element = morseCode.charAt(elementIndex);
  unsigned long elementDuration = (element == '.') ? ditLength : (ditLength * 3);

  if (elementState) {
    if (currentTime - elementTimer >= elementDuration) {
      envelope1.noteOff();
      elementState = false;
      elementTimer = currentTime;
    }
  } else {
    if (currentTime - elementTimer >= ditLength) {
      elementIndex++;
      if (elementIndex < morseCode.length()) {
        envelope1.noteOn();
        elementState = true;
        elementTimer = currentTime;
      }
    }
  }

  return false;
}

void evaluateKochSession() {
  if (kochTotal == 0) {
    Serial.println("No characters to evaluate.");
    return;
  }

  kochAccuracy = (float)kochCorrect / kochTotal * 100.0;
  stats.lessonAccuracy[kochLesson - 1] = kochAccuracy;

  Serial.println("\n=== LESSON RESULTS ===");
  Serial.println("Accuracy: " + String(kochAccuracy, 1) + "% (" + String(kochCorrect) + "/" + String(kochTotal) + ")");

  if (kochAccuracy >= 90.0) {
    kochLesson++;
    if (kochLesson > 40) kochLesson = 40;
    kochCharSet = kochLessons[kochLesson - 1];
    Serial.println("Excellent! Advancing to lesson " + String(kochLesson));

    if (kochLesson > stats.highestLesson) {
      stats.highestLesson = kochLesson;
    }
  } else {
    Serial.println("Practice more with lesson " + String(kochLesson));
  }

  stats.sessionsCompleted++;
  saveSettings();
  calculateKochTiming();
  updateDisplay();
}

void calculateKochTiming() {
  ditLength = 1200.0 / kochSpeed;
  charSpaceThreshold = ditLength * 3.0;
  wordSpaceThreshold = (1200.0 / kochEffectiveSpeed) * 7.0;
}

String getMorseCode(char c) {
  for (int i = 0; i < morseTableSize; i++) {
    if (morseTable[i].character == c) {
      return morseTable[i].code;
    }
  }
  return "";
}

void processCWDecoder() {
  bool toneDetected = toneDetect1.available() && toneDetect1.read() > TONE_THRESHOLD;
  unsigned long currentTime = millis();

  if (toneDetected != lastToneState) {
    if (toneDetected) {
      if (lastToneState == false && currentTime - lastKeyChange > 50) {
        unsigned long spaceLength = currentTime - lastKeyChange;
        processSpace(spaceLength);
      }
    } else {
      if (lastToneState == true && currentTime - lastKeyChange > 30) {
        unsigned long toneLength = currentTime - lastKeyChange;
        processElement(toneLength);
      }
    }
    lastKeyChange = currentTime;
    lastToneState = toneDetected;
  }

  if (currentCharacter.length() > 0 && currentTime - lastCharacterTime > charSpaceThreshold) {
    processCharacter();
  }

  if (currentTime - lastWordTime > wordSpaceThreshold && decodedText.length() > 0 && !decodedText.endsWith(" ")) {
    decodedText += " ";
    if (!kochModeEnabled) Serial.print(" ");
  }
}

void processElement(unsigned long duration) {
  if (stats.totalDits + stats.totalDahs < 10) {
    if (duration < 150) {
      currentCharacter += ".";
      stats.totalDits++;
    } else {
      currentCharacter += "-";
      stats.totalDahs++;
    }
  } else {
    float avgDitLength = ditLength;
    dahThreshold = avgDitLength * 2.5;

    if (duration < dahThreshold) {
      currentCharacter += ".";
      stats.totalDits++;
      ditLength = (ditLength * 0.9) + (duration * 0.1);
    } else {
      currentCharacter += "-";
      stats.totalDahs++;
    }
  }

  lastCharacterTime = millis();
  charSpaceThreshold = ditLength * 3.0;
  wordSpaceThreshold = ditLength * 7.0;
}

void processSpace(unsigned long duration) {
  if (duration > wordSpaceThreshold && currentCharacter.length() == 0) {
    if (!decodedText.endsWith(" ")) {
      decodedText += " ";
      if (!kochModeEnabled) Serial.print(" ");
    }
    lastWordTime = millis();
  }
}

void processCharacter() {
  char decodedChar = lookupMorseCharacter(currentCharacter);

  if (kochListening && decodedChar != '?') {
    kochReceivedText += decodedChar;
    kochTotal++;

    if (kochTotal <= kochSentText.length()) {
      char expectedChar = kochSentText.charAt(kochTotal - 1);
      if (decodedChar == expectedChar) {
        kochCorrect++;
        Serial.print(decodedChar);
      } else {
        Serial.print("[" + String(decodedChar) + "]");
        // Track character errors
        if (expectedChar >= 'A' && expectedChar <= 'Z') {
          stats.characterErrors[expectedChar - 'A']++;
        }
      }
    }
  } else if (!kochModeEnabled) {
    if (decodedChar != '?') {
      decodedText += decodedChar;
      Serial.print(decodedChar);
      stats.charactersDecoded++;

      if (stats.charactersDecoded > 0) {
        float timeMinutes = (millis() - sessionStartTime) / 60000.0;
        currentWPM = (stats.charactersDecoded * 2.4) / timeMinutes;
        if (currentWPM > stats.bestWPM) {
          stats.bestWPM = currentWPM;
        }
      }

      if (stats.charactersDecoded % 10 == 0) {
        Serial.print(" |Stats: ");
        Serial.print(stats.totalDits);
        Serial.print("/");
        Serial.print(stats.totalDahs);
        Serial.print("/");
        Serial.print(stats.charactersDecoded);
        Serial.print("/");
        Serial.print(currentWPM, 1);
        Serial.println("wpm|");
      }
    } else if (currentCharacter.length() > 0) {
      Serial.print("?");
    }
  }

  currentCharacter = "";
  lastWordTime = millis();
}

char lookupMorseCharacter(String morseCode) {
  for (int i = 0; i < morseTableSize; i++) {
    if (morseTable[i].code == morseCode) {
      return morseTable[i].character;
    }
  }
  return '?';
}

void updateControls() {
  if (millis() - lastFreqUpdate > 100) {
    float newFreq = mapFloat(analogRead(FREQ_POT), 0, 1023, 300.0, 1200.0);
    if (abs(newFreq - sidetoneFreq) > 5.0) {
      sidetoneFreq = newFreq;
      updateFrequency();
      updateToneDetector();
      lastFreqUpdate = millis();
    }
  }

  if (millis() - lastVolumeUpdate > 100) {
    float newVolume = mapFloat(analogRead(VOLUME_POT), 0, 1023, 0.0, 1.0);
    if (abs(newVolume - volume) > 0.05) {
      volume = newVolume;
      updateVolume();
      lastVolumeUpdate = millis();
    }
  }

  // Speed control from potentiometer
  int newSpeed = map(analogRead(SPEED_POT), 0, 1023, 5, 50);
  if (abs(newSpeed - kochSpeed) > 1) {
    kochSpeed = newSpeed;
    kochEffectiveSpeed = max(5, kochSpeed * 0.6);  // Auto-adjust Farnsworth
    calculateKochTiming();
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (inMenu) {
    displayMenu();
  } else {
    displayMainScreen();
  }

  display.display();
}

void displayMenu() {
  switch (currentMenu) {
    case KOCH_MENU:
      display.println("KOCH TRAINING");
      display.println("- Start Lesson");
      display.println("- Set Lesson #");
      display.println("- Speed Control");
      break;

    case PRACTICE_MENU:
      display.println("PRACTICE MODES");
      display.println("- Koch Method");
      display.println("- Callsigns");
      display.println("- QSO Simulation");
      display.println("- Contest Mode");
      display.println("- Custom Lesson");
      break;

    case SETTINGS_MENU:
      display.println("SETTINGS");
      display.printf("Freq: %.0f Hz\n", sidetoneFreq);
      display.printf("Vol: %.0f%%\n", volume * 100);
      display.println("Waveform: " + String(waveformNames[currentWaveform]));
      display.println("Output: " + String(useHeadphones ? "HP" : "SPK"));
      break;

    case STATS_MENU:
      display.println("STATISTICS");
      display.printf("Sessions: %lu\n", stats.sessionsCompleted);
      display.printf("Best WPM: %.1f\n", stats.bestWPM);
      display.printf("Chars: %lu\n", stats.charactersDecoded);
      display.printf("Accuracy: %.1f%%\n", stats.averageAccuracy);
      break;

    case QSO_MENU:
      display.println("QSO SIMULATOR");
      display.println("- Start QSO");
      display.println("- Contest Mode");
      display.println("- Ragchew");
      break;

    default:
      display.println("MAIN MENU");
      break;
  }
}

void displayMainScreen() {
  // Line 1: Mode and lesson
  if (kochModeEnabled) {
    display.printf("Koch L%d: %s\n", kochLesson, kochCharSet.c_str());
  } else {
    display.printf("Mode: %s\n", practiceModeNames[currentPracticeMode]);
  }

  // Line 2: Frequency and waveform
  display.printf("%.0fHz %s\n", sidetoneFreq, waveformNames[currentWaveform]);

  // Line 3: Speed and volume
  display.printf("Spd:%d/%d Vol:%.0f%%\n", kochSpeed, kochEffectiveSpeed, volume * 100);

  // Line 4: Status indicators
  String status = "";
  status += decoderEnabled ? "DEC " : "";
  status += useHeadphones ? "HP " : "SPK ";
  status += useExternalAudio ? "EXT" : "INT";
  display.println(status);

  // Line 5-6: Current activity
  if (kochSending) {
    display.println("Sending...");
    display.printf("Char %d/%d\n", kochCharIndex, kochSentText.length());
  } else if (kochListening) {
    display.println("Listening...");
    display.printf("Acc: %.1f%%\n", kochAccuracy);
  } else if (stats.charactersDecoded > 0) {
    display.printf("WPM: %.1f\n", currentWPM);
    display.printf("Total: %lu chars\n", stats.charactersDecoded);
  }

  // Line 7-8: Recent decoded text (last 21 chars)
  String recentText = decodedText;
  if (recentText.length() > 21) {
    recentText = recentText.substring(recentText.length() - 21);
  }
  display.println(recentText);
}

void displayDetailedStats() {
  Serial.println("\n=== DETAILED STATISTICS ===");
  Serial.printf("Total Sessions: %lu\n", stats.sessionsCompleted);
  Serial.printf("Characters Decoded: %lu\n", stats.charactersDecoded);
  Serial.printf("Best WPM: %.1f\n", stats.bestWPM);
  Serial.printf("Training Time: %.1f hours\n", stats.totalTrainingMinutes / 60.0);
  Serial.printf("Highest Koch Lesson: %d\n", stats.highestLesson);
  Serial.printf("Average Accuracy: %.1f%%\n", stats.averageAccuracy);

  Serial.println("\nCharacter Error Counts:");
  for (int i = 0; i < 26; i++) {
    if (stats.characterErrors[i] > 0) {
      Serial.printf("%c: %lu errors\n", 'A' + i, stats.characterErrors[i]);
    }
  }

  Serial.println("\nKoch Lesson Accuracy:");
  for (int i = 0; i < min(40, stats.highestLesson); i++) {
    if (stats.lessonAccuracy[i] > 0) {
      Serial.printf("Lesson %d: %.1f%%\n", i + 1, stats.lessonAccuracy[i]);
    }
  }
  Serial.println("========================\n");
}

void resetAllStats() {
  memset(&stats, 0, sizeof(stats));
  stats.bestWPM = 0;
  stats.averageAccuracy = 0;
  kochLesson = 1;
  initializeKoch();
  saveSettings();
}

void loadSettings() {
  EEPROM.get(STATS_ADDR, stats);
  EEPROM.get(KOCH_LESSON_ADDR, kochLesson);

  // Validate loaded data
  if (kochLesson < 1 || kochLesson > 40) {
    kochLesson = 1;
  }

  // Calculate average accuracy
  float totalAccuracy = 0;
  int validLessons = 0;
  for (int i = 0; i < 40; i++) {
    if (stats.lessonAccuracy[i] > 0) {
      totalAccuracy += stats.lessonAccuracy[i];
      validLessons++;
    }
  }
  if (validLessons > 0) {
    stats.averageAccuracy = totalAccuracy / validLessons;
  }
}

void saveSettings() {
  stats.totalTrainingMinutes += (millis() - sessionStartTime) / 60000.0;
  stats.lastSessionTime = millis();

  EEPROM.put(STATS_ADDR, stats);
  EEPROM.put(KOCH_LESSON_ADDR, kochLesson);

  sessionStartTime = millis();  // Reset session timer
}

/*void initializeWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    setupWebServer();
  } else {
    Serial.println("\nWiFi connection failed. Continuing without web interface.");
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/stats", handleStatsPage);
  server.on("/control", handleControlPage);
  server.on("/api/stats", handleAPIStats);
  server.on("/api/control", HTTP_POST, handleAPIControl);
  server.on("/api/lesson", HTTP_POST, handleAPILesson);

  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>CW Trainer</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#1a1a1a;color:#fff;}";
  html += ".container{max-width:800px;margin:0 auto;}.card{background:#333;padding:20px;margin:10px 0;border-radius:8px;}";
  html += ".button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin:5px;}";
  html += ".button:hover{background:#0056b3;}.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px;}";
  html += "</style></head><body><div class='container'>";
  html += "<h1>CW Ultimate Trainer</h1>";

  html += "<div class='card'><h2>Current Status</h2>";
  html += "<p>Koch Lesson: " + String(kochLesson) + "/40</p>";
  html += "<p>Characters: " + kochCharSet + "</p>";
  html += "<p>Speed: " + String(kochSpeed) + "/" + String(kochEffectiveSpeed) + " WPM</p>";
  html += "<p>Frequency: " + String(sidetoneFreq, 0) + " Hz</p>";
  html += "<p>Decoder: " + String(decoderEnabled ? "ON" : "OFF") + "</p>";
  html += "</div>";

  html += "<div class='card'><h2>Quick Controls</h2>";
  html += "<button class='button' onclick='startLesson()'>Start Koch Lesson</button>";
  html += "<button class='button' onclick='generateCallsigns()'>Callsign Practice</button>";
  html += "<button class='button' onclick='startQSO()'>QSO Simulation</button>";
  html += "<button class='button' onclick='toggleDecoder()'>Toggle Decoder</button>";
  html += "</div>";

  html += "<div class='card'><h2>Statistics</h2>";
  html += "<div class='stats'>";
  html += "<div>Sessions: " + String(stats.sessionsCompleted) + "</div>";
  html += "<div>Best WPM: " + String(stats.bestWPM, 1) + "</div>";
  html += "<div>Characters: " + String(stats.charactersDecoded) + "</div>";
  html += "<div>Accuracy: " + String(stats.averageAccuracy, 1) + "%</div>";
  html += "</div></div>";

  html += "</div>";
  html += "<script>";
  html += "function startLesson(){fetch('/api/lesson',{method:'POST',body:'start'});}";
  html += "function generateCallsigns(){fetch('/api/lesson',{method:'POST',body:'callsign'});}";
  html += "function startQSO(){fetch('/api/lesson',{method:'POST',body:'qso'});}";
  html += "function toggleDecoder(){fetch('/api/control',{method:'POST',body:'decoder'});}";
  html += "setInterval(()=>location.reload(),30000);";  // Auto-refresh every 30 seconds
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleStatsPage() {
  String json = "{";
  json += "\"sessions\":" + String(stats.sessionsCompleted) + ",";
  json += "\"characters\":" + String(stats.charactersDecoded) + ",";
  json += "\"bestWPM\":" + String(stats.bestWPM) + ",";
  json += "\"accuracy\":" + String(stats.averageAccuracy) + ",";
  json += "\"lesson\":" + String(kochLesson) + ",";
  json += "\"trainingHours\":" + String(stats.totalTrainingMinutes / 60.0);
  json += "}";

  server.send(200, "application/json", json);
}

void handleControlPage() {
  server.send(200, "text/plain", "Control API endpoint");
}

void handleAPIStats() {
  handleStatsPage();
}

void handleAPIControl() {
  String command = server.arg("plain");

  if (command == "decoder") {
    decoderEnabled = !decoderEnabled;
    server.send(200, "text/plain", "Decoder " + String(decoderEnabled ? "enabled" : "disabled"));
  } else {
    server.send(400, "text/plain", "Unknown command");
  }
}

void handleAPILesson() {
  String lessonType = server.arg("plain");

  if (lessonType == "start") {
    kochModeEnabled = true;
    currentPracticeMode = KOCH_TRAINING;
    startKochLesson();
    server.send(200, "text/plain", "Koch lesson started");
  } else if (lessonType == "callsign") {
    currentPracticeMode = CALLSIGN_PRACTICE;
    generateCallsignLesson();
    server.send(200, "text/plain", "Callsign practice started");
  } else if (lessonType == "qso") {
    currentPracticeMode = QSO_SIMULATION;
    startQSOSimulation();
    server.send(200, "text/plain", "QSO simulation started");
  } else {
    server.send(400, "text/plain", "Unknown lesson type");
  }
}*/

void updateWaveform() {
  mixer1.gain(0, 0);
  mixer1.gain(1, 0);

  switch (currentWaveform) {
    case 0: mixer1.gain(0, 1.0); break;
    case 1:
      waveform1.begin(WAVEFORM_SQUARE);
      mixer1.gain(1, 0.3);
      break;
    case 2:
      waveform1.begin(WAVEFORM_SAWTOOTH);
      mixer1.gain(1, 0.5);
      break;
    case 3:
      waveform1.begin(WAVEFORM_TRIANGLE);
      mixer1.gain(1, 0.7);
      break;
  }
  updateFrequency();
}

void updateFrequency() {
  sine1.frequency(sidetoneFreq);
  waveform1.frequency(sidetoneFreq);
}

void updateToneDetector() {
  toneDetect1.frequency(sidetoneFreq, 30);
}

void updateVolume() {
  envelope1.releaseNoteOn(volume);
}

void updateOutputRouting() {
  if (useHeadphones) {
    sgtl5000_1.volume(0.0);
  } else {
    sgtl5000_1.volume(0.8);
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}