#include <stdbool.h>
#include <stdint.h>

#include "cartridge_io.h"
#include "cartridge_supercharger.h"
#include "cartridge_firmware.h"
#include "supercharger_bios.h"

#include "tm_stm32f4_fatfs.h"

typedef struct __attribute__((packed)) {
    uint8_t entry_lo;
    uint8_t entri_hi;
    uint8_t control_word;
    uint8_t block_count;
    uint8_t checksum;
    uint8_t multiload_id;
    uint8_t progress_bar_speed_lo;
    uint8_t progress_bar_speed_hi;

    uint8_t padding[8];

    uint8_t block_location[48];
    uint8_t block_checksum[48];
} LoadHeader;

static void read_multiload(uint8_t *buffer, const char* cartridge_path, uint8_t physical_index) {
    __enable_irq();

    FATFS fs;
    FIL fil;

    if (f_mount(&fs, "", 1) != FR_OK) goto unmount;
	if (f_open(&fil, cartridge_path, FA_READ) != FR_OK) goto close;

    f_lseek(&fil, physical_index * 8448);

    UINT bytes_read;
    f_read(&fil, buffer, 8446, &bytes_read);

    close:
        f_close(&fil);

    unmount:
		f_mount(0, "", 1);

    __disable_irq();
}

static void setup_multiload_map(uint8_t *multiload_map, uint8_t multiload_count, const char* cartridge_path) {
    for (uint8_t i = 0; i < multiload_count; i++) {
        uint8_t buffer[8448];
        LoadHeader *header = (LoadHeader*)&buffer[8448 - 256];

        read_multiload(buffer, cartridge_path, i);

        multiload_map[header->multiload_id] = i;
    }
}

static void setup_rom(uint8_t* rom) {
    memset(rom, 0, 0x0800);

    for (uint32_t i = 0; i < supercharger_bios_bin_len; i++)
        rom[i] = supercharger_bios_bin[i];


    rom[0x07ff] = rom[0x07fd] = 0xf8;
    rom[0x07fe] = rom[0x07fc] = 0x07;
}

static void load_multiload(uint8_t *ram, uint8_t phsyical_index, const char* cartridge_path) {

}

void emulate_supercharger_cartridge(const char* cartridge_path, unsigned int image_size) {
    __disable_irq();

    uint8_t ram[0x1800];
    uint8_t rom[0x0800];
    uint8_t multiload_map[0xff];

    uint16_t addr = 0, addr_prev = 0;

    uint8_t *bank0 = ram, *bank1 = rom;
    uint32_t transition_count = 0;
    bool pending_write = false;
    bool write_ram_enabled = false;
    uint8_t data_hold = 0;
    uint8_t multiload_count = image_size / 8448;

    memset(multiload_map, 0, 0xff);
    memset(ram, 0, 0x1800);

    setup_rom(&rom[0]);
    setup_multiload_map(&multiload_map[0], multiload_count, cartridge_path);

    if (!reboot_into_cartridge()) {
        __enable_irq();
        return;
    }

    while (1) {
        while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;

        if (!(addr & 0x1000)) continue;
    }

    __enable_irq();
}
