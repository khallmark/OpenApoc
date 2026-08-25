#!/usr/bin/env bash
# Provision an agent/dev worktree so it can build and test immediately.
#
# Agent worktrees are cut from whatever commit the session started on, with no
# submodules, no data/cd.iso, and no configured build tree. Every agent that
# skips this pays the same cold-start tax. Run this first; it is idempotent and
# safe to re-run.
#
#   ./tools/setup-worktree.sh [branch]
#
# Default branch is the current upstream development branch.
set -euo pipefail

MAIN="/Users/khallmark/Desktop/Code/OpenSource/OpenApoc"
BRANCH="${1:-khallmark/parity-implementation}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"

say() { printf '\033[1m==> %s\033[0m\n' "$*"; }

# 1. Fast-forward onto the target branch when we are behind it. Never rewrites
#    local work: a non-fast-forward is reported and left alone.
say "syncing onto ${BRANCH}"
if git rev-parse --verify --quiet "${BRANCH}" >/dev/null; then
	if git merge-base --is-ancestor HEAD "${BRANCH}" 2>/dev/null; then
		if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
			echo "    local modifications present - skipping fast-forward, rebase yourself if needed"
		else
			git merge --ff-only "${BRANCH}" >/dev/null && echo "    fast-forwarded to $(git log --oneline -1)"
		fi
	else
		echo "    HEAD is not an ancestor of ${BRANCH} - leaving history alone"
	fi
else
	echo "    branch ${BRANCH} not found - skipping"
fi

# 2. Submodules. Reference the main checkout's object store so this is a local
#    hardlink-ish copy rather than six network clones.
say "submodules"
if [ -d "${MAIN}/.git" ] || [ -f "${MAIN}/.git" ]; then
	git submodule update --init --recursive --reference "${MAIN}" 2>/dev/null \
		|| git submodule update --init --recursive
else
	git submodule update --init --recursive
fi
echo "    $(git submodule status --recursive | wc -l | tr -d ' ') submodules ready"

# 3. Game data. The ISO is gitignored and lives beside the main checkout.
say "game data"
mkdir -p data
if [ ! -e data/cd.iso ]; then
	if [ -f "${MAIN}/depot_7661/cd.iso" ]; then
		ln -sfn "${MAIN}/depot_7661/cd.iso" data/cd.iso
		echo "    linked data/cd.iso"
	else
		echo "    WARNING: ${MAIN}/depot_7661/cd.iso not found - gamestate tests will not run"
	fi
else
	echo "    data/cd.iso already present"
fi

# 4. Configure. Tests ON - every parity change needs a lock test.
say "configuring build/"
if [ ! -f build/CMakeCache.txt ]; then
	cmake -S . -B build \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix boost);/opt/homebrew" \
		-DENABLE_TESTS=ON \
		-DBUILD_IMAGEDUMP=OFF \
		-DBUILD_SERIALIZATIONTOOL=OFF >/dev/null
	echo "    configured"
else
	echo "    build/ already configured"
fi

# 5. ccache is wired into CMakeLists.txt (cmake/ccache.cmake) and is what makes
#    a second worktree's build cheap. Warn loudly if it is missing.
say "ccache"
if command -v ccache >/dev/null 2>&1; then
	echo "    $(ccache --version | head -1), $(ccache -s 2>/dev/null | grep -iE 'cache size|Cacheable' | head -1 | tr -s ' ')"
else
	echo "    NOT INSTALLED - every worktree will do a full cold build."
	echo "    Install it: brew install ccache"
fi

say "ready"
echo "    build:  cmake --build build -j\$(sysctl -n hw.ncpu)"
echo "    test:   ctest --test-dir build --output-on-failure"
echo "    NOTE: only re-run 'cmake --build build --target extract-data' if you changed tools/extractors/."
