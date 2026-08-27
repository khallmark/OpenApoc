#!/usr/bin/env python3
"""Capture and verify canonical snapshots of reviewed GitHub REST inputs.

The allowlist is committed in docs/timing/source-snapshots/sources.json.  The tool deliberately
offers no repository, endpoint, or source-number override: widening the evidence set is a reviewed
source change, not a command-line convenience.  This REST projection records current resource
state and ``updated_at`` anchors; it does not claim to preserve GraphQL edit history.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT_BASE = ROOT / "docs/timing/source-snapshots"
SPEC_PATH = SNAPSHOT_BASE / "sources.json"
SCHEMA_PATH = SNAPSHOT_BASE / "source-snapshot-v1.schema.json"
COMMITTED_ROOT = SNAPSHOT_BASE / "v1"
LEDGER_PATH = ROOT / "docs/timing/source-disposition.md"
API_ROOT = "https://api.github.com"
API_VERSION = "2022-11-28"
PROJECTION_ID = "openapoc-github-source-projection-v1"
CANONICALIZATION_ID = "openapoc-json-c14n-v1"
MAX_PAGES = 100
MAX_ATTACHMENT_BYTES = 256 * 1024 * 1024
ALLOWED_ATTACHMENT_HOSTS = {
    "github.com",
    "objects.githubusercontent.com",
    "github-releases.githubusercontent.com",
}


class CaptureError(RuntimeError):
    exit_code = 5


class SpecError(CaptureError):
    exit_code = 2


class IntegrityError(CaptureError):
    exit_code = 3


class DriftError(CaptureError):
    exit_code = 4


class TransportError(CaptureError):
    exit_code = 5


class _SchemaValidationError(ValueError):
    pass


def _reject_float(value: Any) -> None:
    if isinstance(value, float):
        raise SpecError("canonical source data may not contain floating-point values")
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str):
                raise SpecError("canonical object keys must be strings")
            _reject_float(child)
    elif isinstance(value, list):
        for child in value:
            _reject_float(child)


def canonical_bytes(value: Any) -> bytes:
    """The repository's restricted JSON canonical form.

    Captured projections contain no numbers that require RFC-8785 floating-point normalization, so
    sorted compact UTF-8 JSON plus one LF is a complete deterministic encoding for this schema.
    """

    _reject_float(value)
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise IntegrityError(f"cannot read canonical JSON {path}: {exc}") from exc


def _schema_ref(root_schema: dict[str, Any], reference: str) -> dict[str, Any]:
    if not reference.startswith("#/"):
        raise _SchemaValidationError(f"unsupported schema reference {reference}")
    value: Any = root_schema
    for component in reference[2:].split("/"):
        component = component.replace("~1", "/").replace("~0", "~")
        if not isinstance(value, dict) or component not in value:
            raise _SchemaValidationError(f"unresolved schema reference {reference}")
        value = value[component]
    if not isinstance(value, dict):
        raise _SchemaValidationError(f"schema reference is not an object: {reference}")
    return value


def _schema_type_matches(value: Any, expected: str) -> bool:
    return {
        "array": isinstance(value, list),
        "boolean": isinstance(value, bool),
        "integer": isinstance(value, int) and not isinstance(value, bool),
        "null": value is None,
        "object": isinstance(value, dict),
        "string": isinstance(value, str),
    }.get(expected, False)


def _validate_schema_node(
    value: Any, schema: dict[str, Any], root_schema: dict[str, Any], path: str
) -> None:
    if "$ref" in schema:
        _validate_schema_node(value, _schema_ref(root_schema, schema["$ref"]), root_schema, path)
        return
    if "oneOf" in schema:
        successes = 0
        for candidate in schema["oneOf"]:
            try:
                _validate_schema_node(value, candidate, root_schema, path)
                successes += 1
            except _SchemaValidationError:
                pass
        if successes != 1:
            raise _SchemaValidationError(f"{path} matched {successes} oneOf branches")
        return
    if "const" in schema and value != schema["const"]:
        raise _SchemaValidationError(f"{path} does not equal the required constant")
    if "enum" in schema and value not in schema["enum"]:
        raise _SchemaValidationError(f"{path} is not in the closed enumeration")
    expected_type = schema.get("type")
    if expected_type is not None and not _schema_type_matches(value, expected_type):
        raise _SchemaValidationError(f"{path} is not a {expected_type}")
    if isinstance(value, dict):
        required = set(schema.get("required", []))
        missing = sorted(required - set(value))
        if missing:
            raise _SchemaValidationError(f"{path} is missing {', '.join(missing)}")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            extra = sorted(set(value) - set(properties))
            if extra:
                raise _SchemaValidationError(f"{path} has unknown fields {', '.join(extra)}")
        for key, child in value.items():
            if key in properties:
                _validate_schema_node(child, properties[key], root_schema, f"{path}.{key}")
    elif isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            raise _SchemaValidationError(f"{path} has too few items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            raise _SchemaValidationError(f"{path} has too many items")
        if "items" in schema:
            for index, child in enumerate(value):
                _validate_schema_node(child, schema["items"], root_schema, f"{path}[{index}]")
    elif isinstance(value, str) and "pattern" in schema:
        if re.search(schema["pattern"], value) is None:
            raise _SchemaValidationError(f"{path} does not match its required pattern")
    elif isinstance(value, int) and not isinstance(value, bool) and "minimum" in schema:
        if value < schema["minimum"]:
            raise _SchemaValidationError(f"{path} is below its minimum")


def validate_snapshot(value: Any) -> None:
    schema = load_json(SCHEMA_PATH)
    if not isinstance(schema, dict):
        raise IntegrityError("source snapshot schema is not an object")
    try:
        _validate_schema_node(value, schema, schema, "$")
    except _SchemaValidationError as exc:
        raise IntegrityError(f"source snapshot schema violation: {exc}") from exc


def _required(obj: dict[str, Any], *fields: str) -> list[Any]:
    missing = [field for field in fields if field not in obj]
    if missing:
        raise TransportError(f"GitHub response omitted required fields: {', '.join(missing)}")
    return [obj[field] for field in fields]


def _user(value: Any) -> Any:
    if value is None:
        return None
    _required(value, "login", "id", "node_id")
    return {key: value[key] for key in ("login", "id", "node_id")}


def _labels(values: list[dict[str, Any]]) -> list[dict[str, Any]]:
    result = []
    for value in values:
        _required(value, "id", "node_id", "name", "color", "description")
        result.append({key: value[key] for key in ("id", "node_id", "name", "color", "description")})
    return sorted(result, key=lambda item: (item["name"], item["id"]))


def _milestone(value: Any) -> Any:
    if value is None:
        return None
    _required(value, "id", "node_id", "number", "title", "description", "state", "due_on")
    return {
        key: value[key]
        for key in ("id", "node_id", "number", "title", "description", "state", "due_on")
    }


def _accounts(values: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return sorted((_user(value) for value in values), key=lambda item: (item["login"], item["id"]))


def normalize_resource(raw: dict[str, Any], kind: str) -> dict[str, Any]:
    common = (
        "id", "node_id", "number", "state", "title", "body", "created_at",
        "updated_at", "closed_at", "html_url", "author_association", "locked", "comments",
    )
    _required(raw, *common, "user", "labels", "milestone", "assignees")
    result = {key: raw[key] for key in common}
    result.update(
        {
            "user": _user(raw["user"]),
            "labels": _labels(raw["labels"]),
            "milestone": _milestone(raw["milestone"]),
            "assignees": _accounts(raw["assignees"]),
        }
    )
    if kind == "pr":
        _required(
            raw,
            "merged_at", "merge_commit_sha", "draft", "review_comments", "commits",
            "additions", "deletions", "changed_files", "requested_reviewers", "base", "head",
        )
        result.update(
            {
                key: raw[key]
                for key in (
                    "merged_at", "merge_commit_sha", "draft", "review_comments", "commits",
                    "additions", "deletions", "changed_files",
                )
            }
        )
        result["requested_reviewers"] = _accounts(raw["requested_reviewers"])
        for side in ("base", "head"):
            value = raw[side]
            _required(value, "ref", "sha", "repo")
            _required(value["repo"], "id", "node_id", "full_name")
            result[side] = {
                "ref": value["ref"],
                "sha": value["sha"],
                "repo": {
                    key: value["repo"][key] for key in ("id", "node_id", "full_name")
                },
            }
    else:
        _required(raw, "state_reason")
        result["state_reason"] = raw["state_reason"]
    return result


def normalize_issue_comment(raw: dict[str, Any]) -> dict[str, Any]:
    fields = (
        "id", "node_id", "body", "created_at", "updated_at", "html_url", "author_association",
    )
    _required(raw, *fields, "user")
    result = {key: raw[key] for key in fields}
    result["user"] = _user(raw["user"])
    return result


def normalize_review(raw: dict[str, Any]) -> dict[str, Any]:
    fields = (
        "id", "node_id", "state", "body", "commit_id", "submitted_at", "html_url",
        "author_association",
    )
    _required(raw, *fields, "user")
    result = {key: raw[key] for key in fields}
    result["user"] = _user(raw["user"])
    return result


def normalize_review_comment(raw: dict[str, Any]) -> dict[str, Any]:
    fields = (
        "id", "node_id", "pull_request_review_id", "in_reply_to_id", "body", "created_at",
        "updated_at", "html_url", "author_association", "path", "line", "side", "start_line",
        "start_side", "original_line", "original_start_line", "commit_id", "original_commit_id",
        "diff_hunk", "subject_type",
    )
    _required(raw, *fields, "user")
    result = {key: raw[key] for key in fields}
    result["user"] = _user(raw["user"])
    return result


def normalize_commit(raw: dict[str, Any]) -> dict[str, Any]:
    _required(raw, "sha", "parents", "commit", "author", "committer")
    commit = raw["commit"]
    _required(commit, "tree", "author", "committer", "message")
    _required(commit["tree"], "sha")
    for role in ("author", "committer"):
        _required(commit[role], "name", "email", "date")
    return {
        "sha": raw["sha"],
        "parent_shas": sorted(parent["sha"] for parent in raw["parents"]),
        "tree_sha": commit["tree"]["sha"],
        "author": {key: commit["author"][key] for key in ("name", "email", "date")},
        "committer": {key: commit["committer"][key] for key in ("name", "email", "date")},
        "author_account": _user(raw["author"]),
        "committer_account": _user(raw["committer"]),
        "message": commit["message"],
    }


def normalize_file(raw: dict[str, Any]) -> dict[str, Any]:
    fields = (
        "sha", "filename", "status", "additions", "deletions", "changes", "blob_url",
        "raw_url", "contents_url", "patch",
    )
    _required(raw, *fields)
    if not isinstance(raw["patch"], str):
        raise TransportError(f"GitHub omitted patch for {raw.get('filename', '<unknown>')}")
    result = {key: raw[key] for key in fields}
    result["previous_filename"] = raw.get("previous_filename")
    return result


def parse_next_link(current_url: str, link_header: str | None, page: int) -> str | None:
    if not link_header:
        return None
    matches = re.findall(r'<([^>]+)>;\s*rel="([^"]+)"', link_header)
    rels = {rel: url for url, rel in matches}
    if "next" not in rels:
        return None
    next_url = rels["next"]
    current = urllib.parse.urlparse(current_url)
    nxt = urllib.parse.urlparse(next_url)
    if nxt.scheme != "https" or nxt.netloc != "api.github.com":
        raise TransportError("pagination next link left https://api.github.com")
    if nxt.path != current.path:
        raise TransportError("pagination next link changed endpoint")
    query = urllib.parse.parse_qs(nxt.query)
    try:
        next_page = int(query["page"][0])
        per_page = int(query["per_page"][0])
    except (KeyError, ValueError, IndexError) as exc:
        raise TransportError("pagination next link lacks canonical page/per_page") from exc
    if next_page != page + 1 or per_page != 100:
        raise TransportError("pagination next link skipped/repeated a page or changed page size")
    return next_url


@dataclass(frozen=True)
class Response:
    status: int
    headers: dict[str, str]
    body: bytes
    final_url: str


Transport = Callable[[str], Response]


class GitHubClient:
    def __init__(self, transport: Transport | None = None):
        self.transport = transport or self._request
        self._token: str | None = None

    def _auth_token(self) -> str:
        if self._token is not None:
            return self._token
        token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
        if not token:
            try:
                token = subprocess.run(
                    ["gh", "auth", "token"], check=True, capture_output=True, text=True
                ).stdout.strip()
            except (OSError, subprocess.CalledProcessError) as exc:
                raise TransportError("GitHub authentication unavailable") from exc
        if not token:
            raise TransportError("GitHub authentication returned an empty token")
        self._token = token
        return token

    def _request(self, url: str) -> Response:
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != "https" or parsed.netloc != "api.github.com":
            raise TransportError("API request left https://api.github.com")
        request = urllib.request.Request(
            url,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self._auth_token()}",
                "User-Agent": "OpenApoc-timing-source-capture-v1",
                "X-GitHub-Api-Version": API_VERSION,
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return Response(
                    response.status,
                    {key.lower(): value for key, value in response.headers.items()},
                    response.read(),
                    response.geturl(),
                )
        except (urllib.error.URLError, TimeoutError) as exc:
            raise TransportError("GitHub API transport failed without exposing credentials") from exc

    def one(self, endpoint: str) -> dict[str, Any]:
        url = f"{API_ROOT}{endpoint}"
        response = self.transport(url)
        if response.status != 200 or response.final_url != url:
            raise TransportError(f"GitHub API request failed or redirected: {endpoint}")
        try:
            value = json.loads(response.body)
        except json.JSONDecodeError as exc:
            raise TransportError(f"GitHub API returned malformed JSON: {endpoint}") from exc
        if not isinstance(value, dict):
            raise TransportError(f"GitHub resource was not an object: {endpoint}")
        return value

    def collection(self, endpoint: str) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        page = 1
        url = f"{API_ROOT}{endpoint}?per_page=100&page=1"
        seen = set()
        while url:
            if page > MAX_PAGES or url in seen:
                raise TransportError("pagination exceeded its bound or formed a cycle")
            seen.add(url)
            response = self.transport(url)
            if response.status != 200 or response.final_url != url:
                raise TransportError(f"GitHub collection failed or redirected: {endpoint}")
            try:
                values = json.loads(response.body)
            except json.JSONDecodeError as exc:
                raise TransportError(f"GitHub collection returned malformed JSON: {endpoint}") from exc
            if not isinstance(values, list) or any(not isinstance(item, dict) for item in values):
                raise TransportError(f"GitHub collection was not an object array: {endpoint}")
            result.extend(values)
            url = parse_next_link(url, response.headers.get("link"), page)
            page += 1
        return result


def _ids(values: list[dict[str, Any]]) -> list[int]:
    ids = [int(value["id"]) for value in values]
    if len(ids) != len(set(ids)):
        raise TransportError("GitHub collection contains duplicate IDs")
    return sorted(ids)


def _assert_expected(source: dict[str, Any], captured: dict[str, Any]) -> None:
    expected = source["expected"]
    resource = captured["resource"]
    checks = {
        "updated_at": resource["updated_at"],
        "issue_comment_ids": _ids(captured["issue_comments"]),
        "review_ids": _ids(captured["reviews"]),
        "review_comment_ids": _ids(captured["review_comments"]),
    }
    if source["kind"] == "pr":
        checks.update(
            {
                "head_sha": resource["head"]["sha"],
                "base_sha": resource["base"]["sha"],
                "commit_count": len(captured["commits"]),
                "changed_file_count": len(captured["files"]),
            }
        )
        if resource["commits"] != len(captured["commits"]):
            raise DriftError(f"{source['id']} commit count disagrees with resource")
        if resource["changed_files"] != len(captured["files"]):
            raise DriftError(f"{source['id']} file count disagrees with resource")
        if resource["review_comments"] != len(captured["review_comments"]):
            raise DriftError(f"{source['id']} inline-review count disagrees with resource")
    if resource["comments"] != len(captured["issue_comments"]):
        raise DriftError(f"{source['id']} issue-comment count disagrees with resource")
    for key, actual in checks.items():
        if expected.get(key) != actual:
            raise DriftError(f"{source['id']} drifted at {key}")


def capture_once(client: GitHubClient, repository: str, source: dict[str, Any]) -> dict[str, Any]:
    number = int(source["number"])
    if source["kind"] == "pr":
        resource_endpoint = f"/repos/{repository}/pulls/{number}"
    else:
        resource_endpoint = f"/repos/{repository}/issues/{number}"
    before = normalize_resource(client.one(resource_endpoint), source["kind"])
    issue_comments = sorted(
        (
            normalize_issue_comment(item)
            for item in client.collection(f"/repos/{repository}/issues/{number}/comments")
        ),
        key=lambda item: item["id"],
    )
    reviews: list[dict[str, Any]] = []
    review_comments: list[dict[str, Any]] = []
    commits: list[dict[str, Any]] = []
    files: list[dict[str, Any]] = []
    if source["kind"] == "pr":
        reviews = sorted(
            (
                normalize_review(item)
                for item in client.collection(f"/repos/{repository}/pulls/{number}/reviews")
            ),
            key=lambda item: item["id"],
        )
        review_comments = sorted(
            (
                normalize_review_comment(item)
                for item in client.collection(f"/repos/{repository}/pulls/{number}/comments")
            ),
            key=lambda item: item["id"],
        )
        commits = sorted(
            (
                normalize_commit(item)
                for item in client.collection(f"/repos/{repository}/pulls/{number}/commits")
            ),
            key=lambda item: item["sha"],
        )
        files = sorted(
            (
                normalize_file(item)
                for item in client.collection(f"/repos/{repository}/pulls/{number}/files")
            ),
            key=lambda item: (item["filename"], item["sha"]),
        )
        if len(files) >= 3000:
            raise TransportError("pull-request file capture reached GitHub's 3000-file limit")
    after = normalize_resource(client.one(resource_endpoint), source["kind"])
    if canonical_bytes(before) != canonical_bytes(after):
        raise DriftError(f"{source['id']} changed during capture")
    captured = {
        "schema": "openapoc.github_source_snapshot.v1",
        "projection_id": PROJECTION_ID,
        "repository": repository,
        "source_id": source["id"],
        "kind": source["kind"],
        "number": number,
        "resource": before,
        "issue_comments": issue_comments,
        "reviews": reviews,
        "review_comments": review_comments,
        "commits": commits,
        "files": files,
    }
    validate_snapshot(captured)
    _assert_expected(source, captured)
    return captured


def capture_corpus_stable(
    client: GitHubClient,
    repository: str,
    sources: list[dict[str, Any]],
    capture_fn: Callable[[GitHubClient, str, dict[str, Any]], dict[str, Any]] = capture_once,
) -> dict[str, dict[str, Any]]:
    """Capture the complete reviewed corpus twice, then compare the two complete maps."""

    def complete_pass() -> dict[str, dict[str, Any]]:
        return {source["id"]: capture_fn(client, repository, source) for source in sources}

    first = complete_pass()
    second = complete_pass()
    if canonical_bytes(first) != canonical_bytes(second):
        raise DriftError("reviewed GitHub source corpus changed between complete capture passes")
    return first


def load_spec() -> dict[str, Any]:
    spec = load_json(SPEC_PATH)
    _required(
        spec,
        "schema",
        "repository",
        "sources",
        "attachments",
        "mutable_non_authoritative_locators",
        "unavailable_locators",
    )
    if spec["schema"] != "openapoc.timing_source_capture_spec.v1":
        raise SpecError("unknown timing-source capture specification")
    if spec["repository"] != "OpenApoc/OpenApoc":
        raise SpecError("capture repository is not the reviewed OpenApoc/OpenApoc authority")
    ids = [source["id"] for source in spec["sources"]]
    if len(ids) != 6 or len(set(ids)) != 6:
        raise SpecError("capture specification must contain exactly six unique sources")
    locators = spec["mutable_non_authoritative_locators"]
    if not isinstance(locators, list) or len(locators) != 2:
        raise SpecError("capture specification must classify exactly two mutable video locators")
    locator_ids = []
    for locator in locators:
        if not isinstance(locator, dict):
            raise SpecError("mutable locator entries must be objects")
        _required(locator, "id", "url", "disposition")
        locator_ids.append(locator["id"])
        parsed = urllib.parse.urlparse(locator["url"])
        if parsed.scheme != "https" or parsed.hostname not in {"youtu.be", "www.youtube.com"}:
            raise SpecError(f"mutable video locator is not an approved YouTube URL: {locator['id']}")
        if locator["disposition"] != "mutable_locator_only_cannot_authorize_code_or_parity":
            raise SpecError(f"mutable video locator has an authoritative disposition: {locator['id']}")
    if len(locator_ids) != len(set(locator_ids)):
        raise SpecError("mutable video locator IDs must be unique")
    return spec


def build_manifest(source_files: list[tuple[str, str, str]]) -> dict[str, Any]:
    spec = load_spec()
    return {
        "schema": "openapoc.github_source_snapshot_manifest.v1",
        "api_version": API_VERSION,
        "projection_id": PROJECTION_ID,
        "canonicalization_id": CANONICALIZATION_ID,
        "repository": "OpenApoc/OpenApoc",
        "capture_script": {
            "path": "tools/capture_timing_sources.py",
            "sha256": sha256_file(Path(__file__)),
        },
        "source_spec": {
            "path": "docs/timing/source-snapshots/sources.json",
            "sha256": sha256_file(SPEC_PATH),
        },
        "source_schema": {
            "path": "docs/timing/source-snapshots/source-snapshot-v1.schema.json",
            "sha256": sha256_file(SCHEMA_PATH),
        },
        "sources": [
            {"id": source_id, "path": filename, "sha256": digest}
            for source_id, filename, digest in source_files
        ],
        "attachments": [
            {
                "id": item["id"],
                "url": item["url"],
                "sha256": item["sha256"],
                "disposition": item["disposition"],
            }
            for item in spec["attachments"]
        ],
        "mutable_non_authoritative_locators": spec["mutable_non_authoritative_locators"],
        "unavailable_locators": spec["unavailable_locators"],
    }


class _SafeAttachmentRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):  # type: ignore[override]
        parsed = urllib.parse.urlparse(newurl)
        if parsed.scheme != "https" or parsed.hostname not in ALLOWED_ATTACHMENT_HOSTS:
            raise TransportError("attachment redirect left the reviewed HTTPS host allowlist")
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def verify_attachments(spec: dict[str, Any]) -> None:
    opener = urllib.request.build_opener(_SafeAttachmentRedirect())
    with tempfile.TemporaryDirectory(prefix="openapoc-timing-attachments-") as temp_name:
        temp_root = Path(temp_name)
        os.chmod(temp_root, 0o700)
        for attachment in spec["attachments"]:
            parsed = urllib.parse.urlparse(attachment["url"])
            if parsed.scheme != "https" or parsed.hostname not in ALLOWED_ATTACHMENT_HOSTS:
                raise SpecError(f"attachment locator is outside the reviewed allowlist: {attachment['id']}")
            digest = hashlib.sha256()
            size = 0
            target = temp_root / f"{attachment['id']}.opaque"
            request = urllib.request.Request(
                attachment["url"], headers={"User-Agent": "OpenApoc-timing-source-capture-v1"}
            )
            try:
                with opener.open(request, timeout=120) as response, target.open("xb") as output:
                    final = urllib.parse.urlparse(response.geturl())
                    if final.scheme != "https" or final.hostname not in ALLOWED_ATTACHMENT_HOSTS:
                        raise TransportError("attachment final URL left the reviewed allowlist")
                    while True:
                        chunk = response.read(1024 * 1024)
                        if not chunk:
                            break
                        size += len(chunk)
                        if size > MAX_ATTACHMENT_BYTES:
                            raise TransportError(f"attachment exceeded size cap: {attachment['id']}")
                        digest.update(chunk)
                        output.write(chunk)
            except (OSError, urllib.error.URLError, TimeoutError) as exc:
                raise TransportError(
                    f"attachment transport failed without exposing credentials: {attachment['id']}"
                ) from exc
            if digest.hexdigest() != attachment["sha256"]:
                raise DriftError(f"attachment digest drift: {attachment['id']}")
            target.unlink()


def capture_to(out: Path, client: GitHubClient | None = None) -> None:
    if out.exists():
        raise SpecError(f"capture output already exists: {out}")
    spec = load_spec()
    verify_attachments(spec)
    out.mkdir(parents=True, mode=0o755)
    source_files = []
    client = client or GitHubClient()
    try:
        snapshots = capture_corpus_stable(client, spec["repository"], spec["sources"])
        for source in spec["sources"]:
            snapshot = snapshots[source["id"]]
            filename = f"{source['id']}.json"
            data = canonical_bytes(snapshot)
            (out / filename).write_bytes(data)
            source_files.append((source["id"], filename, sha256_bytes(data)))
        (out / "manifest.json").write_bytes(canonical_bytes(build_manifest(source_files)))
    except Exception:
        shutil.rmtree(out, ignore_errors=True)
        raise


def verify_offline(root: Path = COMMITTED_ROOT, ledger_path: Path | None = LEDGER_PATH) -> None:
    manifest_path = root / "manifest.json"
    manifest = load_json(manifest_path)
    if canonical_bytes(manifest) != manifest_path.read_bytes():
        raise IntegrityError("manifest is not canonical openapoc-json-c14n-v1 bytes")
    spec = load_spec()
    expected_ids = [source["id"] for source in spec["sources"]]
    expected_paths = [f"{source_id}.json" for source_id in expected_ids]
    if [entry.get("id") for entry in manifest.get("sources", [])] != expected_ids:
        raise IntegrityError("manifest source IDs are not the exact committed allowlist")
    if [entry.get("path") for entry in manifest["sources"]] != expected_paths:
        raise IntegrityError("manifest source paths are not the exact committed allowlist")
    present_json = sorted(path.name for path in root.glob("*.json") if path.name != "manifest.json")
    if present_json != sorted(expected_paths):
        raise IntegrityError("snapshot directory has missing or extra source payloads")
    actual_entries = []
    for source_id, filename in zip(expected_ids, expected_paths):
        actual_entries.append((source_id, filename, sha256_file(root / filename)))
    expected_manifest = build_manifest(actual_entries)
    if canonical_bytes(expected_manifest) != manifest_path.read_bytes():
        raise IntegrityError("manifest authority/tool/spec/schema digest drift")
    for entry in manifest["sources"]:
        path = root / entry["path"]
        data = path.read_bytes()
        value = json.loads(data)
        validate_snapshot(value)
        if canonical_bytes(value) != data:
            raise IntegrityError(f"snapshot is not canonical: {entry['path']}")
        if sha256_bytes(data) != entry["sha256"]:
            raise IntegrityError(f"snapshot digest mismatch: {entry['path']}")
        if value["source_id"] != entry["id"]:
            raise IntegrityError(f"snapshot source ID mismatch: {entry['path']}")
        source = next(source for source in spec["sources"] if source["id"] == entry["id"])
        if value["kind"] != source["kind"] or value["number"] != source["number"]:
            raise IntegrityError(f"snapshot source identity mismatch: {entry['path']}")
        _assert_expected(source, value)
        expected_root_fields = {
            "schema", "projection_id", "repository", "source_id", "kind", "number", "resource",
            "issue_comments", "reviews", "review_comments", "commits", "files",
        }
        if set(value) != expected_root_fields:
            raise IntegrityError(f"snapshot root schema mismatch: {entry['path']}")
    if ledger_path is None:
        return
    ledger = ledger_path.read_text(encoding="utf-8")
    manifest_digest = sha256_file(manifest_path)
    if f"source-snapshot-v1 manifest `sha256:{manifest_digest}`" not in ledger:
        raise IntegrityError("source ledger does not bind the canonical snapshot manifest")
    for entry in manifest["sources"]:
        marker = f"`{entry['path']}` `sha256:{entry['sha256']}`"
        if marker not in ledger:
            raise IntegrityError(f"source ledger does not bind {entry['path']}")


def verify_live(client: GitHubClient | None = None) -> None:
    verify_offline()
    spec = load_spec()
    verify_attachments(spec)
    client = client or GitHubClient()
    live_corpus = capture_corpus_stable(client, spec["repository"], spec["sources"])
    for source in spec["sources"]:
        live = canonical_bytes(live_corpus[source["id"]])
        committed = (COMMITTED_ROOT / f"{source['id']}.json").read_bytes()
        if live != committed:
            raise DriftError(f"live GitHub source drift: {source['id']}")


def usage() -> None:
    print(
        "usage: capture_timing_sources.py "
        "{verify-offline|verify-live|capture --out NONEXISTENT_DIRECTORY}",
        file=sys.stderr,
    )


def main(argv: list[str]) -> int:
    try:
        if argv == ["verify-offline"]:
            verify_offline()
        elif argv == ["verify-live"]:
            verify_live()
        elif len(argv) == 3 and argv[0] == "capture" and argv[1] == "--out":
            capture_to(Path(argv[2]).resolve())
        else:
            usage()
            return 2
        return 0
    except CaptureError as exc:
        print(f"timing-source capture failed: {exc}", file=sys.stderr)
        return exc.exit_code
    except (OSError, KeyError, TypeError, ValueError) as exc:
        print(f"timing-source capture failed closed: {exc}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
