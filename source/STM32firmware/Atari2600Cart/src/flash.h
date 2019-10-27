#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t* base;
    uint32_t next_write_target;
} flash_context;

uint32_t available_flash();

bool prepare_flash(uint32_t size, flash_context *context);

bool write_flash(uint32_t byte_count, uint8_t* buffer, flash_context *context);

#endif // FLASH_H
