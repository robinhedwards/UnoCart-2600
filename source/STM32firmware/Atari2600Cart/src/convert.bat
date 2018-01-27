copy "UnoCart2600_PAL.bin" firmware_pal.rom
xxd -i firmware_pal.rom > firmware_pal_rom.h
del firmware_pal.rom

copy "UnoCart2600_PAL60.bin" firmware_pal60.rom
xxd -i firmware_pal60.rom > firmware_pal60_rom.h
del firmware_pal60.rom

copy "UnoCart2600_NTSC.bin" firmware_ntsc.rom
xxd -i firmware_ntsc.rom > firmware_ntsc_rom.h
del firmware_ntsc.rom