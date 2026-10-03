// SPDX-License-Identifier: Apache-2.0
// ICSLab FPGA Programmer - test-suite.
//
// Portable tests run on any host. On Windows (or under Wine) the process
// tests also run, using fake_openfpgaloader.exe located next to this test
// executable. Optionally pass the real openFPGALoader.exe as first argument
// to run integration tests against it (no FPGA hardware required).

#include "OpenFPGALoaderBackend.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "ProcessRunner.h"
#include "DriverSetup.h"
#endif

using namespace icslab;

static int g_checks = 0, g_failures = 0;
static const char *g_test = "";

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; \
	std::printf("  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { ++g_checks; auto _va = (a); auto _vb = (b); if (!(_va == _vb)) { ++g_failures; \
	std::printf("  FAIL [%s] %s:%d: %s == %s\n    got:      '%s'\n    expected: '%s'\n", g_test, \
		__FILE__, __LINE__, #a, #b, toStr(_va).c_str(), toStr(_vb).c_str()); } } while (0)

[[maybe_unused]] static std::string toStr(const std::string &s) { return s; }
[[maybe_unused]] static std::string toStr(const char *s) { return s; }
[[maybe_unused]] static std::string toStr(size_t v) { return std::to_string(v); }
[[maybe_unused]] static std::string toStr(int v) { return std::to_string(v); }
[[maybe_unused]] static std::string toStr(unsigned v) { return std::to_string(v); }
[[maybe_unused]] static std::string toStr(bool v) { return v ? "true" : "false"; }
[[maybe_unused]] static std::string toStr(double v) { return std::to_string(v); }
[[maybe_unused]] static std::string toStr(FailureKind k) { return std::to_string(static_cast<int>(k)); }
[[maybe_unused]] static std::string toStr(BitstreamCheck k) { return std::to_string(static_cast<int>(k)); }
[[maybe_unused]] static std::string toStr(const std::vector<std::string> &v)
{
	std::string s = "{";
	for (size_t i = 0; i < v.size(); ++i)
		s += (i ? ", " : "") + std::string("\"") + v[i] + "\"";
	return s + "}";
}

#define TEST(name) static void name(); struct name##_reg { name##_reg() { tests().push_back({#name, name}); } } name##_inst; static void name()
struct TestCase { const char *name; void (*fn)(); };
static std::vector<TestCase> &tests() { static std::vector<TestCase> t; return t; }

static const std::string kStudentPath =
	"C:\\Users\\Student Name\\Desktop\\Lab 3\\final design.bit";

/* ------------------------------------------------------------------------ */
/* Windows quoting                                                          */
/* ------------------------------------------------------------------------ */

TEST(quoting_simple_args_unchanged)
{
	CHECK_EQ(quoteWindowsArg("-b"), "-b");
	CHECK_EQ(quoteWindowsArg("basys3"), "basys3");
	CHECK_EQ(quoteWindowsArg("C:\\fpga\\top.bit"), "C:\\fpga\\top.bit");
}

TEST(quoting_paths_with_spaces)
{
	CHECK_EQ(quoteWindowsArg(kStudentPath),
		"\"C:\\Users\\Student Name\\Desktop\\Lab 3\\final design.bit\"");
	CHECK_EQ(quoteWindowsArg(""), "\"\"");
	// trailing backslash must be doubled before the closing quote
	CHECK_EQ(quoteWindowsArg("C:\\My Dir\\"), "\"C:\\My Dir\\\\\"");
	// embedded quote
	CHECK_EQ(quoteWindowsArg("a\"b c"), "\"a\\\"b c\"");
	// backslashes before an embedded quote
	CHECK_EQ(quoteWindowsArg("a\\\"b"), "\"a\\\\\\\"b\"");
}

TEST(quoting_round_trip)
{
	const std::vector<std::string> samples = {
		kStudentPath, "", " ", "plain", "C:\\Program Files\\x\\", "\\\\server\\share\\a b.bit",
		"tab\there", "q\"uote", "\\\"", "\\\\\"", "end\\\\", "D:\\Lab 3\\x\"y\\z.bit",
		"\xC4\x90\xE1\xBA\xA1i h\xE1\xBB\x8D\x63\\b\xC3\xA0i 1.bit", // Vietnamese UTF-8
	};
	for (const auto &s : samples) {
		auto cmd = buildCommandLine("C:\\Program Files\\ICSLab\\backend\\openFPGALoader.exe", {"-b", s, s});
		auto argv = splitWindowsCommandLine(cmd);
		CHECK_EQ(argv.size(), size_t(4));
		if (argv.size() == 4) {
			CHECK_EQ(argv[0], "C:\\Program Files\\ICSLab\\backend\\openFPGALoader.exe");
			CHECK_EQ(argv[1], "-b");
			CHECK_EQ(argv[2], s);
			CHECK_EQ(argv[3], s);
		}
	}
}

TEST(command_line_quotes_exe)
{
	auto cmd = buildCommandLine("C:\\Lab Tools\\backend\\openFPGALoader.exe", {"--detect"});
	CHECK_EQ(cmd, "\"C:\\Lab Tools\\backend\\openFPGALoader.exe\" --detect");
}

/* ------------------------------------------------------------------------ */
/* Command generation                                                       */
/* ------------------------------------------------------------------------ */

TEST(board_argument_generation)
{
	ProgramRequest r;
	r.board = "basys3";
	r.bitstream = "C:\\a.bit";
	auto a = buildProgramArgs(r);
	CHECK_EQ(a, (std::vector<std::string>{"-b", "basys3", "--write-sram", "--bitstream", "C:\\a.bit"}));

	r.board = "";
	a = buildProgramArgs(r);
	CHECK_EQ(a, (std::vector<std::string>{"--write-sram", "--bitstream", "C:\\a.bit"}));

	r.board = "nexysVideo";
	r.cable = "digilent_b";
	a = buildProgramArgs(r);
	CHECK_EQ(a, (std::vector<std::string>{"-b", "nexysVideo", "-c", "digilent_b",
		"--write-sram", "--bitstream", "C:\\a.bit"}));
}

TEST(sram_command_generation)
{
	ProgramRequest r;
	r.board = "basys3";
	r.bitstream = kStudentPath;
	r.target = ProgramTarget::Sram;
	auto cmd = buildCommandLine("C:\\app\\backend\\openFPGALoader.exe", buildProgramArgs(r));
	CHECK_EQ(cmd, "\"C:\\app\\backend\\openFPGALoader.exe\" -b basys3 --write-sram --bitstream "
		"\"C:\\Users\\Student Name\\Desktop\\Lab 3\\final design.bit\"");
}

TEST(flash_command_generation)
{
	ProgramRequest r;
	r.board = "arty_a7_35t";
	r.bitstream = kStudentPath;
	r.target = ProgramTarget::Flash;
	auto a = buildProgramArgs(r);
	CHECK_EQ(a, (std::vector<std::string>{"-b", "arty_a7_35t", "--write-flash", "--bitstream", kStudentPath}));
	// never adds erase or offset options on its own
	for (const auto &x : a) {
		CHECK(x != "--bulk-erase");
		CHECK(x != "-o" && x != "--offset");
		CHECK(x != "--freq");
	}
}

TEST(detect_and_list_commands)
{
	CHECK_EQ(buildDetectArgs("", ""), (std::vector<std::string>{"--detect"}));
	CHECK_EQ(buildDetectArgs("basys3", ""), (std::vector<std::string>{"-b", "basys3", "--detect"}));
	CHECK_EQ(buildDetectArgs("", "ft2232"), (std::vector<std::string>{"-c", "ft2232", "--detect"}));
	CHECK_EQ(buildListBoardsArgs(), (std::vector<std::string>{"--list-boards"}));
	CHECK_EQ(buildScanUsbArgs(), (std::vector<std::string>{"--scan-usb"}));
}

/* ------------------------------------------------------------------------ */
/* Bitstream handling                                                       */
/* ------------------------------------------------------------------------ */

TEST(bitstream_filename_handling)
{
	CHECK_EQ(fileBaseName(kStudentPath), "final design.bit");
	CHECK_EQ(fileDirName(kStudentPath), "C:\\Users\\Student Name\\Desktop\\Lab 3");
	CHECK_EQ(fileDirName("C:\\top.bit"), "C:\\");
	CHECK_EQ(fileBaseName("top.bit"), "top.bit");
	CHECK_EQ(fileExtensionLower(kStudentPath), "bit");
	CHECK_EQ(fileExtensionLower("C:\\x\\TOP.BIT"), "bit");
	CHECK_EQ(fileExtensionLower("C:\\x\\top.bit.gz"), "bit");
	CHECK_EQ(fileExtensionLower("C:\\my.dir\\noext"), "");
	CHECK_EQ(fileExtensionLower("C:\\x\\.bit"), "");
	for (const char *e : {"a.bit", "a.bin", "a.svf", "a.jed", "a.hex", "a.fs", "a.rbf", "a.bit.gz"})
		CHECK(isKnownBitstreamExtension(e));
	CHECK(!isKnownBitstreamExtension("report.txt"));
	CHECK(!isKnownBitstreamExtension("design.xpr"));
}

TEST(bitstream_check)
{
	auto exists = [](const std::string &p) { return p == kStudentPath || p == "C:\\notes.txt"; };
	CHECK_EQ(checkBitstream("", exists), BitstreamCheck::Empty);
	CHECK_EQ(checkBitstream("   ", exists), BitstreamCheck::Empty);
	CHECK_EQ(checkBitstream("C:\\missing.bit", exists), BitstreamCheck::NotFound);
	CHECK_EQ(checkBitstream("C:\\notes.txt", exists), BitstreamCheck::UnknownExtension);
	CHECK_EQ(checkBitstream(kStudentPath, exists), BitstreamCheck::Ok);
}

TEST(open_file_filter_format)
{
	std::string f = openFileFilter();
	CHECK(f.size() > 4);
	CHECK(f[f.size() - 1] == '\0' && f[f.size() - 2] == '\0');
	CHECK(f.find("*.bit;") != std::string::npos);
	CHECK(f.find("*.svf") != std::string::npos);
	CHECK(f.find("*.*") != std::string::npos); // never artificially restricted
}

/* ------------------------------------------------------------------------ */
/* Backend discovery                                                        */
/* ------------------------------------------------------------------------ */

TEST(backend_discovery)
{
	const std::string dir = "C:\\Users\\Student Name\\Downloads\\ICSLab-FPGA-Programmer";
	auto c = backendCandidates(dir);
	CHECK_EQ(c.size(), size_t(2));
	CHECK_EQ(c[0], dir + "\\backend\\openFPGALoader.exe");
	CHECK_EQ(c[1], dir + "\\openFPGALoader.exe");

	std::set<std::string> fs;
	auto exists = [&](const std::string &p) { return fs.count(p) > 0; };
	CHECK_EQ(findBackend(dir, exists), ""); // missing
	fs.insert(dir + "\\openFPGALoader.exe");
	CHECK_EQ(findBackend(dir, exists), dir + "\\openFPGALoader.exe");
	fs.insert(dir + "\\backend\\openFPGALoader.exe");
	CHECK_EQ(findBackend(dir, exists), dir + "\\backend\\openFPGALoader.exe"); // preferred
	CHECK_EQ(findBackend(dir + "\\", exists), dir + "\\backend\\openFPGALoader.exe");
	CHECK_EQ(bridgeDirectory(dir + "\\backend\\openFPGALoader.exe"),
		dir + "\\backend\\share\\openFPGALoader");
}

TEST(backend_missing_message)
{
	auto info = backendMissingInfo("C:\\App");
	CHECK_EQ(info.kind, FailureKind::BackendMissing);
	CHECK(formatFailure(info).find("C:\\App\\backend\\openFPGALoader.exe") != std::string::npos);
	CHECK(formatFailure(info).find("ZIP") != std::string::npos);
}

/* ------------------------------------------------------------------------ */
/* Output parsing                                                           */
/* ------------------------------------------------------------------------ */

TEST(parse_board_list)
{
	const std::string out =
		"board name                 cable_name         fpga_part                \r\n"
		"basys3                     digilent           xc7a35tcpg236            \r\n"
		"de10lite                   usb-blaster        Undefined                \r\n"
		"\x1b[94mnexysVideo                 digilent_b         xc7a200tsbg484\x1b[0m\r\n"
		"\r\n";
	auto b = parseBoardList(out);
	CHECK_EQ(b.size(), size_t(3));
	if (b.size() == 3) {
		CHECK_EQ(b[0].name, "basys3");
		CHECK_EQ(b[0].cable, "digilent");
		CHECK_EQ(b[0].fpgaPart, "xc7a35tcpg236");
		CHECK_EQ(b[1].fpgaPart, "");
		CHECK_EQ(b[2].name, "nexysVideo");
		CHECK_EQ(b[2].fpgaPart, "xc7a200tsbg484");
	}
	CHECK_EQ(boardsMatchingModel(b, "xc7a35t"), (std::vector<std::string>{"basys3"}));
	CHECK(boardsMatchingModel(b, "").empty());
}

TEST(parse_cable_list)
{
	const std::string out =
		"cable name               vid:pid\n"
		"digilent                 0x0403:6010\n"
		"ft2232                   0x0403:6010\n"
		"usb-blaster              0x09fb:6001\n\n";
	CHECK_EQ(parseCableList(out), (std::vector<std::string>{"digilent", "ft2232", "usb-blaster"}));
}

TEST(parse_lab_boards)
{
	auto l = parseLabBoards("# comment\nbasys3\n  nexysVideo = Nexys Video (room 101) # x\n\n=bad\n");
	CHECK_EQ(l.size(), size_t(2));
	if (l.size() == 2) {
		CHECK_EQ(l[0].name, "basys3");
		CHECK_EQ(l[0].label, "Digilent Basys 3");
		CHECK_EQ(l[1].name, "nexysVideo");
		CHECK_EQ(l[1].label, "Nexys Video (room 101)");
	}
	CHECK(!defaultLabBoards().empty());
}

TEST(line_splitter)
{
	std::vector<std::string> lines;
	auto cb = [&](const std::string &l) { lines.push_back(l); };
	LineSplitter s;
	const std::string chunk1 = "a\r\nb\rLoad";
	const std::string chunk2 = "ing: [==] 10.00%\r\n\nlast";
	s.feed(chunk1.data(), chunk1.size(), cb);
	s.feed(chunk2.data(), chunk2.size(), cb);
	s.flush(cb);
	CHECK_EQ(lines, (std::vector<std::string>{"a", "b", "Loading: [==] 10.00%", "", "last"}));
}

TEST(progress_parsing)
{
	ProgressUpdate u;
	CHECK(parseProgressLine("Loading: [=====                                             ] 9.87%", u));
	CHECK(u.hasPercent);
	CHECK(u.percent > 9.86 && u.percent < 9.88);
	CHECK_EQ(u.phase, "Loading");
	CHECK(parseProgressLine("\x1b[94mWriting: [==================================================] 100.00%\x1b[0m", u));
	CHECK(u.percent == 100.0);
	CHECK_EQ(describePhase(u.phase), "Writing flash");
	CHECK(parseProgressLine("Erasing: [          ] 0.00%", u));
	CHECK_EQ(describePhase(u.phase), "Erasing flash");
	CHECK_EQ(describePhase("Load SRAM"), "Programming FPGA");
	CHECK_EQ(describePhase("Verifying"), "Verifying");
	// not progress lines
	CHECK(!parseProgressLine("Open file DONE", u));
	CHECK(!parseProgressLine("Parse file DONE", u));
	CHECK(!parseProgressLine("Error: [abc] 10%", u));
	CHECK(!parseProgressLine("100%", u));
}

TEST(parse_scan_usb)
{
	const std::string out =
		"Bus device vid:pid       probe_type manufacturer serial        product\n"
		"001 005    0x0403:0x6010 FTDI2232   Digilent     210183A8C1E2  Digilent Adept USB Device\n"
		"001 007    0x09fb:0x6001 usb-blaster Altera      none          USB-Blaster\n";
	auto p = parseScanUsb(out);
	CHECK_EQ(p.size(), size_t(2));
	if (p.size() == 2) {
		CHECK_EQ(p[0].vidPid, "0x0403:0x6010");
		CHECK_EQ(p[0].probeType, "FTDI2232");
		CHECK_EQ(p[0].manufacturer, "Digilent");
		CHECK_EQ(p[0].product, "Digilent Adept USB Device");
		CHECK_EQ(p[1].probeType, "usb-blaster");
	}
	CHECK_EQ(cableForProbeType("FTDI2232"), "ft2232");
	CHECK_EQ(cableForProbeType("ft232H"), "ft232");
	CHECK_EQ(cableForProbeType("usb-blaster"), "usb-blaster");
	CHECK_EQ(cableForProbeType("unknown FTDI"), "");
}

TEST(parse_detect)
{
	// Format of main.cpp "--detect" output
	const std::string out =
		"Jtag frequency : requested 6.00MHz   -> real 6.00MHz  \n"
		"index 0:\n"
		"\tidcode 0x362d093\n"
		"\tmanufacturer xilinx\n"
		"\tfamily artix a7 35t\n"
		"\tmodel  xc7a35\n"
		"\tirlength 6\n"
		"index 1:\n"
		"\tidcode   0x1234567\n"
		"\ttype     ARM cortex A9\n"
		"\tirlength 4\n";
	auto d = parseDetect(out);
	CHECK_EQ(d.size(), size_t(2));
	if (d.size() == 2) {
		CHECK_EQ(d[0].idcode, 0x362d093u);
		CHECK_EQ(d[0].manufacturer, "xilinx");
		CHECK_EQ(d[0].family, "artix a7 35t");
		CHECK_EQ(d[0].model, "xc7a35");
		CHECK_EQ(d[1].type, "ARM cortex A9");
		CHECK_EQ(d[1].model, "");
	}
}

/* ------------------------------------------------------------------------ */
/* Error return codes                                                       */
/* ------------------------------------------------------------------------ */

TEST(error_return_code_handling)
{
	CHECK_EQ(classifyResult(0, "anything Error: foo").kind, FailureKind::None);
	CHECK_EQ(classifyResult(1, "unable to open ftdi device: -3 (device not found)\nJTAG init failed with: unable to open ftdi device").kind,
		FailureKind::CableNotFound);
	CHECK_EQ(classifyResult(1, "unable to open ftdi device: -5 (unable to claim usb device. Make sure the default FTDI driver is not in use)").kind,
		FailureKind::UsbDriver);
	CHECK_EQ(classifyResult(0xFFFFFFFF, "Error: can't open device with vid:vid = 0x0403:0x6010. Error code -12 LIBUSB_ERROR_NOT_SUPPORTED").kind,
		FailureKind::UsbDriver);
	CHECK_EQ(classifyResult(1, "Error: no device found").kind, FailureKind::NoJtagDevice);
	CHECK_EQ(classifyResult(1, "Error: cannot find board 'foo'").kind, FailureKind::WrongBoard);
	CHECK_EQ(classifyResult(1, "Open file FAIL: unable to open 'C:\\x.bit': No such file").kind,
		FailureKind::BadBitstream);
	CHECK_EQ(classifyResult(1, "Parse file FAIL: unable to parse 'x.bit'").kind, FailureKind::BadBitstream);
	CHECK_EQ(classifyResult(1, "Error: more than one FPGA found").kind, FailureKind::MultipleFpga);
	CHECK_EQ(classifyResult(1, "Erasing: ... SPI flash write failed").kind, FailureKind::FlashFailed);
	CHECK_EQ(classifyResult(42, "something unexpected").kind, FailureKind::Unknown);
	auto info = classifyResult(1, "JTAG init failed with: unable to open ftdi device");
	std::string text = formatFailure(info);
	CHECK(text.find("Unable to communicate with the FPGA programmer.") != std::string::npos);
	CHECK(text.find("USB cable connection") != std::string::npos);
	CHECK(text.find("FPGA board power") != std::string::npos);
	CHECK(text.find("selected board") != std::string::npos);
	CHECK(text.find("USB driver") != std::string::npos);
	CHECK(classifyResult(42, "x").explanation.find("42") != std::string::npos);
}

TEST(usb_hardware_ids)
{
	uint16_t vid = 0, pid = 0;
	int mi = 0;
	CHECK(parseUsbHardwareId("USB\\VID_09FB&PID_6010&REV_0100", vid, pid, mi));
	CHECK_EQ(unsigned(vid), 0x09FBu);
	CHECK_EQ(unsigned(pid), 0x6010u);
	CHECK_EQ(mi, -1);
	CHECK(parseUsbHardwareId("usb\\vid_0403&pid_6010&mi_01", vid, pid, mi));
	CHECK_EQ(unsigned(pid), 0x6010u);
	CHECK_EQ(mi, 1);
	CHECK(!parseUsbHardwareId("HID\\VID_09FB&PID_6001", vid, pid, mi));
	CHECK(!parseUsbHardwareId("USB\\ROOT_HUB30", vid, pid, mi));
	CHECK(isIntelBlaster(0x09FB, 0x6001));
	CHECK(isIntelBlaster(0x09FB, 0x6010));
	CHECK(isIntelBlaster(0x09FB, 0x6810));
	CHECK(!isIntelBlaster(0x0403, 0x6010));
	CHECK_EQ(blasterCableName(0x09FB, 0x6001), "usb-blaster");
	CHECK_EQ(blasterCableName(0x09FB, 0x6010), "usb-blasterII");
	CHECK_EQ(blasterCableName(0x09FB, 0x6810), "usb-blasterII");
	CHECK_EQ(blasterCableName(0x0403, 0x6010), "");
	CHECK(cableNeedsBlasterFirmware("usb-blasterII"));
	CHECK(!cableNeedsBlasterFirmware("usb-blaster"));
}

TEST(device_problem_codes)
{
	CHECK(describeDeviceProblem(52).find("digital signature") != std::string::npos);
	CHECK(describeDeviceProblem(39).find("Memory Integrity") != std::string::npos);
	CHECK(describeDeviceProblem(28).find("no driver") != std::string::npos);
	CHECK(describeDeviceProblem(999).find("999") != std::string::npos);
	CHECK(isWinUsbService("WinUSB"));
	CHECK(isWinUsbService("winusb"));
	CHECK(!isWinUsbService("usbblstr"));
}

TEST(blaster_firmware_candidates)
{
	auto roots = quartusInstallRoots("C:");
	CHECK(!roots.empty());
	CHECK_EQ(roots[0], "C:\\intelFPGA_lite");
	auto c = blasterFirmwareCandidates("D:\\Lab App", "E:\\fw\\blaster_6810.hex",
		"C:\\intelFPGA\\23.1\\quartus", {"C:\\intelFPGA_lite\\20.1"});
	CHECK(c.size() >= 5);
	if (c.size() >= 3) {
		CHECK_EQ(c[0], "E:\\fw\\blaster_6810.hex");
		CHECK_EQ(c[1], "D:\\Lab App\\firmware\\blaster_6810.hex");
		CHECK_EQ(c[2], "C:\\intelFPGA\\23.1\\quartus\\bin64\\blaster_6810.hex");
	}
	bool lite = false, prog = false;
	for (const auto &x : c) {
		lite |= x == "C:\\intelFPGA_lite\\20.1\\quartus\\bin64\\blaster_6810.hex";
		prog |= x == "C:\\intelFPGA_lite\\20.1\\qprogrammer\\bin64\\blaster_6810.hex";
	}
	CHECK(lite);
	CHECK(prog);
	// no configured path: starts with the app firmware folder
	c = blasterFirmwareCandidates("D:\\App", "", "", {});
	CHECK_EQ(c.front(), "D:\\App\\firmware\\blaster_6810.hex");
}

TEST(lab_boards_with_cable)
{
	auto l = parseLabBoards("de2_standard = Terasic DE2-Standard | cable=usb-blasterII\n"
		"basys3 | cable = digilent\n");
	CHECK_EQ(l.size(), size_t(2));
	if (l.size() == 2) {
		CHECK_EQ(l[0].name, "de2_standard");
		CHECK_EQ(l[0].label, "Terasic DE2-Standard");
		CHECK_EQ(l[0].cable, "usb-blasterII");
		CHECK_EQ(l[1].name, "basys3");
		CHECK_EQ(l[1].label, "Digilent Basys 3");
		CHECK_EQ(l[1].cable, "digilent");
	}
	bool de2 = false;
	for (const auto &b : defaultLabBoards())
		de2 |= b.name == "de2_standard" && b.cable == "usb-blasterII";
	CHECK(de2);
}

TEST(blaster_ii_commands)
{
	const std::string fw = "C:\\intelFPGA lite\\20.1\\quartus\\bin64\\blaster_6810.hex";
	ProgramRequest r;
	r.cable = "usb-blasterII";
	r.bitstream = "C:\\lab\\top.rbf";
	r.probeFirmware = fw;
	CHECK_EQ(buildProgramArgs(r), (std::vector<std::string>{"-c", "usb-blasterII",
		"--probe-firmware", fw, "--write-sram", "--bitstream", "C:\\lab\\top.rbf"}));
	CHECK_EQ(buildDetectArgs("", "usb-blasterII", fw), (std::vector<std::string>{"-c",
		"usb-blasterII", "--probe-firmware", fw, "--detect"}));
	auto argv = splitWindowsCommandLine(buildCommandLine("x.exe", buildProgramArgs(r)));
	CHECK_EQ(argv.size(), size_t(8));
	if (argv.size() == 8)
		CHECK_EQ(argv[4], fw);
}

TEST(blaster_error_mapping)
{
	CHECK_EQ(classifyResult(1, "missing FX2 firmware\nuse --probe-firmware with something\n"
		"JTAG init failed with: usbBlasterII: missing firmware").kind, FailureKind::BlasterFirmware);
	// firmware given but no blaster connected (real backend output)
	CHECK_EQ(classifyResult(1, "Waiting reload: [====] 100.00%\n\nFail\n"
		"JTAG init failed with: FX2: fail to open device").kind, FailureKind::CableNotFound);
	CHECK_EQ(classifyResult(1, "unable to open ftdi device: -3 (device not found)\n"
		"JTAG init failed with: std::exception").kind, FailureKind::CableNotFound);
	// bitstream open errors still detected
	CHECK_EQ(classifyResult(1, "Error: fail to open C:\\x.bit").kind, FailureKind::BadBitstream);
}

TEST(strip_ansi)
{
	CHECK_EQ(stripAnsi("\x1b[31mError\x1b[0m done"), "Error done");
	CHECK_EQ(trim("  a b \r\n"), "a b");
}

/* ------------------------------------------------------------------------ */
/* Windows process tests                                                    */
/* ------------------------------------------------------------------------ */

#ifdef _WIN32
static std::string g_fake, g_real, g_tmp;

static std::vector<std::string> argLines(const std::string &out)
{
	std::vector<std::string> v;
	LineSplitter s;
	auto cb = [&](const std::string &l) { if (l.rfind("ARG", 0) == 0) v.push_back(l.substr(l.find('=') + 1)); };
	s.feed(out.data(), out.size(), cb);
	s.flush(cb);
	return v;
}

static bool copyTo(const std::string &from, const std::string &to)
{
	return CopyFileW(widen(from).c_str(), widen(to).c_str(), FALSE) != 0;
}

static void writeFile(const std::string &path, const std::string &data)
{
	HANDLE h = CreateFileW(widen(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	DWORD w = 0;
	WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &w, nullptr);
	CloseHandle(h);
}

TEST(win_process_argument_quoting_real)
{
	// The fake backend lives in a directory with spaces, like a student's
	// Downloads folder, and receives a path with spaces.
	const std::string args0 = "C:\\Users\\Student Name\\Desktop\\Lab 3\\final design.bit";
	std::vector<std::string> args = {"-b", "basys3", "--write-sram", "--bitstream", args0,
		"C:\\trailing\\", "a\"b", ""};
	auto r = runProcess(g_fake, args, nullptr);
	CHECK(r.launched);
	CHECK_EQ(r.exitCode, 0);
	CHECK_EQ(argLines(r.output), args);
}

TEST(win_stdout_stderr_capture)
{
	std::vector<std::string> lines;
	auto r = runProcess(g_fake, {"--fake-stderr", "--fake-exit", "3"},
		[&](const std::string &l) { lines.push_back(l); });
	CHECK(r.launched);
	CHECK_EQ(r.exitCode, 3);
	CHECK(r.output.find("ARG1=--fake-stderr") != std::string::npos);            // stdout
	CHECK(r.output.find("Error: this line goes to stderr") != std::string::npos); // stderr
	bool sawErr = false;
	for (const auto &l : lines)
		sawErr |= l == "Error: this line goes to stderr";
	CHECK(sawErr);
}

TEST(win_error_return_codes)
{
	for (int code : {0, 1, 2, 255}) {
		auto r = runProcess(g_fake, {"--fake-exit", std::to_string(code)}, nullptr);
		CHECK(r.launched);
		CHECK_EQ(r.exitCode, code);
		CHECK_EQ(classifyResult(r.exitCode, r.output).kind == FailureKind::None, code == 0);
	}
}

TEST(win_progress_streaming)
{
	std::vector<double> pct;
	auto r = runProcess(g_fake, {"--fake-progress"}, [&](const std::string &l) {
		ProgressUpdate u;
		if (parseProgressLine(l, u))
			pct.push_back(u.percent);
	});
	CHECK(r.launched);
	CHECK_EQ(pct.size(), size_t(3));
	if (pct.size() == 3) {
		CHECK(pct[0] == 0.0);
		CHECK(pct[1] == 50.0);
		CHECK(pct[2] == 100.0);
	}
}

TEST(win_stdin_is_not_interactive)
{
	auto r = runProcess(g_fake, {"--fake-stdin"}, nullptr);
	CHECK(r.launched);
	CHECK(r.output.find("STDIN_BYTES=0") != std::string::npos);
}

TEST(win_backend_missing)
{
	const std::string dir = g_tmp + "\\empty app dir";
	CreateDirectoryW(widen(dir).c_str(), nullptr);
	CHECK_EQ(findBackend(dir, fileExists), "");
	auto r = runProcess(dir + "\\backend\\openFPGALoader.exe", {"--detect"}, nullptr);
	CHECK(!r.launched);
	std::printf("  launch error for missing backend: %lu\n", r.launchError);
	CHECK(r.launchError == ERROR_FILE_NOT_FOUND || r.launchError == ERROR_PATH_NOT_FOUND);
	CHECK(!describeWin32Error(r.launchError).empty());
}

TEST(win_backend_discovery_filesystem)
{
	const std::string dir = g_tmp + "\\ICSLab FPGA Programmer test";
	CreateDirectoryW(widen(dir).c_str(), nullptr);
	CreateDirectoryW(widen(dir + "\\backend").c_str(), nullptr);
	CHECK(copyTo(g_fake, dir + "\\backend\\openFPGALoader.exe"));
	std::string found = findBackend(dir, fileExists);
	CHECK_EQ(found, dir + "\\backend\\openFPGALoader.exe");
	auto r = runProcess(found, {"--detect"}, nullptr);
	CHECK(r.launched);
	CHECK_EQ(argLines(r.output), (std::vector<std::string>{"--detect"}));
	CHECK(fileExists(found));
	CHECK(!fileExists(dir + "\\backend"));
	CHECK(dirExists(dir + "\\backend"));
}

TEST(win_backend_safe_path)
{
	const std::string ascii = g_tmp + "\\Lab 3 final design.bit";
	writeFile(ascii, "x");
	CHECK_EQ(backendSafePath(ascii), ascii);
}

TEST(win_driver_setup_without_devices)
{
	// No USB-Blaster under Wine/CI: must return an empty list, not fail.
	auto devs = listIntelBlasters();
	CHECK(devs.empty());
	CHECK_EQ(blasterDriverHint(devs), "");
	UsbDeviceStatus st;
	st.vid = 0x09FB;
	st.pid = 0x6010;
	st.hasProblem = true;
	st.problem = 52;
	CHECK(st.summary().find("code 52") != std::string::npos);
	CHECK(!st.usableByOpenFPGALoader());
	CHECK(blasterDriverHint({st}).find("USB Setup") != std::string::npos);
	st.hasProblem = false;
	st.problem = 0;
	st.service = "WinUSB";
	CHECK(st.usableByOpenFPGALoader());
	CHECK_EQ(blasterDriverHint({st}), "");
	// helper refuses malformed command lines
	wchar_t a0[] = L"x.exe", a1[] = L"--driver-setup";
	wchar_t *argv[] = {a0, a1};
	CHECK_EQ(driverSetupHelperMain(2, argv), 2);
}

TEST(win_blaster_firmware_lookup)
{
	const std::string app = g_tmp + "\\Lab App";
	CreateDirectoryW(widen(app).c_str(), nullptr);
	CreateDirectoryW(widen(app + "\\firmware").c_str(), nullptr);
	DeleteFileW(widen(app + "\\firmware\\blaster_6810.hex").c_str());
	const std::string configured = g_tmp + "\\my fw.hex";
	DeleteFileW(widen(configured).c_str());
	CHECK_EQ(findBlasterFirmware(app, configured), ""); // nothing anywhere
	writeFile(app + "\\firmware\\blaster_6810.hex", ":00000001FF\n");
	CHECK_EQ(findBlasterFirmware(app, configured), app + "\\firmware\\blaster_6810.hex");
	writeFile(configured, ":00000001FF\n");
	CHECK_EQ(findBlasterFirmware(app, configured), configured); // instructor's choice first
}

TEST(win_real_backend_integration)
{
	if (g_real.empty()) {
		std::printf("  (skipped: no real openFPGALoader.exe given)\n");
		return;
	}
	auto ver = runProcess(g_real, buildVersionArgs(), nullptr);
	CHECK(ver.launched);
	CHECK(ver.output.find("openFPGALoader v") != std::string::npos);

	auto boards = runProcess(g_real, buildListBoardsArgs(), nullptr);
	CHECK(boards.launched);
	auto b = parseBoardList(boards.output);
	CHECK(b.size() > 100);
	std::set<std::string> names;
	for (const auto &x : b)
		names.insert(x.name);
	for (const auto &lb : defaultLabBoards()) {
		if (!lb.cable.empty())
			continue; // cable-only lab board (no openFPGALoader board entry)
		if (!names.count(lb.name))
			std::printf("  lab board unknown to backend: %s\n", lb.name.c_str());
		CHECK(names.count(lb.name) == 1);
	}

	auto cables = runProcess(g_real, buildListCablesArgs(), nullptr);
	auto c = parseCableList(cables.output);
	CHECK(c.size() > 20);

	// Programming without hardware must fail cleanly with a useful message.
	const std::string dir = g_tmp + "\\Student Name\\Lab 3";
	CreateDirectoryW(widen(g_tmp + "\\Student Name").c_str(), nullptr);
	CreateDirectoryW(widen(dir).c_str(), nullptr);
	writeFile(dir + "\\final design.bit", "not a real bitstream");
	ProgramRequest req;
	req.board = "basys3";
	req.bitstream = dir + "\\final design.bit";
	std::vector<std::string> lines;
	auto r = runProcess(g_real, buildProgramArgs(req), [&](const std::string &l) { lines.push_back(l); });
	CHECK(r.launched);
	CHECK(r.exitCode != 0);
	auto info = classifyResult(r.exitCode, r.output);
	CHECK(info.kind == FailureKind::CableNotFound || info.kind == FailureKind::UsbDriver);
	std::printf("  real backend (no hardware) exit=%d -> \"%s\"\n", r.exitCode, info.title.c_str());

	// Unknown board
	req.board = "no_such_board";
	r = runProcess(g_real, buildProgramArgs(req), nullptr);
	CHECK(r.exitCode != 0);
	CHECK_EQ(classifyResult(r.exitCode, r.output).kind, FailureKind::WrongBoard);

	// Detect without hardware
	r = runProcess(g_real, buildDetectArgs("basys3", ""), nullptr);
	CHECK(r.exitCode != 0);
	CHECK(parseDetect(r.output).empty());

	// USB-Blaster II without firmware / without device
	r = runProcess(g_real, buildDetectArgs("", "usb-blasterII"), nullptr);
	CHECK(r.exitCode != 0);
	CHECK_EQ(classifyResult(r.exitCode, r.output).kind, FailureKind::BlasterFirmware);
	writeFile(g_tmp + "\\fw test.hex", ":00000001FF\n");
	r = runProcess(g_real, buildDetectArgs("", "usb-blasterII", g_tmp + "\\fw test.hex"), nullptr);
	CHECK(r.exitCode != 0);
	CHECK_EQ(classifyResult(r.exitCode, r.output).kind, FailureKind::CableNotFound);

	// USB-Blaster (DE2-115) without device
	r = runProcess(g_real, buildDetectArgs("", "usb-blaster"), nullptr);
	CHECK(r.exitCode != 0);
	CHECK_EQ(classifyResult(r.exitCode, r.output).kind, FailureKind::CableNotFound);
}
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	g_fake = currentExeDir() + "\\fake_openfpgaloader.exe";
	if (argc > 1)
		g_real = argv[1];
	wchar_t tmp[MAX_PATH];
	GetTempPathW(MAX_PATH, tmp);
	g_tmp = trim(narrow(tmp));
	if (!g_tmp.empty() && g_tmp.back() == '\\')
		g_tmp.pop_back();
	g_tmp += "\\icslab tests";
	CreateDirectoryW(widen(g_tmp).c_str(), nullptr);
	if (!fileExists(g_fake)) {
		std::printf("fake_openfpgaloader.exe not found next to the test executable\n");
		return 2;
	}
#else
	(void)argc;
	(void)argv;
#endif
	for (const auto &t : tests()) {
		g_test = t.name;
		int before = g_failures;
		t.fn();
		std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[FAIL]", t.name);
	}
	std::printf("\n%d checks, %d failures, %zu tests\n", g_checks, g_failures, tests().size());
	return g_failures ? 1 : 0;
}
