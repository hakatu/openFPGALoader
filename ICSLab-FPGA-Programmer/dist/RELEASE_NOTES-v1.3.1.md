Windows release of **ICSLab FPGA Programmer**, a simple GUI for students to program FPGA boards.

**Download:** `ICSLab-FPGA-Programmer-v1.3.1-win64.zip` → extract → run `ICSLab FPGA Programmer.exe` (no installation, no admin rights).

### Highlights
- Board list from openFPGALoader (166 boards) with lab boards first: Basys 3, Nexys A7/Video, Arty, Zybo, Cmod A7, DE10-Lite, DE0-Nano, DE1, DE1-SoC, DE2 / DE2-70 / DE2-115 / DE2-Standard, DE10-Standard, Tang Nano 9K
- FPGA SRAM and Flash programming, Detect Board, progress bar, student-friendly error messages, full log
- Quartus `.sof` accepted: converted automatically (verified `.svf` for SRAM, `.rbf` for Flash) with any installed Quartus / Quartus II (`C:\intelFPGA*`, `C:\altera*`)
- **USB Setup...**: installs the Windows built-in WinUSB driver for Intel USB-Blaster / USB-Blaster II (fixes the yellow mark of Quartus drivers on Windows 11)

### Package
- Backend: openFPGALoader v1.1.1, static MinGW-w64 build (+1 small Windows path fix, see `patches/`)
- Only Windows system DLLs are used
- SHA-256 (zip): `b32178a9b1465505d17d77927a3b2b94e0aaca9df6e37e79545f6ee6b304a032`

ICSLab FPGA Programmer uses openFPGALoader as its FPGA programming backend. openFPGALoader is an independent open-source project: https://github.com/trabucayre/openFPGALoader (Apache-2.0). Third-party licenses: `README-licenses.txt` and `LICENSES/` in the zip.
