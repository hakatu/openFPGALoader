# ICSLab FPGA Programmer: student guide

## Program your FPGA in four steps

1. **Connect** the board to the PC with the USB cable (use the board's
   *PROG* / *JTAG* USB port) and **switch the board on**.
2. Open **ICSLab FPGA Programmer.exe**.
3. **Board:** choose your board (the lab boards are at the top of the list).
   **Bitstream:** click **Browse…** and pick your `.bit` file, or drag the
   file onto the window.
4. Click **PROGRAM FPGA** and wait for
   **"✓ FPGA programmed successfully."**

The *Status* box always shows what will happen before you click:

```text
Board:   Digilent Basys 3 [basys3]
File:    lab3.bit
Target:  FPGA SRAM (temporary)
```

## FPGA SRAM or Flash?

| Mode | What it does | When to use it |
|---|---|---|
| **FPGA SRAM** (default) | Loads the design directly into the FPGA. Fast. Lost when the board is switched off. | Every lab exercise. |
| **Flash** | Writes the design into the configuration flash of the board. The FPGA loads it at every power-on. Slower; asks for confirmation because it erases the previous flash content. | Final projects / demos only. |

Your FPGA must already be working in SRAM mode before you try Flash mode.

## Which file do I select?

* **Vivado** (Xilinx/AMD): `<project>.runs/impl_1/<top>.bit`
* **Quartus** (Intel, e.g. DE2 boards): `.sof` files are not supported.
  In Quartus use *File > Convert Programming Files* to create a `.rbf`
  (or export a `.svf`), as your instructor indicates.
* **Gowin**: `impl/pnr/<project>.fs`

## Detect Board

**Detect Board** checks the connection without programming anything. It
shows the USB programmer, the FPGA found on the JTAG chain and its IDCODE.
If you don't know which board you have, select
*Auto Detect / Generic JTAG*, click **Detect Board** and look at
*Possible board*.

## Something went wrong?

The Status box turns red and explains the most likely cause with a short
checklist. Click **Show Details** to see the full output of the programming
tool. Copy it into your message when you ask for help.

Common messages:

* **Unable to communicate with the FPGA programmer.** Check the USB cable,
  the power switch and the selected board.
* **The programmer was found but Windows blocks access to it.** The USB
  driver needs to be changed. Ask your instructor (see `windows-driver.md`).
* **The programmer works, but no FPGA answered.** Check the board power and
  jumpers, and the selected board.
* **The bitstream file is not valid.** You selected the wrong file, or
  bitstream generation failed.

The application remembers your board, mode and folder, but it **never**
programs anything until you click **PROGRAM FPGA**.
