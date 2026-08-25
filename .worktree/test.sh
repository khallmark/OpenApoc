#!/usr/bin/env bash
# gitw hook: `gitw test`.
#
# Assumes provisioning has run (gitw up / tools/setup-worktree.sh). Builds, then
# runs the full ctest suite. Deliberately does NOT run extract-data: that target
# is the slowest in the tree and is only needed when tools/extractors/ changes.
set -euo pipefail

cd "${WORKTREE_DIR:-.}"

if [ ! -f build/CMakeCache.txt ]; then
	echo "build/ is not configured - run ./tools/setup-worktree.sh first" >&2
	exit 1
fi

cmake --build build -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
ctest --test-dir build --output-on-failure
