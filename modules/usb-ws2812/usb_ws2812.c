/**
 * @file usb_ws2812.c                                                          *
 * @brief Linux Kernel module to interface with WS2812 Controller via USB.     *
 * It includes handling USB communication and processing of packets.           *
 * @date  Saturday 27th-January-2024                                           *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#include <../../include/linux/init.h>
#include <../../include/linux/kernel.h>
#include <../../include/linux/errno.h>
#include <../../include/linux/slab.h>
#include <../../include/linux/module.h>
#include <../../include/linux/kref.h>
#include <../../include/linux/uaccess.h>
#include <../../include/linux/usb.h>
#include <../../include/linux/mutex.h>
#include <../../include/linux/kthread.h>
#include <../../include/linux/delay.h>
#include <../../include/linux/list.h>
#include "usb_packets.h"
#include "dev_packets.h"

/*==============================================*\
 * DEFINES
\*==============================================*/

#define USB_VENDOR_ID 0xcafe
#define USB_PRODUCT_ID 0x1234

#define PACKET_SIZE 64
#define MAX_WRITES 4

#define DEBUG_MESSAGES // For Debug messages, comment out in production

/*==============================================*\
 * MACROS
\*==============================================*/

#define LOG_INFO(message, ...) \
	printk(KERN_INFO "[USB_WS2812] " message "\n", ##__VA_ARGS__)
#ifdef DEBUG_MESSAGES
#define LOG_DEBUG(caller, message, ...)                             \
	printk(KERN_INFO "[USB_WS2812] (" caller ") " message "\n", \
	       ##__VA_ARGS__)
#else
#define LOG_DEBUG(caller, message, ...)
#endif
#define LOG_ERROR(message, ...) \
	printk(KERN_ERR "[USB_WS2812] " message "\n", ##__VA_ARGS__)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*==============================================*\
 * FORWARD DECLARATIONS
\*==============================================*/

struct ws2812;
static struct usb_driver ws2812_usb_driver;

struct mode_static_s;
struct mode_blink_s;

static void ws2812_usb_write_pixeldata_buffer(struct ws2812 *ws2812_struct);
static inline size_t ws2812_get_mode_packet_size(uint8_t mode_id);
static ssize_t ws2812_dev_file_parse_user_packet(struct ws2812 *ws2812_struct,
						 uint8_t *buffer, ssize_t len,
						 ws2812_usb_packet *packet);

static ssize_t ws2812_ctrl_get_data(struct ws2812 *ws2812_struct,
				    led_get_data *request);
static ssize_t ws2812_ctrl_static_set_length(struct ws2812 *ws2812_struct,
					     uint16_t new_length);
static ssize_t ws2812_ctrl_static_clear(struct ws2812 *ws2812_struct);
static ssize_t ws2812_ctrl_static_set_pixeldata(struct ws2812 *ws2812_struct,
						uint16_t offset,
						uint16_t length,
						led_pixel *data);
static ssize_t ws2812_ctrl_activate_static_mode(struct ws2812 *ws2812_struct,
						led_set_mode *new_mode);
static ssize_t ws2812_ctrl_stop_static_mode(struct ws2812 *ws2812_struct);
static ssize_t ws2812_ctrl_blink_set_length(struct ws2812 *ws2812_struct,
					    uint16_t new_length);
static ssize_t ws2812_ctrl_blink_clear(struct ws2812 *ws2812_struct);
static ssize_t ws2812_ctrl_blink_set_pixeldata(struct ws2812 *ws2812_struct,
					       uint16_t offset, uint16_t length,
					       led_pixel *data);
static ssize_t ws2812_ctrl_activate_blink_mode(struct ws2812 *ws2812_struct,
					       led_set_mode *new_mode);
static ssize_t ws2812_ctrl_stop_blink_mode(struct ws2812 *ws2812_struct);

/*==============================================*\
 * TYPEDEF DECLARATIONS
\*==============================================*/

// Buffer for WS2812 LED pixels, including buffer length, a mutex for thread safety, and a pointer to the pixel data.
typedef struct ws2812_pixel_buffer_s {
	uint16_t len;
	struct mutex buffer_mutex;
	ws2812_pixel *buffer;
} ws2812_pixel_buffer;

// Callback for setting the length of the LED pixel buffer on the USB WS2812 device.
typedef ssize_t (*f_user_length_cb)(struct ws2812 *ws2812_struct,
				    uint16_t new_length);

// Callback for sending a packet of LED pixel data to the USB WS2812 device.
typedef ssize_t (*f_user_packet_cb)(struct ws2812 *ws2812_struct,
				    uint16_t offset, uint16_t length,
				    led_pixel *data);

// Callback for clearing the LED display on the USB WS2812 device.
typedef ssize_t (*f_user_clear_cb)(struct ws2812 *ws2812_struct);

// Callback to stop the current mode of the USB WS2812 device.
typedef ssize_t (*f_user_stop_mode_cb)(struct ws2812 *ws2812_struct);

// Callback to activate a new mode on the USB WS2812 device.
typedef ssize_t (*f_user_activate_mode_cb)(struct ws2812 *ws2812_struct,
					   led_set_mode *new_mode);
// Callback to get data from the USB WS2812 device.
typedef ssize_t (*f_user_get_data_cb)(struct ws2812 *ws2812_struct,
				      led_get_data *request);

/**
 * @brief Structure for WS2812 mode callback functions.
 *
 * This structure contains pointers to callback functions that are used for various operations
 * in different modes. Each callback corresponds to a specific
 * type of packet or event received by the device.
 */
typedef struct ws2812_mode_callbacks_s {
	f_user_length_cb
		dev_packet_length_cb; /**< Callback for length packet handling. */
	f_user_packet_cb
		dev_packet_pixeldata_cb; /**< Called after receiving pixel data packet. */
	f_user_clear_cb
		dev_packet_clear_cb; /**< Called after receiving a clear packet. */
	f_user_activate_mode_cb
		dev_packet_change_mode_activate_cb; /**< Callback for activating a mode. */
	f_user_stop_mode_cb
		dev_packet_change_mode_stop_cb; /**< Callback for stopping a mode. */
	f_user_get_data_cb
		dev_packet_get_data_cb; /**< Callback for data retrieval requests. */
} ws2812_mode_callbacks;

/**
 * @brief State for the DEV-File Packet Parser
 */
typedef enum { STATE_NEW_PACKET = 0, STATE_PIXEL_DATA } PARSE_STATE;

/**
 * @brief This struct represents a read request.
 */
typedef struct ws2812_read_request_s {
	struct list_head list;
	uint8_t type;
} ws2812_read_request;

/*==============================================*\
 * STRUCTS AND UNION DECLARATIONS
\*==============================================*/

/**
 * @brief Data for mode static (empty)
 */
struct mode_static_s {};

/**
 * @brief Data for mode blink
 */
struct mode_blink_s {
	uint8_t running;
	uint8_t pattern_count;
	uint8_t current_pattern;
	uint16_t blink_period; // in ms
	uint16_t pattern_len;
	struct task_struct *blink_thread;
	ws2812_pixel_buffer pattern_data;
};

/**
 * @brief Union for mode specific data
 * 
 */
typedef union modes_u {
	struct mode_static_s mode_static;
	struct mode_blink_s mode_blink;
} mode_data;

/**
 * @brief Representing the state and control of the WS2812 Controller device connected via USB.
 * 
 */
struct ws2812 {
	/* Data for USB Connection */
	struct usb_device *usb_dev; /**< The usb device for this device */
	struct usb_interface *interface; /**< The interface for this device */
	struct semaphore
		limit_sem; /**< Limiting the number of writes in progress */
	struct usb_anchor
		submitted; /**< In case we need to retract our submissions */
	struct urb *bulk_in_urb; /**< the urb to read data with */
	ws2812_usb_packet *bulk_in_pkg; /**< The buffer to receive data */
	ws2812_usb_packet *read_request_pkg;
	size_t bulk_in_size; /**< The size of the receive buffer */
	size_t bulk_in_filled; /**< Number of bytes in the buffer */
	size_t bulk_in_copied; /**< Already copied to user space */
	__u8 bulk_in_endpointAddr; /**< The address of the bulk in endpoint */
	__u8 bulk_out_endpointAddr; /**< The address of the bulk out endpoint */
	int errors; /**< Errors */
	bool ongoing_read; /**< Indicates if a read is going on */
	spinlock_t err_lock; /**< Lock for errors */
	struct kref kref;
	struct mutex io_mutex; /**< Synchronize I/O with disconnect */
	unsigned long disconnected : 1;
	wait_queue_head_t bulk_in_wait; /**< to wait for an ongoing read */

	/* Data for parsing the Device-File Packets */
	PARSE_STATE parse_state;
	size_t parse_pixel_offset;
	size_t parse_pixel_len;
	size_t parse_pixel_next_index;

	/* Mode dependend data */
	ws2812_pixel_buffer *
		parse_data_destination; /**< Specifies in which buffer to write (pattern- / pixeldata) */
	ws2812_mode_callbacks
		*parse_cb; /**< Callbacks for mode specific operations */
	LED_MODE mode; /**< Defines Mode */
	mode_data *mode_data; /**< Saves data for the actual mode */

	/* Others */
	ws2812_pixel_buffer pixeldata; /**< Pixel Buffer */
	struct list_head request_list; /**< List of ws2812_read_request's */
};

/*==============================================*\
 * MODE CALLBACKS ARRAY DEFINITION
\*==============================================*/

/**
 * @brief Array for mode dependend callbacks
 */
static ws2812_mode_callbacks mode_callbacks[CHAR_LED_MODE_LENGTH] = {
	// Mode: Static
	{
		.dev_packet_length_cb = ws2812_ctrl_static_set_length,
		.dev_packet_pixeldata_cb = ws2812_ctrl_static_set_pixeldata,
		.dev_packet_clear_cb = ws2812_ctrl_static_clear,
		.dev_packet_change_mode_activate_cb =
			ws2812_ctrl_activate_static_mode,
		.dev_packet_change_mode_stop_cb = ws2812_ctrl_stop_static_mode,
		.dev_packet_get_data_cb = ws2812_ctrl_get_data,
	},
	// Mode: Blink
	{
		.dev_packet_length_cb = ws2812_ctrl_blink_set_length,
		.dev_packet_pixeldata_cb = ws2812_ctrl_blink_set_pixeldata,
		.dev_packet_clear_cb = ws2812_ctrl_blink_clear,
		.dev_packet_change_mode_activate_cb =
			ws2812_ctrl_activate_blink_mode,
		.dev_packet_change_mode_stop_cb = ws2812_ctrl_stop_blink_mode,
		.dev_packet_get_data_cb = ws2812_ctrl_get_data,
	}
};

/*==============================================*\
 * General functions for the driver
\*==============================================*/

/**
 * @brief Initialize a WS2812 pixel buffer.
 *
 * This function allocates memory for a WS2812 pixel buffer and initializes it.
 * It sets the length of the buffer and allocates the corresponding memory for the pixel data.
 *
 * @param buffer Pointer to the ws2812_pixel_buffer structure that will be initialized.
 * @param length The number of pixels in the buffer.
 *
 * @return Returns 0 on success, or -ENOMEM if memory allocation fails.
 *
 * @warning The function assumes that the 'dest' pointer is valid and does not perform null-checking.
 */
static int ws2812_init_pixel_buffer(ws2812_pixel_buffer *buffer,
				    uint16_t length)
{
	LOG_DEBUG("ws2812_init_pixel_buffer", "length = %d", length);

	ws2812_pixel *data =
		kmalloc_array(length, sizeof(ws2812_pixel), GFP_KERNEL);

	if (!data) {
		return -ENOMEM;
	}

	*buffer = (ws2812_pixel_buffer){
		.len = length,
		.buffer = data,
	};

	mutex_init(&buffer->buffer_mutex);

	return 0;
}

/**
 * @brief Frees the memory allocated for a WS2812 pixel buffer and resets its properties.
 *
 * This function releases the memory allocated for the WS2812 pixel buffer's data and
 * resets the buffer properties. It also ensures thread safety by locking the mutex
 * while modifying the buffer and then unlocks and destroys the mutex.
 *
 * @param dest Pointer to the ws2812_pixel_buffer structure to be deleted.
 *
 * @return Always returns 0 to indicate successful deletion of the buffer.
 *
 * @warning The function assumes that the 'dest' pointer is valid and does not perform null-checking.
 */
static int ws2812_delete_pixel_buffer(ws2812_pixel_buffer *buffer)
{
	LOG_DEBUG("ws2812_delete_pixel_buffer", "");
	mutex_lock(&buffer->buffer_mutex);
	kfree(buffer->buffer);
	buffer->len = 0;
	buffer->buffer = NULL;
	mutex_unlock(&buffer->buffer_mutex);
	mutex_destroy(&buffer->buffer_mutex);
	return 0;
}

/**
 * @brief Resizes an existing WS2812 pixel buffer to a new length.
 *
 * This function adjusts the size of an existing WS2812 pixel buffer. If the new length
 * is different from the old one, it reallocates the buffer to match the new size. It
 * also ensures thread safety by using a mutex during the resize operation. If the buffer
 * is being expanded, new elements are initialized to zero (black).
 *
 * @param buffer Pointer to the ws2812_pixel_buffer structure to resize.
 * @param new_len The new length of the pixel buffer.
 *
 * @return Returns 0 if the buffer is successfully resized or if no resize is needed.
 *         Returns -EIO if memory allocation fails during resizing.
 */
static int ws2812_resize_pixel_buffer(ws2812_pixel_buffer *buffer,
				      uint16_t new_len)
{
	LOG_DEBUG("ws2812_resize_pixel_buffer", "new_len = %d", new_len);
	uint16_t old_len = buffer->len;
	ws2812_pixel *new_buffer = NULL;
	if (old_len == new_len) {
		return 0; // es gibt nichts zu tun.
	}

	mutex_lock(&buffer->buffer_mutex);
	if (buffer->buffer == NULL) {
		new_buffer = kmalloc_array(new_len, sizeof(ws2812_pixel),
					   GFP_KERNEL);
	} else {
		new_buffer = krealloc_array(buffer->buffer, new_len,
					    sizeof(ws2812_pixel), GFP_KERNEL);
	}
	if (!new_buffer) {
		LOG_ERROR("Failed to resize Buffer. Not enough memory!");
		goto error;
	}
	buffer->buffer = new_buffer;
	buffer->len = new_len;
	// Neue Elemente auf 0 setzten
	LOG_INFO("Memzero old_len %d, new_len %d\n", old_len, new_len);

	if (old_len < new_len) {
		memzero_explicit(buffer->buffer + old_len,
				 (new_len - old_len) * sizeof(ws2812_pixel));
	}
	mutex_unlock(&buffer->buffer_mutex);
	return 0;
error:
	mutex_unlock(&buffer->buffer_mutex);
	return -EIO;
}

/**
 * @brief Thread function for the blink mode of a WS2812 LED device.
 *
 * This function implements the blinking behavior for a WS2812 LED device. It repeatedly
 * cycles through patterns defined in the blink mode structure, sleeping for a specified
 * period between each pattern. The function runs in a separate kernel thread and continues
 * until the thread is signaled to stop.
 *
 * @param data Pointer to the ws2812 structure representing the device.
 *
 * @return Always returns 0, indicating the thread exited normally.
 *
 * @note Logs the blink mode configuration (period, pattern length, and pattern count).
 * 
 * @warning The function assumes that the 'dest' pointer is valid and does not perform null-checking.
 *          The function uses mutexes for thread safety, which can potentially lead to deadlocks.
 */
static int ws2812_thread_blink(void *data)
{
	LOG_DEBUG("ws2812_thread_blink", "");
	struct ws2812 *ws2812_struct = (struct ws2812 *)data;

	uint16_t blink_period =
		ws2812_struct->mode_data->mode_blink.blink_period;
	struct mode_blink_s *blink_mode = &ws2812_struct->mode_data->mode_blink;
	LOG_INFO("Modus Blink {%dms, pattern_len %d, pattern_count %d}\n",
		 blink_period, blink_mode->pattern_len,
		 blink_mode->pattern_count);
	int pattern_index = 0;
	while (!kthread_should_stop()) {
		msleep(blink_period);
		LOG_INFO("blink %d\n", pattern_index);

		// copy pattern to pixeldata
		ws2812_pixel_buffer *destination = &ws2812_struct->pixeldata;
		ws2812_pixel_buffer *src = &blink_mode->pattern_data;

		uint16_t start_index = pattern_index * blink_mode->pattern_len;
		uint16_t end_index =
			(pattern_index + 1) * blink_mode->pattern_len;
		uint16_t dest_index = 0;

		// Lock source and destination
		mutex_lock(&destination->buffer_mutex);
		// Deadlock possible!
		mutex_lock(&src->buffer_mutex);
		while (dest_index < destination->len) {
			for (uint16_t i = start_index; i < end_index; i++) {
				if (dest_index >= destination->len)
					break;
				destination->buffer[dest_index++] =
					src->buffer[i];
			}
		}
		mutex_unlock(&src->buffer_mutex);
		mutex_unlock(&destination->buffer_mutex);

		// Send pattern to usb
		ws2812_usb_write_pixeldata_buffer(ws2812_struct);

		pattern_index++;
		pattern_index = pattern_index % blink_mode->pattern_count;
	}

	return 0;
}

/**
 * @brief Get the packet size of a led_mode
 *
 * @param mode_id id if the packet (CHAR_LED_MODE_ID enum)
 * @return size_t Size of the packet.
 */
static inline size_t ws2812_get_mode_packet_size(uint8_t mode_id)
{
	LOG_DEBUG("ws2812_get_mode_packet_size", "");
	switch (mode_id) {
	case CHAR_LED_MODE_STATIC:
		return sizeof(led_set_mode_static);
	case CHAR_LED_MODE_BLINK:
		return sizeof(led_set_mode_blink);
	default:
		return 0;
	}
}

/*============================================================================*\
 * USB communication functions for the driver
\*============================================================================*/

/*======================================*\
 * Read
\*======================================*/

/**
 * @brief Reads a packet from a WS2812 USB device.
 *
 * This function handles the communication with a WS2812 USB device. It sends a request to the 
 * device and then reads the response back. The communication is done using USB bulk transfer. 
 * The function logs the operation and handles any communication errors.
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 * @param request_pkg A pointer to a ws2812_usb_packet structure containing the data to be sent to the USB device.
 * @param recive_pkg A pointer to a ws2812_usb_packet structure where the response from the USB device will be stored.
 *
 * @return On success, returns 0. On error, returns a negative error code.
 */
static int ws2812_usb_read_packet(struct ws2812 *ws2812_struct,
				  ws2812_usb_packet *request_pkg,
				  ws2812_usb_packet *recive_pkg)
{
	LOG_DEBUG("ws2812_usb_read_packet", "");
	int count = 0;

	struct mutex *lock = &ws2812_struct->io_mutex;
	mutex_lock(lock);
	// Request length from USB-device
	int error = usb_bulk_msg(
		ws2812_struct->usb_dev,
		usb_sndbulkpipe(ws2812_struct->usb_dev,
				ws2812_struct->bulk_out_endpointAddr),
		request_pkg, sizeof(ws2812_usb_packet), &count, 1000);
	if (error < 0) {
		return error;
	}
	// Read length from USB-Device
	error = usb_bulk_msg(
		ws2812_struct->usb_dev,
		usb_rcvbulkpipe(ws2812_struct->usb_dev,
				ws2812_struct->bulk_in_endpointAddr),
		recive_pkg, sizeof(ws2812_usb_packet_count), &count, 1000);
	mutex_unlock(lock);
	return error;
}

/**
 * @brief Handles the request to obtain the length of the pixeldata from a WS2812 USB device.
 *
 * This function is responsible for fetching the current and maximum length of the pixeldata 
 * from a WS2812 USB device. It first checks if the user buffer has enough space to store the 
 * LED length data. Then, it sends a request to the device using `ws2812_usb_read_packet` and 
 * retrieves the LED length information. The retrieved data is then formatted and copied back 
 * to the user buffer. 
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 * @param user_buf A buffer where the LED length information will be copied.
 * @param user_buf_len The length of the user buffer.
 *
 * @return On success, returns the size of the LED length data copied to the user buffer. On 
 *         failure, returns a negative error code. Possible error codes include -ENOBUFS if the user 
 *         buffer is too small and -EFAULT if there is an error in copying data to the user space.
 */
static ssize_t ws2812_usb_read_length(struct ws2812 *ws2812_struct,
				      char *user_buf, size_t user_buf_len)
{
	LOG_DEBUG("ws2812_usb_read_length", "user_buf_len = %zu", user_buf_len);
	if (user_buf_len < sizeof(led_len)) {
		return -ENOBUFS; // No buffer space avalible
	}

	memzero_explicit(ws2812_struct->read_request_pkg,
			 sizeof(ws2812_usb_packet));
	ws2812_struct->read_request_pkg->ctrl = REQUEST_LEN;
	// request len from USB-dev
	ssize_t error = ws2812_usb_read_packet(ws2812_struct,
					       ws2812_struct->read_request_pkg,
					       ws2812_struct->bulk_in_pkg);
	if (error < 0) {
		return error;
	}
	ws2812_usb_packet_count *ret_len_pkg =
		(ws2812_usb_packet_count *)ws2812_struct->bulk_in_pkg;

	uint16_t len = (ret_len_pkg->led_count_H << 8) |
		       (ret_len_pkg->led_count_L & 0xFF);
	uint16_t max_len = (ret_len_pkg->max_led_count_H << 8) |
			   (ret_len_pkg->max_led_count_L & 0xFF);
	LOG_DEBUG("ws2812_usb_read_length", "Len: %d, Maxlen: %d", len,
		  max_len);

	led_len len_pkg = {
		.ctrl = CHAR_LED_LEN,
		.len = len,
	};
	if (copy_to_user(user_buf, &len_pkg, sizeof(led_len))) {
		return -EFAULT;
	}
	return sizeof(led_len);
}

/**
 * @brief Handles the request to obtain the current mode settings of a WS2812 USB device.
 *
 * This function retrieves the current mode settings of the WS2812 USB controller and copies this 
 * information into a user-provided buffer. It first determines the size of the packet required 
 * based on the current mode of the device, then checks if the user buffer is large enough. 
 * Depending on the device's mode, it prepares a packet with the relevant mode settings 
 * (e.g., static, blink). The packet is then copied to the buffer. 
 * If the device is in an unrecognized mode, it returns an error.
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 * @param user_buf A buffer where the mode information will be copied.
 * @param len The length of the buffer.
 *
 * @return On success, returns the size of the mode data copied to the user buffer. 
 *         On failure, returns a negative error code. Possible error codes are -ENOBUFS if the 
 *         user buffer is too small, -ENODATA if the device mode is unrecognized, and -EFAULT if 
 *         there is an error in copying data to the user space.
 */
static ssize_t ws2812_usb_read_mode_settings(struct ws2812 *ws2812_struct,
					     char *user_buf, size_t len)
{
	LOG_DEBUG("ws2812_usb_read_mode_settings", "len = %zu", len);
	ssize_t pkg_len = ws2812_get_mode_packet_size(ws2812_struct->mode);
	if (len < pkg_len) {
		return -ENOBUFS; // No buffer space avalible
	}
	union led_set_modes_u mode_pkg;
	switch (ws2812_struct->mode) {
	case CHAR_LED_MODE_STATIC:
		mode_pkg.set_static.ctrl = CHAR_LED_SET_MODE;
		mode_pkg.set_static.mode = CHAR_LED_MODE_STATIC;
		break;
	case CHAR_LED_MODE_BLINK:
		mode_pkg.set_blink.ctrl = CHAR_LED_SET_MODE;
		mode_pkg.set_blink.mode = CHAR_LED_MODE_BLINK;
		mode_pkg.set_blink.blink_period =
			ws2812_struct->mode_data->mode_blink.blink_period;
		mode_pkg.set_blink.pattern_count =
			ws2812_struct->mode_data->mode_blink.pattern_count;
		mode_pkg.set_blink.pattern_len =
			ws2812_struct->mode_data->mode_blink.pattern_len;
		break;
	default:
		return -ENODATA; // Richtig ?
	}
	if (copy_to_user(user_buf, &mode_pkg, pkg_len)) {
		return -EFAULT;
	}
	return pkg_len;
}

/**
 * @brief Reads and copies a block of pixel data from a WS2812 USB device to a buffer.
 *
 * This function requests a specific block of pixel data from a WS2812 USB device and copies it
 * to a buffer. It constructs a request packet to fetch pixel data for the given
 * block index and reads the response. The received WS2812 pixel data is then converted to a generic
 * pixel format and copied into the  buffer.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param block_index Index of the block of pixel data to be read.
 * @param user_buffer Pointer to the buffer where pixel data will be copied.
 *
 * @return On success, returns the total number of bytes copied to the buffer. On failure,
 *         returns a negative error code.
 */
static ssize_t ws2812_usb_read_copy_pixeldata(struct ws2812 *ws2812_struct,
					      uint16_t block_index,
					      uint8_t *user_buffer)
{
	LOG_DEBUG("ws2812_usb_read_copy_pixeldata", "block_index = %d",
		  block_index);
	memzero_explicit(ws2812_struct->read_request_pkg,
			 sizeof(ws2812_usb_packet));

	ws2812_usb_packet_request_led_data *request_pkg =
		(ws2812_usb_packet_request_led_data *)
			ws2812_struct->read_request_pkg;
	request_pkg->ctrl = REQUEST_LED_DATA;
	request_pkg->led_block_index_H = block_index >> 8;
	request_pkg->led_block_index_L = block_index & 0xFF;

	// request len from USB-dev
	ssize_t error = ws2812_usb_read_packet(ws2812_struct,
					       ws2812_struct->read_request_pkg,
					       ws2812_struct->bulk_in_pkg);
	if (error < 0) {
		return error;
	}
	ssize_t copied = 0;

	ws2812_usb_packet_pixeldata *pixel_data_pkg =
		(ws2812_usb_packet_pixeldata *)ws2812_struct->bulk_in_pkg;
	uint16_t end_index = ws2812_struct->pixeldata.len - block_index * 21;

	for (int i = 0; i < end_index && i < 21; i++) {
		// Convert ws2812_pixel to led_pixel
		led_pixel pixel = {
			.red = pixel_data_pkg->color_data[i].red,
			.green = pixel_data_pkg->color_data[i].green,
			.blue = pixel_data_pkg->color_data[i].blue,
		};

		ssize_t not_copied = copy_to_user(user_buffer + copied, &pixel,
						  sizeof(led_pixel));
		if (not_copied != 0) {
			return -EFAULT;
		}
		copied += sizeof(led_pixel);
		LOG_DEBUG("ws2812_usb_read_copy_pixeldata", "[%d] Copied: %ld",
			  i, copied);
	}
	return copied;
}

/**
 * @brief Handles a request for pixel data from a WS2812 USB device.
 *
 * This function manages the process of retrieving pixel data from a WS2812 USB device.
 * It first requests the total length of the pixel data, resizes the pixel buffer if necessary, and
 * then reads the pixel data in blocks. The function constructs a header for the pixel data and
 * copies both the header and the pixel data into a user-provided buffer. The copying is done in
 * blocks to manage potentially large amounts of pixel data.
 * 
 * Copying is done in blocks because usb_bulk_msg only supports 64 Byte!
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param user_buf Pointer to the user buffer where the pixel data will be copied.
 * @param len Length of the user buffer.
 *
 * @return On success, returns the length of the packet data copied to the user buffer. 
 *         On failure, returns a negative error code.
 */
static ssize_t ws2812_usb_read_pixeldata(struct ws2812 *ws2812_struct,
					 char *user_buf, size_t len)
{
	LOG_DEBUG("ws2812_usb_read_pixeldata", "");

	// get length
	memzero_explicit(ws2812_struct->read_request_pkg,
			 sizeof(ws2812_usb_packet));
	ws2812_struct->read_request_pkg->ctrl = REQUEST_LEN;
	// request len from USB-dev
	ssize_t error = ws2812_usb_read_packet(ws2812_struct,
					       ws2812_struct->read_request_pkg,
					       ws2812_struct->bulk_in_pkg);
	if (error < 0) {
		return error;
	}
	ws2812_usb_packet_count *ret_len_pkg =
		(ws2812_usb_packet_count *)ws2812_struct->bulk_in_pkg;

	uint16_t pixel_len = (ret_len_pkg->led_count_H << 8) |
			     (ret_len_pkg->led_count_L & 0xFF);
	if (ws2812_struct->pixeldata.len != pixel_len) {
		LOG_DEBUG("ws2812_usb_read_pixeldata", "Update length!");
		ws2812_resize_pixel_buffer(&ws2812_struct->pixeldata,
					   pixel_len);
	}

	ssize_t pkg_len = sizeof(led_pixel_data) +
			  sizeof(led_pixel) * ws2812_struct->pixeldata.len;
	if (len < pkg_len) {
		return -ENOBUFS;
	}
	led_pixel_data pkg_header = {
		.ctrl = CHAR_LED_PIXEL_DATA,
		.led_count = ws2812_struct->pixeldata.len,
		.offset = 0,
	};

	ssize_t copied = 0;
	// Copy Header
	if (copy_to_user(user_buf, &pkg_header, sizeof(pkg_header))) {
		return -EFAULT;
	}

	// Copy pixel_data
	copied += sizeof(pkg_header);
	int block_count = (pixel_len + 21) / 21;
	for (int i = 0; i < block_count; i++) {
		ssize_t copied_ = ws2812_usb_read_copy_pixeldata(
			ws2812_struct, i, user_buf + copied);
		if (copied_ < 0) {
			return copied_;
		}
		copied += copied_;
	}
	LOG_DEBUG("ws2812_usb_read_pixeldata",
		  "Copied: %ld, expected pkg_len: %ld", copied, pkg_len);

	return pkg_len;
}

/**
 * @brief Handles a request for mode-specific pixel data from a WS2812 USB device.
 *
 * This function responds to a request for pixel data based on the current mode of the WS2812 USB
 * device. It selects the appropriate pixel buffer (static or blink mode) and copies its contents
 * into a user buffer. The function first constructs a header with pixel data information and then
 * copies both the header and pixel data. It verifies if the user buffer is large enough to hold
 * the data.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param user_buf Pointer to the user buffer where pixel data will be copied.
 * @param len Length of the user buffer.
 *
 * @return On success, returns the total size of the data copied to the user buffer. 
 *         On failure, returns a negative error code.
 */
static ssize_t ws2812_usb_read_mode_pixeldata(struct ws2812 *ws2812_struct,
					      char *user_buf, size_t len)
{
	LOG_DEBUG("ws2812_usb_read_mode_pixeldata", "");

	ws2812_pixel_buffer *mode_buffer;

	// Pixelbuffer des Modus auswählen
	switch (ws2812_struct->mode) {
	case CHAR_LED_MODE_STATIC:
		mode_buffer = &ws2812_struct->pixeldata;
		break;
	case CHAR_LED_MODE_BLINK:
		mode_buffer =
			&ws2812_struct->mode_data->mode_blink.pattern_data;
		break;
	default:
		LOG_DEBUG("ws2812_handle_request_mode_pixel_data",
			  "Unknown mode %d", ws2812_struct->mode);
		return -EIO; //
		break;
	}

	ssize_t pkg_len =
		sizeof(led_pixel_data) + sizeof(led_pixel) * mode_buffer->len;
	if (len < pkg_len) {
		return -ENOBUFS;
	}

	led_pixel_data pkg_header = {
		.ctrl = CHAR_LED_PIXEL_DATA,
		.led_count = mode_buffer->len,
		.offset = 0,
	};

	// Copy Header
	if (copy_to_user(user_buf, &pkg_header, sizeof(pkg_header))) {
		return -EFAULT;
	}

	// Copy pixeldata
	if (copy_to_user(user_buf + sizeof(pkg_header), mode_buffer->buffer,
			 sizeof(led_pixel) * mode_buffer->len)) {
		return -EFAULT;
	}

	return pkg_len;
}

/**
 * @brief Handles various read requests for a WS2812 USB device.
 *
 * This function serves as a dispatcher for different types of read requests related to a WS2812 USB
 * device. Depending on the request type (such as data length, mode, pixel data, or mode-specific
 * pixel data), it calls the appropriate handler function and passes along the user buffer for data
 * copying. Each handler function is responsible for a specific aspect of the device's data.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param request Pointer to the ws2812_read_request structure specifying the request type.
 * @param user_buf Pointer to the user buffer where the requested data will be copied.
 * @param len Length of the user buffer.
 *
 * @return On success, returns the size of the data copied to the user buffer. 
 *         On failure, returns a negative error code.
 */
static ssize_t ws2812_usb_read_handle_request(struct ws2812 *ws2812_struct,
					      ws2812_read_request *requset,
					      char *user_buf, size_t len)
{
	LOG_DEBUG("ws2812_usb_read_handle_request", "");
	switch (requset->type) {
	case DATA_LEN:
		return ws2812_usb_read_length(ws2812_struct, user_buf, len);
		break;
	case DATA_MODE:
		return ws2812_usb_read_mode_settings(ws2812_struct, user_buf,
						     len);
		break;
	case DATA_PIXEL:
		return ws2812_usb_read_pixeldata(ws2812_struct, user_buf, len);
		break;
	case DATA_MODE_PIXEL:
		return ws2812_usb_read_mode_pixeldata(ws2812_struct, user_buf,
						      len);
		break;
	default:
		break;
	}
	return -EINVAL; // invalid Argument (type)
}

/**
 * @brief Reads data from a WS2812 USB device.
 *
 * This function performs a bulk read operation from the WS2812 USB device. It reads
 * data into an internal buffer and then copies the data to the user buffer. The function
 * handles the communication with the device using the USB bulk transfer method and is
 * intended to be used as the read operation for a device file in a Linux kernel module.
 *
 * @param file_instance Pointer to the file structure representing the open file instance.
 * @param buf Buffer to which the read data will be copied.
 * @param len Length of the data to be read.
 * @param ofs Offset for the read operation (not yet used in this function).
 *
 * @return Number of bytes successfully read, or -EFAULT if the copy to user space fails.
 *
 * @warning The function uses `usb_bulk_msg` which is not interruptible.
 */
static ssize_t ws2812_usb_read(struct file *file_instance, char *user_buf,
			       size_t len, loff_t *ofs)
{
	LOG_DEBUG("ws2812_usb_read", "len = %ld", len);
	struct ws2812 *ws2812_struct;
	ws2812_struct = file_instance->private_data;
	int read_count = 0;

	if (list_empty(&ws2812_struct->request_list)) {
		LOG_DEBUG("ws2812_usb_read", "request list empty!");
		return 0;
	}

	ws2812_read_request *request = list_first_entry(
		&ws2812_struct->request_list, ws2812_read_request, list);
	read_count = ws2812_usb_read_handle_request(ws2812_struct, request,
						    user_buf, len);

	// Request processed => Delete request from list
	list_del(&request->list);
	kfree(request);

	return read_count;
}

/*======================================*\
 * Write
\*======================================*/

/**
 * @brief Callback function for handling completion of a write operation to a WS2812 USB controller.
 *
 * This function is called when a USB write operation completes. It cleans-up the coherent memory 
 * allocated for the transfer. The function is typically registered as a callback when initiating 
 * a URB.
 *
 * @param urb Pointer to the URB (USB Request Block) structure representing the (completed) USB transfer.
 *
 * @warning This function does not handle errors that may have occurred during the USB transfer.
 */
static void ws2812_usb_write_callback(struct urb *urb)
{
	LOG_DEBUG("ws2812_usb_write_callback", "");
	struct ws2812 *ws2812_struct;
	ws2812_struct = urb->context;
	// TODO Handle errors that may have occurred during the USB transfer
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	// Semaphore for write -1;
}

/**
 * @brief Sends packet to a WS2812 USB device.
 *
 * This function handles sending a packet to the WS2812 USB controller. It allocates an URB
 * and a buffer for the packet, then submits the URB for transmission. The function also includes error handling
 * for USB device disconnection and memory allocation failures.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param packet Pointer to the ws2812_usb_packet structure containing the data to be sent.
 *
 * @return The size of the packet sent in bytes on successful transmission, or a negative error code on failure.
 *
 * @note Error handling includes logging relevant errors and cleaning up allocated resources.
 * 
 * @warning The function expects the size of ws2812_usb_packet to be 64 bytes and checks this with a WARN_ON statement.
 */
static size_t ws2812_usb_write_packet(struct ws2812 *ws2812_struct,
				      ws2812_usb_packet *packet)
{
	LOG_DEBUG("ws2812_usb_write_packet", "");
	struct urb *urb = NULL;
	size_t retVal = 0;
	size_t packet_len = sizeof(ws2812_usb_packet);
	char *urb_packet_buffer = NULL;
	if (ws2812_struct == NULL) {
		LOG_ERROR("Error while writing to usb: No USB-Device");
		retVal = -ENODEV;
		goto err;
	}
	// USB Packet has to be 64 bytes
	WARN_ON(sizeof(ws2812_usb_packet) != 64);
	// TODO: Look for file flags. (nonblocking io,...)

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		LOG_ERROR("Error while writing to usb: No Memory for URB");
		retVal = -ENOMEM;
		goto err;
	}

	urb_packet_buffer = usb_alloc_coherent(ws2812_struct->usb_dev,
					       packet_len, GFP_KERNEL,
					       &urb->transfer_dma);
	if (!urb_packet_buffer) {
		LOG_ERROR(
			"Error while writing to usb: No Memory for USB Packet Buffer");
		retVal = -ENOMEM;
		goto err_free_urb;
	}
	memcpy(urb_packet_buffer, packet, packet_len);

	mutex_lock(&ws2812_struct->io_mutex);
	// Prüfe ob das USB-Device noch existiert.
	if (ws2812_struct->disconnected) {
		mutex_unlock(&ws2812_struct->io_mutex);
		LOG_ERROR(
			"Error while writing to usb: USB-Device disconnected!");
		retVal = -ENODEV;
		goto err_fre_packet_buffer;
	}
	usb_fill_bulk_urb(urb, ws2812_struct->usb_dev,
			  usb_sndbulkpipe(ws2812_struct->usb_dev,
					  ws2812_struct->bulk_out_endpointAddr),
			  urb_packet_buffer, packet_len,
			  ws2812_usb_write_callback, ws2812_struct);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	// usb_anchor_urb
	retVal = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&ws2812_struct->io_mutex);
	if (retVal) {
		LOG_ERROR("Error while submitting URB");
		goto err_fre_packet_buffer;
	}
	usb_free_urb(urb); // Reference to delete URB after completion
	return packet_len;

err_fre_packet_buffer:

	usb_free_coherent(ws2812_struct->usb_dev, packet_len, urb_packet_buffer,
			  urb->transfer_dma);
err_free_urb:
	usb_free_urb(urb);
err:
	return retVal;
}

/**
 * @brief Sends the pixel data buffer (ws2812_struct->pixeldata_buffer) to a WS2812 USB device.
 *
 * This function iterates over the pixel data buffer of the WS2812 device and sends
 * each segment of pixel data in packets. It constructs packets of pixel data and
 * handles the transmission using the ws2812_usb_write_packet function. The function ensures
 * thread safety by locking a mutex during the operation.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 *
 * @warning The function locks a mutex, so care should be taken to avoid deadlocks in multithreaded environments.
 */
static void ws2812_usb_write_pixeldata_buffer(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_usb_write_pixeldata_buffer", "");
	size_t led_count = ws2812_struct->pixeldata.len;
	ws2812_pixel *pixeldata = ws2812_struct->pixeldata.buffer;
	ws2812_usb_packet_pixeldata pixeldata_packet;
	struct mutex *lock = &ws2812_struct->pixeldata.buffer_mutex;

	mutex_lock(lock);
	for (int index = 0; index < led_count;) {
		int packet_index = 0;
		// Beim senden sollten ungenutzte led daten gleich 0 sein!
		// aus irgendeinem Grund trifft dies nicht zu
		memzero_explicit(&pixeldata_packet,
				 sizeof(ws2812_usb_packet_pixeldata));
		pixeldata_packet.ctrl = LED_DATA;
		for (; index + packet_index < led_count && packet_index < 21;
		     packet_index++) {
			pixeldata_packet.color_data[packet_index] =
				pixeldata[index + packet_index];
			if (index + packet_index >= led_count) {
				break;
			}
		}
		ws2812_usb_write_packet(ws2812_struct,
					(ws2812_usb_packet *)&pixeldata_packet);
		index += packet_index;
	}
	mutex_unlock(lock);
}

/**
 * @brief Writes data from user space to the WS2812 USB device.
 *
 * This function writes data received from user space to the WS2812 USB device. It allocates a buffer
 * to copy user data, then parses and processes the data in the form of packets. The function handles
 * each packet by calling `ws2812_dev_file_parse_user_packet`. It ensures that all packets are processed
 * sequentially until all user data is handled.
 *
 * @param file Pointer to the file structure representing the open file.
 * @param user_buf Pointer to the user buffer containing data to be written.
 * @param len Length of the data in the user buffer.
 * @param off_set Pointer to a long offset type, indicating the file position the user is accessing.
 *
 * @return On success, returns 0 indicating that all data was processed. On failure, returns a
 *         negative error code.
 */
static ssize_t ws2812_usb_write(struct file *file, const char *user_buf,
				size_t len, loff_t *off_set)
{
	LOG_DEBUG("ws2812_usb_write", "len = %ld", len);
	struct ws2812 *ws2812_struct;
	uint8_t *buf = NULL;
	ws2812_struct = file->private_data;

	ws2812_usb_packet packet;
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	if (copy_from_user(buf, user_buf, len)) {
		return -EFAULT;
	}

	ssize_t bytes_read = 0;
	uint8_t *next_packet = buf;
	ssize_t bytes_left = len;
	// parse the userdata
	while (bytes_left > 0) {
		bytes_read = ws2812_dev_file_parse_user_packet(
			ws2812_struct, next_packet, bytes_left, &packet);
		if (bytes_read <= 0) {
			return bytes_read;
		}
		next_packet += bytes_read;
		bytes_left -= bytes_read;
	}

	return 0;
	// test:
	if (copy_from_user(&packet, user_buf, sizeof(ws2812_usb_packet))) {
		return -EFAULT;
	}

	return ws2812_usb_write_packet(ws2812_struct, &packet);
}

/*============================================================================*\
 * WS2812 Control functions for the driver
\*============================================================================*/

/**
 * @brief Stops the current operational mode of a WS2812 USB controller.
 *
 * This function invokes the stop mode callback corresponding to the current operational
 * mode. It is used to cleanly terminate the current mode's operation, such as stopping 
 * a blinking pattern or other LED behaviors.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 *
 * @return The return value from the stop mode callback, indicating success or
 *         error status.
 *
 * @warning Assumes that the device's mode is within the bounds of the mode_callbacks array. 
 *          The function does no array index validation atm.
 */
static ssize_t ws2812_ctrl_stop_current_mode(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_ctrl_stop_current_mode", "");
	// TODO Check array index
	f_user_stop_mode_cb stop_mode_cb =
		mode_callbacks[ws2812_struct->mode]
			.dev_packet_change_mode_stop_cb;
	return stop_mode_cb(ws2812_struct);
}

/**
 * @brief Change the active mode for a WS2812 USB device.
 *
 * This function sets a new mode for the WS2812 USB device by invoking the appropriate
 * mode activation callback based on the requested mode. 
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param new_mode Pointer to the led_set_mode structure containing the new mode to be set.
 *
 * @return The return value from the mode activation callback, indicating success or
 *         error status.
 *
 * @warning The function does no array index validation atm.
 */
static ssize_t ws2812_ctrl_start_mode(struct ws2812 *ws2812_struct,
				      led_set_mode *new_mode)
{
	LOG_DEBUG("ws2812_ctrl_start_mode", "mode = %d",
		  new_mode->set_mode.mode);

	LED_MODE new_mode_id = new_mode->set_mode.mode;
	// TODO Check array index
	f_user_activate_mode_cb start_mode_cb =
		mode_callbacks[new_mode_id].dev_packet_change_mode_activate_cb;
	return start_mode_cb(ws2812_struct, new_mode);
}

/*======================================*\
 * Mode Callbacks
\*======================================*/

// ==========  All Modes  ==========

/**
 * @brief Processes a data retrieval from a WS2812 USB controller.
 *
 * This function handles a request to retrieve data from a WS2812 USB device. It allocates a new
 * read request structure, initializes it based on the provided request package, and adds it to the
 * device's list of pending read requests.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param request_pkg Pointer to the led_get_data structure containing the request details.
 *
 * @return Returns 0 on successful addition of the request to the list.
 *         Returns -ENOMEM if memory allocation for the new request fails.
 */
static ssize_t ws2812_ctrl_get_data(struct ws2812 *ws2812_struct,
				    led_get_data *request_pkg)
{
	ws2812_read_request *new_request =
		kmalloc(sizeof(ws2812_read_request), GFP_KERNEL);
	if (!new_request) {
		LOG_DEBUG("ws2812_ctrl_get_data", "out of memory");
		return -ENOMEM;
	}
	// init new Request
	new_request->type = request_pkg->data_type;

	list_add_tail(&new_request->list, &ws2812_struct->request_list);

	LOG_DEBUG("ws2812_ctrl_get_data", "Requestlist:");
	struct list_head *pos;
	list_for_each(pos, &ws2812_struct->request_list) {
		ws2812_read_request *r =
			list_entry(pos, ws2812_read_request, list);
		LOG_DEBUG("ws2812_ctrl_get_data", "	request(type=%d)",
			  r->type);
	};

	return 0;
}

// ==========  Mode: Static  ==========

/**
 * @brief Sets the length of the WS2812 LED strip in static mode and updates the device accordingly.
 *
 * This function sets a new length for the WS2812 LED strip. It sends a packet to the USB device
 * to inform it of the new LED count and resizes the pixel data buffer to match this length. The
 * function handles communication with the device and updates the internal buffer structure.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param length The new length (number of LEDs) to be set for the WS2812 LED strip.
 *
 * @return Always returns 0, indicating the operation was successful.
 *
 * @warning Only call the function when you're in static mode!
 */
static ssize_t ws2812_ctrl_static_set_length(struct ws2812 *ws2812_struct,
					     uint16_t length)
{
	LOG_DEBUG("ws2812_ctrl_static_set_length", "length = %d", length);

	ws2812_usb_packet_count count_packet =
		(ws2812_usb_packet_count){ .ctrl = LED_COUNT,
					   .led_count_H = (length >> 8),
					   .led_count_L = (length & 0xFF) };
	// Länge des Pixelbuffers anpassen.
	ws2812_resize_pixel_buffer(&ws2812_struct->pixeldata, length);

	ws2812_usb_write_packet(ws2812_struct,
				(ws2812_usb_packet *)&count_packet);
	ws2812_usb_write_pixeldata_buffer(ws2812_struct);

	LOG_INFO("USB length packet sent");

	return 0;
}

/**
 * @brief Clears the WS2812 LED strip connected to the USB device in static mode.
 *
 * This function sends a 'clear' packet to the WS2812 USB device, instructing it to turn off
 * all LEDs. The packet sent contains a control command to clear the LED strip.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 *
 * @return Always returns 0, indicating the clear operation was successfully initiated.
 *
 * @warning Only call the function when you're in static mode!
 */
static ssize_t ws2812_ctrl_static_clear(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_ctrl_static_clear", "");
	ws2812_usb_packet clear_packet = {
		.ctrl = LED_CLEAR,
	};

	ws2812_usb_write_packet(ws2812_struct, &clear_packet);
	LOG_INFO("USB clear packet sent.");
	return 0;
}

/**
 * @brief Sets pixel data in static mode (with optional offset)
 *
 * This function updates the WS2812 LED strip starting at a given offset and spanning
 * a specified length. It checks if the data to be set fits within the current pixel buffer and
 * then copies the new pixel data into the buffer. After updating the buffer, it sends the data to
 * the USB device to update the LED strip.
 *
 * @param ws2812_struct Pointer to the ws2812 structure representing the USB device.
 * @param offset The starting position in the pixel buffer where the data update begins.
 * @param length The number of pixels to update.
 * @param data Pointer to an array of led_pixel structures containing the new pixel data.
 *
 * @return Returns 0 on successful update of the pixel data, or -EMSGSIZE if the data exceeds the buffer size.
 *
 * @warning Only call the function when you're in static mode!
 */
static ssize_t ws2812_ctrl_static_set_pixeldata(struct ws2812 *ws2812_struct,
						uint16_t offset,
						uint16_t length,
						led_pixel *data)
{
	LOG_DEBUG("ws2812_ctrl_static_set_pixeldata",
		  "offset = %d, length = %d", offset, length);

	// check destination length
	size_t update_length = offset + length;
	if (update_length > ws2812_struct->pixeldata.len) {
		LOG_ERROR("Received data doesn't fit in buffer");
		return -EMSGSIZE; // Paket zu lang!
	}
	// copy data
	mutex_lock(&ws2812_struct->pixeldata.buffer_mutex);
	for (int i = 0; i < length; i++) {
		ws2812_struct->pixeldata.buffer[offset + i].red = data[i].red;
		ws2812_struct->pixeldata.buffer[offset + i].green =
			data[i].green;
		ws2812_struct->pixeldata.buffer[offset + i].blue = data[i].blue;
	}
	mutex_unlock(&ws2812_struct->pixeldata.buffer_mutex);
	// write to device
	ws2812_usb_write_pixeldata_buffer(ws2812_struct);

	LOG_INFO("USB pixel data packet(s) sent\n");

	return 0;
}

/**
 * @brief Activates the static mode on a WS2812 USB device.
 *
 * This function sets the operating mode of a WS2812 USB controller to static. 
 * It updates the device's mode to 'CHAR_LED_MODE_STATIC' and assigns the corresponding callback 
 * function from the mode_callbacks array. The function logs this mode change for debugging and 
 * informational purposes.
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 * @param new_mode A pointer to the led_set_mode structure representing the new mode to be activated. 
 *                 Currently, this parameter is not used in the function, but it's included for 
 *                 potential future use and consistency with similar mode-setting functions.
 *
 * @return Always returns 0, indicating successful activation of the static mode.
 *
 * @note This function is part of a series of functions designed to handle different LED modes for 
 *       a WS2812 USB device. It is specifically tailored for activating the static mode and logs 
 *       the mode change as an informational message.
 */
static ssize_t ws2812_ctrl_activate_static_mode(struct ws2812 *ws2812_struct,
						led_set_mode *new_mode)
{
	LOG_DEBUG("ws2812_ctrl_activate_static_mode", "");
	ws2812_struct->mode = CHAR_LED_MODE_STATIC;
	ws2812_struct->parse_cb = &mode_callbacks[CHAR_LED_MODE_STATIC];
	LOG_INFO("Started mode %d", ws2812_struct->mode);
	return 0;
}

/**
 * @brief Placeholder for stopping the static mode (doesn't have to be stopped)
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 *
 * @return Always returns 0, indicating successful deactivation of the static mode.
 *
 * @note This function is part of a series of functions designed to handle different LED modes for 
 *       a WS2812 USB device. It is specifically tailored for deactivating the static mode.
 */
static ssize_t ws2812_ctrl_stop_static_mode(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_ctrl_stop_static_mode", "");
	return 0;
}

// ==========  Mode: Blink  ==========

/**
 * @brief Sets the length of the blink pattern for a WS2812 USB controller in blink mode.
 *
 * This function configures the length of the blink pattern for the WS2812 USB device. 
 * It prepares a packet with the new length and sends it to the device. 
 * Additionally, it adjusts the size of the pixel buffer to match the new length. 
 * The function logs both the action of setting the length and the successful transmission of this 
 * information to the USB device.
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 * @param length The new length of the pixeldata.
 *
 * @return Always returns 0, indicating successful execution of the function.
 *
 * @note This function is specifically used for configuring the blink mode of the WS2812 USB device. 
 *       It ensures that the device and the driver's pixel buffer are in sync with the desired length.
 *
 * @warning Ensure that the `length` parameter does not exceed the maximum allowed size for the 
 *          pixel buffer, as this function does not perform explicit bounds checking. 
 *          Exceeding the buffer size can lead to undefined behavior or potential data corruption.
 */
static ssize_t ws2812_ctrl_blink_set_length(struct ws2812 *ws2812_struct,
					    uint16_t length)
{
	LOG_DEBUG("ws2812_ctrl_blink_set_length", "length = %d", length);

	ws2812_usb_packet_count count_packet =
		(ws2812_usb_packet_count){ .ctrl = LED_COUNT,
					   .led_count_H = (length >> 8),
					   .led_count_L = (length & 0xFF) };
	// Länge des Pixelbuffers anpassen.
	ws2812_resize_pixel_buffer(&ws2812_struct->pixeldata, length);

	ws2812_usb_write_packet(ws2812_struct,
				(ws2812_usb_packet *)&count_packet);

	LOG_INFO("USB length Packet sent\n");

	return 0;
}

/**
 * @brief Clears the blink pattern and resets the WS2812 USB device to static mode.
 *
 * This function sends a command to the WS2812 USB device to clear any existing blink pattern. 
 * It then stops the current mode of the device and activates the static mode. 
 * This is achieved by calling a predefined callback function for changing the device's mode to 
 * static. After changing the mode, a 'clear' packet is sent to the device to ensure that any 
 * residual data is cleared.
 *
 * @param ws2812_struct A pointer to the ws2812 structure representing the USB device.
 *
 * @return Returns 0 on successful execution of the function. 
 *         If an error occurs while stopping the current mode, the function will return the 
 *         corresponding error code.
 */
static ssize_t ws2812_ctrl_blink_clear(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_ctrl_blink_clear", "");

	ssize_t error = 0;
	ws2812_usb_packet clear_packet = {
		.ctrl = LED_CLEAR,
	};
	error = ws2812_ctrl_stop_current_mode(ws2812_struct);
	if (error < 0) {
		return error;
	}
	f_user_activate_mode_cb activate_static_mode =
		mode_callbacks[CHAR_LED_MODE_STATIC]
			.dev_packet_change_mode_activate_cb;
	activate_static_mode(ws2812_struct, NULL);

	ws2812_usb_write_packet(ws2812_struct, &clear_packet);
	LOG_INFO("USB clear packet sent");

	return 0;
}

/**
 * @brief Updates blink pattern pixel data for a WS2812 USB controller.
 *
 * This function updates the blink pattern for a WS2812 USB controller by copying new pixel data
 * into the device's buffer at a specified offset. It checks if the data fits within the buffer's
 * size limits. The function locks the buffer for thread safety during the update. It logs the operation
 * and reports any potential buffer overflows.
 *
 * @param ws2812_struct Pointer to ws2812 structure representing the USB device.
 * @param offset Starting index in the buffer for new data.
 * @param length Number of pixels to be copied.
 * @param data Pointer to array of led_pixel data.
 *
 * @return Returns 0 on success. 
 *         Returns a negative error code on failure.
 *
 * @warning Ensure that the offset and length do not cause a buffer overflow. The function currently
 *          does not handle such scenarios gracefully and may return -EMSGSIZE.
 */
static ssize_t ws2812_ctrl_blink_set_pixeldata(struct ws2812 *ws2812_struct,
					       uint16_t offset, uint16_t length,
					       led_pixel *data)
{
	LOG_DEBUG("ws2812_ctrl_blink_set_pixeldata", "offset = %d, length = %d",
		  offset, length);

	// check destination length
	size_t update_length = offset + length;
	if (update_length >
	    ws2812_struct->mode_data->mode_blink.pattern_data.len) {
		LOG_ERROR("Received data doesn't fit in buffer");
		return -EMSGSIZE; // Buffer to small
	}
	// copy data
	mutex_lock(
		&ws2812_struct->mode_data->mode_blink.pattern_data.buffer_mutex);
	for (int i = 0; i < length; i++) {
		ws2812_struct->mode_data->mode_blink.pattern_data
			.buffer[offset + i]
			.red = data[i].red;
		ws2812_struct->mode_data->mode_blink.pattern_data
			.buffer[offset + i]
			.green = data[i].green;
		ws2812_struct->mode_data->mode_blink.pattern_data
			.buffer[offset + i]
			.blue = data[i].blue;
	}
	mutex_unlock(
		&ws2812_struct->mode_data->mode_blink.pattern_data.buffer_mutex);

	LOG_INFO("Blink pattern data saved");

	return 0;
}

/**
 * @brief Activates blink mode for a WS2812 USB controller.
 *
 * This function sets the device's mode to blink and initializes the required data structures. It
 * allocates memory for mode-specific data, initializes the pixel buffer for blink patterns, and
 * creates a kernel thread to handle blinking. The function logs the mode activation and any errors
 * encountered during the thread creation.
 *
 * @param ws2812_struct Pointer to ws2812 structure representing the USB device.
 * @param new_mode Pointer to led_set_mode structure with the new mode settings.
 *
 * @return Returns 0 on successful activation or an error code on failure.
 *
 * @warning This function performs memory allocation and thread creation, which can fail. Proper
 *          error handling is essential. Currently, the function does not have a mechanism to free the
 *          allocated memory if thread creation fails, which can lead to memory leaks.
 */
static ssize_t ws2812_ctrl_activate_blink_mode(struct ws2812 *ws2812_struct,
					       led_set_mode *new_mode)
{
	LOG_DEBUG("ws2812_ctrl_activate_blink_mode", "");

	uint16_t patter_buffer_len = new_mode->set_blink.pattern_count *
				     new_mode->set_blink.pattern_len;

	mode_data *mode_data = kmalloc(sizeof(union modes_u), GFP_KERNEL);
	if (!mode_data) {
		LOG_ERROR("Out of memory!");
		return -ENOMEM;
	}

	mode_data->mode_blink = (struct mode_blink_s){
		.blink_period = new_mode->set_blink.blink_period,
		.pattern_count = new_mode->set_blink.pattern_count,
		.current_pattern = 0,
		.pattern_len = new_mode->set_blink.pattern_len,
		.running = 0,
	};

	ws2812_init_pixel_buffer(&mode_data->mode_blink.pattern_data,
				 patter_buffer_len);
	ws2812_struct->mode = CHAR_LED_MODE_BLINK;
	memzero_explicit(mode_data->mode_blink.pattern_data.buffer,
			 sizeof(ws2812_pixel) *
				 mode_data->mode_blink.pattern_data.len);

	// Den Thread anlegen.
	mode_data->mode_blink.blink_thread =
		kthread_create(ws2812_thread_blink, (void *)ws2812_struct,
			       "ws2812_thread_blink");
	if (!mode_data->mode_blink.blink_thread) {
		LOG_ERROR("Failed to create thread for blinking");
		return -1; // TODO Better error code
	}
	// Starte den Thread
	wake_up_process(mode_data->mode_blink.blink_thread);

	ws2812_struct->mode = CHAR_LED_MODE_BLINK;
	ws2812_struct->mode_data = mode_data;
	ws2812_struct->parse_data_destination =
		&ws2812_struct->mode_data->mode_blink.pattern_data;
	ws2812_struct->parse_cb = &mode_callbacks[CHAR_LED_MODE_BLINK];
	LOG_INFO("Started mode %d with thread", ws2812_struct->mode);
	return 0;
}

/**
 * @brief Stops the blink mode on a WS2812 USB device.
 *
 * It stops the kernel thread handling the blink pattern and frees the associated memory resources. 
 * The function logs any errors encountered during thread termination and confirms the successful 
 * stopping of the mode and thread termination.
 *
 * @param ws2812_struct Pointer to ws2812 structure representing the USB device.
 *
 * @return Returns 0 on successful termination or the error code on failure.
 */
static ssize_t ws2812_ctrl_stop_blink_mode(struct ws2812 *ws2812_struct)
{
	LOG_DEBUG("ws2812_ctrl_stop_blink_mode", "");
	int error =
		kthread_stop(ws2812_struct->mode_data->mode_blink.blink_thread);
	if (error) {
		LOG_ERROR("Error while stopping thread!");
		return error;
	}
	ws2812_delete_pixel_buffer(
		&ws2812_struct->mode_data->mode_blink.pattern_data);
	kfree(ws2812_struct->mode_data);
	LOG_INFO("Stopped mode %d and killed thread", ws2812_struct->mode);
	return 0;
}

/*============================================================================*\
 * Device File functions
\*============================================================================*/

/**
 * @brief Parses a packet received from the user and performs the corresponding action.
 *
 * This function handles different types of packets received from the user, such as setting the
 * length, pixeldata, mode, etc. It validates the size of each packet type and calls the
 * appropriate callback function to process the data. The function also handles packet parsing errors
 * and logs them.
 *
 * @param ws2812_struct Pointer to ws2812 structure representing the USB device.
 * @param buffer Pointer to the buffer containing the user packet.
 * @param len Length of the data in the buffer.
 * @param packet Pointer to a ws2812_usb_packet structure for returning the parsed data.
 *
 * @return On success, returns the number of bytes read from the buffer. On failure, returns a
 * negative error code.
 */
static ssize_t ws2812_dev_file_parse_user_packet(struct ws2812 *ws2812_struct,
						 uint8_t *buffer, ssize_t len,
						 ws2812_usb_packet *packet)
{
	LOG_DEBUG("ws2812_dev_file_parse_user_packet", "");
	ssize_t error = 0;
	ssize_t bytes_read = 0;
	uint8_t ctrl = buffer[0];

	switch (ctrl) {
	case CHAR_LED_LEN:
		if (len < sizeof(led_len)) {
			LOG_ERROR(
				"Parsing of Length packet failed. Too small!");
			return EBADMSG; // Kein vollständiges Paket!
		}
		led_len *led_len_packet = (led_len *)(buffer);

		// Callback aufrufen
		f_user_length_cb len_cb =
			ws2812_struct->parse_cb->dev_packet_length_cb;
		error = len_cb(ws2812_struct, led_len_packet->len);
		if (error < 0) {
			return error;
		}

		bytes_read += sizeof(led_len);

		break;
	case CHAR_LED_PIXEL_DATA:
		// Prüfe ob der Header in die restlichen Daten passt.
		if (len < sizeof(led_pixel_data)) {
			LOG_ERROR(
				"Parsing of Pixeldata packet failed. Too small!");
			return -1;
		}
		// Header lesen
		led_pixel_data *led_pixel_header = (led_pixel_data *)(buffer);
		bytes_read += sizeof(led_pixel_data);

		// Prüfen ob genug Payloaddaten gesendet wurden.
		size_t led_pixel_len =
			sizeof(led_pixel) * led_pixel_header->led_count;
		if (led_pixel_len > len - bytes_read) {
			LOG_ERROR(
				"Parsing of Pixeldata packet failed. Too small! Expected: %ld, got: %ld",
				led_pixel_len, len - bytes_read);
			return EBADMSG; // Kein vollständiges Paket!
		}
		// Callback aufrufen
		f_user_packet_cb pixel_cb =
			ws2812_struct->parse_cb->dev_packet_pixeldata_cb;
		error = pixel_cb(ws2812_struct, led_pixel_header->offset,
				 led_pixel_header->led_count,
				 (led_pixel *)(buffer + bytes_read));
		if (error < 0) {
			return error;
		}

		bytes_read += led_pixel_len;

		break;
	case CHAR_LED_CLEAR:
		if (len < sizeof(led_clear)) {
			LOG_ERROR("Parsing of Clear packet failed. Too small!");
			return EBADMSG; // Kein vollständiges Paket!
		}

		// Callback aufrufen
		f_user_clear_cb clear_cb =
			ws2812_struct->parse_cb->dev_packet_clear_cb;
		error = clear_cb(ws2812_struct);
		if (error < 0) {
			return error;
		}
		bytes_read += sizeof(led_clear);

		break;
	case CHAR_LED_SET_MODE:

		if (len < sizeof(led_set_mode_s)) {
			LOG_ERROR("Parsing of Mode packet failed. Too small!");
			return EBADMSG; // Kein vollständiges Paket!
		}
		led_set_mode_s *new_mode_packet = (led_set_mode_s *)buffer;
		uint8_t new_mode_id = new_mode_packet->mode;
		size_t packet_size = ws2812_get_mode_packet_size(new_mode_id);
		if (len < packet_size) {
			LOG_ERROR("Parsing of Mode packet failed. Too small!");
			return EBADMSG; // Kein vollständiges Paket!
		}
		led_set_mode *new_mode = (led_set_mode *)buffer;
		bytes_read += packet_size;

		// in dieser Funktion wird das Callback zum Stoppen aufgerufen
		error = ws2812_ctrl_stop_current_mode(ws2812_struct);
		if (error < 0) {
			return error;
		}
		// in dieser Funktion wird das Callback zum starten aufgerufen
		error = ws2812_ctrl_start_mode(ws2812_struct, new_mode);
		if (error < 0) {
			return error;
		}
		break;

	case CHAR_LED_GET_DATA:
		if (len < sizeof(led_get_data)) {
			LOG_ERROR(
				"Parsing of data request packet failed. Too small!");
			return EBADMSG; // Kein vollständiges Paket!
		}
		led_get_data *p_request = (led_get_data *)buffer;

		// Callback aufrufen
		f_user_get_data_cb get_data_cb =
			ws2812_struct->parse_cb->dev_packet_get_data_cb;
		error = get_data_cb(ws2812_struct, p_request);
		if (error < 0) {
			return error;
		}
		break;
	default:
		LOG_ERROR("Parsing failed! Unknown packet ctrl: %d", ctrl);
		return -EBADRQC; // Paket ist nicht bekannt!
	}

	return bytes_read;
}

/*============================================================================*\
 * Driver (De-)Init Functions and Registrations
\*============================================================================*/

/**
 * @brief This function is called when a devfile associated with the WS2812 USB controller is opened. 
 * 
 * It finds the USB interface corresponding to the minor number of the inode and retrieves the associated USB
 * device structure. It increases the reference count of the device to prevent it from being
 * released while it's in use. The function logs both successful and unsuccessful attempts to open
 * the device.
 *
 * @param inode Pointer to the inode structure representing the device file.
 * @param instance Pointer to the file structure representing the opened file.
 *
 * @return Returns 0 on successful opening of the file. 
 *         Returns -ENODEV if the device or interface is not found.
 *
 * @warning The function currently does not prevent USB autosuspend, which may affect power management
 *          on some devices and could potentially limit the current to the controller.
 */
static int ws2812_dev_file_open(struct inode *inode, struct file *instance)
{
	LOG_DEBUG("ws2812_dev_file_open", "");
	struct ws2812 *ws2812_struct;
	struct usb_interface *interface;
	int subminor;

	subminor = iminor(inode);
	interface = usb_find_interface(&ws2812_usb_driver, subminor);
	if (!interface) {
		LOG_ERROR("Can't find USB-Device with minor id %d", subminor);
		return -ENODEV;
	}

	ws2812_struct = usb_get_intfdata(interface);
	if (!ws2812_struct) {
		LOG_ERROR("Can't get Interface for USB-Device with minor id %d",
			  subminor);
		return -ENODEV;
	}

	kref_get(&ws2812_struct->kref); // Refcount erhöhen. (Offener file)
	// USB: mit dem ws2812_struct koppeln.
	instance->private_data = ws2812_struct;
	LOG_INFO("File %s has been opened", instance->f_path.dentry->d_iname);
	return 0;
}

/**
 * @brief Cleans up and frees resources associated with a WS2812 USB controller.
 *
 * This function is called when the reference count for the WS2812 USB device drops to zero. It stops
 * any ongoing modes, frees allocated USB resources, and releases interface and device references.
 * Finally, it frees all allocated memory associated with the device. The function uses `container_of`
 * to retrieve the device structure from the kref structure.
 *
 * @param kref Pointer to kref structure embedded in the ws2812 structure.
 */
static void ws2812_dev_file_delete(struct kref *kref)
{
	LOG_DEBUG("ws2812_dev_file_delete", "");
	struct ws2812 *ws2812_struct;
	// get dev form kref (makro container_of(ptr, type, member))
	ws2812_struct = container_of(kref, struct ws2812, kref);
	ws2812_ctrl_stop_current_mode(ws2812_struct);
	usb_free_urb(ws2812_struct->bulk_in_urb);
	usb_put_intf(ws2812_struct->interface);
	usb_put_dev(ws2812_struct->usb_dev);
	kfree(ws2812_struct->bulk_in_pkg);
	kfree(ws2812_struct->read_request_pkg);
	ws2812_delete_pixel_buffer(&ws2812_struct->pixeldata);
	kfree(ws2812_struct);
}

/**
 * @brief Releases a dev file instance associated with the WS2812 USB device.
 *
 * This function is called when a file associated with the WS2812 USB device is closed. It decreases
 * the reference count of the USB device. If the reference count reaches zero, it triggers the
 * cleanup and deletion of the device structure. The function logs the release operation.
 *
 * @param inode Pointer to the inode structure representing the file on disk.
 * @param file Pointer to the file structure representing the file to be released.
 *
 * @return Returns 0 on successful release of the file. Returns -ENODEV if the device is not found.
 */
static int ws2812_dev_file_release(struct inode *inode, struct file *file)
{
	LOG_DEBUG("ws2812_dev_file_release", "");
	struct ws2812 *ws2812_struct;
	ws2812_struct = file->private_data;
	if (ws2812_struct == NULL) {
		return -ENODEV;
	}
	LOG_INFO("Release File: %s", file->f_path.dentry->d_iname);
	kref_put(&ws2812_struct->kref, ws2812_dev_file_delete);
	return 0;
}

// Struct to register Dev File Operations
struct file_operations ws2812_dev_file_ops = {
	.owner = THIS_MODULE,
	.open = ws2812_dev_file_open,
	.read = ws2812_usb_read,
	.write = ws2812_usb_write,
	.release = ws2812_dev_file_release,
};

// Struct to register USB Class Driver to the usb driver core
struct usb_class_driver ws2812_usb_class_desc = {
	.name = "usb_ws2812_%d", // Name of the devicefiles
	.fops = &ws2812_dev_file_ops,
	.minor_base = 16,
};

/**
 * @brief Probes a WS2812 USB device and initializes it.
 *
 * This function is called when the USB subsystem identifies a device that matches the criteria set
 * by this driver. It allocates and initializes the necessary structures for the device, sets up USB
 * communication endpoints, and registers the device with the USB subsystem. The function performs
 * several memory allocations and initializes various fields in the ws2812 structure.
 *
 * @param interface Pointer to the USB interface associated with the device.
 * @param id Pointer to the usb_device_id structure providing the ID of the device.
 *
 * @return Returns 0 on successful initialization. 
 *         Returns a negative error code on failure.
 */
static int ws2812_usb_probe(struct usb_interface *interface,
			    const struct usb_device_id *id)
{
	LOG_DEBUG("ws2812_usb_probe", "");
	struct ws2812 *ws2812_struct;
	struct usb_endpoint_descriptor *bulk_in, *bulk_out;

	ws2812_struct = kmalloc(sizeof(struct ws2812),
				GFP_KERNEL); // alloc kernel Memory
	if (!ws2812_struct)
		return -ENOMEM;
	kref_init(&ws2812_struct->kref);
	mutex_init(&ws2812_struct->io_mutex);

	ws2812_struct->usb_dev = usb_get_dev(interface_to_usbdev(interface));
	ws2812_struct->interface = usb_get_intf(interface);

	// Init usb-dev data
	ws2812_struct->parse_pixel_len = 0;
	ws2812_struct->parse_pixel_next_index = 0;
	ws2812_struct->parse_pixel_next_index = 0;
	ws2812_struct->parse_data_destination = &ws2812_struct->pixeldata;

	ws2812_init_pixel_buffer(&ws2812_struct->pixeldata, 0);

	ws2812_struct->mode = CHAR_LED_MODE_STATIC;
	ws2812_struct->parse_cb = &mode_callbacks[CHAR_LED_MODE_STATIC];

	// Init dev-read data
	INIT_LIST_HEAD(&ws2812_struct->request_list);

	/*
	 * Find endpoint info
	 */
	int error = usb_find_common_endpoints(interface->cur_altsetting,
					      &bulk_in, &bulk_out, NULL, NULL);
	if (error) {
		dev_err(&interface->dev, "Could not find endpoints\n");
		goto free_ws2812_struct;
	}

	ws2812_struct->bulk_in_size =
		usb_endpoint_maxp(bulk_in); // get size of endpoint
	// Save endpoints
	ws2812_struct->bulk_in_endpointAddr = bulk_in->bEndpointAddress;
	ws2812_struct->bulk_out_endpointAddr = bulk_out->bEndpointAddress;
	// alloc buffer to hold read data
	ws2812_struct->bulk_in_pkg =
		kmalloc(sizeof(ws2812_usb_packet), GFP_KERNEL);
	if (!ws2812_struct->bulk_in_pkg) {
		error = -ENOMEM;
		goto free_ws2812_struct;
	}
	ws2812_struct->read_request_pkg =
		kmalloc(sizeof(ws2812_usb_packet), GFP_KERNEL);
	if (!ws2812_struct->read_request_pkg) {
		error = -ENOMEM;
		goto free_bulk_in_pkg;
	}
	// erster Paraneter: iso_packet ???: für bulk = 0;
	ws2812_struct->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ws2812_struct->bulk_in_urb) {
		error = -ENOMEM;
		goto free_read_request_pkg;
	}
	ws2812_struct->disconnected = false;
	// Speicher ws2812_struct so ab, dass Später die Daten mit dem USB-Device assoziiert werden können.
	usb_set_intfdata(interface, ws2812_struct);

	error = usb_register_dev(interface, &ws2812_usb_class_desc);
	if (error) {
		dev_err(&interface->dev, "Failde to create device.\n");
		usb_set_intfdata(interface, NULL); // data vom Device lösen.
		goto free_bulk_in_urb;
	}

	struct usb_device *dev = ws2812_struct->usb_dev;
	LOG_INFO(
		"Found device!\n  ID: %x,%x\n  Minor: %d\n  Serial: %s\n  Endpoint in: %x (%ld bytes)\n  Endpoint out: %x",
		dev->descriptor.idVendor, dev->descriptor.idProduct,
		interface->minor, dev->serial,
		ws2812_struct->bulk_in_endpointAddr,
		ws2812_struct->bulk_in_size,
		ws2812_struct->bulk_out_endpointAddr);
	return 0;

free_bulk_in_urb:
	usb_free_urb(ws2812_struct->bulk_in_urb);
free_read_request_pkg:
	kfree(ws2812_struct->read_request_pkg);
free_bulk_in_pkg:
	kfree(ws2812_struct->bulk_in_pkg);
free_ws2812_struct:
	kfree(ws2812_struct);
	return error;
}

/**
 * @brief Handles the disconnection of a WS2812 USB device.
 *
 * This function is invoked when a WS2812 USB controller is disconnected from the system. It retrieves
 * the device-specific structure, locks the I/O mutex to ensure no I/O operations are in progress,
 * and deregisters the device. The function sets a flag to indicate that the device has been
 * disconnected.
 *
 * @param interface Pointer to the USB interface associated with the disconnected device.
 *
 * @warning This function should ensure that all I/O operations are properly terminated and all
 * resources are freed. Currently, the function does not stop USB requests in progress (URBs),
 * which could lead to undefined behavior or system crashes.
 */
static void ws2812_usb_disconnect(struct usb_interface *interface)
{
	LOG_DEBUG("ws2812_usb_disconnect", "");
	struct ws2812 *ws2812_struct;
	ws2812_struct = usb_get_intfdata(interface);

	// Warte auf noch nicht abgeschlossene IO Operationen.
	mutex_lock(&ws2812_struct->io_mutex);
	// Struktur des devicefiels löschen
	usb_deregister_dev(interface, &ws2812_usb_class_desc);

	mutex_unlock(&ws2812_struct->io_mutex);

	// TODO: Ensure URBs are stopped and all according resources are freed.
	ws2812_struct->disconnected = 1;
}

/**
 * @brief USB device ID array for WS2812 LED strip module.
 *
 * This array `ws2812_usb_ids` contains the USB device identifiers used by the WS2812 Controller.
 * Each entry in the array specifies a pair of vendor and product IDs that the module supports.
 * The array is terminated with an empty entry, following the standard convention for arrays of this type in the Linux kernel.
 *
 * @note This array is used by the USB core to match the device with this driver.
 *
 * @see USB_DEVICE macro for how vendor and product IDs are specified.
 */
static struct usb_device_id ws2812_usb_ids[] = {
	{ USB_DEVICE(
		USB_VENDOR_ID,
		USB_PRODUCT_ID) }, /**< USB device ID for the WS2812 LED strip. */
	{} /**< Terminating entry. */
};

/**
 * @brief USB driver for the WS2812 Controller.
 *
 * This structure defines the USB driver for the WS2812 LED strip. It includes the driver's name,
 * the probe and disconnect functions, and the ID table that contains the supported device IDs.
 *
 * The `probe` and `disconnect` functions handle the initialization and cleanup processes when the USB device
 * is connected or disconnected, respectively.
 */
static struct usb_driver ws2812_usb_driver = {
	.name = "USB_WS2812", /**< Driver name. */
	.probe = ws2812_usb_probe, /**< Probe function. */
	.disconnect = ws2812_usb_disconnect, /**< Disconnect function. */
	.id_table = ws2812_usb_ids, /**< Supported device ID table. */
};

/**
 * @brief Exit function for the WS2812 USB driver.
 *
 * This function is called when the WS2812 USB driver module is removed from the kernel.
 * It deregisters the USB driver, cleaning up any resources that were allocated when the
 * driver was loaded.
 *
 * @see usb_deregister
 *
 * @note This function is marked with '__exit', indicating it's used during module unload.
 */
static void __exit ws2812_exit(void)
{
	usb_deregister(
		&ws2812_usb_driver); /**< Deregister the WS2812 USB driver. */
}

/**
 * @brief Initialization function for the WS2812 USB driver.
 *
 * This function is called when the WS2812 USB driver module is loaded into the kernel.
 * It registers the USB driver with the USB core subsystem. If the registration fails,
 * the function returns an error code.
 *
 * @return Returns 0 on successful registration, and -EIO (Input/Output error) if the
 *         registration fails.
 *
 * @see usb_register
 *
 * @note This function is marked with '__init', indicating it's used during module initialization.
 */
static int __init ws2812_init(void)
{
	if (usb_register(&ws2812_usb_driver)) {
		return -EIO; /**< Return -EIO if registration fails. */
	}
	return 0; /**< Return 0 on successful registration. */
}

// Tells the USB-Core subsystem which Devices to accept
MODULE_DEVICE_TABLE(usb, ws2812_usb_ids);

module_init(ws2812_init);
module_exit(ws2812_exit);
MODULE_LICENSE("GPL");