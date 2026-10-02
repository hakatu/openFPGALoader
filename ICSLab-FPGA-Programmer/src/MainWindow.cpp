// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - main window (Win32).

#include "MainWindow.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <set>

#include "resource.h"

namespace icslab {

namespace {

const wchar_t *kClassName = L"ICSLabFPGAProgrammerWindow";
const wchar_t *kAppTitle = L"ICSLab FPGA Programmer";

const UINT WM_APP_LINE = WM_APP + 1;
const UINT WM_APP_DONE = WM_APP + 2;

enum {
	IDC_BOARD = 1001, IDC_CABLE, IDC_FILE, IDC_BROWSE, IDC_SRAM, IDC_FLASH,
	IDC_DETECT, IDC_STATUS, IDC_PROGRESS, IDC_PROGRAM, IDC_DETAILS, IDC_ABOUT,
	IDC_LOG,
};

const COLORREF kBlue = RGB(0x1f, 0x4e, 0x99);
const COLORREF kGreen = RGB(0x1b, 0x7f, 0x2a);
const COLORREF kRed = RGB(0xb0, 0x1c, 0x1c);

// Logical (96 DPI) layout
const int kWidth = 540;
const int kMargin = 16;
const int kLabelW = 140;
const int kInitialHeight = 560; // corrected by resizeToContent()
const int kLogHeight = 190;

const char *kGenericLabel = "Auto Detect / Generic JTAG";
const char *kAutoCable = "Auto (board default)";
const char *kSeparator = "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80 all boards supported by openFPGALoader \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80";

std::string readTextFile(const std::string &path)
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
	if (data.size() >= 3 && data.compare(0, 3, "\xEF\xBB\xBF") == 0)
		data.erase(0, 3);
	return data;
}

std::string windowText(HWND h)
{
	int n = GetWindowTextLengthW(h);
	std::wstring w(static_cast<size_t>(n) + 1, L'\0');
	GetWindowTextW(h, &w[0], n + 1);
	w.resize(static_cast<size_t>(n));
	return narrow(w);
}

std::string hex32(uint32_t v)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "0x%08x", v);
	return buf;
}

} // namespace

MainWindow::MainWindow(HINSTANCE hInst) : _hInst(hInst)
{
	_exeDir = currentExeDir();
	_backend = findBackend(_exeDir, fileExists);
	_settingsPath = Settings::defaultPath();
	_settings.load(_settingsPath);

	std::string lab = readTextFile(_exeDir + "\\lab-boards.txt");
	_labBoards = lab.empty() ? defaultLabBoards() : parseLabBoards(lab);

	if (!_backend.empty()) {
		// Tell openFPGALoader where the bundled spiOverJtag bridges live
		// (needed for flash programming of Xilinx/Altera FPGAs).
		std::string soj = backendSafePath(bridgeDirectory(_backend));
		if (!soj.empty() && dirExists(soj))
			SetEnvironmentVariableW(L"OPENFPGALOADER_SOJ_DIR", widen(soj).c_str());
	}
}

MainWindow::~MainWindow()
{
	for (HFONT f : {_fontUi, _fontTitle, _fontBig, _fontMono, _fontSmall})
		if (f)
			DeleteObject(f);
	if (_bgBrush)
		DeleteObject(_bgBrush);
	if (_statusBrush)
		DeleteObject(_statusBrush);
	if (_worker)
		CloseHandle(_worker);
}

bool MainWindow::create(int nCmdShow, const std::string &initialBitstream)
{
	_initialBitstream = initialBitstream;
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = wndProc;
	wc.hInstance = _hInst;
	wc.hIcon = LoadIconW(_hInst, MAKEINTRESOURCEW(IDI_APPICON));
	wc.hIconSm = static_cast<HICON>(LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APPICON),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
	wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
	_bgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
	_statusBrush = CreateSolidBrush(RGB(0xf4, 0xf6, 0xf9));
	wc.hbrBackground = _bgBrush;
	wc.lpszClassName = kClassName;
	if (!RegisterClassExW(&wc))
		return false;

	HDC screen = GetDC(nullptr);
	_dpi = GetDeviceCaps(screen, LOGPIXELSY);
	ReleaseDC(nullptr, screen);

	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
	RECT r = {0, 0, S(kWidth), S(kInitialHeight)};
	AdjustWindowRect(&r, style, FALSE);
	_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, kClassName, kAppTitle, style,
		CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
		nullptr, nullptr, _hInst, this);
	if (!_hwnd)
		return false;
	ShowWindow(_hwnd, nCmdShow);
	UpdateWindow(_hwnd);
	return true;
}

LRESULT CALLBACK MainWindow::wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
	MainWindow *self;
	if (msg == WM_NCCREATE) {
		self = static_cast<MainWindow *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
		SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		self->_hwnd = h;
	} else {
		self = reinterpret_cast<MainWindow *>(GetWindowLongPtrW(h, GWLP_USERDATA));
	}
	if (self)
		return self->handle(msg, wp, lp);
	return DefWindowProcW(h, msg, wp, lp);
}

/* ------------------------------------------------------------------------ */
/* Controls                                                                 */
/* ------------------------------------------------------------------------ */

void MainWindow::createControls()
{
	NONCLIENTMETRICSW ncm = {};
	ncm.cbSize = sizeof(ncm);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
	LOGFONTW lf = ncm.lfMessageFont;
	lf.lfHeight = -MulDiv(10, _dpi, 72);
	_fontUi = CreateFontIndirectW(&lf);
	LOGFONTW lfs = lf;
	lfs.lfHeight = -MulDiv(8, _dpi, 72);
	_fontSmall = CreateFontIndirectW(&lfs);
	LOGFONTW lft = lf;
	lft.lfHeight = -MulDiv(17, _dpi, 72);
	lft.lfWeight = FW_BOLD;
	_fontTitle = CreateFontIndirectW(&lft);
	LOGFONTW lfb = lf;
	lfb.lfHeight = -MulDiv(13, _dpi, 72);
	lfb.lfWeight = FW_BOLD;
	_fontBig = CreateFontIndirectW(&lfb);
	LOGFONTW lfm = {};
	lfm.lfHeight = -MulDiv(9, _dpi, 72);
	lfm.lfCharSet = DEFAULT_CHARSET;
	wcscpy(lfm.lfFaceName, L"Consolas");
	_fontMono = CreateFontIndirectW(&lfm);

	auto mk = [&](const wchar_t *cls, const wchar_t *text, DWORD style, int id,
			DWORD exStyle = 0) {
		HWND c = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
			0, 0, 10, 10, _hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), _hInst, nullptr);
		SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(_fontUi), TRUE);
		return c;
	};

	_title = mk(L"STATIC", kAppTitle, SS_CENTER, 0);
	SendMessageW(_title, WM_SETFONT, reinterpret_cast<WPARAM>(_fontTitle), TRUE);

	_lblBoard = mk(L"STATIC", L"Board:", SS_LEFT, 0);
	_cbBoard = mk(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_BOARD);
	_lblCable = mk(L"STATIC", L"Cable:", SS_LEFT, 0);
	_cbCable = mk(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_CABLE);
	_lblFile = mk(L"STATIC", L"Bitstream:", SS_LEFT, 0);
	_edFile = mk(WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_FILE, WS_EX_CLIENTEDGE);
	SendMessageW(_edFile, EM_SETCUEBANNER, TRUE,
		reinterpret_cast<LPARAM>(L"Click Browse... or drop a .bit file here"));
	_btnBrowse = mk(WC_BUTTONW, L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, IDC_BROWSE);
	_lblMode = mk(L"STATIC", L"Programming mode:", SS_LEFT, 0);
	_rbSram = mk(WC_BUTTONW, L"FPGA SRAM  (temporary, lost when power is off)",
		BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, IDC_SRAM);
	_rbFlash = mk(WC_BUTTONW, L"Flash  (permanent, loaded at every power-on)",
		BS_AUTORADIOBUTTON, IDC_FLASH);
	_btnDetect = mk(WC_BUTTONW, L"&Detect Board", BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, IDC_DETECT);

	_sep1 = mk(L"STATIC", L"", SS_ETCHEDHORZ, 0);
	_lblStatus = mk(L"STATIC", L"Status:", SS_LEFT, 0);
	_edStatus = mk(WC_EDITW, L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
		IDC_STATUS, WS_EX_STATICEDGE);
	_progress = mk(PROGRESS_CLASSW, L"", PBS_SMOOTH, IDC_PROGRESS);
	SendMessageW(_progress, PBM_SETRANGE32, 0, 1000);

	_btnProgram = mk(WC_BUTTONW, L"PROGRAM FPGA", BS_PUSHBUTTON | WS_TABSTOP, IDC_PROGRAM);
	SendMessageW(_btnProgram, WM_SETFONT, reinterpret_cast<WPARAM>(_fontBig), TRUE);

	_sep2 = mk(L"STATIC", L"", SS_ETCHEDHORZ, 0);
	_btnDetails = mk(WC_BUTTONW, L"&Show Details >>", BS_PUSHBUTTON | WS_TABSTOP, IDC_DETAILS);
	_btnAbout = mk(WC_BUTTONW, L"About", BS_PUSHBUTTON | WS_TABSTOP, IDC_ABOUT);
	_footer = mk(L"STATIC", L"ICSLab  \x2022  powered by openFPGALoader", SS_CENTER, 0);
	SendMessageW(_footer, WM_SETFONT, reinterpret_cast<WPARAM>(_fontSmall), TRUE);

	_edLog = mk(WC_EDITW, L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
		WS_VSCROLL | WS_HSCROLL, IDC_LOG, WS_EX_CLIENTEDGE);
	SendMessageW(_edLog, WM_SETFONT, reinterpret_cast<WPARAM>(_fontMono), TRUE);
	SendMessageW(_edLog, EM_SETLIMITTEXT, 8 * 1024 * 1024, 0);
	ShowWindow(_edLog, SW_HIDE);

	SendMessageW(_settings.flash ? _rbFlash : _rbSram, BM_SETCHECK, BST_CHECKED, 0);

	layout();
}

void MainWindow::layout()
{
	const int W = kWidth;
	const int x0 = kMargin;
	const int x1 = kMargin + kLabelW;
	const int wRight = W - x1 - kMargin;
	auto place = [&](HWND h, int x, int y, int w, int hgt) {
		MoveWindow(h, S(x), S(y), S(w), S(hgt), TRUE);
	};

	int y = 12;
	place(_title, x0, y, W - 2 * x0, 32);
	y += 46;

	place(_lblBoard, x0, y + 4, kLabelW, 20);
	place(_cbBoard, x1, y, wRight, 400);
	y += 34;
	place(_lblCable, x0, y + 4, kLabelW, 20);
	place(_cbCable, x1, y, wRight, 400);
	y += 34;
	place(_lblFile, x0, y + 4, kLabelW, 20);
	place(_edFile, x1, y, wRight - 96, 26);
	place(_btnBrowse, W - kMargin - 88, y - 1, 88, 28);
	y += 36;
	place(_lblMode, x0, y + 2, kLabelW, 20);
	place(_rbSram, x1, y, wRight, 22);
	y += 22;
	place(_rbFlash, x1, y, wRight, 22);
	y += 30;
	place(_btnDetect, x1, y, 150, 30);
	y += 40;

	place(_sep1, x0, y, W - 2 * x0, 2);
	y += 8;
	place(_lblStatus, x0, y, kLabelW, 20);
	y += 20;
	place(_edStatus, x0, y, W - 2 * x0, 164);
	y += 168;
	place(_progress, x0, y, W - 2 * x0, 18);
	y += 28;

	place(_btnProgram, (W - 280) / 2, y, 280, 46);
	y += 56;

	place(_sep2, x0, y, W - 2 * x0, 2);
	y += 8;
	place(_btnDetails, x0, y, 130, 26);
	place(_footer, x0 + 140, y + 5, W - 2 * x0 - 280, 18);
	place(_btnAbout, W - kMargin - 80, y, 80, 26);
	y += 36;
	_baseHeight = y; // log panel starts here
	place(_edLog, x0, y, W - 2 * x0, kLogHeight - 12);
}

void MainWindow::resizeToContent()
{
	DWORD style = static_cast<DWORD>(GetWindowLongPtrW(_hwnd, GWL_STYLE));
	int logH = _detailsVisible ? kLogHeight : 0;
	RECT r = {0, 0, S(kWidth), S(_baseHeight + logH)};
	AdjustWindowRect(&r, style, FALSE);

	// Small screens (e.g. 1366x768 laptops): shrink the log, never the controls.
	RECT work = {};
	if (_detailsVisible && SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
		int excess = (r.bottom - r.top) - (work.bottom - work.top);
		if (excess > 0) {
			int newLog = std::max(90, logH - MulDiv(excess, 96, _dpi));
			r.bottom -= S(logH - newLog);
			logH = newLog;
		}
	}
	if (_detailsVisible)
		MoveWindow(_edLog, S(kMargin), S(_baseHeight), S(kWidth - 2 * kMargin), S(logH - 12), TRUE);
	SetWindowPos(_hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top,
		SWP_NOMOVE | SWP_NOZORDER);
}

/* ------------------------------------------------------------------------ */
/* Board / cable lists                                                      */
/* ------------------------------------------------------------------------ */

void MainWindow::populateBoards()
{
	SendMessageW(_cbBoard, CB_RESETCONTENT, 0, 0);
	_boardEntries.clear();

	auto add = [&](const std::string &label, const std::string &name, bool sep = false) {
		BoardEntry e;
		e.name = name;
		e.separator = sep;
		_boardEntries.push_back(e);
		SendMessageW(_cbBoard, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(label).c_str()));
	};
	auto partOf = [&](const std::string &name) -> std::string {
		for (const auto &b : _boards)
			if (b.name == name)
				return b.fpgaPart;
		return std::string();
	};

	add(kGenericLabel, "");
	std::set<std::string> known;
	for (const auto &b : _boards)
		known.insert(b.name);
	for (const auto &lb : _labBoards) {
		// Skip lab entries the backend does not know (when the list loaded)
		if (!_boards.empty() && !known.count(lb.name))
			continue;
		std::string label = lb.label;
		if (label != lb.name)
			label += "   [" + lb.name + "]";
		std::string part = partOf(lb.name);
		if (!part.empty())
			label += "   " + part;
		add(label, lb.name);
	}
	if (!_boards.empty()) {
		add(kSeparator, "", true);
		for (const auto &b : _boards) {
			std::string label = b.name;
			if (!b.fpgaPart.empty())
				label += "   (" + b.fpgaPart + ")";
			add(label, b.name);
		}
	}

	// Restore saved board (first occurrence)
	int sel = 0;
	for (size_t i = 0; i < _boardEntries.size(); ++i) {
		if (!_boardEntries[i].separator && _boardEntries[i].name == _settings.board) {
			sel = static_cast<int>(i);
			break;
		}
	}
	SendMessageW(_cbBoard, CB_SETCURSEL, sel, 0);
	SendMessageW(_cbBoard, CB_SETDROPPEDWIDTH, S(kWidth - 2 * kMargin - kLabelW + 60), 0);
}

void MainWindow::populateCables()
{
	SendMessageW(_cbCable, CB_RESETCONTENT, 0, 0);
	SendMessageW(_cbCable, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(kAutoCable).c_str()));
	int sel = 0;
	for (size_t i = 0; i < _cables.size(); ++i) {
		SendMessageW(_cbCable, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(_cables[i]).c_str()));
		if (_cables[i] == _settings.cable)
			sel = static_cast<int>(i) + 1;
	}
	SendMessageW(_cbCable, CB_SETCURSEL, sel, 0);
}

std::string MainWindow::selectedBoard() const
{
	LRESULT i = SendMessageW(_cbBoard, CB_GETCURSEL, 0, 0);
	if (i < 0 || static_cast<size_t>(i) >= _boardEntries.size())
		return std::string();
	return _boardEntries[static_cast<size_t>(i)].name;
}

std::string MainWindow::selectedCable() const
{
	LRESULT i = SendMessageW(_cbCable, CB_GETCURSEL, 0, 0);
	if (i <= 0 || static_cast<size_t>(i) > _cables.size())
		return std::string();
	return _cables[static_cast<size_t>(i) - 1];
}

std::string MainWindow::bitstreamPath() const
{
	return trim(windowText(_edFile));
}

bool MainWindow::flashSelected() const
{
	return SendMessageW(_rbFlash, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void MainWindow::setBitstream(const std::string &path)
{
	SetWindowTextW(_edFile, widen(path).c_str());
	int len = GetWindowTextLengthW(_edFile);
	SendMessageW(_edFile, EM_SETSEL, len, len);
	SendMessageW(_edFile, EM_SCROLLCARET, 0, 0);
	// Only the folder is persisted here; board/cable are saved when changed
	// (the lists may still be loading).
	_settings.lastDir = fileDirName(path);
	_settings.save(_settingsPath);
	showReadySummary();
}

/* ------------------------------------------------------------------------ */
/* Status helpers                                                           */
/* ------------------------------------------------------------------------ */

void MainWindow::setStatus(const std::string &text, Tone tone)
{
	_tone = tone;
	SetWindowTextW(_edStatus, widen(text).c_str());
	InvalidateRect(_edStatus, nullptr, TRUE);
}

std::string MainWindow::targetSummary() const
{
	std::string board = selectedBoard();
	std::string file = bitstreamPath();
	std::string s = "Board:   " + (board.empty() ? std::string(kGenericLabel)
		: friendlyBoardName(board) + (friendlyBoardName(board) != board ? " [" + board + "]" : ""));
	std::string cable = selectedCable();
	if (!cable.empty())
		s += "\r\nCable:   " + cable;
	s += "\r\nFile:    " + (file.empty() ? std::string("(no bitstream selected)") : fileBaseName(file));
	s += std::string("\r\nTarget:  ") + (flashSelected() ? "Flash (permanent)" : "FPGA SRAM (temporary)");
	return s;
}

void MainWindow::showReadySummary()
{
	if (_busy)
		return;
	if (_backend.empty()) {
		setStatus(formatFailure(backendMissingInfo(_exeDir)), Tone::Bad);
		return;
	}
	setStatus(targetSummary(), Tone::Neutral);
	setProgressMarquee(false);
	setProgress(0);
}

void MainWindow::appendLog(const std::string &text)
{
	std::wstring w = widen(text + "\r\n");
	int len = GetWindowTextLengthW(_edLog);
	SendMessageW(_edLog, EM_SETSEL, len, len);
	SendMessageW(_edLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(w.c_str()));
}

void MainWindow::setProgressMarquee(bool on)
{
	if (on == _marquee)
		return;
	_marquee = on;
	LONG_PTR style = GetWindowLongPtrW(_progress, GWL_STYLE);
	if (on) {
		SetWindowLongPtrW(_progress, GWL_STYLE, style | PBS_MARQUEE);
		SendMessageW(_progress, PBM_SETMARQUEE, TRUE, 30);
	} else {
		SendMessageW(_progress, PBM_SETMARQUEE, FALSE, 0);
		SetWindowLongPtrW(_progress, GWL_STYLE, style & ~static_cast<LONG_PTR>(PBS_MARQUEE));
		SendMessageW(_progress, PBM_SETPOS, 0, 0);
	}
}

void MainWindow::setProgress(double percent)
{
	setProgressMarquee(false);
	SendMessageW(_progress, PBM_SETPOS, static_cast<WPARAM>(percent * 10.0), 0);
}

void MainWindow::setBusy(bool busy)
{
	_busy = busy;
	const bool haveBackend = !_backend.empty();
	for (HWND h : {_cbBoard, _cbCable, _edFile, _btnBrowse, _rbSram, _rbFlash})
		EnableWindow(h, !busy);
	EnableWindow(_btnDetect, !busy && haveBackend);
	EnableWindow(_btnProgram, !busy && haveBackend);
}

void MainWindow::saveSettings()
{
	_settings.board = selectedBoard();
	_settings.cable = selectedCable();
	_settings.flash = flashSelected();
	_settings.save(_settingsPath);
}

/* ------------------------------------------------------------------------ */
/* Worker thread                                                            */
/* ------------------------------------------------------------------------ */

void MainWindow::startWorker(Task task, std::vector<std::vector<std::string>> runs,
	std::string detectBoard, std::string detectCable)
{
	if (_busy || _backend.empty())
		return;
	if (_worker) {
		CloseHandle(_worker);
		_worker = nullptr;
	}
	auto *job = new WorkerJob{_hwnd, task, _backend, std::move(runs),
		std::move(detectBoard), std::move(detectCable)};
	_task = task;
	setBusy(true);
	_worker = CreateThread(nullptr, 0, workerMain, job, 0, nullptr);
	if (!_worker) {
		delete job;
		setBusy(false);
		setStatus(formatFailure(launchFailedInfo(describeWin32Error(GetLastError()))), Tone::Bad);
	}
}

DWORD WINAPI MainWindow::workerMain(LPVOID param)
{
	std::unique_ptr<WorkerJob> job(static_cast<WorkerJob *>(param));
	auto *done = new WorkerDone;
	done->task = job->task;

	auto run = [&](const std::vector<std::string> &args, bool streamToUi) {
		if (streamToUi) {
			auto *l = new WorkerLine{"> openFPGALoader.exe " +
				buildCommandLine("x", args).substr(4)};
			if (!PostMessageW(job->hwnd, WM_APP_LINE, 0, reinterpret_cast<LPARAM>(l)))
				delete l;
		}
		ProcessResult r = runProcess(job->exe, args, [&](const std::string &line) {
			if (!streamToUi)
				return;
			auto *l = new WorkerLine{line};
			if (!PostMessageW(job->hwnd, WM_APP_LINE, 0, reinterpret_cast<LPARAM>(l)))
				delete l;
		});
		done->runs.push_back(r);
		return r;
	};

	if (job->task == Task::Detect) {
		// 1. USB scan, 2. JTAG chain detection
		ProcessResult scan = run(buildScanUsbArgs(), true);
		std::string cable = job->detectCable;
		if (scan.launched && job->detectBoard.empty() && cable.empty()) {
			auto probes = parseScanUsb(scan.output);
			for (const auto &p : probes) {
				cable = cableForProbeType(p.probeType);
				if (!cable.empty())
					break;
			}
			if (probes.empty()) {
				done->summary = "nocable";
			} else if (!cable.empty()) {
				done->summary = "cable:" + cable;
			}
		}
		if (scan.launched && done->summary != "nocable")
			run(buildDetectArgs(job->detectBoard, cable), true);
	} else {
		for (const auto &args : job->runs) {
			ProcessResult r = run(args, job->task == Task::Program);
			if (!r.launched)
				break;
		}
	}

	done->ok = !done->runs.empty() && done->runs.back().launched &&
		done->runs.back().exitCode == 0;
	if (!PostMessageW(job->hwnd, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(done)))
		delete done;
	return 0;
}

void MainWindow::onWorkerLine(WorkerLine *line)
{
	std::unique_ptr<WorkerLine> l(line);
	if (!_hwnd)
		return;
	appendLog(l->text);

	if (_task != Task::Program)
		return;
	ProgressUpdate pu;
	if (parseProgressLine(l->text, pu)) {
		std::string phase = describePhase(pu.phase);
		char pct[16];
		snprintf(pct, sizeof(pct), "%.0f%%", pu.percent);
		setProgress(pu.percent);
		_lastPhase = phase;
		setStatus(targetSummary() + "\r\n\r\n" + phase + "...  " + pct, Tone::Busy);
	}
}

void MainWindow::onWorkerDone(WorkerDone *d)
{
	std::unique_ptr<WorkerDone> done(d);
	setBusy(false);
	setProgressMarquee(false);
	const Task task = done->task;
	_task = Task::None;

	// Any launch failure is reported the same way for all tasks.
	for (const auto &r : done->runs) {
		if (!r.launched) {
			setProgress(0);
			auto info = launchFailedInfo(describeWin32Error(r.launchError));
			appendLog("ERROR: " + info.explanation);
			setStatus(formatFailure(info), Tone::Bad);
			return;
		}
	}

	if (task == Task::LoadLists) {
		if (done->runs.size() >= 3) {
			_backendVersion = trim(stripAnsi(done->runs[0].output));
			_boards = parseBoardList(done->runs[1].output);
			_cables = parseCableList(done->runs[2].output);
		}
		populateBoards();
		populateCables();
		appendLog("Backend: " + _backend);
		appendLog("Version: " + (_backendVersion.empty() ? "unknown" : _backendVersion));
		appendLog("Boards: " + std::to_string(_boards.size()) + ", cables: " +
			std::to_string(_cables.size()));
		if (_boards.empty())
			appendLog("WARNING: could not read the board list from openFPGALoader; "
				"showing the lab board list only.");
		showReadySummary();
		return;
	}

	if (task == Task::Detect) {
		setProgress(0);
		const auto &scan = done->runs[0];
		auto probes = parseScanUsb(scan.output);
		std::string s;
		if (probes.empty()) {
			s += "USB programmer: none found\r\n";
		} else {
			for (const auto &p : probes) {
				s += "USB programmer: " + (p.product.empty() || p.product == "none" ?
					p.probeType : p.product) + "  (" + p.probeType + ", " + p.vidPid + ")\r\n";
			}
		}
		if (done->runs.size() < 2) {
			FailureInfo info = classifyResult(1, scan.output);
			if (info.kind == FailureKind::Unknown)
				info = classifyResult(1, "No USB devices found");
			setStatus(s + "\r\n" + formatFailure(info), Tone::Bad);
			return;
		}
		const auto &det = done->runs[1];
		std::string board = selectedBoard();
		std::string cable = selectedCable();
		if (cable.empty() && done->summary.compare(0, 6, "cable:") == 0)
			cable = done->summary.substr(6);
		if (cable.empty() && !board.empty()) {
			for (const auto &b : _boards)
				if (b.name == board)
					cable = b.cable;
		}
		s += "JTAG cable:     " + (cable.empty() ? std::string("ft2232 (default)") : cable) + "\r\n";

		auto devices = parseDetect(det.output);
		if (det.exitCode != 0 || devices.empty()) {
			FailureInfo info = classifyResult(det.exitCode ? det.exitCode : 1, det.output);
			if (devices.empty() && det.exitCode == 0)
				info = classifyResult(1, "Error: no device found");
			setStatus(s + "\r\n" + formatFailure(info), Tone::Bad);
			return;
		}
		for (const auto &dv : devices) {
			if (!dv.model.empty()) {
				s += "FPGA device:    " + dv.model + "  (" + dv.manufacturer + " " + dv.family + ")\r\n";
			} else {
				s += "JTAG device:    " + (dv.type.empty() ? std::string("unknown") : dv.type) + "\r\n";
			}
			s += "IDCODE:         " + hex32(dv.idcode) + "\r\n";
		}
		if (!board.empty()) {
			s += "Board:          " + friendlyBoardName(board) + " (selected)";
		} else {
			std::vector<std::string> matches;
			for (const auto &dv : devices) {
				auto m = boardsMatchingModel(_boards, dv.model);
				matches.insert(matches.end(), m.begin(), m.end());
			}
			// Prefer lab boards in the suggestion
			std::vector<std::string> lab;
			for (const auto &m : matches)
				for (const auto &lb : _labBoards)
					if (lb.name == m)
						lab.push_back(m);
			const auto &show = lab.empty() ? matches : lab;
			if (show.empty()) {
				s += "Board:          not identified (select it in the list)";
			} else {
				s += "Possible board: ";
				for (size_t i = 0; i < show.size() && i < 5; ++i)
					s += (i ? ", " : "") + show[i];
				if (show.size() > 5)
					s += ", ...";
			}
		}
		setStatus(s, Tone::Good);
		return;
	}

	if (task == Task::Program) {
		const auto &r = done->runs.back();
		if (r.exitCode == 0) {
			setProgress(100);
			std::string msg = flashSelected() ?
				"\xE2\x9C\x93 Flash programmed successfully.\r\nThe design will load at every power-on." :
				"\xE2\x9C\x93 FPGA programmed successfully.";
			setStatus(msg + "\r\n\r\n" + targetSummary(), Tone::Good);
			appendLog("== SUCCESS (exit code 0) ==");
		} else {
			setProgress(0);
			FailureInfo info = classifyResult(r.exitCode, r.output);
			appendLog("== FAILED (exit code " + std::to_string(r.exitCode) + ") ==");
			setStatus(formatFailure(info) + "\r\n\r\nFull log: click \"Show Details\".",
				Tone::Bad);
		}
	}
}

/* ------------------------------------------------------------------------ */
/* Actions                                                                  */
/* ------------------------------------------------------------------------ */

void MainWindow::onBoardChanged()
{
	LRESULT i = SendMessageW(_cbBoard, CB_GETCURSEL, 0, 0);
	if (i >= 0 && static_cast<size_t>(i) < _boardEntries.size() &&
			_boardEntries[static_cast<size_t>(i)].separator) {
		// Separator is not selectable: move to the next real entry
		SendMessageW(_cbBoard, CB_SETCURSEL, i + 1 < static_cast<LRESULT>(_boardEntries.size()) ? i + 1 : 0, 0);
	}
	saveSettings();
	showReadySummary();
}

void MainWindow::onBrowse()
{
	wchar_t file[32768] = L"";
	std::string filter = openFileFilter();
	std::wstring wfilter = widen(filter);
	std::wstring initDir = widen(_settings.lastDir);
	std::string current = bitstreamPath();
	if (!current.empty())
		initDir = widen(fileDirName(current));

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = _hwnd;
	ofn.lpstrFilter = wfilter.c_str();
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file) / sizeof(file[0]);
	ofn.lpstrInitialDir = initDir.empty() ? nullptr : initDir.c_str();
	ofn.lpstrTitle = L"Select FPGA bitstream";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
	if (GetOpenFileNameW(&ofn))
		setBitstream(narrow(file));
}

void MainWindow::onDetect()
{
	if (_busy)
		return;
	appendLog("");
	appendLog("== Detect board ==");
	setStatus("Detecting USB programmer and FPGA...", Tone::Busy);
	setProgressMarquee(true);
	startWorker(Task::Detect, {}, selectedBoard(), selectedCable());
}

void MainWindow::onProgram()
{
	if (_busy || _backend.empty())
		return;

	const std::string path = bitstreamPath();
	switch (checkBitstream(path, fileExists)) {
	case BitstreamCheck::Empty:
		MessageBoxW(_hwnd, L"Please select a bitstream file first (Browse...).",
			kAppTitle, MB_ICONINFORMATION);
		return;
	case BitstreamCheck::NotFound:
		MessageBoxW(_hwnd, widen("The bitstream file does not exist:\n\n" + path).c_str(),
			kAppTitle, MB_ICONERROR);
		return;
	case BitstreamCheck::UnknownExtension:
		if (MessageBoxW(_hwnd, widen("\"" + fileBaseName(path) + "\" does not have a usual "
				"bitstream extension (.bit, .bin, .svf, .jed, .hex, ...).\n\n"
				"Program it anyway?").c_str(), kAppTitle,
				MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
			return;
		break;
	case BitstreamCheck::Ok:
		break;
	}

	std::string safePath = backendSafePath(path);
	if (safePath.empty()) {
		MessageBoxW(_hwnd, widen("The file path contains characters that "
			"openFPGALoader cannot handle:\n\n" + path + "\n\n"
			"Copy the file to a folder whose name uses only English letters "
			"(for example C:\\FPGA\\) and select it again.").c_str(),
			kAppTitle, MB_ICONERROR);
		return;
	}

	ProgramRequest req;
	req.board = selectedBoard();
	req.cable = selectedCable();
	req.bitstream = safePath;
	req.target = flashSelected() ? ProgramTarget::Flash : ProgramTarget::Sram;

	if (req.target == ProgramTarget::Flash) {
		std::string msg = "The configuration flash of the board will be ERASED and "
			"overwritten with this bitstream.\n\n" + targetSummary() + "\n\nContinue?";
		// targetSummary uses \r\n which MessageBox renders fine.
		if (MessageBoxW(_hwnd, widen(msg).c_str(), L"Confirm flash programming",
				MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
			return;
	}

	_lastPhase.clear();
	appendLog("");
	appendLog("== Program: " + fileBaseName(path) + " -> " +
		(req.target == ProgramTarget::Flash ? "Flash" : "FPGA SRAM") + " ==");
	setStatus(targetSummary() + "\r\n\r\nStarting openFPGALoader...", Tone::Busy);
	setProgressMarquee(true);
	saveSettings();
	startWorker(Task::Program, {buildProgramArgs(req)});
}

void MainWindow::onToggleDetails()
{
	_detailsVisible = !_detailsVisible;
	ShowWindow(_edLog, _detailsVisible ? SW_SHOW : SW_HIDE);
	SetWindowTextW(_btnDetails, _detailsVisible ? L"<< Hide &Details" : L"&Show Details >>");
	resizeToContent();
	if (_detailsVisible) {
		int len = GetWindowTextLengthW(_edLog);
		SendMessageW(_edLog, EM_SETSEL, len, len);
		SendMessageW(_edLog, EM_SCROLLCARET, 0, 0);
	}
}

void MainWindow::onAbout()
{
	std::string text =
		"ICSLab FPGA Programmer " ICSLAB_VERSION_STR "\n"
		"Simple FPGA programming utility powered by openFPGALoader.\n"
		"Copyright (C) ICSLab\n\n"
		"ICSLab FPGA Programmer uses openFPGALoader as its FPGA programming backend.\n"
		"openFPGALoader is an independent open-source project\n"
		"(Apache License 2.0, Copyright (C) Gwenhael Goavec-Merou and contributors):\n"
		"https://github.com/trabucayre/openFPGALoader\n\n"
		"Backend: " + (_backend.empty() ? std::string("NOT FOUND") : _backend) + "\n"
		"Backend version: " + (_backendVersion.empty() ? std::string("unknown") : _backendVersion) + "\n\n"
		"License notices: see the LICENSES folder and README-licenses.txt.";
	MessageBoxW(_hwnd, widen(text).c_str(), L"About ICSLab FPGA Programmer", MB_ICONINFORMATION);
}

/* ------------------------------------------------------------------------ */
/* Message handling                                                         */
/* ------------------------------------------------------------------------ */

LRESULT MainWindow::handle(UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_CREATE:
		createControls();
		resizeToContent();
		populateBoards();
		populateCables();
		setBusy(false);
		if (!_settings.lastDir.empty())
			appendLog("Last bitstream folder: " + _settings.lastDir);
		// A file given on the command line (e.g. a .bit dropped on the exe
		// icon) only pre-fills the field: programming still needs a click.
		if (!_initialBitstream.empty() && fileExists(_initialBitstream))
			setBitstream(_initialBitstream);
		if (_backend.empty()) {
			appendLog("ERROR: openFPGALoader.exe not found. Looked for:");
			for (const auto &c : backendCandidates(_exeDir))
				appendLog("  " + c);
			showReadySummary();
		} else {
			setStatus("Loading board list...", Tone::Busy);
			startWorker(Task::LoadLists, {buildVersionArgs(), buildListBoardsArgs(),
				buildListCablesArgs()});
		}
		return 0;

	case WM_COMMAND: {
		const int id = LOWORD(wp);
		const int code = HIWORD(wp);
		switch (id) {
		case IDC_BOARD:
			if (code == CBN_SELCHANGE)
				onBoardChanged();
			break;
		case IDC_CABLE:
			if (code == CBN_SELCHANGE) {
				saveSettings();
				showReadySummary();
			}
			break;
		case IDC_FILE:
			if (code == EN_CHANGE)
				showReadySummary();
			break;
		case IDC_SRAM:
		case IDC_FLASH:
			if (code == BN_CLICKED) {
				saveSettings();
				showReadySummary();
			}
			break;
		case IDC_BROWSE: onBrowse(); break;
		case IDC_DETECT: onDetect(); break;
		case IDC_PROGRAM: onProgram(); break;
		case IDC_DETAILS: onToggleDetails(); break;
		case IDC_ABOUT: onAbout(); break;
		// IDOK (Enter key) is deliberately ignored: programming only
		// starts from an explicit click on PROGRAM FPGA.
		}
		return 0;
	}

	case WM_DROPFILES: {
		HDROP drop = reinterpret_cast<HDROP>(wp);
		if (!_busy) {
			UINT n = DragQueryFileW(drop, 0, nullptr, 0);
			std::wstring f(n + 1, L'\0');
			DragQueryFileW(drop, 0, &f[0], n + 1);
			f.resize(n);
			setBitstream(narrow(f));
		}
		DragFinish(drop);
		return 0;
	}

	case WM_CTLCOLORSTATIC: {
		HDC dc = reinterpret_cast<HDC>(wp);
		HWND ctl = reinterpret_cast<HWND>(lp);
		if (ctl == _edStatus) {
			COLORREF c = GetSysColor(COLOR_WINDOWTEXT);
			if (_tone == Tone::Good) c = kGreen;
			else if (_tone == Tone::Bad) c = kRed;
			else if (_tone == Tone::Busy) c = kBlue;
			SetTextColor(dc, c);
			SetBkColor(dc, RGB(0xf4, 0xf6, 0xf9));
			return reinterpret_cast<LRESULT>(_statusBrush);
		}
		if (ctl == _edLog) {
			SetBkColor(dc, GetSysColor(COLOR_WINDOW));
			return reinterpret_cast<LRESULT>(_bgBrush);
		}
		SetBkMode(dc, TRANSPARENT);
		if (ctl == _title)
			SetTextColor(dc, kBlue);
		else if (ctl == _footer)
			SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
		return reinterpret_cast<LRESULT>(_bgBrush);
	}

	case WM_APP_LINE:
		onWorkerLine(reinterpret_cast<WorkerLine *>(lp));
		return 0;

	case WM_APP_DONE:
		onWorkerDone(reinterpret_cast<WorkerDone *>(lp));
		return 0;

	case WM_CLOSE:
		if (_busy && _task == Task::Program) {
			MessageBoxW(_hwnd, L"Programming is in progress.\n\n"
				L"Please wait until it finishes before closing the application.",
				kAppTitle, MB_ICONWARNING);
			return 0;
		}
		DestroyWindow(_hwnd);
		return 0;

	case WM_DESTROY:
		saveSettings();
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(_hwnd, msg, wp, lp);
}

} // namespace icslab
