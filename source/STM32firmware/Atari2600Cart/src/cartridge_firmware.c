#include <string.h>

#include "cartridge_firmware.h"

#include "firmware_pal_rom.h"
#include "firmware_pal60_rom.h"
#include "firmware_ntsc_rom.h"

static unsigned char menu_ram[1024];	// < NUM_DIR_ITEMS * 12
static char menu_status[16];
static unsigned const char *firmware_rom = firmware_ntsc_rom;

void set_menu_status_msg(const char* message) {
    strncpy(menu_status, message, 15);
}

void set_menu_status_byte(char status_byte) {
    menu_status[15] = status_byte;
}

void set_tv_mode(int tv_mode) {
    switch (tv_mode) {
		case TV_MODE_NTSC:
			firmware_rom = firmware_ntsc_rom;
			break;

		case TV_MODE_PAL:
			firmware_rom = firmware_pal_rom;
			break;

		case TV_MODE_PAL60:
			firmware_rom = firmware_pal60_rom;
			break;
	}
}

uint8_t* get_menu_ram() {
    return menu_ram;
}

int emulate_firmware_cartridge() {
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if ((addr & 0x1F00) == 0x1E00) break;	// atari 2600 has sent a command
			if (addr >= 0x1800 && addr < 0x1C00)
				DATA_OUT = ((uint16_t)menu_ram[addr&0x3FF])<<8;
			else if ((addr & 0x1FF0) == CART_STATUS_BYTES)
				DATA_OUT = ((uint16_t)menu_status[addr&0xF])<<8;
			else
				DATA_OUT = ((uint16_t)firmware_rom[addr&0xFFF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}

	__enable_irq();
	return addr;
}

bool reboot_into_cartridge() {
	set_menu_status_byte(1);

	return emulate_firmware_cartridge() == CART_CMD_START_CART;
}
