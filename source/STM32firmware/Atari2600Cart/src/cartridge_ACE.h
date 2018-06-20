#ifndef CARTRIDGE_ACE_H
#define CARTRIDGE_ACE_H

int is_ace_cartridge(unsigned int image_size, uint8_t *buffer);

int launch_ace_cartridge(const char* filename, uint32_t buffer_size, uint8_t *buffer);

#endif // CARTRIDGE_SUPERCHARGER_H
