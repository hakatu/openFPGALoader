// SPDX-License-Identifier: Apache-2.0
// openFPGALoader GUI - persisted user choices.
//
// Stored in %APPDATA%\openFPGALoader GUI\settings.ini (per user, no
// administrator rights needed). The last bitstream *file* is intentionally
// not remembered: only its directory, so nothing is ever programmed
// implicitly at start-up.

#pragma once

#include <string>

namespace oflgui {

struct Settings {
	std::string board;      // "" = Auto Detect / Generic JTAG
	std::string cable;      // "" = automatic
	bool flash = false;     // programming mode
	std::string lastDir;    // last bitstream directory
	std::string blasterFirmware; // blaster_6810.hex chosen by the instructor

	static std::string defaultPath();
	void load(const std::string &iniPath);
	bool save(const std::string &iniPath) const;
};

} // namespace oflgui
