/**
 * @file main.c                                                                *
 * @brief This file contains the main code for controlling the WS2812B-LEDs.   *
 * @date  Saturday 27th-January-2024                                           *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#include "bsp/board.h"
#include "tusb.h"
#include "tusb_config.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "usb_packets.h"
#include <stdlib.h>

/**
 * @def WS2812B_PIN
 * @brief Der PIO-Pin, an den die WS2812B-LEDs angeschlossen sind.
 */
#define WS2812B_PIN 2

/**
 * @def WS2812B_BUFFER_SIZE
 * @brief Die maximale Anzahl von WS2812B-Pixeln im Buffer.
 */
#define WS2812B_BUFFER_SIZE 1000

/**
 * @struct ws2812b_pixel
 * @brief Datenstruktur zur Darstellung eines einzelnen WS2812B-Pixels.
 */
typedef struct ws2812b_pixel {
	uint8_t r; /**< Der Rotanteil des Pixels. */
	uint8_t g; /**< Der Grünanteil des Pixels. */
	uint8_t b; /**< Der Blauanteil des Pixels. */
} ws2812b_pixel;

ws2812b_pixel *ws2812b_buffer; /**< Der Pixel-Buffer. */
uint32_t ws2812b_index = 0; /**< Der aktuelle Index im Buffer. */
bool ws2812b_ready =
	false; /**< Gibt an, ob der Buffer bereit ist, auf die LEDs geschrieben zu werden. */
uint32_t ws2812b_count =
	0; /**< Die in den Buffer geschriebenen WS2812B-Pixel. */

uint32_t ws2812b_send_index = 0; /**< Der Index für das Senden des Buffer */

/**
 * @brief Setzt einen Pixelwert auf den Datenbus des WS2812B.
 *
 * @param pixel_grb Der 24-Bit-Pixelwert in GRB-Reihenfolge.
 */
static inline void put_pixel(uint32_t pixel_grb)
{
	pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
 * @brief Konvertiert RGB-Werte in einen 32-Bit-Wert im Format GRB.
 *
 * @param r Der Rotanteil.
 * @param g Der Grünanteil.
 * @param b Der Blauanteil.
 * @return Der 32-Bit-Pixelwert im Format GRB.
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

/**
 * @brief Hauptfunktion zur Aktualisierung der WS2812B-LEDs.
 */
void ws2812b_task()
{
	if (ws2812b_ready) {
		for (int i = 0; i < ws2812b_count; i++) {
			put_pixel(urgb_u32(ws2812b_buffer[i].r,
					   ws2812b_buffer[i].g,
					   ws2812b_buffer[i].b));
		}
		ws2812b_ready = false;
		sleep_us(
			500); /**< Blocking Sleep für die Aktualisierung der LEDs. */
	}
}

/**
 * @brief Löscht alle Pixel im WS2812B-Buffer (Setzt Helligkeit auf 0).
 */
void ws2812b_clear()
{
	for (int i = 0; i < WS2812B_BUFFER_SIZE; i++) {
		put_pixel(urgb_u32(0, 0, 0));
	}
	sleep_us(500); /**< Blocking Sleep für die Aktualisierung der LEDs. */
}

/**
 * @brief Die Hauptfunktion des Programms.
 *
 * Initialisiert die Hardware, den Buffer und die Endlosschleife für die Programm-Ausführung.
 *
 * @return Der Programm-Rückgabewert (wird in diesem Fall nie erreicht).
 */
int main(void)
{
	PIO pio = pio0;
	int sm = 0;
	uint offset = pio_add_program(pio, &ws2812_program);

	ws2812b_buffer = calloc(WS2812B_BUFFER_SIZE, sizeof(ws2812b_pixel));

	board_init();
	tusb_init();

	ws2812_program_init(pio, sm, offset, WS2812B_PIN, 800000);

	while (1) {
		tud_task();
		ws2812b_task();
	}

	return 0;
}

/**
 * @brief Diese Funktion wird bei Empfang des CTRL-Bit 0x00 ausgeführt.
 *
 * Nimmt den USB-Buffer und schreibt die Daten strukturiertin den WS2812B-Buffer.
 *
 * @param usb_buffer Der USB-Buffer.
 */
void fill_ws2812b_buffer(uint8_t *usb_buffer)
{
	uint32_t i = 1;
	while (i < (CFG_TUD_VENDOR_RX_BUFSIZE - 1) &&
	       ws2812b_index < ws2812b_count) {
		ws2812b_buffer[ws2812b_index].r = usb_buffer[i];
		ws2812b_buffer[ws2812b_index].g = usb_buffer[i + 1];
		ws2812b_buffer[ws2812b_index].b = usb_buffer[i + 2];

		ws2812b_index++;
		i += 3;
	}

	if (ws2812b_index == ws2812b_count) {
		ws2812b_ready = true;
		ws2812b_index = 0;
	}
}

/**
 * @brief Diese Funktion wird bei Empfang des CTRL-Bit 0x01 ausgeführt
 *
 * Nimmt den USB-Buffer, extrahiert die Länge und speichert diese.
 *
 * @param usb_buffer Der USB-Buffer.
 */
void set_ws2812b_length(uint8_t *usb_buffer)
{
	ws2812b_count = usb_buffer[1] << 8 | usb_buffer[2];
	ws2812b_clear();
}

/**
 * @brief Diese Funktion wird bei Empfang des CTRL-Bit 0x02 ausgeführt.
 *
 * Schreibt die Länge des WS2812B-Buffer in den USB-Buffer.
 * 
 * @param buffer_out Der USB-Buffer.
 */
void get_ws2812b_length_usb_packet(uint8_t *buffer_out)
{
	buffer_out[0] = 0x01;
	buffer_out[1] = ws2812b_count >> 8;
	buffer_out[2] = ws2812b_count & 0xFF;
}

/**
 * @brief Diese Funktion wird bei Empfang des CTRL-Bit 0x03 und 0x04 ausgeführt.
 * 
 * Schreibt die Daten des WS2812B-Buffer in den USB-Buffer.
 * 
 * @param buffer_out Der USB-Buffer.
 */
void get_ws2812b_buffer_usb_packet(uint8_t *buffer_out)
{
	buffer_out[0] = 0x00;
	uint32_t i = 0;
	while (i < (CFG_TUD_VENDOR_TX_BUFSIZE - 1) &&
	       ws2812b_send_index < ws2812b_count) {
		buffer_out[i + 1] = ws2812b_buffer[ws2812b_send_index].r;
		buffer_out[i + 2] = ws2812b_buffer[ws2812b_send_index].g;
		buffer_out[i + 3] = ws2812b_buffer[ws2812b_send_index].b;

		ws2812b_send_index++;
		i += 3;
	}

	if (ws2812b_send_index == ws2812b_count) {
		ws2812b_send_index = 0;
	}
}

/**
 * @brief Handles LED data packets for WS2812 LEDs.
 *
 * This function processes packets containing pixel data for WS2812 LEDs.
 * It extracts the color data (red, green, blue) for each LED from the packet
 * and stores it in a buffer. The data is then used to update the state of the LEDs.
 *
 * @param pixel_data_pkg Pointer to the WS2812 USB packet containing pixel data.
 *
 * @note The function processes up to 21 LEDs per packet and updates the LEDs
 *       if the end when data for all LEDs are received. It sets `ws2812b_ready` to true
 *       indicating that the LEDs are ready to be updated with new data.
 */
void ws2812_handle_led_data_pkg(ws2812_usb_packet_pixeldata *pixel_data_pkg)
{
	int i = 0;
	while (i < 21 && ws2812b_index < ws2812b_count) {
		ws2812b_buffer[ws2812b_index].r =
			pixel_data_pkg->color_data[i].red;
		ws2812b_buffer[ws2812b_index].g =
			pixel_data_pkg->color_data[i].green;
		ws2812b_buffer[ws2812b_index].b =
			pixel_data_pkg->color_data[i].blue;

		ws2812b_index++;
		i++;
	}

	if (ws2812b_index == ws2812b_count) {
		ws2812b_ready = true;
		ws2812b_index = 0;
	}
}

/**
 * @brief Diese Funktion wird bei Empfang des CTRL-Bit 0x01 ausgeführt
 *
 * Nimmt das Paket, extrahiert die Länge und speichert diese.
 * 
 * @param count_pkg Das empfangene Paket
 */
void ws2812_handle_led_count_pkg(ws2812_usb_packet_count *count_pkg)
{
	// TOD: Check max size
	ws2812b_count = count_pkg->led_count_H << 8 |
			count_pkg->led_count_L & 0xFF;
	ws2812b_clear();
}

/**
 * @brief Handles LED count request packets.
 *
 * This function responds to requests for the current count of WS2812 LEDs and 
 * the maximum number of LEDs supported. It prepares a packet containing the 
 * current LED count and the maximum count, then sends this packet back to the 
 * USB host.
 *
 * The packet includes both the current number of LEDs (`ws2812b_count`) and the 
 * maximum number of LEDs that can be handled (`WS2812B_BUFFER_SIZE`). These counts 
 * are split into high and low bytes before being sent.
 */
void ws2812_handle_led_request_len_pkg()
{
	ws2812_usb_packet_count count_pkg;
	memset(&count_pkg, 0, sizeof(count_pkg));
	count_pkg.ctrl = LED_COUNT;
	count_pkg.led_count_H = ws2812b_count >> 8;
	count_pkg.led_count_L = ws2812b_count & 0xFF;
	uint16_t max_count = WS2812B_BUFFER_SIZE;
	count_pkg.max_led_count_H = max_count >> 8;
	count_pkg.max_led_count_L = max_count & 0xFF;

	// sizeof(count_pkg) muss gleich CFG_TUD_VENDOR_TX_BUFSIZE sein!
	tud_vendor_write(&count_pkg, CFG_TUD_VENDOR_TX_BUFSIZE);
}

/**
 * @brief Handles requests for pixeldata.
 *
 * This function is called when a request for pixeldata is received. 
 * It constructs a packet containing the color data (red, green, blue) 
 * for a block of WS2812 LEDs starting from the specified index and sends this 
 * packet back to the USB host.
 *
 * The block index is extracted from the request packet and used to determine 
 * the starting index of the LED data in the `ws2812b_buffer`. The function then 
 * populates a pixel data packet with up to 21 LED's color data from this starting 
 * index and sends it using `tud_vendor_write`.
 *
 * @param request_led_data_pkg Pointer to the packet containing the request for 
 *                             LED data, including the starting block index.
 */
void ws2812_handle_led_request_led_data_pkg(
	ws2812_usb_packet_request_led_data *request_led_data_pkg)
{
	uint16_t block_index = request_led_data_pkg->led_block_index_H << 8 |
			       request_led_data_pkg->led_block_index_L & 0xFF;

	ws2812_usb_packet_pixeldata pixel_pkg;
	memset(&pixel_pkg, 0, sizeof(pixel_pkg));
	pixel_pkg.ctrl = LED_DATA;
	int start_index = 21 * block_index;
	int i = 0;
	while (i < 21 && start_index + i < ws2812b_count) {
		pixel_pkg.color_data[i].red = ws2812b_buffer[start_index + i].r;
		pixel_pkg.color_data[i].green =
			ws2812b_buffer[start_index + i].g;
		pixel_pkg.color_data[i].blue =
			ws2812b_buffer[start_index + i].b;
		i++;
	}
	// sizeof(pixel_pkg) muss gleich CFG_TUD_VENDOR_TX_BUFSIZE sein!
	tud_vendor_write(&pixel_pkg, CFG_TUD_VENDOR_TX_BUFSIZE);
}

/**
 * @brief Callback-Funktion für den Empfang von Vendor-Daten über USB.
 *
 * Diese Funktion verarbeitet empfangene Vendor-Daten, um die WS2812B-LEDs zu steuern.
 *
 * @param ift Das USB-Interface, über das die Daten empfangen wurden.
 */
void tud_vendor_rx_cb(uint8_t ift)
{
	uint8_t buffer_in[CFG_TUD_VENDOR_RX_BUFSIZE];
	tud_vendor_read(buffer_in, CFG_TUD_VENDOR_RX_BUFSIZE);

	uint8_t buffer_out[CFG_TUD_VENDOR_TX_BUFSIZE];
	memset(buffer_out, 0, CFG_TUD_VENDOR_TX_BUFSIZE);

	uint8_t ctrl = buffer_in[0];

	switch (ctrl) {
	case LED_DATA:
		//fill_ws2812b_buffer(buffer_in);
		ws2812_handle_led_data_pkg(
			(ws2812_usb_packet_pixeldata *)buffer_in);

		break;

	case LED_COUNT:
		ws2812_handle_led_count_pkg(
			(ws2812_usb_packet_count *)buffer_in);

		break;

	case REQUEST_LEN:
		ws2812_handle_led_request_len_pkg();
		//get_ws2812b_length_usb_packet(buffer_out);
		break;

	case REQUEST_LED_DATA:
		ws2812_handle_led_request_led_data_pkg(
			(ws2812_usb_packet_request_led_data *)buffer_in);
		break;

	case LED_CLEAR:
		ws2812b_clear();

		break;

	default:
		break;
	}
	tud_vendor_read_flush();
}