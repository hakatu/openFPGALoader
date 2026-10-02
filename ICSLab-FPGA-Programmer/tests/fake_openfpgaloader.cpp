// SPDX-License-Identifier: Apache-2.0
// Fake openFPGALoader used by the test-suite (Windows only).
//
// - prints every argument it received (as parsed by Windows) as
//   "ARG<n>=<value>" (UTF-8) on stdout
// - "--fake-exit N"   : exit with code N
// - "--fake-stderr"   : also writes a line to stderr
// - "--fake-progress" : prints openFPGALoader style progress lines
// - "--fake-stdin"    : tries to read stdin (must get EOF immediately)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

static std::string utf8(const wchar_t *w)
{
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
	if (n > 1)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
	return s;
}

int main()
{
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	int exitCode = 0;
	for (int i = 1; i < argc; ++i) {
		std::printf("ARG%d=%s\n", i, utf8(argv[i]).c_str());
		if (!std::wcscmp(argv[i], L"--fake-exit") && i + 1 < argc)
			exitCode = static_cast<int>(std::wcstol(argv[i + 1], nullptr, 10));
		if (!std::wcscmp(argv[i], L"--fake-stderr")) {
			std::fflush(stdout);
			std::fprintf(stderr, "Error: this line goes to stderr\n");
			std::fflush(stderr);
		}
		if (!std::wcscmp(argv[i], L"--fake-progress")) {
			for (int p = 0; p <= 100; p += 50) {
				std::printf("\rLoading: [%-10.*s] %3.2f%%\n", p / 10, "==========",
					static_cast<double>(p));
				std::fflush(stdout);
			}
			std::printf("Done\n");
		}
		if (!std::wcscmp(argv[i], L"--fake-stdin")) {
			char buf[16];
			size_t n = std::fread(buf, 1, sizeof(buf), stdin);
			std::printf("STDIN_BYTES=%u\n", static_cast<unsigned>(n));
		}
	}
	std::fflush(stdout);
	LocalFree(argv);
	return exitCode;
}
