#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Network & Hardware Settings ---
const char* ssid = "Astatine_Diagnostic"; 
const char* password = "";          
const byte DNS_PORT = 53; 

// Hardware Status LEDs (D5 & D6 on ESP8266)
const int RX_LED = 14; 
const int TX_LED = 12; 
unsigned long rxLedTime = 0;
unsigned long txLedTime = 0;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
DNSServer dnsServer; 

uint32_t currentBaudRate = 115200;

// --- High-Speed Logging Buffer ---
String logBuffer = "";
unsigned long lastLogFlush = 0;

// --- Display Auto-Detect Logic ---
enum DisplayType { NONE, OLED_DISPLAY, LCD_DISPLAY };
DisplayType activeDisplay = NONE;
LiquidCrystal_I2C* lcd = nullptr;
Adafruit_SSD1306* oled = nullptr;

String lastTx = "";
String lastRx = "";
unsigned long lastDisplayRefresh = 0;

// --- HTML & JavaScript (Astatine Advanced UI) ---
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Astatine Diagnostics</title>
  <style>
    :root { var(--bg): #0f172a; var(--card): #1e293b; var(--text): #f8fafc; var(--accent): #10b981; var(--border): #334155; }
    * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; margin: 0; padding: 15px; display: flex; justify-content: center; }
    .app-container { width: 100%; max-width: 800px; display: flex; flex-direction: column; gap: 15px; }
    .header { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #334155; }
    .brand { font-size: 1.5rem; font-weight: 700; color: #10b981; text-transform: uppercase; letter-spacing: 1px;}
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 15px; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); }
    .label-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; flex-wrap: wrap; gap: 10px; }
    .label { font-size: 0.8rem; color: #94a3b8; font-weight: 600; text-transform: uppercase; }
    
    textarea { width: 100%; height: 280px; background: #020617; color: #10b981; border: 1px solid #334155; border-radius: 8px; padding: 10px; font-family: 'Courier New', monospace; resize: none; outline: none; margin-bottom: 10px; box-shadow: inset 0 2px 4px rgba(0,0,0,0.5); }
    
    .input-group { display: flex; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
    input[type="text"], input[type="number"], select { flex: 1; min-width: 100px; background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 12px; border-radius: 8px; outline: none; appearance: none; }
    input:focus, select:focus { border-color: #10b981; }
    button { background: #10b981; color: #fff; border: none; padding: 12px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: 0.2s; white-space: nowrap; }
    button:hover { background: #059669; }
    button:active { transform: scale(0.98); }
    button.secondary { background: #334155; }
    button.secondary:hover { background: #475569; }
    
    .macro-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 8px; margin-bottom: 10px; }
    .macro-btn { background: #0f172a; border: 1px dashed #10b981; color: #10b981; padding: 10px; font-size: 0.85rem; border-radius: 8px; cursor: pointer; transition: 0.2s; }
    .macro-btn:hover { background: rgba(16, 185, 129, 0.1); }
    
    .toggle-container { display: flex; align-items: center; gap: 5px; font-size: 0.85rem; color: #94a3b8; }
    input[type="checkbox"] { width: 18px; height: 18px; accent-color: #10b981; cursor: pointer; }

    @media (max-width: 600px) {
      .input-group { flex-direction: column; }
      button { width: 100%; }
    }
  </style>
</head>
<body>

  <div class="app-container">
    <div class="header">
      <div class="brand">Astatine.</div>
      <div id="wsStatus" style="color: #ef4444; font-weight:bold; font-size: 0.85rem;">[ OFFLINE ]</div>
    </div>

    <div class="card">
      <div class="label-row">
        <span class="label">System Terminal Stream</span>
        <div class="toggle-container">
          <input type="checkbox" id="echoToggle" checked> <label for="echoToggle" style="margin-right: 12px; cursor: pointer;">Show TX Echo</label>
          <input type="checkbox" id="hexToggle"> <label for="hexToggle" style="cursor: pointer;">HEX View</label>
        </div>
      </div>
      <textarea id="rxConsole" readonly></textarea>
      
      <div class="input-group" style="justify-content: flex-end;">
        <button class="secondary" onclick="document.getElementById('rxConsole').value = ''">Clear Screen</button>
      </div>

      <div class="input-group">
        <select id="lineEnding" style="flex: 0.5;">
          <option value="">No Line Ending</option>
          <option value="\n">Newline (LF)</option>
          <option value="\r">Carriage (CR)</option>
          <option value="\r\n">Both (CRLF)</option>
        </select>
        <input type="text" id="cmdInput" placeholder="Command...">
        <button onclick="sendCommand()">Transmit</button>
      </div>

      <span class="label" style="display:block; margin: 15px 0 10px;">Programmable Macros</span>
      <div class="macro-grid" id="macroContainer">
        <button class="macro-btn" id="btn_m1" onclick="triggerMacro(1)" oncontextmenu="editMacro(event, 1)">M1 (Empty)</button>
        <button class="macro-btn" id="btn_m2" onclick="triggerMacro(2)" oncontextmenu="editMacro(event, 2)">M2 (Empty)</button>
        <button class="macro-btn" id="btn_m3" onclick="triggerMacro(3)" oncontextmenu="editMacro(event, 3)">M3 (Empty)</button>
        <button class="macro-btn" id="btn_m4" onclick="triggerMacro(4)" oncontextmenu="editMacro(event, 4)">M4 (Empty)</button>
      </div>
    </div>

    <div class="card">
      <span class="label" style="display:block; margin-bottom:10px;">Hardware Controls</span>
      <div class="input-group">
        <select id="baudPreset">
          <option value="" disabled selected>Syncing Hardware...</option>
          <option value="9600">9600 baud</option>
          <option value="19200">19200 baud</option>
          <option value="38400">38400 baud</option>
          <option value="57600">57600 baud</option>
          <option value="74880">74880 baud</option>
          <option value="115200">115200 baud</option>
          <option value="256000">256000 baud</option>
        </select>
        <input type="number" id="customBaud" placeholder="Custom baud">
        <button onclick="changeBaud()">Apply Baud</button>
      </div>
      
      <div class="input-group" style="margin-top: 15px;">
        <button class="secondary" onclick="scanI2C()" style="border: 1px solid #10b981; color: #10b981;">Scan I2C Bus</button>
        <button class="secondary" onclick="window.open('/log.txt', '_blank')">Download Log</button>
        <button class="secondary" onclick="clearLog()">Erase Flash</button>
      </div>
      <div style="font-size: 0.8rem; color: #94a3b8; text-align: right; margin-top:5px;" id="currentBaudDisplay">Fetching baud...</div>
    </div>
  </div>

  <script>
    var connection = new WebSocket('ws://' + window.location.hostname + ':81/');
    connection.binaryType = "arraybuffer"; 
    
    var rxConsole = document.getElementById('rxConsole');
    var statusText = document.getElementById('wsStatus');

    window.macroVal_1 = ""; window.macroVal_2 = ""; window.macroVal_3 = ""; window.macroVal_4 = "";

    connection.onopen = function () {
      statusText.innerText = "[ ONLINE ]"; statusText.style.color = "#10b981";
    };

    connection.onclose = function () {
      statusText.innerText = "[ OFFLINE ]"; statusText.style.color = "#ef4444";
    };

    connection.onmessage = function (event) {
      if (event.data instanceof ArrayBuffer) {
        let view = new Uint8Array(event.data);
        let hexMode = document.getElementById('hexToggle').checked;
        let str = "";
        for (let i = 0; i < view.length; i++) {
          if (hexMode) str += view[i].toString(16).padStart(2, '0').toUpperCase() + " ";
          else str += String.fromCharCode(view[i]);
        }
        rxConsole.value += str;
        rxConsole.scrollTop = rxConsole.scrollHeight;
      } 
      else if (typeof event.data === "string") {
        if (event.data.startsWith("CUR_BAUD:")) {
          let activeBaud = event.data.split(":")[1];
          document.getElementById('currentBaudDisplay').innerText = "Active Baud: " + activeBaud;
          
          let presetDropdown = document.getElementById('baudPreset');
          let customInput = document.getElementById('customBaud');
          let matchFound = false;
          
          for (let i = 0; i < presetDropdown.options.length; i++) {
            if (presetDropdown.options[i].value === activeBaud) {
              presetDropdown.selectedIndex = i; customInput.value = ""; matchFound = true; break;
            }
          }
          if (!matchFound) {
            presetDropdown.selectedIndex = 0; customInput.value = activeBaud;
          }
        }
        else if (event.data.startsWith("MACRO:")) {
          let contentStr = event.data.substring(6);
          let firstColon = contentStr.indexOf(":");
          let mId = contentStr.substring(0, firstColon);
          let mData = contentStr.substring(firstColon + 1);
          let separator = mData.indexOf("|");
          if(separator > -1) {
            let mName = mData.substring(0, separator);
            let mVal = mData.substring(separator + 1);
            window['macroVal_' + mId] = mVal;
            document.getElementById("btn_m" + mId).innerText = mName;
          }
        }
        else if (event.data.startsWith("SYS_MSG:")) {
           let sysMsg = event.data.substring(8);
           rxConsole.value += "\n\n" + sysMsg + "\n\n";
           rxConsole.scrollTop = rxConsole.scrollHeight;
        }
      }
    };

    function sendCommand(cmdOverride = null) {
      var cmd = cmdOverride !== null ? cmdOverride : document.getElementById('cmdInput').value;
      if(cmd !== "") {
        let suffix = document.getElementById('lineEnding').value;
        if(suffix === "\\n") suffix = "\n"; else if(suffix === "\\r") suffix = "\r"; else if(suffix === "\\r\\n") suffix = "\r\n";
        
        if(document.getElementById('echoToggle').checked) {
          rxConsole.value += "\n\n[TX] " + cmd + "\n";
          rxConsole.scrollTop = rxConsole.scrollHeight;
        }
        connection.send("CMD:" + cmd + suffix);
        document.getElementById('cmdInput').value = "";
      }
    }

    function triggerMacro(id) {
      let val = window['macroVal_' + id];
      if(val !== "") sendCommand(val);
      else editMacro(null, id); 
    }

    function editMacro(e, id) {
      if(e) e.preventDefault(); 
      let btn = document.getElementById("btn_m" + id);
      let currentName = btn.innerText.includes("(Empty)") ? ("M" + id) : btn.innerText;
      let currentVal = window['macroVal_' + id];
      
      let name = prompt("Name this Macro button:", currentName);
      if(name === null) return;
      let val = prompt("Enter the exact TX command payload:", currentVal);
      if(val === null) return;
      
      connection.send("SET_MACRO:" + id + ":" + name + "|" + val);
    }

    function changeBaud() {
      var newBaud = document.getElementById('customBaud').value || document.getElementById('baudPreset').value;
      if(newBaud) connection.send("BAUD:" + newBaud);
    }
    
    function scanI2C() {
      rxConsole.value += "\n[SYSTEM] Probing I2C bus for devices...\n";
      rxConsole.scrollTop = rxConsole.scrollHeight;
      connection.send("SYS:I2C_SCAN");
    }

    function clearLog() {
      if(confirm("Permanently erase the flash memory log?")) {
        fetch('/clear_log', {method: 'POST'}).then(res => { if(res.ok) alert("Log Erased"); });
      }
    }
    
    document.getElementById('baudPreset').addEventListener('change', function() {
       document.getElementById('customBaud').value = "";
    });
    
    // Allow pressing Enter to send command
    document.getElementById("cmdInput").addEventListener("keypress", function(event) {
      if (event.key === "Enter") {
        event.preventDefault();
        sendCommand();
      }
    });
  </script>

</body>
</html>
)=====";

// --- Helper Functions to Format Display Strings ---
void setTxBuffer(String txData) {
  lastTx = "";
  for(size_t i=0; i<txData.length(); i++) {
    char c = txData.charAt(i);
    if(c >= 32 && c <= 126) lastTx += c; 
  }
  if(lastTx.length() > 12) lastTx = lastTx.substring(lastTx.length() - 12);
}

void appendRxBuffer(uint8_t *payload, size_t length) {
  for(size_t i=0; i<length; i++) {
    char c = (char)payload[i];
    if(c >= 32 && c <= 126) lastRx += c;
  }
  if(lastRx.length() > 12) lastRx = lastRx.substring(lastRx.length() - 12);
}

// --- Expanded I2C Device Name Database ---
String getI2CDeviceName(byte address) {
  switch(address) {
    case 0x08: case 0x09: return "MLX90614 (IR Temp) / AMG8833";
    case 0x10: case 0x11: case 0x12: return "VEML6070 (UV Sensor)";
    case 0x18: case 0x19: return "LIS3DH (Accel) / MCP9808 (Temp)";
    case 0x1D: case 0x1E: return "ADXL345 (Accel) / HMC5883L (Compass)";
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: 
               return "PCF8574 / MCP23017 (IO Expander) / BH1750 (Light)";
    case 0x27: return "LCD I2C Backpack / PCF8574";
    case 0x28: case 0x29: return "BNO055 (IMU) / VL53L0X (ToF) / TSL2591";
    case 0x38: case 0x39: return "AHT20 (Temp/Hum) / APDS9960 (Gesture) / VEML6040";
    case 0x3C: return "SSD1306/SH1106 (OLED 128x64/32)";
    case 0x3D: return "SSD1306 (OLED Alt)";
    case 0x3F: return "LCD I2C Backpack (Alt)";
    case 0x40: return "PCA9685 (PWM) / INA219 / HTU21D / SI7021";
    case 0x41: case 0x42: case 0x43: return "INA219 (Current) Alt";
    case 0x44: case 0x45: return "SHT31/SHT35 (Temp/Hum)";
    case 0x48: case 0x49: case 0x4A: case 0x4B: return "ADS1115 (ADC) / TMP102 (Temp)";
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: 
               return "AT24C32 / AT24C256 (EEPROM) / RTC NVRAM";
    case 0x5A: return "CCS811 (Gas) / MLX90614 (Alt)";
    case 0x60: case 0x61: case 0x62: return "MCP4725 (DAC)";
    case 0x68: return "MPU6050 (Gyro) / DS3231 (RTC)";
    case 0x69: return "MPU6050 (Alt) / BMM150";
    case 0x76: return "BME280/BMP280 (Temp/Pres/Hum)";
    case 0x77: return "BME280/BMP280 (Alt) / BMP180";
    default: return "Unknown / Custom Device";
  }
}

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    String baudMsg = "CUR_BAUD:" + String(currentBaudRate);
    webSocket.sendTXT(num, baudMsg);
    
    // Fetch and send macros from LittleFS
    for(int i=1; i<=4; i++) {
      String filename = "/m" + String(i) + ".txt";
      if(LittleFS.exists(filename)) {
        File f = LittleFS.open(filename, "r");
        String content = f.readString();
        f.close();
        String macroMsg = "MACRO:" + String(i) + ":" + content;
        webSocket.sendTXT(num, macroMsg);
      }
    }
  }
  else if (type == WStype_TEXT) {
    String msg = (char*)payload;
    
    if (msg.startsWith("CMD:")) {
      String commandToTx = msg.substring(4); 
      Serial.print(commandToTx); 
      setTxBuffer(commandToTx); 
      digitalWrite(TX_LED, HIGH);
      txLedTime = millis();
      
      // Store TX commands in the log buffer
      logBuffer += "\n[TX] " + commandToTx + "\n";
    } 
    else if (msg.startsWith("BAUD:")) {
      uint32_t newBaud = msg.substring(5).toInt();
      if (newBaud > 0) {
        currentBaudRate = newBaud;
        EEPROM.write(0, 0xAA); 
        EEPROM.put(1, currentBaudRate);
        EEPROM.commit();
        Serial.flush();
        Serial.begin(currentBaudRate);
        
        String baudUpdateMsg = "CUR_BAUD:" + String(currentBaudRate);
        webSocket.broadcastTXT(baudUpdateMsg);
        
        logBuffer += "\n=== BAUD RATE CHANGED TO " + String(currentBaudRate) + " ===\n";
      }
    }
    else if (msg.startsWith("SET_MACRO:")) {
      int firstColon = msg.indexOf(':', 10);
      if (firstColon > 0) {
        String idStr = msg.substring(10, firstColon);
        String content = msg.substring(firstColon + 1);
        String filename = "/m" + idStr + ".txt";
        File f = LittleFS.open(filename, "w");
        if (f) { f.print(content); f.close(); }
        String macroUpdateMsg = "MACRO:" + idStr + ":" + content;
        webSocket.broadcastTXT(macroUpdateMsg);
      }
    }
    else if (msg.startsWith("SYS:I2C_SCAN")) {
      String scanResult = "[I2C SCAN RESULTS]\n";
      int nDevices = 0;
      
      for(byte address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
          scanResult += " -> Address: 0x";
          if (address < 16) scanResult += "0";
          scanResult += String(address, HEX);
          
          scanResult += " [Likely: " + getI2CDeviceName(address) + "]\n";
          
          // --- THE BLIND DATA PROBE ---
          Wire.requestFrom((uint8_t)address, (size_t)1);
          if (Wire.available()) {
            byte rawData = Wire.read();
            scanResult += "      Raw Probe Data: 0x";
            if (rawData < 16) scanResult += "0";
            scanResult += String(rawData, HEX) + "\n";
          } else {
            scanResult += "      Raw Probe Data: N/A (Requires Register Pointer)\n";
          }
          nDevices++;
        }
      }
      
      if (nDevices == 0) scanResult += " -> No I2C devices found on bus.\n";
      scanResult += "[SCAN COMPLETE]";

      String wsPayload = "SYS_MSG:" + scanResult;
      webSocket.broadcastTXT(wsPayload);
      logBuffer += "\n" + scanResult + "\n";
    }
  }
}

void setup() {
  pinMode(RX_LED, OUTPUT);
  pinMode(TX_LED, OUTPUT);
  digitalWrite(RX_LED, LOW);
  digitalWrite(TX_LED, LOW);

  EEPROM.begin(512);
  if (EEPROM.read(0) == 0xAA) {
    EEPROM.get(1, currentBaudRate);
    if(currentBaudRate < 300 || currentBaudRate > 2000000) currentBaudRate = 115200;
  }
  Serial.begin(currentBaudRate);

  // --- Display & I2C Setup ---
  Wire.begin(); 
  for(uint8_t i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      if (i == 0x3C || i == 0x3D) { 
        activeDisplay = OLED_DISPLAY;
        oled = new Adafruit_SSD1306(128, 32, &Wire, -1);
        oled->begin(SSD1306_SWITCHCAPVCC, i);
        oled->clearDisplay(); oled->setTextColor(WHITE); oled->setTextSize(1);
        oled->setCursor(0,0); oled->println("ASTATINE TECH");
        oled->println("Terminal Boot..."); oled->display();
        break;
      }
      else if (i == 0x27 || i == 0x3F) { 
        activeDisplay = LCD_DISPLAY;
        lcd = new LiquidCrystal_I2C(i, 16, 2);
        lcd->init(); lcd->backlight();
        lcd->setCursor(0,0); lcd->print("ASTATINE TECH");
        lcd->setCursor(0,1); lcd->print("Terminal Boot...");
        break;
      }
    }
  }

  LittleFS.begin();
  
  // Imprint a Boot Marker in the log file
  File f = LittleFS.open("/log.txt", "a");
  if(f) {
    f.print("\n\n=== DEVICE BOOTED (" + String(currentBaudRate) + " BAUD) ===\n\n");
    f.close();
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  ArduinoOTA.setHostname("Astatine-Terminal");
  ArduinoOTA.begin();

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  
  server.on("/log.txt", [](){
    // Flush any pending memory to flash right before downloading
    if (logBuffer.length() > 0) {
      File file = LittleFS.open("/log.txt", "a");
      if(file) { file.print(logBuffer); file.close(); }
      logBuffer = "";
    }
    
    if (LittleFS.exists("/log.txt")) {
      File file = LittleFS.open("/log.txt", "r");
      server.streamFile(file, "text/plain"); file.close();
    } else {
      server.send(404, "text/plain", "Log is currently empty.");
    }
  });

  server.on("/clear_log", HTTP_POST, [](){
    LittleFS.remove("/log.txt"); 
    logBuffer = "";
    server.send(200, "text/plain", "Cleared");
  });

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  ArduinoOTA.handle(); 
  dnsServer.processNextRequest();
  webSocket.loop();
  server.handleClient();

  if (millis() - rxLedTime > 30) digitalWrite(RX_LED, LOW);
  if (millis() - txLedTime > 30) digitalWrite(TX_LED, LOW);

  // --- Read RX Data ---
  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    
    webSocket.broadcastBIN(sbuf, len);
    appendRxBuffer(sbuf, len); 

    digitalWrite(RX_LED, HIGH);
    rxLedTime = millis();

    // Accumulate data in the high-speed RAM buffer
    for(size_t i=0; i<len; i++) {
      logBuffer += (char)sbuf[i];
    }
  }

  // --- Safe Flash Write Logic ---
  // Flush RAM buffer if it exceeds 512 bytes or 2 seconds pass
  if (logBuffer.length() > 512 || (logBuffer.length() > 0 && millis() - lastLogFlush > 2000)) {
    File f = LittleFS.open("/log.txt", "a");
    if(f) { 
      f.print(logBuffer); 
      f.close(); 
    }
    logBuffer = ""; 
    lastLogFlush = millis();
  }

  // --- Asynchronous Hardware Display Refresh ---
  if (activeDisplay != NONE && (millis() - lastDisplayRefresh > 250)) {
    lastDisplayRefresh = millis();
    
    if (activeDisplay == OLED_DISPLAY) {
      oled->clearDisplay();
      oled->setCursor(0, 0); oled->print("ASTATINE: "); oled->print(currentBaudRate);
      oled->setCursor(0, 12); oled->print("TX:" + lastTx);
      oled->setCursor(0, 24); oled->print("RX:" + lastRx);
      oled->display();
    } 
    else if (activeDisplay == LCD_DISPLAY) {
      lcd->setCursor(0, 0); lcd->print("TX:" + lastTx);
      for(int i = lastTx.length(); i < 13; i++) lcd->print(" "); 
      lcd->setCursor(0, 1); lcd->print("RX:" + lastRx);
      for(int i = lastRx.length(); i < 13; i++) lcd->print(" "); 
    }
  }
}
