#!/usr/bin/env python3
"""Find unreferenced top-level definitions in tools/oa_*.py without importing them."""
from __future__ import annotations

import ast
import re
from pathlib import Path

# Deliberate command-line entry point, invoked through each module's __main__ guard.
ALLOWLIST = {"main"}


class References(ast.NodeVisitor):
    """Collect executable references and strings, ignoring comments and docstrings."""

    def __init__(self):
        self.names: list[tuple[str, int]] = []
        self.strings: list[str] = []

    def visit_Expr(self, node):
        # A standalone string is documentation, including module/class/function docstrings.
        if not isinstance(node.value, ast.Constant) or not isinstance(node.value.value, str):
            self.generic_visit(node)

    def visit_Name(self, node):
        if isinstance(node.ctx, ast.Load):
            self.names.append((node.id, node.lineno))

    def visit_Attribute(self, node):
        if isinstance(node.ctx, ast.Load):
            self.names.append((node.attr, node.lineno))
        self.generic_visit(node)

    def visit_Import(self, node):
        for alias in node.names:
            self.names.append((alias.name.rsplit(".", 1)[-1], node.lineno))

    def visit_ImportFrom(self, node):
        for alias in node.names:
            self.names.append((alias.name, node.lineno))

    def visit_Constant(self, node):
        if isinstance(node.value, str):
            self.strings.append(node.value)


def dead_definitions(directory: Path) -> list[tuple[Path, int, str]]:
    """References include imports/attributes and other files' getattr-style string names."""
    sources = {}
    for path in sorted(directory.glob("*.py")):
        tree = ast.parse(path.read_text(), filename=str(path))
        refs = References()
        refs.visit(tree)
        sources[path] = (tree, refs)

    dead = []
    for path, (tree, _) in sources.items():
        if not path.name.startswith("oa_"):
            continue
        for node in tree.body:
            if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                continue
            if node.name in ALLOWLIST:
                continue
            pattern = re.compile(r"\b" + re.escape(node.name) + r"\b")
            referenced = False
            for other, (_, refs) in sources.items():
                if any(name == node.name and
                       (other != path or not node.lineno <= line <= node.end_lineno)
                       for name, line in refs.names):
                    referenced = True
                    break
                if other != path and any(pattern.search(value) for value in refs.strings):
                    referenced = True
                    break
            if not referenced:
                dead.append((path, node.lineno, node.name))
    return dead


def main() -> int:
    dead = dead_definitions(Path(__file__).resolve().parent)
    for path, line, name in dead:
        print(f"{path.name}:{line}: unreferenced definition {name}")
    if dead:
        return 1
    print("No unreferenced definitions in tools/oa_*.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
