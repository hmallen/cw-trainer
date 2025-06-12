# CW Trainer (Teensy 4.1 + ESP8266)

## Overview

CW Trainer is a full-featured Morse code (CW) learning and practice station built around a Teensy 4.1 microcontroller with the PJRC Audio library, an optional MAX98357A I²S amplifier and an ESP-01 (ESP8266) Wi-Fi companion module. It can both generate and decode CW, show results on a 128×64 OLED display and expose a responsive web UI over Wi-Fi.

The system is intended for radio amateurs who want an all-in-one portable trainer that covers Koch lessons, call-sign drills, simulated QSOs/contests and custom text at Farnsworth speeds.

## Key Features

- High-quality sine / arbitrary-waveform sidetone from 300–1200 Hz
- Adjustable audio output: on-board DAC, headphone jack or external MAX98357A I²S amplifier
- Practice modes  
  * **Koch progression** (40 lessons) with automatic evaluation  
  * **Random callsign** generator  
  * **Simulated QSO** and **contest** exchanges  
  * **Custom / weak-character** drills
- Real-time CW decoder with AGC and tone detection
- Integrated statistics saved to EEPROM (accuracy, WPM, session time, per-character errors)
- 128×64 SSD1306 OLED-based menu system (Adafruit_GFX)
- Wi-Fi web interface (ESP-01) to monitor status, issue commands and view live decoded text
- Companion firmware for ESP8266 written using Arduino core & ArduinoJson

## Repository Layout
```text
cw-trainer/
├── cw-trainer/              # Teensy 4.1 firmware (main project)
│   └── cw-trainer.ino
├── esp8266_wifi_companion/  # ESP-01 Wi-Fi companion firmware
│   ├── esp8266_wifi_companion.ino
│   └── secrets.h.example
├── max98357a_tester/        # Simple test sketch for audio amp
│   └── max98357a_tester.ino
├── LICENSE
└── README.md
```

## Hardware Connections (Teensy 4.1)
| Function | Pin | Note |
|----------|-----|------|
| Key input | 2 | Straight key or keyer output (active-low) |
| Waveform select button | 3 | |
| Output select button | 4 | Headphones ↔ Speaker |
| Decoder enable toggle | 5 | |
| Koch mode toggle | 6 | |
| Next lesson button | 7 | |
| Audio input select | 8 | Sidetone ↔ External |
| Menu / back | 9 | |
| Select / enter | 10 | |
| Audio input (AF from RX) | A3 | AC-coupled, 1 Vpp max |
| Frequency pot | A0 | 10 k linear |
| Volume pot | A1 | 10 k linear |
| Speed pot | A2 | 10 k linear |
| I²S data out | 22 (TX) | To MAX98357A, follow PJRC Audio shield pin-out |
| OLED SDA/SCL | 18 / 19 | 3.3 V I²C |
| ESP-01 Serial1 RX/TX | 0 / 1 | 115 200 Bd |

## Building & Flashing

1. Install **Arduino IDE** with **Teensyduino** (v1.59 +) and the following libraries via Library Manager:
   - `Audio` (bundled with Teensyduino)
   - `Bounce2`
   - `Adafruit_GFX` & `Adafruit_SSD1306`
   - `ArduinoJson` (for ESP8266 sketch)

2. Open `cw-trainer/cw-trainer.ino`, set board to **Teensy 4.1**, CPU speed 600 MHz and compile / upload.

3. For Wi-Fi support: compile `esp8266_wifi_companion/esp8266_wifi_companion.ino` with **ESP8266** core 3.1 +.  
   Copy `secrets.h.example` to `secrets.h` and add your Wi-Fi SSID and password.

4. (Optional) To verify audio path run `max98357a_tester/max98357a_tester.ino`.

## Usage

- On power-up the OLED main screen shows lesson, speed, accuracy and current WPM.  
- Press **Menu** to navigate practice modes, settings and statistics.  
- In Koch training the sketch automatically advances when ≥ 90 % accuracy is achieved.  
- Connect to the device’s IP (printed on Serial Monitor) to open the web UI.

## Potential Improvements

Feel free to open issues or PRs! Some ideas:
- OTA firmware update for ESP-01
- Alternate decoder algorithms (FFT-based or ML)
- Integration with online practice services (LCWO, MorseRunner)

## License

This project is licensed under the MIT License – see [`LICENSE`](LICENSE) for details.

## Credits

Inspired by many open-source CW projects and the excellent PJRC Teensy Audio ecosystem.
