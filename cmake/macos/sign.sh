#!/usr/bin/env bash
# Sign an OpenApoc.app inside-out. Entitlements apply only to the main executable.
# Usage: sign.sh <OpenApoc.app> [codesign-identity] [entitlements.plist]
set -euo pipefail

APP="${1:?path to OpenApoc.app}"
IDENTITY="${2:-${APPLE_CODESIGN_IDENTITY:-Developer ID Application}}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENTITLEMENTS="${3:-${SCRIPT_DIR}/OpenApoc.entitlements}"

if [[ ! -d "${APP}" ]]; then
	echo "not an app bundle: ${APP}" >&2
	exit 1
fi

if [[ ! -f "${ENTITLEMENTS}" ]]; then
	echo "missing entitlements: ${ENTITLEMENTS}" >&2
	exit 1
fi

MAIN="${APP}/Contents/MacOS/OpenApoc"
if [[ ! -x "${MAIN}" ]]; then
	echo "missing main executable: ${MAIN}" >&2
	exit 1
fi

# Nested Mach-Os first (Frameworks, PlugIns, dylibs). Never use --deep.
while IFS= read -r -d '' macho; do
	if [[ "${macho}" == "${MAIN}" ]]; then
		continue
	fi
	echo "signing ${macho}"
	codesign --force --sign "${IDENTITY}" --timestamp --options runtime "${macho}"
done < <(find "${APP}" -type f \( -name '*.dylib' -o -name '*.so' -o -name '*.framework' \) -print0 2>/dev/null)
if [[ -d "${APP}/Contents/Frameworks" ]]; then
	while IFS= read -r -d '' nested; do
		echo "signing ${nested}"
		codesign --force --sign "${IDENTITY}" --timestamp --options runtime "${nested}"
	done < <(find "${APP}/Contents/Frameworks" -type f -perm +111 -print0 2>/dev/null)
fi

echo "signing ${MAIN}"
codesign --force --sign "${IDENTITY}" --timestamp --options runtime \
	--entitlements "${ENTITLEMENTS}" "${MAIN}"

echo "signing ${APP}"
codesign --force --sign "${IDENTITY}" --timestamp --options runtime "${APP}"

codesign --verify --strict --verbose=2 "${APP}"
echo "codesign ok: ${APP}"
