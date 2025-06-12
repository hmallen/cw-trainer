/*
 * ESP8266 ESP-01 WiFi Companion for CW Ultimate Trainer
 * 
 * This code runs on an ESP8266 ESP-01 module to provide WiFi
 * connectivity and web interface for the Teensy 4.1 CW trainer.
 * 
 * Hardware Setup:
 * ESP-01 → Teensy 4.1
 * VCC    → 3.3V
 * GND    → GND
 * TX     → Pin 0 (RX1 - Serial1)
 * RX     → Pin 1 (TX1 - Serial1)
 * CH_PD  → 3.3V
 * RST    → 3.3V (through 10kΩ resistor)
 * 
 * Programming the ESP-01:
 * 1. Use an ESP-01 programmer or FTDI adapter
 * 2. Connect GPIO0 to GND during programming
 * 3. Upload this code using ESP8266 Arduino Core
 * 4. Remove GPIO0-GND connection for normal operation
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "secrets.h"

// WiFi Configuration
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Web server on port 80
ESP8266WebServer server(80);

// Data structure to hold CW trainer status
struct TrainerStatus {
  int lesson = 1;
  int frequency = 600;
  int speed = 20;
  int effectiveSpeed = 13;
  float accuracy = 0.0;
  bool decoderEnabled = true;
  bool kochMode = false;
  String currentText = "";
  String decodedText = "";
  unsigned long sessions = 0;
  unsigned long characters = 0;
  float bestWPM = 0.0;
  String waveform = "Sine";
  String output = "Headphones";
  bool sending = false;
  bool listening = false;
} trainer;

// Communication buffers
String incomingData = "";
unsigned long lastUpdate = 0;
unsigned long lastHeartbeat = 0;

void setup() {
  // Initialize serial communication with Teensy
  Serial.begin(115200);
  delay(100);

  // Connect to WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  setupWebServer();

  // Start the server
  server.begin();
  Serial.println("Web server started");

  // Request initial status from Teensy
  Serial.println("ESP01:READY");
}

void loop() {
  // Handle web server requests
  server.handleClient();

  // Handle communication with Teensy
  handleTeensyComm();

  // Send periodic heartbeat
  if (millis() - lastHeartbeat > 10000) {  // Every 10 seconds
    Serial.println("ESP01:HEARTBEAT");
    lastHeartbeat = millis();
  }

  delay(10);  // Small delay to prevent watchdog issues
}

void setupWebServer() {
  // Main page
  server.on("/", handleRoot);

  // API endpoints
  server.on("/api/status", HTTP_GET, handleAPIStatus);
  server.on("/api/control", HTTP_POST, handleAPIControl);
  server.on("/api/stats", HTTP_GET, handleAPIStats);

  // Static resources
  server.on("/style.css", handleCSS);
  server.on("/script.js", handleJavaScript);

  // Handle not found
  server.onNotFound(handleNotFound);
}

void handleRoot() {
  String html = generateMainPage();
  server.send(200, "text/html", html);
}

String generateMainPage() {
  String html = "<!DOCTYPE html>\n";
  html += "<html>\n";
  html += "<head>\n";
  html += "    <title>CW Ultimate Trainer</title>\n";
  html += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  html += "    <link rel=\"stylesheet\" href=\"/style.css\">\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "    <div class=\"container\">\n";
  html += "        <h1>🎵 CW Ultimate Trainer</h1>\n";
  html += "        \n";
  html += "        <div class=\"status-grid\">\n";
  html += "            <div class=\"card\">\n";
  html += "                <h3>Koch Training</h3>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Lesson:</span>\n";
  html += "                    <span id=\"lesson\">" + String(trainer.lesson) + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Speed:</span>\n";
  html += "                    <span id=\"speed\">" + String(trainer.speed) + "/" + String(trainer.effectiveSpeed) + " WPM</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Accuracy:</span>\n";
  html += "                    <span id=\"accuracy\">" + String(trainer.accuracy, 1) + "%</span>\n";
  html += "                </div>\n";
  html += "            </div>\n";
  html += "            \n";
  html += "            <div class=\"card\">\n";
  html += "                <h3>Audio Settings</h3>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Frequency:</span>\n";
  html += "                    <span id=\"frequency\">" + String(trainer.frequency) + " Hz</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Waveform:</span>\n";
  html += "                    <span id=\"waveform\">" + trainer.waveform + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Output:</span>\n";
  html += "                    <span id=\"output\">" + trainer.output + "</span>\n";
  html += "                </div>\n";
  html += "            </div>\n";
  html += "            \n";
  html += "            <div class=\"card\">\n";
  html += "                <h3>Session Stats</h3>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Sessions:</span>\n";
  html += "                    <span id=\"sessions\">" + String(trainer.sessions) + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Characters:</span>\n";
  html += "                    <span id=\"characters\">" + String(trainer.characters) + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Best WPM:</span>\n";
  html += "                    <span id=\"bestwpm\">" + String(trainer.bestWPM, 1) + "</span>\n";
  html += "                </div>\n";
  html += "            </div>\n";
  html += "            \n";
  html += "            <div class=\"card\">\n";
  html += "                <h3>Status</h3>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Mode:</span>\n";
  html += "                    <span id=\"mode\">" + String(trainer.kochMode ? "Koch Training" : "Free Practice") + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Decoder:</span>\n";
  html += "                    <span id=\"decoder\">" + String(trainer.decoderEnabled ? "ON" : "OFF") + "</span>\n";
  html += "                </div>\n";
  html += "                <div class=\"status-item\">\n";
  html += "                    <span>Activity:</span>\n";
  html += "                    <span id=\"activity\">" + String(trainer.sending ? "Sending" : trainer.listening ? "Listening"
                                                                                                                : "Ready")
          + "</span>\n";
  html += "                </div>\n";
  html += "            </div>\n";
  html += "        </div>\n";
  html += "        \n";
  html += "        <div class=\"controls\">\n";
  html += "            <h3>Quick Controls</h3>\n";
  html += "            <div class=\"button-grid\">\n";
  html += "                <button onclick=\"sendCommand('START_KOCH')\" class=\"btn btn-primary\">Start Koch Lesson</button>\n";
  html += "                <button onclick=\"sendCommand('CALLSIGN_PRACTICE')\" class=\"btn btn-secondary\">Callsign Practice</button>\n";
  html += "                <button onclick=\"sendCommand('QSO_SIMULATION')\" class=\"btn btn-secondary\">QSO Simulation</button>\n";
  html += "                <button onclick=\"sendCommand('TOGGLE_DECODER')\" class=\"btn btn-info\">Toggle Decoder</button>\n";
  html += "            </div>\n";
  html += "            \n";
  html += "            <div class=\"input-controls\">\n";
  html += "                <div class=\"control-group\">\n";
  html += "                    <label>Frequency (Hz):</label>\n";
  html += "                    <input type=\"range\" id=\"freqSlider\" min=\"300\" max=\"1200\" value=\"" + String(trainer.frequency) + "\" \n";
  html += "                           onchange=\"setFrequency(this.value)\">\n";
  html += "                    <span id=\"freqValue\">" + String(trainer.frequency) + "</span>\n";
  html += "                </div>\n";
  html += "                \n";
  html += "                <div class=\"control-group\">\n";
  html += "                    <label>Speed (WPM):</label>\n";
  html += "                    <input type=\"range\" id=\"speedSlider\" min=\"5\" max=\"50\" value=\"" + String(trainer.speed) + "\" \n";
  html += "                           onchange=\"setSpeed(this.value)\">\n";
  html += "                    <span id=\"speedValue\">" + String(trainer.speed) + "</span>\n";
  html += "                </div>\n";
  html += "            </div>\n";
  html += "        </div>\n";
  html += "        \n";
  html += "        <div class=\"decoded-text\">\n";
  html += "            <h3>Decoded Text</h3>\n";
  html += "            <div id=\"decodedOutput\" class=\"text-output\">" + trainer.decodedText + "</div>\n";
  html += "        </div>\n";
  html += "        \n";
  html += "        <div class=\"current-text\">\n";
  html += "            <h3>Current Lesson</h3>\n";
  html += "            <div id=\"currentOutput\" class=\"text-output\">" + trainer.currentText + "</div>\n";
  html += "        </div>\n";
  html += "    </div>\n";
  html += "    \n";
  html += "    <script src=\"/script.js\"></script>\n";
  html += "</body>\n";
  html += "</html>\n";

  return html;
}

void handleCSS() {
  String css = R"(
body {
    font-family: 'Segoe UI', Arial, sans-serif;
    margin: 0;
    padding: 20px;
    background: linear-gradient(135deg, #1e3c72, #2a5298);
    color: #fff;
    min-height: 100vh;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
}

h1 {
    text-align: center;
    margin-bottom: 30px;
    text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
}

.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 20px;
    margin-bottom: 30px;
}

.card {
    background: rgba(255,255,255,0.1);
    backdrop-filter: blur(10px);
    border-radius: 12px;
    padding: 20px;
    border: 1px solid rgba(255,255,255,0.2);
}

.card h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #ffd700;
}

.status-item {
    display: flex;
    justify-content: space-between;
    margin-bottom: 10px;
    padding: 5px 0;
    border-bottom: 1px solid rgba(255,255,255,0.1);
}

.controls {
    background: rgba(255,255,255,0.1);
    backdrop-filter: blur(10px);
    border-radius: 12px;
    padding: 20px;
    margin-bottom: 20px;
    border: 1px solid rgba(255,255,255,0.2);
}

.button-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 10px;
    margin-bottom: 20px;
}

.btn {
    padding: 12px 24px;
    border: none;
    border-radius: 8px;
    cursor: pointer;
    font-size: 14px;
    font-weight: bold;
    transition: all 0.3s ease;
}

.btn-primary {
    background: #007bff;
    color: white;
}

.btn-primary:hover {
    background: #0056b3;
    transform: translateY(-2px);
}

.btn-secondary {
    background: #6c757d;
    color: white;
}

.btn-secondary:hover {
    background: #545b62;
    transform: translateY(-2px);
}

.btn-info {
    background: #17a2b8;
    color: white;
}

.btn-info:hover {
    background: #117a8b;
    transform: translateY(-2px);
}

.input-controls {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 20px;
}

.control-group {
    display: flex;
    flex-direction: column;
    gap: 8px;
}

.control-group label {
    font-weight: bold;
    color: #ffd700;
}

.control-group input[type="range"] {
    width: 100%;
}

.text-output {
    background: rgba(0,0,0,0.3);
    padding: 15px;
    border-radius: 8px;
    font-family: 'Courier New', monospace;
    font-size: 16px;
    min-height: 50px;
    border: 1px solid rgba(255,255,255,0.2);
    word-break: break-all;
}

.decoded-text, .current-text {
    background: rgba(255,255,255,0.1);
    backdrop-filter: blur(10px);
    border-radius: 12px;
    padding: 20px;
    margin-bottom: 20px;
    border: 1px solid rgba(255,255,255,0.2);
}

.decoded-text h3, .current-text h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #ffd700;
}

@media (max-width: 768px) {
    .status-grid {
        grid-template-columns: 1fr;
    }
    
    .button-grid {
        grid-template-columns: 1fr;
    }
    
    .input-controls {
        grid-template-columns: 1fr;
    }
}
)";
  server.send(200, "text/css", css);
}

void handleJavaScript() {
  String js = R"(
let updateInterval;

function sendCommand(command) {
    fetch('/api/control', {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain'
        },
        body: command
    })
    .then(response => response.text())
    .then(data => {
        console.log('Command sent:', command, 'Response:', data);
        // Update status immediately after command
        setTimeout(updateStatus, 500);
    })
    .catch(error => {
        console.error('Error sending command:', error);
    });
}

function setFrequency(value) {
    document.getElementById('freqValue').textContent = value;
    sendCommand('SET_FREQ:' + value);
}

function setSpeed(value) {
    document.getElementById('speedValue').textContent = value;
    sendCommand('SET_SPEED:' + value);
}

function updateStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Update all status fields
            document.getElementById('lesson').textContent = data.lesson;
            document.getElementById('speed').textContent = data.speed + '/' + data.effectiveSpeed + ' WPM';
            document.getElementById('accuracy').textContent = data.accuracy.toFixed(1) + '%';
            document.getElementById('frequency').textContent = data.frequency + ' Hz';
            document.getElementById('waveform').textContent = data.waveform;
            document.getElementById('output').textContent = data.output;
            document.getElementById('sessions').textContent = data.sessions;
            document.getElementById('characters').textContent = data.characters;
            document.getElementById('bestwpm').textContent = data.bestWPM.toFixed(1);
            document.getElementById('mode').textContent = data.kochMode ? 'Koch Training' : 'Free Practice';
            document.getElementById('decoder').textContent = data.decoderEnabled ? 'ON' : 'OFF';
            
            let activity = 'Ready';
            if (data.sending) activity = 'Sending';
            else if (data.listening) activity = 'Listening';
            document.getElementById('activity').textContent = activity;
            
            // Update text outputs
            document.getElementById('decodedOutput').textContent = data.decodedText;
            document.getElementById('currentOutput').textContent = data.currentText;
            
            // Update sliders
            document.getElementById('freqSlider').value = data.frequency;
            document.getElementById('speedSlider').value = data.speed;
            document.getElementById('freqValue').textContent = data.frequency;
            document.getElementById('speedValue').textContent = data.speed;
        })
        .catch(error => {
            console.error('Error updating status:', error);
        });
}

// Start auto-update when page loads
document.addEventListener('DOMContentLoaded', function() {
    updateStatus();
    updateInterval = setInterval(updateStatus, 2000); // Update every 2 seconds
});

// Stop auto-update when page is hidden
document.addEventListener('visibilitychange', function() {
    if (document.hidden) {
        if (updateInterval) {
            clearInterval(updateInterval);
        }
    } else {
        updateStatus();
        updateInterval = setInterval(updateStatus, 2000);
    }
});
)";
  server.send(200, "application/javascript", js);
}

void handleAPIStatus() {
  JsonDocument doc;

  doc["lesson"] = trainer.lesson;
  doc["frequency"] = trainer.frequency;
  doc["speed"] = trainer.speed;
  doc["effectiveSpeed"] = trainer.effectiveSpeed;
  doc["accuracy"] = trainer.accuracy;
  doc["decoderEnabled"] = trainer.decoderEnabled;
  doc["kochMode"] = trainer.kochMode;
  doc["currentText"] = trainer.currentText;
  doc["decodedText"] = trainer.decodedText;
  doc["sessions"] = trainer.sessions;
  doc["characters"] = trainer.characters;
  doc["bestWPM"] = trainer.bestWPM;
  doc["waveform"] = trainer.waveform;
  doc["output"] = trainer.output;
  doc["sending"] = trainer.sending;
  doc["listening"] = trainer.listening;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAPIControl() {
  String command = server.arg("plain");

  // Send command to Teensy
  Serial.println("TEENSY:" + command);

  server.send(200, "text/plain", "Command sent: " + command);
}

void handleAPIStats() {
  JsonDocument doc;

  doc["sessions"] = trainer.sessions;
  doc["characters"] = trainer.characters;
  doc["bestWPM"] = trainer.bestWPM;
  doc["accuracy"] = trainer.accuracy;
  doc["lesson"] = trainer.lesson;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Page not found");
}

void handleTeensyComm() {
  // Read data from Teensy
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processTeensyMessage(incomingData);
      incomingData = "";
    } else {
      incomingData += c;
    }
  }
}

void processTeensyMessage(String message) {
  message.trim();

  if (message.startsWith("STATUS:")) {
    parseStatusMessage(message.substring(7));
  } else if (message.startsWith("DECODED:")) {
    trainer.decodedText += message.substring(8);
    // Keep only last 200 characters
    if (trainer.decodedText.length() > 200) {
      trainer.decodedText = trainer.decodedText.substring(trainer.decodedText.length() - 200);
    }
  } else if (message.startsWith("CURRENT:")) {
    trainer.currentText = message.substring(8);
  } else if (message.startsWith("STATS:")) {
    parseStatsMessage(message.substring(6));
  }
}

void parseStatusMessage(String status) {
  // Parse format: "LESSON=5,FREQ=600,SPEED=20,ACC=94.2,DEC=1,KOCH=1"
  int pos = 0;
  while (pos < status.length()) {
    int eqPos = status.indexOf('=', pos);
    int commaPos = status.indexOf(',', pos);
    if (commaPos == -1) commaPos = status.length();

    if (eqPos != -1 && eqPos < commaPos) {
      String key = status.substring(pos, eqPos);
      String value = status.substring(eqPos + 1, commaPos);

      if (key == "LESSON") trainer.lesson = value.toInt();
      else if (key == "FREQ") trainer.frequency = value.toInt();
      else if (key == "SPEED") trainer.speed = value.toInt();
      else if (key == "EFFSPEED") trainer.effectiveSpeed = value.toInt();
      else if (key == "ACC") trainer.accuracy = value.toFloat();
      else if (key == "DEC") trainer.decoderEnabled = (value == "1");
      else if (key == "KOCH") trainer.kochMode = (value == "1");
      else if (key == "WAVE") trainer.waveform = value;
      else if (key == "OUT") trainer.output = value;
      else if (key == "SEND") trainer.sending = (value == "1");
      else if (key == "LISTEN") trainer.listening = (value == "1");
    }

    pos = commaPos + 1;
  }
}

void parseStatsMessage(String stats) {
  // Parse format: "SESSIONS=15,CHARS=1250,BESTWPM=18.5"
  int pos = 0;
  while (pos < stats.length()) {
    int eqPos = stats.indexOf('=', pos);
    int commaPos = stats.indexOf(',', pos);
    if (commaPos == -1) commaPos = stats.length();

    if (eqPos != -1 && eqPos < commaPos) {
      String key = stats.substring(pos, eqPos);
      String value = stats.substring(eqPos + 1, commaPos);

      if (key == "SESSIONS") trainer.sessions = value.toInt();
      else if (key == "CHARS") trainer.characters = value.toInt();
      else if (key == "BESTWPM") trainer.bestWPM = value.toFloat();
    }

    pos = commaPos + 1;
  }
}