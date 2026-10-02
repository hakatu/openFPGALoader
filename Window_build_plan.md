You are an experienced C++/Windows desktop software engineer.

Your task is to create a simple Windows GUI application for students based on:

https://github.com/trabucayre/openFPGALoader

The final deliverable should be an easy-to-use Windows `.exe` application that students can open directly without needing to use the command line.

Application name:

**ICSLab FPGA Programmer**

Author / organization:

**ICSLab**

The purpose is to provide a simple graphical front-end for `openFPGALoader`.

---

# 1. Overall Goal

Take the upstream `openFPGALoader` project and produce a Windows application with this architecture:

```text
ICSLab FPGA Programmer.exe
        |
        +--> Simple Windows GUI
        |
        +--> openFPGALoader.exe
                |
                +--> USB/JTAG programmer
                |
                +--> FPGA board
```

Do NOT unnecessarily rewrite the programming logic of openFPGALoader.

Prefer keeping openFPGALoader as the backend and building a clean GUI wrapper around it.

The GUI should generate and execute appropriate `openFPGALoader` commands.

---

# 2. Target Users

The users are undergraduate electronics / FPGA students.

Assume they:

- know how to select an FPGA bitstream
- know which FPGA board they are using
- may NOT know command-line syntax
- should NOT need to install Python
- should NOT need to install MSYS2
- should NOT need to install CMake
- should NOT need to open a terminal

The application should therefore be as simple as possible.

---

# 3. Main GUI

Create a small, clean desktop window.

Suggested layout:

```text
-------------------------------------------------------
             ICSLab FPGA Programmer

 Board:
 [ Nexys Video                         v ]

 Cable:
 [ Auto Detect                         v ]

 Bitstream:
 [ C:\...\design.bit              ] [ Browse ]

 Programming Mode:
 (●) FPGA SRAM
 ( ) Flash

 [ Detect Board ]

 ------------------------------------------------------
 Status:
 Board detected: Digilent Nexys Video
 Cable: FT2232
 Device: XC7A200T

 [============= 65% ==================]

 Programming FPGA...

 ------------------------------------------------------

 [ PROGRAM FPGA ]

                ICSLab
-------------------------------------------------------
```

The interface should prioritize simplicity over advanced configuration.

---

# 4. Minimum Features

Implement the following.

## Bitstream selection

Provide a `Browse...` button using the normal Windows file-selection dialog.

Support common formats accepted by openFPGALoader, including at least:

- `.bit`
- `.bin`
- `.svf`
- `.jed`
- `.hex`

Do not artificially restrict formats if openFPGALoader supports additional ones.

---

## Board selection

Provide a drop-down menu for selecting a supported board.

Do NOT maintain a hardcoded list if openFPGALoader has a reliable way to expose its supported boards.

Prefer obtaining the board list from openFPGALoader itself or generating the list from the upstream source/configuration.

There should also be:

**Auto Detect / Generic JTAG**

where practical.

---

## Detect Hardware

Provide a button:

**Detect Board**

Use the appropriate openFPGALoader detection command.

Display the detected:

- USB programmer
- JTAG cable
- FPGA device
- IDCODE if available
- board if identifiable

Present useful information in the GUI status panel.

Do not expose raw command-line noise unless the user opens the detailed log.

---

# 5. Program FPGA

The main button should be:

**PROGRAM FPGA**

For example, if the user chooses a supported board and `design.bit`, internally the GUI may execute something equivalent to:

```text
openFPGALoader.exe -b <board> design.bit
```

Use the actual syntax supported by the current openFPGALoader version.

Do NOT blindly assume command-line arguments.

Inspect the current upstream source and documentation and use the correct command syntax.

---

# 6. Flash Programming

Provide two modes:

```text
FPGA SRAM
Flash
```

When `FPGA SRAM` is selected, program volatile FPGA configuration.

When `Flash` is selected, use the appropriate openFPGALoader flash programming option.

Hide advanced flash parameters unless needed.

If a selected board requires a particular option, automatically supply it where it can be determined safely.

---

# 7. Programming Log

Add a collapsible section:

**Show Details**

When expanded, display the output from openFPGALoader.

For example:

```text
Open file: design.bit
Parse bitstream...
JTAG frequency: 6.00 MHz
Device: XC7A200T
Programming...
100%
Done
```

Capture both stdout and stderr.

The GUI MUST remain responsive while programming.

Therefore run openFPGALoader asynchronously / in a worker thread or child process.

Do not block the UI thread.

---

# 8. Progress Bar

Provide a progress bar.

Where possible parse openFPGALoader output to determine programming progress.

Example:

```text
Programming

[################------] 72%
```

If reliable percentage parsing is not possible for a particular programmer, use an indeterminate progress indicator instead.

Do not fake a percentage.

---

# 9. Result Handling

On successful programming:

```text
✓ FPGA programmed successfully.
```

On failure:

```text
Programming failed.

Unable to communicate with the FPGA programmer.

Check:
• USB cable connection
• FPGA board power
• selected board
• USB driver

[ Show Details ]
```

Convert common openFPGALoader failures into student-friendly explanations.

Always retain the raw backend error in the detailed log.

---

# 10. Windows USB Driver Considerations

Investigate how openFPGALoader accesses each supported USB programmer under Windows.

Do not silently require a dangerous or inappropriate driver replacement.

Document when WinUSB/libusb or another driver is required.

If some programmers require a Windows driver setup, create documentation such as:

```text
docs/windows-driver.md
```

If Zadig is commonly necessary for a particular programmer, explain exactly:

- which USB interface should be changed
- which driver should be used
- which interfaces should NOT be changed
- how to restore the original driver

Do NOT bundle Zadig unless its distribution/license permits it and doing so is genuinely useful.

---

# 11. Technology Choice

Choose a GUI technology suitable for a lightweight Windows utility.

Preferred choices:

### Option A — C++ + Qt 6

Use Qt Widgets if deployment can remain reasonably small.

OR

### Option B — C++ + Win32

Use native Win32 if this produces a significantly smaller and more portable executable.

OR

### Option C — C#/.NET

Use a modern .NET Windows GUI only if a normal Windows 10/11 machine can run the distributed application without requiring students to manually install additional components.

If using .NET, publish it as self-contained.

The final result should behave like a normal Windows application.

Do NOT use Electron.

Do NOT require Python at runtime.

---

# 12. Backend Integration

Prefer the architecture:

```text
GUI process
   |
   +--> bundled backend/openFPGALoader.exe
```

rather than deeply modifying openFPGALoader.

This allows the backend to be updated independently.

Suggested layout:

```text
ICSLab-FPGA-Programmer/
│
├── src/
│   ├── main.cpp
│   ├── MainWindow.cpp
│   ├── MainWindow.h
│   ├── OpenFPGALoaderBackend.cpp
│   └── OpenFPGALoaderBackend.h
│
├── resources/
│   └── ...
│
├── backend/
│   └── openFPGALoader.exe
│
├── docs/
│   ├── student-guide.md
│   └── windows-driver.md
│
├── LICENSES/
│   └── ...
│
├── CMakeLists.txt
└── README.md
```

The GUI should locate `openFPGALoader.exe` relative to its own executable rather than relying on PATH.

---

# 13. Build openFPGALoader for Windows

Use the official upstream Windows build procedure whenever possible.

Inspect:

```text
doc/guide/install.rst
cmake/
CMakeLists.txt
```

The upstream project currently supports Windows builds using CMake and MinGW-w64.

Prefer a static backend build so students don't have to manage arbitrary DLL dependencies.

Verify the resulting executable dependencies using appropriate Windows or cross-compilation tooling.

Do not claim the application is portable until its runtime dependencies have actually been checked.

---

# 14. Packaging

Produce:

```text
release/
└── ICSLab-FPGA-Programmer/
    ├── ICSLab FPGA Programmer.exe
    ├── backend/
    │   └── openFPGALoader.exe
    ├── LICENSES/
    └── README.txt
```

If practical, also produce:

```text
ICSLab-FPGA-Programmer-v1.0.0-win64.zip
```

The ZIP should be usable as:

```text
Download
→ Extract
→ Run ICSLab FPGA Programmer.exe
```

No administrator privileges should be required for the application itself unless Windows USB driver configuration genuinely requires them.

---

# 15. Optional Installer

After the portable version works, optionally create:

```text
ICSLab-FPGA-Programmer-Setup.exe
```

Using an appropriate installer system such as Inno Setup or WiX.

The portable version remains mandatory.

---

# 16. Application Metadata

Set Windows executable metadata:

```text
Product Name:
ICSLab FPGA Programmer

Company:
ICSLab

Author:
ICSLab

Description:
Simple FPGA programming utility powered by openFPGALoader

Copyright:
ICSLab
```

Do not claim that ICSLab authored openFPGALoader.

Clearly state:

```text
ICSLab FPGA Programmer uses openFPGALoader as its FPGA programming backend.
openFPGALoader is an independent open-source project.
```

Preserve all upstream copyright and license notices.

---

# 17. Open-Source Licensing

Before redistribution:

1. Inspect the exact openFPGALoader license.
2. Determine obligations created by distributing its executable.
3. Preserve copyright notices.
4. Include the upstream license.
5. Include licenses for bundled third-party libraries.
6. Do not remove or obscure attribution.
7. Clearly distinguish the ICSLab GUI from the upstream openFPGALoader project.

Create:

```text
LICENSES/
README-licenses.txt
```

with the necessary notices.

---

# 18. Student Mode

The default GUI should expose ONLY:

```text
Board
Bitstream
SRAM / Flash
Detect
Program
Status
```

Do not present students with:

- JTAG frequency
- SPI flash offsets
- VID/PID
- index-chain
- custom cable arguments
- reset sequencing
- low-level JTAG parameters

unless necessary.

Add an optional:

```text
Advanced >>
```

section for those parameters later.

The first release does not need advanced mode if that delays a stable basic release.

---

# 19. Configuration Persistence

Remember the most recently selected:

- board
- programming mode
- bitstream directory

Do NOT automatically program the previous bitstream when the application starts.

Programming must always require the user to click:

**PROGRAM FPGA**

---

# 20. Safety

Before executing a programming command, display enough information for the student to recognize what will happen:

```text
Board: Nexys Video
File: lab3.bit
Target: FPGA SRAM
```

Disable the Program button while a programming operation is active.

Do not permit two programming processes to execute simultaneously.

Do not automatically erase flash.

Only erase or overwrite flash as required by the explicitly requested programming operation.

---

# 21. First-Version Scope

The first stable version should prioritize the boards typically used in an FPGA teaching laboratory.

However, architect it so other openFPGALoader-supported boards can be added easily.

Do not fork the backend unnecessarily.

---

# 22. Tests

Implement at least basic tests for:

- backend executable discovery
- correct quoting of Windows paths containing spaces
- board argument generation
- bitstream filename handling
- SRAM command generation
- flash command generation
- error return code handling
- stdout/stderr capture
- application behavior when openFPGALoader is missing

Also test paths such as:

```text
C:\Users\Student Name\Desktop\Lab 3\final design.bit
```

Correct Windows argument quoting is mandatory.

---

# 23. CI

Create GitHub Actions that can build a Windows release automatically.

Prefer:

```text
.github/workflows/windows-release.yml
```

The workflow should:

1. checkout the GUI repository
2. fetch a pinned openFPGALoader version
3. build openFPGALoader for Windows
4. build the ICSLab GUI
5. collect runtime dependencies
6. package the release directory
7. generate the ZIP artifact

Pin the upstream version rather than silently building whatever happens to be on `master`.

Also support a deliberate manual update of the pinned version.

---

# 24. README

Write a short README oriented toward students.

Example:

```text
# ICSLab FPGA Programmer

1. Connect your FPGA board to USB.
2. Power on the board.
3. Open ICSLab FPGA Programmer.
4. Select your board.
5. Select the .bit file.
6. Click PROGRAM FPGA.
7. Wait for "Programming successful."

Developed by ICSLab.

FPGA programming backend:
openFPGALoader
https://github.com/trabucayre/openFPGALoader
```

Add troubleshooting for:

- board not detected
- USB driver issue
- wrong board selected
- malformed bitstream
- JTAG device not found
- flash programming failure

---

# 25. Deliverables

Do not stop after writing example code.

Actually produce a buildable project.

Required deliverables:

```text
1. Complete source code
2. Build scripts
3. CMake configuration
4. Windows GUI
5. Built openFPGALoader Windows backend
6. Portable Windows package
7. Windows executable
8. README
9. Driver instructions
10. License notices
11. GitHub Actions Windows build
```

If the current environment supports producing Windows binaries, build them.

If cross-compilation is required, use the official openFPGALoader Windows MinGW toolchain.

Verify that the produced binaries are genuine Windows PE executables.

---

# 26. Development Process

Work autonomously.

First:

1. Clone and inspect the current openFPGALoader repository.
2. Read its Windows installation/build instructions.
3. Inspect current command-line options.
4. Inspect board enumeration support.
5. Inspect USB/backend requirements.
6. Check the license.
7. Decide the smallest reasonable GUI implementation.
8. Implement it.
9. Compile it.
10. Run tests.
11. Package the Windows release.

Do not merely describe what should be done.

Make the project.

Where something cannot be tested because physical FPGA hardware is unavailable, clearly identify that limitation, but still complete all software-side testing.

---

# 27. UI Philosophy

Think:

**"A student should be able to program an FPGA during a lab without knowing what openFPGALoader is."**

The ideal workflow is:

```text
Connect board
       ↓
Open ICSLab FPGA Programmer
       ↓
Select .bit
       ↓
PROGRAM FPGA
       ↓
Done
```

Keep the application small, fast, reliable, and boring.

That is more valuable for a teaching laboratory than a complicated interface.

At the end, provide:

- the final repository tree
- build instructions
- generated Windows release path
- exact openFPGALoader version used
- list of tested commands
- remaining hardware-dependent tests
- screenshots of the GUI if possible
- SHA-256 hashes of the final `.exe` and `.zip`