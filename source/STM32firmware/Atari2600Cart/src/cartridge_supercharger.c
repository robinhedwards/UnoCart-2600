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

static void setup_multiload_map(uint8_t *multiload_map, uint8_t multiload_count, const char* cartridge_path) {
    FATFS fs;
    FIL fil;
    UINT bytes_read;
    LoadHeader header;

    memset(multiload_map, 0, 0xff);

    if (f_mount(&fs, "", 1) != FR_OK) goto unmount;
	if (f_open(&fil, cartridge_path, FA_READ) != FR_OK) goto close;

    for (uint8_t i = 0; i < multiload_count; i++) {
        f_lseek(&fil, (i + 1) * 8448 - 256);

        f_read(&fil, &header, sizeof(LoadHeader), &bytes_read);
        multiload_map[header.multiload_id] = i;
    }

    close:
        f_close(&fil);

    unmount:
		f_mount(0, "", 1);
}

static void setup_rom(uint8_t* rom) {
    memset(rom, 0, 0x0800);
    memcpy(rom, supercharger_bios_bin, supercharger_bios_bin_len);

    rom[0x07ff] = rom[0x07fd] = 0xf8;
    rom[0x07fe] = rom[0x07fc] = 0x07;
}

static void read_multiload(uint8_t *buffer, const char* cartridge_path, uint8_t physical_index) {
    __enable_irq();

    FATFS fs;
    FIL fil;

    if (f_mount(&fs, "", 1) != FR_OK) goto unmount;
	if (f_open(&fil, cartridge_path, FA_READ) != FR_OK) goto close;

    f_lseek(&fil, physical_index * 8448);

    UINT bytes_read;
    f_read(&fil, buffer, 8448, &bytes_read);

    close:
        f_close(&fil);

    unmount:
		f_mount(0, "", 1);

    __disable_irq();
}

static void load_multiload(uint8_t *ram, uint8_t physical_index, const char* cartridge_path) {
    uint8_t buffer[8448];
    LoadHeader *header = (void*)buffer + 8448 - 256;

    read_multiload(buffer, cartridge_path, physical_index);

    for (uint8_t i = 0; i < header->block_count; i++) {
        uint8_t location = header->block_location[i];
        uint8_t bank = (location & 0x03) % 3;
        uint8_t base = (location & 0x1f) >> 2;

        memcpy(ram + bank * 2048 + base * 256, buffer + 256 * i, 256);
    }
}

void emulate_supercharger_cartridge(const char* cartridge_path, unsigned int image_size) {
    uint8_t ram[0x1800];
    uint8_t rom[0x0800];
    uint8_t multiload_map[0xff];

    uint16_t addr = 0, addr_prev = 0, data_prev = 0, data = 0;

    uint8_t *bank0 = ram, *bank1 = rom;
    uint32_t transition_count = 0;
    bool pending_write = false;
    bool write_ram_enabled = false;
    uint8_t data_hold = 0;
    uint8_t multiload_count = image_size / 8448;

    memset(ram, 0, 0x1800);

    setup_rom(rom);
    setup_multiload_map(multiload_map, multiload_count, cartridge_path);

    if (!reboot_into_cartridge()) return;

    __disable_irq();

    while (1) {
        while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;

        if (transition_count < 6) transition_count++;

        if (!(addr & 0x1000)) continue;

        if (addr == 0x1ff9 && bank1 == rom && addr_prev <= 0xff) {
            while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }

            load_multiload(ram, multiload_map[data_prev >> 8], cartridge_path);

            continue;
        }

        uint8_t value_out = addr < 0x1800 ? bank0[addr & 0x07ff] : bank1[addr & 0x07ff];

        if ((addr & 0x0f00) == 0 && (!pending_write || !write_ram_enabled)) {
            data_hold = addr & 0xff;
            transition_count = 0;
            pending_write = true;
        }
        else if (addr == 0x1ff8) {
            pending_write = false;
            write_ram_enabled = data_hold & 0x02;
            switch (data_hold & 0x1c) {
                case 4:
                case 0:
                    bank0 = ram + 2048 * 2;
                    bank1 = rom;
                    break;

                case 1:
                    bank0 = ram;
                    bank1 = rom;
                    break;

                case 2:
                    bank0 = ram + 2048 * 2;
                    bank1 = ram;
                    break;

                case 3:
                    bank0 = ram;
                    bank1 = ram + 2048 * 2;
                    break;

                case 5:
                    bank0 = ram + 2048;
                    bank1 = rom;
                    break;

                case 6:
                    bank0 = ram + 2048 * 2;
                    bank1 = ram + 2048;
                    break;

                case 7:
                    bank0 = ram + 2048;
                    bank1 = ram + 2048 * 2;
                    break;
            }
        }
        else if (pending_write && write_ram_enabled && transition_count == 5) {
            pending_write = false;

            if (addr < 0x1800) {
                bank0[addr & 0x07ff] = value_out = data_hold;
            }
            else if (bank1 != rom) {
                bank0[addr & 0x07ff] = value_out = data_hold;
            }
        }

        DATA_OUT = ((uint16_t)value_out)<<8;
        SET_DATA_MODE_OUT
        // wait for address bus to change
        while (ADDR_IN == addr) ;
        SET_DATA_MODE_IN
    }

    __enable_irq();
}
