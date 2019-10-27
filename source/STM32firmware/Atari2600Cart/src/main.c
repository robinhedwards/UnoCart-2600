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

/* Version History
 * ---------------
 * v1.01 5/2/18 Fixed menu navigation bug after selecting a bad rom file
 * v1.02 8/2/18 Added partial DPC support - Pitfall II works
 * v1.03 19/3/18 Supercharger support thanks to Christian Speckner
 * v1.04 21/3/18 Bug fixes (file extension->cart type)
 * v1.05 29/3/18 Firmware compatible with the 7800 bios
 */

#define _GNU_SOURCE

#include "defines.h"
#include "stm32f4xx.h"
#include "tm_stm32f4_fatfs.h"
#include "tm_stm32f4_delay.h"

#include <ctype.h>

#include "cartridge_io.h"
#include "cartridge_firmware.h"
#include "cartridge_supercharger.h"
#include "cartridge_3f.h"
#include "cartridge_3e.h"
#include "cartridge_ace.h"

/*************************************************************************
 * Cartridge Definitions
 *************************************************************************/
#define MAX_CART_ROM_SIZE	64	// in kilobytes, historical to be removed
#define MAX_CART_RAM_SIZE	32	// in kilobytes, historical to be removed
#define BUFFER_SIZE			96  // kilobytes

uint8_t buffer[BUFFER_SIZE * 1024];

char cartridge_image_path[256];
unsigned int cart_size_bytes;
int tv_mode;

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
#define CART_TYPE_3EX   12
#define CART_TYPE_E0	13	// 8k
#define CART_TYPE_0840	14	// 8k
#define CART_TYPE_CV	15	// 2k+ram
#define CART_TYPE_EF	16	// 64k
#define CART_TYPE_EFSC	17	// 64k+ram
#define CART_TYPE_F0	18	// 64k
#define CART_TYPE_FA	19	// 12k
#define CART_TYPE_E7	20	// 16k+ram
#define CART_TYPE_DPC	21	// 8k+DPC(2k)
#define CART_TYPE_AR	22  // Arcadia Supercharger (variable size)
#define CART_TYPE_ACE	23  // ARM Custom Executable

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
	{"3EX", CART_TYPE_3E},
	{"E0", CART_TYPE_E0},
	{"084", CART_TYPE_0840},
	{"CV", CART_TYPE_CV},
	{"EF", CART_TYPE_EF},
	{"EFS", CART_TYPE_EFSC},
	{"F0", CART_TYPE_F0},
	{"FA", CART_TYPE_FA},
	{"E7", CART_TYPE_E7},
	{"DPC", CART_TYPE_DPC},
	{"AR", CART_TYPE_AR},
	{"ACE", CART_TYPE_ACE},
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
	else return strcasecmp(e1->long_filename, e2->long_filename);
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
		if (strcasecmp(ext, p->ext) == 0)
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

int identify_cartridge(char *filename)
{
	TM_DELAY_Init();
	FATFS FatFs;
	unsigned int image_size;
	int cart_type = CART_TYPE_NONE;
	FIL fil;

	if (f_mount(&FatFs, "", 1) != FR_OK) return CART_TYPE_NONE;
	if (f_open(&fil, filename, FA_READ) != FR_OK) goto unmount;

	// select type by file extension?
	char *ext = get_filename_ext(filename);
	EXT_TO_CART_TYPE_MAP *p = ext_to_cart_type_map;
	while (p->ext) {
		if (strcasecmp(ext, p->ext) == 0) {
			cart_type = p->cart_type;
			break;
		}
		p++;
	}

	image_size = f_size(&fil);

	// Supercharger cartridges get special treatment, since we don't load the entire
	// file into the buffer here
	if (cart_type == CART_TYPE_NONE && (image_size % 8448) == 0)
		cart_type = CART_TYPE_AR;
	if (cart_type == CART_TYPE_AR) goto close;

	// otherwise, read the file into the cartridge buffer
	unsigned int bytes_to_read = image_size > (BUFFER_SIZE * 1024) ? (BUFFER_SIZE * 1024) : image_size;
	UINT bytes_read;
	FRESULT read_result = f_read(&fil, buffer, bytes_to_read, &bytes_read);

	if (read_result != FR_OK || bytes_to_read != bytes_read) {
		cart_type = CART_TYPE_NONE;
		goto close;
	}

	if (cart_type != CART_TYPE_NONE) goto close;

	// If we don't already know the type (from the file extension), then we
	// auto-detect the cart type - largely follows code in Stella's CartDetector.cpp

	if (is_ace_cartridge(bytes_read, buffer))
	{
		cart_type = CART_TYPE_ACE;
	}
	else if (image_size == 2*1024)
	{
		if (isProbablyCV(bytes_read, buffer))
			cart_type = CART_TYPE_CV;
		else
			cart_type = CART_TYPE_2K;
	}
	else if (image_size == 4*1024)
	{
		cart_type = CART_TYPE_4K;
	}
	else if (image_size == 8*1024)
	{
		// First check for *potential* F8
		unsigned char  signature[] = { 0x8D, 0xF9, 0x1F };  // STA $1FF9
		int f8 = searchForBytes(buffer, bytes_read, signature, 3, 2);

		if (isProbablySC(bytes_read, buffer))
			cart_type = CART_TYPE_F8SC;
		else if (memcmp(buffer, buffer + 4096, 4096) == 0)
			cart_type = CART_TYPE_4K;
		else if (isProbablyE0(bytes_read, buffer))
			cart_type = CART_TYPE_E0;
		else if (isProbably3E(bytes_read, buffer))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, buffer))
			cart_type = CART_TYPE_3F;
		else if (isProbablyFE(bytes_read, buffer) && !f8)
			cart_type = CART_TYPE_FE;
		else if (isProbably0840(bytes_read, buffer))
			cart_type = CART_TYPE_0840;
		else
			cart_type = CART_TYPE_F8;
	}
	else if(image_size >= 10240 && image_size <= 10496)
	{  // ~10K - Pitfall II
		cart_type = CART_TYPE_DPC;
	}
	else if (image_size == 12*1024)
	{
		cart_type = CART_TYPE_FA;
	}
	else if (image_size == 16*1024)
	{
		if (isProbablySC(bytes_read, buffer))
			cart_type = CART_TYPE_F6SC;
		else if (isProbablyE7(bytes_read, buffer))
			cart_type = CART_TYPE_E7;
		else if (isProbably3E(bytes_read, buffer))
			cart_type = CART_TYPE_3E;
		else
			cart_type = CART_TYPE_F6;
	}
	else if (image_size == 32*1024)
	{
		if (isProbablySC(bytes_read, buffer))
			cart_type = CART_TYPE_F4SC;
		else if (isProbably3E(bytes_read, buffer))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, buffer))
			cart_type = CART_TYPE_3F;
		else
			cart_type = CART_TYPE_F4;
	}
	else if (image_size == 64*1024)
	{
		if (isProbably3E(bytes_read, buffer))
			cart_type = CART_TYPE_3E;
		else if (isProbably3F(bytes_read, buffer))
			cart_type = CART_TYPE_3F;
		else if (isProbablyEF(bytes_read, buffer))
		{
			if (isProbablySC(bytes_read, buffer))
				cart_type = CART_TYPE_EFSC;
			else
				cart_type = CART_TYPE_EF;
		}
		else
			cart_type = CART_TYPE_F0;
	}

	close:
		f_close(&fil);

	unmount:
		f_mount(0, "", 1);

	if (cart_type)
		cart_size_bytes = image_size;

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

/* Input Signals GPIO pins - PC0, PC1 (PC0=0 PAL60, PC1=0 PAL) */
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

/*************************************************************************
 * Cartridge Emulation
 *************************************************************************/

#define setup_cartridge_image() \
	if (cart_size_bytes > 0x010000) return; \
	uint8_t* cart_rom = buffer; \
	if (!reboot_into_cartridge()) return;

#define setup_cartridge_image_with_ram() \
	if (cart_size_bytes > 0x010000) return; \
	uint8_t* cart_rom = buffer; \
	uint8_t* cart_ram = buffer + cart_size_bytes + (((~cart_size_bytes & 0x03) + 1) & 0x03); \
	if (!reboot_into_cartridge()) return;

void emulate_2k_cartridge() {
	setup_cartridge_image();

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
	setup_cartridge_image();

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
	setup_cartridge_image_with_ram();

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
	setup_cartridge_image_with_ram();

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
	setup_cartridge_image();

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

/* Scheme as described by Eckhard Stolberg. Didn't work on my test 7800, so replaced
 * by the simpler 3F only scheme above.
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
 */

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
	setup_cartridge_image();

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
 * A12		   A0
 * ----------------
 * 0 1xxx xBxx xxxx (x = don't care, B is the bank we select)
 *
 * If address AND $1840 == $0800, then we select bank 0
 * If address AND $1840 == $0840, then we select bank 1
 */
void emulate_0840_cartridge()
{
	setup_cartridge_image();

	__disable_irq();	// Disable interrupts
	uint16_t addr, addr_prev = 0, addr_prev2 = 0;
	unsigned char *bankPtr = &cart_rom[0];

	while (1)
	{
		while (((addr = ADDR_IN) != addr_prev) || (addr != addr_prev2))
		{	// new more robust test for stable address (seems to be needed for 7800)
			addr_prev2 = addr_prev;
			addr_prev = addr;
		}
		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			DATA_OUT = ((uint16_t)bankPtr[addr&0xFFF])<<8;
			SET_DATA_MODE_OUT
			// wait for address bus to change
			while (ADDR_IN == addr) ;
			SET_DATA_MODE_IN
		}
		else
		{
			if ((addr & 0x0840) == 0x0800) bankPtr = &cart_rom[0];
			else if ((addr & 0x0840) == 0x0840) bankPtr = &cart_rom[4*1024];
			// wait for address bus to change
			while (ADDR_IN == addr) ;
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
	setup_cartridge_image_with_ram();

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
	setup_cartridge_image();

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
	setup_cartridge_image_with_ram();

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

/* DPC (Pitfall II) Bankswitching
 * ------------------------------
 * Bankswitching like F8(8k)
 * DPC implementation based on:
 * - Stella (https://github.com/stella-emu) - CartDPC.cxx
 * - Kevin Horton's 2600 Mappers (http://blog.kevtris.org/blogfiles/Atari 2600 Mappers.txt)
 *
 * Note this is not a full implementation of DPC, but is enough to run Pitfall II and the music sounds ok.
 */

void emulate_DPC_cartridge()
{
	setup_cartridge_image();

	SysTick_Config(SystemCoreClock / 21000);	// 21KHz
	__disable_irq();	// Disable interrupts

	unsigned char prevRom = 0, prevRom2 = 0;
	int soundAmplitudeIndex = 0;
	unsigned char soundAmplitudes[8] = {0x00, 0x04, 0x05, 0x09, 0x06, 0x0a, 0x0b, 0x0f};

	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	unsigned char *bankPtr = &cart_rom[0], *DpcDisplayPtr = &cart_rom[8*1024];

	unsigned char DpcTops[8], DpcBottoms[8], DpcFlags[8];
	uint16_t DpcCounters[8];
	int DpcMusicModes[3], DpcMusicFlags[3];

	// Initialise the DPC's random number generator register (must be non-zero)
	int DpcRandom = 1;

	// Initialise the DPC registers
	for(int i = 0; i < 8; ++i)
		DpcTops[i] = DpcBottoms[i] = DpcCounters[i] = DpcFlags[i] = 0;

	DpcMusicModes[0] = DpcMusicModes[1] = DpcMusicModes[2] = 0;
	DpcMusicFlags[0] = DpcMusicFlags[1] = DpcMusicFlags[2] = 0;


	uint32_t lastSysTick = SysTick->VAL;
	uint32_t DpcClocks = 0;

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;

		// got a stable address
		if (addr & 0x1000)
		{ // A12 high

			if (addr < 0x1040)
			{	// DPC read
				int index = addr & 0x07;
				int function = (addr >> 3) & 0x07;

				// Update flag register for selected data fetcher
				if((DpcCounters[index] & 0x00ff) == DpcTops[index])
					DpcFlags[index] = 0xff;
				else if((DpcCounters[index] & 0x00ff) == DpcBottoms[index])
					DpcFlags[index] = 0x00;

				unsigned char result = 0;
				switch (function)
				{
					case 0x00:
					{
						if(index < 4)
						{	// random number read
							DpcRandom ^= DpcRandom << 3;
							DpcRandom ^= DpcRandom >> 5;
							result = (unsigned char)DpcRandom;
						}
						else
						{	// sound
							soundAmplitudeIndex = (DpcMusicModes[0] & DpcMusicFlags[0]);
							soundAmplitudeIndex |=  (DpcMusicModes[1] & DpcMusicFlags[1]);
							soundAmplitudeIndex |=  (DpcMusicModes[2] & DpcMusicFlags[2]);
							result = soundAmplitudes[soundAmplitudeIndex];;
						}
						break;
					}

					case 0x01:
					{	// DFx display data read
						result = DpcDisplayPtr[2047 - DpcCounters[index]];
						break;
					}

					case 0x02:
					{	// DFx display data read AND'd w/flag
						result = DpcDisplayPtr[2047 - DpcCounters[index]] & DpcFlags[index];
						break;
					}

					case 0x07:
					{	// DFx flag
						result = DpcFlags[index];
						break;
					}
				}

				DATA_OUT = ((uint16_t)result)<<8;
				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN

				// Clock the selected data fetcher's counter if needed
				if ((index < 5) || ((index >= 5) && (!DpcMusicModes[index - 5])))
					DpcCounters[index] = (DpcCounters[index] - 1) & 0x07ff;
			}
			else if (addr < 0x1080)
			{	// DPC write
				int index = addr & 0x07;
				int function = (addr >> 3) & 0x07;

				while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
				unsigned char value = data_prev>>8;
				switch (function)
				{
					case 0x00:
					{	// DFx top count
						DpcTops[index] = value;
						DpcFlags[index] = 0x00;
						break;
					}

					case 0x01:
					{	// DFx bottom count
						DpcBottoms[index] = value;
						break;
					}

					case 0x02:
					{	// DFx counter low
						DpcCounters[index] = (DpcCounters[index] & 0x0700) | value;
						break;
					}

					case 0x03:
					{	// DFx counter high
						DpcCounters[index] = (((uint16_t)(value & 0x07)) << 8) | (DpcCounters[index] & 0xff);
						if(index >= 5)
							DpcMusicModes[index - 5] = (value & 0x10) ? 0x7 : 0;
						break;
					}

					case 0x06:
					{	// Random Number Generator Reset
						DpcRandom = 1;
						break;
					}
				}
			}
			else
			{	// check bank-switch
				if (addr == 0x1FF8)
					bankPtr = &cart_rom[0];
				else if (addr == 0x1FF9)
					bankPtr = &cart_rom[4*1024];

				// normal rom access
				DATA_OUT = ((uint16_t)bankPtr[addr&0xFFF])<<8;
				SET_DATA_MODE_OUT
				prevRom2 = prevRom;
				prevRom = bankPtr[addr&0xFFF];
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
		}
		else if((prevRom2 & 0xec) == 0x84) // Only do this when ZP write since there will be a full cycle available there
		{	// non cartridge access - e.g. sta wsync
			while (ADDR_IN == addr) {
				// should the DPC clock be incremented?
				uint32_t sysTick = SysTick->VAL;
				if (sysTick > lastSysTick)
				{	// the 21KHz clock has wrapped, so we increase the DPC clock
					DpcClocks++;
					// update the music flags here, since there isn't enough time when the music register
					// is being read.
					DpcMusicFlags[0] = (DpcClocks % (DpcTops[5] + 1))
							> DpcBottoms[5] ? 1 : 0;
					DpcMusicFlags[1] = (DpcClocks % (DpcTops[6] + 1))
							> DpcBottoms[6] ? 2 : 0;
					DpcMusicFlags[2] = (DpcClocks % (DpcTops[7] + 1))
							> DpcBottoms[7] ? 4 : 0;
				}
				lastSysTick = sysTick;
			}
		}
	}
	__enable_irq();
}

/*************************************************************************
 * Main loop/helper functions
 *************************************************************************/

void emulate_cartridge(int cart_type)
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
		emulate_3f_cartridge(cartridge_image_path, cart_size_bytes, buffer);
	else if (cart_type == CART_TYPE_3E)
		emulate_3e_cartridge(cartridge_image_path, cart_size_bytes, buffer, 32);
	else if (cart_type == CART_TYPE_3EX)
		emulate_3e_cartridge(cartridge_image_path, cart_size_bytes, buffer, BUFFER_SIZE);
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
	else if (cart_type == CART_TYPE_DPC)
		emulate_DPC_cartridge();
	else if (cart_type == CART_TYPE_AR) {
		emulate_supercharger_cartridge(cartridge_image_path, cart_size_bytes, buffer, tv_mode);
	}
	else if (cart_type == CART_TYPE_ACE)
	{
		if(launch_ace_cartridge(cartridge_image_path, BUFFER_SIZE * 1024, buffer))
			set_menu_status_msg("GOOD ACE ROM");
		else
			set_menu_status_msg("BAD ACE FILE");
	}
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
	uint8_t *menu_ram = get_menu_ram();
	// create a table of entries for the atari to read
	memset(menu_ram, 0, 1024);
	for (int i=0; i<num_dir_entries; i++)
	{
		unsigned char *dst = menu_ram + i*12;
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

	init();
	/* In/Out: PE{8..15} */
	config_gpio_data();
	/* In: PD{0..15} */
	config_gpio_addr();
	/* In: Other Cart Input Signals - PC{0..1} */
	config_gpio_sig();

	if (!(GPIOC->IDR & 0x0001))
		tv_mode = TV_MODE_PAL60;
	else if (!(GPIOC->IDR & 0x0002))
		tv_mode = TV_MODE_PAL;
	else
		tv_mode = TV_MODE_NTSC;

	set_tv_mode(tv_mode);

	// set up status area
	set_menu_status_msg("R.EDWARDS  2");
	set_menu_status_byte(0);

	while (1) {
		int ret = emulate_firmware_cartridge();

		if (ret == CART_CMD_ROOT_DIR)
		{
			curPath[0] = 0;
			if (!readDirectoryForAtari(curPath))
				set_menu_status_msg("CANT READ SD");
		}
		else
		{
			int sel = ret - CART_CMD_SEL_ITEM_n;
			DIR_ENTRY *d = &dir_entries[sel];

			if (d->isDir)
			{	// selection is a directory
				if (!strcmp(d->filename, ".."))
				{	// go back
					int len = strlen(curPath);
					while (len && curPath[--len] != '/');
					curPath[len] = 0;
				}
				else
				{	// go into director
					strcat(curPath, "/");
					strcat(curPath, d->filename);
				}

				if (!readDirectoryForAtari(curPath))
					set_menu_status_msg("CANT READ SD");
				Delayms(200);
			}
			else
			{	// selection is a rom file
				strcpy(cartridge_image_path, curPath);
				strcat(cartridge_image_path, "/");
				strcat(cartridge_image_path, d->filename);
				cart_type = identify_cartridge(cartridge_image_path);
				Delayms(200);
				if (cart_type != CART_TYPE_NONE)
					emulate_cartridge(cart_type);
				else
					set_menu_status_msg("BAD ROM FILE");
			}
		}
	}
}
