#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

// --- Network & Hardware Settings ---
const char* ssid = "Astatine_Diagnostic"; 
const char* password = "";          
const byte DNS_PORT = 53; 

// Hardware Status LEDs
const int RX_LED = 2; // GPIO4 (D2)
const int TX_LED = 5; // GPIO5 (D1)
unsigned long rxLedTime = 0;
unsigned long txLedTime = 0;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
DNSServer dnsServer; 

uint32_t currentBaudRate = 115200;

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
    .brand { font-size: 1.5rem; font-weight: 700; color: #10b981; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 15px; }
    .label-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; flex-wrap: wrap; gap: 10px; }
    .label { font-size: 0.8rem; color: #94a3b8; font-weight: 600; text-transform: uppercase; }
    
    textarea { width: 100%; height: 280px; background: #020617; color: #10b981; border: 1px solid #334155; border-radius: 8px; padding: 10px; font-family: 'Courier New', monospace; resize: none; outline: none; margin-bottom: 10px; }
    
    .input-group { display: flex; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
    input[type="text"], input[type="number"], select { flex: 1; min-width: 100px; background: #0f172a; color: #f8fafc; border: 1px solid #334155; padding: 12px; border-radius: 8px; outline: none; appearance: none; }
    button { background: #10b981; color: #fff; border: none; padding: 12px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: 0.2s; white-space: nowrap; }
    button:hover { background: #059669; }
    button.secondary { background: #334155; }
    button.secondary:hover { background: #475569; }
    
    .macro-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 8px; margin-bottom: 10px; }
    .macro-btn { background: #0f172a; border: 1px dashed #10b981; color: #10b981; padding: 10px; font-size: 0.85rem; border-radius: 8px; cursor: pointer; }
    
    .toggle-container { display: flex; align-items: center; gap: 5px; font-size: 0.85rem; color: #94a3b8; }
    input[type="checkbox"] { width: 18px; height: 18px; accent-color: #10b981; cursor: pointer; }

    @media (max-width: 600px) {
      .input-group { flex-direction: column; }
      button { width: 100%; }
    }
  </style>
</head>
<body onload="initApp()">

  <div class="app-container">
    <div class="header">
      <div class="brand">Astatine.</div>
      <div id="wsStatus" style="color: #ef4444; font-weight:bold; font-size: 0.85rem;">[ OFFLINE ]</div>
    </div>

    <div class="card">
      <div class="label-row">
        <span class="label">UART Data Stream</span>
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
      <div class="macro-grid" id="macroContainer"></div>
    </div>

    <div class="card">
      <span class="label" style="display:block; margin-bottom:10px;">Hardware Controls</span>
      <div class="input-group">
        <select id="baudPreset">
          <option value="9600">9600 baud</option>
          <option value="115200">115200 baud</option>
          <option value="256000">256000 baud</option>
        </select>
        <input type="number" id="customBaud" placeholder="Custom baud">
        <button onclick="changeBaud()">Apply Baud</button>
      </div>
      
      <div class="input-group" style="margin-top: 15px;">
        <button class="secondary" onclick="window.open('/log.txt', '_blank')">Download On-Board Log</button>
        <button class="secondary" onclick="clearLog()">Erase Flash Log</button>
      </div>
      <div style="font-size: 0.8rem; color: #94a3b8; text-align: right; margin-top:5px;" id="currentBaudDisplay">Fetching baud...</div>
    </div>
  </div>

  <script>
    var connection = new WebSocket('ws://' + window.location.hostname + ':81/');
    connection.binaryType = "arraybuffer"; 
    
    var rxConsole = document.getElementById('rxConsole');
    var statusText = document.getElementById('wsStatus');

    function initApp() {
      let grid = document.getElementById('macroContainer');
      for(let i=1; i<=4; i++) {
        let name = localStorage.getItem('m_name_'+i) || ('M'+i+ ' (Empty)');
        grid.innerHTML += `<button class="macro-btn" onclick="triggerMacro(${i})" oncontextmenu="editMacro(event, ${i})">${name}</button>`;
      }
    }

    connection.onopen = function () {
      statusText.innerText = "[ ONLINE ]";
      statusText.style.color = "#10b981";
    };

    connection.onclose = function () {
      statusText.innerText = "[ OFFLINE ]";
      statusText.style.color = "#ef4444";
    };

    connection.onmessage = function (event) {
      if (event.data instanceof ArrayBuffer) {
        let view = new Uint8Array(event.data);
        let hexMode = document.getElementById('hexToggle').checked;
        let str = "";
        
        for (let i = 0; i < view.length; i++) {
          if (hexMode) {
            str += view[i].toString(16).padStart(2, '0').toUpperCase() + " ";
          } else {
            str += String.fromCharCode(view[i]);
          }
        }
        rxConsole.value += str;
        rxConsole.scrollTop = rxConsole.scrollHeight;
      } 
      else if (typeof event.data === "string") {
        if (event.data.startsWith("CUR_BAUD:")) {
          document.getElementById('currentBaudDisplay').innerText = "Active Baud: " + event.data.split(":")[1];
        }
      }
    };

    function sendCommand(cmdOverride = null) {
      var cmd = cmdOverride !== null ? cmdOverride : document.getElementById('cmdInput').value;
      if(cmd !== "") {
        let suffix = document.getElementById('lineEnding').value;
        if(suffix === "\\n") suffix = "\n";
        else if(suffix === "\\r") suffix = "\r";
        else if(suffix === "\\r\\n") suffix = "\r\n";
        
        // --- TX LOCAL ECHO LOGIC ---
        if(document.getElementById('echoToggle').checked) {
          // Add spacing and [TX] prefix to make it clearly stand out from raw RX stream
          rxConsole.value += "\n\n[TX] " + cmd + "\n";
          rxConsole.scrollTop = rxConsole.scrollHeight;
        }
        
        connection.send("CMD:" + cmd + suffix);
        document.getElementById('cmdInput').value = "";
      }
    }

    function triggerMacro(id) {
      let val = localStorage.getItem('m_val_'+id);
      if(val) sendCommand(val);
      else editMacro(null, id); 
    }

    function editMacro(e, id) {
      if(e) e.preventDefault(); 
      let currentVal = localStorage.getItem('m_val_'+id) || "";
      let currentName = localStorage.getItem('m_name_'+id) || "M" + id;
      
      let name = prompt("Name this Macro button:", currentName);
      if(name === null) return;
      let val = prompt("Enter the exact TX command payload:", currentVal);
      if(val === null) return;
      
      localStorage.setItem('m_name_'+id, name);
      localStorage.setItem('m_val_'+id, val);
      location.reload(); 
    }

    function changeBaud() {
      var newBaud = document.getElementById('customBaud').value || document.getElementById('baudPreset').value;
      if(newBaud) connection.send("BAUD:" + newBaud);
    }

    function clearLog() {
      if(confirm("Permanently erase the flash memory log?")) {
        fetch('/clear_log', {method: 'POST'})
          .then(res => { if(res.ok) alert("Log Erased"); });
      }
    }
  </script>

</body>
</html>
)=====";

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    String baudMsg = "CUR_BAUD:" + String(currentBaudRate);
    webSocket.sendTXT(num, baudMsg);
  }
  else if (type == WStype_TEXT) {
    String msg = (char*)payload;
    if (msg.startsWith("CMD:")) {
      String commandToTx = msg.substring(4); 
      Serial.print(commandToTx); 
      
      // Flash TX LED
      digitalWrite(TX_LED, HIGH);
      txLedTime = millis();
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
      }
    }
  }
}

void setup() {
  // LED Setup
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

  // File System Setup
  LittleFS.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // --- OTA (Over The Air) Updates Setup ---
  ArduinoOTA.setHostname("Astatine-Terminal");
  ArduinoOTA.begin();

  // --- Web Routes ---
  server.on("/", []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });
  
  // Endpoint to download the log file
  server.on("/log.txt", [](){
    if (LittleFS.exists("/log.txt")) {
      File f = LittleFS.open("/log.txt", "r");
      server.streamFile(f, "text/plain");
      f.close();
    } else {
      server.send(404, "text/plain", "Log is currently empty.");
    }
  });

  // Endpoint to clear the log file
  server.on("/clear_log", HTTP_POST, [](){
    LittleFS.remove("/log.txt");
    server.send(200, "text/plain", "Cleared");
  });

  // Captive Portal Redirect
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  ArduinoOTA.handle(); // Listen for wireless firmware flashes
  dnsServer.processNextRequest();
  webSocket.loop();
  server.handleClient();

  // Turn off LEDs rapidly to create a visual "blink" effect
  if (millis() - rxLedTime > 30) digitalWrite(RX_LED, LOW);
  if (millis() - txLedTime > 30) digitalWrite(TX_LED, LOW);

  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    
    // Broadcast binary to websocket for accurate Hex rendering
    webSocket.broadcastBIN(sbuf, len);

    // Flash RX LED
    digitalWrite(RX_LED, HIGH);
    rxLedTime = millis();

    // Append to On-Board Flash Memory
    File f = LittleFS.open("/log.txt", "a");
    if(f) {
      f.write(sbuf, len);
      f.close();
    }
  }
}