/* ----------------------------------------------------------
 * UnoCart2600 Firmware (c)2018 Robin Edwards (ElectroTrains)
 * ----------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE

#include "defines.h"
#include "stm32f4xx.h"
#include "tm_stm32f4_fatfs.h"
#include "tm_stm32f4_delay.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "firmware_pal_rom.h"
#include "firmware_pal60_rom.h"
#include "firmware_ntsc_rom.h"

unsigned const char *firmware_rom;

/*************************************************************************
 * Cartridge Definitions
 *************************************************************************/
#define MAX_CART_ROM_SIZE	64	// in kilobytes
#define MAX_CART_RAM_SIZE	32	// in kilobytes

unsigned char cart_rom[MAX_CART_ROM_SIZE*1024];	// largest is 32k cart
unsigned char cart_ram[MAX_CART_RAM_SIZE*1024];
unsigned char menu_ram[1024];	// < NUM_DIR_ITEMS * 12
char menu_status[16];

#define CART_TYPE_NONE	0
#define CART_TYPE_2K	1
#define CART_TYPE_4K	2
#define CART_TYPE_F8	3	// 8k
#define CART_TYPE_F6	4	// 16k
#define CART_TYPE_F4	5	// 32k
#define CART_TYPE_F8SC	6	// 8k+ram
#define CART_TYPE_F6SC	7	// 16k+ram
#define CART_TYPE_F4SC	8	// 32k+ram
#define CART_TYPE_FE	9	// 8k
#define CART_TYPE_3F	10	// varies (examples 8k)
#define CART_TYPE_3E	11	// varies (only example 32k)
#define CART_TYPE_E0	12	// 8k
#define CART_TYPE_0840	13	// 8k
#define CART_TYPE_CV	14	// 2k+ram
#define CART_TYPE_EF	15	// 64k
#define CART_TYPE_EFSC	16	// 64k+ram
#define CART_TYPE_F0	17	// 64k
#define CART_TYPE_FA	18	// 12k
#define CART_TYPE_E7	19	// 16k+ram

typedef struct {
	const char *ext;
	int cart_type;
} EXT_TO_CART_TYPE_MAP;

EXT_TO_CART_TYPE_MAP ext_to_cart_type_map[] = {
    {"ROM", CART_TYPE_NONE},
    {"BIN", CART_TYPE_NONE},
    {"A26", CART_TYPE_NONE},
    {"2K", CART_TYPE_2K},
    {"4K", CART_TYPE_4K},
    {"F8", CART_TYPE_F8},
    {"F6", CART_TYPE_F6},
    {"F4", CART_TYPE_F4},
    {"F8S", CART_TYPE_F8SC},
    {"F6S", CART_TYPE_F6SC},
    {"F4S", CART_TYPE_F4SC},
    {"FE", CART_TYPE_FE},
    {"3F", CART_TYPE_3F},
    {"3E", CART_TYPE_3E},
    {"E0", CART_TYPE_E0},
    {"084", CART_TYPE_0840},
    {"CV", CART_TYPE_CV},
	{"EF", CART_TYPE_EF},
	{"EFS", CART_TYPE_EFSC},
	{"F0", CART_TYPE_F0},
	{"FA", CART_TYPE_FA},
	{"E7", CART_TYPE_E7},
	{0,0}
};

/*************************************************************************
 * Cartridge Type Detection
 *************************************************************************/

/* The following detection routines are modified from the Atari 2600 Emulator Stella
  (https://github.com/stella-emu) */

int isProbablySC(int size, unsigned char *bytes)
{
	int banks = size/4096;
	for (int i = 0; i < banks; i++)
	{
		for (int j = 0; j < 128; j++)
		{
			if (bytes[i*4096+j] != bytes[i*4096+j+128])
				return 0;
		}
	}
	return 1;
}

int searchForBytes(unsigned char *bytes, int size, unsigned char *signature, int sigsize, int minhits)
{
	int count = 0;
	for(int i = 0; i < size - sigsize; ++i)
	{
		int matches = 0;
		for(int j = 0; j < sigsize; ++j)
		{
			if(bytes[i+j] == signature[j])
				++matches;
			else
				break;
		}
		if(matches == sigsize)
		{
			++count;
			i += sigsize;  // skip past this signature 'window' entirely
		}
		if(count >= minhits)
			break;
	}
	return (count >= minhits);
}

int isProbablyFE(int size, unsigned char *bytes)
{	// These signatures are attributed to the MESS project
	unsigned char signature[4][5] = {
		{ 0x20, 0x00, 0xD0, 0xC6, 0xC5 },  // JSR $D000; DEC $C5
		{ 0x20, 0xC3, 0xF8, 0xA5, 0x82 },  // JSR $F8C3; LDA $82
		{ 0xD0, 0xFB, 0x20, 0x73, 0xFE },  // BNE $FB; JSR $FE73
		{ 0x20, 0x00, 0xF0, 0x84, 0xD6 }   // JSR $F000; STY $D6
	};
	for (int i = 0; i < 4; ++i)
		if(searchForBytes(bytes, size, signature[i], 5, 1))
			return 1;

	return 0;
}

int isProbably3F(int size, unsigned char *bytes)
{	// 3F cart bankswitching is triggered by storing the bank number
	// in address 3F using 'STA $3F'
	// We expect it will be present at least 2 times, since there are
	// at least two banks
	unsigned char signature[] = { 0x85, 0x3F };  // STA $3F
	return searchForBytes(bytes, size, signature, 2, 2);
}

int isProbably3E(int size, unsigned char *bytes)
{	// 3E cart bankswitching is triggered by storing the bank number
	// in address 3E using 'STA $3E', commonly followed by an
	// immediate mode LDA
	unsigned char  signature[] = { 0x85, 0x3E, 0xA9, 0x00 };  // STA $3E; LDA #$00
	return searchForBytes(bytes, size, signature, 4, 1);
}

int isProbablyE0(int size, unsigned char *bytes)
{	// E0 cart bankswitching is triggered by accessing addresses
	// $FE0 to $FF9 using absolute non-indexed addressing
	// These signatures are attributed to the MESS project
	unsigned char signature[8][3] = {
		    { 0x8D, 0xE0, 0x1F },  // STA $1FE0
		    { 0x8D, 0xE0, 0x5F },  // STA $5FE0
		    { 0x8D, 0xE9, 0xFF },  // STA $FFE9
		    { 0x0C, 0xE0, 0x1F },  // NOP $1FE0
		    { 0xAD, 0xE0, 0x1F },  // LDA $1FE0
		    { 0xAD, 0xE9, 0xFF },  // LDA $FFE9
		    { 0xAD, 0xED, 0xFF },  // LDA $FFED
		    { 0xAD, 0xF3, 0xBF }   // LDA $BFF3
		};
	for (int i = 0; i < 8; ++i)
		if(searchForBytes(bytes, size, signature[i], 3, 1))
			return 1;
	return 0;
}

int isProbably0840(int size, unsigned char *bytes)
{	// 0840 cart bankswitching is triggered by accessing addresses 0x0800
	// or 0x0840 at least twice
	unsigned char signature1[3][3] = {
		    { 0xAD, 0x00, 0x08 },  // LDA $0800
		    { 0xAD, 0x40, 0x08 },  // LDA $0840
		    { 0x2C, 0x00, 0x08 }   // BIT $0800
		};
	for (int i = 0; i < 3; ++i)
		if(searchForBytes(bytes, size, signature1[i], 3, 2))
			return 1;

	unsigned char signature2[2][4] = {
		    { 0x0C, 0x00, 0x08, 0x4C },  // NOP $0800; JMP ...
		    { 0x0C, 0xFF, 0x0F, 0x4C }   // NOP $0FFF; JMP ...
		};
	for (int i = 0; i < 2; ++i)
		if(searchForBytes(bytes, size, signature2[i], 4, 2))
			return 1;

	return 0;
}

int isProbablyCV(int size, unsigned char *bytes)
{ 	// CV RAM access occurs at addresses $f3ff and $f400
	// These signatures are attributed to the MESS project
	unsigned char signature[2][3] = {
		    { 0x9D, 0xFF, 0xF3 },  // STA $F3FF.X
		    { 0x99, 0x00, 0xF4 }   // STA $F400.Y
		};
	for (int i = 0; i < 2; ++i)
		if(searchForBytes(bytes, size, signature[i], 3, 1))
			return 1;
	return 0;
}

int isProbablyEF(int size, unsigned char *bytes)
{ 	// EF cart bankswitching switches banks by accessing addresses
	// 0xFE0 to 0xFEF, usually with either a NOP or LDA
	// It's likely that the code will switch to bank 0, so that's what is tested
	unsigned char signature[4][3] = {
		    { 0x0C, 0xE0, 0xFF },  // NOP $FFE0
		    { 0xAD, 0xE0, 0xFF },  // LDA $FFE0
		    { 0x0C, 0xE0, 0x1F },  // NOP $1FE0
		    { 0xAD, 0xE0, 0x1F }   // LDA $1FE0
		};
	for (int i = 0; i < 4; ++i)
		if(searchForBytes(bytes, size, signature[i], 3, 1))
			return 1;
	return 0;
}

int isProbablyE7(int size, unsigned char *bytes)
{ 	// These signatures are attributed to the MESS project
	unsigned char signature[7][3] = {
		    { 0xAD, 0xE2, 0xFF },  // LDA $FFE2
		    { 0xAD, 0xE5, 0xFF },  // LDA $FFE5
		    { 0xAD, 0xE5, 0x1F },  // LDA $1FE5
		    { 0xAD, 0xE7, 0x1F },  // LDA $1FE7
		    { 0x0C, 0xE7, 0x1F },  // NOP $1FE7
		    { 0x8D, 0xE7, 0xFF },  // STA $FFE7
		    { 0x8D, 0xE7, 0x1F }   // STA $1FE7
		};
	for (int i = 0; i < 7; ++i)
		if(searchForBytes(bytes, size, signature[i], 3, 1))
			return 1;
	return 0;
}

/*************************************************************************
 * File/Directory Handling
 *************************************************************************/

#define NUM_DIR_ITEMS	80

typedef struct {
	char isDir;
	char filename[13];
	char long_filename[32];
} DIR_ENTRY;

DIR_ENTRY dir_entries[NUM_DIR_ITEMS];

int num_dir_entries = 0; // how many entries in the current directory

int entry_compare(const void* p1, const void* p2)
{
	DIR_ENTRY* e1 = (DIR_ENTRY*)p1;
	DIR_ENTRY* e2 = (DIR_ENTRY*)p2;
	if (e1->isDir && !e2->isDir) return -1;
	else if (!e1->isDir && e2->isDir) return 1;
	else return stricmp(e1->long_filename, e2->long_filename);
}

char *get_filename_ext(char *filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int is_valid_file(char *filename) {
	char *ext = get_filename_ext(filename);
	EXT_TO_CART_TYPE_MAP *p = ext_to_cart_type_map;
	while (p->ext) {
		if (stricmp(ext, p->ext) == 0)
			return 1;
		p++;
	}
	return 0;
}

// single FILINFO structure
FILINFO fno;
char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */

void init() {
	// this seems to be required for this version of FAT FS
	fno.lfname = lfn;
	fno.lfsize = sizeof lfn;
}

int read_directory(char *path) {
	int ret = 0;
	num_dir_entries = 0;
	DIR_ENTRY *dst = (DIR_ENTRY *)&dir_entries[0];

	FATFS FatFs;
	TM_DELAY_Init();
	if (f_mount(&FatFs, "", 1) == FR_OK) {
		DIR dir;
		if (f_opendir(&dir, path) == FR_OK) {
			if (strlen(path))
			{	// not root directory, add pseudo ".." to go up a dir
				dst->isDir = 1;
				strcpy(dst->long_filename, "(GO BACK)");
				strcpy(dst->filename, "..");
				num_dir_entries++;
				dst++;
			}
			while (num_dir_entries < NUM_DIR_ITEMS) {
				if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
					break;
				if (fno.fattrib & (AM_HID | AM_SYS))
					continue;
				dst->isDir = fno.fattrib & AM_DIR ? 1 : 0;
				if (!dst->isDir)
					if (!is_valid_file(fno.fname)) continue;
				// copy file record
	            strcpy(dst->filename, fno.fname);
				if (fno.lfname[0]) {
					strncpy(dst->long_filename, fno.lfname, 31);
					dst->long_filename[31] = 0;
				}
				else strcpy(dst->long_filename, fno.fname);
	            dst++;
				num_dir_entries++;
			}
			f_closedir(&dir);
			qsort((DIR_ENTRY *)&dir_entries[0], num_dir_entries, sizeof(DIR_ENTRY), entry_compare);
			ret = 1;
		}
		f_mount(0, "", 1);
	}
	return ret;
}

int read_file(char *filename, int *cart_size_bytes)
{
	TM_DELAY_Init();
	FATFS FatFs;
	UINT bytes_read;
	int cart_type = CART_TYPE_NONE;

	if (f_mount(&FatFs, "", 1) != FR_OK)
		return CART_TYPE_NONE;
	FIL fil;
	if (f_open(&fil, filename, FA_READ) != FR_OK)
		goto cleanup;
	if (f_read(&fil, &cart_rom[0], MAX_CART_ROM_SIZE*1024, &bytes_read) != FR_OK)
		goto closefile;

	// auto-detect cart type - largely follows code in Stella's CartDetector.cpp
	if (bytes_read == 2*1024)
	{
		if (isProbablyCV(bytes_read, cart_rom))
			cart_type = CART_TYPE_CV;
		else
			cart_type = CART_TYPE_2K;
	}
	else if (bytes_read == 4*1024)
	{
		cart_type = CART_TYPE_4K;
	}
	else if (bytes_read == 8*1024)
	{
		// First check for *potential* F8
		unsigned char  signature[] = { 0x8D, 0xF9, 0x1F };  // STA $1FF9
		int f8 = searchForBytes(cart_rom, bytes_read, signature, 3, 2);

		if (isProbablySC(bytes_read, cart_rom))
			cart_type = CART_TYPE_F8SC;
		else if (memcmp(cart_rom, cart_rom + 4096, 4096) == 0)
			cart_type = CART_TYPE_4K;
		else if (isProbablyE0(bytes_read, cart_rom))
			cart_type = CART_TYPE_E0;
		else if (isProbably3E(bytes_read, cart_rom))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, cart_rom))
			cart_type = CART_TYPE_3F;
		else if (isProbablyFE(bytes_read, cart_rom) && !f8)
			cart_type = CART_TYPE_FE;
		else if (isProbably0840(bytes_read, cart_rom))
			cart_type = CART_TYPE_0840;
		else
			cart_type = CART_TYPE_F8;
	}
	else if (bytes_read == 12*1024)
	{
		cart_type = CART_TYPE_FA;
	}
	else if (bytes_read == 16*1024)
	{
		if (isProbablySC(bytes_read, cart_rom))
			cart_type = CART_TYPE_F6SC;
		else if (isProbablyE7(bytes_read, cart_rom))
			cart_type = CART_TYPE_E7;
		else if (isProbably3E(bytes_read, cart_rom))
			cart_type = CART_TYPE_3E;
		else
			cart_type = CART_TYPE_F6;
	}
	else if (bytes_read == 32*1024)
	{
		if (isProbablySC(bytes_read, cart_rom))
			cart_type = CART_TYPE_F4SC;
		else if (isProbably3E(bytes_read, cart_rom))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, cart_rom))
			cart_type = CART_TYPE_3F;
		else
			cart_type = CART_TYPE_F4;
	}
	else if (bytes_read == 64*1024)
	{
		if (isProbably3E(bytes_read, cart_rom))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, cart_rom))
			cart_type = CART_TYPE_3F;
		else if (isProbablyEF(bytes_read, cart_rom))
		{
			if (isProbablySC(bytes_read, cart_rom))
				cart_type = CART_TYPE_EFSC;
			else
				cart_type = CART_TYPE_EF;
		}
		else
			cart_type = CART_TYPE_F0;
	}

	// override type by file extension?
	char *ext = get_filename_ext(filename);
	EXT_TO_CART_TYPE_MAP *p = ext_to_cart_type_map;
	while (p->ext) {
		if (stricmp(ext, p->ext) == 0) {
			if (p->cart_type) cart_type = p->cart_type;
			break;
		}
		p++;
	}
	*cart_size_bytes = bytes_read;

closefile:
	f_close(&fil);
cleanup:
	f_mount(0, "", 1);
	return cart_type;
}

/*************************************************************************
 * MCU Initialisation
 *************************************************************************/

GPIO_InitTypeDef  GPIO_InitStructure;

/* Input/Output data GPIO pins on PE{8..15} */
void config_gpio_data(void) {
	/* GPIOE Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

	/* Configure GPIO Settings */
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
		GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/* Input Address GPIO pins on PD{0..15} */
void config_gpio_addr(void) {
	/* GPIOD Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	/* Configure GPIO Settings */
	GPIO_InitStructure.GPIO_Pin =
		GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
		GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 |
		GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 |
		GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/* Input Signals GPIO pins - PC0 (PAL-high/NTSC-low) */
void config_gpio_sig(void) {
	/* GPIOC Periph clock enable */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	/* Configure GPIO Settings */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

#define ADDR_IN GPIOD->IDR
#define DATA_IN GPIOE->IDR
#define DATA_OUT GPIOE->ODR
#define CONTROL_IN GPIOC->IDR
#define SET_DATA_MODE_IN GPIOE->MODER = 0x00000000;
#define SET_DATA_MODE_OUT GPIOE->MODER = 0x55550000;

/*************************************************************************
 * Cartridge Emulation
 *************************************************************************/

#define CART_CMD_SEL_ITEM_n	0x1E00
#define CART_CMD_ROOT_DIR	0x1EF0
#define CART_CMD_START_CART	0x1EFF

#define CART_STATUS_BYTES	0x1FE0	// 16 bytes of status

int emulate_firmware_cartridge() {
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if ((addr & 0x1F00) == 0x1E00) break;	// atari 2600 has sent a command
			if (addr >= 0x1800 && addr < 0x1C00)
				DATA_OUT = ((uint16_t)menu_ram[addr&0x3FF])<<8;
			else if ((addr & 0x1FF0) == CART_STATUS_BYTES)
				DATA_OUT = ((uint16_t)menu_status[addr&0xF])<<8;
			else
				DATA_OUT = ((uint16_t)firmware_rom[addr&0xFFF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}

	__enable_irq();
	return addr;
}

void emulate_2k_cartridge() {
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;
	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			DATA_OUT = ((uint16_t)cart_rom[addr&0x7FF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}

void emulate_4k_cartridge() {
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;
	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			DATA_OUT = ((uint16_t)cart_rom[addr&0xFFF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}

/* 'Standard' Bankswitching
 * ------------------------
 * Used by F8(8k), F6(16k), F4(32k), EF(64k)
 * and F8SC(8k), F6SC(16k), F4SC(32k), EFSC(64k)
 *
 * SC variants have 128 bytes of RAM:
 * RAM read port is $1080 - $10FF, write port is $1000 - $107F.
 */
void emulate_FxSC_cartridge(uint16_t lowBS, uint16_t highBS, int isSC)
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (addr >= lowBS && addr <= highBS)	// bank-switch
				bankPtr = &cart_rom[(addr-lowBS)*4*1024];

			if (isSC && (addr & 0x1F00) == 0x1000)
			{	// SC RAM access
				if (addr & 0x0080)
				{	// a read from cartridge ram
					DATA_OUT = ((uint16_t)cart_ram[addr&0x7F])<<8;
					SET_DATA_MODE_OUT
					// wait for address bus to change
					while (ADDR_IN == addr) ;
					SET_DATA_MODE_IN
				}
				else
				{	// a write to cartridge ram
					// read last data on the bus before the address lines change
					while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
					cart_ram[addr&0x7F] = data_prev>>8;
				}
			}
			else
			{	// normal rom access
				DATA_OUT = ((uint16_t)bankPtr[addr&0xFFF])<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
		}
	}
	__enable_irq();
}

/* FA (CBS RAM plus) Bankswitching
 * -------------------------------
 * Similar to the above, but with 3 ROM banks for a total of 12K
 * plus 256 bytes of RAM:
 * RAM read port is $1100 - $11FF, write port is $1000 - $10FF.
 */
void emulate_FA_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (addr >= 0x1FF8 && addr <= 0x1FFA)	// bank-switch
				bankPtr = &cart_rom[(addr-0x1FF8)*4*1024];

			if ((addr & 0x1F00) == 0x1100)
			{	// a read from cartridge ram
				DATA_OUT = ((uint16_t)cart_ram[addr&0xFF])<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
			else if ((addr & 0x1F00) == 0x1000)
			{	// a write to cartridge ram
				// read last data on the bus before the address lines change
				while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
				cart_ram[addr&0xFF] = data_prev>>8;
			}
			else
			{	// normal rom access
				DATA_OUT = ((uint16_t)bankPtr[addr&0xFFF])<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
		}
	}
	__enable_irq();
}

/* FE Bankswitching
 * ----------------
 * The text below is quoted verbatim from the source code of the Atari
 * 2600 emulator Stella (https://github.com/stella-emu) which was the
 * best reference that I could find for FE bank-switching.
 * The implementation below is based on this description, and the relevant
 * source files in Stella.
 */

/*
  Bankswitching method used by Activision's Robot Tank and Decathlon.

  This scheme was originally designed to have up to 8 4K banks, and is
  triggered by monitoring the address bus for address $01FE.  All released
  carts had only two banks, and this implementation assumes that (ie, ROM
  is always 8K, and there are two 4K banks).

  The following is paraphrased from the original patent by David Crane,
  European Patent Application # 84300730.3, dated 06.02.84:
  ---------------------------------------------------------------------------
  The twelve line address bus is connected to a plurality of 4K by eight bit
  memories.

  The eight line data bus is connected to each of the banks of memory, also.
  An address comparator is connected to the bus for detecting the presence of
  the 01FE address.  Actually, the comparator will detect only the lowest 12
  bits of 1FE, because of the twelve bit limitation of the address bus.  Upon
  detection of the 01FE address, a one cycle delay is activated which then
  actuates latch connected to the data bus.  The three most significant bits
  on the data bus are latched and provide the address bits A13, A14, and A15
  which are then applied to a 3 to 8 de-multiplexer.  The 3 bits A13-A15
  define a code for selecting one of the eight banks of memory which is used
  to enable one of the banks of memory by applying a control signal to the
  enable, EN, terminal thereof.  Accordingly, memory bank selection is
  accomplished from address codes on the data bus following a particular
  program instruction, such as a jump to subroutine.
  ---------------------------------------------------------------------------

  Note that in the general scheme, we use D7, D6 and D5 for the bank number
  (3 bits, so 8 possible banks).  However, the scheme as used historically
  by Activision only uses two banks.  Furthermore, the two banks it uses
  are actually indicated by binary 110 and 111, and translated as follows:

    binary 110 -> decimal 6 -> Upper 4K ROM (bank 1) @ $D000 - $DFFF
    binary 111 -> decimal 7 -> Lower 4K ROM (bank 0) @ $F000 - $FFFF

  Since the actual bank numbers (0 and 1) do not map directly to their
  respective bitstrings (7 and 6), we simply test for D5 being 0 or 1.
  This is the significance of the test '(value & 0x20) ? 0 : 1' in the code.

  NOTE: Consult the patent application for more specific information, in
        particular *why* the address $01FE will be placed on the address
        bus after both the JSR and RTS opcodes.

  @author  Stephen Anthony; with ideas/research from Christian Speckner and
           alex_79 and TomSon (of AtariAge)
*/
void emulate_FE_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];
	int lastAccessWasFE = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (!(addr & 0x1000))
		{	// A12 low, read last data on the bus before the address lines change
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;
		}
		else
		{ // A12 high
			data = bankPtr[addr&0xFFF];
			DATA_OUT = data<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
		// end of cycle
		if (lastAccessWasFE)
		{	// bank-switch - check the 5th bit of the data bus
			if (data & 0x20)
				bankPtr = &cart_rom[0];
			else
				bankPtr = &cart_rom[4 * 1024];
		}
		lastAccessWasFE = (addr == 0x01FE);
	}
	__enable_irq();
}

/* 3F (Tigervision) Bankswitching
 * ------------------------------
 * Generally 8K ROMs, containing 4 x 2K banks. The last bank is always mapped into
 * the upper part of the 4K cartridge ROM space. The bank mapped into the lower part
 * of the 4K cartridge ROM space is selected by the lowest two bits written to $003F
 * (or any lower address).
 * In theory this scheme supports up to 512k ROMs if we use all the bits written to
 * $003F - the code below should support up to MAX_CART_ROM_SIZE.
 *
 * Note - Stella restricts bank switching to only *WRITES* to $0000-$003f. But we
 * can't do this here and Miner 2049'er crashes (unless we restrict to $003f only).
 *
 * From an post by Eckhard Stolberg, it seems the switch would happen on a real cart
 * only when the access is followed by an access to an address between $1000 and $1FFF.
 * This check is implemented below, and seems to work.

 * Refs:
 * http://atariage.com/forums/topic/266245-tigervision-banking-and-low-memory-reads/
 * http://atariage.com/forums/topic/68544-3f-bankswitching/
 */
void emulate_3F_cartridge(int cart_size_bytes)
{
	__disable_irq();	// Disable interrupts
	int cartPages = cart_size_bytes/2048;
	int newPage = -1;

	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];
	unsigned char *fixedPtr = &cart_rom[(cartPages-1)*2048];

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (!(addr & 0x1000))
		{	// A12 low, read last data on the bus before the address lines change
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;
			if (addr <= 0x003F) newPage = data % cartPages; else newPage = -1;
		}
		else
		{ // A12 high
			if (newPage >=0) {
				bankPtr = &cart_rom[newPage*2048];	// switch bank
				newPage = -1;
			}
			if (addr & 0x800)
				data = fixedPtr[addr&0x7FF];
			else
				data = bankPtr[addr&0x7FF];
			DATA_OUT = data<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}

/* 3E (3F + RAM) Bankswitching
 * ------------------------------
 * This scheme supports up to 512k ROM and 256K RAM.
 * However here we only support up to MAX_CART_ROM_SIZE and MAX_CART_RAM_SIZE
 *
 * The text below is the best description of the mapping scheme I could find,
 * quoted from http://blog.kevtris.org/blogfiles/Atari%202600%20Mappers.txt
 */

/*
This works similar to 3F (Tigervision) above, except RAM has been added.  The range of
addresses has been restricted, too.  Only 3E and 3F can be written to now.

1000-17FF - this bank is selectable
1800-1FFF - this bank is the last 2K of the ROM

To select a particular 2K ROM bank, its number is poked into address 3F.  Because there's
8 bits, there's enough for 256 2K banks, or a maximum of 512K of ROM.

Writing to 3E, however, is what's new.  Writing here selects a 1K RAM bank into
1000-17FF.  The example (Boulderdash) uses 16K of RAM, however there's theoretically
enough space for 256K of RAM.  When RAM is selected, 1000-13FF is the read port while
1400-17FF is the write port.
*/
void emulate_3E_cartridge(int cart_size_bytes)
{
	__disable_irq();	// Disable interrupts
	int cartROMPages = cart_size_bytes/2048;
	int cartRAMPages = MAX_CART_RAM_SIZE/1024;

	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];
	unsigned char *fixedPtr = &cart_rom[(cartROMPages-1)*2048];
	int bankIsRAM = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (bankIsRAM && (addr & 0xC00) == 0x400)
			{	// we are accessing the RAM write addresses ($1400-$17FF)
				// read last data on the bus before the address lines change
				while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
				bankPtr[addr&0x3FF] = data_prev>>8;
			}
			else
			{	// reads to either ROM or RAM
				if (addr & 0x800)
				{	// upper 2k ($1800-$1FFF)
					data = fixedPtr[addr&0x7FF];	// upper 2k -> read fixed ROM bank
				}
				else
				{	// lower 2k ($1000-$17FF)
					if (!bankIsRAM)
						data = bankPtr[addr&0x7FF];	// read switching ROM bank
					else
						data = bankPtr[addr&0x3FF];	// must be RAM read
				}
				DATA_OUT = data<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
		}
		else
		{	// A12 low, read last data on the bus before the address lines change
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
			data = data_prev>>8;
			if (addr == 0x003F) {
				bankIsRAM = 0;
				bankPtr = &cart_rom[(data%cartROMPages)*2048];	// switch in ROM bank
			}
			else if (addr == 0x003E) {
				bankIsRAM = 1;
				bankPtr = &cart_ram[(data%cartRAMPages)*1024];	// switch in RAM bank
			}
		}
	}
	__enable_irq();
}

/* E0 Bankswitching
 * ------------------------------
 * The text below is the best description of the mapping scheme I could find,
 * quoted from http://blog.kevtris.org/blogfiles/Atari%202600%20Mappers.txt
 */

/*
Parker Brothers used this, and it was used on one other game (Tooth Protectors).  It
uses 8K of ROM and can map 1K sections of it.

This mapper has 4 1K banks of ROM in the address space.  The address space is broken up
into the following locations:

1000-13FF : To select a 1K ROM bank here, access 1FE0-1FE7 (1FE0 = select first 1K, etc)
1400-17FF : To select a 1K ROM bank, access 1FE8-1FEF
1800-1BFF : To select a 1K ROM bank, access 1FF0-1FF7
1C00-1FFF : This is fixed to the last 1K ROM bank of the 8K

Like F8, F6, etc. accessing one of the locations indicated will perform the switch.
*/
void emulate_E0_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;
	unsigned char curBanks[4] = {0,0,0,7};

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high

			if (addr >= 0x1FE0 && addr <= 0x1FF7)
			{	// bank-switching addresses
				if (addr <= 0x1FE7)	// switch 1st bank
					curBanks[0] = addr-0x1FE0;
				else if (addr >= 0x1FE8 && addr <= 0x1FEF)	// switch 2nd bank
					curBanks[1] = addr-0x1FE8;
				else if (addr >= 0x1FF0)	// switch 3rd bank
					curBanks[2] = addr-0x1FF0;
			}
			// fetch data from the correct bank
			int target = (addr & 0xC00) >> 10;
			DATA_OUT = ((uint16_t)cart_rom[curBanks[target]*1024 + (addr&0x3FF)])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();

}

/* 0840 Bankswitching
 * ------------------------------
 * 8k cartridge with two 4k banks.
 * The following description was derived from:
 * http://blog.kevtris.org/blogfiles/Atari%202600%20Mappers.txt
 *
 * Bankswitch triggered by access to an address matching the pattern below:
 *
 * A12           A0
 * ----------------
 * 0 1xxx xBxx xxxx (x = don't care, B is the bank we select)
 *
 * If address AND $1840 == $0800, then we select bank 0
 * If address AND $1840 == $0840, then we select bank 1
 */
void emulate_0840_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if ((addr & 0x1840) == 0x0800) bankPtr = &cart_rom[0];
		else if ((addr & 0x1840) == 0x0840) bankPtr = &cart_rom[4*1024];
		else if (addr & 0x1000)
		{ // A12 high
			DATA_OUT = ((uint16_t)bankPtr[addr&0xFFF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}

/* CommaVid Cartridge
 * ------------------------------
 * 2K ROM + 1K RAM
 *  $F000-$F3FF 1K RAM read
 *  $F400-$F7FF 1K RAM write
 *  $F800-$FFFF 2K ROM
 */
void emulate_CV_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (addr & 0x0800)
			{	// ROM read
				DATA_OUT = ((uint16_t)cart_rom[addr&0x7FF])<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
			else
			{	// RAM access
				if (addr & 0x0400)
				{	// a write to cartridge ram
					// read last data on the bus before the address lines change
					while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
					cart_ram[addr&0x3FF] = data_prev>>8;
				}
				else
				{	// a read from cartridge ram
					DATA_OUT = ((uint16_t)cart_ram[addr&0x3FF])<<8;
					SET_DATA_MODE_OUT
					// wait for address bus to change
					while (ADDR_IN == addr) ;
					SET_DATA_MODE_IN
				}
			}
		}
	}
	__enable_irq();
}

/* F0 Bankswitching
 * ------------------------------
 * 64K cartridge with 16 x 4K banks. An access to $1FF0 switches to the next
 * bank in sequence.
 */
void emulate_F0_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0;
	int currentBank = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (addr == 0x1FF0)
				currentBank = (currentBank + 1) % 16;
			// ROM access
			DATA_OUT = ((uint16_t)cart_rom[(currentBank * 4096)+(addr&0xFFF)])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
	}
	__enable_irq();
}

/* E7 Bankswitching
 * ------------------------------
 * 16K cartridge with additional RAM.
 * The text below is the best description of the mapping scheme I could find,
 * quoted from http://blog.kevtris.org/blogfiles/Atari%202600%20Mappers.txt
 */

/*
M-network wanted something of their own too, so they came up with what they called
"Big Game" (this was printed on the prototype ASICs on the prototype carts).  It
can handle up to 16K of ROM and 2K of RAM.

1000-17FF is selectable
1800-19FF is RAM
1A00-1FFF is fixed to the last 1.5K of ROM

Accessing 1FE0 through 1FE6 selects bank 0 through bank 6 of the ROM into 1000-17FF.
Accessing 1FE7 enables 1K of the 2K RAM, instead.

When the RAM is enabled, this 1K appears at 1000-17FF.  1000-13FF is the write port, 1400-17FF
is the read port.

1800-19FF also holds RAM. 1800-18FF is the write port, 1900-19FF is the read port.
Only 256 bytes of RAM is accessable at time, but there are four different 256 byte
banks making a total of 1K accessable here.

Accessing 1FE8 through 1FEB select which 256 byte bank shows up.
 */
void emulate_E7_cartridge()
{
	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0];
	unsigned char *fixedPtr = &cart_rom[(8-1)*2048];
	unsigned char *ram1Ptr = &cart_ram[0];
	unsigned char *ram2Ptr = &cart_ram[1024];
	int ram_mode = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			if (addr & 0x0800)
			{	// higher 2k cartridge ROM area
				if ((addr & 0x0E00) == 0x0800)
				{	// 256 byte RAM access
					if (addr & 0x0100)
					{	// 1900-19FF is the read port
						DATA_OUT = ((uint16_t)ram1Ptr[addr&0xFF])<<8;
						SET_DATA_MODE_OUT
						// wait for address bus to change
						while (ADDR_IN == addr) ;
						SET_DATA_MODE_IN
					}
					else
					{	// 1800-18FF is the write port
						while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
						ram1Ptr[addr&0xFF] = data_prev>>8;
					}
				}
				else
				{	// fixed ROM bank access
					// check bankswitching addresses
					if (addr >= 0x1FE0 && addr <= 0x1FE7)
					{
						if (addr == 0x1FE7) ram_mode = 1;
						else
						{
							bankPtr = &cart_rom[(addr - 0x1FE0)*2048];
							ram_mode = 0;
						}
					}
					else if (addr >= 0x1FE8 && addr <= 0x1FEB)
						ram1Ptr = &cart_ram[(addr - 0x1FE8)*256];

					DATA_OUT = ((uint16_t)fixedPtr[addr&0x7FF])<<8;
					SET_DATA_MODE_OUT
					// wait for address bus to change
					while (ADDR_IN == addr) ;
					SET_DATA_MODE_IN
				}
			}
			else
			{	// lower 2k cartridge ROM area
				if (ram_mode)
				{	// 1K RAM access
					if (addr & 0x400)
					{	// 1400-17FF is the read port
						DATA_OUT = ((uint16_t)ram2Ptr[addr&0x3FF])<<8;
						SET_DATA_MODE_OUT
						// wait for address bus to change
						while (ADDR_IN == addr) ;
						SET_DATA_MODE_IN
					}
					else
					{	// 1000-13FF is the write port
						while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
						ram2Ptr[addr&0x3FF] = data_prev>>8;
					}
				}
				else
				{	// selected ROM bank access
					DATA_OUT = ((uint16_t)bankPtr[addr&0x7FF])<<8;
					SET_DATA_MODE_OUT
					// wait for address bus to change
					while (ADDR_IN == addr) ;
					SET_DATA_MODE_IN
				}
			}
		}
	}
	__enable_irq();
}

/*************************************************************************
 * Main loop/helper functions
 *************************************************************************/

void emulate_cartridge(int cart_type, int cart_size_bytes)
{
	if (cart_type == CART_TYPE_2K)
		emulate_2k_cartridge();
	else if (cart_type == CART_TYPE_4K)
		emulate_4k_cartridge();
	else if (cart_type == CART_TYPE_F8)
		emulate_FxSC_cartridge(0x1FF8, 0x1FF9, 0);
	else if (cart_type == CART_TYPE_F6)
		emulate_FxSC_cartridge(0x1FF6, 0x1FF9, 0);
	else if (cart_type == CART_TYPE_F4)
		emulate_FxSC_cartridge(0x1FF4, 0x1FFB, 0);
	else if (cart_type == CART_TYPE_F8SC)
		emulate_FxSC_cartridge(0x1FF8, 0x1FF9, 1);
	else if (cart_type == CART_TYPE_F6SC)
		emulate_FxSC_cartridge(0x1FF6, 0x1FF9, 1);
	else if (cart_type == CART_TYPE_F4SC)
		emulate_FxSC_cartridge(0x1FF4, 0x1FFB, 1);
	else if (cart_type == CART_TYPE_FE)
		emulate_FE_cartridge();
	else if (cart_type == CART_TYPE_3F)
		emulate_3F_cartridge(cart_size_bytes);
	else if (cart_type == CART_TYPE_3E)
		emulate_3E_cartridge(cart_size_bytes);
	else if (cart_type == CART_TYPE_E0)
		emulate_E0_cartridge();
	else if (cart_type == CART_TYPE_0840)
		emulate_0840_cartridge();
	else if (cart_type == CART_TYPE_CV)
		emulate_CV_cartridge();
	else if (cart_type == CART_TYPE_EF)
		emulate_FxSC_cartridge(0x1FE0, 0x1FEF, 0);
	else if (cart_type == CART_TYPE_EFSC)
		emulate_FxSC_cartridge(0x1FE0, 0x1FEF, 1);
	else if (cart_type == CART_TYPE_F0)
		emulate_F0_cartridge();
	else if (cart_type == CART_TYPE_FA)
		emulate_FA_cartridge();
	else if (cart_type == CART_TYPE_E7)
		emulate_E7_cartridge();
}

void convertFilenameForCart(unsigned char *dst, char *src)
{
	memset(dst, ' ', 12);
	for (int i=0; i<12; i++) {
		if (!src[i]) break;
		char c = toupper(src[i]);
		dst[i] = c;
	}
}

int readDirectoryForAtari(char *path)
{
	int ret = read_directory(path);
	// create a table of entries for the atari to read
	memset(menu_ram, 0, 1024);
	for (int i=0; i<num_dir_entries; i++)
	{
		unsigned char *dst = &menu_ram[i*12];
		convertFilenameForCart(dst, dir_entries[i].long_filename);
		// set the high-bit of the first character if directory
		if (dir_entries[i].isDir) *dst += 0x80;
	}
	return ret;
}

int main(void)
{
	char curPath[256] = "";
	int cart_type = CART_TYPE_NONE;
	int cart_size_bytes = 0;
	init();
	/* In/Out: PE{8..15} */
	config_gpio_data();
	/* In: PD{0..15} */
	config_gpio_addr();
	/* In: Other Cart Input Signals - PC{0} */
	config_gpio_sig();

	if (!(GPIOC->IDR & 0x0001))
		firmware_rom = firmware_pal60_rom;
	else if (!(GPIOC->IDR & 0x0002))
		firmware_rom = firmware_pal_rom;
	else
		firmware_rom = firmware_ntsc_rom;

	// set up status area
	strcpy(menu_status, "BY R.EDWARDS");
	menu_status[0xF] = 0;

	while (1) {
		int ret = emulate_firmware_cartridge();

		if (ret == CART_CMD_START_CART)
		{
			if (cart_type != CART_TYPE_NONE)
				emulate_cartridge(cart_type, cart_size_bytes);	// never returns
		}
		else if (ret == CART_CMD_ROOT_DIR)
		{
			curPath[0] = 0;
			if (!readDirectoryForAtari(curPath))
				strcpy(menu_status, "CANT READ SD");
		}
		else
		{
			int sel = ret - CART_CMD_SEL_ITEM_n;
			DIR_ENTRY *d = &dir_entries[sel];
			if (d->isDir && !strcmp(d->filename, ".."))
			{
				int len = strlen(curPath);
				while (len && curPath[--len] != '/');
				curPath[len] = 0;
			}
			else
			{
				strcat(curPath, "/");
				strcat(curPath, d->filename);
			}

			if (d->isDir)
			{	// selection is a directory
				if (!readDirectoryForAtari(curPath))
					strcpy(menu_status, "CANT READ SD");
				Delayms(200);
			}
			else
			{	// selection is a rom file
				cart_type = read_file(curPath, &cart_size_bytes);
				Delayms(200);
				if (cart_type != CART_TYPE_NONE)
					menu_status[0xF] = 1;	// tell the atari cartridge is loaded
				else
					strcpy(menu_status, "BAD ROM FILE");
			}
		}
	}
}
