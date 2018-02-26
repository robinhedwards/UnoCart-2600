BUILDDIR = build

DEVICE = stm32/device
CORE = stm32/core
PERIPH = stm32/periph
DISCOVERY = stm32/discovery
USB = stm32/usb

SOURCES = \
	src/startup_stm32f40xx.s \
	src/system_stm32f4xx.c \
	Libraries/tm_stm32f4_delay/tm_stm32f4_delay.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c \
	Libraries/STM32F4xx_StdPeriph_Driver/src/stm32f4xx_spi.c \
	Libraries/tm_stm32f4_spi/tm_stm32f4_spi.c \
	Libraries/tm_stm32f4_gpio/tm_stm32f4_gpio.c \
	Libraries/tm_stm32f4_fatfs/tm_stm32f4_fatfs.c \
	Libraries/tm_stm32f4_fatfs/fatfs/diskio.c \
	Libraries/tm_stm32f4_fatfs/fatfs/option/ccsbcs.c \
	Libraries/tm_stm32f4_fatfs/fatfs/drivers/fatfs_sd.c \
	Libraries/tm_stm32f4_fatfs/fatfs/option/syscall.c \
	Libraries/tm_stm32f4_fatfs/fatfs/ff.c \
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

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

ELF = $(BUILDDIR)/firmware.elf
HEX = $(BUILDDIR)/firmware.hex
BIN = $(BUILDDIR)/firmware.bin

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
AR = arm-none-eabi-ar
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = \
	-Os -Wall -nostdlib \
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
	-Wl,--gc-sections

bin: $(BIN)
hex: $(HEX)
elf: $(ELF)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

flash: $(BIN)
	st-flash write $(BIN) 0x8000000

clean:
	rm -rf build
