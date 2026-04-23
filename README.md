Astatine Wireless UART Diagnostic Terminal
A high-performance, ESP8266-based wireless serial monitor and diagnostic tool. This firmware transforms an ESP8266 into a standalone Wi-Fi access point featuring a Captive Portal and a real-time, WebSocket-driven web dashboard. It is designed for rapid debugging of microcontrollers, IoT hardware, and serial devices without needing physical USB connections to a PC.

🚀 Key Features
Zero-Delay WebSockets: Utilizes raw binary WebSockets for instant, low-latency bidirectional communication between the browser and the target hardware.

Captive Portal: No IP addresses to remember. Connecting to the device's Wi-Fi network automatically launches the Astatine Terminal on iOS, Android, or Windows.

Modern App UI: A fully responsive, mobile-optimized dark mode interface built with native CSS flexbox and touch-friendly controls.

Advanced Data Analysis:

Hex View: Toggle between standard ASCII text and raw Hexadecimal bytes for debugging non-printable sensor data.

Local TX Echo: Clearly distinguish sent commands from incoming streams with [TX] tagging.

Custom Line Endings: Selectable \n, \r, or \r\n terminations for AT commands.

Programmable Macros: Save up to four frequently used commands directly to your browser's local storage for rapid execution.

On-Board Data Logging: Automatically appends incoming UART streams to the ESP8266's internal LittleFS flash memory for long-term monitoring. Logs can be downloaded or erased via the web interface.

Hardware Status LEDs: Visual indicators for RX and TX line activity.

Over-The-Air (OTA) Updates: Flash new firmware updates wirelessly over the local network without disassembling the prototype.

🛠️ Hardware Requirements & Wiring
Microcontroller: ESP8266 (NodeMCU, Wemos D1 Mini, or custom board)

Status LEDs (Optional): * TX Indicator: GPIO5 (D1) + 330Ω resistor to GND

RX Indicator: GPIO4 (D2) + 330Ω resistor to GND

Connecting the Target Device:
Ensure the target device and the ESP8266 share a common ground.

Target TX ➔ ESP8266 RX

Target RX ➔ ESP8266 TX

Target GND ➔ ESP8266 GND

(Note: If interfacing with high-voltage industrial equipment, it is highly recommended to use optocouplers on the RX/TX lines for galvanic isolation).

💻 Software Setup & Installation
1. Arduino IDE Dependencies
You will need to install the following libraries via the Arduino Library Manager:

WebSockets by Markus Sattler

The following are built into the ESP8266 core:

ESP8266WiFi

ESP8266WebServer

DNSServer

EEPROM

LittleFS

ArduinoOTA

2. IDE Configuration
Before uploading, you must configure the flash layout to allocate space for the LittleFS file system (used for the data logger).

Go to Tools > Flash Size

Select 4MB (FS: 1MB OTA:~1019KB) (or the appropriate setting for your specific ESP module that includes at least 1MB of FS).

3. Compilation & Upload
Disconnect any external devices from the ESP8266's RX/TX pins. Compile and upload the sketch via USB. Future updates can be pushed wirelessly via ArduinoOTA.

📱 Usage Instructions
Power on the ESP8266 device.

Open your phone or laptop's Wi-Fi settings and connect to the network: Astatine_Diagnostic (Open network, no password by default).

The OS should automatically detect the Captive Portal and slide up the Astatine Terminal UI. If it does not, open a web browser and navigate to http://192.168.4.1.

Use the Hardware Controls panel to set the operating Baud Rate. This setting is saved to EEPROM and persists across reboots.

Begin transmitting and receiving data!

⚙️ Configuration Variables
If you wish to change the default network settings, modify the following lines at the top of the sketch:

C++
const char* ssid = "Astatine_Diagnostic"; // Broadcasted Wi-Fi Name
const char* password = "";                // Leave blank for open network
Developed by Astatine Technologies
