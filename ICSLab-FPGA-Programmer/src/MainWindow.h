// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - main window (Win32).

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "OpenFPGALoaderBackend.h"
#include "ProcessRunner.h"
#include "Settings.h"

namespace icslab {

class MainWindow {
public:
	MainWindow(HINSTANCE hInst);
	~MainWindow();
	bool create(int nCmdShow, const std::string &initialBitstream = std::string());
	HWND hwnd() const { return _hwnd; }

private:
	enum class Task { None, LoadLists, Detect, Program };

	struct BoardEntry {
		std::string name;   // "" = generic
		bool separator = false;
	};

	// Messages posted from the worker thread to the UI thread.
	struct WorkerLine { std::string text; };
	struct WorkerDone {
		Task task = Task::None;
		std::vector<ProcessResult> runs;
		std::string summary;  // detect summary
		bool ok = false;
	};

	static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
	LRESULT handle(UINT, WPARAM, LPARAM);

	void createControls();
	void layout();
	void resizeToContent();
	int S(int v) const { return MulDiv(v, _dpi, 96); }

	void populateBoards();
	void populateCables();
	std::string selectedBoard() const;
	std::string selectedCable() const;
	std::string bitstreamPath() const;
	bool flashSelected() const;
	void setBitstream(const std::string &path);

	void onBoardChanged();
	void onBrowse();
	void onDetect();
	void onProgram();
	void onToggleDetails();
	void onAbout();
	void onWorkerLine(WorkerLine *line);
	void onWorkerDone(WorkerDone *done);

	void startWorker(Task task, std::vector<std::vector<std::string>> runs,
		std::string detectBoard = std::string(), std::string detectCable = std::string());
	static DWORD WINAPI workerMain(LPVOID param);

	void setBusy(bool busy);
	enum class Tone { Neutral, Busy, Good, Bad };
	void setStatus(const std::string &text, Tone tone);
	void showReadySummary();
	std::string targetSummary() const;
	void appendLog(const std::string &text);
	void setProgressMarquee(bool on);
	void setProgress(double percent);
	void saveSettings();

	HINSTANCE _hInst;
	HWND _hwnd = nullptr;
	int _dpi = 96;
	int _baseHeight = 560;

	HWND _title = nullptr;
	HWND _lblBoard = nullptr, _cbBoard = nullptr;
	HWND _lblCable = nullptr, _cbCable = nullptr;
	HWND _lblFile = nullptr, _edFile = nullptr, _btnBrowse = nullptr;
	HWND _lblMode = nullptr, _rbSram = nullptr, _rbFlash = nullptr;
	HWND _btnDetect = nullptr;
	HWND _sep1 = nullptr, _lblStatus = nullptr, _edStatus = nullptr, _progress = nullptr;
	HWND _btnProgram = nullptr;
	HWND _sep2 = nullptr, _btnDetails = nullptr, _btnAbout = nullptr, _footer = nullptr;
	HWND _edLog = nullptr;

	HFONT _fontUi = nullptr, _fontTitle = nullptr, _fontBig = nullptr;
	HFONT _fontMono = nullptr, _fontSmall = nullptr;
	HBRUSH _bgBrush = nullptr, _statusBrush = nullptr;

	bool _detailsVisible = false;
	bool _busy = false;
	bool _marquee = false;
	Task _task = Task::None;
	Tone _tone = Tone::Neutral;
	std::string _lastPhase;
	std::string _initialBitstream;

	std::string _exeDir;
	std::string _backend;      // "" when missing
	std::string _backendVersion;
	std::vector<BoardInfo> _boards;
	std::vector<std::string> _cables;
	std::vector<LabBoard> _labBoards;
	std::vector<BoardEntry> _boardEntries;

	Settings _settings;
	std::string _settingsPath;

	// worker state
	HANDLE _worker = nullptr;
	struct WorkerJob {
		HWND hwnd;
		Task task;
		std::string exe;
		std::vector<std::vector<std::string>> runs;
		std::string detectBoard, detectCable;
	};
};

} // namespace icslab
