UnoCart-2600
============
The UnoCart-2600 is an open source SD-card multicart for the Atari 2600.
(more description to follow)

![Image](images/UnoCart2600Disco.jpg?raw=true)

Hardware
--------
The UnoCart-2600 is based on my earlier UnoCart project for the Atari 8-bit. It can be built using a STM32F407 DISCOVERY board
connected to a small PCB to breakout the Atari 2600 cartridge signals.

The design files for the breakout PCB are hosted here and can be used to make your own copy of the PCB.

An article describing how to build an UnoCart for the Atari 8-bit was published in
[Excel Magazine](http://excel-retro-mag.co.uk) issue #4. You can also get a PDF of the article [here](https://github.com/robinhedwards/UnoCart/blob/master/UnoCart_EXCEL4.pdf).

Building the cartridge for the 2600 is almost identical - but requires fewer connections, since the 2600 cartridge slot has
less signals than the Atari 8-bit. Remember to program your DISCO board with the UnoCart-2600 firmware, rather than the UnoCart firmware.
(better building instructions to follow)

![Image](images/menuPAL.jpg?raw=true)

Credits
-------
* Design, hardware and firmware by Robin Edwards (electrotrains at atariage)
