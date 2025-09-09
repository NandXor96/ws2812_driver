/**
 * @file dev_packets.h                                                         *
 * @brief This file defines data structures and communication protocols        *
 * for the Kernel Module via Dev-File, including packet formats.               *
 * @date  Saturday 11th-November-2023                                          *
 * Document class: public                                                      *
 * (c) 2023 Erik Appel, Kristian Minderer                                      *
 */

#ifndef DEV_PACKETS_H
#define DEV_PACKETS_H

/**
 * @brief Enumeration representing control package IDs for communication with the kernel module.
 */
typedef enum LED_CTRL_E {
	CHAR_LED_LEN, /**<  Package ID for LED length. */
	CHAR_LED_PIXEL_DATA, /**<  Package ID for LED pixel data. */
	CHAR_LED_SET_MODE, /**<  Package ID for setting LED mode. */
	CHAR_LED_CLEAR, /**<  Package ID for clearing LED data. */
	CHAR_LED_GET_DATA /**<  Package ID for getting LED data. */
} LED_CTRL;

/**
 * @brief Enumeration representing the operational modes of the kernel module strip.
 *
 * This enumeration defines the available operational modes supported by the kernel module strip.
 * Modes include static mode, where the LED strip displays a fixed pattern or color, and blink mode,
 * where the LED strip blinks or flashes according to a specified pattern.
 */
typedef enum LED_MODE_E {
	CHAR_LED_MODE_STATIC = 0, /**< Static mode for LED operation. */
	CHAR_LED_MODE_BLINK, /**< Blink mode for LED operation. */
	CHAR_LED_MODE_LENGTH /**< Total number of modes. Should be the last entry in the enum. */
} LED_MODE;

/**
 * @brief Enumeration representing the data type IDs for the LED get data request.
 *
 * This enumeration defines the data type IDs used as the `data_type` field in the
 * `led_get_data_s` structure. It specifies the type of data to retrieve from the
 * kernel module during communication between the userspace and the kernel module.
 */
typedef enum LED_DATA_ID_E {
	DATA_LEN, /**< Data type ID for retrieving LED length information. */
	DATA_MODE, /**< Data type ID for retrieving LED mode information. */
	DATA_PIXEL, /**< Data type ID for retrieving LED pixel data. */
	DATA_MODE_PIXEL /**< Data type ID for retrieving mode-specific pixel data. */
} LED_DATA_ID;

/**
 * @brief Structure representing an RGB LED pixel.
 *
 * This structure defines the RGB components of an LED pixel. Each component (red, green, and blue)
 * is represented by an 8-bit unsigned integer, allowing for 256 levels of intensity for each color.
 */
typedef struct led_pixel_s {
	uint8_t red; /**< Red component of the LED pixel. */
	uint8_t green; /**< Green component of the LED pixel. */
	uint8_t blue; /**< Blue component of the LED pixel. */
} led_pixel;

/**
 * @brief Structure representing the length packet for updating and retrieving the length of the WS2812 LED strip.
 *
 * This structure defines the format for a length packet used in updating and retrieving the length of the WS2812 LED strip.
 * It consists of a control byte (`ctrl`), which represents the packet ID defined by the LED_CTRL_E enum (in this case, CHAR_LED_LEN),
 * and a 16-bit unsigned integer (`len`) representing the length of the LED strip.
 */
typedef struct led_len_s {
	uint8_t ctrl; /**< Control byte for the length packet (LED packet ID defined by LED_CTRL_E enum). */
	uint16_t len; /**< Length of the WS2812 LED strip. */
} led_len;

/**
 * @brief Structure representing the header of a packet used to update or retrieve LED pixel data.
 *
 * This structure defines the header format of a packet used to update or retrieve LED pixel data
 * for the WS2812 LED strip. If the kernel module sends LED pixel data to the userspace, the same
 * packet structure is used. In such cases, the offset is always zero.
 *
 * The packet is composed of this structure followed by `led_count` instances of the `led_pixel`
 * structure representing the RGB components of each LED.
 */
typedef struct led_pixel_data_s {
	uint8_t ctrl; /**< Control byte for the pixel data packet (0x01 or CHAR_LED_PIXEL_DATA). */
	uint16_t led_count; /**< Number of LEDs represented by the pixel data. */
	uint16_t offset; /**< Offset of the pixel data (always zero when sent from kernel module to userspace). */
} led_pixel_data;

/**
 * @brief Structure representing the set mode packet for controlling the operation mode of the WS2812 LED strip.
 *
 * This structure defines the common fields among the different set mode packets used to control the operation mode
 * of the WS2812 LED strip. It includes a control byte (`ctrl`) and a mode byte (`mode`) indicating the desired
 * operation mode.
 */
typedef struct led_set_mode_s {
	uint8_t ctrl; /**< Control byte for the set mode packet (0x02 or CHAR_LED_SET_MODE). */
	uint8_t mode; /**< Desired operation mode of the LED strip. */
} led_set_mode_s;

/**
 * @brief Structure representing the set mode packet for setting the static operation mode of the WS2812 LED strip.
 *
 * This structure defines the format of a packet used to set the static operation mode of the WS2812 LED strip.
 * It includes a control byte (`ctrl`) and a mode byte (`mode`) indicating the static operation mode.
 * In the static mode, LED data is set when requested using a `led_pixel_data` packet.
 */
typedef struct led_set_mode_static_s {
	uint8_t ctrl; /**< Control byte for the set static mode packet (0x02 or CHAR_LED_SET_MODE). */
	uint8_t mode; /**< Desired static operation mode of the LED strip (0x00 or CHAR_LED_MODE_STATIC). */
} led_set_mode_static;

/**
 * @brief Structure representing the set mode packet for setting the blink operation mode of the WS2812 LED strip.
 *
 * This structure defines the format of a packet used to set the blink operation mode of the WS2812 LED strip.
 * It includes a control byte (`ctrl`), a mode byte (`mode`) indicating the blink operation mode,
 * a byte specifying the number of blink patterns (`pattern_count`),
 * a byte specifying the length of each blink pattern (`pattern_len`), and
 * a 16-bit integer specifying the delay between different patterns in milliseconds (`blink_period`).
 */
typedef struct led_set_mode_blink_s {
	uint8_t ctrl; /**< Control byte for the set blink mode packet (0x02 or CHAR_LED_SET_MODE). */
	uint8_t mode; /**< Desired blink operation mode of the LED strip. */
	uint8_t pattern_count; /**< Number of blink patterns. */
	uint8_t pattern_len; /**< Length of each blink pattern. */
	uint16_t blink_period; /**< Delay between different patterns in milliseconds. */
} led_set_mode_blink;

/**
 * @brief Union for representing different set mode packets of the WS2812 LED strip.
 *
 * This union allows accessing different types of set mode packets for controlling the operation
 * mode of the kernel module. It can hold instances of `led_set_mode_s`, `led_set_mode_static_s`,
 * or `led_set_mode_blink_s` structures.
 */
typedef union led_set_modes_u {
	led_set_mode_s set_mode; /**< Set mode packet for general mode setting. */
	led_set_mode_static
		set_static; /**< Set mode packet for setting static mode. */
	led_set_mode_blink
		set_blink; /**< Set mode packet for setting blink mode. */
} led_set_mode;

/**
 * @brief Structure representing the packet for clearing the LED data of the WS2812 LED strip.
 *
 * This structure defines a packet used to clear the LED data of the WS2812 LED strip.
 * It includes a control byte (`ctrl`) indicating the clear operation (0x03 or CHAR_LED_CLEAR).
 * Clearing the LED data resets the operation mode to static and clears the LED strip.
 */
typedef struct led_clear_s {
	uint8_t ctrl; /**< Control byte for the clear packet (0x03 or CHAR_LED_CLEAR). */
} led_clear;

/**
 * @brief Structure representing a request to get specific data from the kernel module.
 *
 * This structure defines a request format to retrieve specific data from the kernel module.
 * It encapsulates control information, data type specified by the LED_DATA_ID_E enum, and
 * packet length required for communication between the userspace and the kernel module.
 */
typedef struct led_get_data_s {
	uint8_t ctrl; /**< Control byte for the request (0x04 or CHAR_LED_GET_DATA). */
	uint8_t data_type; /**< Type of data to retrieve from the kernel module (specified by LED_DATA_ID_E enum). */
	uint8_t p_len; /**< Packet length for the request. This field is unused. */
} led_get_data;

#define MAKE_LED_CLEAR(struct_ptr) *(struct_ptr) = { .ctrl = LED_CLEAR }

#endif