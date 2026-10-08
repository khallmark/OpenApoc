#!/usr/bin/env python3
"""Fail if a serialized C++ enum has a value game/state/gamestate_serialize.xml does not list.

The generated serializer maps enum values to names using only the values listed in the XML. A
value missing there is written to a save as "" and makes that save unloadable: AgentMission's
InvestigateBuilding was missing for years, so any game saved while an agent investigated a
building could never be loaded again (the loader then segfaulted in initState).

Usage: python3 tools/check_serialize_enums.py  (exit 1 and list the gaps if any)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def cpp_enum_values(qualified: str, headers: dict[Path, str]) -> list[str] | None:
    """Values of `Owner::Name` (or a namespace-level `Name`), or None if not found."""
    parts = qualified.split("::")
    name, owner = parts[-1], (parts[-2] if len(parts) > 1 else None)
    for text in headers.values():
        for m in re.finditer(r"enum\s+(?:class\s+)?" + re.escape(name) + r"\b[^{;]*\{(.*?)\}",
                             text, re.S):
            if owner:
                # The nearest enclosing class/struct declared before the enum must be the owner.
                decls = re.findall(r"\b(?:class|struct)\s+(\w+)\b[^;{]*\{", text[:m.start()])
                if not decls or decls[-1] != owner:
                    continue
            body = re.sub(r"//.*", "", m.group(1))
            body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
            return [v.split("=")[0].strip() for v in body.split(",") if v.strip()]
    return None


def main() -> int:
    xml = (ROOT / "game/state/gamestate_serialize.xml").read_text()
    headers = {p: p.read_text(errors="ignore")
               for d in ("game", "framework", "library") for p in (ROOT / d).rglob("*.h")}
    gaps = []
    for qualified, block in re.findall(r"<enum>\s*<name>([^<]+)</name>(.*?)</enum>", xml, re.S):
        listed = set(re.findall(r"<value>([^<]+)</value>", block))
        values = cpp_enum_values(qualified, headers)
        if values is None:
            continue
        missing = [v for v in values if v not in listed]
        if missing:
            gaps.append(f"{qualified}: {', '.join(missing)}")
    for g in gaps:
        print(f"MISSING from gamestate_serialize.xml - {g}")
    if not gaps:
        print("Every serialized enum value is listed in gamestate_serialize.xml")
    return 1 if gaps else 0


if __name__ == "__main__":
    sys.exit(main())
