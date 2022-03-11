#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h> 
#include <LITTLEFS.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <LOLIN_I2C_MOTOR.h>
#include <ticker.h>
#include <RunningMedian.h>  
#include "config.h"
//#include "localconfig.h"

#define dir_forward 0
#define dir_backward 1
#define STATUS_LED D6

// Function prototypes
String getContentType(String filename); 
bool handleFileRead(String path);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);
void motionControl();
void checkPower();

// Create a webserver that listens for HTTP request on port 80
ESP8266WebServer server(80);

// create a websocket server on port 81
WebSocketsServer webSocket(81);

// Lolin Motor-Shield (Version 2.0.0, HR8833, AT8870)
LOLIN_I2C_MOTOR motor;

// Timer regelt alle 100 ms die Motorgeschwindigkeit
Ticker motionControlTicker(motionControl, 100, 0, MILLIS);

// Timer fragt alle 30 Sekunden den Akku-Status ab
Ticker powerCheckTicker(checkPower, 30000, 0, MILLIS);

// Speicher zur Berechnung der Durchschnittswerte bei der Akkuprüfung
RunningMedian median1 = RunningMedian(10);

// interne Variablen
int direction = dir_forward;   // Richtung 0: vorwärts, 1: rückwärts
int actual_speed = 0;          // aktuelle Geschwindigkeit 0 - 100
int target_speed = 0;          // Zielgeschwindigkeit
byte batRate = 100;            // Akku-Kapazität
float batVoltage = 4.2;        // Akku-Spannung

/*------------------------------------------------------------------------------
SETUP
------------------------------------------------------------------------------*/

/**
 * @brief Öffnet WLAN Access-Point.
 * 
 */
void setupWifiAP() {
  // Accesspoint starten
  boolean status = WiFi.softAP(ssid, password);
  if (!status) {
    Serial.println("Wifi SoftAP cannot Connect");
  }
  Serial.printf("Connecting to [%s] ...\n", ssid);
  Serial.println("Connection established!");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

/**
 * @brief Lolin D1 Setup
 * 
 */
void setup() {
  Serial.begin(115200);
  Serial.println("Init application");

  // Analoger Eingang für Überwachung der Akku-Spannung
  pinMode(A0, INPUT);

  // Status-LED initialisieren
  pinMode(STATUS_LED, OUTPUT);
  
  // Start WLAN
  //setupWiFi();  // WiFi Verbindung mit bestehendem WLAN aufbauen
  setupWifiAP();  // WLAN-Accesspoint starten

  // Filesystem (statische Webablage) initialisieren
  if (LittleFS.begin()){
    Serial.println("LittleFS init OK...");
  }

  server.onNotFound([]{
    if (!handleFileRead(server.uri()))                  // send it if it exists
      // otherwise, respond with a 404 (Not Found) error
      server.send(404, "text/plain", "404: Not Found");
  });
  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");

  webSocket.begin();     
  webSocket.onEvent(webSocketEvent); 
  Serial.println("WebSocket server started");

  // Start the mDNS responder for microrail.local
  if (MDNS.begin("microrail")) {              
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // Start Timer
  motionControlTicker.start();
  powerCheckTicker.start();
  Serial.println("Timer started");

  // Init Motor-Shield
  while (motor.PRODUCT_ID != PRODUCT_ID_I2C_MOTOR) {
    motor.getInfo();
  }
  motor.changeFreq(MOTOR_CH_BOTH, motor_frequency);
  motor.changeDuty(MOTOR_CH_BOTH, 0.0);
  motor.changeStatus(MOTOR_CH_BOTH, MOTOR_STATUS_CW); // Vorwärts
  Serial.println("Motorshield ready");

  // LED einschalten
  analogWrite(STATUS_LED, 50);
  Serial.println("Setup completed");
}

/*------------------------------------------------------------------------------
LOOP
------------------------------------------------------------------------------*/
void loop() {
  server.handleClient(); // HTTP Webserver
  webSocket.loop();      // Websocket-Server      
  motionControlTicker.update();  
  powerCheckTicker.update();
}

/*------------------------------------------------------------------------------
Websocket-Eventhandling
------------------------------------------------------------------------------*/

/**
 * @brief Baut ein Statusobjekt im JSON-Format (String), dass bei jeder Änderung an alle 
 * Clients versendet wird.
 * 
 * @return String 
 */
String buildStatus() {
  DynamicJsonDocument status(1024);
  status["ssid"] = ssid;
  status["version"] = appVersionString;
  status["direction"] = direction;
  status["speed"] = actual_speed;
  status["batRate"] = batRate;
  status["batVoltage"] = (float)((int)(batVoltage*100))/100.0; 

  String statusResponse;
  serializeJson(status, statusResponse);
  return statusResponse;
}

void handleCommands(uint8_t * command) {

  String sCommand = (char *)command;
  if (sCommand == "#STOP") {
    target_speed = 0;
  } else if (sCommand == "#SLOWER") {
    target_speed -= motor_speed_step;
    if (target_speed < 0) {
      target_speed = 0;
    }
  } else if (sCommand == "#FASTER") {
    target_speed += motor_speed_step;
    if (target_speed > 100) {
      target_speed = 100;
    }
  } else if (sCommand == "#DIRBACK") {
    if (actual_speed == 0 && direction != dir_backward) {
      direction = dir_backward;
      motor.changeStatus(MOTOR_CH_BOTH, MOTOR_STATUS_CCW);
    }
  } else if (sCommand == "#DIRFWD") {
    if (actual_speed == 0 && direction != dir_forward) {
      direction = dir_forward;
      motor.changeStatus(MOTOR_CH_BOTH, MOTOR_STATUS_CW);
    }
  }

  // neuen Status an alle Clients senden.
  String status = buildStatus();
  webSocket.broadcastTXT(status);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { 
  // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        String status = buildStatus();
        webSocket.sendTXT(num, status);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == '#') {
        // Command erkannt.
        handleCommands(payload);
      }
      break;
  }
}

/**
 * @brief timer-gesteuerte Routine zur Anpasusng der Geschwindigkeit.
 * 
 */
void motionControl() {
  if (actual_speed == target_speed) {
    // nichts zu tun
    return;
  }

  if (actual_speed < target_speed) {
    // Geschwindigkeit erhöhen
    actual_speed += motor_speed_step;
    if (actual_speed > 100) {
      actual_speed = 100;
    }
  } else if (actual_speed > target_speed) {
    // Geschwindigkeit verringern
    actual_speed -= motor_speed_step;
    if (actual_speed < 0) {
      actual_speed = 0;
    }
  }

  // Motor steuern...
  motor.changeDuty(MOTOR_CH_BOTH, actual_speed * motor_maxspeed);

  // neuen Status an alle Clients senden.
  String status = buildStatus();
  webSocket.broadcastTXT(status);
}

/**
 * @brief 
 * 
 */
void checkPower() {
  int raw = analogRead(A0);
  median1.add(raw); 
  long m = median1.getMedian(); 
  unsigned long BatValue = m * 100L;   
  float BatVoltage1 = BatValue * 4.2 / 1024L;  

  batRate = map(BatVoltage1, 240, 420, 0, 100);
  batVoltage = BatVoltage1 / 100.0;   
  batVoltage = roundf(batVoltage * 100) / 100; 
  Serial.printf("Akku %f V, %d %%\n", batVoltage, batRate);

  // neuen Status an alle Clients senden.
  String status = buildStatus();
  webSocket.broadcastTXT(status);
}

/*------------------------------------------------------------------------------
Hilfsroutinen HTTP-Webserver
------------------------------------------------------------------------------*/

/**
 * @brief Get the Content Type object
 * 
 * @param filename 
 * @return String Content Type as String
 */
String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

/**
 * @brief send the right file to the client (if it exists)
 * 
 * @param path 
 * @return true 
 * @return false 
 */
bool handleFileRead(String path){  
  //Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if(LittleFS.exists(pathWithGz) || LittleFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(LittleFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    File file = LittleFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    //Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}
