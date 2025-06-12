/*
 * MAX98357A I2S Amplifier Test Script for Teensy 4.1
 * 
 * This script tests the MAX98357A breakout board and speaker
 * with various audio patterns to verify proper operation.
 * 
 * Wiring:
 * MAX98357A → Teensy 4.1
 * BCLK     → Pin 21 (I2S_BCLK)
 * WCLK     → Pin 20 (I2S_LRCLK) 
 * DIN      → Pin 7  (I2S_OUT1A)
 * GND      → GND
 * VIN      → 3.3V
 * Speaker+ → OUT+
 * Speaker- → OUT-
 * 
 * Optional: Connect GAIN pin to control volume
 * GAIN floating = 9dB gain
 * GAIN to GND = 6dB gain  
 * GAIN to VDD = 12dB gain
 * GAIN to 100k→GND = 15dB gain
 * GAIN to 100k→VDD = 18dB gain
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Audio objects for testing
AudioSynthWaveformSine sine1;   // Sine wave generator
AudioSynthWaveform waveform1;   // Multi-waveform generator
AudioSynthToneSweep sweep1;     // Frequency sweep
AudioSynthNoiseWhite noise1;    // White noise generator
AudioSynthWaveformDc dc1;       // DC offset for testing
AudioMixer4 mixer1;             // Mix multiple sources
AudioEffectEnvelope envelope1;  // Volume envelope
AudioOutputI2S i2s1;            // I2S output to MAX98357A

// Audio connections
AudioConnection patchCord1(sine1, 0, mixer1, 0);
AudioConnection patchCord2(waveform1, 0, mixer1, 1);
AudioConnection patchCord3(sweep1, 0, mixer1, 2);
AudioConnection patchCord4(noise1, 0, mixer1, 3);
AudioConnection patchCord5(mixer1, envelope1);
AudioConnection patchCord6(envelope1, 0, i2s1, 0);  // Left channel
AudioConnection patchCord7(envelope1, 0, i2s1, 1);  // Right channel (mono output)

AudioControlSGTL5000 sgtl5000_1;  // Audio shield control (if present)

// Test parameters
int currentTest = 0;
unsigned long testStartTime = 0;
const unsigned long testDuration = 5000;  // 5 seconds per test
bool testRunning = false;

// Test frequencies
float testFrequencies[] = { 100, 250, 440, 600, 800, 1000, 1500, 2000, 3000 };
int numFrequencies = sizeof(testFrequencies) / sizeof(testFrequencies[0]);
int currentFrequency = 0;

void setup() {
  Serial.begin(115200);

  // Wait for serial connection
  while (!Serial && millis() < 3000) {
    delay(100);
  }

  Serial.println("=======================================");
  Serial.println("MAX98357A I2S Amplifier Test Script");
  Serial.println("Teensy 4.1 + Audio Library");
  Serial.println("=======================================");
  Serial.println();

  // Initialize audio
  AudioMemory(20);

  // Try to initialize audio shield if present (optional)
  if (sgtl5000_1.enable()) {
    Serial.println("Audio Shield detected - using for monitoring");
    sgtl5000_1.volume(0.3);  // Low volume for monitoring
  } else {
    Serial.println("No audio shield detected - using I2S only");
  }

  // Configure mixer gains (all off initially)
  mixer1.gain(0, 0);  // Sine wave
  mixer1.gain(1, 0);  // Waveform
  mixer1.gain(2, 0);  // Sweep
  mixer1.gain(3, 0);  // Noise

  // Configure envelope for smooth transitions
  envelope1.attack(50);    // 50ms attack
  envelope1.decay(0);      // No decay
  envelope1.sustain(1.0);  // Full sustain
  envelope1.release(50);   // 50ms release

  Serial.println("Audio system initialized.");
  Serial.println();
  printMenu();
}

void loop() {
  handleSerialInput();

  if (testRunning) {
    updateAutomaticTest();
  }

  // Print audio statistics every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    printAudioStats();
    lastStats = millis();
  }
}

void printMenu() {
  Serial.println("=== MAX98357A TEST MENU ===");
  Serial.println("1 - Test 440Hz sine wave (5 seconds)");
  Serial.println("2 - Test multiple frequencies");
  Serial.println("3 - Test different waveforms");
  Serial.println("4 - Test frequency sweep (100Hz to 3kHz)");
  Serial.println("5 - Test white noise");
  Serial.println("6 - Test volume levels");
  Serial.println("7 - Test channel balance");
  Serial.println("8 - Run full automatic test suite");
  Serial.println("9 - Stop current test");
  Serial.println("0 - Show this menu");
  Serial.println();
  Serial.println("Enter test number:");
}

void handleSerialInput() {
  if (Serial.available()) {
    char input = Serial.read();

    switch (input) {
      case '1':
        test440Hz();
        break;
      case '2':
        testMultipleFrequencies();
        break;
      case '3':
        testDifferentWaveforms();
        break;
      case '4':
        testFrequencySweep();
        break;
      case '5':
        testWhiteNoise();
        break;
      case '6':
        testVolumeLevels();
        break;
      case '7':
        testChannelBalance();
        break;
      case '8':
        runFullTestSuite();
        break;
      case '9':
        stopCurrentTest();
        break;
      case '0':
        printMenu();
        break;
      default:
        Serial.println("Invalid option. Press '0' for menu.");
        break;
    }
  }
}

void test440Hz() {
  Serial.println("Test 1: 440Hz sine wave for 5 seconds");
  Serial.println("Listen for clear, steady tone...");

  stopAllAudio();
  sine1.frequency(440);
  sine1.amplitude(0.5);
  mixer1.gain(0, 1.0);
  envelope1.noteOn();

  delay(5000);

  envelope1.noteOff();
  delay(100);
  stopAllAudio();

  Serial.println("Test 1 complete.");
  Serial.println();
}

void testMultipleFrequencies() {
  Serial.println("Test 2: Multiple frequency test");
  Serial.println("Testing frequencies: 100, 250, 440, 600, 800, 1000, 1500, 2000, 3000 Hz");

  for (int i = 0; i < numFrequencies; i++) {
    Serial.print("Playing ");
    Serial.print(testFrequencies[i], 0);
    Serial.println(" Hz...");

    stopAllAudio();
    sine1.frequency(testFrequencies[i]);
    sine1.amplitude(0.4);
    mixer1.gain(0, 1.0);
    envelope1.noteOn();

    delay(2000);  // 2 seconds per frequency

    envelope1.noteOff();
    delay(500);  // 0.5 second gap
  }

  stopAllAudio();
  Serial.println("Test 2 complete.");
  Serial.println();
}

void testDifferentWaveforms() {
  Serial.println("Test 3: Different waveforms at 600Hz");

  const char* waveNames[] = { "Sine", "Square", "Sawtooth", "Triangle" };
  int waveTypes[] = { WAVEFORM_SINE, WAVEFORM_SQUARE, WAVEFORM_SAWTOOTH, WAVEFORM_TRIANGLE };

  for (int i = 0; i < 4; i++) {
    Serial.print("Playing ");
    Serial.print(waveNames[i]);
    Serial.println(" wave...");

    stopAllAudio();

    if (i == 0) {
      // Use sine generator for pure sine
      sine1.frequency(600);
      sine1.amplitude(0.4);
      mixer1.gain(0, 1.0);
    } else {
      // Use waveform generator for others
      waveform1.begin(waveTypes[i]);
      waveform1.frequency(600);
      waveform1.amplitude(0.3);  // Lower amplitude for square waves
      mixer1.gain(1, 1.0);
    }

    envelope1.noteOn();
    delay(3000);  // 3 seconds per waveform
    envelope1.noteOff();
    delay(500);
  }

  stopAllAudio();
  Serial.println("Test 3 complete.");
  Serial.println();
}

void testFrequencySweep() {
  Serial.println("Test 4: Frequency sweep 100Hz to 3000Hz over 10 seconds");
  Serial.println("Listen for smooth frequency change...");

  stopAllAudio();
  sweep1.play(0.3, 100, 3000, 10.0);  // amplitude, start_freq, end_freq, time
  mixer1.gain(2, 1.0);
  envelope1.noteOn();

  delay(11000);  // Wait for sweep to complete

  envelope1.noteOff();
  stopAllAudio();

  Serial.println("Test 4 complete.");
  Serial.println();
}

void testWhiteNoise() {
  Serial.println("Test 5: White noise test for 3 seconds");
  Serial.println("Listen for static/hiss sound...");

  stopAllAudio();
  noise1.amplitude(0.2);  // Lower amplitude for noise
  mixer1.gain(3, 1.0);
  envelope1.noteOn();

  delay(3000);

  envelope1.noteOff();
  stopAllAudio();

  Serial.println("Test 5 complete.");
  Serial.println();
}

void testVolumeLevels() {
  Serial.println("Test 6: Volume level test");
  Serial.println("Testing different volume levels...");

  float volumes[] = { 0.1, 0.2, 0.4 };//, 0.6, 0.8, 1.0 };
  int numVolumes = sizeof(volumes) / sizeof(volumes[0]);

  for (int i = 0; i < numVolumes; i++) {
    Serial.print("Volume level: ");
    Serial.print(volumes[i] * 100, 0);
    Serial.println("%");

    stopAllAudio();
    sine1.frequency(800);
    sine1.amplitude(volumes[i]);
    mixer1.gain(0, 1.0);
    envelope1.noteOn();

    delay(2000);

    envelope1.noteOff();
    delay(500);
  }

  stopAllAudio();
  Serial.println("Test 6 complete.");
  Serial.println();
}

void testChannelBalance() {
  Serial.println("Test 7: Channel balance test");
  Serial.println("Testing left and right channels separately...");
  Serial.println("(MAX98357A is mono, but testing I2S channels)");

  // Test left channel only
  Serial.println("Left channel only...");
  stopAllAudio();
  sine1.frequency(800);
  sine1.amplitude(0.5);
  mixer1.gain(0, 1.0);
  envelope1.noteOn();

  // Disconnect right channel temporarily
  AudioConnection tempCord(envelope1, 0, i2s1, 0);  // Left only

  delay(3000);
  envelope1.noteOff();
  delay(500);

  // Test right channel only
  Serial.println("Right channel only...");
  envelope1.noteOn();

  // This would require dynamic patching - for now just note the test
  Serial.println("(Both channels will play - MAX98357A mixes L+R)");

  delay(3000);
  envelope1.noteOff();

  stopAllAudio();
  Serial.println("Test 7 complete.");
  Serial.println();
}

void runFullTestSuite() {
  Serial.println("Test 8: Full automatic test suite");
  Serial.println("Running all tests automatically...");
  Serial.println("Total time: approximately 2 minutes");
  Serial.println();

  test440Hz();
  delay(1000);

  testMultipleFrequencies();
  delay(1000);

  testDifferentWaveforms();
  delay(1000);

  testFrequencySweep();
  delay(1000);

  testWhiteNoise();
  delay(1000);

  testVolumeLevels();
  delay(1000);

  testChannelBalance();

  Serial.println("=== FULL TEST SUITE COMPLETE ===");
  Serial.println("If you heard all tests clearly, your MAX98357A is working properly!");
  Serial.println();
}

void updateAutomaticTest() {
  // Handle automatic test progression (if implemented)
  if (millis() - testStartTime > testDuration) {
    // Move to next test or stop
    testRunning = false;
    envelope1.noteOff();
    stopAllAudio();
  }
}

void stopCurrentTest() {
  Serial.println("Stopping current test...");
  testRunning = false;
  envelope1.noteOff();
  stopAllAudio();
  Serial.println("Test stopped.");
  Serial.println();
}

void stopAllAudio() {
  mixer1.gain(0, 0);
  mixer1.gain(1, 0);
  mixer1.gain(2, 0);
  mixer1.gain(3, 0);
}

void printAudioStats() {
  Serial.print("Audio CPU: ");
  Serial.print(AudioProcessorUsage(), 1);
  Serial.print("%, Memory: ");
  Serial.print(AudioMemoryUsage());
  Serial.print("/");
  Serial.print(AudioMemoryUsageMax());
  Serial.println(" blocks");
}

// Diagnostic functions
void printI2SStatus() {
  Serial.println("=== I2S STATUS ===");
  Serial.println("If you're not hearing anything, check:");
  Serial.println("1. Wiring connections (BCLK=21, WCLK=20, DIN=7)");
  Serial.println("2. Power to MAX98357A (3.3V, GND)");
  Serial.println("3. Speaker connections (OUT+, OUT-)");
  Serial.println("4. Speaker impedance (4-8 ohms recommended)");
  Serial.println("5. GAIN pin configuration for volume");
  Serial.println();
}

void printTroubleshooting() {
  Serial.println("=== TROUBLESHOOTING ===");
  Serial.println("No sound? Check:");
  Serial.println("- All wiring connections");
  Serial.println("- 3.3V power to MAX98357A");
  Serial.println("- Speaker polarity (try swapping +/-)");
  Serial.println("- Serial monitor shows CPU usage > 0%");
  Serial.println();
  Serial.println("Distorted sound? Check:");
  Serial.println("- Lower volume levels (reduce amplitude)");
  Serial.println("- Speaker impedance (4-8 ohms)");
  Serial.println("- Power supply quality");
  Serial.println();
  Serial.println("Quiet sound? Check:");
  Serial.println("- GAIN pin configuration");
  Serial.println("- Volume levels in code");
  Serial.println("- Speaker efficiency");
  Serial.println();
}