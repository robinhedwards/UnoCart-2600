#include <stdbool.h>

#include "tm_stm32f4_fatfs.h"

#include "cartridge_3e.h"
#include "flash.h"
#include "cartridge_firmware.h"

#define CCM_RAM ((uint8_t*)0x10000000)
#define CCM_SIZE (64 * 1024)

#define CCM_BANKS 32

#define BUFFER_SIZE (96 * 1024)

#define MAX_CCM_BANK (CCM_BANKS - 1)

typedef struct {
	uint8_t max_bank;
	uint8_t max_ccm_bank;

	uint8_t *flash_base;
} cartridge_layout;

static bool setup_cartridge_image(const char* filename, uint32_t image_size, uint8_t *buffer, cartridge_layout* layout) {
	if (image_size == 0) return false;

	image_size = image_size > 512 * 1024 ? 512 * 1024 : image_size;

	uint32_t banks = image_size / 2048;
	if (image_size % 2048) banks++;

	layout->max_bank = (uint8_t)(banks - 1);
	layout->max_ccm_bank = layout->max_bank > MAX_CCM_BANK ? MAX_CCM_BANK : layout->max_bank;

    if (layout->max_bank == layout->max_ccm_bank) {
        memcpy(CCM_RAM, buffer, image_size);
        return true;
    }

    uint32_t flash_image_size = image_size - CCM_SIZE;
    if (flash_image_size > available_flash()) return false;

    flash_context ctx;

    if (!prepare_flash(flash_image_size, &ctx)) return false;

    layout->flash_base = ctx.base;

    uint32_t flash_from_buffer =
        image_size > BUFFER_SIZE ? BUFFER_SIZE - CCM_SIZE : image_size - CCM_SIZE;

    if (!write_flash(flash_from_buffer, buffer + CCM_SIZE, &ctx)) return false;

    if (image_size <= BUFFER_SIZE) {
        memcpy(CCM_RAM, buffer, CCM_SIZE);
        return true;
    }

    uint32_t flash_from_file = image_size - BUFFER_SIZE;
	FATFS fs;
	FIL fil;
	UINT bytes_read;

	if (f_mount(&fs, "", 1) != FR_OK) goto fail_unmount;
	if (f_open(&fil, filename, FA_READ) != FR_OK) goto fail_close;

    if (f_lseek(&fil, BUFFER_SIZE) != FR_OK) goto fail_close;

    uint32_t bytes_written_to_flash = 0;

    while (bytes_written_to_flash < flash_from_file) {
        if (f_read(&fil, CCM_RAM, CCM_SIZE, &bytes_read) != FR_OK) goto fail_close;

        bytes_written_to_flash += bytes_read;
        if (bytes_read < CCM_SIZE && bytes_written_to_flash < flash_from_file) goto fail_close;

        if (!write_flash(bytes_read, CCM_RAM, &ctx)) goto fail_close;
    }

    f_close(&fil);
	f_mount(0, "", 1);

    memcpy(CCM_RAM, buffer, CCM_SIZE);
    return true;

	fail_close:
		f_close(&fil);
	fail_unmount:
		f_mount(0, "", 1);

	return false;
}

void emulate_3e_cartridge(const char* filename, uint32_t image_size, uint8_t* buffer, uint8_t ram_banks) {
	cartridge_layout layout;

	if (!setup_cartridge_image(filename, image_size, buffer, &layout)) return;

	uint16_t addr, addr_prev = 0, addr_prev2 = 0, data = 0, data_prev = 0;
	uint8_t *bankPtr = CCM_RAM;
	uint8_t *fixedPtr;
    bool mode_ram = false;

	if (layout.max_bank <= layout.max_ccm_bank) fixedPtr = CCM_RAM + layout.max_bank * 2048;
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
		if (addr == 0x3f) {
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;

			uint8_t bank = data % (layout.max_bank + 1);
            mode_ram = false;

			if (bank <= layout.max_ccm_bank) bankPtr = CCM_RAM + bank * 2048;
			else bankPtr = layout.flash_base + (bank - layout.max_ccm_bank - 1) * 2048;
		}
        else if (addr == 0x3e) {
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;

            uint8_t bank = data % ram_banks;
            mode_ram = true;

            bankPtr = buffer + bank * 1024;
        }
		else if (addr & 0x1000) {
			if (addr & 0x800) {
				data = fixedPtr[addr & 0x7ff];

                DATA_OUT = ((uint16_t)data)<<8;
	    		SET_DATA_MODE_OUT

                while (ADDR_IN == addr) ;
                SET_DATA_MODE_IN
			} else if (mode_ram && addr & 0x400) {
                while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
                data = data_prev>>8;

                bankPtr[addr & 0x3ff] = data;
            } else {
                data = bankPtr[addr & 0x7ff];

                DATA_OUT = ((uint16_t)data)<<8;
                SET_DATA_MODE_OUT

                while (ADDR_IN == addr) ;
                SET_DATA_MODE_IN
            }
		}
	}
	__enable_irq();
}
