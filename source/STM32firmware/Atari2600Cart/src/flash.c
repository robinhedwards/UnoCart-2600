#include "stm32f4xx_flash.h"

#include "flash.h"

#define RESERVED_FLASH_KB       64

extern uint8_t _END_OF_FLASH;
#define FIRST_FREE_BYTE (uint32_t)(&_END_OF_FLASH)
#define DEVICE_ID_FLASH_SIZE ((uint16_t*)0x1fff7a22)

static const uint8_t flash_sector[] = {
    FLASH_Sector_0,
    FLASH_Sector_1,
    FLASH_Sector_2,
    FLASH_Sector_3,
    FLASH_Sector_4,
    FLASH_Sector_5,
    FLASH_Sector_6,
    FLASH_Sector_7,
    FLASH_Sector_8,
    FLASH_Sector_9,
    FLASH_Sector_10,
    FLASH_Sector_11,
    FLASH_Sector_12,
    FLASH_Sector_13,
    FLASH_Sector_14,
    FLASH_Sector_15,
    FLASH_Sector_16,
    FLASH_Sector_17,
    FLASH_Sector_18,
    FLASH_Sector_19,
    FLASH_Sector_20,
    FLASH_Sector_21,
    FLASH_Sector_22,
    FLASH_Sector_23
};

static const uint32_t sector_boundaries[] = {
    0x08004000,
    0x08008000,
    0x0800c000,
    0x08010000,
    0x08020000,
    0x08040000,
    0x08060000,
    0x08080000,
    0x080a0000,
    0x080c0000,
    0x080e0000,
    0x08100000,
    // STM32F42/STM32F43
    0x08104000,
    0x08108000,
    0x0810c000,
    0x08110000,
    0x08120000,
    0x08140000,
    0x08160000,
    0x08180000,
    0x081a0000,
    0x081c0000,
    0x081e0000,
    0x08200000
};

static uint32_t flash_size_bytes() {
    return (*DEVICE_ID_FLASH_SIZE) * 1024;
}

static uint32_t highest_flash_address() {
    return 0x08000000 + flash_size_bytes() - 1;
}

static uint8_t sector_id_for_address(uint32_t address) {
    if (address < 0x08000000) return 0xff;

    for (uint8_t i = 0; i < 24; i++) {
        if (address < sector_boundaries[i]) return i;
    }

    return 0xff;
}

static uint32_t lowest_available_flash_address() {
    uint8_t firmware_size = FIRST_FREE_BYTE - 0x08000000;

    return (RESERVED_FLASH_KB * 1024) > firmware_size ?
        (0x08000000 + RESERVED_FLASH_KB * 1024) :
        FIRST_FREE_BYTE;
}

uint32_t available_flash() {
    uint8_t last_reserved_sector = sector_id_for_address(lowest_available_flash_address() - 1);
    uint8_t highest_available_sector = sector_id_for_address(highest_flash_address());

    if (last_reserved_sector == 0xff || highest_available_sector == 0xff) return 0;

    return sector_boundaries[highest_available_sector] - sector_boundaries[last_reserved_sector];
}

bool prepare_flash(uint32_t size, flash_context* context) {
    if (size > avaiable_flash() || size == 0) return false;

    const uint8_t first_sector_id = sector_id_for_address(highest_flash_address() - size + 1);
    const uint8_t last_sector_id = sector_id_for_address(highest_flash_address());

    if (first_sector_id == 0xff || last_sector_id == 0xff) return false;
    if (first_sector_id <= sector_id_for_address(lowest_available_flash_address() - 1)) return false;

    FLASH_Unlock();
    if (FLASH_WaitForLastOperation() != FLASH_COMPLETE) goto error;

    for (uint8_t sector_id = first_sector_id; sector_id <= last_sector_id; sector_id++) {
        if (FLASH_EraseSector(flash_sector[sector_id], VoltageRange_3) != FLASH_COMPLETE) goto error;
    }

    FLASH_Lock();

    context->next_write_target = highest_flash_address() - size + 1;
    context->base = (uint8_t*)(context->next_write_target);

    return true;

    error:
        FLASH_Lock();
        return false;
}

bool write_flash(uint32_t byte_count, uint8_t* buffer, flash_context *context) {
    if (context->next_write_target < lowest_available_flash_address()) return false;
    if (context->next_write_target + byte_count - 1 > highest_flash_address()) return false;

    FLASH_Unlock();
    if (FLASH_WaitForLastOperation() != FLASH_COMPLETE) goto error;

    if (((uint32_t)buffer & 0x03) == 0x00 && (context->next_write_target & 0x03) == 0x00 && (byte_count & 0x03) == 0x00) {
        for (uint32_t i = 0; i < byte_count >> 2; i++) {
            if (FLASH_ProgramWord(context->next_write_target, *((uint32_t*)buffer)) != FLASH_COMPLETE)
                goto error;

            context->next_write_target += 4;
            buffer += 4;
        }
    }
    else if (((uint32_t)buffer & 0x01) == 0x00 && (context->next_write_target & 0x01) == 0x00 && (byte_count & 0x01) == 0) {
        for (uint32_t i = 0; i < byte_count >> 1; i++) {
            if (FLASH_ProgramHalfWord(context->next_write_target, *((uint16_t*)buffer)) != FLASH_COMPLETE)
                goto error;

            context->next_write_target += 2;
            buffer += 2;
        }
    }
    else  {
        for (uint32_t i = 0; i < byte_count; i++) {
            if (FLASH_ProgramByte(context->next_write_target, *buffer) != FLASH_COMPLETE)
                goto error;

            context->next_write_target++;
            buffer++;
        }
    }

    FLASH_Lock();
    return true;

    error:
        FLASH_Lock();
        return false;
}
