# ICSLab FPGA Programmer - simple Windows GUI for openFPGALoader
#
# Windows (cross) build:
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Host build (Linux/macOS): only the portable core and its unit tests.

cmake_minimum_required(VERSION 3.16)
project(ICSLabFPGAProgrammer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_definitions(
	ICSLAB_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	ICSLAB_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	ICSLAB_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(-Wall -Wextra)
endif()

enable_testing()

# Portable core (command generation, parsing, error mapping)
add_library(icslab_core STATIC src/OpenFPGALoaderBackend.cpp)
target_include_directories(icslab_core PUBLIC src)

if(WIN32)
	enable_language(RC)
	add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)

	# Fully static: no MinGW runtime DLLs to ship
	if(MINGW)
		set(ICSLAB_STATIC_FLAGS -static -static-libgcc -static-libstdc++)
	endif()

	add_library(icslab_win STATIC src/ProcessRunner.cpp src/Settings.cpp src/DriverSetup.cpp)
	target_link_libraries(icslab_win PUBLIC icslab_core setupapi cfgmgr32 shell32)

	add_executable(icslab_fpga_programmer WIN32
		src/main.cpp
		src/MainWindow.cpp
		resources/app.rc)
	set_target_properties(icslab_fpga_programmer PROPERTIES OUTPUT_NAME "ICSLab FPGA Programmer")
	target_link_libraries(icslab_fpga_programmer PRIVATE icslab_win comctl32 comdlg32 shell32)
	if(MINGW)
		target_link_options(icslab_fpga_programmer PRIVATE -municode -mwindows ${ICSLAB_STATIC_FLAGS} -s)
	endif()

	add_executable(fake_openfpgaloader tests/fake_openfpgaloader.cpp)
	target_link_libraries(fake_openfpgaloader PRIVATE shell32)
	if(MINGW)
		target_link_options(fake_openfpgaloader PRIVATE ${ICSLAB_STATIC_FLAGS})
	endif()

	add_executable(icslab_tests tests/tests.cpp)
	target_link_libraries(icslab_tests PRIVATE icslab_win)
	if(MINGW)
		target_link_options(icslab_tests PRIVATE ${ICSLAB_STATIC_FLAGS})
	endif()
	add_dependencies(icslab_tests fake_openfpgaloader)
	if(CMAKE_CROSSCOMPILING_EMULATOR OR NOT CMAKE_CROSSCOMPILING)
		add_test(NAME icslab_tests COMMAND icslab_tests)
	endif()
else()
	add_executable(icslab_tests tests/tests.cpp)
	target_link_libraries(icslab_tests PRIVATE icslab_core)
	add_test(NAME icslab_tests COMMAND icslab_tests)
endif()
