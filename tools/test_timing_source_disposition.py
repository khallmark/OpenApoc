#!/usr/bin/env python3
"""Prove every frozen upstream timing artifact has an exact ledger disposition."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "docs" / "timing" / "source-disposition-map-v1.json"
LEDGER_PATH = ROOT / "docs" / "timing" / "source-disposition.md"
SNAPSHOT_ROOT = ROOT / "docs" / "timing" / "source-snapshots" / "v1"

EXPECTED_SOURCES = {"pr-1166", "pr-1237", "pr-1270"}
EXPECTED_ROW_BOUNDS = {
	"R1166": {"first": 1, "last": 13},
	"R1237": {"first": 1, "last": 15},
	"R1270": {"first": 1, "last": 12},
	"TA": {"first": 1, "last": 41},
	"TS": {"first": 1, "last": 47},
}
EXPECTED_SUPPLEMENTAL_CROSS_LINKS = {
	"TS-44": ["TS-33", "TS-36", "TS-43"],
	"TS-45": ["TS-38", "TS-43"],
	"TS-46": ["TS-43"],
	"TS-47": ["TS-41"],
}
ROW_PATTERN = re.compile(r"^\| ([A-Z][A-Z0-9]*-\d{2}[a-z]?) \|")


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
	value: dict[str, Any] = {}
	for key, item in pairs:
		if key in value:
			raise ValueError(f"duplicate JSON key: {key}")
		value[key] = item
	return value


def load_json(path: Path) -> dict[str, Any]:
	return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys)


def normalized_lines(value: str) -> list[str]:
	"""Split without retaining a CR from captured GitHub CRLF text."""

	return [line[:-1] if line.endswith("\r") else line for line in value.split("\n")]


def expected_rows(prefix: str, bounds: dict[str, int]) -> set[str]:
	return {
		f"{prefix}-{ordinal:02d}"
		for ordinal in range(bounds["first"], bounds["last"] + 1)
	}


def parsed_hunk_locators(files: list[dict[str, Any]]) -> list[tuple[str, str, int]]:
	locators: list[tuple[str, str, int]] = []
	occurrences: dict[tuple[str, str], int] = {}
	for file_record in files:
		filename = file_record["filename"]
		for line in normalized_lines(file_record["patch"]):
			if not line.startswith("@@"):
				continue
			key = (filename, line)
			occurrences[key] = occurrences.get(key, 0) + 1
			locators.append((filename, line, occurrences[key]))
	return locators


class TimingSourceDispositionTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		cls.mapping = load_json(MAP_PATH)
		cls.ledger = LEDGER_PATH.read_text(encoding="utf-8")
		cls.ledger_rows: dict[str, list[str]] = {}
		for line in normalized_lines(cls.ledger):
			match = ROW_PATTERN.match(line)
			if match:
				cls.ledger_rows.setdefault(match.group(1), []).append(line)
		cls.row_ids = set(cls.ledger_rows)

	def assert_disposition_rows(
		self, rows: Any, label: object, *, empty_allowed: bool = False
	) -> set[str]:
		self.assertIsInstance(rows, list, label)
		self.assertTrue(all(isinstance(row, str) for row in rows), label)
		self.assertEqual(len(rows), len(set(rows)), label)
		if not empty_allowed:
			self.assertTrue(rows, label)
		self.assertTrue(set(rows).issubset(self.row_ids), (label, rows))
		return set(rows)

	def test_required_row_sets_are_exact(self) -> None:
		self.assertEqual(self.mapping["required_disposition_row_sets"], EXPECTED_ROW_BOUNDS)
		self.assertIn("TS", self.mapping["required_disposition_row_sets"])
		self.assertTrue(all(len(lines) == 1 for lines in self.ledger_rows.values()))
		for prefix, bounds in EXPECTED_ROW_BOUNDS.items():
			actual = {row_id for row_id in self.row_ids if row_id.startswith(f"{prefix}-")}
			self.assertEqual(actual, expected_rows(prefix, bounds), prefix)

	def test_required_supplemental_cross_links_are_literal(self) -> None:
		cross_links = self.mapping["required_supplemental_cross_links"]
		self.assertEqual(cross_links, EXPECTED_SUPPLEMENTAL_CROSS_LINKS)
		for supplemental_row, linked_rows in cross_links.items():
			self.assertIn(supplemental_row, self.ledger_rows)
			self.assertEqual(len(linked_rows), len(set(linked_rows)), supplemental_row)
			for linked_row in linked_rows:
				self.assertIn(linked_row, self.ledger_rows, (supplemental_row, linked_row))
				self.assertIn(
					f"`{supplemental_row}`",
					self.ledger_rows[linked_row][0],
					(supplemental_row, linked_row),
				)

	def test_every_frozen_artifact_has_an_exact_disposition(self) -> None:
		self.assertEqual(set(self.mapping["sources"]), EXPECTED_SOURCES)
		total_files = 0
		total_hunks = 0
		total_comments = 0

		for source_id, source_map in self.mapping["sources"].items():
			snapshot = load_json(SNAPSHOT_ROOT / f"{source_id}.json")
			expected = source_map["expected"]
			files = snapshot["files"]
			comments = snapshot["issue_comments"]
			body = snapshot["resource"]["body"]
			hunk_locators = parsed_hunk_locators(files)

			self.assertEqual(len(files), expected["file_count"], source_id)
			self.assertEqual(len(hunk_locators), expected["hunk_count"], source_id)
			self.assertEqual(1 if body else 0, expected["body_count"], source_id)
			self.assertEqual(len(comments), expected["issue_comment_count"], source_id)
			self.assertEqual(len(snapshot["reviews"]), expected["review_count"], source_id)
			self.assertEqual(
				len(snapshot["review_comments"]), expected["review_comment_count"], source_id
			)

			artifact_rows = set()
			body_lines = normalized_lines(body)
			actual_body_locators = {
				(line_index, line_text)
				for line_index, line_text in enumerate(body_lines)
				if line_text.strip()
			}
			mapped_body_locators: list[tuple[int, str]] = []
			for segment in source_map["body_segments"]:
				line_index = segment["line_index"]
				line_text = segment["line_text"]
				kind = segment["kind"]
				self.assertIs(type(line_index), int, (source_id, segment))
				self.assertGreaterEqual(line_index, 0, (source_id, segment))
				self.assertIsInstance(line_text, str, (source_id, segment))
				self.assertTrue(line_text.strip(), (source_id, segment))
				self.assertIn(kind, {"context_no_requirement", "requirement"})
				rows = self.assert_disposition_rows(
					segment["disposition_rows"],
					(source_id, "body", line_index),
					empty_allowed=kind == "context_no_requirement",
				)
				if kind == "context_no_requirement":
					self.assertFalse(rows, (source_id, "body", line_index))
				else:
					self.assertTrue(rows, (source_id, "body", line_index))
				artifact_rows.update(rows)
				mapped_body_locators.append((line_index, line_text))

			self.assertEqual(
				len(mapped_body_locators), len(set(mapped_body_locators)), (source_id, "body")
			)
			self.assertEqual(set(mapped_body_locators), actual_body_locators, (source_id, "body"))

			actual_comment_ids = {str(comment["id"]) for comment in comments}
			comment_map = source_map["issue_comment_dispositions"]
			self.assertEqual(set(comment_map), actual_comment_ids, (source_id, "comments"))
			for comment_id, rows in comment_map.items():
				artifact_rows.update(
					self.assert_disposition_rows(rows, (source_id, "comment", comment_id))
				)

			mapped_hunk_locators: list[tuple[str, str, int]] = []
			for hunk in source_map["hunk_dispositions"]:
				filename = hunk["filename"]
				hunk_header = hunk["hunk_header"]
				occurrence = hunk["occurrence"]
				self.assertIsInstance(filename, str, (source_id, hunk))
				self.assertIsInstance(hunk_header, str, (source_id, hunk))
				self.assertTrue(hunk_header.startswith("@@"), (source_id, hunk))
				self.assertIs(type(occurrence), int, (source_id, hunk))
				self.assertGreater(occurrence, 0, (source_id, hunk))
				artifact_rows.update(
					self.assert_disposition_rows(
						hunk["disposition_rows"],
						(source_id, "hunk", filename, hunk_header, occurrence),
					)
				)
				mapped_hunk_locators.append((filename, hunk_header, occurrence))

			self.assertEqual(
				len(mapped_hunk_locators),
				len(set(mapped_hunk_locators)),
				(source_id, "hunks"),
			)
			self.assertEqual(set(mapped_hunk_locators), set(hunk_locators), (source_id, "hunks"))

			retired = self.assert_disposition_rows(
				source_map["retired_dispositions_with_no_source_artifact"],
				(source_id, "retired"),
				empty_allowed=True,
			)
			self.assertTrue(artifact_rows.isdisjoint(retired), (source_id, "retired"))
			prefix = f"R{source_id.removeprefix('pr-')}"
			self.assertEqual(
				artifact_rows | retired,
				expected_rows(prefix, EXPECTED_ROW_BOUNDS[prefix]),
				(source_id, "artifact row union"),
			)

			total_files += len(files)
			total_hunks += len(hunk_locators)
			total_comments += len(comments)

		self.assertEqual(total_files, 28)
		self.assertEqual(total_hunks, 40)
		self.assertEqual(total_comments, 8)

	def test_local_design_remains_authoritative(self) -> None:
		self.assertTrue(
			self.mapping["completeness"][
				"upstream_architecture_is_evidence_only_and_local_plan_is_authoritative"
			]
		)
		self.assertIn("three-clock design is authoritative", self.ledger)


if __name__ == "__main__":
	unittest.main()
