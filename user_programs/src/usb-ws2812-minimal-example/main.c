/**
 * @file main.c                                                                *
 * @brief TODO: Give a brief description about the file and its functionality  *
 * @date  Sunday 28th-January-2024                                             *
 * Document class: public                                                      *
 * (c) 2024 Erik Appel, Kristian Minderer, https://git.fh-muenster.de          *
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include "usb_ws2812_lib.h"

int main(int argc, char** argv)
{
    ws2812_init(); // Init
    int fd = open("/dev/usb_ws2812_0", O_RDWR);
    if(fd < 0){
        return 1;
    }

    if(ws2812_set_length(fd, 16) < 0){
        printf("Länge konnte nicht verändert werden!\n");
        return 1;
    }
    led_pixel pixel_data[16];
    for(int i = 0; i < 16; i++){
        pixel_data[i].red = 65;
        pixel_data[i].green = 0;
        pixel_data[i].blue = 0;
    }
    // Alle Leds rot färben
    ws2812_set_led_pixel(fd, 0, 16, pixel_data);

    sleep(10);

    for(int i = 0; i < 4; i++){
        pixel_data[i].red = 0;
        pixel_data[i].green = 65;
        pixel_data[i].blue = 0;
    }
    // LEDs 5 bis 8 grün Färben
    ws2812_set_led_pixel(fd, 4, 4, pixel_data);
    ws2812_deinit(); // deinit
    return 0;
}
