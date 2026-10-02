// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - entry point.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	// Optional bitstream path argument (pre-fills the field only)
	std::string initialBitstream;
	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv) {
		if (argc > 1)
			initialBitstream = icslab::narrow(argv[1]);
		LocalFree(argv);
	}

	icslab::MainWindow win(hInst);
	if (!win.create(nCmdShow, initialBitstream)) {
		MessageBoxW(nullptr, L"Unable to create the main window.",
			L"ICSLab FPGA Programmer", MB_ICONERROR);
		return 1;
	}

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		if (!IsDialogMessageW(win.hwnd(), &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	return static_cast<int>(msg.wParam);
}
