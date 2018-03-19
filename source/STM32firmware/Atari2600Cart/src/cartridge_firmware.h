#ifndef CARTRIDGE_FIRMWARE_H
#define CARTRIDGE_FIRMWARE_H

#include <stdint.h>
#include <stdbool.h>

#include "cartridge_io.h"

#define CART_CMD_SEL_ITEM_n	0x1E00
#define CART_CMD_ROOT_DIR	0x1EF0
#define CART_CMD_START_CART	0x1EFF

#define CART_STATUS_BYTES	0x1FE0	// 16 bytes of status

#define TV_MODE_NTSC	1
#define TV_MODE_PAL     2
#define TV_MODE_PAL60   3

void set_menu_status_msg(const char* message);

void set_menu_status_byte(char status_byte);

void set_tv_mode(int tv_mode);

uint8_t* get_menu_ram();

int emulate_firmware_cartridge();

bool reboot_into_cartridge();

#endif // CARTRIDGE_FIRMWARE_H
