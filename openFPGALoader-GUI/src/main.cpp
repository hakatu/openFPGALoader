// SPDX-License-Identifier: Apache-2.0
// openFPGALoader GUI - entry point.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "MainWindow.h"
#include "DriverSetup.h"

#include <cwchar>

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
		// Elevated USB driver setup helper started by "USB Setup..."
		if (argc > 1 && !wcscmp(argv[1], L"--driver-setup")) {
			int rc = oflgui::driverSetupHelperMain(argc, argv);
			LocalFree(argv);
			return rc;
		}
		if (argc > 1)
			initialBitstream = oflgui::narrow(argv[1]);
		LocalFree(argv);
	}

	oflgui::MainWindow win(hInst);
	if (!win.create(nCmdShow, initialBitstream)) {
		MessageBoxW(nullptr, L"Unable to create the main window.",
			L"openFPGALoader GUI", MB_ICONERROR);
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
