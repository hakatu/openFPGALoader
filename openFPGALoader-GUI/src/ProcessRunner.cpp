// SPDX-License-Identifier: Apache-2.0
// openFPGALoader GUI - child process execution (Windows only).

#include "ProcessRunner.h"
#include "OpenFPGALoaderBackend.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace oflgui {

std::wstring widen(const std::string &s)
{
	if (s.empty())
		return std::wstring();
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring w(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
	return w;
}

std::string narrow(const std::wstring &w)
{
	if (w.empty())
		return std::string();
	int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
		nullptr, 0, nullptr, nullptr);
	std::string s(static_cast<size_t>(n), '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n,
		nullptr, nullptr);
	return s;
}

std::string currentExeDir()
{
	std::wstring buf(MAX_PATH, L'\0');
	for (;;) {
		DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
		if (n == 0)
			return std::string();
		if (n < buf.size()) {
			buf.resize(n);
			break;
		}
		buf.resize(buf.size() * 2);
	}
	return fileDirName(narrow(buf));
}

bool fileExists(const std::string &p)
{
	DWORD a = GetFileAttributesW(widen(p).c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::string &p)
{
	DWORD a = GetFileAttributesW(widen(p).c_str());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool representableInAcp(const std::wstring &w)
{
	if (w.empty())
		return true;
	BOOL usedDefault = FALSE;
	int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.data(),
		static_cast<int>(w.size()), nullptr, 0, nullptr, &usedDefault);
	return n > 0 && !usedDefault;
}

std::string backendSafePath(const std::string &utf8Path)
{
	std::wstring w = widen(utf8Path);
	if (representableInAcp(w))
		return utf8Path;
	DWORD n = GetShortPathNameW(w.c_str(), nullptr, 0);
	if (n == 0)
		return std::string();
	std::wstring s(n, L'\0');
	n = GetShortPathNameW(w.c_str(), &s[0], n);
	s.resize(n);
	if (n == 0 || !representableInAcp(s))
		return std::string();
	return narrow(s);
}

std::string describeWin32Error(unsigned long err)
{
	wchar_t *msg = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0,
		reinterpret_cast<wchar_t *>(&msg), 0, nullptr);
	std::string s = msg ? trim(narrow(msg)) : std::string();
	if (msg)
		LocalFree(msg);
	return "Windows error " + std::to_string(err) + (s.empty() ? "" : ": " + s);
}

/* The backend writes in the ANSI code page; convert to UTF-8 for display. */
static std::string acpToUtf8(const std::string &s)
{
	if (s.empty())
		return s;
	int n = MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	if (n <= 0)
		return s;
	std::wstring w(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
	return narrow(w);
}

ProcessResult runProcess(const std::string &exe, const std::vector<std::string> &args,
	const std::function<void(const std::string &)> &onLine)
{
	ProcessResult res;

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE readPipe = nullptr, writePipe = nullptr;
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
		res.launchError = GetLastError();
		return res;
	}
	SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

	HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		&sa, OPEN_EXISTING, 0, nullptr);

	// Only the three std handles are inherited by the child.
	HANDLE inherit[2] = {writePipe, nul};
	SIZE_T attrSize = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
	std::vector<unsigned char> attrBuf(attrSize);
	auto attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
	bool haveAttr = InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize) &&
		UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
			(nul != INVALID_HANDLE_VALUE ? 2 : 1) * sizeof(HANDLE), nullptr, nullptr);

	STARTUPINFOEXW si = {};
	si.StartupInfo.cb = haveAttr ? sizeof(si) : sizeof(STARTUPINFOW);
	si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	si.StartupInfo.hStdInput = nul != INVALID_HANDLE_VALUE ? nul : nullptr;
	si.StartupInfo.hStdOutput = writePipe;
	si.StartupInfo.hStdError = writePipe;
	if (haveAttr)
		si.lpAttributeList = attrList;

	std::wstring cmd = widen(buildCommandLine(exe, args));
	std::wstring wexe = widen(exe);
	std::string dir = fileDirName(exe);
	std::wstring workDir = dirExists(dir) ? widen(dir) : std::wstring();

	PROCESS_INFORMATION pi = {};
	DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
		(haveAttr ? EXTENDED_STARTUPINFO_PRESENT : 0);
	BOOL ok = CreateProcessW(wexe.c_str(), &cmd[0], nullptr, nullptr, TRUE, flags,
		nullptr, workDir.empty() ? nullptr : workDir.c_str(),
		&si.StartupInfo, &pi);
	if (!ok)
		res.launchError = GetLastError();

	if (haveAttr)
		DeleteProcThreadAttributeList(attrList);
	// Close our copy of the write end so ReadFile reports EOF when the child exits.
	CloseHandle(writePipe);
	if (nul != INVALID_HANDLE_VALUE)
		CloseHandle(nul);

	if (!ok) {
		CloseHandle(readPipe);
		return res;
	}
	res.launched = true;
	CloseHandle(pi.hThread);

	LineSplitter splitter;
	auto emit = [&](const std::string &line) {
		std::string l = acpToUtf8(line);
		if (onLine)
			onLine(l);
	};
	char buf[4096];
	DWORD got = 0;
	while (ReadFile(readPipe, buf, sizeof(buf), &got, nullptr) && got > 0) {
		res.output.append(buf, got);
		splitter.feed(buf, got, emit);
	}
	splitter.flush(emit);
	CloseHandle(readPipe);

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hProcess);
	res.exitCode = static_cast<int>(code);
	res.output = acpToUtf8(res.output);
	return res;
}

} // namespace oflgui
