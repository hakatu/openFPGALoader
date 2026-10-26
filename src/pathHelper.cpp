// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 *
 * Modified 2026 for the openFPGALoader Windows GUI build: keep native Windows paths unchanged and fall back
 * to the original path when cygpath is unavailable (plain Windows without
 * MSYS2/Cygwin), so spiOverJtag bridges are found on student PCs.
 */

#if defined (_WIN64) || defined (_WIN32)
#include "pathHelper.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <array>
#include <cctype>
#include <regex>

std::string PathHelper::absolutePath(std::string input_path) {

    /* Already a native Windows path (drive letter or UNC): nothing to
     * convert. Avoids depending on cygpath, which only exists in
     * MSYS2/Cygwin environments.
     */
    if ((input_path.size() >= 2 && std::isalpha(static_cast<unsigned char>(input_path[0]))
            && input_path[1] == ':') ||
            input_path.rfind("\\\\", 0) == 0 || input_path.rfind("//", 0) == 0)
        return input_path;

    /* Attempt to execute cygpath */
    std::string cmd = std::string("cygpath -m " + input_path);

    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        /* If cygpath fails to run, return original path */
        return input_path;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    /* Trim trailing newline */
    static const std::regex tws{"[[:space:]]*$", std::regex_constants::extended};
    result = std::regex_replace(result, tws, "");

    /* cygpath not available (plain Windows): keep original path */
    if (result.empty())
        return input_path;
    return result;
}

#endif