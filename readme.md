# Microrail

Heiko Deserno, 2022, [FB22](https://feldbahn22.de)

FB22-Artikel zum Projekt: https://fb22.de/123

`Microrail` ist eine Software zur drahtlosen Steuerung von Modellfahrzeugen. Zum System gehören neben dem hier beschriebenen Empfänger ein Sender (Handregler). Der Sender als Bediengerät mit Drehregler und Display ist ein separates Softwareprojekt (derzeit noch nicht vorhanden).

### Warum gibt es Microrail?

Da die wunderbaren [Deltang](http://www.deltang.co.uk/)-Empfänger *Rx6* nicht mehr hergestellt werden und kaum noch lieferbar sind, habe ich nach einer geeigneten Alternative zur drahtlosen Steuerung von Modellbahn-Fahrzeugen (insbesondere Feldbahnen im Maßstab 1:13) gesucht. [Locoremote](http://www.locoremote.co.uk/) ist auf den ersten Blick eine brauchbare Alternative. Den für mich größten Makel wollte ich jedoch nicht hinnehmen: Eine Modellfahrzeug sollte nicht mit einem Handy gesteuert werden. Hier gehört ein richtiger Handregler mit Tasten und einem Drehregler her. Da *Locoremote* diese Funktion nicht unterstützt, kam die Idee einer eigenen Software. Die Idee zur dieser Software stammt urspünglich von [Olle Sköld](http://depronized.com/), der eine solche Steuerung für seine [Hectorrail](https://www.thingiverse.com/thing:2575667)-Lokomotive vorstellte. `Microrail` nutzt eine andere Hardware-Plattform sowie ein modifiziertes Bedienkonzept. Auch die Idee des Handsenders stammt von ihm. Auf [Thingiverse](https://www.thingiverse.com/thing:3252986) stellte er einen Prototypen vor, der eher als Idee statt einer fertigen Lösung zu verstehen ist. 

## Microrail-Receiver

Der `Microrail`-Receiver bzw. Empfänger ist die Komponente zur Motorensteuerung. Ein  Microcontroller und eine Motoren-Steuerplatine genügen für diese Funktion. Der Microcontroller erstellt einen eigenen WLAN-Accesspoint. WLAN-fähige Clients wie Mobiltelefone oder Tablets können sich mit dem WLAN des Microcontrollers verbinden und die UI über die Adresse http://192.168.1.4 aufrufen. Die UI enthält Buttons zur Steuerung von Geschwindigkeit und Fahrtrichtung. 

### Hardware

Als Microcontroller wird ein [Lolin D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html) verwendet. Als Motorentreiber unterstützt die Software folgende Platinen:

- [Lolin Motorshield V 2.0.0](https://www.wemos.cc/en/latest/d1_mini_shield/motor.html), TB6612FNG Chipsatz, schwer lieferbar
- [Lolin Motorshield HR8833](https://www.wemos.cc/en/latest/d1_mini_shield/hr8833_motor.html), HR8833 Chipsatz
- [Lolin Motorshield AT8870](https://www.wemos.cc/en/latest/d1_mini_shield/at8870_motor.html), AT8870 Chipsatz 

Alle diese Platinen werden über den *I2C*-Bus mit dem *D1 mini* verbunden. Die Software-Steuerung basiert auf der [Lolin-Treiberlib](https://github.com/wemos/LOLIN_I2C_MOTOR_Library).

Die Platinen mit den *HR8833* und *AT8870* Chipsätzen sind seit 2022 neu und haben verschiedene Einsatzzwecke:

- *HR8833*: 3 bis 10 V, 1,5 A Motorstrom
- *AT8870*: 6,5 bis 38 V, 2 A Motorstrom

### Schaltplan

#### Schaltung mit 1S-Akku (Eine Zelle)

![Schaltplan 1S Akku](/images/schaltplan1.png)

Die Schaltung zeigt die Variante mit einem einzelligen LiPo/LiIo-Akku. Die Versorgungsspannung beträgt 3,7 - 4,2 V und liegt damit unter der Grenze von 5 V für den *D1 mini*. Am Ausgang *D6* ist eine Status-LED angeschlossen, die nach erfolgreicher Initilisierung leuchtet. Der 100k Widerstand liegt am analogen Eingang an und dient zur Messung der Versorgungsspannung. Als Motor wird ein 3V Getriebemotor verwendet.

#### Schaltung mit 2S-Akku (Zwei Zellen)

![Schaltplan 2S Akku](/images/schaltplan2.png)

Bei einem Zwei-Zellen Akku liegt die Versorgungsspannung im Bereich von 7,4 - 8,4 V. Zur Minderung der Spannung auf die erlaubten 5 V wird ein [Power-Shield](https://www.wemos.cc/en/latest/d1_mini_shield/dc_power.html) von *Lolin* verwendet. Im Prinzip ist das ein *Step-Down*-Regler. Das Motorshield wird aus dem Akku mit 7,2 V versorgt.

Anstelle des *Lolin Power Shield* kann auch ein anderer *Step-Down*-Regler benutzt werden.

### Software

#### Architektur

Die Software des Microcontrollers startet einen Webserver auf Port 80 zum Abruf der statischen Webseite (HTML, JavaScript und CSS). Aktionen wie Button-Behandlungen werden durch den Webserver nicht durchgeführt. Zusätzlich wird auf Port 81 ein Websocket-Server gestartet, der die bidirektionale Kommunikation mit dem Frontend übernimmt. Via Javascript werden die Aktionen in den Websocket-Kanal geschrieben und Statusänderungen empfangen. Ein- und Ausgabeelemente der Weboberfläche sind vollständig getrennt. Die Anzeigeelemente für Geschwindigkeit, Fahrtrichtung und Akkustandsanzeige werden ausschließlich durch den Rückkanal des Websockets gesetzt.

#### Funktionsbeschreibung

Während der Initialisierung werden folgende Schritte ausgeführt:

- Initialisierung Hardware-Pins (Analoger Eingang und LED-Ausgang)
- WLAN-Setup
- Setup Webserver
- Setup Websocket-Server
- Start *mDNS*-Dienst
- Start der zeitgesteuerten Tasks
- Initialisierung Motortreiber
- Status-Led on

Mit jeder Client-Verbindung wird ein Kanal im Websocket-Server geöffnet. Der Server lauscht auf Events. Button-Clicks in der UI führen zu Websocket-Events. Dabei werden die internen Werte für Geschwindigkeit und Richtung gesetzt. *Halt* setzt die Geschwindigkeit auf den Wert 0. Das Erhöhen bzw. Verringern der Geschwindigkeit erfolgt schrittweise verzögert. Ein Timer prüft alle 100 ms die Differenz zwischen aktueller und angeforderter Geschwindigkeit. Bei jedem Timer-Aufruf wird die aktuelle Geschwindigkeit um eine Schrittweite erhöht oder verringert bis zum Erreichen der Zielgeschwindigkeit. Ein Richtungswechsel ist nur bei Geschwindigkeit 0 möglich.

#### Projekt/Entwicklungsumgebung

Das Software-Projekt ist ein [PlatformIO](https://platformio.org/)-Projekt. Zur Entwicklung und Übersetzung des Codes wird [VS Code](https://code.visualstudio.com/) mit der Erweiterung *PlatformIO* benötigt. 

#### Code-Struktur

Der Code für die Steuerung ist in der Datei `src/main.cpp` enthalten. In `src/config.h` werden Konfigurationsparameter definiert. Der Ordner `data` enthält alle Dateien des Frontends (Css, Javascript und HTML). Als CSS-Bibliothek wird [PureCSS](https://purecss.io/) verwendet. Der Web-Ordner `data` wird als Filesystem-Image in den Microcontroller geladen.

#### Konfiguration

In der Datei `config.h` werden Einstellungen des Empfängers festgelegt. In einer späteren Version werden diese Einstellungen im EPROM des Microcontrollers gespeichert und sind über eine Ui im Browser anzupassen.

Folgende Einstellungen sind definiert:

- `ssid` und `password`: WLAN-Einstellungen. Das Passwort muss eine Mindestlänge von 8 Zeichen besitzen.
- `motor_frequency`: Frequenz zur Motoransteuerung. 100 Hz sind für Getriebemotoren üblich, während Faulhaber-Motoren mit 20 KHz betrieben werden können. Möglicherweise sind einige Versuche nötig, bis die optimale Frequenz gefunden wurde.
- `motor_maxspeed`: Korrekturfaktor für Höchstgeschwindigkeit mit Wertebereich < 1. Dieser Wert dient zur Reduktion der Motordrehzahl bei zu schnell drehenden Motoren. Ein Wert von *0.8* reduziert die maximale Drehzahl auf 80%.
- `motor_speed_step`: Schrittweite in % für die Erhöhung bzw. Verringerung der Geschwindigkeit. Ein Wert von 10 bedeutet, dass 10 Fahrstufen bis zum einem Maximalwert von 100 zur Verfügung stehen.

### Weboberfläche

Nach dem Start des `Microrail`-Empfängers wird ein WLAN erzeugt. Ein Client verbindet sich mit diesem WLAN und startet die Oberfläche über http://192.168.4.1 gestartet.

![Mircorail-WebUi](/images/microrail-ui-1.png)

Ale Eingabe-Elemente stehen zur Verfügung:

- Fahrtrichtung vorwärts/rückwärts
- Geschwindigkeit erhöhen/verringern
- Stop

Die WebUi empfängt Ereignisse über den Websocket:

- WLAN-SSID
- Fahrtrichtung
- Geschwindigkeit in %
- Batteriespannung
- Software-Version

Bei Geschwidigkeiten > 0 kann die Fahrtrichtung nicht geändert werden. Die Buttons sind deaktiviert. 

![Mircorail-WebUi](/images/microrail-ui-2.png)

# Ausblick

Die Steuerung des `Microrail`-Empfängers über die Web-Oberfläche ist nur ein erster Schritt. Das große Ziel ist ein eigenständiger `Microrail`-Sender mit Tasten, Drehregler und Display. Der Sender startet WLAN und einen Websocket-Client und kommuniziert mit dem Empfänger.

