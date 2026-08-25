#!/usr/bin/env python3
"""Refresh live sections of docs/original-game/compare-report.html.

Static lab census (function export, table audit, rebase CSVs) is left in place.
This rewrites gap rows, TODO/FIXME counts, tick constants, and the priority list
from in-repo sources. Use --check to fail if the committed HTML is stale.
"""
from __future__ import annotations

import argparse
import html
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "docs" / "original-game" / "compare-report.html"
MATRIX = ROOT / "docs" / "original-game" / "openapoc-gap-matrix.md"
NEXT_IMPL = ROOT / "docs" / "original-game" / "next-implementation.md"

BEGIN = "<!-- regen:{name} -->"
END = "<!-- /regen:{name} -->"


def replace_region(text: str, name: str, inner: str) -> str:
    start = BEGIN.format(name=name)
    stop = END.format(name=name)
    pattern = re.compile(re.escape(start) + r".*?" + re.escape(stop), re.S)
    if not pattern.search(text):
        raise SystemExit(f"missing regen markers for {name}")
    return pattern.sub(start + "\n" + inner.rstrip() + "\n" + stop, text)


def parse_gap_rows() -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    domain = ""
    for raw in MATRIX.read_text().splitlines():
        if raw.startswith("## City"):
            domain = "city"
            continue
        if raw.startswith("## Battle"):
            domain = "battle"
            continue
        if not raw.startswith("|") or raw.startswith("| ---") or raw.startswith("| OG") or raw.startswith("| Asked"):
            continue
        if "OG system" in raw or "Checked" in raw:
            continue
        cells = [c.strip() for c in raw.strip("|").split("|")]
        if len(cells) < 7 or not domain:
            continue
        status = re.sub(r"\s+", " ", cells[5])
        status_key = status.split()[0].split("/")[0]
        conf = cells[6].split()[0] if cells[6] else ""
        path = re.sub(r"`", "", cells[3])
        path = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", path)
        path = path.split(",")[0].strip()
        rows.append(
            {
                "domain": domain,
                "system": cells[0],
                "binary": cells[1],
                "evidence": re.sub(r"`([^`]*)`", r"\1", cells[2]),
                "path": path,
                "issue": cells[4],
                "status": status_key,
                "status_full": status,
                "conf": conf,
            }
        )
    return rows


def pill(kind: str, label: str) -> str:
    cls = {"implemented": "ok", "high": "ok", "approximate": "warn", "partial": "warn", "medium": "warn", "missing": "bad", "wrong": "bad", "low": "muted"}.get(kind, "muted")
    return f'<span class="pill {cls}">{html.escape(label)}</span>'


def gap_tbody(rows: list[dict[str, str]]) -> str:
    parts = []
    for r in rows:
        parts.append(
            "<tr>"
            f"<td>{html.escape(r['domain'])}</td>"
            f"<td>{html.escape(r['system'])}</td>"
            f"<td>{html.escape(r['binary'])}</td>"
            f"<td>{html.escape(r['evidence'])}</td>"
            f"<td><code>{html.escape(r['path'])}</code></td>"
            f"<td>{html.escape(r['issue'])}</td>"
            f"<td>{pill(r['status'], r['status'])}</td>"
            f"<td>{pill(r['conf'], r['conf'])}</td>"
            "</tr>"
        )
    return "".join(parts)


def gap_mix(rows: list[dict[str, str]]) -> str:
    counts = Counter(r["status"] for r in rows)
    bits = [f"{k} {counts[k]}" for k in sorted(counts)]
    return f'    <p class="muted">Status mix:\n    {", ".join(bits)}.</p>'


def todo_counts() -> tuple[int, list[tuple[str, int]], list[tuple[str, int]]]:
    dirs: Counter[str] = Counter()
    files: Counter[str] = Counter()
    total = 0
    for folder in (ROOT / "game" / "state", ROOT / "game" / "ui"):
        for path in folder.rglob("*"):
            if path.suffix not in {".cpp", ".h"}:
                continue
            if "generated" in path.name:
                continue
            text = path.read_text(errors="ignore")
            n = len(re.findall(r"\bTODO\b|\bFIXME\b", text))
            if not n:
                continue
            total += n
            rel = str(path.relative_to(ROOT))
            files[rel] += n
            parent = str(path.parent.relative_to(ROOT))
            dirs[parent] += n
    return total, dirs.most_common(8), files.most_common(15)


def todo_html(total: int, dirs: list[tuple[str, int]], files: list[tuple[str, int]]) -> tuple[str, str]:
    dir_rows = []
    for name, count in dirs:
        pct = (100.0 * count / total) if total else 0
        dir_rows.append(
            f"<tr><td>{html.escape(name)}</td><td>{count}</td>"
            f'<td><div class="bar" role="img" aria-label="{pct:.0f} percent">'
            f'<span class="bar-fill warn" style="width:{pct:.1f}%"></span></div></td></tr>'
        )
    file_rows = [
        f"<tr><td><code>{html.escape(name)}</code></td><td>{count}</td></tr>" for name, count in files
    ]
    return "".join(dir_rows), "".join(file_rows)


def read_constant(symbol: str, rel: str) -> tuple[str, int]:
    text = (ROOT / rel).read_text()
    patterns = [
        rf"(?:static constexpr unsigned|static const unsigned|static const int)\s+{symbol}\s*=\s*([^;]+);",
        rf"#define\s+{symbol}\s+(\S+)",
    ]
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            return match.group(1).strip(), text[: match.start()].count("\n") + 1
    return "?", 1


def strip_md(text: str) -> str:
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    return text.replace("`", "")


def constants_tbody() -> str:
    specs = [
        ("VANILLA_TICKS_PER_SECOND", "game/state/gametime.h", "Original city/battle sim rate"),
        ("TICKS_MULTIPLIER", "game/state/gametime.h", "OpenApoc runs 4× vanilla"),
        ("TICKS_PER_SECOND", "game/state/gametime.h", "36 × multiplier — contaminates rates"),
        ("TICKS_PER_TURN", "game/state/battle/battle.h", "TPS × 4"),
        ("TICKS_PER_WOUND_EFFECT", "game/state/battle/battleunit.h", "FIXME: ensure vanilla"),
        ("TICKS_PER_ENZYME_EFFECT", "game/state/battle/battleunit.h", "TPS/9 — unverified"),
        ("TICKS_PER_FIRE_EFFECT", "game/state/battle/battleunit.h", "Unverified vs original"),
        ("HAZARD_SPREAD_CHANCE", "game/state/battle/battlehazard.h", "Explicitly made up"),
        ("FUEL_TICKS_PER_SECOND", "game/state/city/vehicle.h", "Tracks TICKS_PER_SECOND"),
    ]
    rows = []
    for symbol, rel, note in specs:
        value, line = read_constant(symbol, rel)
        rows.append(
            f"<tr><td><code>{symbol}</code></td><td>{html.escape(value)}</td>"
            f"<td><code>{html.escape(rel)}:{line}</code></td>"
            f"<td>{html.escape(note)}</td></tr>"
        )
    return "".join(rows)


def priority_ol() -> str:
    items = []
    for raw in NEXT_IMPL.read_text().splitlines():
        m = re.match(r"^(\d+)\.\s+\*\*(.+?)\*\*\s+[—-]\s+(.+)$", raw)
        if not m:
            continue
        title = strip_md(m.group(2))
        body = strip_md(m.group(3))
        items.append(f"    <li><strong>{html.escape(title)}</strong> — {html.escape(body)}</li>")
    return "<ol>\n" + "\n".join(items) + "\n  </ol>"


def render(text: str) -> str:
    rows = parse_gap_rows()
    total, dirs, files = todo_counts()
    dir_html, file_html = todo_html(total, dirs, files)
    text = replace_region(text, "kpi-gaps", f"      <div class=\"kpi\"><b>{len(rows)}</b><span>gameplay gap rows</span></div>")
    text = replace_region(text, "kpi-todos", f"      <div class=\"kpi\"><b>{total}</b><span>TODO+FIXME in game/state and game/ui</span></div>")
    text = replace_region(text, "gap-mix", gap_mix(rows))
    text = replace_region(text, "gap-tbody", gap_tbody(rows))
    text = replace_region(text, "todo-dirs", dir_html)
    text = replace_region(text, "todo-files", file_html)
    text = replace_region(text, "const-tbody", constants_tbody())
    text = replace_region(text, "priority-ol", priority_ol())
    text = re.sub(r"Generated 20\d\d-\d\d-\d\d\.", "Generated from in-repo sources.", text)
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    original = REPORT.read_text()
    updated = render(original)
    if args.check:
        if updated != original:
            print("compare-report.html is stale; run tools/regen_compare_report.py", file=sys.stderr)
            return 1
        print("compare-report.html is current")
        return 0
    REPORT.write_text(updated)
    print(f"wrote {REPORT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
