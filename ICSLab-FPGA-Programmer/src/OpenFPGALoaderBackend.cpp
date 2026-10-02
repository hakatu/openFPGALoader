// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - openFPGALoader backend helpers.

#include "OpenFPGALoaderBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>

namespace icslab {

namespace {

std::string toLower(std::string s)
{
	for (auto &c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

bool startsWith(const std::string &s, const std::string &prefix)
{
	return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool containsNoCase(const std::string &haystack, const std::string &needle)
{
	return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

std::vector<std::string> splitWhitespace(const std::string &line)
{
	std::vector<std::string> out;
	std::istringstream iss(line);
	std::string tok;
	while (iss >> tok)
		out.push_back(tok);
	return out;
}

std::vector<std::string> splitLines(const std::string &text)
{
	std::vector<std::string> lines;
	LineSplitter splitter;
	auto cb = [&](const std::string &l) { lines.push_back(l); };
	splitter.feed(text.data(), text.size(), cb);
	splitter.flush(cb);
	return lines;
}

std::string undefinedToEmpty(const std::string &s)
{
	return s == "Undefined" ? std::string() : s;
}

bool isPathSeparator(char c) { return c == '\\' || c == '/'; }

std::string joinPath(const std::string &dir, const std::string &name)
{
	if (dir.empty())
		return name;
	if (isPathSeparator(dir.back()))
		return dir + name;
	return dir + "\\" + name;
}

} // namespace

/* ------------------------------------------------------------------------ */
/* Boards / cables                                                          */
/* ------------------------------------------------------------------------ */

std::vector<BoardInfo> parseBoardList(const std::string &output)
{
	std::vector<BoardInfo> boards;
	for (const auto &raw : splitLines(stripAnsi(output))) {
		auto tok = splitWhitespace(raw);
		if (tok.size() < 3)
			continue;
		if (tok[0] == "board" && tok[1] == "name")
			continue; // header
		BoardInfo b;
		b.name = tok[0];
		b.cable = undefinedToEmpty(tok[1]);
		b.fpgaPart = undefinedToEmpty(tok[2]);
		boards.push_back(b);
	}
	return boards;
}

std::vector<std::string> parseCableList(const std::string &output)
{
	std::vector<std::string> cables;
	for (const auto &raw : splitLines(stripAnsi(output))) {
		auto tok = splitWhitespace(raw);
		if (tok.size() < 2)
			continue;
		if (tok[0] == "cable" && tok[1] == "name")
			continue; // header
		if (!startsWith(tok.back(), "0x"))
			continue;
		cables.push_back(tok[0]);
	}
	return cables;
}

std::vector<LabBoard> parseLabBoards(const std::string &text)
{
	std::vector<LabBoard> out;
	for (auto line : splitLines(text)) {
		auto hash = line.find('#');
		if (hash != std::string::npos)
			line = line.substr(0, hash);
		line = trim(line);
		if (line.empty())
			continue;
		LabBoard lb;
		auto eq = line.find('=');
		if (eq == std::string::npos) {
			lb.name = line;
		} else {
			lb.name = trim(line.substr(0, eq));
			lb.label = trim(line.substr(eq + 1));
		}
		if (lb.name.empty())
			continue;
		if (lb.label.empty())
			lb.label = friendlyBoardName(lb.name);
		out.push_back(lb);
	}
	return out;
}

std::string friendlyBoardName(const std::string &board)
{
	static const std::map<std::string, std::string> names = {
		{"basys3",          "Digilent Basys 3"},
		{"nexys_a7_50",     "Digilent Nexys A7-50T"},
		{"nexys_a7_100",    "Digilent Nexys A7-100T"},
		{"nexysVideo",      "Digilent Nexys Video"},
		{"arty_a7_35t",     "Digilent Arty A7-35T"},
		{"arty_a7_100t",    "Digilent Arty A7-100T"},
		{"arty_s7_50",      "Digilent Arty S7-50"},
		{"zedboard",        "Digilent ZedBoard"},
		{"zybo_z7_10",      "Digilent Zybo Z7-10"},
		{"zybo_z7_20",      "Digilent Zybo Z7-20"},
		{"cmod_s7",         "Digilent Cmod S7"},
		{"cmoda7_35t",      "Digilent Cmod A7-35T"},
		{"de0nano",         "Terasic DE0-Nano"},
		{"de10lite",        "Terasic DE10-Lite"},
		{"de10nano",        "Terasic DE10-Nano"},
		{"de1Soc",          "Terasic DE1-SoC"},
		{"tangnano9k",      "Sipeed Tang Nano 9K"},
		{"tangnano20k",     "Sipeed Tang Nano 20K"},
		{"ulx3s",           "Radiona ULX3S"},
	};
	auto it = names.find(board);
	if (it != names.end())
		return it->second;
	return board;
}

std::vector<LabBoard> defaultLabBoards()
{
	// Typical teaching-lab boards; edit lab-boards.txt next to the
	// executable to change this list without recompiling.
	return parseLabBoards(
		"basys3\n"
		"nexys_a7_100\n"
		"nexys_a7_50\n"
		"nexysVideo\n"
		"arty_a7_35t\n"
		"arty_a7_100t\n"
		"zybo_z7_20\n"
		"de10lite\n"
		"de0nano\n"
		"tangnano9k\n"
		"cmoda7_35t\n");
}

/* ------------------------------------------------------------------------ */
/* Command generation                                                       */
/* ------------------------------------------------------------------------ */

std::vector<std::string> buildListBoardsArgs() { return {"--list-boards"}; }
std::vector<std::string> buildListCablesArgs() { return {"--list-cables"}; }
std::vector<std::string> buildVersionArgs() { return {"--Version"}; }
std::vector<std::string> buildScanUsbArgs() { return {"--scan-usb"}; }

static void appendTargetArgs(std::vector<std::string> &args, const std::string &board,
	const std::string &cable)
{
	if (!board.empty()) {
		args.push_back("-b");
		args.push_back(board);
	}
	if (!cable.empty()) {
		args.push_back("-c");
		args.push_back(cable);
	}
}

std::vector<std::string> buildDetectArgs(const std::string &board, const std::string &cable)
{
	std::vector<std::string> args;
	appendTargetArgs(args, board, cable);
	args.push_back("--detect");
	return args;
}

std::vector<std::string> buildProgramArgs(const ProgramRequest &req)
{
	std::vector<std::string> args;
	appendTargetArgs(args, req.board, req.cable);
	// Explicit mode: never rely on backend defaults (SPI-mode boards would
	// otherwise default to flash).
	if (req.target == ProgramTarget::Flash)
		args.push_back("--write-flash");
	else
		args.push_back("--write-sram");
	// Bitstream is passed as a positional argument. "--bitstream" avoids any
	// ambiguity with file names beginning with '-'.
	args.push_back("--bitstream");
	args.push_back(req.bitstream);
	return args;
}

/* ------------------------------------------------------------------------ */
/* Windows quoting                                                          */
/* ------------------------------------------------------------------------ */

std::string quoteWindowsArg(const std::string &arg)
{
	if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos)
		return arg;

	std::string out = "\"";
	for (size_t i = 0; ; ++i) {
		size_t backslashes = 0;
		while (i < arg.size() && arg[i] == '\\') {
			++i;
			++backslashes;
		}
		if (i == arg.size()) {
			// Escape all trailing backslashes: the closing quote follows.
			out.append(backslashes * 2, '\\');
			break;
		} else if (arg[i] == '"') {
			out.append(backslashes * 2 + 1, '\\');
			out.push_back('"');
		} else {
			out.append(backslashes, '\\');
			out.push_back(arg[i]);
		}
	}
	out.push_back('"');
	return out;
}

std::string buildCommandLine(const std::string &exe, const std::vector<std::string> &args)
{
	// argv[0] is parsed with simpler rules (no escapes): always quote it,
	// a path cannot contain '"'.
	std::string cmd = "\"" + exe + "\"";
	for (const auto &a : args) {
		cmd.push_back(' ');
		cmd += quoteWindowsArg(a);
	}
	return cmd;
}

std::vector<std::string> splitWindowsCommandLine(const std::string &cmd)
{
	std::vector<std::string> out;
	size_t i = 0;
	const size_t n = cmd.size();

	// argv[0]
	{
		std::string a;
		if (i < n && cmd[i] == '"') {
			++i;
			while (i < n && cmd[i] != '"')
				a.push_back(cmd[i++]);
			if (i < n)
				++i;
		} else {
			while (i < n && cmd[i] != ' ' && cmd[i] != '\t')
				a.push_back(cmd[i++]);
		}
		out.push_back(a);
	}

	for (;;) {
		while (i < n && (cmd[i] == ' ' || cmd[i] == '\t'))
			++i;
		if (i >= n)
			break;
		std::string a;
		bool inQuotes = false;
		while (i < n) {
			if (!inQuotes && (cmd[i] == ' ' || cmd[i] == '\t'))
				break;
			size_t bs = 0;
			while (i < n && cmd[i] == '\\') {
				++bs;
				++i;
			}
			if (i < n && cmd[i] == '"') {
				a.append(bs / 2, '\\');
				if (bs % 2) {
					a.push_back('"');
					++i;
				} else {
					// "" inside quotes produces a literal quote (MSVCRT 2008+)
					if (inQuotes && i + 1 < n && cmd[i + 1] == '"') {
						a.push_back('"');
						i += 2;
					} else {
						inQuotes = !inQuotes;
						++i;
					}
				}
			} else {
				a.append(bs, '\\');
				if (i < n && (inQuotes || (cmd[i] != ' ' && cmd[i] != '\t')))
					a.push_back(cmd[i++]);
			}
		}
		out.push_back(a);
	}
	return out;
}

/* ------------------------------------------------------------------------ */
/* Bitstream files                                                          */
/* ------------------------------------------------------------------------ */

std::string fileBaseName(const std::string &path)
{
	auto pos = path.find_last_of("\\/");
	return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string fileDirName(const std::string &path)
{
	auto pos = path.find_last_of("\\/");
	if (pos == std::string::npos)
		return std::string();
	if (pos == 2 && path.size() > 1 && path[1] == ':')
		return path.substr(0, 3); // "C:\"
	return path.substr(0, pos);
}

std::string fileExtensionLower(const std::string &path)
{
	std::string base = toLower(fileBaseName(path));
	// "design.bit.gz" -> "bit" (openFPGALoader handles .gz transparently)
	if (base.size() > 3 && base.compare(base.size() - 3, 3, ".gz") == 0)
		base = base.substr(0, base.size() - 3);
	auto dot = base.find_last_of('.');
	if (dot == std::string::npos || dot == 0)
		return std::string();
	return base.substr(dot + 1);
}

const std::vector<std::string> &knownBitstreamExtensions()
{
	// From openFPGALoader's parsers (src/*.cpp file type checks).
	static const std::vector<std::string> exts = {
		"bit", "bin", "svf", "jed", "hex", "fs", "rbf", "rpd", "mcs",
		"pof", "dfu", "cfg", "fea", "pdi", "pub", "raw",
	};
	return exts;
}

bool isKnownBitstreamExtension(const std::string &path)
{
	const auto ext = fileExtensionLower(path);
	const auto &k = knownBitstreamExtensions();
	return std::find(k.begin(), k.end(), ext) != k.end();
}

BitstreamCheck checkBitstream(const std::string &path,
	const std::function<bool(const std::string &)> &fileExists)
{
	if (trim(path).empty())
		return BitstreamCheck::Empty;
	if (!fileExists(path))
		return BitstreamCheck::NotFound;
	if (!isKnownBitstreamExtension(path))
		return BitstreamCheck::UnknownExtension;
	return BitstreamCheck::Ok;
}

std::string openFileFilter()
{
	std::string patterns;
	for (const auto &e : knownBitstreamExtensions()) {
		if (!patterns.empty())
			patterns += ";";
		patterns += "*." + e + ";*." + e + ".gz";
	}
	std::string f;
	f += "FPGA bitstreams (*.bit, *.bin, *.svf, *.jed, *.hex, ...)";
	f.push_back('\0');
	f += patterns;
	f.push_back('\0');
	f += "All files (*.*)";
	f.push_back('\0');
	f += "*.*";
	f.push_back('\0');
	f.push_back('\0');
	return f;
}

/* ------------------------------------------------------------------------ */
/* Backend discovery                                                        */
/* ------------------------------------------------------------------------ */

std::vector<std::string> backendCandidates(const std::string &exeDir)
{
	return {
		joinPath(joinPath(exeDir, "backend"), "openFPGALoader.exe"),
		joinPath(exeDir, "openFPGALoader.exe"),
	};
}

std::string findBackend(const std::string &exeDir,
	const std::function<bool(const std::string &)> &fileExists)
{
	for (const auto &c : backendCandidates(exeDir)) {
		if (fileExists(c))
			return c;
	}
	return std::string();
}

std::string bridgeDirectory(const std::string &backendExe)
{
	return joinPath(joinPath(fileDirName(backendExe), "share"), "openFPGALoader");
}

/* ------------------------------------------------------------------------ */
/* Output handling                                                          */
/* ------------------------------------------------------------------------ */

std::string stripAnsi(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
			i += 2;
			while (i < s.size() && !(s[i] >= '@' && s[i] <= '~'))
				++i;
			continue;
		}
		out.push_back(s[i]);
	}
	return out;
}

std::string trim(const std::string &s)
{
	size_t b = 0, e = s.size();
	while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
		++b;
	while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
		--e;
	return s.substr(b, e - b);
}

void LineSplitter::feed(const char *data, size_t len,
	const std::function<void(const std::string &)> &onLine)
{
	for (size_t i = 0; i < len; ++i) {
		const char c = data[i];
		if (c == '\n') {
			if (_lastWasCR) { // "\r\n": line already emitted on '\r'
				_lastWasCR = false;
				continue;
			}
			onLine(_pending);
			_pending.clear();
		} else if (c == '\r') {
			onLine(_pending);
			_pending.clear();
			_lastWasCR = true;
			continue;
		} else {
			_pending.push_back(c);
		}
		_lastWasCR = false;
	}
}

void LineSplitter::flush(const std::function<void(const std::string &)> &onLine)
{
	if (!_pending.empty())
		onLine(_pending);
	_pending.clear();
	_lastWasCR = false;
}

bool parseProgressLine(const std::string &rawLine, ProgressUpdate &out)
{
	const std::string line = stripAnsi(rawLine);
	auto open = line.find(": [");
	if (open == std::string::npos)
		return false;
	auto close = line.find(']', open + 3);
	if (close == std::string::npos)
		return false;
	// Bar content must only be '=' and spaces
	for (size_t i = open + 3; i < close; ++i) {
		if (line[i] != '=' && line[i] != ' ')
			return false;
	}
	auto pct = line.find('%', close);
	if (pct == std::string::npos)
		return false;
	std::string num = trim(line.substr(close + 1, pct - close - 1));
	if (num.empty())
		return false;
	char *end = nullptr;
	double v = std::strtod(num.c_str(), &end);
	if (end == num.c_str() || *end != '\0')
		return false;
	if (v < 0.0)
		v = 0.0;
	if (v > 100.0)
		v = 100.0;
	out.hasPercent = true;
	out.percent = v;
	out.phase = trim(line.substr(0, open));
	return true;
}

std::string describePhase(const std::string &phase)
{
	const std::string p = toLower(phase);
	if (p.find("eras") != std::string::npos)
		return "Erasing flash";
	if (p.find("verif") != std::string::npos)
		return "Verifying";
	if (p.find("read") != std::string::npos || p.find("dump") != std::string::npos)
		return "Reading back";
	if (p.find("writ") != std::string::npos)
		return p.find("svf") != std::string::npos ? "Programming (SVF)" : "Writing flash";
	if (p.find("firmware") != std::string::npos)
		return "Loading programmer firmware";
	if (p.find("load") != std::string::npos || p.find("sram") != std::string::npos ||
			p.find("cram") != std::string::npos)
		return "Programming FPGA";
	if (phase.empty())
		return "Working";
	return phase;
}

std::vector<UsbProbe> parseScanUsb(const std::string &output)
{
	std::vector<UsbProbe> probes;
	for (const auto &raw : splitLines(stripAnsi(output))) {
		auto tok = splitWhitespace(raw);
		// Bus device vid:pid probe_type manufacturer serial product...
		if (tok.size() < 4)
			continue;
		if (tok[0] == "Bus")
			continue;
		if (tok[2].size() != 13 || tok[2][6] != ':' || !startsWith(tok[2], "0x"))
			continue;
		UsbProbe p;
		p.vidPid = tok[2];
		p.probeType = tok[3];
		if (tok.size() > 4)
			p.manufacturer = tok[4];
		if (tok.size() > 5)
			p.serial = tok[5];
		for (size_t i = 6; i < tok.size(); ++i) {
			if (!p.product.empty())
				p.product += " ";
			p.product += tok[i];
		}
		probes.push_back(p);
	}
	return probes;
}

std::string cableForProbeType(const std::string &probeType)
{
	// --scan-usb prints chip names for FTDI parts and cable names otherwise.
	static const std::map<std::string, std::string> ftdi = {
		{"FTDI2232", "ft2232"},
		{"ft4232",   "ft4232"},
		{"ft232H",   "ft232"},
		{"ft232RL",  "ft232RL"},
		{"ft231X",   "ft231X"},
		{"FT4232HP", "ft4232hp"},
	};
	auto it = ftdi.find(probeType);
	if (it != ftdi.end())
		return it->second;
	if (probeType == "unknown" || startsWith(probeType, "unknown"))
		return std::string();
	return probeType;
}

std::vector<JtagDevice> parseDetect(const std::string &output)
{
	std::vector<JtagDevice> devices;
	JtagDevice *cur = nullptr;
	for (const auto &raw : splitLines(stripAnsi(output))) {
		std::string line = trim(raw);
		if (startsWith(line, "index ") && line.back() == ':') {
			devices.emplace_back();
			cur = &devices.back();
			continue;
		}
		if (!cur)
			continue;
		auto sp = line.find_first_of(" \t");
		if (sp == std::string::npos)
			continue;
		std::string key = line.substr(0, sp);
		std::string val = trim(line.substr(sp));
		if (key == "idcode")
			cur->idcode = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 16));
		else if (key == "manufacturer")
			cur->manufacturer = val;
		else if (key == "family")
			cur->family = val;
		else if (key == "model")
			cur->model = val;
		else if (key == "type")
			cur->type = val;
	}
	return devices;
}

std::vector<std::string> boardsMatchingModel(const std::vector<BoardInfo> &boards,
	const std::string &model)
{
	std::vector<std::string> out;
	if (model.empty())
		return out;
	const std::string m = toLower(model);
	for (const auto &b : boards) {
		if (!b.fpgaPart.empty() && startsWith(toLower(b.fpgaPart), m))
			out.push_back(b.name);
	}
	return out;
}

/* ------------------------------------------------------------------------ */
/* Result interpretation                                                    */
/* ------------------------------------------------------------------------ */

static const std::vector<std::string> kHardwareChecks = {
	"USB cable connection",
	"FPGA board power (power switch ON)",
	"the selected board",
	"USB driver (see docs\\windows-driver.md)",
};

FailureInfo classifyResult(int exitCode, const std::string &rawOutput)
{
	FailureInfo info;
	if (exitCode == 0) {
		info.kind = FailureKind::None;
		return info;
	}

	const std::string out = stripAnsi(rawOutput);
	auto has = [&](const char *s) { return containsNoCase(out, s); };

	if (has("cannot find board")) {
		info.kind = FailureKind::WrongBoard;
		info.title = "Unknown board.";
		info.explanation = "openFPGALoader does not know the selected board.";
		info.checks = {"select your board again from the list",
			"or choose \"Auto Detect / Generic JTAG\""};
	} else if (has("unable to open '") || has("fail to open ") ||
			has("No such file") || has("Unable to open '")) {
		info.kind = FailureKind::BadBitstream;
		info.title = "Cannot open the bitstream file.";
		info.explanation = "The selected file could not be read.";
		info.checks = {"the file still exists and is not open in another program",
			"the file is on a local drive (not a disconnected USB/network drive)"};
	} else if (has("unable to parse") || has("Failed to parse bitstream") ||
			has("parseHeader failed") || has("Bitstream too small") ||
			has("Invalid bitstream") || has("Empty bitstream") ||
			has("Bound check failure") || has("decompress failed") ||
			has("File is shorter than bitstream") || has("unknown file type") ||
			has("please add correct extension")) {
		info.kind = FailureKind::BadBitstream;
		info.title = "The bitstream file is not valid.";
		info.explanation = "The file could not be understood as an FPGA "
			"bitstream for this board (corrupted, wrong format or generated for another FPGA).";
		info.checks = {"you selected the right file (e.g. the .bit generated by Vivado)",
			"the design finished generating the bitstream without errors",
			"the bitstream targets the FPGA on your board"};
	} else if (has("claim usb device") || has("default FTDI driver") ||
			has("can't open device with vid") ||
			has("LIBUSB_ERROR_NOT_SUPPORTED") || has("LIBUSB_ERROR_ACCESS") ||
			has("Operation not supported") || has("Access denied") ||
			has("libusb error while claiming") || has("unable to set interface")) {
		info.kind = FailureKind::UsbDriver;
		info.title = "The programmer was found but Windows blocks access to it.";
		info.explanation = "The USB driver installed for the programmer cannot be "
			"used by openFPGALoader (usually it needs the WinUSB driver).";
		info.checks = {"follow docs\\windows-driver.md (Zadig, interface 0 only)",
			"close other programs using the board (Vivado Hardware Manager, Quartus, Gowin)",
			"unplug and re-plug the USB cable"};
	} else if (has("unable to open ftdi device") || has("No cable found") ||
			has("fails to open device") || has("can't get USB device") ||
			has("No USB devices found") || has("device not found") ||
			has("Failed to claim cable") || has("JTAG init failed") ||
			(has("not found") && has("cable"))) {
		info.kind = FailureKind::CableNotFound;
		info.title = "Unable to communicate with the FPGA programmer.";
		info.explanation = "No matching USB programmer was found.";
		info.checks = kHardwareChecks;
	} else if (has("more than one FPGA found")) {
		info.kind = FailureKind::MultipleFpga;
		info.title = "More than one FPGA was found on the JTAG chain.";
		info.explanation = "This board needs an explicit device selection, "
			"which the simple mode does not provide.";
		info.checks = {"ask your lab instructor"};
	} else if (has("no device found") || has("TDO is stuck") ||
			has("IDCODE not found") || has("Device not supported") ||
			has("unknown device") || (has("not supported") && has("idcode"))) {
		info.kind = FailureKind::NoJtagDevice;
		info.title = "The programmer works, but no FPGA answered.";
		info.explanation = "The JTAG chain is empty or the FPGA is not recognised.";
		info.checks = {"FPGA board power (power switch ON)",
			"the board's JTAG/programming jumpers",
			"the selected board"};
	} else if (has("flash") && (has("fail") || has("error") || has("verify"))) {
		info.kind = FailureKind::FlashFailed;
		info.title = "Flash programming failed.";
		info.explanation = "The flash memory could not be erased, written or verified.";
		info.checks = {"the board is still powered and connected",
			"the selected board (flash type depends on the board)",
			"try FPGA SRAM mode to check your bitstream first"};
	} else {
		info.kind = FailureKind::Unknown;
		info.title = "Programming failed.";
		info.explanation = "openFPGALoader reported an error (exit code " +
			std::to_string(exitCode) + ").";
		info.checks = kHardwareChecks;
	}
	return info;
}

FailureInfo backendMissingInfo(const std::string &exeDir)
{
	FailureInfo info;
	info.kind = FailureKind::BackendMissing;
	info.title = "openFPGALoader.exe is missing.";
	info.explanation = "The programming backend was not found next to the "
		"application. Expected: " + backendCandidates(exeDir).front();
	info.checks = {"extract the whole ZIP file (do not run the program from inside the ZIP)",
		"keep the \"backend\" folder next to \"ICSLab FPGA Programmer.exe\"",
		"your antivirus did not quarantine openFPGALoader.exe"};
	return info;
}

FailureInfo launchFailedInfo(const std::string &reason)
{
	FailureInfo info;
	info.kind = FailureKind::LaunchFailed;
	info.title = "Could not start openFPGALoader.exe.";
	info.explanation = reason;
	info.checks = {"your antivirus did not block openFPGALoader.exe",
		"extract the whole ZIP file before running"};
	return info;
}

std::string formatFailure(const FailureInfo &info)
{
	std::string s = info.title;
	if (!info.explanation.empty())
		s += "\r\n" + info.explanation;
	if (!info.checks.empty()) {
		s += "\r\n\r\nCheck:";
		for (const auto &c : info.checks)
			s += "\r\n  \xE2\x80\xA2 " + c; // U+2022 bullet
	}
	return s;
}

} // namespace icslab
