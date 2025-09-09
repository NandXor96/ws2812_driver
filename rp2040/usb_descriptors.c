/**
 * @file usb_descriptors.c                                                     *
 * @brief USB Descriptors for TinyUSB                                          *
 * @date  Saturday 27th-January-2024                                           *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#include <string.h>
#include <pico/unique_id.h>
#include "tusb.h"

enum string_index {
	RESERVED_IDX = 0,
	MANUFACTURER_IDX,
	PRODUCT_IDX,
	SERIALNUMBER_IDX,
	SOURCE_SINK_IDX,
};

/**
 * @brief Device descriptor structure for a USB device.
 *
 * This structure holds the standard description for a USB device. It defines
 * essential information such as USB version, device class, vendor and
 * product identifiers, and the number of configurations. Each field in the
 * structure represents a specific characteristic of the USB device as per
 * USB specifications.
 */
tusb_desc_device_t const device_descriptor = {
	.bLength = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB = 0x0110, // USB 1.1

	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,

	.idVendor = 0xCAFE, // VID
	.idProduct = 0x1234, // PID
	.bcdDevice = 0x0001, // Version

	.iManufacturer = MANUFACTURER_IDX,
	.iProduct = PRODUCT_IDX,
	.iSerialNumber = SERIALNUMBER_IDX,

	.bNumConfigurations = 1,
};

/**
 * @brief Callback function to retrieve the device descriptor.
 *
 * This function is a callback used by the TinyUSB stack to obtain the
 * device descriptor of the USB device.
 *
 * @return A constant pointer to a uint8_t array representing the USB device descriptor.
 */
uint8_t const *tud_descriptor_device_cb(void)
{
	return (uint8_t const *)&device_descriptor;
}

/**
 * @brief Structure representing the USB descriptor config.
 *
 * This structure defines the descriptors for a USB device.
 * It is packed to ensure the data structure aligns with the USB standard's 
 * strict byte alignment requirements. The structure includes the configuration,
 * interface, and endpoint descriptors.
 * 
 * @warning Das blöde Struct wird von Doxygen nicht erkannt, weil es ein Attribut hat.
 * Ich habe keine Lösung gefunden und mir ist es jetzt auch egal. Doxygen ist blöd.
 */
typedef struct __attribute__ ((packed)) {
	tusb_desc_configuration_t config;     ///< USB configuration descriptor
	tusb_desc_interface_t interface;      ///< USB interface descriptor
	tusb_desc_endpoint_t bulk_in;         ///< USB bulk IN endpoint descriptor
	tusb_desc_endpoint_t bulk_out;        ///< USB bulk OUT endpoint descriptor
} usb_descriptor_config_t;


#define USB_ENDPOINT_DESCRIPTOR(_attr, _addr, _size, _interval)   \
	{                                                         \
		.bLength = sizeof(tusb_desc_endpoint_t),          \
		.bDescriptorType = TUSB_DESC_ENDPOINT,            \
		.bEndpointAddress = _addr, .bmAttributes = _attr, \
		.wMaxPacketSize = _size, .bInterval = _interval,  \
	}

/**
 * @brief USB descriptor configuration for the device.
 *
 * This variable holds the entire USB descriptor configuration, including the
 * configuration, interface, and endpoint descriptors. It is used to define
 * the properties and capabilities of the USB device.
 */
usb_descriptor_config_t descriptor_config = {
    .config = {
        .bLength = sizeof(tusb_desc_configuration_t), /**< Size of the configuration descriptor */
        .bDescriptorType = TUSB_DESC_CONFIGURATION, /**< Descriptor type */
        .wTotalLength = sizeof(usb_descriptor_config_t), /**< Total length of the configuration */
        .bNumInterfaces = 1, /**< Number of interfaces */
        .bConfigurationValue = 1, /**< Configuration value */
        .iConfiguration = 0, /**< Configuration string index */
        .bmAttributes = TU_BIT(7), /**< Attribute bitmask */
        .bMaxPower = 450 / 2, /**< Maximum power consumption (450mA) */
    },

    .interface = {
        .bLength = sizeof(tusb_desc_interface_t), /**< Size of the interface descriptor */
        .bDescriptorType = TUSB_DESC_INTERFACE, /**< Descriptor type */
        .bInterfaceNumber = 0, /**< Interface number */
        .bAlternateSetting = 0, /**< Alternate setting */
        .bNumEndpoints = 2, /**< Number of endpoints (Two interfaces) */
        .bInterfaceClass = TUSB_CLASS_VENDOR_SPECIFIC, /**< Interface class */
        .bInterfaceSubClass = 0x00, /**< Interface subclass */
        .bInterfaceProtocol = 0x00, /**< Interface protocol */
        .iInterface = 0, /**< Interface string index */
    },
    // Bulk input endpoint with address 0x81, receives 120 bits (5 LEDs at 24 bits each) of data
    .bulk_in = USB_ENDPOINT_DESCRIPTOR(TUSB_XFER_BULK, 0x81, CFG_USB_BULK_ENDPOINT_SIZE, 0), /**< Bulk IN endpoint descriptor */

    // Bulk output endpoint with address 0x02, transmits 120 bits (5 LEDs at 24 bits each) of data
    .bulk_out = USB_ENDPOINT_DESCRIPTOR(TUSB_XFER_BULK, 0x02, CFG_USB_BULK_ENDPOINT_SIZE, 0) /**< Bulk OUT endpoint descriptor */
};

/**
 * @brief Callback function to retrieve the configuration descriptor.
 *
 * This function is a callback used by the TinyUSB stack to obtain the
 * configuration descriptor for a specific configuration index.
 *
 * @param index The index of the configuration descriptor to retrieve.
 *              This parameter is not used in this implementation and is
 *              included to comply with the TinyUSB API.
 *
 * @return A constant pointer to a uint8_t array representing the USB configuration descriptor.
 */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
	(void)index;

	return (uint8_t const *)&descriptor_config;
}

/**
 * @brief Structure representing a USB string descriptor.
 *
 * This structure defines the USB string descriptor, used for providing
 * human-readable string representations, such as manufacturer, product,
 * or serial number, in Unicode format. The structure is packed to ensure
 * byte alignment in accordance with USB specifications.
 * 
 * @warning Das blöde Struct wird von Doxygen nicht erkannt, weil es ein Attribut hat.
 * Ich habe keine Lösung gefunden und mir ist es jetzt auch egal. Doxygen ist blöd.
 */
typedef struct __attribute__ ((packed)) {
	uint8_t bLength; /**< Size of the descriptor in bytes */
	uint8_t bDescriptorType; /**< Descriptor type (String) */
	uint16_t unicode_string[31]; /**< Unicode string buffer */
} gud_desc_string_t;

/**
 * @brief Static instance of a USB string descriptor.
 */
static gud_desc_string_t string_descriptor = {
	.bDescriptorType = TUSB_DESC_STRING,
};

// https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_hid_composite/usb_descriptors.c
char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1]; // Aus pico example

/**
 * @brief Array of USB string descriptors.
 *
 * This array holds the string descriptors used by the USB device. Each entry
 * corresponds to a specific type of string information, like manufacturer,
 * product name, or serial number. The strings are used by the USB host to
 * display or log descriptive information about the USB device.
 */
static const char *strings[] = {
    [0] = (const char *)0x0409, /**< Language: English */
    [MANUFACTURER_IDX] = "FH MS", /**< Manufacturer string */
    [PRODUCT_IDX] = "WS2812B Controller", /**< Product string */
    [SERIALNUMBER_IDX] = serial, /**< Placeholder for unique serial number */
};

/**
 * @brief Callback function to retrieve string descriptors.
 *
 * This function is called by the TinyUSB stack to obtain the string
 * descriptors based on the index and language ID provided.
 *
 * @param index The index of the string descriptor to retrieve.
 * @param langid The language ID for the string descriptors, not used in this implementation.
 *
 * @return A pointer to the string descriptor if index is valid, NULL otherwise.
 */
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void)langid;

	if (index >= TU_ARRAY_SIZE(strings))
		return NULL;

	const char *str = strings[index];

	if (SERIALNUMBER_IDX == index) {
		pico_get_unique_board_id_string(serial,
						sizeof(serial)); // Copy serial
	}

	uint8_t len = strlen(str);
	if (len > sizeof(string_descriptor.unicode_string))
		len = sizeof(string_descriptor.unicode_string);

	string_descriptor.bLength = 2 + 2 * len;

	for (uint8_t i = 0; i < len; i++)
		string_descriptor.unicode_string[i] = str[i];

	return (uint16_t *)&string_descriptor;
}
