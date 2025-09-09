/**
 * @file ws2812-test-client.c                                                  *
 * @brief TODO: Give a brief description about the file and its functionality  *
 * @date  Sunday 28th-January-2024                                             *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */


#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include "usb_ws2812_lib.h"

const char *argp_program_version = "usb-ws2812-client";
const char *argp_program_bug_address = "";
static char doc[] = "Der usb-ws2812-client ist eine Beispielimplementation, die zeigt wie die WS2812-LEDs und das zugehörige Kernelmodul mit der usb-ws2812-lib Bibliothek angesteuert werden kann.";
static char args_doc[] = "";
/**
 * @struct argp_option
 * @brief Structure for command-line options
 */
static struct argp_option options[] = { 
    { "devicefile", 'f', "FILE", 0, "Devicefile des USB-Geräts"},
    { "mode", 'm', 0, 0, "Fragt ab in welchem Modus sich das USB-Gerät befindet"},
    { "blink", 'b', 0, 0, "Aktiviert den Blinkmodus des USB-Geräts"},
	{ "blinkdelay", 'd', "NUM", 0, "Gibt die Zeit in ms an die Zwischen den Musterwechsel verstreichen soll."},
	{ "blinkpattern", 'p', "PATTERN FILE", 0, "Eine Datei mit den Musterdaten im Format \"MUSTER_ANZAHL MUSTER_LÄNGE R0 G0 B0 ... RN GN BN\""},
	{ "static", 's', 0, 0, "Aktiviert den Statischen des USB-Geräts"},
	{ "length", 'l', "NUM", 0, "Ändert die Länge."},
	{ "get_data", 322, 0, 0, "Zeigt die Leddaten des USB-Geräts an."},
	{ "get_mode_data", 323, 0, 0, "Zeigt die Daten des aktuellen Modus an."},
	{ "pixeldatafile", 324, "LED DATEN FILE", 0, "Eine Datei mit Leddaten im Format: \"LÄNGE OFFSET R0 G0 B0 ... RN GN BN\""},
	{ "clear", 'c', 0, 0, "Clear den Ledstreifen"},
	{ "get_length", 325, 0, 0, "gibt die aktuelle Länge des USB-Geräts zurück."},
    { 0 },
};

/**
 * @brief Enumeration for different modes
 */
typedef enum MODE_CHANGE_E {
	NONE, STATIC, BLINK
}MODE_CHANGE;

/**
 * @struct arguments
 * @brief Structure to hold command-line arguments
 */
struct arguments {
	char* device_file;
	char* pattern;
	char* led_daten;
	uint16_t pattern_delay;
	uint16_t length;
	MODE_CHANGE new_mode;
	bool get_mode;
	bool get_mode_data;
	bool get_data;
	bool get_length;
	bool set_mode;
	bool set_legnth;
	bool clear;
};


/**
 * @brief Callback function to parse command-line options
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arg_s = state->input;
	switch (key)
	{
	case 'f':
		arg_s->device_file = arg;
		break;
	case 'm':
		arg_s->get_mode = true;
		break;
	case 'b':
		arg_s->new_mode = BLINK;
		break;
	case 'd':
		if(sscanf(arg, "%" SCNd16, &arg_s->pattern_delay) == 0){
			printf("Error: %s is not a number\n", arg);
			return ARGP_KEY_ERROR;
		};
		break;
	case 'p':
		arg_s->pattern = arg;
		break;
	case 's':
		arg_s->new_mode = STATIC;
		break;
	case 'l':
		arg_s->set_legnth = true;
		if(sscanf(arg, "%" SCNd16, &arg_s->length) == 0){
			printf("Error: %s is not a number\n", arg);
			return ARGP_KEY_ERROR;
		};
		break;
	case 322:
		arg_s->get_data = true;
		break;
	case 323:
		arg_s->get_mode_data = true;
		break;
	case 324:
		arg_s->led_daten = arg;
		break;
	case 325:
		arg_s->get_length = true;
		break;
	case 'c':
		arg_s->clear = true;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
    return 0;
}
static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

/**
 * @brief Function to start the blinking mode
 */
void start_blink(int fd, uint16_t delay, char* pattern_file){
	led_pixel pattern_data[9];
	pattern_data[0] = (led_pixel){ 0x41, 0, 0 };
	pattern_data[1] = (led_pixel){ 0, 0x41, 0 };
	pattern_data[2] = (led_pixel){ 0, 0, 0x41 };

	pattern_data[3] = (led_pixel){ 0, 0x41, 0 };
	pattern_data[4] = (led_pixel){ 0, 0, 0x41 };
	pattern_data[5] = (led_pixel){ 0x41, 0, 0 };

	pattern_data[6] = (led_pixel){ 0, 0, 0x41 };
	pattern_data[7] = (led_pixel){ 0x41, 0, 0 };
	pattern_data[8] = (led_pixel){ 0, 0x41, 0 };

	ws2812_pattern pattern = {
		.length = 3,
		.pattern_states = 3,
		.pattern_data = pattern_data,
	};

	if(pattern_file != NULL){
		FILE* pattern_f = fopen(pattern_file, "r");
		if(pattern_f == NULL){
			perror("File not found");
			return;
		}
		// Read length
		if(fscanf(pattern_f, "%"SCNd16, &pattern.length) != 1){
			perror("Failed to read patternlength");
		}
		// Read pattern_count
		if(fscanf(pattern_f, "%"SCNd16, &pattern.pattern_states) != 1){
			perror("Failed to read patterncount");
		}
		size_t pattern_length = pattern.length * pattern.pattern_states;
		led_pixel* pixel_data = malloc(sizeof(led_pixel) * pattern_length);
		int read_count = 0;
		int i = 0;
		// parse file
		do{
			read_count = 0;
			read_count += fscanf(pattern_f, "%"SCNd8, &pixel_data[i].red);
			read_count += fscanf(pattern_f, "%"SCNd8, &pixel_data[i].green);
			read_count += fscanf(pattern_f, "%"SCNd8, &pixel_data[i].blue);
			i++;
		}while(read_count == 3 && i <= pattern_length);

		for(i = 0; i < pattern_length; i++){
			printf("Pixel[%03d]{r = %x, g = %x, b = %x}\n", i , pixel_data[i].red,pixel_data[i].green,pixel_data[i].blue);
		}
		pattern.pattern_data = pixel_data;


	}
	// Modus ändern
	if(ws2812_set_mode_blink(fd, pattern.pattern_states, pattern.length, delay) < 0){
		perror("Modechange failed!\n");
		return;
	};

	// Pattern senden
	if(ws2812_set_blink_pattern(fd, &pattern) < 0){
		perror("Failed to send new pattern!\n");
	};
}


/**
 * @brief Function to send the get mode command
 */
void send_get_mode(int fd){
	led_set_mode mode;
	// modus abfragen
	int mode_id = ws2812_get_mode(fd, &mode);
	if(mode_id < 0){
		printf("get_mode encountered a problem: %s\n", strerror(errno));
	}

	switch (mode_id)
	{
	case CHAR_LED_MODE_STATIC:
		printf("Mode: static\n");
		break;
	case CHAR_LED_MODE_BLINK:
		printf("Mode: blink{patter_count = %d, pattern_len = %d, blink_period = %d}\n",
			mode.set_blink.pattern_count,
			mode.set_blink.pattern_len,
			mode.set_blink.blink_period
		);
		break;
	default:
		break;
	}
}

/**
 * @brief Function to send the get pixel data command
 */
void send_get_pixel_data(int fd){
	uint16_t pixel_count = ws2812_get_length(fd);
	ws2812_pixel_buffer pixel_buf = {
		.length = pixel_count,
		.pixel_data = malloc(pixel_count * sizeof(led_pixel))
	};

	if(pixel_buf.pixel_data == NULL){
		printf("Faild to alloc pixelbuffer!\n");
		return;
	}

	int error = ws2812_get_data(fd, &pixel_buf);
	if(error < 0){
		printf("ws2812_get_data encountered a problem: %s\n", strerror(errno));
		return;
	}
	printf("Got %d led pixel:\n", pixel_buf.length);
	led_pixel *pixe_data = pixel_buf.pixel_data;
	for(int i = 0; i < pixel_buf.length; i++){
		printf("Pixel[%03d]{r = %x, g = %x, b = %x}\n", i , pixe_data[i].red,pixe_data[i].green,pixe_data[i].blue);
	}
	free(pixel_buf.pixel_data);
}

/**
 * @brief Function to send the get mode pixel data command
 */
void send_get_mode_pixel_data(int fd){
	uint16_t pixel_count = ws2812_get_mode_data_length(fd);
	ws2812_pixel_buffer pixel_buf = {
		.length = pixel_count,
		.pixel_data = malloc(pixel_count * sizeof(led_pixel))
	};

	if(pixel_buf.pixel_data == NULL){
		printf("Faild to alloc pixelbuffer!\n");
		return;
	}

	int error = ws2812_get_mode_data(fd, &pixel_buf);
	if(error < 0){
		printf("ws2812_get_mode_data encountered a problem: %s\n", strerror(errno));
		return;
	}
	printf("Got %d led pixel:\n", pixel_buf.length);
	led_pixel *pixe_data = pixel_buf.pixel_data;
	for(int i = 0; i < pixel_buf.length; i++){
		printf("Pixel[%03d]{r = %x, g = %x, b = %x}\n", i , pixe_data[i].red,pixe_data[i].green,pixe_data[i].blue);
	}
	free(pixel_buf.pixel_data);
}


/**
 * @brief Function to update pixel data
 */
void update_pixel(int fd, char* pixel_daten_file){
	FILE* pixel_file = fopen(pixel_daten_file, "r");
	if(pixel_file == NULL){
		perror("File not found");
		return;
	}
	int pixel_count = 0;
	// Read length
	if(fscanf(pixel_file, "%d", &pixel_count) != 1){
		perror("Failed to read length");
		return;
	}
		
	// Read offset
	uint16_t offset = 0;
	if(fscanf(pixel_file, "%"SCNd16, &offset) != 1){
		perror("Failed to read offset");
		return;
	}
	
	led_pixel* pixel_data = malloc(sizeof(led_pixel) * pixel_count);
	int read_count = 0;
	int i = 0;
	// parse file
	do{
		read_count = 0;
		read_count += fscanf(pixel_file, "%"SCNd8, &pixel_data[i].red);
		read_count += fscanf(pixel_file, "%"SCNd8, &pixel_data[i].green);
		read_count += fscanf(pixel_file, "%"SCNd8, &pixel_data[i].blue);
		i++;
	}while(read_count == 3 && i <= pixel_count);

	/*for(i = 0; i < pixel_count; i++){
		printf("Pixel[%03d]{r = %x, g = %x, b = %x}\n", i , pixel_data[i].red,pixel_data[i].green,pixel_data[i].blue);
	}*/
	if(ws2812_set_led_pixel(fd, offset, pixel_count, pixel_data) < 0){
		printf("Pixeldaten wurden nicht gesendet: %s\n", strerror(errno));
	}
	free(pixel_data);
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[])
{
    struct arguments arguments =  {
		.device_file = NULL, //
		.pattern = NULL, //
		.led_daten = NULL, //
		.pattern_delay = 1000, //
		.length = 0, //
		.new_mode = NONE, //
		.get_data = false, //
		.get_mode = false, //
		.get_mode_data = false, //
		.get_length = false, //
		.set_legnth = false, //
		.clear = false, //
	};

	if(argc <= 1){
		return 0;
	}
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    if(arguments.device_file == NULL){
		printf("Kein Devicefile angegben!\n");
		return 1;
	}
	int fd = open(arguments.device_file, O_RDWR);
	printf("fd: %d\n",fd);
	if(fd < 0){
		perror("Der Devicefile konnte nicht geöfnet werden!\n");
		return 0;
	}
	ws2812_init();

	if(arguments.set_legnth == true) {
		printf("Ändere die länge auf %d\n", arguments.length);
		ws2812_set_length(fd,  arguments.length);
	}

	if(arguments.new_mode != NONE){
		printf("Ändere den Modus\n");
		if(arguments.new_mode == STATIC){
			ws2812_set_mode_static(fd);
		}else if(arguments.new_mode == BLINK){
			start_blink(fd, arguments.pattern_delay, arguments.pattern);
		}
	}

	if(arguments.get_length == true){
		int len = 0;
		len = ws2812_get_length(fd);
		if(len < 0)  {
			perror("Failed to update length!");
		}else{
			printf("Länge des LED-Streifens: %d\n", len);
		}
		
	}

	if(arguments.get_mode == true){
		send_get_mode(fd);
	}

	if(arguments.get_data == true){
		printf("Daten des USB-Geräts:\n");
		send_get_pixel_data(fd);
	}

	if(arguments.get_mode_data == true){
		printf("Daten des Modus:\n");
		send_get_mode_pixel_data(fd);
	}

	if(arguments.led_daten != NULL){
		printf("Update Pixeldaten\n");
		update_pixel(fd, arguments.led_daten);
	}

	if(arguments.clear == true){
		printf("Clear\n");
		if(ws2812_clear(fd) < 0){
			perror("Failed to send clear command!");
		};
	}

	ws2812_deinit();
	close(fd);
}