USB-Blaster II firmware
=======================

Boards with an on-board Intel USB-Blaster II (for example Terasic
DE2-Standard, DE10-Nano, DE1-SoC) need Intel's firmware file
"blaster_6810.hex". openFPGALoader loads it into the blaster before use.

openFPGALoader GUI looks for it automatically in Quartus / Quartus
Programmer installations (C:\intelFPGA*\<version>, older Quartus II in
C:\altera\<version>, on any local drive). If it is not
found, either:
  - copy blaster_6810.hex into this folder (all users of this PC), or
  - click "USB Setup..." > "Locate..." in the application.

The file belongs to Intel and is not included in this package.
