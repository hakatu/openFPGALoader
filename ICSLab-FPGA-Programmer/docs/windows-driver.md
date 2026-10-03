# Windows USB drivers for ICSLab FPGA Programmer

**Audience:** lab instructors and IT staff. Students should not change drivers
on their own.

## Why a driver change can be needed

`openFPGALoader.exe` talks to USB programmers through **libusb** (directly, or
through **libftdi** for FTDI-based programmers). On Windows, libusb can only
open a USB interface that uses one of these drivers:

* **WinUSB** (built into Windows, recommended)
* libusbK
* libusb0 (libusb-win32)

Most lab boards are delivered with vendor drivers instead (FTDI VCP/D2XX
installed by Vivado/Digilent Adept, or the Intel USB-Blaster driver installed
by Quartus). libusb can see those devices but **cannot open them**. In that
case ICSLab FPGA Programmer shows:

> The programmer was found but Windows blocks access to it.

and the detailed log contains messages such as
`unable to claim usb device. Make sure the default FTDI driver is not in use`
or `LIBUSB_ERROR_NOT_SUPPORTED`.

The application itself never installs or replaces drivers and never needs
administrator rights. Only the one-time driver change described below needs
administrator rights.

> **Trade-off:** after an interface is switched to WinUSB, the vendor tool that
> used the vendor driver on that same interface (Vivado Hardware Manager /
> Digilent Adept, Quartus Programmer, Gowin Programmer) can no longer use it
> on that PC. You can switch back at any time, see
> [Restoring the original driver](#restoring-the-original-driver).
> On PCs that must keep using Vivado/Quartus for programming, don't change the
> driver.

## Which interface to change

Only change the **JTAG interface** of the programmer. Never change the UART
(COM port) interface: students need it for serial terminals.

| Board (openFPGALoader name) | Programmer (USB ID) | Zadig entry to change | Driver | Do NOT change |
|---|---|---|---|---|
| Basys 3 (`basys3`), Nexys A7 (`nexys_a7_50/100`), Arty A7/S7 (`arty_*`), Cmod A7/S7 (`cmoda7_*`, `cmod_s7`), Zybo Z7 (`zybo_z7_*`) | FTDI FT2232 (`0403:6010`) | **"… (Interface 0)"**, e.g. "Digilent USB Device (Interface 0)" | WinUSB | "… (Interface 1)" = USB serial port |
| Nexys Video (`nexysVideo`, cable `digilent_b`) | FTDI FT2232 (`0403:6010`) | **"… (Interface 1)"**. This board uses channel B for JTAG | WinUSB | "… (Interface 0)" |
| Generic FT2232 boards (cable `ft2232`), e.g. Sipeed Tang Nano 9K (`tangnano9k`) | `0403:6010` | **"… (Interface 0)"**, e.g. "JTAG Debugger (Interface 0)" | WinUSB | "… (Interface 1)" = USB serial port |
| Terasic DE10-Lite (`de10lite`), DE0-Nano (`de0nano`) | USB-Blaster (`09FB:6001`) | **"USB-Blaster"** (single interface) | WinUSB | n/a |
| USB-Blaster II boards (`de10nano`, `de1Soc`, …) | `09FB:6010` (before firmware) / `09FB:6810` | **"USB-Blaster II"** (both IDs) | WinUSB | n/a |

USB-Blaster II also needs Intel's firmware file (`blaster_6810.hex` from a
Quartus installation); ICSLab FPGA Programmer passes it automatically
(`--probe-firmware`), see the USB Setup section above. For Intel blasters,
prefer the built-in **USB Setup...** over Zadig.

How to tell what the programmer is: on the affected PC, open ICSLab FPGA
Programmer, click **Detect Board**, then **Show Details**. The
`--scan-usb` output lists the USB ID (`vid:pid`) of every programmer
openFPGALoader can see.

Check the USB ID in Zadig before replacing anything. If the ID doesn't
match the table, stop.

## Intel USB-Blaster / USB-Blaster II (DE2, DE10, DE0 ...): built-in "USB Setup"

On Windows 11, the USB-Blaster drivers installed by Quartus often show a
yellow exclamation mark in Device Manager even after installation (the
driver is blocked or rejected by Windows). ICSLab FPGA Programmer does not
use Intel's driver: it needs **WinUSB**, which is part of Windows and signed
by Microsoft, so it is not affected by that problem.

1. Connect the board (USB-Blaster port) and switch it on.
2. Open ICSLab FPGA Programmer and click **USB Setup...** (bottom right).
   The list shows each blaster, its USB ID and the Windows driver status
   (including the Device Manager error code, e.g. *code 52: Windows cannot
   verify the driver's digital signature*).
3. **USB-Blaster II only:** the *firmware* field must show
   `blaster_6810.hex`. It is found automatically in Quartus / Quartus
   Programmer installations (`C:\intelFPGA*\<version>`, older Quartus II in
   `C:\altera\<version>`, on any local drive); otherwise click
   **Locate...** and select it. Lab tip: copy the file into the `firmware`
   folder next to `ICSLab FPGA Programmer.exe` so every user of the PC finds
   it. (Intel's file is not included in this package.)
4. Click **Install WinUSB driver** and accept the Windows administrator
   prompt. For a USB-Blaster II the application then loads the firmware once
   and configures the second USB ID (09FB:6810) as well.
5. Close the dialog and click **Detect Board**.

What it does: it selects the inbox driver *Universal Serial Bus devices >
WinUsb Device* (`%SystemRoot%\INF\winusb.inf`) for the blaster only, exactly
as you could do by hand in Device Manager. No driver package or certificate
is added to Windows.

**Restore original driver** uninstalls the WinUSB binding of the connected
blasters; Windows then re-installs its default (Intel) driver, for Quartus.

If the automatic setup reports an error, do the same by hand: Device Manager
> right-click the USB-Blaster > *Update driver* > *Browse my computer* >
*Let me pick from a list* > *Universal Serial Bus devices* > *WinUsb Device*
> *Next* (accept the warning). For a USB-Blaster II, repeat after the first
**Detect Board** for the second device that appears.

The driver choice is stored per USB device instance: plugging the same
board into another USB port may require running the setup again.

**Not yet validated on hardware:** the USB Setup function was developed
without access to a DE2 board or a Windows 11 PC. Try it on one lab PC
first; the manual procedure above is the fallback.

## Installing WinUSB with Zadig (administrator rights required)

Zadig is a free tool (GPLv3) from the libusb project authors. It is **not
bundled** with ICSLab FPGA Programmer: download the current version from the
official site, <https://zadig.akeo.ie/>.

1. Connect the board and switch it on.
2. Run `zadig-x.y.exe` (Windows asks for administrator approval).
3. Menu **Options → List All Devices**.
4. **Uncheck Options → Ignore Hubs or Composite Parents** if it is checked, so
   that you see the individual "(Interface 0)" / "(Interface 1)" entries.
5. In the drop-down list, select the **JTAG interface** from the table above,
   e.g. `Digilent USB Device (Interface 0)`.
6. Check the fields next to "USB ID": they must show the expected VID/PID
   (e.g. `0403 6010 00`, where the third number is the interface number).
7. In the target driver box (right of the green arrow) select **WinUSB**.
8. Click **Replace Driver** (or **Install Driver**) and wait for "successful".
9. Unplug and re-plug the board.
10. In ICSLab FPGA Programmer click **Detect Board**: the FPGA should now be
    listed.

Notes:

* The driver change is per PC **and** per USB device (it follows the serial
  number). Another board of the same model can need the same step.
* Never select a keyboard, mouse, USB hub, Bluetooth or "Composite Parent"
  entry.
* Windows Update can occasionally put the vendor driver back. Re-do the steps
  if "Windows blocks access" re-appears.

## Restoring the original driver

1. Connect the board.
2. Open **Device Manager** (`Win+X` → Device Manager).
3. Expand **Universal Serial Bus devices** (WinUSB devices appear there),
   right-click the interface you changed (e.g. "Digilent USB Device") →
   **Uninstall device**.
4. Tick **"Attempt to remove the driver for this device"** (Windows 10) /
   **"Delete the driver software for this device"** and confirm.
5. Unplug and re-plug the board. Windows re-installs the vendor driver
   (FTDI / Digilent / USB-Blaster) if it is installed on the PC (Vivado,
   Quartus or the vendor driver package). Otherwise install the vendor
   driver package again.
6. Optional cleanup of the driver package Zadig added (administrator command
   prompt): `pnputil /enum-drivers` → find the entry whose *Original Name*
   is the `.inf` Zadig created → `pnputil /delete-driver oemNN.inf`.

## Programmers that need no driver change

* Programmers that are already WinUSB-compatible (for example devices that
  ship with a WinUSB/MS OS descriptor) work without Zadig.
* CMSIS-DAP (HID) probes are **not supported** by this Windows build
  (the upstream Windows cross-build disables CMSIS-DAP because it needs
  hidapi).

## Tested status

The driver procedure above follows the documented behaviour of
libusb/libftdi on Windows and the cable definitions of openFPGALoader
(`src/cable.hpp`). It has **not** been validated with physical hardware as
part of this build. Validate it on one lab PC per board model before a lab
session.
