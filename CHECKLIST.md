# cw-trainer Comprehensive Checklist

(✓ = present/ok, ✗ = issue/missing, ⚠ = improvement idea)

## 1. Serial / Wi-Fi Protocol
- **1.1** READY message accepted for both `"ESP01:READY"` and `"ESP32:READY"` ✗
- **1.2** Heartbeat: Teensy still expects `"ESP01:HEARTBEAT"` but ESP32 never sends it ✗
- **1.3** PING sent every 1 s in `handleWiFiComm()` ✓
- **1.4** PONG processed in both blocking handshake (≈1330) and `processWiFiMessage()` (≈1404) ✓
- **1.5** `RESET_ESP` command path from Teensy → ESP32 missing ⚠
- **1.6** Debug prints use `Serial1` instead of `Serial` (e.g., line 1446) ✗
- **1.7** Protocol strings hard-coded rather than shared header ⚠

## 2. Duplicate / Dead Code
- **2.1** Lesson-text generators share ~80 % logic ⚠
- **2.2** Button debouncing duplicated ✗
- **2.3** Multiple setters could be wrapped in single `applySettings()` ⚠
- **2.4** FFT & AGC allocated even when decoder disabled ⚠

## 3. Potential Logic / Runtime Bugs
- **3.1** `kochLessons[]` length vs index bounds (`<= 40`) ✗
- **3.2** `lessonAccuracy[40]` indexed with 1-based `kochLesson` ✗
- **3.3** `wifiIncomingData` may grow unbounded ⚠
- **3.4** No interrupt safety around shared vars ⚠
- **3.5** Frequent EEPROM writes – add dirty-flag ⚠

## 4. User-Experience Improvements
- **4.1** Buffer OLED updates to avoid flicker ⚠
- **4.2** Wi-Fi status icon on display ⚠
- **4.3** Menu item to trigger `RESET_ESP` / reconnect ⚠

## 5. Code Style / Maintainability
- **5.1** Replace magic numbers with `constexpr` ⚠
- **5.2** Move protocol strings & pin maps to shared header ⚠
- **5.3** Prefer `constexpr float` literals ⚠

## 6. Memory / Performance
- **6.1** FFT1024 object always alive ⚠
- **6.2** Heap churn in `sendStatusToWiFi()` ⚠
- **6.3** Use PROGMEM / `F()` for constant text ⚠

## 7. Audio Path Checks
- **7.1** Tone detector syncs with sidetone ✓
- **7.2** External audio gain/AGC may need adjustment ⚠

---

### Next Action Priorities
1. Fix READY/HEARTBEAT mismatches & debug port usage.
2. Resolve array-bounds issues.
3. Merge duplicate button handling & text generators.
4. Extract shared protocol & pin constants.
5. Proceed with optimisations and UX polish.
