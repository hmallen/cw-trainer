/* main_firmware_with_flasherx.ino  -------- Teensy 4.1 */
#include "FlashTxx.h"
#include "FXUtil.h"

// pick the port for incoming HEX:
#define UPDATE_PORT  Serial1     // pins 0 / 1
#define UPDATE_BAUD  115200

void setup() {
  Serial.begin(9600);           // debug over USB
  UPDATE_PORT.begin(UPDATE_BAUD);

  // Start FlasherX in non-interactive "auto-commit" mode
  if (update_firmware(&UPDATE_PORT, &Serial, /*autoCommit=*/true)) {
      Serial.println("FW updated, rebooting…");
      delay(50);
      SCB_AIRCR = 0x05FA0004;   // software reset
  }
}

void loop() {
  // your normal application code
}
