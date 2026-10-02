// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - openFPGALoader backend helpers.
//
// Everything in this file is platform independent (no <windows.h>) so it can
// be unit tested on any host. All strings are UTF-8.
//
// The GUI never re-implements FPGA programming: it only builds command lines
// for openFPGALoader and interprets its textual output.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace icslab {

/* ------------------------------------------------------------------------ */
/* Boards / cables as reported by "openFPGALoader --list-boards/--list-cables" */
/* ------------------------------------------------------------------------ */

struct BoardInfo {
	std::string name;     // value for "-b"
	std::string cable;    // default cable ("" when "Undefined")
	std::string fpgaPart; // fpga part ("" when "Undefined")
};

std::vector<BoardInfo> parseBoardList(const std::string &listBoardsOutput);
std::vector<std::string> parseCableList(const std::string &listCablesOutput);

// Lab board list: one entry per line, "boardName" or "boardName = Friendly
// name". '#' starts a comment. Unknown names are kept (the backend decides).
struct LabBoard {
	std::string name;
	std::string label;
};
std::vector<LabBoard> parseLabBoards(const std::string &text);
std::vector<LabBoard> defaultLabBoards();

// Human friendly label for a board, e.g. "Digilent Basys 3".
std::string friendlyBoardName(const std::string &board);

/* ------------------------------------------------------------------------ */
/* Command generation                                                       */
/* ------------------------------------------------------------------------ */

enum class ProgramTarget { Sram, Flash };

struct ProgramRequest {
	std::string board;     // empty: generic JTAG (no -b)
	std::string cable;     // empty: board default / backend default
	std::string bitstream; // full path to bitstream
	ProgramTarget target = ProgramTarget::Sram;
};

std::vector<std::string> buildListBoardsArgs();
std::vector<std::string> buildListCablesArgs();
std::vector<std::string> buildVersionArgs();
std::vector<std::string> buildScanUsbArgs();
std::vector<std::string> buildDetectArgs(const std::string &board, const std::string &cable);
std::vector<std::string> buildProgramArgs(const ProgramRequest &req);

/* ------------------------------------------------------------------------ */
/* Windows command line quoting (CommandLineToArgvW / MSVCRT rules)         */
/* ------------------------------------------------------------------------ */

// Quote one argument so that the MS C runtime parses it back unchanged.
std::string quoteWindowsArg(const std::string &arg);
// exe + args -> full command line suitable for CreateProcessW.
std::string buildCommandLine(const std::string &exe, const std::vector<std::string> &args);
// Reverse operation, following the MS C runtime rules (used by tests).
std::vector<std::string> splitWindowsCommandLine(const std::string &cmdline);

/* ------------------------------------------------------------------------ */
/* Bitstream file names                                                     */
/* ------------------------------------------------------------------------ */

std::string fileBaseName(const std::string &path);
std::string fileDirName(const std::string &path);
std::string fileExtensionLower(const std::string &path); // without the dot
// Extensions openFPGALoader knows how to parse (see configBitstreamParser users).
const std::vector<std::string> &knownBitstreamExtensions();
bool isKnownBitstreamExtension(const std::string &path);

enum class BitstreamCheck { Ok, Empty, NotFound, UnknownExtension };
BitstreamCheck checkBitstream(const std::string &path,
	const std::function<bool(const std::string &)> &fileExists);

// Filter string for GetOpenFileName ('\0' separated, double '\0' terminated)
std::string openFileFilter();

/* ------------------------------------------------------------------------ */
/* Backend discovery                                                        */
/* ------------------------------------------------------------------------ */

std::vector<std::string> backendCandidates(const std::string &exeDir);
// Returns the first existing candidate or "" when openFPGALoader is missing.
std::string findBackend(const std::string &exeDir,
	const std::function<bool(const std::string &)> &fileExists);
// Directory holding spiOverJtag bridges (OPENFPGALOADER_SOJ_DIR).
std::string bridgeDirectory(const std::string &backendExe);

/* ------------------------------------------------------------------------ */
/* Output handling                                                          */
/* ------------------------------------------------------------------------ */

std::string stripAnsi(const std::string &s);
std::string trim(const std::string &s);

// Splits a byte stream into lines on '\n', '\r' or "\r\n".
class LineSplitter {
public:
	void feed(const char *data, size_t len, const std::function<void(const std::string &)> &onLine);
	void flush(const std::function<void(const std::string &)> &onLine);
private:
	std::string _pending;
	bool _lastWasCR = false;
};

// Progress lines look like "Loading: [=====      ] 45.12%".
struct ProgressUpdate {
	bool hasPercent = false;
	double percent = 0.0;
	std::string phase; // e.g. "Loading", "Erasing", "Writing"
};
bool parseProgressLine(const std::string &line, ProgressUpdate &out);
// Student friendly description of a backend progress phase.
std::string describePhase(const std::string &phase);

struct UsbProbe {
	std::string vidPid;
	std::string probeType;
	std::string manufacturer;
	std::string serial;
	std::string product;
};
std::vector<UsbProbe> parseScanUsb(const std::string &output);
// Map a --scan-usb probe_type to a --cable name ("" if no safe mapping).
std::string cableForProbeType(const std::string &probeType);

struct JtagDevice {
	uint32_t idcode = 0;
	std::string manufacturer;
	std::string family;
	std::string model;
	std::string type; // non FPGA devices
};
std::vector<JtagDevice> parseDetect(const std::string &output);

// Boards (from the given list) whose fpga part starts with the detected model.
std::vector<std::string> boardsMatchingModel(const std::vector<BoardInfo> &boards,
	const std::string &model);

/* ------------------------------------------------------------------------ */
/* Result interpretation                                                    */
/* ------------------------------------------------------------------------ */

enum class FailureKind {
	None,
	BackendMissing,
	LaunchFailed,
	CableNotFound,  // programmer not found / cannot open USB device
	UsbDriver,      // device present but driver prevents access
	NoJtagDevice,   // cable OK but no FPGA answers
	WrongBoard,     // board name unknown / FPGA mismatch
	BadBitstream,   // cannot open / parse bitstream
	FlashFailed,    // flash erase/write/verify failed
	MultipleFpga,
	Unknown,
};

struct FailureInfo {
	FailureKind kind = FailureKind::None;
	std::string title;
	std::string explanation;
	std::vector<std::string> checks;
};

// Exit code 0 is success. Otherwise inspect output to explain the failure.
FailureInfo classifyResult(int exitCode, const std::string &output);
FailureInfo backendMissingInfo(const std::string &exeDir);
FailureInfo launchFailedInfo(const std::string &reason);
std::string formatFailure(const FailureInfo &info);

} // namespace icslab
