Source code for the UnoCart2600 firmware.

# Building with Atollic Truestudio

Atollic Truestudio 6 project.
After building, firmware will be in the Debug subdirectory.

The various 2600 firmware ROMs (PAL/NTSC etc) are in include files. These can be rebuilt from the source code in the /source/Atari2600ROM/ directory. The convert.bat batch file will turn the binary files into header files. Add the "const" keyword so these will be stored in STM32F4 flash rather than RAM.

# Building from CLI

You can build and flash the firmware with the provided `Makefile`using the official
[ARM Cortex GNU toolchain](http://developer.arm.com/open-source/gnu-toolchain/gnu-rm)
and the open source [stlink tool suite](https://github.com/texane/stlink).

```
    $ make           # Build firware in build directory
    $ make flash     # Build and flash firmware
```

