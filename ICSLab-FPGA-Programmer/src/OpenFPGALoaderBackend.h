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

// Lab board list: one entry per line,
//   boardName [= Friendly name] [| cable=<cable name>]
// '#' starts a comment. boardName is an openFPGALoader board ("-b") or, when
// openFPGALoader has no entry for the board, any identifier combined with
// "cable=" (the GUI then only passes "-c <cable>").
struct LabBoard {
	std::string name;
	std::string label;
	std::string cable; // optional cable override / cable-only board
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
	std::string probeFirmware; // USB-Blaster II FX2 firmware (blaster_6810.hex)
};

std::vector<std::string> buildListBoardsArgs();
std::vector<std::string> buildListCablesArgs();
std::vector<std::string> buildVersionArgs();
std::vector<std::string> buildScanUsbArgs();
std::vector<std::string> buildDetectArgs(const std::string &board, const std::string &cable,
	const std::string &probeFirmware = std::string());
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
/* Intel/Altera USB-Blaster support                                         */
/* ------------------------------------------------------------------------ */

constexpr uint16_t kIntelVid = 0x09FB;
constexpr uint16_t kBlasterPid = 0x6001;        // USB-Blaster
constexpr uint16_t kBlasterIIColdPid = 0x6010;  // USB-Blaster II, no firmware yet
constexpr uint16_t kBlasterIIPid = 0x6810;      // USB-Blaster II, firmware loaded

// "USB\VID_09FB&PID_6010&REV_0001" -> vid/pid; mi = interface or -1.
bool parseUsbHardwareId(const std::string &hwid, uint16_t &vid, uint16_t &pid, int &mi);
bool isIntelBlaster(uint16_t vid, uint16_t pid);
// openFPGALoader cable name for an Intel blaster ("" when not a blaster).
std::string blasterCableName(uint16_t vid, uint16_t pid);
std::string blasterDisplayName(uint16_t vid, uint16_t pid);
// USB-Blaster II needs Intel's FX2 firmware file passed with --probe-firmware.
bool cableNeedsBlasterFirmware(const std::string &cable);
const char *blasterFirmwareFileName(); // "blaster_6810.hex"

// Candidate locations of blaster_6810.hex, most preferred first:
// the instructor's choice, <app>\firmware, $QUARTUS_ROOTDIR, then every
// Quartus / Quartus Programmer installation directory given.
std::vector<std::string> blasterFirmwareCandidates(const std::string &exeDir,
	const std::string &configuredPath, const std::string &quartusRootDir,
	const std::vector<std::string> &quartusVersionDirs);
// Base folders that contain Quartus versions on one drive: intelFPGA_lite,
// intelFPGA, intelFPGA_pro, intelFPGA_standard and, for older releases
// (Quartus II 13.x and earlier, still needed for Cyclone II boards such as the
// original DE2), altera, altera_lite, altera_pro.
std::vector<std::string> quartusInstallRoots(const std::string &drive);
// Version folder names compared naturally ("23.1" > "13.0sp1" > "9.1sp2").
bool quartusVersionNewer(const std::string &dirA, const std::string &dirB);
void sortQuartusVersionDirsNewestFirst(std::vector<std::string> &dirs);
// "C:\altera\13.0sp1\quartus\bin64\quartus_cpf.exe" -> "13.0sp1"
std::string quartusVersionOfTool(const std::string &toolPath);

// Windows Device Manager problem code (CM_PROB_*) -> short explanation.
std::string describeDeviceProblem(unsigned long problemCode);
bool isWinUsbService(const std::string &service);

/* ------------------------------------------------------------------------ */
/* Quartus .sof files                                                       */
/* ------------------------------------------------------------------------ */

// openFPGALoader cannot program Intel's .sof format: the GUI converts it to
// an uncompressed .rbf with Quartus's quartus_cpf when Quartus is installed.
bool isQuartusSof(const std::string &path);
// "C:\p\output_files\top.sof" -> "C:\p\output_files\top.rbf"
std::string siblingRbf(const std::string &sofPath);
// quartus_cpf arguments for an uncompressed .rbf (JTAG configuration cannot
// use compressed bitstreams).
std::vector<std::string> buildSofToRbfArgs(const std::string &sof, const std::string &rbf);
// Candidate locations of a Quartus tool (e.g. "quartus_cpf.exe").
std::vector<std::string> quartusToolCandidates(const std::string &exeName,
	const std::string &quartusRootDir, const std::vector<std::string> &quartusVersionDirs);

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
	BlasterFirmware, // USB-Blaster II firmware missing
	SofConversion,   // .sof selected and it could not be converted
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
// .sof selected but Quartus (quartus_cpf) is not installed on this PC.
FailureInfo sofNeedsConversionInfo(const std::string &sofPath);
// quartus_cpf ran but failed with every installed Quartus version tried.
FailureInfo sofConversionFailedInfo(int exitCode,
	const std::vector<std::string> &triedVersions = std::vector<std::string>());

} // namespace icslab
