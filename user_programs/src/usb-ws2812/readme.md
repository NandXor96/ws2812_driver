# Das usb-ws2812-client Userspaceprogram

Dieses Programm ist eine Beispielimplementierung, welche über die usb-ws2812-lib mit dem LED-Streifen kommuniziert.

## Kommandozeile Argumente


1. Der Devicefile (`--devicefile=FILE` oder `-f FILE`):
   
   Dieses Argument gibt an, welcher LED-Streifen angesteuert werden soll. Deswegen wird dieses Argument immer benötigt.
2. Den Modus anfragen (`--mode` oder `-m`):
   
   Mit diesem Argument wird der Modus abgefragt, in dem sich das Kernelmodul gerade befindet.
   Beispiel: `./usb-ws2812-client -f /dev/usb_ws2812_0 -m`
3. In den Blinkmodus wechseln (`--blink` oder `-b`):

   Mit diesem Argument wird das Kernmodul in den Blinkmodus versetzt. Optional kann noch mit dem Argument `-d` oder `--blinkdelay` die Wartezeiten zwischen den Musterwechseln eingestellt werden. Das Default Blinkdelay ist 1000ms.
   Wenn das Beispielmuster nicht verwendet werden soll, kann mit der optionalen Option `-p FILE` oder `--blinkpattern=FILE` eine Datei angegeben werden, die das gewünschte Muster speichert.

   Beispiel:
   - Einfacher Moduswechsel: `./usb-ws2812-client -f /dev/usb_ws2812_0 -b`
   - Moduswechsel mit eigener Wartezeit von 500ms: `./usb-ws2812-client -f /dev/usb_ws2812_0 -b -d 500`
   - Eigenes Muster: `./usb-ws2812-client -f /dev/usb_ws2812_0 -b -p pattern2`.
     
     Eine Datei die ein Muster speichert, muss folgendes Format besitzen:
     `Musterlänge Musteranzahl R-0 G-0 B-0 ... R-n G-n B-n`

     Die Datei pattern2: `1 3 65 0 0 65 65 0 65 65 65`

     Diese Datei hat:
     - Eine Musteranzahl von 3
     - Eine Musterlänge von 1
     - Drei Muster
        1. Muster: `65 0 0`
        2. Muster: `65 65 0`
        3. Muster: `65 65 65`

    Der Controller wird mit einer Länge von 0 initialisiert. Falls die Länge noch nicht gesetzt wurde, zeigt der LED-Streifen nichts an.
4. In den statischen Modus wechsel (`--static` oder `-s`):

   Beispiel: `./usb-ws2812-client -f /dev/usb_ws2812_0 -s`
5. Länge ändern (`--length=Länge` oder `-l Länge `):

   Ändert die Länge auf die angegebene Länge.
   Beispiel (Länge auf 16 setzten): `./usb-ws2812-client -f /dev/usb_ws2812_0 -l 16`

6. Die Ledpixeldaten des Picos auslesen (`--get_data`):
   
   Diese Option signalisiert, dass die LED-Daten des Picos ausgelesen werden sollen. Das Programm gibt die Daten formatiert aus.

   Beispiel:`./usb-ws2812-client -f /dev/usb_ws2812_0 --get_data`

7. Die Daten des aktuellen Modus auslesen (`--get_mode_data`):
   
   Wenn dieses Argument angegeben wird, werden die Daten des Kernelmoduls für den aktuellen Modus ausgelesen. Der statische Modus gibt die LED-Daten des LED-Streifens aus. 
   Der Blinkmodus, das Blinkmuster.

   Beispiele:
   - Daten ausgeben: `./usb-ws2812-client -f /dev/usb_ws2812_0 --get_mode_data`
   - In den Blinkmodus wechseln und Daten ausgeben: `./usb-ws2812-client -f /dev/usb_ws2812_0 -b --get_mode_data`
8. Pixeldaten ändern (`--pixeldatafile=FILE`):
   
   Ändert die Farben der LEDs oder das Muster je nach Modus ab. 
   Die Datei muss folgendes Format einhalten: `Länge offset led0-r led0-g led0-b ... ledn-r ledn-g ledn-b`.

   Im statischen Modus werden die Pixeldaten startend mit dem Offset auf den LED-Streifen geschrieben. Falls die Daten außerhalb der Länge liegen, schlägt die Anfrage fehl. 
   Der Buffer wird nicht verlängert.
   Im Blinkmodus werden die Daten in den Musterbuffer geschrieben, auch hier müssen die Daten in dem Buffer liegen.

   Beispiel: 
   - Länge auf 50 ändern und Pixeldaten setzten: `./usb-ws2812-client -f /dev/usb_ws2812_0 -l 50 --pixeldatafile=led_daten_test`
     Die Datei led_daten_test liegt in dem Ordner `root/user_programs`.
   - Die LEDs 3 bis 6 in Rot färben:  `./usb-ws2812-client -f /dev/usb_ws2812_0 -l --pixeldatafile=led_daten_red_3_bis_6`
     Auch die Datei led_daten_red_3_bis_6 liegt im Ordner `root/user_programs`.
9. Länge abfragen (`--get_length`):

   Beispiel: Länge auf 50 ändern und abfragen `./usb-ws2812-client -f /dev/usb_ws2812_0 -l 50 --get_length`

10. LED-Streifen zurücksetzten (`--clear` oder `c`):
    
    Löscht alle LEDs des LED-Streifens und setzt den Modus des Kernelmoduls auf static.

    Beispiel: `./usb-ws2812-client -f /dev/usb_ws2812_0 -c`

