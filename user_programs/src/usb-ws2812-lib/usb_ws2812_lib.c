/**
 * @file usb_ws2812_lib.c                                                      *
 * @brief Functions for the user library                                       *
 * @date  Saturday 27th-January-2024                                           *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#include "usb_ws2812_lib.h"
#include "dev_packets.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static uint8_t *transfer_buffer = NULL;
static size_t transfer_buffer_size = 0;

/**
 * @brief Initializes the WS2812 userspace library.
 *
 * This function allocates memory for the transfer buffer used for communication
 * with the kernel module.
 *
 * @return 1 on success, 0 on failure.
 */
int ws2812_init()
{
	transfer_buffer =
		malloc(sizeof(led_pixel_data) + 100 * sizeof(led_pixel));
	if (!transfer_buffer) {
		return 0;
	}
	transfer_buffer_size = sizeof(led_pixel_data) + 100 * sizeof(led_pixel);
	return 1;
}

/**
 * @brief Deinitializes the WS2812 userspace library.
 *
 * This function frees the memory allocated for the transfer buffer.
 */
void ws2812_deinit()
{
	free(transfer_buffer);
}

/**
 * @brief Sets the length of the WS2812 LED strip.
 *
 * This function sends a command to the kernel module via the device file descriptor
 * to set the length of the LED strip.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param length Length (in LED's) of the LED strip.
 * @return Number of bytes written, or -1 on error. (check errno for specific error).
 */
int ws2812_set_length(int fd, uint16_t length)
{
	led_len len_p = {
		.ctrl = CHAR_LED_LEN,
		.len = length,
	};

	return write(fd, &len_p, sizeof(led_len));
}

/**
 * @brief Clears the WS2812 LED strip.
 *
 * This function sends a command to the kernel module via the device file descriptor
 * to clear the LED strip.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @return Number of bytes written, or -1 on error (check errno for specific error).
 */
int ws2812_clear(int fd)
{
	led_clear clear_p = { .ctrl = CHAR_LED_CLEAR };

	return write(fd, &clear_p, sizeof(led_clear));
}

/**
 * @brief Sets the WS2812 LED strip to a static mode.
 *
 * This function sends a command to the kernel module via the device file descriptor
 * to set the LED strip to a static mode.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @return Number of bytes written, or -1 on error (check errno for specific error).
 */
int ws2812_set_mode_static(int fd)
{
	led_set_mode_static set_mode_static_p = {
		.ctrl = CHAR_LED_SET_MODE,
		.mode = CHAR_LED_MODE_STATIC,
	};

	return write(fd, &set_mode_static_p, sizeof(led_set_mode_static));
}

/**
 * @brief Sets the WS2812 LED strip to a blinking mode.
 *
 * This function sends a command to the kernel module via the device file descriptor
 * to set the LED strip to a blinking mode with specified parameters.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param pattern_count Number of blinking patterns.
 * @param pattern_len Length of each blinking pattern.
 * @param delay Delay between each blink in milliseconds.
 * @return Number of bytes written, or -1 on error (check errno for specific error).
 */
int ws2812_set_mode_blink(int fd, uint16_t pattern_count, uint16_t pattern_len,
			  uint16_t delay)
{
	led_set_mode_blink set_mode_blink_p = {
		.ctrl = CHAR_LED_SET_MODE,
		.mode = CHAR_LED_MODE_BLINK,
		.pattern_count = pattern_count,
		.pattern_len = pattern_len,
		.blink_period = delay,
	};

	return write(fd, &set_mode_blink_p, sizeof(led_set_mode_blink));
}

/**
 * @brief Sets LED pixels on the WS2812 LED strip.
 *
 * This function sends a command to the kernel module via the device file descriptor
 * to set LED pixels on the LED strip starting from the specified index.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param start_index Starting index to set LED pixels.
 * @param length Number of LED pixels to set.
 * @param pixel_data Pointer to an array of LED pixels to set.
 * @return Number of bytes written, or -1 on error (check errno for specific error).
 */
int ws2812_set_led_pixel(int fd, uint16_t start_index, uint16_t length,
			 led_pixel *pixel_data)
{
	size_t required_buffer_size =
		sizeof(led_pixel_data) + length * sizeof(led_pixel);
	if (transfer_buffer_size < required_buffer_size) {
		uint8_t *new_transfer_buffer =
			realloc(transfer_buffer, required_buffer_size);
		if (new_transfer_buffer == NULL) {
			return -1;
		}
		transfer_buffer = new_transfer_buffer;
	}

	led_pixel_data pixel_header = {
		.ctrl = CHAR_LED_PIXEL_DATA,
		.offset = start_index,
		.led_count = length,
	};

	memcpy(transfer_buffer, &pixel_header, sizeof(led_pixel_data));
	memcpy(transfer_buffer + sizeof(led_pixel_data), pixel_data,
	       length * sizeof(led_pixel));

	return write(fd, transfer_buffer, required_buffer_size);
}

/**
 * @brief Sets the blinking pattern for the WS2812 LED strip.
 *
 * This function sets the blinking pattern for the LED strip if it's currently in blinking mode.
 * It checks if the provided pattern fits inside the current pattern buffer of the kernel module
 * and if the blink mode is active.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param pattern Pointer to the blinking pattern to set.
 * @return 0 on success, -1 on error (check errno for specific error).
 */
int ws2812_set_blink_pattern(int fd, ws2812_pattern *pattern)
{
	// Check mode
	led_set_mode mode_data;
	LED_MODE mode = ws2812_get_mode(fd, &mode_data);
	if (CHAR_LED_MODE_BLINK != mode) {
		printf("Blink mode is not active.\n");
		return -1; // nicht im richtigen Modus
	}

	// check pattern parameter
	if (pattern->length != mode_data.set_blink.pattern_len) {
		printf("Pattern length mismatch: driver %d, new pattern %d\n",
		       mode_data.set_blink.pattern_len, pattern->length);
		errno = EINVAL;
		return -1;
	}
	if (pattern->pattern_states != mode_data.set_blink.pattern_count) {
		printf("Pattern state mismatch: driver %d, new pattern %d\n",
		       mode_data.set_blink.pattern_count,
		       pattern->pattern_states);
		errno = EINVAL;
		return -1;
	}

	uint16_t data_len = pattern->length * pattern->pattern_states;
	return ws2812_set_led_pixel(fd, 0, data_len, pattern->pattern_data);
}

/**
 * @brief Sends a request to the kernel module to retrieve LED data.
 *
 * This function constructs and sends a request to the kernel module via the provided device file descriptor
 * to retrieve LED data of the specified type. If successful, the kernel module will prepare to return the
 * requested data upon the next read operation.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param data_type Type of LED data to retrieve.
 * @return 0 on success, -1 on error (check errno for specific error).
 */
static int ws2812_send_get_data(int fd, LED_DATA_ID data_type)
{
	// Construct the LED data request
	led_get_data get_len = {
		.ctrl = CHAR_LED_GET_DATA,
		.data_type = data_type,
		.p_len = 3, // unused
	};

	// Send the request to the kernel module
	if (write(fd, &get_len, sizeof(led_get_data)) < 0) {
		return -1;
	}
	return 0;
}

/**
 * @brief Retrieves the current operation mode of the WS2812 LED strip from the kernel module.
 *
 * This function sends a request to the kernel module via the provided device file descriptor
 * to retrieve the current operation mode of the WS2812 LED strip. It reads the response from the kernel
 * module and stores it in the provided structure.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param result Pointer to the structure where the result will be stored.
 * @return The operation mode id on success, -1 on error (check errno for specific error).
 *
 * @note The operation modes determine how the kernel module controls the WS2812 LED strip.
 * The mode ID is represented by the enum constant DATA_MODE defined in dev_packets.h.
 */
int ws2812_get_mode(int fd, led_set_mode *result)
{
	if (ws2812_send_get_data(fd, DATA_MODE)) {
		return -1;
	}

	if (read(fd, result, sizeof(led_set_mode)) < 0) {
		return -1;
	}

	return result->set_mode.mode;
}

/**
 * @brief Retrieves the length of the WS2812 LED strip from the kernel module.
 *
 * This function sends a request to the kernel module via the provided device file descriptor
 * to retrieve the length of the WS2812 LED strip. It reads the response from the kernel
 * module and returns the length of the LED strip.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @return The length of the LED strip on success, -1 on error (check errno for specific error).
 */
extern int ws2812_get_length(int fd)
{
	if (ws2812_send_get_data(fd, DATA_LEN)) {
		return -1;
	}

	led_len result;

	if (read(fd, &result, sizeof(led_len)) < 0) {
		return -1;
	}

	return result.len;
}

/**
 * @brief Retrieves the length of the internal data buffer corresponding to the current mode of the WS2812 LED strip.
 *
 * This function retrieves the length of the internal data buffer corresponding to the current mode of the WS2812 LED strip
 * from the kernel module. The length depends on the current mode of operation, where the static mode saves the pixel data
 * of the LED strip, and the blink mode saves the pattern data.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @return The length of the internal data buffer corresponding to the current mode on success, -1 on error or invalid mode.
 *
 * @note This function determines the appropriate data length based on the current mode of the LED strip.
 */
int ws2812_get_mode_data_length(int fd)
{
	led_set_mode mode;
	int mode_id = ws2812_get_mode(fd, &mode);
	if (mode_id < 0) {
		return mode_id; // error
	}

	int length = 0;
	switch (mode_id) {
	case CHAR_LED_MODE_STATIC:
		length = ws2812_get_length(fd);
		break;
	case CHAR_LED_MODE_BLINK:
		length = mode.set_blink.pattern_count *
			 mode.set_blink.pattern_len;
		break;
	default:
		return -1;
		break;
	}
	return length;
}

/**
 * @brief Requests LED pixel data from the WS2812 LED strip (USB device).
 *
 * This function sends a request to the kernel module via the provided device file descriptor
 * to obtain LED pixel data from the WS2812 LED strip (USB device). It retrieves the length
 * of the LED strip to ensure correct buffer sizes, sends the request to the kernel module,
 * and reads the received data into the result buffer.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param result Pointer to the structure where the LED pixel data will be stored.
 * @return 0 on success, -1 on error (check errno for specific error).
 *
 * @note Ensure that the size of the result buffer matches the length of the LED strip.
 * 		 This function communicates with the USB device to retrieve LED pixel data. 
 * 	     If the buffer length of the result does not match the length of the LED strip, 
 *       the function returns an error.
 */
int ws2812_get_data(int fd, ws2812_pixel_buffer *result)
{
	int length = ws2812_get_length(fd);
	if (length < 0) {
		return length;
	}

	if (result->length != length) {
		errno = EINVAL;
		printf("Pixelbuffersize doesn't match!\n");
		return -1;
	}

	if (ws2812_send_get_data(fd, DATA_PIXEL)) {
		return -1;
	}
	size_t required_transfer_buffer_size =
		sizeof(led_pixel_data) + sizeof(led_pixel) * length;

	if (transfer_buffer_size < required_transfer_buffer_size) {
		uint8_t *new_transfer_buffer =
			realloc(transfer_buffer, required_transfer_buffer_size);
		if (new_transfer_buffer == NULL) {
			return -1;
		}
		transfer_buffer = new_transfer_buffer;
	}

	int read_count =
		read(fd, transfer_buffer, required_transfer_buffer_size);
	if (read_count < 0) {
		return -1;
	}

	memcpy(result->pixel_data, transfer_buffer + sizeof(led_pixel_data),
	       required_transfer_buffer_size - sizeof(led_pixel_data));
	led_pixel_data *header = (led_pixel_data *)transfer_buffer;
	printf("Got led_pixel_data{ offset=%d, count=%d}\n", header->offset,
	       header->led_count);
	return 0;
}

/**
 * @brief Requests mode-specific data from the kernel module.
 *
 * This function sends a request to the kernel module via the provided device file descriptor
 * to obtain mode-specific data from the kernel module. It retrieves the length
 * of the mode-specific data to ensure correct buffer sizes, sends the request to the kernel module,
 * and reads the received data into the result buffer.
 *
 * @param fd File descriptor (device file) for the WS2812 LED strip.
 * @param result Pointer to the structure where the mode-specific data will be stored.
 * @return 0 on success, -1 on error (check errno for specific error).
 *
 * @note Mode-specific data includes the blinking pattern for the blink mode and the LED pixel data
 *       stored inside the kernel module for the static mode. Ensure that the size of the result buffer
 *       matches the length of the mode-specific data.
 */
int ws2812_get_mode_data(int fd, ws2812_pixel_buffer *result)
{
	int length = ws2812_get_mode_data_length(fd);
	if (length < 0) {
		return length;
	}

	if (result->length != length) {
		errno = EINVAL;
		printf("Pixelbuffersize doesn't match!\n");
		return -1;
	}

	if (ws2812_send_get_data(fd, DATA_MODE_PIXEL)) {
		return -1;
	}
	size_t required_transfer_buffer_size =
		sizeof(led_pixel_data) + sizeof(led_pixel) * length;

	if (transfer_buffer_size < required_transfer_buffer_size) {
		uint8_t *new_transfer_buffer =
			realloc(transfer_buffer, required_transfer_buffer_size);
		if (new_transfer_buffer == NULL) {
			return -1;
		}
		transfer_buffer = new_transfer_buffer;
	}

	int read_count =
		read(fd, transfer_buffer, required_transfer_buffer_size);
	if (read_count < 0) {
		return -1;
	}

	memcpy(result->pixel_data, transfer_buffer + sizeof(led_pixel_data),
	       required_transfer_buffer_size - sizeof(led_pixel_data));
	led_pixel_data *header = (led_pixel_data *)transfer_buffer;
	printf("Got led_pixel_data{ offset=%d, count=%d}\n", header->offset,
	       header->led_count);
	return 0;
}