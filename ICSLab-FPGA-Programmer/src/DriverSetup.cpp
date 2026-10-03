// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - Intel USB-Blaster driver setup (Windows only).

#include "DriverSetup.h"
#include "OpenFPGALoaderBackend.h"
#include "ProcessRunner.h"

#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <memory>

namespace icslab {

namespace {

// {88BAE032-5A81-49F0-BC3D-A4FF138216D6}: "Universal Serial Bus devices",
// the setup class of the inbox winusb.inf.
const GUID kUsbDeviceClass =
	{0x88bae032, 0x5a81, 0x49f0, {0xbc, 0x3d, 0xa4, 0xff, 0x13, 0x82, 0x16, 0xd6}};
const wchar_t *kUsbDeviceClassStr = L"{88BAE032-5A81-49F0-BC3D-A4FF138216D6}";

#ifndef DN_HAS_PROBLEM
#define DN_HAS_PROBLEM 0x00000400
#endif

struct DevInfoSet {
	HDEVINFO h = INVALID_HANDLE_VALUE;
	~DevInfoSet() { if (h != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(h); }
};

std::string regString(HDEVINFO set, SP_DEVINFO_DATA *dev, DWORD prop)
{
	wchar_t buf[2048];
	DWORD type = 0, size = 0;
	if (!SetupDiGetDeviceRegistryPropertyW(set, dev, prop, &type,
			reinterpret_cast<BYTE *>(buf), sizeof(buf) - sizeof(wchar_t), &size))
		return std::string();
	buf[size / sizeof(wchar_t)] = L'\0';
	return narrow(buf); // first string of a REG_MULTI_SZ
}

std::vector<std::string> regMultiString(HDEVINFO set, SP_DEVINFO_DATA *dev, DWORD prop)
{
	std::vector<std::string> out;
	wchar_t buf[4096] = {};
	DWORD type = 0, size = 0;
	if (!SetupDiGetDeviceRegistryPropertyW(set, dev, prop, &type,
			reinterpret_cast<BYTE *>(buf), sizeof(buf) - 2 * sizeof(wchar_t), &size))
		return out;
	for (const wchar_t *p = buf; *p; p += wcslen(p) + 1)
		out.push_back(narrow(p));
	return out;
}

std::string instanceIdOf(HDEVINFO set, SP_DEVINFO_DATA *dev)
{
	wchar_t id[MAX_DEVICE_ID_LEN + 1] = {};
	if (!SetupDiGetDeviceInstanceIdW(set, dev, id, MAX_DEVICE_ID_LEN, nullptr))
		return std::string();
	return narrow(id);
}

void appendLine(std::string &s, const std::string &line)
{
	if (!s.empty())
		s += "\r\n";
	s += line;
}

typedef BOOL (WINAPI *DiInstallDeviceFn)(HWND, HDEVINFO, PSP_DEVINFO_DATA,
	PSP_DRVINFO_DATA_W, DWORD, PBOOL);
typedef BOOL (WINAPI *DiUninstallDeviceFn)(HWND, HDEVINFO, PSP_DEVINFO_DATA, DWORD, PBOOL);

FARPROC newdevProc(const char *name)
{
	static HMODULE newdev = LoadLibraryW(L"newdev.dll");
	return newdev ? GetProcAddress(newdev, name) : nullptr;
}

bool openDevice(DevInfoSet &set, SP_DEVINFO_DATA &dev, const std::string &instanceId)
{
	set.h = SetupDiCreateDeviceInfoList(nullptr, nullptr);
	if (set.h == INVALID_HANDLE_VALUE)
		return false;
	dev = {};
	dev.cbSize = sizeof(dev);
	return SetupDiOpenDeviceInfoW(set.h, widen(instanceId).c_str(), nullptr, 0, &dev) != FALSE;
}

/* Bind the inbox WinUSB driver (winusb.inf, "WinUsb Device") to one device. */
bool installWinUsbOn(const std::string &instanceId, std::string &log, bool &reboot)
{
	auto diInstall = reinterpret_cast<DiInstallDeviceFn>(reinterpret_cast<void (*)()>(newdevProc("DiInstallDevice")));
	if (!diInstall) {
		appendLine(log, "  DiInstallDevice is not available on this Windows version.");
		return false;
	}

	DevInfoSet set;
	SP_DEVINFO_DATA dev;
	if (!openDevice(set, dev, instanceId)) {
		appendLine(log, "  cannot open the device: " + describeWin32Error(GetLastError()));
		return false;
	}

	// winusb.inf belongs to the "Universal Serial Bus devices" class: move the
	// device to that class first (what Device Manager does when the user
	// picks that hardware type), then reopen it.
	if (!IsEqualGUID(dev.ClassGuid, kUsbDeviceClass)) {
		if (!SetupDiSetDeviceRegistryPropertyW(set.h, &dev, SPDRP_CLASSGUID,
				reinterpret_cast<const BYTE *>(kUsbDeviceClassStr),
				static_cast<DWORD>((wcslen(kUsbDeviceClassStr) + 1) * sizeof(wchar_t)))) {
			appendLine(log, "  cannot change the device class: " +
				describeWin32Error(GetLastError()));
			return false;
		}
		SetupDiDestroyDeviceInfoList(set.h);
		set.h = INVALID_HANDLE_VALUE;
		if (!openDevice(set, dev, instanceId)) {
			appendLine(log, "  cannot reopen the device: " + describeWin32Error(GetLastError()));
			return false;
		}
	}

	// Only look at the inbox winusb.inf
	SP_DEVINSTALL_PARAMS_W params = {};
	params.cbSize = sizeof(params);
	if (!SetupDiGetDeviceInstallParamsW(set.h, &dev, &params)) {
		appendLine(log, "  SetupDiGetDeviceInstallParams: " + describeWin32Error(GetLastError()));
		return false;
	}
	params.Flags |= DI_ENUMSINGLEINF;
	params.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS;
	ExpandEnvironmentStringsW(L"%SystemRoot%\\INF\\winusb.inf", params.DriverPath, MAX_PATH);
	if (!SetupDiSetDeviceInstallParamsW(set.h, &dev, &params) ||
			!SetupDiBuildDriverInfoList(set.h, &dev, SPDIT_CLASSDRIVER)) {
		appendLine(log, "  cannot read winusb.inf: " + describeWin32Error(GetLastError()));
		return false;
	}

	SP_DRVINFO_DATA_W drv = {};
	drv.cbSize = sizeof(drv);
	bool found = false;
	for (DWORD i = 0; ; ++i) {
		SP_DRVINFO_DATA_W d = {};
		d.cbSize = sizeof(d);
		if (!SetupDiEnumDriverInfoW(set.h, &dev, SPDIT_CLASSDRIVER, i, &d))
			break;
		std::string desc = narrow(d.Description);
		if (!found || desc.find("WinUsb") != std::string::npos ||
				desc.find("WinUSB") != std::string::npos) {
			drv = d;
			found = true;
		}
	}
	if (!found) {
		appendLine(log, "  winusb.inf offers no driver for this device.");
		return false;
	}
	if (!SetupDiSetSelectedDriverW(set.h, &dev, &drv)) {
		appendLine(log, "  SetupDiSetSelectedDriver: " + describeWin32Error(GetLastError()));
		return false;
	}
	BOOL needReboot = FALSE;
	if (!diInstall(nullptr, set.h, &dev, &drv, 0, &needReboot)) {
		appendLine(log, "  DiInstallDevice: " + describeWin32Error(GetLastError()));
		return false;
	}
	reboot = reboot || needReboot;
	appendLine(log, "  WinUSB driver installed (\"" + narrow(drv.Description) + "\").");
	return true;
}

/* Uninstall the device so that Windows re-installs its best (Intel) driver. */
bool restoreDriverOn(const std::string &instanceId, std::string &log, bool &reboot)
{
	auto diUninstall = reinterpret_cast<DiUninstallDeviceFn>(reinterpret_cast<void (*)()>(newdevProc("DiUninstallDevice")));
	DevInfoSet set;
	SP_DEVINFO_DATA dev;
	if (!openDevice(set, dev, instanceId)) {
		appendLine(log, "  cannot open the device: " + describeWin32Error(GetLastError()));
		return false;
	}
	BOOL needReboot = FALSE;
	bool ok = diUninstall ? diUninstall(nullptr, set.h, &dev, 0, &needReboot) != FALSE
		: SetupDiRemoveDevice(set.h, &dev) != FALSE;
	if (!ok) {
		appendLine(log, "  uninstall failed: " + describeWin32Error(GetLastError()));
		return false;
	}
	reboot = reboot || needReboot;
	appendLine(log, "  WinUSB removed; Windows re-installs the default driver.");
	return true;
}

void rescanDevices()
{
	DEVINST root;
	if (CM_Locate_DevNodeW(&root, nullptr, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
		CM_Reenumerate_DevNode(root, 0);
}

std::vector<std::string> subDirectories(const std::string &dir)
{
	std::vector<std::string> out;
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(widen(dir + "\\*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return out;
	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
			out.push_back(dir + "\\" + narrow(fd.cFileName));
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	sortQuartusVersionDirsNewestFirst(out);
	return out;
}

// "C:", "D:", ... for local fixed disks (Quartus may be installed on D:).
std::vector<std::string> localFixedDrives()
{
	std::vector<std::string> out;
	DWORD mask = GetLogicalDrives();
	for (int i = 0; i < 26; ++i) {
		if (!(mask & (1u << i)))
			continue;
		wchar_t root[4] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', 0};
		if (GetDriveTypeW(root) == DRIVE_FIXED)
			out.push_back(std::string(1, static_cast<char>('A' + i)) + ":");
	}
	return out;
}

std::string findFileRecursive(const std::string &dir, const std::string &name, int depth)
{
	std::string direct = dir + "\\" + name;
	if (fileExists(direct))
		return direct;
	if (depth <= 0)
		return std::string();
	for (const auto &sub : subDirectories(dir)) {
		std::string r = findFileRecursive(sub, name, depth - 1);
		if (!r.empty())
			return r;
	}
	return std::string();
}

std::string envVar(const wchar_t *name)
{
	wchar_t buf[MAX_PATH * 2];
	DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH * 2);
	return (n > 0 && n < MAX_PATH * 2) ? narrow(buf) : std::string();
}

bool writeUtf8File(const std::string &path, const std::string &text)
{
	HANDLE h = CreateFileW(widen(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD w = 0;
	WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &w, nullptr);
	CloseHandle(h);
	return true;
}

std::string readUtf8File(const std::string &path)
{
	HANDLE h = CreateFileW(widen(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return std::string();
	std::string data;
	char buf[4096];
	DWORD got = 0;
	while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0)
		data.append(buf, got);
	CloseHandle(h);
	return data;
}

} // namespace

/* ------------------------------------------------------------------------ */

bool UsbDeviceStatus::usesWinUsb() const
{
	return isWinUsbService(service);
}

std::string UsbDeviceStatus::summary() const
{
	std::string s = blasterDisplayName(vid, pid) + ": ";
	if (hasProblem)
		return s + "driver NOT working (" + describeDeviceProblem(problem) + ")";
	if (usesWinUsb())
		return s + "WinUSB driver - ready for openFPGALoader";
	if (service.empty())
		return s + "no driver";
	return s + "driver \"" + service + "\" (Quartus) - openFPGALoader needs WinUSB";
}

std::vector<UsbDeviceStatus> listIntelBlasters()
{
	std::vector<UsbDeviceStatus> out;
	DevInfoSet set;
	set.h = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (set.h == INVALID_HANDLE_VALUE)
		return out;
	for (DWORD i = 0; ; ++i) {
		SP_DEVINFO_DATA dev = {};
		dev.cbSize = sizeof(dev);
		if (!SetupDiEnumDeviceInfo(set.h, i, &dev))
			break;
		UsbDeviceStatus st;
		bool match = false;
		for (const auto &hw : regMultiString(set.h, &dev, SPDRP_HARDWAREID)) {
			if (parseUsbHardwareId(hw, st.vid, st.pid, st.mi) && isIntelBlaster(st.vid, st.pid) &&
					st.mi <= 0) {
				st.hardwareId = hw;
				match = true;
				break;
			}
		}
		if (!match)
			continue;
		st.instanceId = instanceIdOf(set.h, &dev);
		st.description = regString(set.h, &dev, SPDRP_DEVICEDESC);
		st.service = regString(set.h, &dev, SPDRP_SERVICE);
		ULONG status = 0, problem = 0;
		if (CM_Get_DevNode_Status(&status, &problem, dev.DevInst, 0) == CR_SUCCESS &&
				(status & DN_HAS_PROBLEM)) {
			st.hasProblem = true;
			st.problem = problem;
		} else if (st.service.empty()) {
			st.hasProblem = true;
			st.problem = 28; // CM_PROB_FAILED_INSTALL: no driver
		}
		out.push_back(st);
	}
	return out;
}

std::string blasterDriverHint(const std::vector<UsbDeviceStatus> &devs)
{
	std::string s;
	for (const auto &d : devs) {
		if (!d.usableByOpenFPGALoader())
			appendLine(s, d.summary());
	}
	if (!s.empty())
		appendLine(s, "Lab instructor: click \"USB Setup...\" > \"Install WinUSB driver\".");
	return s;
}

bool isProcessElevated()
{
	HANDLE token = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return false;
	TOKEN_ELEVATION elev = {};
	DWORD size = 0;
	bool r = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size) &&
		elev.TokenIsElevated;
	CloseHandle(token);
	return r;
}

std::vector<std::string> installedQuartusVersionDirs()
{
	// System drive first, then the other local disks.
	std::vector<std::string> drives;
	std::string sys = envVar(L"SystemDrive");
	if (!sys.empty())
		drives.push_back(sys);
	for (const auto &d : localFixedDrives()) {
		if (_stricmp(d.c_str(), sys.c_str()) != 0)
			drives.push_back(d);
	}
	std::vector<std::string> versions;
	for (const auto &drive : drives) {
		for (const auto &root : quartusInstallRoots(drive)) {
			auto v = subDirectories(root);
			versions.insert(versions.end(), v.begin(), v.end());
		}
	}
	// C:\\altera\\13.0sp1 and C:\\intelFPGA_lite\\23.1 sorted together, newest first
	sortQuartusVersionDirsNewestFirst(versions);
	return versions;
}

std::vector<std::string> findQuartusTools(const std::string &exeName)
{
	std::vector<std::string> found;
	auto add = [&](const std::string &p) {
		for (const auto &f : found)
			if (_stricmp(f.c_str(), p.c_str()) == 0)
				return;
		found.push_back(p);
	};
	for (const auto &c : quartusToolCandidates(exeName, envVar(L"QUARTUS_ROOTDIR"),
			installedQuartusVersionDirs())) {
		if (fileExists(c))
			add(c);
	}
	// Quartus's bin64 folder on PATH (custom install locations)
	wchar_t buf[MAX_PATH * 2];
	DWORD n = SearchPathW(nullptr, widen(exeName).c_str(), nullptr, MAX_PATH * 2, buf, nullptr);
	if (n > 0 && n < MAX_PATH * 2)
		add(narrow(buf));
	return found;
}

std::string findQuartusTool(const std::string &exeName)
{
	auto all = findQuartusTools(exeName);
	return all.empty() ? std::string() : all.front();
}

bool isFileNotOlder(const std::string &a, const std::string &b)
{
	WIN32_FILE_ATTRIBUTE_DATA fa, fb;
	if (!GetFileAttributesExW(widen(a).c_str(), GetFileExInfoStandard, &fa) ||
			!GetFileAttributesExW(widen(b).c_str(), GetFileExInfoStandard, &fb))
		return false;
	return CompareFileTime(&fa.ftLastWriteTime, &fb.ftLastWriteTime) >= 0;
}

std::string findBlasterFirmware(const std::string &exeDir, const std::string &configuredPath)
{
	const std::vector<std::string> versions = installedQuartusVersionDirs();
	for (const auto &c : blasterFirmwareCandidates(exeDir, configuredPath,
			envVar(L"QUARTUS_ROOTDIR"), versions)) {
		if (fileExists(c))
			return c;
	}
	// File layout differs between Quartus versions: search each install.
	for (const auto &v : versions) {
		std::string r = findFileRecursive(v, blasterFirmwareFileName(), 4);
		if (!r.empty())
			return r;
	}
	return std::string();
}

/* ------------------------------------------------------------------------ */
/* Elevated helper                                                          */
/* ------------------------------------------------------------------------ */

DriverResult runDriverSetupElevated(const std::string &action, const std::string &backendExe,
	const std::string &firmware)
{
	DriverResult res;
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	std::string resultFile = narrow(tmp) + "icslab-driver-setup-" +
		std::to_string(GetCurrentProcessId()) + ".txt";
	DeleteFileW(widen(resultFile).c_str());

	std::wstring exe(MAX_PATH, L'\0');
	exe.resize(GetModuleFileNameW(nullptr, &exe[0], MAX_PATH));

	std::vector<std::string> args = {action, resultFile,
		backendExe.empty() ? "-" : backendExe, firmware.empty() ? "-" : firmware};
	std::string params = "--driver-setup";
	for (const auto &a : args)
		params += " " + quoteWindowsArg(a);
	std::wstring wparams = widen(params);

	SHELLEXECUTEINFOW sei = {};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
	sei.lpVerb = L"runas";
	sei.lpFile = exe.c_str();
	sei.lpParameters = wparams.c_str();
	sei.nShow = SW_HIDE;
	if (!ShellExecuteExW(&sei) || !sei.hProcess) {
		DWORD err = GetLastError();
		res.cancelled = err == ERROR_CANCELLED;
		res.message = res.cancelled ? "Cancelled: administrator rights are required."
			: "Could not start the setup: " + describeWin32Error(err);
		return res;
	}
	WaitForSingleObject(sei.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(sei.hProcess, &code);
	CloseHandle(sei.hProcess);

	res.message = readUtf8File(resultFile);
	DeleteFileW(widen(resultFile).c_str());
	res.ok = code == 0 || code == 3;
	res.rebootRequired = code == 3;
	if (res.message.empty())
		res.message = "The setup ended with code " + std::to_string(code) + ".";
	return res;
}

int driverSetupHelperMain(int argc, wchar_t **argv)
{
	// argv: exe --driver-setup <install|restore> <resultFile> <backend|-> <firmware|->
	if (argc < 6)
		return 2;
	const std::string action = narrow(argv[2]);
	const std::string resultFile = narrow(argv[3]);
	const std::string backend = narrow(argv[4]) == "-" ? std::string() : narrow(argv[4]);
	const std::string firmware = narrow(argv[5]) == "-" ? std::string() : narrow(argv[5]);

	std::string log;
	bool reboot = false;
	int failures = 0;

	if (!isProcessElevated()) {
		writeUtf8File(resultFile, "Administrator rights are required.");
		return 2;
	}

	auto devices = listIntelBlasters();
	if (action == "restore") {
		int n = 0;
		for (const auto &d : devices) {
			if (!d.usesWinUsb())
				continue;
			++n;
			appendLine(log, blasterDisplayName(d.vid, d.pid) + ":");
			if (!restoreDriverOn(d.instanceId, log, reboot))
				++failures;
		}
		if (n == 0)
			appendLine(log, "No USB-Blaster uses the WinUSB driver: nothing to restore.");
		rescanDevices();
	} else if (action == "install") {
		if (devices.empty())
			appendLine(log, "No Intel USB-Blaster is connected. Connect the board, "
				"switch it on and try again.");
		bool coldBlasterII = false;
		for (const auto &d : devices) {
			appendLine(log, blasterDisplayName(d.vid, d.pid) + ":");
			if (d.usableByOpenFPGALoader()) {
				appendLine(log, "  already uses WinUSB.");
			} else if (!installWinUsbOn(d.instanceId, log, reboot)) {
				++failures;
				continue;
			}
			if (d.pid == kBlasterIIColdPid)
				coldBlasterII = true;
		}

		// USB-Blaster II: load the firmware once so that the device appears
		// with its final ID (09FB:6810), then bind WinUSB to it too.
		if (coldBlasterII) {
			if (firmware.empty() || backend.empty()) {
				appendLine(log, "USB-Blaster II: select blaster_6810.hex (firmware) and run the "
					"setup again to finish (second USB ID 09FB:6810).");
			} else {
				appendLine(log, "USB-Blaster II: loading firmware to finish the setup...");
				bool done = false;
				for (int attempt = 0; attempt < 3 && !done; ++attempt) {
					Sleep(1500);
					runProcess(backend, buildDetectArgs("", "usb-blasterII", firmware), nullptr);
					for (int t = 0; t < 20 && !done; ++t) {
						Sleep(500);
						for (const auto &d : listIntelBlasters()) {
							if (d.pid != kBlasterIIPid)
								continue;
							appendLine(log, blasterDisplayName(d.vid, d.pid) + ":");
							if (d.usableByOpenFPGALoader())
								appendLine(log, "  already uses WinUSB.");
							else if (!installWinUsbOn(d.instanceId, log, reboot))
								++failures;
							done = true;
							break;
						}
					}
				}
				if (!done) {
					appendLine(log, "  the USB-Blaster II did not re-appear as 09FB:6810. "
						"Unplug/replug the board and run the setup again.");
					++failures;
				}
			}
		}
	} else {
		writeUtf8File(resultFile, "unknown action " + action);
		return 2;
	}

	if (failures)
		appendLine(log, "\r\nSome devices could not be configured automatically. Manual "
			"alternative: Device Manager > right-click the USB-Blaster > Update driver > "
			"Browse my computer > Let me pick > \"Universal Serial Bus devices\" > "
			"\"WinUsb Device\" (see docs\\windows-driver.md).");
	if (reboot)
		appendLine(log, "Windows asks for a restart to finish.");
	writeUtf8File(resultFile, log);
	return failures ? 1 : (reboot ? 3 : 0);
}

} // namespace icslab
