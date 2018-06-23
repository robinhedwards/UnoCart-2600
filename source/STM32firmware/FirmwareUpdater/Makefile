################################################################################
# 
# The MIT License (MIT)
# Copyright (c) 2018 STMicroelectronics
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
################################################################################

SHELL=cmd

# System configuration
CC = arm-atollic-eabi-gcc
CXX = arm-atollic-eabi-g++
OBJCOPY = arm-atollic-eabi-objcopy
RM=rm -rf

# Assembler, Compiler and Linker flags and linker script settings
LINKER_FLAGS=-lm -mthumb -mhard-float -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -Wl,--gc-sections -T$(LINK_SCRIPT) -static  -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group -specs=nano.specs -specs=nosys.specs  -Wl,-cref "-Wl,-Map=$(BIN_DIR)/FirmwareUpdater.map" -Wl,--defsym=malloc_getpagesize_P=0x1000
LINK_SCRIPT="stm32f4_flash.ld"
ASSEMBLER_FLAGS=-c -g -O0 -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -mhard-float -specs=nano.specs -D"STM32F4XX" -D"STM32F40XX" -D"USE_STDPERIPH_DRIVER"  -x assembler-with-cpp  -Isrc -ILibraries/CMSIS/Include -ILibraries/Device/STM32F4xx/Include -ILibraries/STM32F4xx_StdPeriph_Driver/inc
COMPILER_FLAGS=-c -g -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -O0 -Wall -ffunction-sections -fdata-sections -mthumb -mhard-float -specs=nano.specs -D"STM32F4XX" -D"STM32F40XX" -D"USE_STDPERIPH_DRIVER"   -Isrc -ILibraries/CMSIS/Include -ILibraries/Device/STM32F4xx/Include -ILibraries/STM32F4xx_StdPeriph_Driver/inc
CXXCOMPILER_FLAGS=-fno-threadsafe-statics -c -g -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -O0 -Wall -ffunction-sections -fdata-sections -mthumb -mhard-float -specs=nano.specs -fno-exceptions -fno-rtti -D"STM32F4XX" -D"STM32F40XX" -D"USE_STDPERIPH_DRIVER"   -Isrc -ILibraries/CMSIS/Include -ILibraries/Device/STM32F4xx/Include -ILibraries/STM32F4xx_StdPeriph_Driver/inc

# Define output directory
OBJECT_DIR = Debug
BIN_DIR = $(OBJECT_DIR)

# Define sources and objects
SRC := $(wildcard */*/*/*/*/*/*/*.c) \
	$(wildcard */*/*/*/*/*/*.c) \
	$(wildcard */*/*/*/*/*.c) \
	$(wildcard */*/*/*/*.c) \
	$(wildcard */*/*/*.c) \
	$(wildcard */*/*.c) \
	$(wildcard */*.c) \
	$(wildcard */*/*/*/*/*/*/*.cpp) \
	$(wildcard */*/*/*/*/*/*.cpp) \
	$(wildcard */*/*/*/*/*.cpp) \
	$(wildcard */*/*/*/*.cpp) \
	$(wildcard */*/*/*.cpp) \
	$(wildcard */*/*.cpp) \
	$(wildcard */*.cpp)
SRCSASM := $(wildcard */*/*/*/*/*/*/*/*.s) \
	$(wildcard */*/*/*/*/*/*/*.s) \
	$(wildcard */*/*/*/*/*/*.s) \
	$(wildcard */*/*/*/*/*.s) \
	$(wildcard */*/*/*/*.s) \
	$(wildcard */*/*/*.s) \
	$(wildcard */*/*.s) \
	$(wildcard */*.s)
OBJS := $(SRC:%.c=$(OBJECT_DIR)/%.o) $(SRCSASM:%.s=$(OBJECT_DIR)/%.o)
OBJS := $(OBJS:%.S=$(OBJECT_DIR)/%.o)  
OBJS := $(OBJS:%.cpp=$(OBJECT_DIR)/%.o)

ELF = $(BIN_DIR)/FirmwareUpdater.elf
BIN = $(BIN_DIR)/FirmwareUpdater.bin

###############
# Build project
# Major targets
###############
all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $(ELF) $(BIN)

$(ELF): $(OBJS) $(OBJECT_DIR)/src/AtariCart.o
	$(CXX) -o "$(ELF)" $(OBJECT_DIR)/src/AtariCart.o $(OBJS) $(LINKER_FLAGS)

clean:
	$(RM) $(OBJS) "$(BIN_DIR)/FirmwareUpdater.elf" "$(BIN_DIR)/FirmwareUpdater.map"


##################
# Specific targets
##################
$(OBJECT_DIR)/src/AtariCart.o: ../Atari2600Cart/Release/AtariCart.elf
	$(OBJCOPY) -O binary ../Atari2600Cart/Release/AtariCart.elf $(BIN_DIR)/AtariCart.bin
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm $(BIN_DIR)/AtariCart.bin $(OBJECT_DIR)/src/AtariCart.o

../AtariCart/Release/AtariCart.elf:	

##################
# Implicit targets
##################
$(OBJECT_DIR)/%.o: %.c stm32f4_flash.ld
	@mkdir $(subst /,\,$(dir $@)) 2> NUL || echo off
	$(CC) $(COMPILER_FLAGS) $< -o $@

$(OBJECT_DIR)/%.o: %.cpp stm32f4_flash.ld
	@mkdir $(subst /,\,$(dir $@)) 2> NUL || echo off
	$(CXX) $(CXXCOMPILER_FLAGS) $< -o $@

$(OBJECT_DIR)/%.o: %.s stm32f4_flash.ld
	@mkdir $(subst /,\,$(dir $@)) 2> NUL || echo off
	$(CC) $(ASSEMBLER_FLAGS) $< -o $@
	
$(OBJECT_DIR)/%.o: %.S stm32f4_flash.ld
	@mkdir $(subst /,\,$(dir $@)) 2> NUL || echo off
	$(CC) $(ASSEMBLER_FLAGS) $< -o $@
