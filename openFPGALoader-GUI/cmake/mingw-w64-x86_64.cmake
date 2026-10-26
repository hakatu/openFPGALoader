# Cross-compile openFPGALoader GUI for 64-bit Windows with MinGW-w64.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_prefix x86_64-w64-mingw32)
find_program(CMAKE_C_COMPILER NAMES ${_prefix}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${_prefix}-g++)
find_program(CMAKE_RC_COMPILER NAMES ${_prefix}-windres)

if(NOT CMAKE_CXX_COMPILER)
	message(FATAL_ERROR "${_prefix}-g++ not found. Install the mingw-w64 toolchain.")
endif()

set(CMAKE_FIND_ROOT_PATH /usr/${_prefix})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Run Windows test executables through Wine when available.
find_program(_wine NAMES wine64 wine PATHS /usr/lib/wine)
if(_wine)
	set(CMAKE_CROSSCOMPILING_EMULATOR ${_wine})
endif()
