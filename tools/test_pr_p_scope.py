#!/usr/bin/env python3
"""Prove that planning PR P contains only its closed, base-bound evidence scope."""

from __future__ import annotations

import argparse
import ast
import json
import re
import stat
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCOPE_RELATIVE_PATH = "docs/timing/pr-p-scope-v1.json"
SCOPE_PATH = ROOT / SCOPE_RELATIVE_PATH
PLAN_RELATIVE_PATH = "docs/plans/2026-08-26-001-render-simulation-decoupling-plan.md"
FULL_SHA_PATTERN = re.compile(r"[0-9a-f]{40}")

EXPECTED_ALLOWED_PATHS = [
	"docs/plans/2026-08-26-001-render-simulation-decoupling-plan.md",
	"docs/timing/pr-p-scope-v1.json",
	"docs/timing/source-disposition-map-v1.json",
	"docs/timing/source-disposition.md",
	"docs/timing/source-snapshots/source-snapshot-v1.schema.json",
	"docs/timing/source-snapshots/sources.json",
	"docs/timing/source-snapshots/v1/issue-1216.json",
	"docs/timing/source-snapshots/v1/issue-1336.json",
	"docs/timing/source-snapshots/v1/issue-997.json",
	"docs/timing/source-snapshots/v1/manifest.json",
	"docs/timing/source-snapshots/v1/pr-1166.json",
	"docs/timing/source-snapshots/v1/pr-1237.json",
	"docs/timing/source-snapshots/v1/pr-1270.json",
	"docs/timing/train-scope-v1.json",
	"tools/capture_timing_sources.py",
	"tools/fixtures/timing_source_capture/v1/expected-issue.json",
	"tools/fixtures/timing_source_capture/v1/expected-pr.json",
	"tools/fixtures/timing_source_capture/v1/http-transcript.json",
	"tools/test_capture_timing_sources.py",
	"tools/test_pr_p_scope.py",
	"tools/test_timing_source_disposition.py",
	"tools/test_timing_train_scope.py",
]

EXPECTED_IMPORTS_BY_PATH = {
	"tools/capture_timing_sources.py": [
		"from __future__ import annotations",
		"from dataclasses import dataclass",
		"from pathlib import Path",
		"from typing import Any",
		"from typing import Callable",
		"import hashlib",
		"import json",
		"import os",
		"import re",
		"import shutil",
		"import subprocess",
		"import sys",
		"import tempfile",
		"import urllib.error",
		"import urllib.parse",
		"import urllib.request",
	],
	"tools/test_capture_timing_sources.py": [
		"from __future__ import annotations",
		"from pathlib import Path",
		"import capture_timing_sources as capture",
		"import copy",
		"import json",
		"import shutil",
		"import tempfile",
		"import unittest",
	],
	"tools/test_pr_p_scope.py": [
		"from __future__ import annotations",
		"from pathlib import Path",
		"from pathlib import PurePosixPath",
		"from typing import Any",
		"import argparse",
		"import ast",
		"import json",
		"import re",
		"import stat",
		"import subprocess",
		"import sys",
	],
	"tools/test_timing_source_disposition.py": [
		"from __future__ import annotations",
		"from pathlib import Path",
		"from typing import Any",
		"import json",
		"import re",
		"import unittest",
	],
	"tools/test_timing_train_scope.py": [
		"from __future__ import annotations",
		"from pathlib import Path",
		"from typing import Any",
		"import hashlib",
		"import json",
		"import unittest",
	],
}

EXPECTED_CAPABILITY_GRAMMAR = {
	"capability_roots": [
		"os",
		"posix",
		"pty",
		"shutil",
		"socket",
		"subprocess",
		"sys",
		"tempfile",
		"urllib",
	],
	"closed_module_names_by_exact_path": {
		"tools/capture_timing_sources.py": {},
		"tools/test_capture_timing_sources.py": {
			"capture_timing_sources": [
				"capture_timing_sources.COMMITTED_ROOT",
				"capture_timing_sources.COMMITTED_ROOT.glob",
				"capture_timing_sources.DriftError",
				"capture_timing_sources.GitHubClient",
				"capture_timing_sources.IntegrityError",
				"capture_timing_sources.Response",
				"capture_timing_sources.SpecError",
				"capture_timing_sources.TransportError",
				"capture_timing_sources._ids",
				"capture_timing_sources.build_manifest",
				"capture_timing_sources.canonical_bytes",
				"capture_timing_sources.capture_corpus_stable",
				"capture_timing_sources.capture_once",
				"capture_timing_sources.capture_to",
				"capture_timing_sources.load_json",
				"capture_timing_sources.load_spec",
				"capture_timing_sources.main",
				"capture_timing_sources.normalize_file",
				"capture_timing_sources.parse_next_link",
				"capture_timing_sources.sha256_file",
				"capture_timing_sources.validate_snapshot",
				"capture_timing_sources.verify_offline",
			],
		},
		"tools/test_pr_p_scope.py": {},
		"tools/test_timing_source_disposition.py": {},
		"tools/test_timing_train_scope.py": {},
	},
	"dynamic_indirection_calls": [
		"__import__",
		"breakpoint",
		"compile",
		"delattr",
		"eval",
		"exec",
		"getattr",
		"globals",
		"locals",
		"setattr",
		"vars",
	],
	"dunder_attribute_access": "forbidden",
	"capability_module_or_callable_escape": "forbidden",
	"capability_subscript_access": "forbidden_except_exact_bases",
	"allowed_subscript_bases_by_exact_path": {
		"tools/capture_timing_sources.py": ["sys.argv"],
		"tools/test_capture_timing_sources.py": [],
		"tools/test_pr_p_scope.py": [],
		"tools/test_timing_source_disposition.py": [],
		"tools/test_timing_train_scope.py": [],
	},
	"allowed_calls_by_exact_path": {
		"tools/capture_timing_sources.py": [
			"os.chmod",
			"os.environ.get",
			"shutil.rmtree",
			"subprocess.run",
			"sys.exit",
			"tempfile.TemporaryDirectory",
			"urllib.parse.parse_qs",
			"urllib.parse.urlparse",
			"urllib.request.Request",
			"urllib.request.build_opener",
			"urllib.request.urlopen",
		],
		"tools/test_capture_timing_sources.py": [
			"shutil.copytree",
			"tempfile.TemporaryDirectory",
		],
		"tools/test_pr_p_scope.py": ["subprocess.run"],
		"tools/test_timing_source_disposition.py": [],
		"tools/test_timing_train_scope.py": [],
	},
	"allowed_references_by_exact_path": {
		"tools/capture_timing_sources.py": [
			"subprocess.CalledProcessError",
			"sys.argv",
			"sys.stderr",
			"urllib.error.URLError",
			"urllib.request.HTTPRedirectHandler",
		],
		"tools/test_capture_timing_sources.py": [],
		"tools/test_pr_p_scope.py": ["subprocess.PIPE", "sys.stderr"],
		"tools/test_timing_source_disposition.py": [],
		"tools/test_timing_train_scope.py": [],
	},
	"external_process_calls_by_exact_path": {
		"tools/capture_timing_sources.py": {
			"callable": "subprocess.run",
			"argv_grammar": "exact_gh_auth_token_v1",
			"keyword_literals_exact": {
				"capture_output": True,
				"check": True,
				"text": True,
			},
		},
		"tools/test_pr_p_scope.py": {
			"callable": "subprocess.run",
			"argv_grammar": "runtime_validated_read_only_git_v1",
			"enclosing_function": "git_result",
			"keyword_literals_exact": {
				"check": False,
				"cwd": "name:ROOT",
				"stderr": "attribute:subprocess.PIPE",
				"stdout": "attribute:subprocess.PIPE",
			},
		},
	},
	"all_other_capability_calls_and_references": "forbidden",
	"all_other_external_process_launches": "forbidden",
}


class ScopeViolation(RuntimeError):
	pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
	value: dict[str, Any] = {}
	for key, item in pairs:
		if key in value:
			raise ScopeViolation(f"duplicate JSON key in scope manifest: {key}")
		value[key] = item
	return value


def parse_json_bytes(data: bytes, source: str) -> dict[str, Any]:
	try:
		value = json.loads(data.decode("utf-8"), object_pairs_hook=reject_duplicate_keys)
	except (UnicodeDecodeError, json.JSONDecodeError) as exc:
		raise ScopeViolation(f"cannot parse {source}: {exc}") from exc
	if not isinstance(value, dict):
		raise ScopeViolation(f"{source} must contain a JSON object")
	return value


def validate_full_sha(value: str, label: str) -> None:
	if FULL_SHA_PATTERN.fullmatch(value) is None:
		raise ScopeViolation(f"{label} must be a lowercase full 40-character commit SHA")


def validate_repo_path(path: str) -> None:
	parts = path.split("/")
	if (
		not path
		or path.startswith("/")
		or "\\" in path
		or ":" in path
		or any(part in {"", ".", ".."} for part in parts)
		or any(ord(character) < 32 or ord(character) == 127 for character in path)
		or PurePosixPath(path).as_posix() != path
	):
		raise ScopeViolation(f"non-canonical repository path: {path!r}")


def validate_git_argv(args: tuple[str, ...]) -> tuple[str, ...]:
	if not args or not all(isinstance(item, str) for item in args):
		raise ScopeViolation("git argv must be a non-empty tuple of strings")
	command = args[0]
	valid = False
	if command == "cat-file" and len(args) == 3 and args[1] == "-t":
		validate_full_sha(args[2], "git cat-file object")
		valid = True
	elif command == "merge-base" and len(args) == 4 and args[1] == "--is-ancestor":
		validate_full_sha(args[2], "git merge-base base")
		validate_full_sha(args[3], "git merge-base head")
		valid = True
	elif command == "show" and len(args) == 2 and ":" in args[1]:
		revision, path = args[1].split(":", 1)
		validate_full_sha(revision, "git show revision")
		validate_repo_path(path)
		valid = True
	elif command == "diff":
		prefix = (
			"diff",
			"--no-ext-diff",
			"--no-textconv",
			"--name-status",
			"-z",
			"--no-renames",
		)
		if args[: len(prefix)] == prefix and len(args) in {8, 9} and args[-1] == "--":
			validate_full_sha(args[6], "git diff base")
			if len(args) == 9:
				validate_full_sha(args[7], "git diff head")
			valid = True
	elif args == ("ls-files", "-z", "--others", "--exclude-standard"):
		valid = True
	elif args == ("rev-parse", "--verify", "HEAD^{commit}"):
		valid = True
	elif command == "ls-tree" and len(args) == 5 and args[1] == "-z" and args[3] == "--":
		validate_full_sha(args[2], "git ls-tree revision")
		validate_repo_path(args[4])
		valid = True
	if not valid:
		raise ScopeViolation(f"git argv is outside the exact read-only grammar: {args!r}")
	return args


def git_result(*args: str) -> Any:
	return subprocess.run(
		[
			"git",
			"--no-pager",
			"--no-replace-objects",
			*validate_git_argv(args),
		],
		cwd=ROOT,
		check=False,
		stdout=subprocess.PIPE,
		stderr=subprocess.PIPE,
	)


def git_bytes(*args: str) -> bytes:
	result = git_result(*args)
	if result.returncode != 0:
		detail = result.stderr.decode("utf-8", errors="replace").strip()
		raise ScopeViolation(f"git {' '.join(args)} failed: {detail}")
	return result.stdout


def require_commit(value: str, label: str) -> None:
	validate_full_sha(value, label)
	object_type = git_bytes("cat-file", "-t", value).decode("ascii", errors="strict").strip()
	if object_type != "commit":
		raise ScopeViolation(f"{label} is not a commit: {value}")


def require_ancestor(base: str, head: str) -> None:
	result = git_result("merge-base", "--is-ancestor", base, head)
	if result.returncode != 0:
		detail = result.stderr.decode("utf-8", errors="replace").strip()
		raise ScopeViolation(
			f"scope base {base} is not an ancestor of head {head}: {detail}"
		)


def string_list(value: Any, label: str) -> list[str]:
	if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
		raise ScopeViolation(f"{label} must be a list of strings")
	if len(value) != len(set(value)):
		raise ScopeViolation(f"{label} contains duplicate entries")
	return value


def validate_policy(policy: Any) -> dict[str, Any]:
	if not isinstance(policy, dict):
		raise ScopeViolation("executable_python_policy must be an object")
	expected_keys = {
		"mode",
		"inspection_scope",
		"local_content_source",
		"ci_content_source",
		"max_python_source_bytes",
		"imports_by_exact_path",
		"capability_grammar",
	}
	if set(policy) != expected_keys:
		raise ScopeViolation("executable_python_policy has missing or unknown fields")
	for key, expected in (
		("mode", "closed_per_file_capability_grammar"),
		("inspection_scope", "all_changed_dot_py_files"),
		("local_content_source", "working_tree_including_untracked_files"),
		("ci_content_source", "exact_head_commit_tree"),
	):
		if policy[key] != expected:
			raise ScopeViolation(f"executable_python_policy.{key} is not closed")
	if policy["max_python_source_bytes"] != 1_048_576:
		raise ScopeViolation("max_python_source_bytes must remain exactly 1048576")
	if policy["imports_by_exact_path"] != EXPECTED_IMPORTS_BY_PATH:
		raise ScopeViolation("per-file import grammar is not the exact frozen grammar")
	if policy["capability_grammar"] != EXPECTED_CAPABILITY_GRAMMAR:
		raise ScopeViolation("per-file effect-call grammar is not the exact frozen grammar")
	return policy


def validate_scope_contract(scope: dict[str, Any]) -> dict[str, Any]:
	expected_keys = {
		"schema",
		"base_authority",
		"purpose",
		"allowed_exact_paths",
		"allowed_change_types",
		"required_blob_mode",
		"trust_boundary",
		"executable_python_policy",
		"runtime_implementation_forbidden",
	}
	if set(scope) != expected_keys:
		raise ScopeViolation("scope manifest has missing or unknown root fields")
	if scope["schema"] != "openapoc.pr_p_scope.v1":
		raise ScopeViolation("unknown planning PR scope schema")
	if scope["purpose"] != "planning_source_evidence_machine_contract_and_validation_only":
		raise ScopeViolation("planning PR purpose widened")
	if scope["base_authority"] != {
		"ci": "immutable_event_base_sha_from_landed_P0_workflow",
		"local": "explicit_cli_base_sha",
		"candidate_base_claim": "forbidden",
	}:
		raise ScopeViolation("candidate manifest attempted to claim base authority")
	allowed_paths = string_list(scope["allowed_exact_paths"], "allowed_exact_paths")
	for path in allowed_paths:
		validate_repo_path(path)
	if allowed_paths != EXPECTED_ALLOWED_PATHS:
		raise ScopeViolation("planning path allowlist is not the exact frozen artifact set")
	if scope["allowed_change_types"] != ["A", "M"]:
		raise ScopeViolation("planning PR may contain only added or modified paths")
	if scope["required_blob_mode"] != "100644":
		raise ScopeViolation("every planning artifact must be a regular 100644 blob")
	if scope["trust_boundary"] != {
		"authoritative_policy": "docs/timing/pr-p-policy-v1.json@event_base",
		"authoritative_validator": "tools/validate_pr_p_scope.py@event_base",
		"authoritative_workflow": ".github/workflows/planning-scope.yml@default_develop",
		"candidate_manifest_role": "required_untrusted_claim_only",
		"candidate_test_role": "defense_in_depth_only",
		"candidate_may_modify_trust_root": False,
		"exact_candidate_path_count": 22,
	}:
		raise ScopeViolation("P0/P trust boundary is not the exact closed contract")
	if scope["runtime_implementation_forbidden"] != [
		"engine_or_game_runtime_change",
		"subprocess_or_socket_game_launcher",
		"runtime_closure_collector_or_launcher",
		"candidate_admission_or_landing_implementation",
	]:
		raise ScopeViolation("runtime implementation prohibition is not the exact closed set")
	validate_policy(scope["executable_python_policy"])
	return scope


def read_head_blob(head: str, path: str) -> bytes:
	validate_repo_path(path)
	return git_bytes("show", f"{head}:{path}")


def load_scope(head: str | None) -> dict[str, Any]:
	if head is None:
		try:
			data = SCOPE_PATH.read_bytes()
		except OSError as exc:
			raise ScopeViolation(f"cannot read local scope manifest: {exc}") from exc
	else:
		data = read_head_blob(head, SCOPE_RELATIVE_PATH)
	return validate_scope_contract(parse_json_bytes(data, SCOPE_RELATIVE_PATH))


def parse_name_status(raw: bytes) -> dict[str, str]:
	try:
		parts = raw.decode("utf-8").split("\0")
	except UnicodeDecodeError as exc:
		raise ScopeViolation("changed paths must be UTF-8") from exc
	if parts and parts[-1] == "":
		parts.pop()
	if len(parts) % 2:
		raise ScopeViolation("git emitted an invalid name-status stream")
	entries: dict[str, str] = {}
	for index in range(0, len(parts), 2):
		status, path = parts[index : index + 2]
		validate_repo_path(path)
		if path in entries:
			raise ScopeViolation(f"changed path appeared more than once: {path}")
		entries[path] = status
	return entries


def changed_paths(base: str, head: str | None) -> dict[str, str]:
	arguments = [
		"diff",
		"--no-ext-diff",
		"--no-textconv",
		"--name-status",
		"-z",
		"--no-renames",
		base,
	]
	if head is not None:
		arguments.append(head)
	arguments.append("--")
	entries = parse_name_status(git_bytes(*arguments))
	if head is None:
		try:
			untracked = git_bytes(
				"ls-files", "-z", "--others", "--exclude-standard"
			).decode("utf-8")
		except UnicodeDecodeError as exc:
			raise ScopeViolation("untracked paths must be UTF-8") from exc
		for path in (item for item in untracked.split("\0") if item):
			validate_repo_path(path)
			if path in entries:
				raise ScopeViolation(f"path is both tracked-diff and untracked: {path}")
			entries[path] = "A"
	return entries


def validate_changed_paths(scope: dict[str, Any], entries: dict[str, str], ci_mode: bool) -> None:
	if not entries:
		raise ScopeViolation("planning PR scope diff is empty")
	bad_status = {
		path: status
		for path, status in entries.items()
		if status not in set(scope["allowed_change_types"])
	}
	if bad_status:
		raise ScopeViolation(f"planning PR contains forbidden change types: {bad_status}")
	expected = set(scope["allowed_exact_paths"])
	actual = set(entries)
	if actual != expected:
		unexpected = sorted(actual - expected)
		missing = sorted(expected - actual)
		raise ScopeViolation(
			f"planning PR changed-path set is not exact; unexpected={unexpected}; missing={missing}"
		)
	if ci_mode and entries.get(SCOPE_RELATIVE_PATH) != "A":
		raise ScopeViolation("CI scope validation is legal only on the PR that adds the scope manifest")


def parse_tree_entry(raw: bytes, path: str, required_mode: str) -> None:
	try:
		parts = raw.decode("utf-8").split("\0")
	except UnicodeDecodeError as exc:
		raise ScopeViolation(f"non-UTF-8 tree entry for {path}") from exc
	if parts and parts[-1] == "":
		parts.pop()
	if len(parts) != 1 or "\t" not in parts[0]:
		raise ScopeViolation(f"head tree does not contain exactly one entry for changed path: {path}")
	metadata, actual_path = parts[0].split("\t", 1)
	metadata_parts = metadata.split(" ")
	if len(metadata_parts) != 3:
		raise ScopeViolation(f"malformed tree metadata for changed path: {path}")
	mode, object_type, object_id = metadata_parts
	if (
		actual_path != path
		or object_type != "blob"
		or mode != required_mode
		or FULL_SHA_PATTERN.fullmatch(object_id) is None
	):
		raise ScopeViolation(
			f"changed path must be one regular non-executable {required_mode} blob: {path}"
		)


def validate_regular_artifact(path: str, head: str | None, required_mode: str) -> None:
	if head is not None:
		parse_tree_entry(git_bytes("ls-tree", "-z", head, "--", path), path, required_mode)
		return
	ancestor = ROOT
	for component in path.split("/")[:-1]:
		ancestor /= component
		try:
			ancestor_metadata = ancestor.lstat()
		except OSError as exc:
			raise ScopeViolation(f"cannot stat parent of changed local path {path}: {exc}") from exc
		if not stat.S_ISDIR(ancestor_metadata.st_mode):
			raise ScopeViolation(f"changed local path traverses a non-directory or symlink: {path}")
	local_path = ROOT / path
	try:
		metadata = local_path.lstat()
	except OSError as exc:
		raise ScopeViolation(f"cannot stat changed local path {path}: {exc}") from exc
	if not stat.S_ISREG(metadata.st_mode) or stat.S_IMODE(metadata.st_mode) != 0o644:
		raise ScopeViolation(f"changed local path must be a regular non-symlink 0644 file: {path}")


def artifact_bytes(path: str, head: str | None) -> bytes:
	if head is not None:
		return read_head_blob(head, path)
	try:
		return (ROOT / path).read_bytes()
	except OSError as exc:
		raise ScopeViolation(f"cannot read changed local artifact {path}: {exc}") from exc


def import_record(node: ast.Import | ast.ImportFrom, item: ast.alias) -> str:
	alias = f" as {item.asname}" if item.asname else ""
	if isinstance(node, ast.Import):
		return f"import {item.name}{alias}"
	if node.level != 0 or node.module is None or item.name == "*":
		raise ScopeViolation("relative and star imports are forbidden")
	return f"from {node.module} import {item.name}{alias}"


def collect_import_records(
	path: str, tree: ast.AST, parents: dict[ast.AST, ast.AST]
) -> list[str]:
	records: list[str] = []
	for node in ast.walk(tree):
		if not isinstance(node, (ast.Import, ast.ImportFrom)):
			continue
		if not isinstance(parents.get(node), ast.Module):
			raise ScopeViolation(f"{path}:{node.lineno}: imports must be unconditional module statements")
		for item in node.names:
			records.append(import_record(node, item))
	if len(records) != len(set(records)):
		raise ScopeViolation(f"{path}: duplicate import records are forbidden")
	return sorted(records)


def collect_import_aliases(tree: ast.AST) -> dict[str, str]:
	aliases: dict[str, str] = {}
	for node in ast.iter_child_nodes(tree):
		if isinstance(node, ast.Import):
			for item in node.names:
				bound = item.asname or item.name.split(".", 1)[0]
				aliases[bound] = item.name if item.asname else item.name.split(".", 1)[0]
		elif isinstance(node, ast.ImportFrom):
			module = node.module or ""
			for item in node.names:
				bound = item.asname or item.name
				aliases[bound] = ".".join(part for part in (module, item.name) if part)
	return aliases


def resolve_name(node: ast.AST, aliases: dict[str, str]) -> str | None:
	if isinstance(node, ast.Name):
		return aliases.get(node.id, node.id)
	if isinstance(node, ast.Attribute):
		base = resolve_name(node.value, aliases)
		return f"{base}.{node.attr}" if base is not None else None
	return None


def literal_signature(node: ast.AST, aliases: dict[str, str]) -> Any:
	if isinstance(node, ast.Constant) and isinstance(node.value, (bool, str, int, type(None))):
		return node.value
	if isinstance(node, ast.Name):
		return f"name:{node.id}"
	name = resolve_name(node, aliases)
	if name is not None:
		return f"attribute:{name}"
	return "<nonliteral>"


class ExecutablePythonVisitor(ast.NodeVisitor):
	def __init__(
		self,
		path: str,
		policy: dict[str, Any],
		aliases: dict[str, str],
		parents: dict[ast.AST, ast.AST],
	):
		self.path = path
		self.grammar = policy["capability_grammar"]
		self.aliases = aliases
		self.parents = parents
		self.errors: list[str] = []
		self.function_stack: list[str] = []
		self.roots = set(self.grammar["capability_roots"])
		self.allowed_calls = set(self.grammar["allowed_calls_by_exact_path"][path])
		self.allowed_references = set(
			self.grammar["allowed_references_by_exact_path"][path]
		)
		self.allowed_subscripts = set(
			self.grammar["allowed_subscript_bases_by_exact_path"][path]
		)
		self.closed_module_names = {
			module: set(names)
			for module, names in self.grammar["closed_module_names_by_exact_path"][path].items()
		}

	def error(self, node: ast.AST, message: str) -> None:
		line = node.lineno if hasattr(node, "lineno") else 0
		self.errors.append(f"{self.path}:{line}: {message}")

	def visit_FunctionDef(self, node: ast.FunctionDef) -> None:
		self.function_stack.append(node.name)
		self.generic_visit(node)
		self.function_stack.pop()

	def visit_AsyncFunctionDef(self, node: ast.AsyncFunctionDef) -> None:
		self.function_stack.append(node.name)
		self.generic_visit(node)
		self.function_stack.pop()

	def visit_Call(self, node: ast.Call) -> None:
		name = resolve_name(node.func, self.aliases)
		self.check_closed_module_name(node, name)
		if name in set(self.grammar["dynamic_indirection_calls"]):
			self.error(node, f"dynamic indirection call is forbidden: {name}")
		if name is not None and name.split(".", 1)[0] in self.roots:
			if name not in self.allowed_calls:
				self.error(node, f"capability call is not in this file's closed grammar: {name}")
			if name == "subprocess.run":
				self.check_process_call(node)
		self.generic_visit(node)

	def check_process_call(self, node: ast.Call) -> None:
		spec = self.grammar["external_process_calls_by_exact_path"].get(self.path)
		if spec is None:
			self.error(node, "external process launch is forbidden in this file")
			return
		if len(node.args) != 1 or any(keyword.arg is None for keyword in node.keywords):
			self.error(node, "external process call has a non-exact positional/keyword shape")
			return
		actual_keywords = {
			keyword.arg: literal_signature(keyword.value, self.aliases)
			for keyword in node.keywords
			if keyword.arg is not None
		}
		if actual_keywords != spec["keyword_literals_exact"]:
			self.error(node, "external process keywords differ from the exact closed grammar")
		argument = node.args[0]
		if spec["argv_grammar"] == "exact_gh_auth_token_v1":
			if not isinstance(argument, (ast.List, ast.Tuple)) or [
				item.value
				for item in argument.elts
				if isinstance(item, ast.Constant) and isinstance(item.value, str)
			] != ["gh", "auth", "token"] or len(argument.elts) != 3 or any(
				not isinstance(item, ast.Constant) or not isinstance(item.value, str)
				for item in argument.elts
			):
				self.error(node, "capture may execute only exact argv ['gh', 'auth', 'token']")
		elif spec["argv_grammar"] == "runtime_validated_read_only_git_v1":
			if not self.function_stack or self.function_stack[-1] != spec["enclosing_function"]:
				self.error(node, "git subprocess call must be enclosed by git_result")
			expected_literals = ["git", "--no-pager", "--no-replace-objects"]
			valid_shape = isinstance(argument, ast.List) and len(argument.elts) == 4
			if valid_shape:
				for index, value in enumerate(expected_literals):
					item = argument.elts[index]
					valid_shape = valid_shape and isinstance(item, ast.Constant) and item.value == value
				starred = argument.elts[3]
				valid_shape = valid_shape and isinstance(starred, ast.Starred)
				if isinstance(starred, ast.Starred):
					call = starred.value
					valid_shape = (
						valid_shape
						and isinstance(call, ast.Call)
						and isinstance(call.func, ast.Name)
						and call.func.id == "validate_git_argv"
						and len(call.args) == 1
						and isinstance(call.args[0], ast.Name)
						and call.args[0].id == "args"
						and not call.keywords
					)
			if not valid_shape:
				self.error(node, "git argv must inline the runtime read-only validator")
		else:
			self.error(node, "unknown external process argv grammar")

	def top_attribute(self, node: ast.Attribute) -> ast.Attribute:
		current = node
		while True:
			parent = self.parents.get(current)
			if not isinstance(parent, ast.Attribute) or parent.value is not current:
				return current
			current = parent

	def check_closed_module_name(self, node: ast.AST, name: str | None) -> None:
		if name is None:
			return
		root = name.split(".", 1)[0]
		if root in self.closed_module_names and name not in self.closed_module_names[root]:
			self.error(node, f"closed local module name is not in this file's exact export grammar: {name}")

	def visit_Attribute(self, node: ast.Attribute) -> None:
		if node.attr.startswith("__") and node.attr.endswith("__"):
			self.error(node, f"dunder attribute access is forbidden: {node.attr}")
		name = resolve_name(node, self.aliases)
		top = self.top_attribute(node)
		top_name = resolve_name(top, self.aliases)
		self.check_closed_module_name(node, top_name)
		if name is not None and name.split(".", 1)[0] in self.roots:
			parent = self.parents.get(top)
			is_call_target = isinstance(parent, ast.Call) and parent.func is top
			if not is_call_target and top_name not in self.allowed_references:
				self.error(node, f"capability reference is not in this file's closed grammar: {top_name}")
		self.generic_visit(node)

	def visit_Name(self, node: ast.Name) -> None:
		name = resolve_name(node, self.aliases)
		if name in self.closed_module_names:
			parent = self.parents.get(node)
			if not isinstance(parent, ast.Attribute) or parent.value is not node:
				self.error(node, f"bare closed local module reference is forbidden: {name}")
		if name in self.roots:
			parent = self.parents.get(node)
			if not isinstance(parent, ast.Attribute) or parent.value is not node:
				self.error(node, f"bare capability module reference is forbidden: {name}")

	def visit_Subscript(self, node: ast.Subscript) -> None:
		name = resolve_name(node.value, self.aliases)
		self.check_closed_module_name(node, name)
		if (
			name is not None
			and name.split(".", 1)[0] in self.roots
			and name not in self.allowed_subscripts
		):
			self.error(node, f"capability subscript access is forbidden: {name}")
		self.generic_visit(node)

	def visit_Assign(self, node: ast.Assign) -> None:
		self.check_capability_escape(node, node.value)
		self.generic_visit(node)

	def visit_AnnAssign(self, node: ast.AnnAssign) -> None:
		if node.value is not None:
			self.check_capability_escape(node, node.value)
		self.generic_visit(node)

	def visit_NamedExpr(self, node: ast.NamedExpr) -> None:
		self.check_capability_escape(node, node.value)
		self.generic_visit(node)

	def check_capability_escape(self, node: ast.AST, value: ast.AST) -> None:
		name = resolve_name(value, self.aliases)
		if name is not None and name.split(".", 1)[0] in self.closed_module_names:
			self.error(node, f"closed local module value escape is forbidden: {name}")
		if name is not None and name.split(".", 1)[0] in self.roots:
			self.error(node, f"capability module/callable escape is forbidden: {name}")


def validate_python_source(path: str, data: bytes, policy: dict[str, Any]) -> None:
	if path not in policy["imports_by_exact_path"]:
		raise ScopeViolation(f"changed Python path has no closed grammar: {path}")
	if len(data) > policy["max_python_source_bytes"]:
		raise ScopeViolation(f"changed Python source exceeds size cap: {path}")
	try:
		source = data.decode("utf-8")
		tree = ast.parse(source, filename=path)
	except (UnicodeDecodeError, SyntaxError) as exc:
		raise ScopeViolation(f"changed Python source is not valid UTF-8 Python: {path}: {exc}") from exc
	parents = {
		child: parent
		for parent in ast.walk(tree)
		for child in ast.iter_child_nodes(parent)
	}
	imports = collect_import_records(path, tree, parents)
	if imports != policy["imports_by_exact_path"][path]:
		raise ScopeViolation(
			f"{path}: imports differ from the exact per-file grammar; actual={imports}"
		)
	aliases = collect_import_aliases(tree)
	visitor = ExecutablePythonVisitor(path, policy, aliases, parents)
	visitor.visit(tree)
	if visitor.errors:
		raise ScopeViolation("\n".join(visitor.errors))


def sentinel_source(policy: dict[str, Any], path: str, body: str) -> bytes:
	imports = policy["imports_by_exact_path"][path]
	future = [line for line in imports if line.startswith("from __future__ import ")]
	others = [line for line in imports if line not in future]
	return ("\n".join([*future, *others, "", body, ""])).encode("utf-8")


def expect_source_rejected(
	policy: dict[str, Any], label: str, path: str, body: str, expected_fragment: str
) -> None:
	try:
		validate_python_source(path, sentinel_source(policy, path, body), policy)
	except ScopeViolation as exc:
		if expected_fragment not in str(exc):
			raise ScopeViolation(
				f"policy sentinel {label!r} failed for the wrong reason: {exc}"
			) from exc
	else:
		raise ScopeViolation(f"policy sentinel {label!r} was unexpectedly accepted")


def validate_python_policy_sentinels(policy: dict[str, Any]) -> None:
	validate_python_source(
		"tools/capture_timing_sources.py",
		sentinel_source(
			policy,
			"tools/capture_timing_sources.py",
			"subprocess.run(['gh', 'auth', 'token'], check=True, capture_output=True, text=True)",
		),
		policy,
	)
	validate_python_source(
		"tools/test_pr_p_scope.py",
		sentinel_source(
			policy,
			"tools/test_pr_p_scope.py",
			"def git_result(*args):\n\treturn subprocess.run(['git', '--no-pager', '--no-replace-objects', *validate_git_argv(args)], cwd=ROOT, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)",
		),
		policy,
	)
	for label, path, body, expected in (
		(
			"gh command widening",
			"tools/capture_timing_sources.py",
			"subprocess.run(['gh', 'api', '/user'], check=True, capture_output=True, text=True)",
			"only exact argv",
		),
		(
			"game subprocess",
			"tools/capture_timing_sources.py",
			"subprocess.run(['OpenApoc'], check=True, capture_output=True, text=True)",
			"only exact argv",
		),
		(
			"posix system",
			"tools/capture_timing_sources.py",
			"posix.system('OpenApoc')",
			"capability call is not",
		),
		(
			"subprocess dunder dictionary",
			"tools/capture_timing_sources.py",
			"subprocess.__dict__['run'](['OpenApoc'])",
			"dunder attribute access",
		),
		(
			"dunder subclass indirection",
			"tools/capture_timing_sources.py",
			"object.__subclasses__()",
			"dunder attribute access",
		),
		(
			"dynamic getattr",
			"tools/capture_timing_sources.py",
			"getattr(subprocess, 'run')(['OpenApoc'])",
			"dynamic indirection call",
		),
		(
			"sys modules registry",
			"tools/capture_timing_sources.py",
			"sys.modules['posix'].system('OpenApoc')",
			"capability subscript access",
		),
		(
			"callable escape",
			"tools/capture_timing_sources.py",
			"runner = subprocess.run",
			"capability module/callable escape",
		),
		(
			"local module subprocess re-export",
			"tools/test_capture_timing_sources.py",
			"capture.subprocess.run(['OpenApoc'])",
			"closed local module name is not",
		),
		(
			"local module os re-export",
			"tools/test_capture_timing_sources.py",
			"capture.os.system('OpenApoc')",
			"closed local module name is not",
		),
		(
			"git literal bypass",
			"tools/test_pr_p_scope.py",
			"def git_result(*args):\n\treturn subprocess.run(['git', '-c', 'alias.status=!OpenApoc', 'status'], cwd=ROOT, check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)",
			"inline the runtime read-only validator",
		),
		(
			"extra socket import",
			"tools/test_timing_train_scope.py",
			"import socket",
			"imports differ",
		),
		(
			"runtime harness import",
			"tools/test_timing_train_scope.py",
			"from tools import oa_play",
			"imports differ",
		),
	):
		expect_source_rejected(policy, label, path, body, expected)


def validate_git_grammar_sentinels() -> None:
	sha = "1" * 40
	for allowed in (
		("cat-file", "-t", sha),
		("merge-base", "--is-ancestor", sha, sha),
		("show", f"{sha}:{SCOPE_RELATIVE_PATH}"),
		(
			"diff",
			"--no-ext-diff",
			"--no-textconv",
			"--name-status",
			"-z",
			"--no-renames",
			sha,
			sha,
			"--",
		),
		("ls-files", "-z", "--others", "--exclude-standard"),
		("rev-parse", "--verify", "HEAD^{commit}"),
		("ls-tree", "-z", sha, "--", SCOPE_RELATIVE_PATH),
	):
		validate_git_argv(allowed)
	for denied in (
		("-c", "alias.status=!OpenApoc", "status"),
		("status",),
		("diff", "--name-status", sha, "--"),
		("show", f"HEAD:{SCOPE_RELATIVE_PATH}"),
		("ls-tree", "-z", sha, "--", "../escape"),
	):
		try:
			validate_git_argv(denied)
		except ScopeViolation:
			continue
		raise ScopeViolation(f"git grammar sentinel unexpectedly accepted: {denied!r}")


def validate_blob_mode_sentinels() -> None:
	path = "tools/test_pr_p_scope.py"
	object_id = "2" * 40
	parse_tree_entry(f"100644 blob {object_id}\t{path}\0".encode(), path, "100644")
	for mode in ("100755", "120000", "160000"):
		object_type = "commit" if mode == "160000" else "blob"
		raw = f"{mode} {object_type} {object_id}\t{path}\0".encode()
		try:
			parse_tree_entry(raw, path, "100644")
		except ScopeViolation:
			continue
		raise ScopeViolation(f"blob mode sentinel unexpectedly accepted: {mode}")


def validate_changed_path_sentinels(scope: dict[str, Any]) -> None:
	entries = {path: "M" for path in scope["allowed_exact_paths"]}
	entries[SCOPE_RELATIVE_PATH] = "A"
	validate_changed_paths(scope, entries, True)
	for label, mutated in (
		(
			"former broad snapshot prefix",
			{**entries, "docs/timing/source-snapshots/v1/unlisted.json": "A"},
		),
		(
			"former broad fixture prefix",
			{**entries, "tools/fixtures/unlisted.json": "A"},
		),
		(
			"missing exact artifact",
			{path: status for path, status in entries.items() if path != PLAN_RELATIVE_PATH},
		),
		(
			"forbidden change status",
			{**entries, PLAN_RELATIVE_PATH: "D"},
		),
	):
		try:
			validate_changed_paths(scope, mutated, True)
		except ScopeViolation:
			continue
		raise ScopeViolation(f"changed-path sentinel unexpectedly accepted: {label}")


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument(
		"--base",
		required=True,
		help="exact trusted base commit SHA supplied by the caller, never by the candidate manifest",
	)
	parser.add_argument("--head", help="exact pull-request head commit SHA")
	return parser.parse_args()


def run() -> None:
	args = parse_args()
	base = args.base
	require_commit(base, "--base")
	ci_mode = args.head is not None
	if ci_mode:
		head = args.head
		assert head is not None
		require_commit(head, "--head")
		require_ancestor(base, head)
		scope = load_scope(head)
	else:
		head = None
		scope = load_scope(None)
		local_head = git_bytes("rev-parse", "--verify", "HEAD^{commit}").decode(
			"ascii", errors="strict"
		).strip()
		require_commit(local_head, "local HEAD")
		require_ancestor(base, local_head)
	entries = changed_paths(base, head)
	validate_changed_paths(scope, entries, ci_mode)
	for path in sorted(entries):
		validate_regular_artifact(path, head, scope["required_blob_mode"])
	policy = scope["executable_python_policy"]
	validate_changed_path_sentinels(scope)
	validate_git_grammar_sentinels()
	validate_blob_mode_sentinels()
	validate_python_policy_sentinels(policy)
	for path in sorted(item for item in entries if item.endswith(".py")):
		validate_python_source(path, artifact_bytes(path, head), policy)
	mode = f"{base}..{head}" if head is not None else f"{base}..working-tree"
	print(f"planning PR scope verified: {mode}; {len(entries)} changed paths")


def main() -> int:
	try:
		run()
	except ScopeViolation as exc:
		print(f"planning PR scope violation: {exc}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
