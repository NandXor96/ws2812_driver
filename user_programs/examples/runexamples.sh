#!/bin/sh
echo Lade Kernelmodul
modprobe usb_ws2812

echo "./usb-ws2812-client -f /dev/usb_ws2812_0 -l 50 # Setzte die länge auf 50"
./usb-ws2812-client -f /dev/usb_ws2812_0 -l 50 # Setzte die länge auf
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 --pixeldatafile=led_daten_test"
./usb-ws2812-client -f /dev/usb_ws2812_0 --pixeldatafile=led_daten_test
echo
echo Die vierte bis zur sechsten LED rot färben
echo press any key to continue
read -n 1
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 --pixeldatafile=led_daten_red_3_bis_6"
./usb-ws2812-client -f /dev/usb_ws2812_0 --pixeldatafile=led_daten_red_3_bis_6

echo
echo "Länge abfragen"
echo press any key to continue
read -n 1
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 -m --get_data --get_length # Frage den Modus die Länge und die Pixeldaten ab"
./usb-ws2812-client -f /dev/usb_ws2812_0 -m --get_data --get_length # Frage den Modus die Länge und die Pixeldaten ab

echo
echo In den Blinkmodus wechseln
echo press any key to continue
read -n 1
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 -l 16 -b -p pattern2 -d 500 # Wechsel in den Blinkmodus mit einem Delay von 500 ms"
./usb-ws2812-client -f /dev/usb_ws2812_0 -l 16 -b -p pattern2 -d 500 # Wechsel in den Blinkmodus mit einem Delay von 500 ms

echo
echo "Muster abfragen"
echo press any key to continue
read -n 1
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 -m --get_mode_data"
./usb-ws2812-client -f /dev/usb_ws2812_0 -m --get_mode_data

echo
echo "Clear"
echo press any key to continue
read -n 1
echo "./usb-ws2812-client -f /dev/usb_ws2812_0 -c"
./usb-ws2812-client -f /dev/usb_ws2812_0 -c
