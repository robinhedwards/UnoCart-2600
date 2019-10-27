#include <stdbool.h>

#include "tm_stm32f4_fatfs.h"

#include "cartridge_3f.h"
#include "flash.h"
#include "cartridge_firmware.h"

#define CCM_RAM ((uint8_t*)0x10000000)
#define CCM_SIZE (64 * 1024)

#define RAM_BANKS 48
#define CCM_BANKS 32

#define MAX_RAM_BANK (RAM_BANKS - 1)
#define MAX_CCM_BANK (MAX_RAM_BANK + CCM_BANKS)

typedef struct {
	uint8_t max_bank;
	uint8_t max_ram_bank;
	uint8_t max_ccm_bank;

	uint8_t *flash_base;
} cartridge_layout;

static bool setup_cartridge_image(const char* filename, uint32_t image_size, uint8_t *buffer, cartridge_layout* layout) {
	if (image_size == 0) return false;

	image_size = image_size > 512 * 1024 ? 512 * 1024 : image_size;

	uint32_t banks = image_size / 2048;
	if (image_size % 2048) banks++;

	layout->max_bank = (uint8_t)(banks - 1);
	layout->max_ram_bank = layout->max_bank > MAX_RAM_BANK ? MAX_RAM_BANK : layout->max_bank;

	if (layout->max_bank == layout->max_ram_bank) return true;

	layout->max_ccm_bank = layout->max_bank > MAX_CCM_BANK ? MAX_CCM_BANK : layout->max_bank;

	FATFS fs;
	FIL fil;
	UINT bytes_read;

	if (f_mount(&fs, "", 1) != FR_OK) goto fail_unmount;
	if (f_open(&fil, filename, FA_READ) != FR_OK) goto fail_close;

	if (layout->max_ccm_bank != layout->max_bank) {
		uint32_t flash_image_size = image_size - (layout->max_ccm_bank + 1) * 2048;

		if (flash_image_size > available_flash()) goto fail_close;

		flash_context ctx;

		if (!prepare_flash(flash_image_size, &ctx)) goto fail_close;

		layout->flash_base = ctx.base;

		if (f_lseek(&fil, (RAM_BANKS + CCM_BANKS) * 2048) != FR_OK) goto fail_close;
		uint32_t bytes_written_to_flash = 0;

		while (bytes_written_to_flash < flash_image_size) {
			if (f_read(&fil, CCM_RAM, CCM_SIZE, &bytes_read) != FR_OK) goto fail_close;

			bytes_written_to_flash += bytes_read;
			if (bytes_read < CCM_SIZE && bytes_written_to_flash < flash_image_size) goto fail_close;

			if (!write_flash(bytes_read, CCM_RAM, &ctx)) goto fail_close;
		}
	}

	if (f_lseek(&fil, RAM_BANKS * 2048) != FR_OK) goto fail_close;
	if (f_read(&fil, CCM_RAM, CCM_SIZE, &bytes_read) != FR_OK) goto fail_close;

	f_close(&fil);
	f_mount(0, "", 1);
	return true;

	fail_close:
		f_close(&fil);
	fail_unmount:
		f_mount(0, "", 1);

	return false;
}

void emulate_3f_cartridge(const char* filename, uint32_t image_size, uint8_t* buffer) {
	cartridge_layout layout;

	if (!setup_cartridge_image(filename, image_size, buffer, &layout)) return;

	uint16_t addr, addr_prev = 0, addr_prev2 = 0, data = 0, data_prev = 0;
	uint8_t *bankPtr = buffer;
	uint8_t *fixedPtr;

	if (layout.max_bank <= layout.max_ram_bank) fixedPtr = buffer + layout.max_bank * 2048;
	else if (layout.max_bank <= layout.max_ccm_bank) fixedPtr = CCM_RAM + (layout.max_bank - layout.max_ram_bank - 1) * 2048;
	else fixedPtr = layout.flash_base + (layout.max_bank - layout.max_ccm_bank - 1) * 2048;

	if (!reboot_into_cartridge()) return;
	__disable_irq();

	while (1)
	{
		while (((addr = ADDR_IN) != addr_prev) || (addr != addr_prev2))
		{
			addr_prev2 = addr_prev;
			addr_prev = addr;
		}

		// got a stable address
		if (addr == 0x3f)
		{	// A12 low, read last data on the bus before the address lines change
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;

			uint8_t bank = data > layout.max_bank ? data % (layout.max_bank + 1) : data;

			if (bank <= layout.max_ram_bank) bankPtr = buffer + bank * 2048;
			else if (bank <= layout.max_ccm_bank) bankPtr = CCM_RAM + (bank - layout.max_ram_bank - 1) * 2048;
			else bankPtr = layout.flash_base + (bank - layout.max_ccm_bank - 1) * 2048;
		}
		else if (addr & 0x1000)
		{ // A12 high
			if (addr & 0x800)
				data = fixedPtr[addr&0x7FF];
			else
				data = bankPtr[addr&0x7FF];
			DATA_OUT = ((uint16_t)data)<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}
