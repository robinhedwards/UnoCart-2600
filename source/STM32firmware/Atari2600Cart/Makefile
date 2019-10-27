# Use the official ARM Cortex GNU toolchain
#     (http//developer.arm.com/open-source/gnu-toolchain/gnu-rm)
# and the open source stlink tool suite
#     (https://github.com/texane/stlink)
# to build and flash the firmware with this Makefile.
#
# Makefile derived from https://github.com/nitsky/stm32-example

BUILDDIR = build
DEPDIR = .depend/build

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

STFLASH = st-flash

SOURCES = \
	src/startup_stm32f40xx.s \
	src/system_stm32f4xx.c \
	Libraries/tm_stm32f4_delay/tm_stm32f4_delay.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_spi.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_flash.c \
	Libraries/tm_stm32f4_spi/tm_stm32f4_spi.c \
	Libraries/tm_stm32f4_gpio/tm_stm32f4_gpio.c \
	Libraries/tm_stm32f4_fatfs/tm_stm32f4_fatfs.c \
	Libraries/tm_stm32f4_fatfs/fatfs/diskio.c \
	Libraries/tm_stm32f4_fatfs/fatfs/option/ccsbcs.c \
	Libraries/tm_stm32f4_fatfs/fatfs/drivers/fatfs_sd.c \
	Libraries/tm_stm32f4_fatfs/fatfs/option/syscall.c \
	Libraries/tm_stm32f4_fatfs/fatfs/ff.c \
	src/cartridge_firmware.c \
	src/cartridge_supercharger.c \
	src/cartridge_3f.c \
	src/cartridge_3e.c \
	src/flash.c \
	src/main.c

INCLUDES = \
	-Isrc \
	-ILibraries/CMSIS/Include \
	-ILibraries/Device/STM32F4xx/Include \
	-ILibraries/STM32F4xx_StdPeriph_Driver/inc \
	-ILibraries/tm_stm32f4_fatfs \
	-ILibraries/tm_stm32f4_fatfs/fatfs \
	-ILibraries/tm_stm32f4_fatfs/fatfs/drivers \
	-ILibraries/tm_stm32f4_delay \
	-ILibraries/tm_stm32f4_gpio \
	-ILibraries/tm_stm32f4_spi

GARBAGE = $(DEPDIR) $(BUILDDIR)

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

ELF = $(BUILDDIR)/firmware.elf
HEX = $(BUILDDIR)/firmware.hex
BIN = $(BUILDDIR)/firmware.bin
MAP = $(BUILDDIR)/firmware.map

CFLAGS = \
	-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d \
	-Os -Wall -g \
	-mcpu=cortex-m4 -mthumb \
	-mfpu=fpv4-sp-d16 -mfloat-abi=softfp \
	-DSTM32F4XX \
	-DSTM32F40XX \
	-DUSE_STM32F4_DISCOVERY \
	-DHSE_VALUE=8000000 \
	-DUSE_STDPERIPH_DRIVER \
	-ffunction-sections \
	-fdata-sections \
	$(INCLUDES)

LDSCRIPT = stm32f4_flash.ld
LDFLAGS += \
	-T$(LDSCRIPT) \
	-mthumb -mcpu=cortex-m4 \
	--specs=nosys.specs \
	-Wl,--gc-sections \
	-Wl,-Map=$(MAP)

bin: $(BIN)
hex: $(HEX)
elf: $(ELF)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(ELF): $(OBJECTS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	mkdir -p .depend/$(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

flash: $(BIN)
	$(STFLASH) write $(BIN) 0x8000000

clean:
	rm -rf $(GARBAGE)

.PHONY: bin elf hex flash clean

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SOURCES))))
