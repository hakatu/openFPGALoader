// SPDX-License-Identifier: Apache-2.0
// openFPGALoader GUI - child process execution (Windows only).

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace oflgui {

/* UTF-8 <-> UTF-16 helpers */
std::wstring widen(const std::string &utf8);
std::string narrow(const std::wstring &utf16);

/* Directory containing the running executable (no trailing backslash). */
std::string currentExeDir();
bool fileExists(const std::string &utf8Path);
bool dirExists(const std::string &utf8Path);

/* openFPGALoader uses the narrow (ANSI code page) C runtime. A path containing
 * characters outside that code page (e.g. a Vietnamese user name) cannot be
 * passed as is. Returns a path usable by the backend: the path itself, its
 * 8.3 short form, or "" when no safe form exists.
 */
std::string backendSafePath(const std::string &utf8Path);

struct ProcessResult {
	bool launched = false;
	unsigned long launchError = 0; // GetLastError() when !launched
	int exitCode = -1;
	std::string output;            // stdout + stderr, in arrival order
};

/* Runs exe with args, blocking the calling thread (call it from a worker
 * thread). stdout and stderr are captured through one pipe so the log keeps
 * the backend's ordering; stdin is NUL so the backend can never wait for
 * keyboard input; no console window is shown.
 * onLine is invoked (on the calling thread) for every output line.
 */
ProcessResult runProcess(const std::string &exe, const std::vector<std::string> &args,
	const std::function<void(const std::string &)> &onLine);

std::string describeWin32Error(unsigned long err);

} // namespace oflgui
