# tinyDiskII
Homemade floppy disk emulator for Apple II series computers

> :hammer: Developement branch

> :warning: **Work in progress**: read carefully the README before trying to reproduce this project!

![tinyDiskII](docs/tinyDiskII.png)

tinyDiskII is able to simulate an [Apple Disk II](https://it.wikipedia.org/wiki/Disk_II) both in reading and writing.
Disk images are stored into a FAT16 formatted SD Card.
Each disk image must be a DOS 3.3 floppy image, saved as .NIC.
A cross platform .NIC <-> .DSK conversion tool will be available soon on this repository.
Floppy formatting is currently not supported, but it is currently under developement.

## BOM

| Reference(s)                                  |        Description        |         Notes          |
| :-------------------------------------------- | :-----------------------: | :--------------------: |
| C1, C2, C3, C4, C5, C6, C8, C9, C10, C11, C12 |    0805 100n Capacitor    |                        |
| D1, D2                                        |       0805 Red LED        |                        |
| J1                                            |      2x3 male jumper      |     ICSP connector     |
| J2                                            |     SMD SD Card slot      |                        |
| J3                                            | THT USB B Mini connector  |  Mount for USB debug   |
| J4                                            |    2x10 male connector    |    DiskII connector    |
| R5, R6                                        |            330            |                        |
| R2, R3, R4, R7, R8, R9, R10, R11, R12, R15    |            10k            |                        |
| R13, R14                                      |             0             | Replaced with a bridge |
| SW1                                           | RotaryEncoder with switch |                        |
| U2                                            |        SN74LVC244         |   5V tolerant buffer   |
| U3                                            |          CH340C           |  Mount for USB debug   |
| U4                                            |      ATxmega16A4U-A       |          MCU           |
| U5                                            |         74AHCT125         |     Level shifter      |
| U7                                            |      0.96" OLED I2C       |      7-pin model       |
| U10                                           |         AP1117-33         |  3v3 linear regulator  |


### Do Not Place, reserved for future implementations:

| Reference(s) |        Feature         |
| :----------- | :--------------------: |
| U1           | Flash/EEPROM interface |
| C12          |           "            |
| R1           |           "            |
| C13, C14     |        USB ESD         |
| R13, R14     |           "            |
| D3, D4       |           "            |
| C7           |     USB self reset     |
| J5           |  Debugging connector   |

## Schematics and PCB

> :warning: Schematics and PCB layout can be found in `/schematics` folder. Please note that I had physically implemented only rev1.0, which requires some hardware patches. The issues were fixed in rev1.1, but I never put it into production. Build it at your own risk.

## Firmware

Firmware source code is in `/firmware` folder.
Code can be compiled on a linux/wsl machine through `make` command.
Make sure you have `make` and `avr-gcc` toolchanin installed.
Output binary will be generated as `/firmware/output/tinyDiskII.hex`.

In `/firmware/tests` there are some unit tests that can run on host system.
They can be compiled and executed with:

```
cd firmware/tests
cmake -B build .
cd build
cmake --build .
./tinyDiskII-tests
```

## Changelog

* rev1.0: first version
* rev1.1: fixes the issues present in rev1.0:

    - LV_DISK_READ connected to U4 pin 21;
    - SD_WP connected to U4 pin 22;
    - SD_DETECT connected to U4 pin 23;
    - Added 10K pull-up resistor to DISK_WRITE_EN;
    - Pin 4 and 16 of U3 are shorted together.


## Contributors and references

The whole projects borns as a custom implementation of Koichi Nishida's SdiskII [[1]](https://tulip-house.ddo.jp/digital/SDISK2/english.html) [[2]](https://github.com/suaide/SDisk2), which I used as a starting point for firmware developement.
