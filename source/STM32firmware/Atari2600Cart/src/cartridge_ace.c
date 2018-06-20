#include <stdbool.h>

#include "tm_stm32f4_fatfs.h"

#include "ace2600.h"
#include "cartridge_ace.h"
#include "flash.h"
#include "cartridge_firmware.h"

static const unsigned char MagicNumber[] = "ACE-2600";

int is_ace_cartridge(unsigned int image_size, uint8_t *buffer)
{
	if(image_size < sizeof(ACEFileHeader))
		return 0;

	ACEFileHeader * header = (ACEFileHeader *)buffer;

	// Check magic number
	for(int i = 0; i < 8; i++)
	{
		if(MagicNumber[i] != header->magic_number[i])
			return 0;
	}

	return 1;
}

int launch_ace_cartridge(const char* filename, uint32_t buffer_size, uint8_t *buffer)
{
	ACEFileHeader header = *((ACEFileHeader *)buffer);

    if (header.rom_size > (448*1024))
    	return false;

    flash_context ctx;

    if (!prepare_flash(header.rom_size, &ctx)) return false;

    uint32_t bytes_written_to_flash = header.rom_size > buffer_size ? buffer_size : header.rom_size;

    if (!write_flash(bytes_written_to_flash, buffer, &ctx))
    	return false;

    if (header.rom_size > buffer_size)
    {
    	FATFS fs;
    	FIL fil;
    	UINT bytes_read;

    	if (f_mount(&fs, "", 1) != FR_OK) goto fail_unmount;
    	if (f_open(&fil, filename, FA_READ) != FR_OK) goto fail_close;

        if (f_lseek(&fil, buffer_size) != FR_OK) goto fail_close;

        while (bytes_written_to_flash < header.rom_size) {
            if (f_read(&fil, buffer, buffer_size, &bytes_read) != FR_OK) goto fail_close;

            bytes_written_to_flash += bytes_read;
            if (bytes_read < buffer_size && bytes_written_to_flash < header.rom_size) goto fail_close;

            if (!write_flash(bytes_read, buffer, &ctx)) goto fail_close;
        }

        f_close(&fil);
    	f_mount(0, "", 1);

    	//return true;
    	// Should never return
    	((void (*)())header.entry_point)();
    	return false;

    	fail_close:
    		f_close(&fil);
    	fail_unmount:
    		f_mount(0, "", 1);

    	return false;
    }

	//return true;
	// Should never return
	((void (*)())header.entry_point)();
	return false;
}
