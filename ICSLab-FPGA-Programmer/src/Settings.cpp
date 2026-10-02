// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - persisted user choices.

#include "Settings.h"
#include "OpenFPGALoaderBackend.h"
#include "ProcessRunner.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace icslab {

static const wchar_t *kSection = L"ICSLabFPGAProgrammer";

std::string Settings::defaultPath()
{
	wchar_t buf[MAX_PATH * 2];
	DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, sizeof(buf) / sizeof(buf[0]));
	if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
		return std::string();
	return narrow(buf) + "\\ICSLab\\FPGA Programmer\\settings.ini";
}

static std::string readKey(const std::wstring &ini, const wchar_t *key)
{
	wchar_t buf[2048];
	GetPrivateProfileStringW(kSection, key, L"", buf, sizeof(buf) / sizeof(buf[0]), ini.c_str());
	return narrow(buf);
}

void Settings::load(const std::string &iniPath)
{
	if (iniPath.empty())
		return;
	std::wstring ini = widen(iniPath);
	board = readKey(ini, L"board");
	cable = readKey(ini, L"cable");
	flash = readKey(ini, L"mode") == "flash";
	lastDir = readKey(ini, L"lastDir");
}

bool Settings::save(const std::string &iniPath) const
{
	if (iniPath.empty())
		return false;
	// Create %APPDATA%\ICSLab\FPGA Programmer
	std::string dir = fileDirName(iniPath);
	CreateDirectoryW(widen(fileDirName(dir)).c_str(), nullptr);
	CreateDirectoryW(widen(dir).c_str(), nullptr);

	std::wstring ini = widen(iniPath);
	// Write a UTF-16LE BOM first so the profile API stores Unicode values.
	if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
		HANDLE h = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			const unsigned char bom[2] = {0xFF, 0xFE};
			DWORD written = 0;
			WriteFile(h, bom, 2, &written, nullptr);
			CloseHandle(h);
		}
	}
	bool ok = true;
	ok &= !!WritePrivateProfileStringW(kSection, L"board", widen(board).c_str(), ini.c_str());
	ok &= !!WritePrivateProfileStringW(kSection, L"cable", widen(cable).c_str(), ini.c_str());
	ok &= !!WritePrivateProfileStringW(kSection, L"mode", flash ? L"flash" : L"sram", ini.c_str());
	ok &= !!WritePrivateProfileStringW(kSection, L"lastDir", widen(lastDir).c_str(), ini.c_str());
	return ok;
}

} // namespace icslab
