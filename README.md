UnoCart-2600
============
The UnoCart-2600 is an open source SD-card multicart for the Atari 2600. Use the joystick or the SELECT/RESET keys
to navigate the SD card and select a title to play.

SD card should be formatted as FAT or FAT32.

The UnoCart-2600 can emulate most banking schemes with ROM sizes up to 64k and RAM sizes up to 32k.
(more description to follow)

![Image](images/UnoCart2600Disco.jpg?raw=true)

Hardware
--------
The UnoCart-2600 is based on my earlier UnoCart project for the Atari 8-bit. It can be built using a STM32F407 DISCOVERY board
connected to a small PCB to breakout the Atari 2600 cartridge signals.

An article describing how to build an UnoCart for the Atari 8-bit was published in
[Excel Magazine](http://excel-retro-mag.co.uk) issue #4. You can also get a PDF of the article [here](https://github.com/robinhedwards/UnoCart/blob/master/UnoCart_EXCEL4.pdf).

Building the cartridge for the 2600 is almost identical, with the same connections between D0-D7 and A0-A12, +5V and GND.
All the other connections to the breakout PCB can be skipped, since these signals are not present on the 2600 cartridge slot.

Obviously, you'll need a breakout board designed for the 2600 cartridge slot rather than the Atari 8-bit. The design files for the breakout PCB are hosted here and can be used to make your own copy of the PCB.

Remember to program your DISCO board with the UnoCart-2600 firmware, rather than the UnoCart firmware.

(better building instructions to follow)

The UnoCart-2600 menu can be set to display in NTSC, PAL or PAL60 as follows:
* By default, the firmware will be NTSC.
* Connect PC0 -> GND for PAL60
* Connect PC1 -> GND for PAL

Note that this is for the menu only, and has no effect after you have selected a cartridge to play.

![Image](images/menuPAL.jpg?raw=true)

Custom PCB
----------
I have also designed a custom PCB for the UnoCart-2600 pictured here. This is mainly to make it easier to get people
to test the design. The PCB design files are not currently public.

![Front of PCB when inserted in Atari](images/test_board_front_small.jpg?raw=true)
![Back of PCB when inserted in Atari](images/test_board_back_small.jpg?raw=true)

Firmware
--------
The UnoCart-2600 firmware is open source under a GPL license and my original firmware hosted here.
However, there is a newer version - see below.

New firmware
------------
I haven't had any time to develop the firmware further, but DirtyHairy & ZackAttack on the AtariAge forums
have added some great stuff to the firmware, including updating from SD card, support for Pink Panther protoype
cartridge and much more to come.

The releases of this new firmware can be found on [DirtyHairy's branch](https://github.com/DirtyHairy/UnoCart-2600/releases)

You'll need an ST-Link programmer the first time you update to this branch of the firmware. Subsequent updates can
done simply from the SD card.

Credits
-------
* Design, hardware and firmware by Robin Edwards (electrotrains at atariage)
* Additional work on firmware by Christian Speckner (DirtyHairy at atariage)