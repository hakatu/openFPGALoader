// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - Intel USB-Blaster driver setup (Windows only).
//
// openFPGALoader reaches USB-Blasters through libusb, which needs the WinUSB
// driver on the device. Intel's own USB-Blaster drivers often fail to load on
// Windows 11 (yellow mark in Device Manager). This module binds the WinUSB
// driver that ships with Windows (inbox winusb.inf, signed by Microsoft) to
// the blaster - the programmatic equivalent of Device Manager > Update driver
// > "Universal Serial Bus devices" > "WinUsb Device". No third-party driver
// package or certificate is installed. "Restore" uninstalls the device so
// Windows re-installs its default (Intel) driver.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace icslab {

struct UsbDeviceStatus {
	std::string instanceId;   // e.g. USB\VID_09FB&PID_6010\5&2a0d...
	std::string hardwareId;
	std::string description;
	std::string service;      // bound driver service ("WinUSB", "usbblstr", "")
	uint16_t vid = 0, pid = 0;
	int mi = -1;
	unsigned long problem = 0; // CM_PROB_*; 0 when working
	bool hasProblem = false;

	bool usesWinUsb() const;
	bool usableByOpenFPGALoader() const { return usesWinUsb() && !hasProblem; }
	std::string summary() const; // one line for the GUI
};

// Present Intel USB-Blaster / USB-Blaster II devices.
std::vector<UsbDeviceStatus> listIntelBlasters();

// One status line per blaster that openFPGALoader cannot use yet ("" if none).
std::string blasterDriverHint(const std::vector<UsbDeviceStatus> &devs);

struct DriverResult {
	bool ok = false;
	bool cancelled = false;   // UAC prompt refused
	bool rebootRequired = false;
	std::string message;      // multi-line, for the GUI
};

bool isProcessElevated();

// Run the driver setup in an elevated copy of this executable (UAC prompt).
// action: "install" or "restore". Blocking: call from a worker thread.
DriverResult runDriverSetupElevated(const std::string &action, const std::string &backendExe,
	const std::string &firmware);

// Entry point of the elevated helper ("--driver-setup ..." command line).
int driverSetupHelperMain(int argc, wchar_t **argv);

// Locate blaster_6810.hex (configured path, app firmware folder, Quartus installs).
std::string findBlasterFirmware(const std::string &exeDir, const std::string &configuredPath);

// Quartus / Quartus Programmer version folders (C:\intelFPGA_lite\23.1, ...),
// newest first.
std::vector<std::string> installedQuartusVersionDirs();
// Every installed copy of a Quartus tool such as "quartus_cpf.exe":
// $QUARTUS_ROOTDIR first, then all Quartus / Quartus II versions found under
// <drive>:\\intelFPGA*, <drive>:\\altera* on every local disk (newest first),
// then PATH.
std::vector<std::string> findQuartusTools(const std::string &exeName);
// First of findQuartusTools() ("" if Quartus is not installed).
std::string findQuartusTool(const std::string &exeName);
// true when file a exists and is not older than file b.
bool isFileNotOlder(const std::string &a, const std::string &b);

} // namespace icslab
