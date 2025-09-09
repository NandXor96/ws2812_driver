/**
 * @file usb_packets.h                                                         *
 * @brief This file defines data structures and communication protocols        *
 * for the WS2812 controller via USB, including packet formats for             *
 * sending pixeldata, requesting states, and managing LED counts.              *
 * @date  Wednesday 24th-January-2024                                          *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer                                      *
 */

#ifndef USB_PACKETS_H
#define USB_PACKETS_H

/**
 * @brief Enumeration for WS2812 USB control commands.
 *
 * This enumeration defines control commands used in the communication with a WS2812 Controller
 * over USB. These commands are used to control the behavior of the LED strip and to request
 * or send LED data.
 */
enum WS2812_USB_CTRL {
	LED_DATA = 0, /**< Command to send data for a maximum of 21 LEDs. */
	LED_COUNT, /**< Command to specify the number of LEDs in the strip. */
	REQUEST_LEN, /**< Command to request the length of the LED strip. */
	REQUEST_LED_DATA, /**< Command to request the pixeldata. */
	LED_CLEAR = 0x99 /**< Command to clear all LEDs (off). */
};

/**
 * @brief Structure representing a single WS2812 pixel.
 *
 * This structure defines the color of a single WS2812 pixel in terms of its
 * red, green, and blue components. Each color component is represented by an 8-bit value,
 * allowing for 256 intensity levels per color.
 *
 * The `__attribute__((packed))` ensures that the compiler does not insert padding
 * between the color components, which is important for ensuring the correct
 * layout of the data when it is sent to the WS2812 LEDs.
 */
typedef struct ws2812_pixel_s {
	uint8_t red; /**< Red color component of the LED pixel. */
	uint8_t green; /**< Green color component of the LED pixel. */
	uint8_t blue; /**< Blue color component of the LED pixel. */
} __attribute__((packed)) ws2812_pixel;

/**
 * @brief Structure representing a generic USB packet for a WS2812 controller.
 *
 * This structure defines the format of a USB packet used for controlling WS2812 LEDs.
 * It includes control commands and is designed to match the expected packet size for USB communication.
 *
 * The `__attribute__((packed))` attribute is used to ensure that the compiler does not add any padding
 * between the fields, maintaining the strict size requirements for USB data packets.
 */
typedef struct ws2812_usb_packet_s {
	uint8_t ctrl; /**< Control byte */
	uint8_t reserved
		[63]; /**< Reserved bytes to fill the structure to a size of 64 bytes. */
} __attribute__((packed)) ws2812_usb_packet;

/**
 * @brief Structure representing a USB packet for sending and receiving the LED length informations.
 *
 * The structure includes fields for the current LED count and the maximum LED count, split into high and low bytes
 * for each, to accommodate a larger range of values. The packet is padded with reserved bytes to meet the USB
 * data packet size requirements.
 *
 * The `__attribute__((packed))` attribute is used to ensure that the compiler does not add any padding
 * between the fields, maintaining the strict size requirements for USB data packets.
 */
typedef struct ws2812_usb_packet_count_s {
	uint8_t ctrl; /**< Control byte */
	uint8_t led_count_H; /**< High byte of the current LED count. */
	uint8_t led_count_L; /**< Low byte of the current LED count. */
	uint8_t max_led_count_H; /**< High byte of the maximum LED count supported. */
	uint8_t max_led_count_L; /**< Low byte of the maximum LED count supported. */
	uint8_t reserved
		[59]; /**< Reserved bytes to fill the structure to a size of 64 bytes. */
} __attribute__((packed)) ws2812_usb_packet_count;

/**
 * @brief Structure representing a USB packet for pixeldata.
 *
 * This structure is used for sending and receiving RGB color data for a series of WS2812 LEDs over USB.
 *
 * The packet includes a control byte followed by an array of `ws2812_pixel` structures, each representing
 * the RGB values of a single LED.
 *
 * The `__attribute__((packed))` attribute is used to ensure that the compiler does not add any padding
 * between the fields, maintaining the strict size requirements for USB data packets.
 *
 * @note The array `color_data` can hold color information for up to 21 LEDs, with each LED's color
 * represented by a `ws2812_pixel` structure.
 */
typedef struct ws2812_usb_packet_pixeldata_s {
	uint8_t ctrl; /**< Control byte */
	ws2812_pixel color_data
		[21]; /**< Array of `ws2812_pixel` structures for RGB color data of up to 21 LEDs. */
} __attribute__((packed)) ws2812_usb_packet_pixeldata;

/**
 * @brief Structure representing a USB packet for requesting specific pixeldata.
 *
 * This structure is used for requesting data of a specific block of LEDs over USB.
 *
 * The packet includes a control byte and fields for specifying the index of the LED block, split into high
 * and low bytes to accommodate a larger range of values. The rest of the packet is padded with reserved bytes
 * to meet the USB data packet size requirements.
 *
 * The `__attribute__((packed))` attribute is used to ensure that the compiler does not add any padding
 * between the fields, maintaining the strict size requirements for USB data packets.
 */
typedef struct ws2812_usb_packet_request_led_data_s {
	uint8_t ctrl; /**< Control byte */
	uint8_t led_block_index_H; /**< High byte of the LED block index to request data from. */
	uint8_t led_block_index_L; /**< Low byte of the LED block index to request data from. */
	uint8_t reserved
		[61]; /**< Reserved bytes to fill the structure to a size of 64 bytes. */
} __attribute__((packed)) ws2812_usb_packet_request_led_data;
#endif
