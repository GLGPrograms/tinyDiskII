# tinyDiskII
Homemade floppy disk emulator for Apple II series computers

> :warning: **Work in progress**: read carefully the README before trying to reproduce this project!

![tinyDiskII](tinyDiskII.png)

tinyDiskII is able to simulate an [Apple Disk II](https://it.wikipedia.org/wiki/Disk_II) both in reading and writing.
Disk images are stored into a FAT16 formatted SD Card.
Each disk image must be a DOS 3.3 floppy image, saved as .NIC.
A cross platform .NIC <-> .DSK conversion tool will be available soon on this repository.
Floppy formatting is currently not supported, but it is currently under developement.

## Contributors and references

The whole projects borns as a custom implementation of Koichi Nishida's SdiskII [[1]](https://tulip-house.ddo.jp/digital/SDISK2/english.html) [[2]](https://github.com/suaide/SDisk2), which I used as a starting point for firmware developement.
