#!/usr/bin/env bash
# Build the portable Windows release of ICSLab FPGA Programmer from Linux.
#
#   ICSLab-FPGA-Programmer/scripts/build-release.sh
#
# Steps: openFPGALoader backend (official MinGW-w64 cross build, static) ->
# GUI -> tests (through Wine when available) -> release folder -> dependency
# check -> ZIP + SHA-256.
#
# Requirements (Debian/Ubuntu):
#   sudo apt install mingw-w64 libz-mingw-w64-dev cmake ninja-build pkg-config \
#                    p7zip-full zip file   (+ wine64 to run the tests)
#
# Environment overrides:
#   OFL_SOURCE / OFL_REF   see openfpgaloader.pin
#   WORK=<dir>             build directory   (default: ICSLab-FPGA-Programmer/build-release)
#   OUT=<dir>              output directory  (default: ICSLab-FPGA-Programmer/release)
#   LIBFTDI_TARBALL=<file> pre-downloaded libftdi1-1.5.tar.bz2 (offline builds)
#   RUN_TESTS=0|1|auto     run the test-suite with Wine (default: auto)
#   CORRESPONDING_SOURCE=1 also build the LGPL corresponding-source ZIP

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUI_DIR="$(cd "$HERE/.." && pwd)"
REPO_DIR="$(cd "$GUI_DIR/.." && pwd)"

# shellcheck disable=SC1091
source <(grep -E '^OFL_[A-Z_]+=' "$GUI_DIR/openfpgaloader.pin" | sed 's/^/: ${/; s/=/:=/; s/$/}/')
WORK="${WORK:-$GUI_DIR/build-release}"
OUT="${OUT:-$GUI_DIR/release}"
RUN_TESTS="${RUN_TESTS:-auto}"
CORRESPONDING_SOURCE="${CORRESPONDING_SOURCE:-0}"
JOBS="$(nproc 2>/dev/null || echo 2)"

VERSION="$(sed -n 's/^project(ICSLabFPGAProgrammer VERSION \([0-9.]*\).*/\1/p' "$GUI_DIR/CMakeLists.txt")"
APP_NAME="ICSLab FPGA Programmer"
PKG_NAME="ICSLab-FPGA-Programmer"
ZIP_NAME="$PKG_NAME-v$VERSION-win64.zip"

log() { printf '\n==> %s\n' "$*"; }

mkdir -p "$WORK" "$OUT"

# ---------------------------------------------------------------------------
log "1/7 openFPGALoader sources ($OFL_SOURCE ${OFL_REF})"
# ---------------------------------------------------------------------------
if [ "$OFL_SOURCE" = "local" ]; then
	OFL_DIR="$REPO_DIR"
	[ -f "$OFL_DIR/src/main.cpp" ] || { echo "no openFPGALoader sources in $OFL_DIR"; exit 1; }
	OFL_COMMIT="$(git -C "$OFL_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
	if ! git -C "$OFL_DIR" diff --quiet HEAD -- src 2>/dev/null; then
		OFL_COMMIT="$OFL_COMMIT (with uncommitted changes)"
	fi
elif [ "$OFL_SOURCE" = "upstream" ]; then
	[ -n "$OFL_REF" ] || { echo "OFL_REF must be set for OFL_SOURCE=upstream"; exit 1; }
	OFL_DIR="$WORK/openFPGALoader-src"
	rm -rf "$OFL_DIR"
	git init -q "$OFL_DIR"
	git -C "$OFL_DIR" fetch -q --depth 1 "$OFL_UPSTREAM_URL" "$OFL_REF"
	git -C "$OFL_DIR" checkout -q FETCH_HEAD
	OFL_COMMIT="$(git -C "$OFL_DIR" rev-parse HEAD)"
else
	echo "unknown OFL_SOURCE=$OFL_SOURCE"; exit 1
fi

# Apply ICSLab patches (idempotent)
for p in "$GUI_DIR"/patches/*.patch; do
	[ -e "$p" ] || continue
	if git -C "$OFL_DIR" apply --reverse --check "$p" 2>/dev/null; then
		echo "patch already applied: $(basename "$p")"
	else
		git -C "$OFL_DIR" apply "$p"
		echo "patch applied: $(basename "$p")"
	fi
done
OFL_VERSION="$(sed -n 's/^project(openFPGALoader VERSION "\([^"]*\)".*/\1/p' "$OFL_DIR/CMakeLists.txt")"
echo "openFPGALoader v$OFL_VERSION, commit $OFL_COMMIT"

# ---------------------------------------------------------------------------
log "2/7 Build openFPGALoader for Windows (upstream cross-compile procedure)"
# ---------------------------------------------------------------------------
BACKEND_BUILD="$WORK/backend"
BACKEND_STAGE="$WORK/backend-install"
if [ -n "${LIBFTDI_TARBALL:-}" ]; then
	mkdir -p "$BACKEND_BUILD/cross-deps/src"
	cp -n "$LIBFTDI_TARBALL" "$BACKEND_BUILD/cross-deps/src/libftdi1-1.5.tar.bz2" || true
fi
cmake -S "$OFL_DIR" -B "$BACKEND_BUILD" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$OFL_DIR/cmake/Toolchain-x86_64-w64-mingw32.cmake" \
	-DWINDOWS_CROSSCOMPILE=ON -DCROSS_COMPILE_DEPS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$BACKEND_BUILD" --parallel "$JOBS"
rm -rf "$BACKEND_STAGE"
cmake --install "$BACKEND_BUILD" --prefix "$BACKEND_STAGE"
x86_64-w64-mingw32-strip "$BACKEND_STAGE/bin/openFPGALoader.exe"

# ---------------------------------------------------------------------------
log "3/7 Build ICSLab GUI"
# ---------------------------------------------------------------------------
GUI_BUILD="$WORK/gui"
cmake -S "$GUI_DIR" -B "$GUI_BUILD" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$GUI_DIR/cmake/mingw-w64-x86_64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$GUI_BUILD" --parallel "$JOBS"

# ---------------------------------------------------------------------------
log "4/7 Tests"
# ---------------------------------------------------------------------------
WINE=""
for w in wine64 /usr/lib/wine/wine64 wine; do
	if command -v "$w" >/dev/null 2>&1; then WINE="$w"; break; fi
done
if [ "$RUN_TESTS" = "1" ] || { [ "$RUN_TESTS" = "auto" ] && [ -n "$WINE" ]; }; then
	[ -n "$WINE" ] || { echo "RUN_TESTS=1 but wine is not installed"; exit 1; }
	export WINEDEBUG=-all
	BACKEND_WIN="$("$WINE" winepath -w "$BACKEND_STAGE/bin/openFPGALoader.exe" 2>/dev/null | tr -d '\r')"
	(cd "$GUI_BUILD" && "$WINE" icslab_tests.exe "$BACKEND_WIN")
else
	echo "Wine not available: Windows tests skipped (they run on the windows-latest CI job)."
fi

# ---------------------------------------------------------------------------
log "5/7 Assemble release folder"
# ---------------------------------------------------------------------------
PKG="$OUT/$PKG_NAME"
rm -rf "$PKG"
mkdir -p "$PKG/backend/share" "$PKG/docs" "$PKG/LICENSES"
cp "$GUI_BUILD/$APP_NAME.exe" "$PKG/"
cp "$BACKEND_STAGE/bin/openFPGALoader.exe" "$PKG/backend/"
cp -r "$BACKEND_STAGE/share/openFPGALoader" "$PKG/backend/share/"
cp "$GUI_DIR"/LICENSES/* "$PKG/LICENSES/"
cp "$GUI_DIR/docs/student-guide.md" "$GUI_DIR/docs/windows-driver.md" "$PKG/docs/"
cp "$GUI_DIR/resources/lab-boards.txt" "$PKG/"
mkdir -p "$PKG/firmware"
to_crlf_early() { sed 's/\r$//; s/$/\r/' "$1" > "$2"; }
to_crlf_early "$GUI_DIR/resources/firmware-README.txt" "$PKG/firmware/README.txt"
to_crlf() { sed 's/\r$//; s/$/\r/' "$1" > "$2"; }
to_crlf "$GUI_DIR/resources/README.txt" "$PKG/README.txt"
to_crlf "$GUI_DIR/README-licenses.txt" "$PKG/README-licenses.txt"
{
	echo "openFPGALoader v$OFL_VERSION"
	echo "source: $OFL_SOURCE ${OFL_REF}"
	echo "commit: $OFL_COMMIT"
	echo "patches: $(cd "$GUI_DIR/patches" && ls ./*.patch 2>/dev/null | xargs -n1 basename | tr '\n' ' ')"
	echo "build:  MinGW-w64 cross build, static (CROSS_COMPILE_DEPS=ON: libusb 1.0.27, libftdi1 1.5, zlib)"
} | sed 's/$/\r/' > "$PKG/backend/VERSION.txt"

# ---------------------------------------------------------------------------
log "6/7 Verify executables and runtime dependencies"
# ---------------------------------------------------------------------------
# Only DLLs that ship with every Windows 7..11 installation are allowed.
ALLOWED='^(KERNEL32|USER32|GDI32|ADVAPI32|SHELL32|COMCTL32|COMDLG32|WS2_32|msvcrt|ole32|OLEAUT32|SHLWAPI|bcrypt|CRYPT32|SETUPAPI|CFGMGR32|WINUSB|HID)\.dll$'
DEPS_REPORT="$OUT/dependencies.txt"
: > "$DEPS_REPORT"
for exe in "$PKG/$APP_NAME.exe" "$PKG/backend/openFPGALoader.exe"; do
	info="$(file -b "$exe")"
	echo "$(basename "$exe"): $info" | tee -a "$DEPS_REPORT"
	case "$info" in
		PE32+\ executable*x86-64*) ;;
		*) echo "ERROR: not a 64-bit Windows PE executable"; exit 1 ;;
	esac
	dlls="$(x86_64-w64-mingw32-objdump -p "$exe" | sed -n 's/^\s*DLL Name: //p' | sort -u)"
	for d in $dlls; do
		echo "    imports $d" | tee -a "$DEPS_REPORT"
		if ! echo "$d" | grep -qiE "$ALLOWED"; then
			echo "ERROR: $(basename "$exe") depends on non-system DLL $d"; exit 1
		fi
	done
done
echo "OK: only Windows system DLLs are imported (no extra DLLs to ship)." | tee -a "$DEPS_REPORT"

# ---------------------------------------------------------------------------
log "7/7 ZIP + SHA-256"
# ---------------------------------------------------------------------------
rm -f "$OUT/$ZIP_NAME"
(cd "$OUT" && zip -qr9 "$ZIP_NAME" "$PKG_NAME")
(cd "$OUT" && sha256sum "$ZIP_NAME" "$PKG_NAME/$APP_NAME.exe" "$PKG_NAME/backend/openFPGALoader.exe" > SHA256SUMS.txt)

if [ "$CORRESPONDING_SOURCE" = "1" ]; then
	SRC_DIR="$WORK/corresponding-source/$PKG_NAME-v$VERSION-source"
	rm -rf "$WORK/corresponding-source"
	mkdir -p "$SRC_DIR"
	if git -C "$REPO_DIR" rev-parse HEAD >/dev/null 2>&1; then
		git -C "$REPO_DIR" archive --format=tar.gz -o "$SRC_DIR/repository-$(git -C "$REPO_DIR" rev-parse --short HEAD).tar.gz" HEAD
	fi
	cp "$BACKEND_BUILD/cross-deps/src/libftdi1-1.5.tar.bz2" "$SRC_DIR/"
	curl -fsSL -o "$SRC_DIR/libusb-1.0.27.tar.bz2" \
		https://github.com/libusb/libusb/releases/download/v1.0.27/libusb-1.0.27.tar.bz2
	(cd "$WORK/corresponding-source" && zip -qr9 "$OUT/$PKG_NAME-v$VERSION-corresponding-source.zip" .)
	(cd "$OUT" && sha256sum "$PKG_NAME-v$VERSION-corresponding-source.zip" >> SHA256SUMS.txt)
fi

log "Done"
echo "Release folder : $PKG"
echo "ZIP            : $OUT/$ZIP_NAME"
cat "$OUT/SHA256SUMS.txt"
