#!/usr/bin/env bash
# gitw hook: run on `gitw up` / `gitw open` / `gitw agent`, after the data plane.
#
# All the real work lives in tools/setup-worktree.sh so that worktrees created
# OUTSIDE gitw -- notably the isolated worktrees coding agents get -- can run the
# exact same provisioning with one command.
#
# Available env (see the gitw generic provisioner): GITW_REPO GITW_BRANCH
# WORKTREE_DIR MAIN_DIR GITW_PORT_BASE GITW_REDIS_INDEX GITW_DB_NAME GITW_DB_URL
set -euo pipefail

cd "${WORKTREE_DIR:-.}"

# Provision against whatever branch this worktree is actually on, not a hardcoded
# one -- gitw already checked out the right thing before calling us.
BRANCH="${GITW_BRANCH:-$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo HEAD)}"

exec ./tools/setup-worktree.sh "${BRANCH}"
