// This file was copied from https://github.com/robinhedwards/UnoCart-2600

#ifndef CARTRIDGE_IO_H
#define CARTRIDGE_IO_H

#include "stm32f4xx.h"

#define ADDR_IN GPIOD->IDR
#define DATA_IN GPIOE->IDR
#define DATA_OUT GPIOE->ODR
#define CONTROL_IN GPIOC->IDR
#define SET_DATA_MODE_IN GPIOE->MODER = 0x00000000;
#define SET_DATA_MODE_OUT GPIOE->MODER = 0x55550000;

#endif // CARTRIDGE_IO_H
