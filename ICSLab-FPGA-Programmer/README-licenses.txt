ICSLab FPGA Programmer - Third-party software and license notices
==================================================================

ICSLab FPGA Programmer uses openFPGALoader as its FPGA programming backend.
openFPGALoader is an independent open-source project. ICSLab did not author
openFPGALoader; ICSLab only wrote the graphical front-end described below.

The license texts referenced here are in the LICENSES folder.


1. ICSLab FPGA Programmer (the GUI: "ICSLab FPGA Programmer.exe")
------------------------------------------------------------------
Copyright (C) 2026 ICSLab
Licensed under the Apache License, Version 2.0
  -> LICENSES/openFPGALoader-Apache-2.0.txt (same license text)
Source code: the ICSLab-FPGA-Programmer folder of the repository this
release was built from.


2. openFPGALoader (backend\openFPGALoader.exe and backend\share\openFPGALoader\*)
---------------------------------------------------------------------------------
Copyright (C) 2019-2026 Gwenhael Goavec-Merou and the openFPGALoader contributors
Upstream: https://github.com/trabucayre/openFPGALoader
License:  Apache License, Version 2.0 -> LICENSES/openFPGALoader-Apache-2.0.txt
Version:  see backend\VERSION.txt (also shown in the application's "About" box)

Modification made for this distribution (Apache-2.0 section 4(b)):
  src/pathHelper.cpp - native Windows paths are no longer passed to
  "cygpath" and the original path is kept when cygpath is unavailable.
  Without this change openFPGALoader cannot locate its spiOverJtag bridge
  bitstreams on a Windows PC without MSYS2/Cygwin, which breaks flash
  programming of Xilinx and Intel/Altera FPGAs. The modified file carries
  a notice; the patch is in ICSLab-FPGA-Programmer/patches/.

The backend\share\openFPGALoader folder contains the spiOverJtag/bpiOverJtag
bridge bitstreams distributed with openFPGALoader (same license).

openFPGALoader includes cxxopts:
  Copyright (c) 2014-2017 Jarryd Beck - MIT license -> LICENSES/cxxopts-MIT.txt


3. Libraries statically linked into backend\openFPGALoader.exe
--------------------------------------------------------------
libusb 1.0.27
  Copyright (C) 2007-2024 the libusb authors
  https://libusb.info/
  GNU Lesser General Public License 2.1 or later -> LICENSES/LGPL-2.1.txt

libftdi1 1.5
  Copyright (C) 2003-2020 Intra2net AG and the libftdi developers
  https://www.intra2net.com/en/developer/libftdi/
  GNU Lesser General Public License 2.1 -> LICENSES/LGPL-2.1.txt,
  LICENSES/libftdi1-LICENSE.txt

zlib
  Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
  zlib license -> LICENSES/zlib.txt

LGPL notice (libusb, libftdi1): these libraries are used unmodified. You may
modify them and relink openFPGALoader against your modified versions: the
complete source code of openFPGALoader and the build scripts used to produce
this release (scripts/build-release.sh, .github/workflows/windows-release.yml)
are published with this application, and the exact library sources are
published as the "corresponding source" archive of each release (see
README.md, section "Corresponding source"). Upstream sources:
  https://github.com/libusb/libusb/releases/tag/v1.0.27
  https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.5.tar.bz2


4. Compiler runtime (statically linked into both executables)
-------------------------------------------------------------
MinGW-w64 runtime (CRT and headers)
  Copyright (c) the mingw-w64 project; mostly ZPL-2.1 / public domain
  -> LICENSES/mingw-w64-runtime.txt

GCC runtime libraries (libgcc, libstdc++)
  Copyright (C) Free Software Foundation, Inc.
  GPL-3.0 with the GCC Runtime Library Exception
  -> LICENSES/gcc-runtime-library-exception.txt, LICENSES/GPL-3.0.txt


5. Not included
---------------
Zadig, WinUSB/libusbK driver installers and vendor drivers (FTDI, Digilent,
Intel) are NOT included in this package. See docs\windows-driver.md.
