/**
 * @file usb_ws2812_lib.h                                                      *
 * @brief Header for the user library                                          *
 * @date  Saturday 27th-January-2024                                           *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#ifndef USB_WS2812_H
#define USB_WS2812_H

#include <stdint.h>
#include "dev_packets.h"

/**
 * @brief Structure representing a pattern for the WS2812 LED strip.
 */
typedef struct ws2812_pattern_s {
	uint16_t length; // Length of the pattern.
	uint16_t pattern_states; // Number of pattern states.
	led_pixel *pattern_data; // Pointer to the pattern data array.
} ws2812_pattern;

/**
 * @brief Structure representing a buffer for pixel data of the WS2812 LED strip.
 */
typedef struct ws2812_pixel_buffer_s {
	uint16_t length; // Length of the pixel buffer.
	led_pixel *pixel_data; // Pointer to the pixel data array.
} ws2812_pixel_buffer;

extern int ws2812_init();
extern void ws2812_deinit();
extern int ws2812_set_length(int fd, uint16_t length);
extern int ws2812_clear(int fd);
extern int ws2812_set_mode_static(int fd);
extern int ws2812_set_mode_blink(int fd, uint16_t pattern_count,
				 uint16_t pattern_len, uint16_t delay);
extern int ws2812_set_led_pixel(int fd, uint16_t start_index, uint16_t length,
				led_pixel *pixel_data);
extern int ws2812_set_blink_pattern(int fd, ws2812_pattern *pattern);

extern int ws2812_get_length(int fd);
extern int ws2812_get_mode_data_length(int fd);
extern int ws2812_get_data(int fd, ws2812_pixel_buffer *result);
extern int ws2812_get_mode_data(int fd, ws2812_pixel_buffer *result);
extern int ws2812_get_mode(int fd, led_set_mode *result);

#endif