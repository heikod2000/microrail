/**
 * Configuration for MicroRail
 * 
 * Diese Datei enthält Parameter zur Konfiguration der Software.
 * 
 * hde, 2/22
 */

// Zugangsdaten für WLAN Accesspoint
const char* ssid = "microrail01";
const char* password = "12345678";

// Motorfrequenz: 100 Hz für kleine Getriebemotoren, > 10 KHz für Faulhabermotoren
const int motor_frequency = 100;  // Motorfrequenz 
const float motor_maxspeed = 1;   // Höchstgeschwindigkeit in % (0,8 für 80%)
int motor_speed_step = 7;         // Schrittweite für Geschwindigkeitsänderung


// Software-Version
String appVersion = "0.5";
String appVersionString = "MicroRail R v" + appVersion + " by hde";