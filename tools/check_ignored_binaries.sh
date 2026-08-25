#!/usr/bin/env bash
# Fail if Steam/ISO binaries or Ghidra project DBs would be committed.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

fail=0

check_ignore() {
	local path="$1"
	if ! git check-ignore -q "${path}"; then
		echo "not gitignored: ${path}" >&2
		fail=1
	fi
}

check_ignore "depot_7661/cd.iso"
check_ignore "depot_7661/dummy.EXE"
check_ignore "data/cd.iso"
check_ignore "docs/original-game/.local/notes.md"

tracked="$(git ls-files -- 'depot_7661' 'data/cd.iso' '*.iso' '*.EXE' '*.exe' '*.rep' '*.gpr' || true)"
if [[ -n "${tracked}" ]]; then
	echo "tracked binary or Ghidra project:" >&2
	echo "${tracked}" >&2
	fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
	echo "check_ignored_binaries: fail" >&2
	exit 1
fi
echo "check_ignored_binaries: ok"
