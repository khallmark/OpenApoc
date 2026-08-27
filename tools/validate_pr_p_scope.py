#!/usr/bin/env python3
"""Validate planning PR P from a landed, immutable P0 trust anchor.

This program is materialized from the protected P0 tag by the
``pull_request_target`` workflow. It never imports, checks out, or executes a
candidate artifact. Candidate Git objects are read only as bounded data.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any


FULL_SHA = re.compile(r"[0-9a-f]{40}")
PR_NUMBER = re.compile(r"[1-9][0-9]{0,9}")
TAG_REF = re.compile(r"refs/tags/[A-Za-z0-9][A-Za-z0-9._/-]{0,127}")
OPERATION_ID = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
POLICY_SCHEMA = "openapoc.pr_p_policy.v1"


class ValidationError(RuntimeError):
	pass


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
	result: dict[str, Any] = {}
	for key, value in pairs:
		if key in result:
			raise ValidationError(f"duplicate JSON key: {key}")
		result[key] = value
	return result


def parse_json(data: bytes, label: str) -> dict[str, Any]:
	try:
		value = json.loads(data.decode("utf-8"), object_pairs_hook=reject_duplicate_keys)
	except (UnicodeDecodeError, json.JSONDecodeError) as exc:
		raise ValidationError(f"invalid UTF-8 JSON in {label}: {exc}") from exc
	if not isinstance(value, dict):
		raise ValidationError(f"{label} must contain one JSON object")
	return value


def require_exact_keys(value: dict[str, Any], expected: set[str], label: str) -> None:
	if set(value) != expected:
		raise ValidationError(
			f"{label} has missing or unknown keys: expected={sorted(expected)} actual={sorted(value)}"
		)


def require_full_sha(value: str, label: str) -> str:
	if not isinstance(value, str) or FULL_SHA.fullmatch(value) is None:
		raise ValidationError(f"{label} must be one lowercase 40-character SHA")
	return value


def require_canonical_path(value: str, label: str) -> str:
	if not isinstance(value, str):
		raise ValidationError(f"{label} must be a string")
	parts = value.split("/")
	if (
		not value
		or value.startswith("/")
		or "\\" in value
		or ":" in value
		or any(part in {"", ".", ".."} for part in parts)
		or any(ord(character) < 32 or ord(character) == 127 for character in value)
		or PurePosixPath(value).as_posix() != value
	):
		raise ValidationError(f"{label} is not a canonical repository path: {value!r}")
	return value


def require_string_list(value: Any, label: str) -> list[str]:
	if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
		raise ValidationError(f"{label} must be a list of strings")
	if len(value) != len(set(value)):
		raise ValidationError(f"{label} contains duplicates")
	return value


def load_policy(path: Path) -> dict[str, Any]:
	try:
		policy_bytes = path.read_bytes()
	except OSError as exc:
		raise ValidationError(f"cannot read trusted policy: {exc}") from exc
	policy = parse_json(policy_bytes, str(path))
	require_exact_keys(
		policy,
		{
			"schema",
			"policy_id",
			"repository",
			"trust_anchor",
			"planning_candidate",
			"artifact_contract",
			"non_candidate_behavior",
		},
		"policy",
	)
	if policy["schema"] != POLICY_SCHEMA or policy["policy_id"] != "render-simulation-planning-p-v1":
		raise ValidationError("unknown trusted planning policy")

	repository = policy["repository"]
	if not isinstance(repository, dict):
		raise ValidationError("repository policy must be an object")
	require_exact_keys(repository, {"full_name", "node_id", "default_branch", "https_url"}, "repository")
	if repository != {
		"full_name": "khallmark/OpenApoc",
		"node_id": "R_kgDOUBJ-Dg",
		"default_branch": "develop",
		"https_url": "https://github.com/khallmark/OpenApoc.git",
	}:
		raise ValidationError("repository identity differs from the reviewed P0 policy")

	anchor = policy["trust_anchor"]
	if not isinstance(anchor, dict):
		raise ValidationError("trust_anchor must be an object")
	require_exact_keys(
		anchor,
		{
			"ref",
			"receipt_ref",
			"required_object_type",
			"exact_paths",
			"protected_workflow_prefix",
			"required_context_markers",
			"bootstrap_receipt_schema",
		},
		"trust_anchor",
	)
	if (
		TAG_REF.fullmatch(anchor["ref"]) is None
		or TAG_REF.fullmatch(anchor["receipt_ref"]) is None
		or anchor["ref"] == anchor["receipt_ref"]
		or anchor["required_object_type"] != "tag"
	):
		raise ValidationError("trust anchor must be one fixed annotated tag ref")
	trust_paths = require_string_list(anchor["exact_paths"], "trust_anchor.exact_paths")
	if trust_paths != [
		".github/workflows/cmake.yml",
		".github/workflows/harness.yml",
		".github/workflows/lint.yml",
		".github/workflows/planning-scope.yml",
		"docs/timing/pr-p-policy-v1.json",
		"tools/validate_pr_p_scope.py",
	]:
		raise ValidationError("trust-root paths differ from the exact P0 set")
	if anchor["protected_workflow_prefix"] != ".github/workflows/":
		raise ValidationError("the complete workflow tree must remain protected")
	if anchor["required_context_markers"] != {
		"workflow_name_line": "name: Planning Scope",
		"job_id_line": "  trusted-pr-p-scope:",
		"exact_occurrence_count_each": 1,
	}:
		raise ValidationError("required-context uniqueness markers differ")
	if anchor["bootstrap_receipt_schema"] != "openapoc.planning_scope_trust_bootstrap_receipt.v1":
		raise ValidationError("unknown bootstrap receipt schema")

	candidate = policy["planning_candidate"]
	if not isinstance(candidate, dict):
		raise ValidationError("planning_candidate must be an object")
	require_exact_keys(candidate, {"head_repository_node_id", "head_ref"}, "planning_candidate")
	if candidate != {
		"head_repository_node_id": "R_kgDOUBJ-Dg",
		"head_ref": "khallmark/render-simulation-design",
	}:
		raise ValidationError("planning-candidate identity differs from the reviewed P0 policy")

	contract = policy["artifact_contract"]
	if not isinstance(contract, dict):
		raise ValidationError("artifact_contract must be an object")
	require_exact_keys(
		contract,
		{
			"exact_path_count",
			"exact_paths",
			"expected_status_by_path",
			"expected_mode_by_path",
			"sha256_by_path",
			"max_bytes_by_path",
			"maximum_total_bytes",
			"utf8_text_required",
			"one_trailing_lf_required",
			"json_root_schema_by_path",
			"python_ast_paths",
		},
		"artifact_contract",
	)
	paths = require_string_list(contract["exact_paths"], "artifact_contract.exact_paths")
	for index, item in enumerate(paths):
		require_canonical_path(item, f"artifact_contract.exact_paths[{index}]")
	if contract["exact_path_count"] != 22 or len(paths) != 22 or paths != sorted(paths):
		raise ValidationError("planning artifact paths must be the sorted exact 22-path set")
	path_set = set(paths)
	for map_name in (
		"expected_status_by_path",
		"expected_mode_by_path",
		"sha256_by_path",
		"max_bytes_by_path",
	):
		mapping = contract[map_name]
		if not isinstance(mapping, dict) or set(mapping) != path_set:
			raise ValidationError(f"{map_name} must cover the exact path set")
	if set(contract["expected_status_by_path"].values()) != {"A"}:
		raise ValidationError("the initial planning PR must add every frozen artifact")
	if set(contract["expected_mode_by_path"].values()) != {"100644"}:
		raise ValidationError("every planning artifact must be one regular 100644 blob")
	for path_name, digest in contract["sha256_by_path"].items():
		if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
			raise ValidationError(f"invalid SHA-256 for {path_name}")
	for path_name, ceiling in contract["max_bytes_by_path"].items():
		if not isinstance(ceiling, int) or isinstance(ceiling, bool) or ceiling <= 0:
			raise ValidationError(f"invalid byte ceiling for {path_name}")
	if (
		not isinstance(contract["maximum_total_bytes"], int)
		or isinstance(contract["maximum_total_bytes"], bool)
		or contract["maximum_total_bytes"] <= 0
		or contract["utf8_text_required"] is not True
		or contract["one_trailing_lf_required"] is not True
	):
		raise ValidationError("artifact byte/text policy is not closed")
	json_schemas = contract["json_root_schema_by_path"]
	if not isinstance(json_schemas, dict) or not set(json_schemas).issubset(path_set):
		raise ValidationError("JSON schema map is not a subset of the exact path set")
	python_paths = require_string_list(contract["python_ast_paths"], "python_ast_paths")
	if not set(python_paths).issubset(path_set):
		raise ValidationError("Python AST paths are not a subset of the exact path set")

	behavior = policy["non_candidate_behavior"]
	if behavior != {
		"always_run": True,
		"trust_root_change": "reject",
		"pre_P_base": "require_all_P_artifacts_remain_absent",
		"post_P_base": "require_all_P_artifacts_remain_byte_identical",
		"partial_or_drifted_P_base": "reject",
		"successful_result": "explicit_out_of_scope_preserved_not_skipped_or_neutral",
	}:
		raise ValidationError("non-candidate behavior is not the exact fail-closed state machine")
	policy["_policy_sha256"] = hashlib.sha256(policy_bytes).hexdigest()
	return policy


def isolated_environment() -> dict[str, str]:
	path = os.environ.get("PATH")
	if not path:
		raise ValidationError("PATH is unavailable")
	return {
		"PATH": path,
		"LANG": "C",
		"LC_ALL": "C",
		"GIT_CONFIG_NOSYSTEM": "1",
		"GIT_CONFIG_GLOBAL": os.devnull,
		"GIT_NO_REPLACE_OBJECTS": "1",
		"GIT_OPTIONAL_LOCKS": "0",
		"GIT_PROTOCOL_FROM_USER": "0",
		"GIT_TERMINAL_PROMPT": "0",
	}


class BareRepository:
	def __init__(self, path: Path):
		self.path = path
		self.environment = isolated_environment()

	def run(self, *args: str, check: bool = True) -> subprocess.CompletedProcess[bytes]:
		command = [
			"git",
			"--no-pager",
			"--no-replace-objects",
			"-c",
			"core.hooksPath=/dev/null",
			"-c",
			"protocol.file.allow=never",
			"-c",
			"protocol.ext.allow=never",
			"-c",
			"diff.external=",
			"--git-dir",
			str(self.path),
			*args,
		]
		result = subprocess.run(
			command,
			check=False,
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			env=self.environment,
		)
		if check and result.returncode != 0:
			detail = result.stderr.decode("utf-8", errors="replace").strip()
			raise ValidationError(f"trusted git command failed ({args[0]}): {detail}")
		return result

	def initialize(self) -> None:
		result = subprocess.run(
			["git", "init", "--bare", "--quiet", str(self.path)],
			check=False,
			stdout=subprocess.PIPE,
			stderr=subprocess.PIPE,
			env=self.environment,
		)
		if result.returncode != 0:
			detail = result.stderr.decode("utf-8", errors="replace").strip()
			raise ValidationError(f"cannot initialize isolated bare repository: {detail}")

	def fetch(
		self,
		remote: str,
		default_branch: str,
		pull_number: str,
		anchor_ref: str,
		receipt_ref: str,
	) -> None:
		self.run(
			"fetch",
			"--quiet",
			"--force",
			"--no-tags",
			"--no-write-fetch-head",
			remote,
			f"+refs/heads/{default_branch}:refs/trusted/event-base",
			f"+refs/pull/{pull_number}/head:refs/untrusted/event-head",
			f"+{anchor_ref}:refs/trusted/anchor-tag",
			f"+{receipt_ref}:refs/trusted/anchor-receipt-tag",
		)

	def text(self, *args: str) -> str:
		return self.run(*args).stdout.decode("utf-8", errors="strict").strip()

	def require_ancestor(self, ancestor: str, descendant: str, label: str) -> None:
		result = self.run("merge-base", "--is-ancestor", ancestor, descendant, check=False)
		if result.returncode != 0:
			raise ValidationError(f"required ancestry failed: {label}")

	def tree_entry(self, commit: str, path: str) -> tuple[str, str, str] | None:
		require_canonical_path(path, "tree path")
		raw = self.run("ls-tree", "-z", commit, "--", path).stdout
		parts = raw.split(b"\0")
		if parts and parts[-1] == b"":
			parts.pop()
		if not parts:
			return None
		if len(parts) != 1 or b"\t" not in parts[0]:
			raise ValidationError(f"tree contains an ambiguous entry for {path}")
		metadata, raw_path = parts[0].split(b"\t", 1)
		try:
			actual_path = raw_path.decode("utf-8")
			mode, object_type, object_id = metadata.decode("ascii").split(" ")
		except (UnicodeDecodeError, ValueError) as exc:
			raise ValidationError(f"malformed tree entry for {path}") from exc
		if actual_path != path or FULL_SHA.fullmatch(object_id) is None:
			raise ValidationError(f"tree entry identity mismatch for {path}")
		return mode, object_type, object_id

	def object_bytes(
		self, object_id: str, expected_type: str, maximum_bytes: int, label: str
	) -> bytes:
		require_full_sha(object_id, f"object ID for {label}")
		if self.text("cat-file", "-t", object_id) != expected_type:
			raise ValidationError(f"object type differs for {label}")
		try:
			size = int(self.text("cat-file", "-s", object_id))
		except ValueError as exc:
			raise ValidationError(f"non-integer object size for {label}") from exc
		if size < 0 or size > maximum_bytes:
			raise ValidationError(f"object exceeds reviewed byte ceiling for {label}: {size}")
		data = self.run("cat-file", expected_type, object_id).stdout
		if len(data) != size:
			raise ValidationError(f"object size changed while reading {label}")
		return data

	def blob_bytes(self, object_id: str, maximum_bytes: int, path: str) -> bytes:
		return self.object_bytes(object_id, "blob", maximum_bytes, path)

	def changed_paths(self, parent: str, child: str) -> dict[str, str]:
		raw = self.run(
			"diff-tree",
			"--no-commit-id",
			"--name-status",
			"-r",
			"-z",
			"--no-renames",
			parent,
			child,
		).stdout
		return parse_name_status(raw)

	def workflow_tree(self, commit: str, prefix: str) -> dict[str, tuple[str, str, str]]:
		raw = self.run("ls-tree", "-r", "-z", commit, "--", prefix).stdout
		result: dict[str, tuple[str, str, str]] = {}
		for item in (entry for entry in raw.split(b"\0") if entry):
			if b"\t" not in item:
				raise ValidationError("malformed protected workflow tree entry")
			metadata, raw_path = item.split(b"\t", 1)
			try:
				path = raw_path.decode("utf-8")
				mode, object_type, object_id = metadata.decode("ascii").split(" ")
			except (UnicodeDecodeError, ValueError) as exc:
				raise ValidationError("malformed protected workflow tree") from exc
			require_canonical_path(path, "protected workflow path")
			if not path.startswith(prefix) or path in result:
				raise ValidationError("protected workflow path escaped or duplicated")
			if mode != "100644" or object_type != "blob" or FULL_SHA.fullmatch(object_id) is None:
				raise ValidationError(f"protected workflow is not one 100644 blob: {path}")
			result[path] = (mode, object_type, object_id)
		if not result:
			raise ValidationError("protected workflow tree is empty")
		return result


def parse_name_status(raw: bytes) -> dict[str, str]:
	if raw and not raw.endswith(b"\0"):
		raise ValidationError("name-status stream lacks its terminal NUL")
	try:
		parts = raw.decode("utf-8").split("\0")
	except UnicodeDecodeError as exc:
		raise ValidationError("changed paths are not UTF-8") from exc
	if parts and parts[-1] == "":
		parts.pop()
	if len(parts) % 2:
		raise ValidationError("malformed name-status stream")
	result: dict[str, str] = {}
	for index in range(0, len(parts), 2):
		status, path = parts[index : index + 2]
		require_canonical_path(path, "changed path")
		if path in result:
			raise ValidationError(f"duplicate changed path: {path}")
		result[path] = status
	return result


def read_annotated_tag(
	repository: BareRepository, ref: str, expected_tag_name: str
) -> dict[str, str]:
	object_type = repository.text("cat-file", "-t", ref)
	if object_type != "tag":
		raise ValidationError(f"protected trust ref is not an annotated tag: {ref}")
	object_id = require_full_sha(repository.text("rev-parse", ref), f"tag object ID for {ref}")
	raw = repository.object_bytes(object_id, "tag", 16_384, ref)
	if b"\n\n" not in raw:
		raise ValidationError(f"annotated tag has no message separator: {ref}")
	raw_headers, message = raw.split(b"\n\n", 1)
	try:
		header_lines = raw_headers.decode("utf-8").splitlines()
	except UnicodeDecodeError as exc:
		raise ValidationError(f"annotated tag headers are not UTF-8: {ref}") from exc
	if len(header_lines) != 4:
		raise ValidationError(f"annotated tag must have exactly object/type/tag/tagger headers: {ref}")
	parsed: dict[str, str] = {}
	for line in header_lines:
		if " " not in line:
			raise ValidationError(f"malformed annotated tag header: {ref}")
		key, value = line.split(" ", 1)
		if key in parsed:
			raise ValidationError(f"duplicate annotated tag header: {ref}")
		parsed[key] = value
	if set(parsed) != {"object", "type", "tag", "tagger"}:
		raise ValidationError(f"annotated tag header set differs: {ref}")
	if parsed["type"] != "commit" or parsed["tag"] != expected_tag_name:
		raise ValidationError(f"annotated tag is nested or misnamed: {ref}")
	target_commit = require_full_sha(parsed["object"], f"direct tag target for {ref}")
	if repository.text("cat-file", "-t", target_commit) != "commit":
		raise ValidationError(f"annotated tag does not point directly to a commit: {ref}")
	return {
		"object_id": object_id,
		"target_commit": target_commit,
		"message": message.decode("utf-8", errors="strict"),
	}


def validate_bootstrap_tags(
	repository: BareRepository, policy: dict[str, Any]
) -> dict[str, str]:
	anchor_policy = policy["trust_anchor"]
	anchor_name = anchor_policy["ref"].removeprefix("refs/tags/")
	receipt_name = anchor_policy["receipt_ref"].removeprefix("refs/tags/")
	anchor = read_annotated_tag(repository, "refs/trusted/anchor-tag", anchor_name)
	receipt = read_annotated_tag(
		repository, "refs/trusted/anchor-receipt-tag", receipt_name
	)
	if anchor["message"] != "openapoc-planning-scope-trust-v1\n":
		raise ValidationError("trust tag annotation differs from the exact bootstrap marker")
	if receipt["target_commit"] != anchor["target_commit"]:
		raise ValidationError("receipt tag and trust tag do not point directly to the same P0 commit")
	receipt_payload = parse_json(receipt["message"].encode("utf-8"), "bootstrap receipt tag")
	require_exact_keys(
		receipt_payload,
		{
			"schema",
			"bootstrap_operation_id",
			"trust_ref_absent_before_push",
			"receipt_ref_absent_before_push",
			"atomic_create_only_push",
			"trust_tag_object_oid",
			"peeled_P0_commit_sha",
			"peeled_P0_tree_sha",
			"provider_actor",
			"tag_ruleset_receipt_sha256",
		},
		"bootstrap receipt tag",
	)
	if receipt_payload["schema"] != anchor_policy["bootstrap_receipt_schema"]:
		raise ValidationError("bootstrap receipt tag schema differs")
	if OPERATION_ID.fullmatch(receipt_payload["bootstrap_operation_id"]) is None:
		raise ValidationError("bootstrap operation ID is not one lowercase UUID")
	for flag in (
		"trust_ref_absent_before_push",
		"receipt_ref_absent_before_push",
		"atomic_create_only_push",
	):
		if receipt_payload[flag] is not True:
			raise ValidationError(f"bootstrap receipt does not prove {flag}")
	if receipt_payload["trust_tag_object_oid"] != anchor["object_id"]:
		raise ValidationError("bootstrap receipt does not bind the exact trust tag object")
	if receipt_payload["peeled_P0_commit_sha"] != anchor["target_commit"]:
		raise ValidationError("bootstrap receipt does not bind the direct P0 commit")
	peeled_tree = require_full_sha(
		repository.text("rev-parse", f"{anchor['target_commit']}^{{tree}}"), "P0 tree"
	)
	if receipt_payload["peeled_P0_tree_sha"] != peeled_tree:
		raise ValidationError("bootstrap receipt does not bind the exact P0 tree")
	if receipt_payload["provider_actor"] != "khallmark":
		raise ValidationError("bootstrap receipt provider actor differs")
	if re.fullmatch(r"[0-9a-f]{64}", receipt_payload["tag_ruleset_receipt_sha256"]) is None:
		raise ValidationError("bootstrap receipt lacks the exact tag-ruleset receipt digest")
	canonical = json.dumps(receipt_payload, sort_keys=True, separators=(",", ":")).encode() + b"\n"
	if receipt["message"].encode("utf-8") != canonical:
		raise ValidationError("bootstrap receipt tag is not canonical one-line JSON")
	return {
		"trust_tag_object_oid": anchor["object_id"],
		"receipt_tag_object_oid": receipt["object_id"],
		"anchor_commit": anchor["target_commit"],
		"anchor_tree": peeled_tree,
		"tag_ruleset_receipt_sha256": receipt_payload["tag_ruleset_receipt_sha256"],
	}


def require_same_trust_root(
	repository: BareRepository, anchor: str, base: str, head: str, paths: list[str]
) -> None:
	for path in paths:
		anchor_entry = repository.tree_entry(anchor, path)
		base_entry = repository.tree_entry(base, path)
		head_entry = repository.tree_entry(head, path)
		if anchor_entry is None or anchor_entry[0:2] != ("100644", "blob"):
			raise ValidationError(f"trust anchor lacks regular trust-root blob: {path}")
		if base_entry != anchor_entry:
			raise ValidationError(f"event base trust-root blob differs from protected P0: {path}")
		if head_entry != base_entry:
			raise ValidationError(f"ordinary PR attempts to change protected P0 trust root: {path}")


def require_same_unique_workflow_tree(
	repository: BareRepository,
	anchor: str,
	base: str,
	head: str,
	policy: dict[str, Any],
) -> None:
	anchor_policy = policy["trust_anchor"]
	prefix = anchor_policy["protected_workflow_prefix"]
	anchor_tree = repository.workflow_tree(anchor, prefix)
	if repository.workflow_tree(base, prefix) != anchor_tree:
		raise ValidationError("event-base workflow tree differs from the exact P0 anchor")
	if repository.workflow_tree(head, prefix) != anchor_tree:
		raise ValidationError("ordinary PR attempts to alter or add an Actions workflow")
	markers = anchor_policy["required_context_markers"]
	counts = {markers["workflow_name_line"]: 0, markers["job_id_line"]: 0}
	for path, entry in anchor_tree.items():
		data = repository.blob_bytes(entry[2], 1_048_576, path)
		try:
			lines = data.decode("utf-8").splitlines()
		except UnicodeDecodeError as exc:
			raise ValidationError(f"protected workflow is not UTF-8: {path}") from exc
		for marker in counts:
			counts[marker] += lines.count(marker)
	for marker, count in counts.items():
		if count != markers["exact_occurrence_count_each"]:
			raise ValidationError(f"required check context marker is not unique: {marker}")


def artifact_state(
	repository: BareRepository, commit: str, contract: dict[str, Any]
) -> str:
	paths = contract["exact_paths"]
	entries = {path: repository.tree_entry(commit, path) for path in paths}
	if all(entry is None for entry in entries.values()):
		return "pre_P"
	if any(entry is None for entry in entries.values()):
		return "partial_or_drifted"
	for path, entry in entries.items():
		assert entry is not None
		if entry[0:2] != (contract["expected_mode_by_path"][path], "blob"):
			return "partial_or_drifted"
		data = repository.blob_bytes(entry[2], contract["max_bytes_by_path"][path], path)
		if hashlib.sha256(data).hexdigest() != contract["sha256_by_path"][path]:
			return "partial_or_drifted"
	return "post_P"


def require_single_commit_candidate_history(
	repository: BareRepository, base: str, head: str
) -> None:
	raw_lines = repository.text("rev-list", "--reverse", "--parents", f"{base}..{head}").splitlines()
	if len(raw_lines) != 1:
		raise ValidationError("planning candidate must contain exactly one commit")
	parts = raw_lines[0].split()
	if len(parts) != 2 or parts[0] != head or parts[1] != base:
		raise ValidationError("planning candidate must be the exact one-commit base-to-head edge")
	require_full_sha(parts[0], "candidate commit")


def require_exact_planning_artifacts(
	repository: BareRepository, base: str, head: str, contract: dict[str, Any]
) -> None:
	changes = repository.changed_paths(base, head)
	if changes != contract["expected_status_by_path"]:
		raise ValidationError(
			f"planning candidate diff is not the exact frozen path/status map: {changes}"
		)
	total_bytes = 0
	json_schemas = contract["json_root_schema_by_path"]
	python_paths = set(contract["python_ast_paths"])
	for path in contract["exact_paths"]:
		entry = repository.tree_entry(head, path)
		if entry is None or entry[0:2] != (contract["expected_mode_by_path"][path], "blob"):
			raise ValidationError(f"final planning artifact mode/type differs: {path}")
		data = repository.blob_bytes(entry[2], contract["max_bytes_by_path"][path], path)
		total_bytes += len(data)
		if hashlib.sha256(data).hexdigest() != contract["sha256_by_path"][path]:
			raise ValidationError(f"final planning artifact bytes differ from P0 policy: {path}")
		try:
			text = data.decode("utf-8")
		except UnicodeDecodeError as exc:
			raise ValidationError(f"planning artifact is not UTF-8: {path}") from exc
		if not data.endswith(b"\n") or data.endswith(b"\n\n"):
			raise ValidationError(f"planning artifact must end in exactly one LF: {path}")
		if path in json_schemas:
			value = parse_json(data, path)
			if value.get("schema") != json_schemas[path]:
				raise ValidationError(f"JSON root schema differs for {path}")
		if path in python_paths:
			try:
				ast.parse(text, filename=path)
			except SyntaxError as exc:
				raise ValidationError(f"Python AST parse failed for {path}: {exc}") from exc
	if total_bytes > contract["maximum_total_bytes"]:
		raise ValidationError("planning artifact set exceeds total reviewed byte ceiling")


def validate_event(args: argparse.Namespace, policy: dict[str, Any]) -> dict[str, Any]:
	repository_policy = policy["repository"]
	if args.repository != repository_policy["full_name"]:
		raise ValidationError("event repository name differs from P0 policy")
	if args.event_base_repo_node_id != repository_policy["node_id"]:
		raise ValidationError("event base repository node ID differs from P0 policy")
	if args.event_base_ref != repository_policy["default_branch"]:
		raise ValidationError("event base ref is not the default branch")
	if PR_NUMBER.fullmatch(args.pr_number) is None:
		raise ValidationError("pull-request number is not a bounded positive decimal")
	base_sha = require_full_sha(args.event_base_sha, "event base SHA")
	head_sha = require_full_sha(args.event_head_sha, "event head SHA")
	workflow_sha = require_full_sha(args.workflow_sha, "workflow source SHA")
	github_sha = require_full_sha(args.github_sha, "GITHUB_SHA")
	if workflow_sha != base_sha or github_sha != base_sha:
		raise ValidationError("workflow bytes and pull_request_target GITHUB_SHA are not event-base bytes")

	with tempfile.TemporaryDirectory(prefix="openapoc-planning-scope-") as temporary:
		bare = BareRepository(Path(temporary) / "objects.git")
		bare.initialize()
		bare.fetch(
			repository_policy["https_url"],
			repository_policy["default_branch"],
			args.pr_number,
			policy["trust_anchor"]["ref"],
			policy["trust_anchor"]["receipt_ref"],
		)
		resolved_base = require_full_sha(
			bare.text("rev-parse", "refs/trusted/event-base^{commit}"), "resolved default head"
		)
		resolved_head = require_full_sha(
			bare.text("rev-parse", "refs/untrusted/event-head^{commit}"), "resolved PR head"
		)
		bootstrap = validate_bootstrap_tags(bare, policy)
		anchor_commit = bootstrap["anchor_commit"]
		if resolved_base != base_sha or resolved_head != head_sha:
			raise ValidationError("fetched immutable refs differ from event base/head SHAs")
		bare.require_ancestor(anchor_commit, resolved_base, "P0 anchor must be an ancestor of event base")
		bare.require_ancestor(resolved_base, resolved_head, "event base must be an ancestor of event head")
		require_same_trust_root(
			bare,
			anchor_commit,
			resolved_base,
			resolved_head,
			policy["trust_anchor"]["exact_paths"],
		)
		require_same_unique_workflow_tree(
			bare, anchor_commit, resolved_base, resolved_head, policy
		)

		contract = policy["artifact_contract"]
		base_state = artifact_state(bare, resolved_base, contract)
		is_exact_candidate = (
			args.event_head_repo_node_id == policy["planning_candidate"]["head_repository_node_id"]
			and args.event_head_ref == policy["planning_candidate"]["head_ref"]
		)
		if is_exact_candidate:
			if base_state != "pre_P":
				raise ValidationError("exact planning candidate does not start from the pre-P base state")
			require_single_commit_candidate_history(bare, resolved_base, resolved_head)
			require_exact_planning_artifacts(bare, resolved_base, resolved_head, contract)
			mode = "exact_planning_candidate_accepted"
		else:
			if base_state == "partial_or_drifted":
				raise ValidationError("event base has a partial or drifted planning artifact set")
			head_state = artifact_state(bare, resolved_head, contract)
			if head_state != base_state:
				raise ValidationError("non-planning PR changes the protected planning artifact state")
			mode = "explicit_out_of_scope_preserved"

		return {
			"schema": "openapoc.pr_p_validation_receipt.v1",
			"result": "success",
			"mode": mode,
			"policy_sha256": policy["_policy_sha256"],
			"trust_anchor_ref": policy["trust_anchor"]["ref"],
			"trust_anchor_receipt_ref": policy["trust_anchor"]["receipt_ref"],
			"trust_tag_object_oid": bootstrap["trust_tag_object_oid"],
			"receipt_tag_object_oid": bootstrap["receipt_tag_object_oid"],
			"trust_anchor_commit": anchor_commit,
			"trust_anchor_tree": bootstrap["anchor_tree"],
			"tag_ruleset_receipt_sha256": bootstrap["tag_ruleset_receipt_sha256"],
			"event_base_sha": resolved_base,
			"event_head_sha": resolved_head,
			"planning_base_state": base_state,
		}


def run_self_test() -> None:
	require_canonical_path("docs/timing/pr-p-scope-v1.json", "sentinel")
	for invalid in ("", "/absolute", "../escape", "a//b", "a\\b", "a:b", "a/./b"):
		try:
			require_canonical_path(invalid, "sentinel")
		except ValidationError:
			continue
		raise ValidationError(f"path sentinel unexpectedly accepted: {invalid!r}")
	if parse_name_status(b"A\0docs/a.json\0") != {"docs/a.json": "A"}:
		raise ValidationError("name-status positive sentinel failed")
	for malformed in (b"A\0docs/a.json", b"A\0docs/a.json\0M\0"):
		try:
			parse_name_status(malformed)
		except ValidationError:
			continue
		raise ValidationError("name-status malformed sentinel unexpectedly passed")
	try:
		parse_json(b'{"a":1,"a":2}\n', "duplicate sentinel")
	except ValidationError:
		pass
	else:
		raise ValidationError("duplicate JSON sentinel unexpectedly passed")

	class HistorySentinel:
		def __init__(self, output: str):
			self.output = output

		def text(self, *args: str) -> str:
			if args != ("rev-list", "--reverse", "--parents", f"{'a' * 40}..{'b' * 40}"):
				raise ValidationError("history sentinel received unexpected git arguments")
			return self.output

	base = "a" * 40
	head = "b" * 40
	require_single_commit_candidate_history(HistorySentinel(f"{head} {base}\n"), base, head)  # type: ignore[arg-type]
	invalid_histories = (
		"",
		f"{'c' * 40} {base}\n{head} {'c' * 40}\n",
		f"{head} {base} {'c' * 40}\n",
		f"{'c' * 40} {base}\n",
		f"{head} {'c' * 40}\n",
	)
	for history in invalid_histories:
		try:
			require_single_commit_candidate_history(HistorySentinel(history), base, head)  # type: ignore[arg-type]
		except ValidationError:
			continue
		raise ValidationError("multi-commit or misbound history sentinel unexpectedly passed")
	print("trusted planning-scope validator self-test passed")


def parse_args() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--self-test", action="store_true")
	parser.add_argument("--policy", type=Path)
	parser.add_argument("--repository")
	parser.add_argument("--pr-number")
	parser.add_argument("--event-base-repo-node-id")
	parser.add_argument("--event-base-ref")
	parser.add_argument("--event-base-sha")
	parser.add_argument("--event-head-repo-node-id")
	parser.add_argument("--event-head-ref")
	parser.add_argument("--event-head-sha")
	parser.add_argument("--workflow-sha")
	parser.add_argument("--github-sha")
	args = parser.parse_args()
	if args.self_test:
		return args
	required = (
		"policy",
		"repository",
		"pr_number",
		"event_base_repo_node_id",
		"event_base_ref",
		"event_base_sha",
		"event_head_repo_node_id",
		"event_head_ref",
		"event_head_sha",
		"workflow_sha",
		"github_sha",
	)
	missing = [name for name in required if getattr(args, name) is None]
	if missing:
		parser.error(f"missing required arguments: {', '.join(missing)}")
	return args


def main() -> int:
	try:
		args = parse_args()
		if args.self_test:
			run_self_test()
			return 0
		policy = load_policy(args.policy)
		receipt = validate_event(args, policy)
		print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
		return 0
	except ValidationError as exc:
		print(f"trusted planning-scope validation failed: {exc}", file=os.sys.stderr)
		return 1


if __name__ == "__main__":
	raise SystemExit(main())
