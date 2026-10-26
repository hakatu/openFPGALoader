Unofficial Windows GUI for **openFPGALoader**, plus a ready-to-run static Windows build of openFPGALoader. Made for students programming FPGA boards in a lab.

**Download:** `openFPGALoader-GUI-v1.4.0-win64.zip` → extract → run `openFPGALoader GUI.exe` (no installation, no admin rights).

### Features
- Board list from openFPGALoader (166 boards); lab boards first: Basys 3, Nexys A7/Video, Arty, Zybo, Cmod A7, DE10-Lite, DE0-Nano, DE1, DE1-SoC, DE2 / DE2-70 / DE2-115 / DE2-Standard, DE10-Standard, Tang Nano 9K (editable `lab-boards.txt`)
- FPGA SRAM and Flash programming, Detect Board, progress bar, clear error messages, full log
- Quartus `.sof` accepted: converted automatically with any installed Quartus / Quartus II (`C:\intelFPGA*`, `C:\altera*`): verified `.svf` for SRAM, `.rbf` for Flash
- **USB Setup...**: binds the WinUSB driver built into Windows to Intel USB-Blaster / USB-Blaster II (fixes the yellow mark of Quartus drivers on Windows 11)

### Package
- Backend: openFPGALoader v1.1.1, static MinGW-w64 build (+1 small Windows path fix, see `openFPGALoader-GUI/patches/`)
- Only Windows system DLLs are used
- SHA-256 (zip): `7ef4179c8b80448bd19d95f1d7715a80ac96ad9c79810a1cc8431d8a18209a32`

openFPGALoader is an independent open-source project by Gwenhael Goavec-Merou and contributors (Apache-2.0): https://github.com/trabucayre/openFPGALoader. This GUI is unofficial. Third-party licenses: `README-licenses.txt` and `LICENSES/` in the zip.

<sub>created by haka @ ICSLab HCMUT</sub>
