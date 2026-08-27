#!/usr/bin/env python3
"""Pure regression tests for deterministic timing-source capture."""

from __future__ import annotations

import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path

import capture_timing_sources as capture


def user(login: str = "author", ident: int = 1) -> dict:
    return {"login": login, "id": ident, "node_id": f"U_{ident}"}


def issue_resource(number: int = 9, body: str = "body\nwith ünicode") -> dict:
    return {
        "id": number,
        "node_id": f"I_{number}",
        "number": number,
        "state": "open",
        "state_reason": None,
        "title": "timing source",
        "body": body,
        "created_at": "2020-01-01T00:00:00Z",
        "updated_at": "2020-01-02T00:00:00Z",
        "closed_at": None,
        "html_url": f"https://github.com/OpenApoc/OpenApoc/issues/{number}",
        "author_association": "CONTRIBUTOR",
        "locked": False,
        "comments": 1,
        "user": user(),
        "labels": [],
        "milestone": None,
        "assignees": [],
    }


def pr_resource(number: int = 8, body: str = "body") -> dict:
    value = issue_resource(number, body)
    value.pop("state_reason")
    value["html_url"] = f"https://github.com/OpenApoc/OpenApoc/pull/{number}"
    value.update(
        {
            "merged_at": None,
            "merge_commit_sha": None,
            "draft": False,
            "review_comments": 0,
            "commits": 1,
            "additions": 2,
            "deletions": 1,
            "changed_files": 1,
            "requested_reviewers": [],
            "base": {
                "ref": "master",
                "sha": "b" * 40,
                "repo": {"id": 1, "node_id": "R_1", "full_name": "OpenApoc/OpenApoc"},
            },
            "head": {
                "ref": "topic",
                "sha": "h" * 40,
                "repo": {"id": 1, "node_id": "R_1", "full_name": "OpenApoc/OpenApoc"},
            },
        }
    )
    return value


def comment(ident: int = 11) -> dict:
    return {
        "id": ident,
        "node_id": f"IC_{ident}",
        "body": "comment",
        "created_at": "2020-01-01T00:00:00Z",
        "updated_at": "2020-01-01T00:00:00Z",
        "html_url": f"https://github.com/x/y/issues/9#issuecomment-{ident}",
        "author_association": "MEMBER",
        "user": user("commenter", 2),
    }


def commit() -> dict:
    return {
        "sha": "c" * 40,
        "parents": [{"sha": "p" * 40}],
        "author": user("author", 1),
        "committer": user("committer", 2),
        "commit": {
            "tree": {"sha": "t" * 40},
            "author": {"name": "A", "email": "a@example.test", "date": "2020-01-01T00:00:00Z"},
            "committer": {"name": "C", "email": "c@example.test", "date": "2020-01-01T00:00:00Z"},
            "message": "test commit",
        },
    }


def changed_file() -> dict:
    return {
        "sha": "f" * 40,
        "filename": "framework/framework.cpp",
        "status": "modified",
        "additions": 2,
        "deletions": 1,
        "changes": 3,
        "blob_url": "https://github.com/blob",
        "raw_url": "https://github.com/raw",
        "contents_url": "https://api.github.com/content",
        "patch": "@@ -1 +1 @@\n-old\n+new",
    }


class FakeClient:
    def __init__(self, resource: dict, collections: dict[str, list[dict]], mutate_after: int = 0):
        self.resource = resource
        self.collections = collections
        self.resource_calls = 0
        self.mutate_after = mutate_after

    def one(self, endpoint: str) -> dict:
        self.resource_calls += 1
        value = json.loads(json.dumps(self.resource))
        if self.mutate_after and self.resource_calls > self.mutate_after:
            value["body"] += " changed"
        return value

    def collection(self, endpoint: str) -> list[dict]:
        for suffix, values in self.collections.items():
            if endpoint.endswith(suffix):
                return json.loads(json.dumps(values))
        return []


class CaptureTests(unittest.TestCase):
    def test_canonical_bytes_are_sorted_utf8_and_keep_newline(self) -> None:
        self.assertEqual(capture.canonical_bytes({"z": "ü\n", "a": 1}), b'{"a":1,"z":"\xc3\xbc\\n"}\n')

    def test_canonical_bytes_reject_float(self) -> None:
        with self.assertRaises(capture.SpecError):
            capture.canonical_bytes({"not_allowed": 1.5})

    def test_next_link_requires_same_host_endpoint_and_next_page(self) -> None:
        current = "https://api.github.com/repos/a/b/issues/1/comments?per_page=100&page=1"
        good = '<https://api.github.com/repos/a/b/issues/1/comments?per_page=100&page=2>; rel="next"'
        self.assertEqual(capture.parse_next_link(current, good, 1), good.split("<", 1)[1].split(">", 1)[0])
        for bad in (
            '<https://evil.test/repos/a/b/issues/1/comments?per_page=100&page=2>; rel="next"',
            '<https://api.github.com/repos/a/b/issues/2/comments?per_page=100&page=2>; rel="next"',
            '<https://api.github.com/repos/a/b/issues/1/comments?per_page=100&page=3>; rel="next"',
        ):
            with self.assertRaises(capture.TransportError):
                capture.parse_next_link(current, bad, 1)

    def test_collection_follows_two_pages_and_rejects_duplicate_ids(self) -> None:
        first = "https://api.github.com/x?per_page=100&page=1"
        second = "https://api.github.com/x?per_page=100&page=2"
        responses = {
            first: capture.Response(200, {"link": f'<{second}>; rel="next"'}, b'[{"id":1}]', first),
            second: capture.Response(200, {}, b'[{"id":2}]', second),
        }
        client = capture.GitHubClient(lambda url: responses[url])
        self.assertEqual([row["id"] for row in client.collection("/x")], [1, 2])
        with self.assertRaises(capture.TransportError):
            capture._ids([{"id": 1}, {"id": 1}])

    def test_file_patch_is_mandatory(self) -> None:
        value = changed_file()
        value.pop("patch")
        with self.assertRaises(capture.TransportError):
            capture.normalize_file(value)

    def test_issue_capture_includes_empty_pr_arrays_and_exact_comment(self) -> None:
        source = {
            "id": "issue-9",
            "kind": "issue",
            "number": 9,
            "expected": {
                "updated_at": "2020-01-02T00:00:00Z",
                "issue_comment_ids": [11],
                "review_ids": [],
                "review_comment_ids": [],
            },
        }
        client = FakeClient(issue_resource(), {"/comments": [comment()]})
        result = capture.capture_once(client, "OpenApoc/OpenApoc", source)
        self.assertEqual(result["issue_comments"][0]["id"], 11)
        self.assertEqual(result["reviews"], [])
        self.assertEqual(result["review_comments"], [])
        self.assertEqual(result["commits"], [])
        self.assertEqual(result["files"], [])

    def test_pr_capture_sorts_and_cross_checks_counts(self) -> None:
        source = {
            "id": "pr-8",
            "kind": "pr",
            "number": 8,
            "expected": {
                "updated_at": "2020-01-02T00:00:00Z",
                "head_sha": "h" * 40,
                "base_sha": "b" * 40,
                "commit_count": 1,
                "changed_file_count": 1,
                "issue_comment_ids": [11],
                "review_ids": [],
                "review_comment_ids": [],
            },
        }
        collections = {
            "/issues/8/comments": [comment()],
            "/pulls/8/reviews": [],
            "/pulls/8/comments": [],
            "/pulls/8/commits": [commit()],
            "/pulls/8/files": [changed_file()],
        }
        result = capture.capture_once(FakeClient(pr_resource(), collections), "OpenApoc/OpenApoc", source)
        self.assertEqual(result["resource"]["head"]["sha"], "h" * 40)
        self.assertIn("patch", result["files"][0])

    def test_resource_mutation_inside_or_between_passes_fails(self) -> None:
        source = {
            "id": "issue-9",
            "kind": "issue",
            "number": 9,
            "expected": {
                "updated_at": "2020-01-02T00:00:00Z",
                "issue_comment_ids": [11],
                "review_ids": [],
                "review_comment_ids": [],
            },
        }
        collections = {"/comments": [comment()]}
        with self.assertRaises(capture.DriftError):
            capture.capture_once(
                FakeClient(issue_resource(), collections, mutate_after=1),
                "OpenApoc/OpenApoc",
                source,
            )
        with self.assertRaises(capture.DriftError):
            capture.capture_corpus_stable(
                FakeClient(issue_resource(), collections, mutate_after=2),
                "OpenApoc/OpenApoc",
                [source],
            )

    def test_corpus_passes_are_complete_before_comparison(self) -> None:
        sources = [{"id": "first"}, {"id": "second"}]
        version = {"first": 0, "second": 0}
        visits: list[str] = []

        def fake_capture_once(client, repository, source):
            source_id = source["id"]
            visits.append(source_id)
            if source_id == "second" and visits.count("second") == 1:
                version["first"] = 1
            return {"source_id": source_id, "version": version[source_id]}

        with self.assertRaises(capture.DriftError):
            capture.capture_corpus_stable(
                object(), "OpenApoc/OpenApoc", sources, capture_fn=fake_capture_once
            )
        self.assertEqual(visits, ["first", "second", "first", "second"])

    def test_committed_snapshots_match_exact_nested_schema(self) -> None:
        for path in sorted(capture.COMMITTED_ROOT.glob("*.json")):
            if path.name != "manifest.json":
                capture.validate_snapshot(capture.load_json(path))

    def test_youtube_sources_are_mutable_non_authoritative_locators(self) -> None:
        spec = capture.load_spec()
        locators = spec["mutable_non_authoritative_locators"]
        self.assertEqual(len(locators), 2)
        self.assertEqual(
            {locator["disposition"] for locator in locators},
            {"mutable_locator_only_cannot_authorize_code_or_parity"},
        )

    def test_nested_schema_rejects_extra_and_missing_fields(self) -> None:
        original = capture.load_json(capture.COMMITTED_ROOT / "pr-1166.json")
        extra = copy.deepcopy(original)
        extra["resource"]["not_in_projection"] = True
        with self.assertRaises(capture.IntegrityError):
            capture.validate_snapshot(extra)

        missing = copy.deepcopy(original)
        missing["issue_comments"][0].pop("body")
        with self.assertRaises(capture.IntegrityError):
            capture.validate_snapshot(missing)

    def test_offline_verifier_executes_nested_schema(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "v1"
            shutil.copytree(capture.COMMITTED_ROOT, root)
            target = root / "pr-1166.json"
            value = capture.load_json(target)
            value["resource"]["not_in_projection"] = True
            target.write_bytes(capture.canonical_bytes(value))

            spec = capture.load_spec()
            source_files = []
            for source in spec["sources"]:
                filename = f"{source['id']}.json"
                source_files.append((source["id"], filename, capture.sha256_file(root / filename)))
            (root / "manifest.json").write_bytes(
                capture.canonical_bytes(capture.build_manifest(source_files))
            )
            with self.assertRaises(capture.IntegrityError):
                capture.verify_offline(root, ledger_path=None)

    def test_expected_anchor_drift_fails(self) -> None:
        source = {
            "id": "issue-9",
            "kind": "issue",
            "number": 9,
            "expected": {
                "updated_at": "wrong",
                "issue_comment_ids": [11],
                "review_ids": [],
                "review_comment_ids": [],
            },
        }
        with self.assertRaises(capture.DriftError):
            capture.capture_once(
                FakeClient(issue_resource(), {"/comments": [comment()]}),
                "OpenApoc/OpenApoc",
                source,
            )

    def test_capture_refuses_existing_output_before_network(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            with self.assertRaises(capture.SpecError):
                capture.capture_to(Path(name), client=object())  # type: ignore[arg-type]

    def test_main_exit_classes_are_stable(self) -> None:
        self.assertEqual(capture.main([]), 2)
        with tempfile.TemporaryDirectory() as name:
            self.assertEqual(capture.main(["capture", "--out", name]), 2)


if __name__ == "__main__":
    unittest.main()
