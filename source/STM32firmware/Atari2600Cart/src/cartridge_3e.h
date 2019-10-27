#ifndef CARTRIDGE_3E_H
#define CARTRIDGE_3E_H

#include <stdint.h>

void emulate_3e_cartridge(const char* filename, uint32_t image_size, uint8_t* buffer, uint8_t ram_banks);

#endif // CARTRIDGE_3F_H
