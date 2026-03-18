#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------- WiFi Config ----------
const char* ssid = "Aimers-2GHz";
const char* password = "Aimers@254";

// ---------- Robot Fixed IP ----------
IPAddress local_IP(192, 168, 1, 4);    // Robot ESP32 fixed IP
IPAddress gateway(192, 168, 1, 1);     // Router gateway
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);      // Optional 
IPAddress secondaryDNS(8, 8, 4, 4);    // Optional

// ---------- Web Server ----------
WebServer server(80);

// ---------- Sensor Pins ----------
#define DHTPIN 17           // DHT11 sensor pin
#define GAS_SENSOR_PIN 16  // Gas sensor analog pin (0=High Gas, 4095=Low Gas)
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ---------------- ROBOT MOVEMENT MOTOR PINS ---------------
#define MOTOR1_PIN1 21  // Motor 1 forward
#define MOTOR1_PIN2 19  // Motor 1 backward
#define MOTOR2_PIN1 18  // Motor 2 forward
#define MOTOR2_PIN2 5  // Motor 2 backward

// ---------------- ROBOTIC ARM PINS ------------
#define ARM_UP_PIN     33  // Arm up movement
#define ARM_DOWN_PIN   32   // Arm down movement
#define GRIPPER_OPEN_PIN   22 // Gripper open
#define GRIPPER_CLOSE_PIN 23  // Gripper close
#define ARM_CW_PIN 14      // Arm clockwise rotation (relay)
#define ARM_CCW_PIN 27     // Arm counter-clockwise rotation (relay)

// ---------------- TOOL CONTROL PINS (RELAYS) ------------
#define DRILL_RELAY_PIN 26   // Drill relay (LOW = ON)
#define LIGHT_RELAY_PIN 25  // Light relay (LOW = ON)

// Arm movement duration (configurable via web)
unsigned long armMoveDuration = 1000; // Default 1 second (1000ms)

// Control mode variables
bool robotLockMode = false;
bool drillOn = false;
bool lightOn = false;
String currentRobotAction = "stop";

// Arm control timers
unsigned long armUpStopTime = 0;
unsigned long armDownStopTime = 0;
unsigned long gripperOpenStopTime = 0;
unsigned long gripperCloseStopTime = 0;
unsigned long armCwStopTime = 0;
unsigned long armCcwStopTime = 0;

// Sensor values
float temperature = 0.0;
float humidity = 0.0;
int gasValue = 0;      // Raw value (0=High Gas, 4095=Low Gas)
int gasPercentage = 0; // Converted to 0-100% (0%=High Gas, 100%=Low Gas)

// Command history
String commandHistory[10];
int historyIndex = 0;

// -----------------------------------------------------------
// MOTOR FUNCTIONS
// -----------------------------------------------------------
void robotForward() {
  digitalWrite(MOTOR1_PIN1, HIGH); digitalWrite(MOTOR1_PIN2, LOW);
  digitalWrite(MOTOR2_PIN1, HIGH); digitalWrite(MOTOR2_PIN2, LOW);
  currentRobotAction = "forward";
  Serial.println("🤖 Robot: Moving Forward");
  addToHistory("🤖 Robot: Moving Forward");
}

void robotBackward() {
  digitalWrite(MOTOR1_PIN1, LOW); digitalWrite(MOTOR1_PIN2, HIGH);
  digitalWrite(MOTOR2_PIN1, LOW); digitalWrite(MOTOR2_PIN2, HIGH);
  currentRobotAction = "backward";
  Serial.println("🤖 Robot: Moving Backward");
  addToHistory("🤖 Robot: Moving Backward");
}

void robotLeft() {
  digitalWrite(MOTOR1_PIN1, LOW); digitalWrite(MOTOR1_PIN2, HIGH);
  digitalWrite(MOTOR2_PIN1, HIGH); digitalWrite(MOTOR2_PIN2, LOW);
  currentRobotAction = "left";
  Serial.println("🤖 Robot: Turning Left");
  addToHistory("🤖 Robot: Turning Left");
}

void robotRight() {
  digitalWrite(MOTOR1_PIN1, HIGH); digitalWrite(MOTOR1_PIN2, LOW);
  digitalWrite(MOTOR2_PIN1, LOW); digitalWrite(MOTOR2_PIN2, HIGH);
  currentRobotAction = "right";
  Serial.println("🤖 Robot: Turning Right");
  addToHistory("🤖 Robot: Turning Right");
}

void robotStop() {
  digitalWrite(MOTOR1_PIN1, LOW); digitalWrite(MOTOR1_PIN2, LOW);
  digitalWrite(MOTOR2_PIN1, LOW); digitalWrite(MOTOR2_PIN2, LOW);
  currentRobotAction = "stop";
  Serial.println("🤖 Robot: Stopped");
  addToHistory("🤖 Robot: Stopped");
}

// -----------------------------------------------------------
// ROBOTIC ARM FUNCTIONS WITH TIMED MOVEMENT
// -----------------------------------------------------------
void armUp() {
  digitalWrite(ARM_UP_PIN, HIGH);
  digitalWrite(ARM_DOWN_PIN, LOW);
  armUpStopTime = millis() + armMoveDuration;
  Serial.println("🦾 Arm: Moving Up for " + String(armMoveDuration) + "ms");
  addToHistory("🦾 Arm: Moving Up (" + String(armMoveDuration) + "ms)");
}

void armDown() {
  digitalWrite(ARM_UP_PIN, LOW);
  digitalWrite(ARM_DOWN_PIN, HIGH);
  armDownStopTime = millis() + armMoveDuration;
  Serial.println("🦾 Arm: Moving Down for " + String(armMoveDuration) + "ms");
  addToHistory("🦾 Arm: Moving Down (" + String(armMoveDuration) + "ms)");
}

void armStop() {
  digitalWrite(ARM_UP_PIN, LOW);
  digitalWrite(ARM_DOWN_PIN, LOW);
}

void gripperOpen() {
  digitalWrite(GRIPPER_OPEN_PIN, HIGH);
  digitalWrite(GRIPPER_CLOSE_PIN, LOW);
  gripperOpenStopTime = millis() + armMoveDuration;
  Serial.println("✋ Gripper: Opening for " + String(armMoveDuration) + "ms");
  addToHistory("✋ Gripper: Opening (" + String(armMoveDuration) + "ms)");
}

void gripperClose() {
  digitalWrite(GRIPPER_OPEN_PIN, LOW);
  digitalWrite(GRIPPER_CLOSE_PIN, HIGH);
  gripperCloseStopTime = millis() + armMoveDuration;
  Serial.println("✋ Gripper: Closing for " + String(armMoveDuration) + "ms");
  addToHistory("✋ Gripper: Closing (" + String(armMoveDuration) + "ms)");
}

void gripperStop() {
  digitalWrite(GRIPPER_OPEN_PIN, LOW);
  digitalWrite(GRIPPER_CLOSE_PIN, LOW);
}

void armClockwise() {
  digitalWrite(ARM_CW_PIN, HIGH);
  digitalWrite(ARM_CCW_PIN, LOW);
  armCwStopTime = millis() + armMoveDuration;
  Serial.println("🔄 Arm: Rotating Clockwise for " + String(armMoveDuration) + "ms");
  addToHistory("🔄 Arm: Rotating CW (" + String(armMoveDuration) + "ms)");
}

void armCounterClockwise() {
  digitalWrite(ARM_CW_PIN, LOW);
  digitalWrite(ARM_CCW_PIN, HIGH);
  armCcwStopTime = millis() + armMoveDuration;
  Serial.println("🔄 Arm: Rotating Counter-Clockwise for " + String(armMoveDuration) + "ms");
  addToHistory("🔄 Arm: Rotating CCW (" + String(armMoveDuration) + "ms)");
}

void armRotationStop() {
  digitalWrite(ARM_CW_PIN, LOW);
  digitalWrite(ARM_CCW_PIN, LOW);
}

// Function to check and stop timed arm movements
void checkArmTimers() {
  unsigned long currentTime = millis();
  
  if (armUpStopTime > 0 && currentTime >= armUpStopTime) {
    armStop();
    armUpStopTime = 0;
    Serial.println("🦾 Arm: Auto stopped after " + String(armMoveDuration) + "ms");
  }
  
  if (armDownStopTime > 0 && currentTime >= armDownStopTime) {
    armStop();
    armDownStopTime = 0;
    Serial.println("🦾 Arm: Auto stopped after " + String(armMoveDuration) + "ms");
  }
  
  if (gripperOpenStopTime > 0 && currentTime >= gripperOpenStopTime) {
    gripperStop();
    gripperOpenStopTime = 0;
    Serial.println("✋ Gripper: Auto stopped after " + String(armMoveDuration) + "ms");
  }
  
  if (gripperCloseStopTime > 0 && currentTime >= gripperCloseStopTime) {
    gripperStop();
    gripperCloseStopTime = 0;
    Serial.println("✋ Gripper: Auto stopped after " + String(armMoveDuration) + "ms");
  }
  
  if (armCwStopTime > 0 && currentTime >= armCwStopTime) {
    armRotationStop();
    armCwStopTime = 0;
    Serial.println("🔄 Arm: Auto stopped after " + String(armMoveDuration) + "ms");
  }
  
  if (armCcwStopTime > 0 && currentTime >= armCcwStopTime) {
    armRotationStop();
    armCcwStopTime = 0;
    Serial.println("🔄 Arm: Auto stopped after " + String(armMoveDuration) + "ms");
  }
}

// -----------------------------------------------------------
// TOOL CONTROL FUNCTIONS (RELAYS)
// -----------------------------------------------------------
void drillOnFunction() {
  digitalWrite(DRILL_RELAY_PIN, LOW);  // LOW = ON for relays
  drillOn = true;
  Serial.println("🔩 Drill: ACTIVATED");
  addToHistory("🔩 Drill: ACTIVATED");
}

void drillOffFunction() {
  digitalWrite(DRILL_RELAY_PIN, HIGH); // HIGH = OFF for relays
  drillOn = false;
  Serial.println("🔩 Drill: DEACTIVATED");
  addToHistory("🔩 Drill: DEACTIVATED");
}

void toggleDrill() {
  if (drillOn) {
    drillOffFunction();
  } else {
    drillOnFunction();
  }
}

void lightOnFunction() {
  digitalWrite(LIGHT_RELAY_PIN, LOW);  // LOW = ON for relays
  lightOn = true;
  Serial.println("💡 Light: ACTIVATED");
  addToHistory("💡 Light: ACTIVATED");
}

void lightOffFunction() {
  digitalWrite(LIGHT_RELAY_PIN, HIGH); // HIGH = OFF for relays
  lightOn = false;
  Serial.println("💡 Light: DEACTIVATED");
  addToHistory("💡 Light: DEACTIVATED");
}

void toggleLight() {
  if (lightOn) {
    lightOffFunction();
  } else {
    lightOnFunction();
  }
}

// -----------------------------------------------------------
// SENSOR FUNCTIONS
// -----------------------------------------------------------
void updateSensors() {
  // Read DHT11 sensor
  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();
  
  if (!isnan(newTemp)) {
    temperature = newTemp;
  }
  if (!isnan(newHumidity)) {
    humidity = newHumidity;
  }
  
  // Read gas sensor (0=High Gas, 4095=Low Gas)
  gasValue = analogRead(GAS_SENSOR_PIN);
  
  // Convert to percentage: 0-4095 -> 0-100% (0%=High Gas, 100%=Low Gas)
  // Inverted: 0 = 100% gas (danger), 4095 = 0% gas (safe)
  gasPercentage = map(gasValue, 0, 4095, 100, 0);
  
  Serial.printf("📊 Sensors - Temp: %.1f°C, Humidity: %.1f%%, Gas Raw: %d, Gas %%: %d%%\n", 
                temperature, humidity, gasValue, gasPercentage);
}

// -----------------------------------------------------------
// HELPER FUNCTIONS
// -----------------------------------------------------------
void addToHistory(String command) {
  commandHistory[historyIndex] = command;
  historyIndex = (historyIndex + 1) % 10;
}

// Corrected gas level functions (0=High Gas, 4095=Low Gas)
String getGasLevelText(int gasValue) {
  // 0-1365: High Gas (DANGER) - 0-33% of range
  // 1366-2730: Medium Gas (WARNING) - 34-66% of range
  // 2731-4095: Low Gas (SAFE) - 67-100% of range
  if (gasValue < 1365) return "DANGER";
  else if (gasValue < 2730) return "WARNING";
  else return "SAFE";
}

String getGasLevelColor(int gasValue) {
  if (gasValue < 1365) return "danger";      // High Gas = Red
  else if (gasValue < 2730) return "warning"; // Medium Gas = Yellow
  else return "success";                     // Low Gas = Green
}

String getGasLevelIcon(int gasValue) {
  if (gasValue < 1365) return "fas fa-skull-crossbones";    // Danger
  else if (gasValue < 2730) return "fas fa-exclamation-triangle"; // Warning
  else return "fas fa-check-circle";                       // Safe
}

// Get gas percentage for display (0-100%)
int getGasPercentage(int gasValue) {
  return map(gasValue, 0, 4095, 100, 0); // Inverted: 0=100%, 4095=0%
}

// -----------------------------------------------------------
// HTTP REQUEST HANDLERS
// -----------------------------------------------------------
void handleRobotForward() {
  if (!robotLockMode) {
    robotForward();
    server.send(200, "text/plain", "Moving forward");
  } else {
    robotForward();
    server.send(200, "text/plain", "LOCKED: Moving forward");
  }
}

void handleRobotBackward() {
  if (!robotLockMode) {
    robotBackward();
    server.send(200, "text/plain", "Moving backward");
  } else {
    robotBackward();
    server.send(200, "text/plain", "LOCKED: Moving backward");
  }
}

void handleRobotLeft() {
  if (!robotLockMode) {
    robotLeft();
    server.send(200, "text/plain", "Turning left");
  } else {
    robotLeft();
    server.send(200, "text/plain", "LOCKED: Turning left");
  }
}

void handleRobotRight() {
  if (!robotLockMode) {
    robotRight();
    server.send(200, "text/plain", "Turning right");
  } else {
    robotRight();
    server.send(200, "text/plain", "LOCKED: Turning right");
  }
}

void handleRobotStop() {
  robotStop();
  server.send(200, "text/plain", "Robot stopped");
}

void handleRobotLock() {
  robotLockMode = true;
  server.send(200, "text/plain", "Lock mode activated");
}

void handleRobotUnlock() {
  robotLockMode = false;
  robotStop();
  server.send(200, "text/plain", "Unlock mode activated");
}

// Arm control handlers - single press for timed movement
void handleArmUp() {
  armUp();
  server.send(200, "text/plain", "Arm moving up for " + String(armMoveDuration) + "ms");
}

void handleArmDown() {
  armDown();
  server.send(200, "text/plain", "Arm moving down for " + String(armMoveDuration) + "ms");
}

void handleArmStop() {
  armStop();
  armUpStopTime = 0;
  armDownStopTime = 0;
  server.send(200, "text/plain", "Arm stopped");
}

void handleGripperOpen() {
  gripperOpen();
  server.send(200, "text/plain", "Gripper opening for " + String(armMoveDuration) + "ms");
}

void handleGripperClose() {
  gripperClose();
  server.send(200, "text/plain", "Gripper closing for " + String(armMoveDuration) + "ms");
}

void handleGripperStop() {
  gripperStop();
  gripperOpenStopTime = 0;
  gripperCloseStopTime = 0;
  server.send(200, "text/plain", "Gripper stopped");
}

void handleArmCw() {
  armClockwise();
  server.send(200, "text/plain", "Arm rotating clockwise for " + String(armMoveDuration) + "ms");
}

void handleArmCcw() {
  armCounterClockwise();
  server.send(200, "text/plain", "Arm rotating counter-clockwise for " + String(armMoveDuration) + "ms");
}

void handleRotationStop() {
  armRotationStop();
  armCwStopTime = 0;
  armCcwStopTime = 0;
  server.send(200, "text/plain", "Arm rotation stopped");
}

// Tool control handlers
void handleDrillControl(String action) {
  if (action == "on") {
    drillOnFunction();
    server.send(200, "text/plain", "Drill activated");
  } else if (action == "off") {
    drillOffFunction();
    server.send(200, "text/plain", "Drill deactivated");
  } else if (action == "toggle") {
    toggleDrill();
    server.send(200, "text/plain", drillOn ? "Drill activated" : "Drill deactivated");
  }
}

void handleLightControl(String action) {
  if (action == "on") {
    lightOnFunction();
    server.send(200, "text/plain", "Light activated");
  } else if (action == "off") {
    lightOffFunction();
    server.send(200, "text/plain", "Light deactivated");
  } else if (action == "toggle") {
    toggleLight();
    server.send(200, "text/plain", lightOn ? "Light activated" : "Light deactivated");
  }
}

void handleAllStop() {
  robotStop();
  armStop();
  gripperStop();
  armRotationStop();
  drillOffFunction();
  lightOffFunction();
  
  // Reset all timers
  armUpStopTime = 0;
  armDownStopTime = 0;
  gripperOpenStopTime = 0;
  gripperCloseStopTime = 0;
  armCwStopTime = 0;
  armCcwStopTime = 0;
  
  addToHistory("🚨 EMERGENCY: All systems stopped");
  server.send(200, "text/plain", "All systems stopped");
}

void handleGetStatus() {
  String status = "🤖 Robot: " + currentRobotAction + " | Mode: " + (robotLockMode ? "🔒 LOCK" : "🔓 UNLOCK") + "<br>";
  status += "🦾 Arm Duration: " + String(armMoveDuration) + "ms<br>";
  status += "🔩 Drill: " + String(drillOn ? "🟢 ON" : "⚪ OFF") + "<br>";
  status += "💡 Light: " + String(lightOn ? "🟢 ON" : "⚪ OFF") + "<br>";
  status += "🌡️ Temp: " + String(temperature, 1) + "°C<br>";
  status += "💧 Humidity: " + String(humidity, 1) + "%<br>";
  status += "⚠️ Gas Level: " + String(gasPercentage) + "% (" + getGasLevelText(gasValue) + ")<br>";
  status += "📊 Gas Raw: " + String(gasValue) + " (0=High, 4095=Low)<br>";
  server.send(200, "text/html", status);
}

void handleGetSensorData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"gasRaw\":" + String(gasValue) + ",";
  json += "\"gasPercentage\":" + String(gasPercentage) + ",";
  json += "\"gasLevelText\":\"" + getGasLevelText(gasValue) + "\",";
  json += "\"gasLevelColor\":\"" + getGasLevelColor(gasValue) + "\",";
  json += "\"gasLevelIcon\":\"" + getGasLevelIcon(gasValue) + "\",";
  json += "\"drillStatus\":" + String(drillOn ? "true" : "false") + ",";
  json += "\"lightStatus\":" + String(lightOn ? "true" : "false") + ",";
  json += "\"armMoveDuration\":" + String(armMoveDuration);
  json += "}";
  server.send(200, "application/json", json);
}

void handleGetHistory() {
  String history = "<div class='command-history'>";
  history += "<h6><i class='fas fa-history'></i> Recent Commands:</h6>";
  for (int i = 0; i < 10; i++) {
    int idx = (historyIndex - i - 1 + 10) % 10;
    if (commandHistory[idx].length() > 0) {
      history += "<div class='history-item'>" + commandHistory[idx] + "</div>";
    }
  }
  history += "</div>";
  server.send(200, "text/html", history);
}

// Handle arm duration setting
void handleSetArmDuration() {
  if (server.hasArg("duration")) {
    int newDuration = server.arg("duration").toInt();
    if (newDuration >= 500 && newDuration <= 5000) { // 0.5 to 5 seconds
      armMoveDuration = newDuration;
      server.send(200, "application/json", "{\"success\":true,\"duration\":" + String(armMoveDuration) + "}");
      addToHistory("⚙️ Arm duration set to " + String(armMoveDuration) + "ms");
      Serial.println("⚙️ Arm movement duration set to: " + String(armMoveDuration) + "ms");
    } else {
      server.send(400, "text/plain", "Invalid duration. Must be between 500 and 5000 ms");
    }
  } else {
    server.send(400, "text/plain", "Missing duration parameter");
  }
}

// -----------------------------------------------------------
// WEB PAGE GENERATION - AI BASED DRAINAGE CLEANING ROBOT
// -----------------------------------------------------------
String generateWebPage() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html lang='en'>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>🧠 AI BASED DRAINAGE CLEANING ROBOT | Control Center</title>
    <link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css' rel='stylesheet'>
    <link href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css' rel='stylesheet'>
    <link href='https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=Exo+2:wght@300;400;600&family=Share+Tech+Mono&family=Roboto+Mono:wght@300;400;500&display=swap' rel='stylesheet'>
    <style>
      :root {
        --ai-blue: #1a73e8;
        --ai-dark-blue: #0d47a1;
        --ai-light-blue: #4285f4;
        --ai-cyan: #00bcd4;
        --ai-teal: #20b2aa;
        --ai-dark: #0d1117;
        --ai-gray: #1c1e26;
        --ai-light: #e6f2ff;
        --danger-red: #ff3333;
        --safety-green: #00cc66;
        --warning-yellow: #ffcc00;
        --steel: #4d4d4d;
      }
      
      body {
        background: 
          linear-gradient(135deg, #0a1a2a 0%, #1a2a3a 50%, #2a3a4a 100%),
          repeating-linear-gradient(45deg, transparent, transparent 2px, rgba(26, 115, 232, 0.05) 2px, rgba(26, 115, 232, 0.05) 4px);
        min-height: 100vh;
        font-family: 'Exo 2', 'Roboto Mono', sans-serif;
        color: var(--ai-light);
        overflow-x: hidden;
        position: relative;
      }
      
      .ai-scanline {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 3px;
        background: linear-gradient(90deg, transparent, var(--ai-cyan), transparent);
        z-index: -1;
        animation: ai-scan 3s linear infinite;
        box-shadow: 0 0 15px var(--ai-cyan);
      }
      
      @keyframes ai-scan {
        0% { top: 0; opacity: 0.3; }
        50% { opacity: 1; }
        100% { top: 100%; opacity: 0.3; }
      }
      
      .ai-particles {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        pointer-events: none;
        z-index: -1;
      }
      
      .particle {
        position: absolute;
        width: 2px;
        height: 2px;
        background: var(--ai-cyan);
        border-radius: 50%;
        opacity: 0.5;
        animation: particle-float 20s infinite linear;
      }
      
      @keyframes particle-float {
        0% { transform: translateY(100vh) rotate(0deg); opacity: 0; }
        10% { opacity: 0.5; }
        90% { opacity: 0.5; }
        100% { transform: translateY(-100px) rotate(360deg); opacity: 0; }
      }
      
      .navbar-ai {
        background: linear-gradient(90deg, var(--ai-dark-blue) 0%, var(--ai-blue) 100%) !important;
        backdrop-filter: blur(10px);
        border-bottom: 3px solid var(--ai-cyan);
        box-shadow: 0 5px 25px rgba(0, 0, 0, 0.5);
        position: relative;
        overflow: hidden;
        padding: 15px 0;
      }
      
      .navbar-ai::before {
        content: '';
        position: absolute;
        top: 0;
        left: -100%;
        width: 100%;
        height: 2px;
        background: linear-gradient(90deg, transparent, var(--ai-light-blue), transparent);
        animation: slide 3s infinite;
      }
      
      @keyframes slide {
        0% { left: -100%; }
        100% { left: 100%; }
      }
      
      .navbar-brand-ai {
        font-family: 'Orbitron', sans-serif;
        font-weight: 900;
        font-size: 2.2rem;
        background: linear-gradient(45deg, var(--ai-light-blue), var(--ai-cyan), var(--ai-blue));
        -webkit-background-clip: text;
        -webkit-text-fill-color: transparent;
        text-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
        position: relative;
        padding: 10px 0;
      }
      
      .navbar-brand-ai::after {
        content: '';
        position: absolute;
        bottom: 0;
        left: 10%;
        width: 80%;
        height: 3px;
        background: linear-gradient(90deg, var(--ai-light-blue), var(--ai-cyan), var(--ai-blue));
        border-radius: 2px;
      }
      
      .ai-badge {
        background: linear-gradient(45deg, var(--ai-blue), var(--ai-cyan));
        color: white;
        padding: 4px 12px;
        border-radius: 20px;
        font-size: 0.8rem;
        font-weight: bold;
        letter-spacing: 1px;
      }
      
      .card-ai {
        background: linear-gradient(145deg, rgba(28, 30, 38, 0.95), rgba(13, 17, 23, 0.98));
        border: 2px solid var(--ai-blue);
        border-radius: 15px;
        box-shadow: 
          0 10px 30px rgba(0, 0, 0, 0.6),
          inset 0 1px 0 rgba(255, 255, 255, 0.1),
          0 0 20px rgba(26, 115, 232, 0.2);
        transition: all 0.3s ease;
        position: relative;
        overflow: hidden;
      }
      
      .card-ai::before {
        content: '';
        position: absolute;
        top: 0;
        left: 0;
        right: 0;
        height: 3px;
        background: linear-gradient(90deg, var(--ai-blue), var(--ai-cyan), var(--ai-light-blue));
      }
      
      .card-ai:hover {
        transform: translateY(-5px);
        box-shadow: 
          0 15px 40px rgba(0, 0, 0, 0.8),
          inset 0 1px 0 rgba(255, 255, 255, 0.2),
          0 0 30px rgba(26, 115, 232, 0.4);
        border-color: var(--ai-cyan);
      }
      
      .card-header-ai {
        background: linear-gradient(90deg, rgba(26, 115, 232, 0.3), rgba(0, 188, 212, 0.3));
        border-bottom: 2px solid var(--ai-blue);
        font-family: 'Orbitron', sans-serif;
        font-weight: 700;
        font-size: 1.2rem;
        color: var(--ai-light);
        text-transform: uppercase;
        letter-spacing: 1.5px;
        padding: 15px;
        position: relative;
      }
      
      .card-header-ai::after {
        content: '';
        position: absolute;
        bottom: -2px;
        left: 0;
        width: 100%;
        height: 2px;
        background: linear-gradient(90deg, var(--ai-blue), var(--ai-cyan), var(--ai-light-blue));
      }
      
      .btn-ai {
        background: linear-gradient(45deg, var(--ai-gray), var(--ai-dark));
        border: 2px solid var(--ai-blue);
        color: var(--ai-light);
        font-family: 'Orbitron', sans-serif;
        font-weight: 700;
        padding: 12px 24px;
        border-radius: 8px;
        letter-spacing: 1px;
        transition: all 0.3s ease;
        text-transform: uppercase;
        box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3);
      }
      
      .btn-ai:hover {
        background: linear-gradient(45deg, var(--ai-blue), var(--ai-cyan));
        border-color: var(--ai-cyan);
        transform: translateY(-3px);
        box-shadow: 0 8px 20px rgba(26, 115, 232, 0.4);
      }
      
      .btn-ai:active {
        transform: translateY(1px);
      }
      
      .btn-danger-ai {
        background: linear-gradient(45deg, #8B0000, var(--danger-red));
        border-color: var(--danger-red);
      }
      
      .btn-warning-ai {
        background: linear-gradient(45deg, #996600, var(--warning-yellow));
        border-color: var(--warning-yellow);
        color: #000;
      }
      
      .control-panel-ai {
        display: grid;
        grid-template-columns: repeat(3, 1fr);
        gap: 15px;
        max-width: 350px;
        margin: 0 auto;
      }
      
      .control-btn-ai {
        aspect-ratio: 1;
        border-radius: 12px;
        border: 3px solid var(--ai-blue);
        background: linear-gradient(145deg, var(--ai-gray), #2a2a3a);
        color: var(--ai-light);
        font-size: 1.8rem;
        transition: all 0.3s ease;
        display: flex;
        align-items: center;
        justify-content: center;
        cursor: pointer;
        position: relative;
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 5px 10px rgba(0, 0, 0, 0.3);
      }
      
      .control-btn-ai:hover {
        border-color: var(--ai-cyan);
        transform: translateY(-3px);
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 8px 15px rgba(0, 188, 212, 0.3);
      }
      
      .control-btn-ai:active {
        transform: translateY(1px);
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 3px 5px rgba(0, 0, 0, 0.3);
      }
      
      .control-btn-ai.active {
        background: linear-gradient(145deg, var(--ai-cyan), var(--ai-light-blue));
        border-color: var(--ai-cyan);
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 0 20px rgba(0, 188, 212, 0.5);
      }
      
      .btn-center-ai {
        background: linear-gradient(145deg, #8B0000, var(--danger-red));
        border-color: var(--danger-red);
      }
      
      .sensor-panel-ai {
        background: rgba(13, 17, 23, 0.9);
        border: 2px solid var(--ai-blue);
        border-radius: 12px;
        padding: 20px;
        margin-bottom: 20px;
        position: relative;
        transition: all 0.3s ease;
      }
      
      .sensor-panel-ai:hover {
        border-color: var(--ai-cyan);
        box-shadow: 0 0 20px rgba(0, 188, 212, 0.3);
      }
      
      .sensor-panel-ai::before {
        content: '';
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 3px;
        background: linear-gradient(90deg, var(--ai-blue), var(--ai-cyan));
      }
      
      .sensor-title-ai {
        font-family: 'Orbitron', sans-serif;
        font-size: 1rem;
        color: var(--ai-light);
        text-transform: uppercase;
        letter-spacing: 1px;
        margin-bottom: 15px;
        display: flex;
        align-items: center;
        gap: 10px;
      }
      
      .sensor-value-ai {
        font-family: 'Roboto Mono', monospace;
        font-size: 2rem;
        font-weight: bold;
        color: var(--ai-light);
        text-align: center;
        padding: 15px;
        border-radius: 8px;
        background: rgba(0, 0, 0, 0.3);
        border: 1px solid var(--ai-blue);
        margin: 10px 0;
        text-shadow: 0 0 10px currentColor;
      }
      
      .sensor-unit-ai {
        font-size: 1rem;
        color: var(--ai-cyan);
        margin-left: 5px;
      }
      
      .status-indicator-ai {
        display: inline-block;
        width: 12px;
        height: 12px;
        border-radius: 50%;
        margin-right: 8px;
        box-shadow: 0 0 10px;
      }
      
      .status-on-ai {
        background: var(--safety-green);
        box-shadow: 0 0 10px var(--safety-green);
        animation: pulse-green 2s infinite;
      }
      
      .status-off-ai {
        background: var(--steel);
        box-shadow: 0 0 5px var(--steel);
      }
      
      .status-warning-ai {
        background: var(--warning-yellow);
        box-shadow: 0 0 10px var(--warning-yellow);
        animation: pulse-yellow 2s infinite;
      }
      
      .status-danger-ai {
        background: var(--danger-red);
        box-shadow: 0 0 10px var(--danger-red);
        animation: pulse-red 2s infinite;
      }
      
      @keyframes pulse-green {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.5; }
      }
      
      @keyframes pulse-yellow {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.5; }
      }
      
      @keyframes pulse-red {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.5; }
      }
      
      .ai-alert {
        background: linear-gradient(90deg, var(--danger-red), var(--warning-yellow));
        color: #000;
        padding: 12px;
        border-radius: 8px;
        text-align: center;
        font-family: 'Orbitron', sans-serif;
        font-weight: bold;
        text-transform: uppercase;
        letter-spacing: 1px;
        margin: 15px 0;
        animation: alert-pulse 1s infinite;
        border: 2px solid var(--danger-red);
      }
      
      @keyframes alert-pulse {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.7; }
      }
      
      .ai-safe {
        background: linear-gradient(90deg, var(--safety-green), var(--ai-teal));
        color: white;
        padding: 12px;
        border-radius: 8px;
        text-align: center;
        font-family: 'Orbitron', sans-serif;
        font-weight: bold;
        text-transform: uppercase;
        letter-spacing: 1px;
        margin: 15px 0;
        border: 2px solid var(--safety-green);
      }
      
      .tool-control-ai {
        background: linear-gradient(145deg, rgba(28, 30, 38, 0.9), rgba(13, 17, 23, 0.95));
        border: 2px solid var(--ai-blue);
        border-radius: 12px;
        padding: 20px;
        margin-bottom: 20px;
        position: relative;
      }
      
      .tool-control-ai::before {
        content: '';
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 3px;
        background: linear-gradient(90deg, var(--ai-blue), var(--ai-cyan));
      }
      
      .tool-title-ai {
        font-family: 'Orbitron', sans-serif;
        font-size: 1.1rem;
        color: var(--ai-light);
        text-transform: uppercase;
        letter-spacing: 1px;
        margin-bottom: 15px;
        display: flex;
        align-items: center;
        justify-content: space-between;
      }
      
      .tool-toggle-ai {
        width: 80px;
        height: 80px;
        border-radius: 50%;
        border: 3px solid var(--ai-blue);
        background: linear-gradient(145deg, var(--ai-gray), #2a2a3a);
        color: var(--ai-light);
        font-size: 1.8rem;
        transition: all 0.3s ease;
        display: flex;
        align-items: center;
        justify-content: center;
        cursor: pointer;
        position: relative;
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 5px 10px rgba(0, 0, 0, 0.3);
      }
      
      .tool-toggle-ai.on {
        background: linear-gradient(145deg, var(--ai-cyan), var(--ai-light-blue));
        border-color: var(--ai-cyan);
        box-shadow: 
          inset 0 2px 4px rgba(0, 0, 0, 0.5),
          0 0 25px rgba(0, 188, 212, 0.6);
      }
      
      .tool-toggle-ai:hover {
        transform: scale(1.1);
        border-color: var(--ai-cyan);
      }
      
      .duration-control-ai {
        background: linear-gradient(145deg, rgba(28, 30, 38, 0.9), rgba(13, 17, 23, 0.95));
        border: 2px solid var(--ai-blue);
        border-radius: 12px;
        padding: 20px;
        margin-bottom: 20px;
      }
      
      .duration-title-ai {
        font-family: 'Orbitron', sans-serif;
        font-size: 1.1rem;
        color: var(--ai-light);
        text-transform: uppercase;
        letter-spacing: 1px;
        margin-bottom: 15px;
        display: flex;
        align-items: center;
        gap: 10px;
      }
      
      .duration-value-ai {
        font-family: 'Roboto Mono', monospace;
        font-size: 2.5rem;
        font-weight: bold;
        color: var(--ai-cyan);
        text-align: center;
        margin: 20px 0;
        text-shadow: 0 0 15px rgba(0, 188, 212, 0.7);
      }
      
      .duration-slider-ai {
        width: 100%;
        height: 25px;
        -webkit-appearance: none;
        background: linear-gradient(90deg, var(--ai-blue), var(--ai-cyan), var(--ai-light-blue));
        border-radius: 8px;
        outline: none;
        border: 2px solid var(--ai-blue);
        box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.5);
      }
      
      .duration-slider-ai::-webkit-slider-thumb {
        -webkit-appearance: none;
        width: 35px;
        height: 35px;
        border-radius: 50%;
        background: var(--ai-light);
        border: 3px solid var(--ai-cyan);
        cursor: pointer;
        box-shadow: 0 0 15px rgba(0, 188, 212, 0.8);
      }
      
      .duration-slider-ai::-moz-range-thumb {
        width: 35px;
        height: 35px;
        border-radius: 50%;
        background: var(--ai-light);
        border: 3px solid var(--ai-cyan);
        cursor: pointer;
        box-shadow: 0 0 15px rgba(0, 188, 212, 0.8);
      }
      
      .duration-presets-ai {
        display: flex;
        justify-content: space-between;
        margin-top: 15px;
        gap: 5px;
      }
      
      .duration-preset-btn-ai {
        flex: 1;
        padding: 10px 5px;
        background: rgba(26, 115, 232, 0.2);
        border: 1px solid var(--ai-blue);
        border-radius: 6px;
        color: var(--ai-light);
        font-family: 'Roboto Mono', monospace;
        font-size: 0.9rem;
        cursor: pointer;
        transition: all 0.3s ease;
        text-align: center;
      }
      
      .duration-preset-btn-ai:hover {
        background: rgba(26, 115, 232, 0.4);
        border-color: var(--ai-cyan);
        transform: translateY(-2px);
      }
      
      .duration-preset-btn-ai.active {
        background: linear-gradient(145deg, var(--ai-cyan), var(--ai-light-blue));
        border-color: var(--ai-cyan);
      }
      
      .arm-control-panel-ai {
        display: grid;
        grid-template-columns: repeat(2, 1fr);
        gap: 12px;
        margin-bottom: 20px;
      }
      
      .arm-btn-ai {
        padding: 20px 15px;
        background: linear-gradient(145deg, var(--ai-gray), #2a2a3a);
        border: 2px solid var(--ai-blue);
        border-radius: 10px;
        color: var(--ai-light);
        font-family: 'Orbitron', sans-serif;
        font-size: 1rem;
        text-align: center;
        cursor: pointer;
        transition: all 0.3s ease;
        text-transform: uppercase;
        letter-spacing: 1px;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: 8px;
      }
      
      .arm-btn-ai:hover {
        border-color: var(--ai-cyan);
        transform: translateY(-3px);
        box-shadow: 0 5px 15px rgba(0, 188, 212, 0.3);
      }
      
      .arm-btn-ai.active {
        background: linear-gradient(145deg, var(--ai-cyan), var(--ai-light-blue));
        border-color: var(--ai-cyan);
        box-shadow: 0 0 20px rgba(0, 188, 212, 0.5);
      }
      
      .arm-btn-label {
        font-size: 0.8rem;
        color: var(--ai-cyan);
        opacity: 0.9;
      }
      
      .notification-ai {
        position: fixed;
        bottom: 20px;
        right: 20px;
        background: linear-gradient(45deg, var(--ai-gray), var(--ai-blue));
        color: var(--ai-light);
        padding: 15px 25px;
        border-radius: 10px;
        font-family: 'Orbitron', sans-serif;
        font-weight: bold;
        z-index: 1000;
        transform: translateX(150%);
        transition: transform 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275);
        box-shadow: 0 5px 25px rgba(0, 0, 0, 0.5);
        border: 2px solid var(--ai-cyan);
        max-width: 400px;
      }
      
      .notification-ai.show {
        transform: translateX(0);
      }
      
      .connection-status-ai {
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 10px 20px;
        background: rgba(0, 0, 0, 0.3);
        border-radius: 8px;
        border: 1px solid var(--ai-blue);
      }
      
      .connection-dot-ai {
        width: 10px;
        height: 10px;
        border-radius: 50%;
      }
      
      .connection-good-ai {
        background: var(--safety-green);
        box-shadow: 0 0 10px var(--safety-green);
        animation: pulse-green 2s infinite;
      }
      
      .connection-poor-ai {
        background: var(--warning-yellow);
        box-shadow: 0 0 10px var(--warning-yellow);
        animation: pulse-yellow 2s infinite;
      }
      
      .connection-bad-ai {
        background: var(--danger-red);
        box-shadow: 0 0 10px var(--danger-red);
        animation: pulse-red 2s infinite;
      }
      
      .ai-terminal {
        background: rgba(0, 0, 0, 0.7);
        border: 2px solid var(--ai-blue);
        border-radius: 10px;
        padding: 20px;
        font-family: 'Roboto Mono', monospace;
        font-size: 0.9rem;
        color: var(--ai-cyan);
        line-height: 1.6;
        overflow: hidden;
        position: relative;
      }
      
      .ai-terminal::before {
        content: '';
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background: linear-gradient(transparent 50%, rgba(0, 188, 212, 0.05) 50%);
        background-size: 100% 4px;
        pointer-events: none;
      }
      
      .terminal-line {
        margin: 5px 0;
      }
      
      .terminal-prompt {
        color: var(--ai-cyan);
      }
      
      .terminal-text {
        color: var(--ai-light);
      }
      
      .terminal-value {
        color: var(--ai-blue);
        font-weight: bold;
      }
      
      @media (max-width: 768px) {
        .control-panel-ai {
          max-width: 280px;
          gap: 10px;
        }
        
        .control-btn-ai {
          font-size: 1.5rem;
        }
        
        .navbar-brand-ai {
          font-size: 1.5rem;
        }
        
        .arm-control-panel-ai {
          grid-template-columns: 1fr;
        }
        
        .duration-presets-ai {
          flex-wrap: wrap;
        }
        
        .duration-preset-btn-ai {
          min-width: 60px;
        }
      }
      
      /* Custom scrollbar */
      ::-webkit-scrollbar {
        width: 10px;
      }
      
      ::-webkit-scrollbar-track {
        background: var(--ai-dark);
        border-radius: 5px;
      }
      
      ::-webkit-scrollbar-thumb {
        background: linear-gradient(to bottom, var(--ai-blue), var(--ai-cyan));
        border-radius: 5px;
      }
      
      ::-webkit-scrollbar-thumb:hover {
        background: linear-gradient(to bottom, var(--ai-cyan), var(--ai-blue));
      }
    </style>
  </head>
  <body>
    
    <div class="ai-scanline"></div>
    
    <!-- AI Particles Background -->
    <div class="ai-particles" id="aiParticles"></div>
    
    <nav class="navbar navbar-ai navbar-expand-lg">
      <div class="container-fluid">
        <a class="navbar-brand-ai" href="#">
          <i class="fas fa-brain"></i> AI DRAINAGE CLEANING ROBOT
          <span class="ai-badge ms-2">v2.0</span>
        </a>
        <div class="d-flex align-items-center gap-3">
          <div class="connection-status-ai">
            <div class="connection-dot-ai connection-good-ai"></div>
            <span class="small">SYSTEM: ONLINE</span>
          </div>
          <div class="connection-status-ai">
            <i class="fas fa-wifi"></i>
            <span class="small">)rawliteral";
            
  page += WiFi.localIP().toString();
  page += R"rawliteral(</span>
          </div>
        </div>
      </div>
    </nav>

    <div class="container-fluid my-4">
      <div class="row g-4">
        
        <!-- Left Column - Movement & Duration -->
        <div class="col-lg-4 col-md-6">
          
          <!-- Robot Movement Control -->
          <div class="card-ai mb-4">
            <div class="card-header-ai">
              <i class="fas fa-tractor"></i> ROBOT NAVIGATION
              <span id="robotModeIndicator" class="badge bg-success">UNLOCKED</span>
            </div>
            <div class="card-body p-4">
              <div class="mb-4">
                <div class="control-panel-ai">
                  <div class="control-btn-ai" 
                       onmousedown="controlRobot('forward')" 
                       onmouseup="if(!robotLockMode) controlRobot('stop')"
                       ontouchstart="controlRobot('forward')" 
                       ontouchend="if(!robotLockMode) controlRobot('stop')">
                    <i class="fas fa-arrow-up"></i>
                  </div>
                  
                  <div class="control-btn-ai" 
                       onmousedown="controlRobot('left')" 
                       onmouseup="if(!robotLockMode) controlRobot('stop')"
                       ontouchstart="controlRobot('left')" 
                       ontouchend="if(!robotLockMode) controlRobot('stop')">
                    <i class="fas fa-arrow-left"></i>
                  </div>
                  
                  <div class="control-btn-ai btn-center-ai" onclick="controlRobot('stop')">
                    <i class="fas fa-stop-circle"></i>
                  </div>
                  
                  <div class="control-btn-ai" 
                       onmousedown="controlRobot('right')" 
                       onmouseup="if(!robotLockMode) controlRobot('stop')"
                       ontouchstart="controlRobot('right')" 
                       ontouchend="if(!robotLockMode) controlRobot('stop')">
                    <i class="fas fa-arrow-right"></i>
                  </div>
                  
                  <div class="control-btn-ai" 
                       onmousedown="controlRobot('backward')" 
                       onmouseup="if(!robotLockMode) controlRobot('stop')"
                       ontouchstart="controlRobot('backward')" 
                       ontouchend="if(!robotLockMode) controlRobot('stop')">
                    <i class="fas fa-arrow-down"></i>
                  </div>
                </div>
              </div>
              
              <div class="row g-3">
                <div class="col-6">
                  <button class="btn btn-ai w-100" id="robotLockBtn" onclick="setRobotMode('lock')">
                    <i class="fas fa-lock"></i> LOCK MODE
                  </button>
                </div>
                <div class="col-6">
                  <button class="btn btn-ai w-100" id="robotUnlockBtn" onclick="setRobotMode('unlock')">
                    <i class="fas fa-unlock"></i> UNLOCK MODE
                  </button>
                </div>
              </div>
            </div>
          </div>
          
          <!-- Arm Movement Duration Control -->
          <div class="duration-control-ai mb-4">
            <div class="duration-title-ai">
              <i class="fas fa-clock"></i> ARM MOVEMENT DURATION
              <span class="ai-badge" id="durationBadge">1000ms</span>
            </div>
            <div class="duration-value-ai" id="durationValue">1000 ms</div>
            <input type="range" min="500" max="5000" step="100" value="1000" 
                   class="duration-slider-ai" id="durationSlider" 
                   oninput="updateDurationValue(this.value)"
                   onchange="updateDuration(this.value)">
            
            <div class="duration-presets-ai">
              <button class="duration-preset-btn-ai" onclick="setDuration(500)">0.5s</button>
              <button class="duration-preset-btn-ai" onclick="setDuration(1000)">1.0s</button>
              <button class="duration-preset-btn-ai" onclick="setDuration(2000)">2.0s</button>
              <button class="duration-preset-btn-ai" onclick="setDuration(3000)">3.0s</button>
              <button class="duration-preset-btn-ai" onclick="setDuration(5000)">5.0s</button>
            </div>
            
            <div class="text-center mt-3 small text-muted">
              <i class="fas fa-info-circle"></i> Single press = Timed movement
            </div>
          </div>
          
        </div>
        
        <!-- Center Column - Arm Controls -->
        <div class="col-lg-4 col-md-6">
          
          <!-- Robotic Arm Control -->
          <div class="card-ai mb-4">
            <div class="card-header-ai">
              <i class="fas fa-robot"></i> ROBOTIC ARM CONTROL
              <span class="badge bg-info">TIMED</span>
            </div>
            <div class="card-body p-4">
              
              <!-- Arm Vertical Movement -->
              <div class="mb-4">
                <div class="sensor-title-ai">
                  <i class="fas fa-arrows-alt-v"></i> ARM VERTICAL
                </div>
                <div class="arm-control-panel-ai">
                  <button class="arm-btn-ai" id="armUpBtn" onclick="controlArm('up')">
                    <i class="fas fa-arrow-up"></i>
                    <span class="arm-btn-label">UP</span>
                  </button>
                  <button class="arm-btn-ai" id="armDownBtn" onclick="controlArm('down')">
                    <i class="fas fa-arrow-down"></i>
                    <span class="arm-btn-label">DOWN</span>
                  </button>
                </div>
              </div>
              
              <!-- Gripper Control -->
              <div class="mb-4">
                <div class="sensor-title-ai">
                  <i class="fas fa-hand-rock"></i> GRIPPER CONTROL
                </div>
                <div class="arm-control-panel-ai">
                  <button class="arm-btn-ai" id="gripperOpenBtn" onclick="controlGripper('open')">
                    <i class="fas fa-expand"></i>
                    <span class="arm-btn-label">OPEN</span>
                  </button>
                  <button class="arm-btn-ai" id="gripperCloseBtn" onclick="controlGripper('close')">
                    <i class="fas fa-compress"></i>
                    <span class="arm-btn-label">CLOSE</span>
                  </button>
                </div>
              </div>
              
              <!-- Arm Rotation -->
              <div class="mb-4">
                <div class="sensor-title-ai">
                  <i class="fas fa-redo"></i> ARM ROTATION
                </div>
                <div class="arm-control-panel-ai">
                  <button class="arm-btn-ai" id="armCwBtn" onclick="controlRotation('cw')">
                    <i class="fas fa-redo"></i>
                    <span class="arm-btn-label">CLOCKWISE</span>
                  </button>
                  <button class="arm-btn-ai" id="armCcwBtn" onclick="controlRotation('ccw')">
                    <i class="fas fa-undo"></i>
                    <span class="arm-btn-label">COUNTER-CW</span>
                  </button>
                </div>
              </div>
              
              <div class="alert alert-info text-center mt-3 border-ai">
                <i class="fas fa-clock"></i> Each press moves for <span id="currentDurationDisplay" class="fw-bold">1000ms</span>
                <div class="small mt-1">Auto-stops after duration completes</div>
              </div>
              
            </div>
          </div>
          
          <!-- AI System Terminal -->
          <div class="card-ai">
            <div class="card-header-ai">
              <i class="fas fa-terminal"></i> AI SYSTEM TERMINAL
            </div>
            <div class="card-body p-4">
              <div class="ai-terminal">
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">AI Drainage Robot v2.0</span>
                </div>
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">Status: </span><span class="terminal-value" id="terminalStatus">OPERATIONAL</span>
                </div>
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">Mode: </span><span class="terminal-value" id="terminalMode">UNLOCKED</span>
                </div>
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">Arm Duration: </span><span class="terminal-value" id="terminalDuration">1000ms</span>
                </div>
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">Gas Safety: </span><span class="terminal-value" id="terminalGas">CHECKING...</span>
                </div>
                <div class="terminal-line">
                  <span class="terminal-prompt">$></span> <span class="terminal-text">Last Update: </span><span class="terminal-value" id="terminalTime">--:--:--</span>
                </div>
              </div>
            </div>
          </div>
          
        </div>
        
        <!-- Right Column - Sensors & Tools -->
        <div class="col-lg-4">
          
          <!-- Environment Sensors -->
          <div class="card-ai mb-4">
            <div class="card-header-ai">
              <i class="fas fa-chart-line"></i> ENVIRONMENT MONITORING
            </div>
            <div class="card-body p-4">
              
              <!-- Temperature & Humidity -->
              <div class="row g-3 mb-4">
                <div class="col-6">
                  <div class="sensor-panel-ai">
                    <div class="sensor-title-ai">
                      <i class="fas fa-thermometer-half"></i> TEMPERATURE
                    </div>
                    <div class="sensor-value-ai" id="temperatureValue">
                      0.0<span class="sensor-unit-ai">°C</span>
                    </div>
                  </div>
                </div>
                <div class="col-6">
                  <div class="sensor-panel-ai">
                    <div class="sensor-title-ai">
                      <i class="fas fa-tint"></i> HUMIDITY
                    </div>
                    <div class="sensor-value-ai" id="humidityValue">
                      0.0<span class="sensor-unit-ai">%</span>
                    </div>
                  </div>
                </div>
              </div>
              
              <!-- Gas Sensor Display -->
              <div class="mb-4">
                <div class="sensor-title-ai mb-3">
                  <i class="fas fa-wind"></i> GAS CONCENTRATION MONITOR
                </div>
                
                <!-- Gas Level Status -->
                <div id="gasAlert" class="ai-alert mb-3">
                  <i class="fas fa-exclamation-triangle"></i> 
                  <span id="gasLevelText">DANGER: HIGH GAS CONCENTRATION</span>
                </div>
                
                <!-- Gas Percentage -->
                <div class="sensor-panel-ai mb-3">
                  <div class="sensor-title-ai">
                    <i class="fas fa-chart-pie"></i> GAS LEVEL
                  </div>
                  <div class="sensor-value-ai" id="gasPercentageValue">
                    0<span class="sensor-unit-ai">%</span>
                  </div>
                  <div class="text-center small mt-2">
                    <span id="gasInterpretation">0% = High Gas, 100% = Low Gas</span>
                  </div>
                </div>
                
                <!-- Raw Value & Status -->
                <div class="row g-3">
                  <div class="col-6">
                    <div class="sensor-panel-ai">
                      <div class="sensor-title-ai">
                        <i class="fas fa-microchip"></i> RAW VALUE
                      </div>
                      <div class="sensor-value-ai" id="gasRawValue">
                        0
                      </div>
                      <div class="text-center small mt-2">
                        <span id="gasRawInfo">0=High Gas, 4095=Low Gas</span>
                      </div>
                    </div>
                  </div>
                  <div class="col-6">
                    <div class="sensor-panel-ai">
                      <div class="sensor-title-ai">
                        <i class="fas fa-shield-alt"></i> SAFETY STATUS
                      </div>
                      <div class="d-flex align-items-center justify-content-center py-3">
                        <div class="status-indicator-ai me-2" id="gasStatusIndicator"></div>
                        <div>
                          <div id="gasSafetyText" class="fw-bold">CHECKING</div>
                          <div class="small" id="gasSafetyDetail">Initializing sensor...</div>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
                
                <!-- Gas Level Scale -->
                <div class="mt-4">
                  <div class="sensor-title-ai mb-2">
                    <i class="fas fa-ruler"></i> GAS LEVEL SCALE
                  </div>
                  <div class="progress" style="height: 25px; background: linear-gradient(90deg, #ff3333, #ffcc00, #00cc66); border-radius: 8px; overflow: hidden;">
                    <div class="progress-bar" role="progressbar" style="width: 0%; background: rgba(0,0,0,0.3);" 
                         id="gasProgressBar"></div>
                  </div>
                  <div class="d-flex justify-content-between mt-2">
                    <span class="small text-danger">DANGER<br>(0-33%)</span>
                    <span class="small text-warning">WARNING<br>(34-66%)</span>
                    <span class="small text-success">SAFE<br>(67-100%)</span>
                  </div>
                </div>
                
              </div>
              
            </div>
          </div>
          
          <!-- Tool Control -->
          <div class="card-ai">
            <div class="card-header-ai">
              <i class="fas fa-tools"></i> TOOL CONTROL SYSTEM
            </div>
            <div class="card-body p-4">
              
              <div class="row g-4 align-items-stretch">
                <!-- Drill Control -->
                <div class="col-md-6">
                  <div class="tool-control-ai h-100 d-flex flex-column">
                    <div class="tool-title-ai">
                      <div>
                        <i class="fas fa-screwdriver"></i> DRILL
                      </div>
                      <div class="d-flex align-items-center">
                        <span class="status-indicator-ai me-2" id="drillStatusIndicator"></span>
                        <span id="drillStatusText" class="small">OFF</span>
                      </div>
                    </div>
                    <div class="flex-grow-1 d-flex flex-column align-items-center justify-content-center">
                      <div class="tool-toggle-ai mb-3" id="drillToggleBtn" onclick="toggleDrill()">
                        <i class="fas fa-screwdriver"></i>
                      </div>
                      <div class="d-flex justify-content-center mt-auto">
                        <button class="btn btn-ai btn-sm me-1" onclick="controlDrill('on')">
                          <i class="fas fa-play"></i> ON
                        </button>
                        <button class="btn btn-ai btn-sm" onclick="controlDrill('off')">
                          <i class="fas fa-stop"></i> OFF
                        </button>
                      </div>
                    </div>
                  </div>
                </div>
                
                <!-- Light Control -->
                <div class="col-md-6">
                  <div class="tool-control-ai h-100 d-flex flex-column">
                    <div class="tool-title-ai">
                      <div>
                        <i class="fas fa-lightbulb"></i> LIGHT
                      </div>
                      <div class="d-flex align-items-center">
                        <span class="status-indicator-ai me-2" id="lightStatusIndicator"></span>
                        <span id="lightStatusText" class="small">OFF</span>
                      </div>
                    </div>
                    <div class="flex-grow-1 d-flex flex-column align-items-center justify-content-center">
                      <div class="tool-toggle-ai mb-3" id="lightToggleBtn" onclick="toggleLight()">
                        <i class="fas fa-lightbulb"></i>
                      </div>
                      <div class="d-flex justify-content-center mt-auto">
                        <button class="btn btn-ai btn-sm me-1" onclick="controlLight('on')">
                          <i class="fas fa-play"></i> ON
                        </button>
                        <button class="btn btn-ai btn-sm" onclick="controlLight('off')">
                          <i class="fas fa-stop"></i> OFF
                        </button>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
              
              <!-- Emergency Stop -->
              <div class="mt-4">
                <button class="btn btn-danger-ai w-100 py-3" onclick="controlAllStop()">
                  <i class="fas fa-skull-crossbones fa-2x"></i>
                  <div class="mt-2">EMERGENCY STOP</div>
                  <small class="d-block">All systems halt immediately</small>
                </button>
              </div>
              
            </div>
          </div>
          
        </div>
      </div>
      
      <!-- Bottom Bar - AI System Info -->
      <div class="row mt-4">
        <div class="col-12">
          <div class="card-ai">
            <div class="card-body p-4">
              <div class="row">
                <div class="col-md-6">
                  <h6><i class="fas fa-robot"></i> AI ROBOT FEATURES</h6>
                  <div class="small">
                    <div><i class="fas fa-brain"></i> Intelligent Navigation</div>
                    <div><i class="fas fa-shield-alt"></i> Environment Monitoring</div>
                    <div><i class="fas fa-clock"></i> Precise Timed Controls</div>
                    <div><i class="fas fa-tachometer-alt"></i> Real-time Sensors</div>
                    <div><i class="fas fa-wifi"></i> Remote Control</div>
                  </div>
                </div>
                <div class="col-md-6">
                  <h6><i class="fas fa-info-circle"></i> SYSTEM INFORMATION</h6>
                  <div class="small">
                    <div><i class="fas fa-robot"></i> AI Drainage Cleaning Robot</div>
                    <div><i class="fas fa-microchip"></i> ESP32 AI Controller</div>
                    <div><i class="fas fa-wifi"></i> IP: )rawliteral";
                    
  page += WiFi.localIP().toString();
  page += R"rawliteral(</div>
                    <div><i class="fas fa-clock"></i> Arm Duration: <span id="systemDuration">1000ms</span></div>
                    <div><i class="fas fa-code-branch"></i> Version: 2.0</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
    
    <!-- AI Notification System -->
    <div id="notification" class="notification-ai">
      <i class="fas fa-info-circle"></i> <span id="notificationText">AI System Initialized</span>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js"></script>
    <script>
      // Global variables
      let robotLockMode = false;
      let drillState = false;
      let lightState = false;
      let armMoveDuration = 1000; // Default 1 second
      
      // Initialize AI system
      function initAISystem() {
        // Create particles
        createParticles();
        
        // Initial updates
        updateSensorData();
        updateDurationDisplay();
        updateTerminal();
        
        // Start periodic updates
        setInterval(updateSensorData, 2000);
        setInterval(updateTerminal, 1000);
        
        // Show AI welcome message
        setTimeout(() => {
          showNotification('🧠 AI Drainage Cleaning Robot Online', 'ai');
        }, 1000);
        
        console.log('🤖 AI System Initialized');
      }
      
      // Create floating particles
      function createParticles() {
        const container = document.getElementById('aiParticles');
        const particleCount = 50;
        
        for (let i = 0; i < particleCount; i++) {
          const particle = document.createElement('div');
          particle.className = 'particle';
          
          // Random position
          particle.style.left = Math.random() * 100 + '%';
          particle.style.top = Math.random() * 100 + '%';
          
          // Random animation delay
          particle.style.animationDelay = Math.random() * 20 + 's';
          
          // Random size
          const size = Math.random() * 3 + 1;
          particle.style.width = size + 'px';
          particle.style.height = size + 'px';
          
          // Random opacity
          particle.style.opacity = Math.random() * 0.7 + 0.3;
          
          container.appendChild(particle);
        }
      }
      
      // Show notification
      function showNotification(message, type = 'info') {
        const notification = document.getElementById('notification');
        const text = document.getElementById('notificationText');
        
        notification.className = 'notification-ai';
        
        // Set colors based on type
        if (type === 'success') {
          notification.style.background = 'linear-gradient(45deg, var(--ai-gray), var(--safety-green))';
          notification.style.borderColor = 'var(--safety-green)';
        } else if (type === 'error') {
          notification.style.background = 'linear-gradient(45deg, var(--ai-gray), var(--danger-red))';
          notification.style.borderColor = 'var(--danger-red)';
        } else if (type === 'warning') {
          notification.style.background = 'linear-gradient(45deg, var(--ai-gray), var(--warning-yellow))';
          notification.style.borderColor = 'var(--warning-yellow)';
          text.style.color = '#000';
        } else if (type === 'ai') {
          notification.style.background = 'linear-gradient(45deg, var(--ai-blue), var(--ai-cyan))';
          notification.style.borderColor = 'var(--ai-cyan)';
        }
        
        text.textContent = message;
        notification.classList.add('show');
        
        setTimeout(() => {
          notification.classList.remove('show');
        }, 3000);
      }
      
      // Duration control functions
      function updateDurationValue(value) {
        document.getElementById('durationValue').textContent = value + ' ms';
        document.getElementById('currentDurationDisplay').textContent = value + 'ms';
        document.getElementById('durationBadge').textContent = value + 'ms';
      }
      
      function updateDuration(value) {
        fetch('/set_arm_duration?duration=' + value)
          .then(response => response.json())
          .then(data => {
            if (data.success) {
              armMoveDuration = data.duration;
              updateDurationDisplay();
              showNotification('Arm duration set to ' + data.duration + 'ms', 'success');
            }
          })
          .catch(error => {
            console.error('Duration update error:', error);
            showNotification('Failed to update duration', 'error');
          });
      }
      
      function setDuration(ms) {
        document.getElementById('durationSlider').value = ms;
        updateDurationValue(ms);
        updateDuration(ms);
        
        // Highlight active preset
        document.querySelectorAll('.duration-preset-btn-ai').forEach(btn => {
          btn.classList.remove('active');
        });
        event.target.classList.add('active');
      }
      
      function updateDurationDisplay() {
        document.getElementById('systemDuration').textContent = armMoveDuration + 'ms';
        document.getElementById('terminalDuration').textContent = armMoveDuration + 'ms';
      }
      
      // Update tool status displays
      function updateToolStatus() {
        // Drill status
        const drillIndicator = document.getElementById('drillStatusIndicator');
        const drillText = document.getElementById('drillStatusText');
        const drillToggle = document.getElementById('drillToggleBtn');
        
        if (drillState) {
          drillIndicator.className = 'status-indicator-ai status-on-ai';
          drillText.textContent = 'ON';
          drillText.className = 'small text-success';
          drillToggle.classList.add('on');
        } else {
          drillIndicator.className = 'status-indicator-ai status-off-ai';
          drillText.textContent = 'OFF';
          drillText.className = 'small';
          drillToggle.classList.remove('on');
        }
        
        // Light status
        const lightIndicator = document.getElementById('lightStatusIndicator');
        const lightText = document.getElementById('lightStatusText');
        const lightToggle = document.getElementById('lightToggleBtn');
        
        if (lightState) {
          lightIndicator.className = 'status-indicator-ai status-on-ai';
          lightText.textContent = 'ON';
          lightText.className = 'small text-warning';
          lightToggle.classList.add('on');
        } else {
          lightIndicator.className = 'status-indicator-ai status-off-ai';
          lightText.textContent = 'OFF';
          lightText.className = 'small';
          lightToggle.classList.remove('on');
        }
      }
      
      // Robot control
      function controlRobot(action) {
        fetch('/robot_' + action)
          .then(response => response.text())
          .then(data => {
            if (action !== 'stop') {
              showNotification('Navigation: ' + data, 'info');
            }
          })
          .catch(error => {
            console.error('Robot control error:', error);
            showNotification('Navigation failed', 'error');
          });
      }
      
      function setRobotMode(mode) {
        if (mode === 'lock') {
          robotLockMode = true;
          document.getElementById('robotModeIndicator').className = 'badge bg-danger';
          document.getElementById('robotModeIndicator').innerText = 'LOCKED';
          document.getElementById('terminalMode').textContent = 'LOCKED';
          fetch('/robot_lock');
          showNotification('Lock Mode Activated', 'warning');
        } else {
          robotLockMode = false;
          document.getElementById('robotModeIndicator').className = 'badge bg-success';
          document.getElementById('robotModeIndicator').innerText = 'UNLOCKED';
          document.getElementById('terminalMode').textContent = 'UNLOCKED';
          fetch('/robot_unlock');
          showNotification('Unlock Mode Activated', 'success');
        }
      }
      
      // Arm control functions (single press = timed movement)
      function controlArm(action) {
        fetch('/arm_' + action)
          .then(response => response.text())
          .then(data => {
            showNotification('Arm: ' + data, 'info');
            // Visual feedback
            const btn = document.getElementById(action === 'up' ? 'armUpBtn' : 'armDownBtn');
            btn.classList.add('active');
            setTimeout(() => btn.classList.remove('active'), armMoveDuration);
          })
          .catch(error => {
            console.error('Arm control error:', error);
            showNotification('Arm control failed', 'error');
          });
      }
      
      function controlGripper(action) {
        fetch('/gripper_' + action)
          .then(response => response.text())
          .then(data => {
            showNotification('Gripper: ' + data, 'info');
            // Visual feedback
            const btn = document.getElementById(action === 'open' ? 'gripperOpenBtn' : 'gripperCloseBtn');
            btn.classList.add('active');
            setTimeout(() => btn.classList.remove('active'), armMoveDuration);
          })
          .catch(error => {
            console.error('Gripper control error:', error);
            showNotification('Gripper control failed', 'error');
          });
      }
      
      function controlRotation(action) {
        fetch('/rotation_' + action)
          .then(response => response.text())
          .then(data => {
            showNotification('Rotation: ' + data, 'info');
            // Visual feedback
            const btn = document.getElementById(action === 'cw' ? 'armCwBtn' : 'armCcwBtn');
            btn.classList.add('active');
            setTimeout(() => btn.classList.remove('active'), armMoveDuration);
          })
          .catch(error => {
            console.error('Rotation control error:', error);
            showNotification('Rotation control failed', 'error');
          });
      }
      
      // Drill control
      function controlDrill(action) {
        fetch('/drill_' + action)
          .then(response => response.text())
          .then(data => {
            drillState = (action === 'on');
            updateToolStatus();
            showNotification(data, 'info');
          })
          .catch(error => {
            console.error('Drill control error:', error);
            showNotification('Drill control failed', 'error');
          });
      }
      
      function toggleDrill() {
        const action = drillState ? 'off' : 'on';
        controlDrill(action);
      }
      
      // Light control
      function controlLight(action) {
        fetch('/light_' + action)
          .then(response => response.text())
          .then(data => {
            lightState = (action === 'on');
            updateToolStatus();
            showNotification(data, 'info');
          })
          .catch(error => {
            console.error('Light control error:', error);
            showNotification('Light control failed', 'error');
          });
      }
      
      function toggleLight() {
        const action = lightState ? 'off' : 'on';
        controlLight(action);
      }
      
      // Emergency stop
      function controlAllStop() {
        fetch('/all_stop')
          .then(response => response.text())
          .then(data => {
            drillState = false;
            lightState = false;
            updateToolStatus();
            showNotification('🚨 ' + data, 'error');
          });
      }
      
      // Sensor data functions
      function updateSensorData() {
        fetch('/get_sensor_data')
          .then(response => response.json())
          .then(data => {
            // Update temperature
            document.getElementById('temperatureValue').innerHTML = 
              data.temperature + '<span class="sensor-unit-ai">°C</span>';
            
            // Update humidity
            document.getElementById('humidityValue').innerHTML = 
              data.humidity + '<span class="sensor-unit-ai">%</span>';
            
            // Update gas sensor data
            document.getElementById('gasPercentageValue').innerHTML = 
              data.gasPercentage + '<span class="sensor-unit-ai">%</span>';
            
            document.getElementById('gasRawValue').innerHTML = data.gasRaw;
            
            // Update gas safety status
            const gasAlert = document.getElementById('gasAlert');
            const gasLevelText = document.getElementById('gasLevelText');
            const gasSafetyText = document.getElementById('gasSafetyText');
            const gasSafetyDetail = document.getElementById('gasSafetyDetail');
            const gasStatusIndicator = document.getElementById('gasStatusIndicator');
            const gasProgressBar = document.getElementById('gasProgressBar');
            
            // Set colors and text based on gas level
            if (data.gasLevelText === 'DANGER') {
              gasAlert.className = 'ai-alert';
              gasAlert.innerHTML = '<i class="fas fa-skull-crossbones"></i> <span id="gasLevelText">DANGER: HIGH GAS CONCENTRATION</span>';
              gasSafetyText.textContent = 'DANGER';
              gasSafetyText.className = 'fw-bold text-danger';
              gasSafetyDetail.textContent = 'High gas levels detected';
              gasStatusIndicator.className = 'status-indicator-ai status-danger-ai';
              gasProgressBar.style.width = '33%';
            } else if (data.gasLevelText === 'WARNING') {
              gasAlert.className = 'ai-alert';
              gasAlert.innerHTML = '<i class="fas fa-exclamation-triangle"></i> <span id="gasLevelText">WARNING: MODERATE GAS LEVELS</span>';
              gasSafetyText.textContent = 'WARNING';
              gasSafetyText.className = 'fw-bold text-warning';
              gasSafetyDetail.textContent = 'Moderate gas levels';
              gasStatusIndicator.className = 'status-indicator-ai status-warning-ai';
              gasProgressBar.style.width = '66%';
            } else {
              gasAlert.className = 'ai-safe';
              gasAlert.innerHTML = '<i class="fas fa-check-circle"></i> <span id="gasLevelText">SAFE: LOW GAS CONCENTRATION</span>';
              gasSafetyText.textContent = 'SAFE';
              gasSafetyText.className = 'fw-bold text-success';
              gasSafetyDetail.textContent = 'Environment is safe';
              gasStatusIndicator.className = 'status-indicator-ai status-on-ai';
              gasProgressBar.style.width = '100%';
            }
            
            // Update gas interpretation
            document.getElementById('gasInterpretation').textContent = 
              `${data.gasPercentage}% Gas Concentration`;
            
            // Update terminal gas status
            document.getElementById('terminalGas').textContent = data.gasLevelText;
            document.getElementById('terminalGas').className = 
              data.gasLevelText === 'SAFE' ? 'terminal-value text-success' : 
              data.gasLevelText === 'WARNING' ? 'terminal-value text-warning' : 
              'terminal-value text-danger';
            
            // Update tool states
            drillState = data.drillStatus;
            lightState = data.lightStatus;
            updateToolStatus();
            
            // Update duration
            if (data.armMoveDuration !== armMoveDuration) {
              armMoveDuration = data.armMoveDuration;
              document.getElementById('durationSlider').value = armMoveDuration;
              updateDurationValue(armMoveDuration);
              updateDurationDisplay();
            }
            
          })
          .catch(error => {
            console.error('Sensor data error:', error);
          });
      }
      
      // Terminal update
      function updateTerminal() {
        const now = new Date();
        document.getElementById('terminalTime').textContent = 
          now.getHours().toString().padStart(2, '0') + ':' +
          now.getMinutes().toString().padStart(2, '0') + ':' +
          now.getSeconds().toString().padStart(2, '0');
        
        document.getElementById('terminalStatus').textContent = 'OPERATIONAL';
      }
      
      // Keyboard controls
      document.addEventListener('keydown', function(e) {
        const key = e.key.toLowerCase();
        
        // Prevent default for control keys
        if (['w', 'a', 's', 'd', ' ', '1', '2', 'q', 'l', 'u'].includes(key)) {
          e.preventDefault();
        }
        
        // Tool control
        if (key === '1') {
          toggleDrill();
        } else if (key === '2') {
          toggleLight();
        }
        
        // Robot movement
        switch(key) {
          case 'w': case 'arrowup': controlRobot('forward'); break;
          case 's': case 'arrowdown': controlRobot('backward'); break;
          case 'a': case 'arrowleft': controlRobot('left'); break;
          case 'd': case 'arrowright': controlRobot('right'); break;
          case ' ': case 'q': controlRobot('stop'); break;
          case 'l': setRobotMode('lock'); break;
          case 'u': setRobotMode('unlock'); break;
        }
      });
      
      document.addEventListener('keyup', function(e) {
        const key = e.key.toLowerCase();
        
        // Stop robot on key release (if not in lock mode)
        if (['w', 's', 'a', 'd', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright'].includes(key)) {
          if (!robotLockMode) {
            controlRobot('stop');
          }
        }
      });
      
      // Initialize AI system when page loads
      document.addEventListener('DOMContentLoaded', initAISystem);
    </script>
  </body>
  </html>
  )rawliteral";
  
  return page;
}

// -----------------------------------------------------------
void setup() {
  Serial.begin(115200);
  
  // Initialize motor pins
  pinMode(MOTOR1_PIN1, OUTPUT); pinMode(MOTOR1_PIN2, OUTPUT);
  pinMode(MOTOR2_PIN1, OUTPUT); pinMode(MOTOR2_PIN2, OUTPUT);
  
  // Initialize arm pins
  pinMode(ARM_UP_PIN, OUTPUT); pinMode(ARM_DOWN_PIN, OUTPUT);
  pinMode(GRIPPER_OPEN_PIN, OUTPUT); pinMode(GRIPPER_CLOSE_PIN, OUTPUT);
  pinMode(ARM_CW_PIN, OUTPUT); pinMode(ARM_CCW_PIN, OUTPUT);
  
  // Initialize tool pins (relays - LOW = ON)
  pinMode(DRILL_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  // Stop all motors and tools initially
  robotStop();
  armStop();
  gripperStop();
  armRotationStop();
  drillOffFunction();
  lightOffFunction();
  
  // Initialize command history
  for (int i = 0; i < 10; i++) commandHistory[i] = "";
  addToHistory("🚀 AI Drainage Cleaning Robot Initialized");
  addToHistory("🧠 AI System: Online");
  addToHistory("⚙️ Arm duration: " + String(armMoveDuration) + "ms");
  addToHistory("📡 Connecting to WiFi...");
  
  // Connect to WiFi with static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    addToHistory("❌ WiFi Configuration Failed");
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    addToHistory("✅ WiFi Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi Connection Failed!");
    addToHistory("❌ WiFi Connection Failed");
  }
  
  // Initial sensor reading
  updateSensors();
  
  // Define server routes
  server.on("/", []() { server.send(200, "text/html", generateWebPage()); });
  
  // Robot control routes
  server.on("/robot_forward", handleRobotForward);
  server.on("/robot_backward", handleRobotBackward);
  server.on("/robot_left", handleRobotLeft);
  server.on("/robot_right", handleRobotRight);
  server.on("/robot_stop", handleRobotStop);
  server.on("/robot_lock", handleRobotLock);
  server.on("/robot_unlock", handleRobotUnlock);
  
  // Arm control routes (single press = timed movement)
  server.on("/arm_up", handleArmUp);
  server.on("/arm_down", handleArmDown);
  server.on("/arm_stop", handleArmStop);
  
  // Gripper control routes
  server.on("/gripper_open", handleGripperOpen);
  server.on("/gripper_close", handleGripperClose);
  server.on("/gripper_stop", handleGripperStop);
  
  // Rotation control routes
  server.on("/rotation_cw", handleArmCw);
  server.on("/rotation_ccw", handleArmCcw);
  server.on("/rotation_stop", handleRotationStop);
  
  // Tool control routes
  server.on("/drill_on", []() { handleDrillControl("on"); });
  server.on("/drill_off", []() { handleDrillControl("off"); });
  server.on("/drill_toggle", []() { handleDrillControl("toggle"); });
  
  server.on("/light_on", []() { handleLightControl("on"); });
  server.on("/light_off", []() { handleLightControl("off"); });
  server.on("/light_toggle", []() { handleLightControl("toggle"); });
  
  // System routes
  server.on("/all_stop", handleAllStop);
  server.on("/get_status", handleGetStatus);
  server.on("/get_sensor_data", handleGetSensorData);
  server.on("/get_history", handleGetHistory);
  
  // Arm duration setting route
  server.on("/set_arm_duration", handleSetArmDuration);
  
  server.begin();
  Serial.println("=== AI DRAINAGE CLEANING ROBOT ===");
  Serial.println("🤖 AI Controller: ESP32");
  Serial.println("🌐 IP Address: http://" + WiFi.localIP().toString());
  Serial.println("⏱️ Arm Movement Duration: " + String(armMoveDuration) + "ms");
  Serial.println("🧠 AI System: Operational - Monitoring Drainage Environment");
  
  addToHistory("🌐 Web Server Started");
  addToHistory("🤖 AI System: Monitoring Active");
}

// -----------------------------------------------------------
void loop() {
  server.handleClient();
  
  // Check and stop timed arm movements
  checkArmTimers();
  
  // Periodically update sensors
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 2000) {
    updateSensors();
    lastSensorUpdate = millis();
  }
  
  delay(2);
}