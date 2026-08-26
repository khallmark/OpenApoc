#!/usr/bin/env python3
"""Resolve OpenApoc .form definitions into absolute on-screen rectangles.

The engine lays forms out in forms/control.cpp: numeric <position>/<size> values are literal,
"left"/"centre"/"right" and "top"/"centre"/"bottom" align against the *parent* size, and a size
ending in '%' is a percentage of the parent. Form roots align against the display size. Every
<style> block is applied in document order (forms/form.cpp still has the "pick best style" TODO),
so we replicate that rather than trying to select one.

This lets an automated client click a control by id instead of by hard-coded pixel guesses.
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

# Nodes that describe the owning control rather than a child control.
ATTRIBUTE_NODES = {
    "position",
    "size",
    "backcolour",
    "font",
    "alignment",
    "image",
    "tooltip",
    "palette",
    "text",
    "bordercolour",
    "imageposition",
    "hoverimage",
    "downimage",
    "scrollbarbuttons",
    "scrollbarcolours",
    "item",
    "itemsize",
    "hovercolour",
    "selcolour",
    "scrollbarvalues",
}

CONTROL_TAGS = {
    "form",
    "subform",
    "graphic",
    "label",
    "textbutton",
    "graphicbutton",
    "checkbox",
    "radiobutton",
    "listbox",
    "scroll",
    "scrollbar",
    "textedit",
    "ticker",
    "multilistbox",
    "tristatebox",
}


@dataclass
class Control:
    tag: str
    cid: str | None
    text: str | None
    raw_x: str | None
    raw_y: str | None
    raw_w: str | None
    raw_h: str | None
    src: str | None = None
    children: list["Control"] = field(default_factory=list)
    # resolved
    x: int = 0
    y: int = 0
    w: int = 0
    h: int = 0

    @property
    def centre(self) -> tuple[int, int]:
        return (self.x + self.w // 2, self.y + self.h // 2)


def _axis(raw: str | None, parent_extent: int, own_extent: int, kind: str) -> int:
    """Resolve one position axis. `kind` is 'x' or 'y'."""
    if raw is None or raw == "":
        return 0
    try:
        return int(raw)
    except ValueError:
        pass
    if kind == "x":
        if raw == "left":
            return 0
        if raw == "centre":
            return (parent_extent - own_extent) // 2
        if raw == "right":
            return parent_extent - own_extent
    else:
        if raw == "top":
            return 0
        if raw == "centre":
            return (parent_extent - own_extent) // 2
        if raw == "bottom":
            return parent_extent - own_extent
    return 0


def _extent(raw: str | None, parent_extent: int) -> int:
    if raw is None or raw == "":
        return 0
    if raw.endswith("%"):
        try:
            return parent_extent * int(raw[:-1]) // 100
        except ValueError:
            return 0
    try:
        return int(raw)
    except ValueError:
        return 0


class FormLibrary:
    def __init__(self, forms_dir: Path):
        self.forms_dir = Path(forms_dir)
        self.forms: dict[str, Control] = {}
        self.by_file: dict[str, Control] = {}
        self._load_all()

    def _load_all(self) -> None:
        for path in sorted(self.forms_dir.rglob("*.form")):
            rel = path.relative_to(self.forms_dir).with_suffix("").as_posix()
            try:
                root = ET.parse(path).getroot()
            except ET.ParseError:
                continue
            for form_node in root.iter("form"):
                ctrl = self._build(form_node)
                self.by_file[rel] = ctrl
                if ctrl.cid:
                    self.forms[ctrl.cid] = ctrl

    def _build(self, node: ET.Element) -> Control:
        pos = node.find("position")
        size = node.find("size")
        text_node = node.find("text")
        text = node.get("text")
        if text is None and text_node is not None:
            text = (text_node.text or "").strip()
        ctrl = Control(
            tag=node.tag,
            cid=node.get("id"),
            text=text,
            raw_x=pos.get("x") if pos is not None else None,
            raw_y=pos.get("y") if pos is not None else None,
            raw_w=size.get("width") if size is not None else None,
            raw_h=size.get("height") if size is not None else None,
            src=node.get("src"),
        )
        for child in node:
            if child.tag in ATTRIBUTE_NODES:
                continue
            if child.tag == "style":
                # Style blocks configure the *owning* form, applied in document order.
                styled = self._build(child)
                ctrl.raw_x = styled.raw_x if styled.raw_x is not None else ctrl.raw_x
                ctrl.raw_y = styled.raw_y if styled.raw_y is not None else ctrl.raw_y
                ctrl.raw_w = styled.raw_w if styled.raw_w is not None else ctrl.raw_w
                ctrl.raw_h = styled.raw_h if styled.raw_h is not None else ctrl.raw_h
                ctrl.children.extend(styled.children)
                continue
            if child.tag in CONTROL_TAGS:
                ctrl.children.append(self._build(child))
        return ctrl

    def resolve(self, form_key: str, display_w: int, display_h: int) -> dict[str, Control]:
        """Return {control_id: Control} with absolute display rects for one form."""
        root = self.by_file.get(form_key) or self.forms.get(form_key)
        if root is None:
            raise KeyError(f"unknown form {form_key!r}")
        found: dict[str, Control] = {}
        self._resolve_into(root, 0, 0, display_w, display_h, found, set())
        return found

    def _resolve_into(
        self,
        node: Control,
        parent_x: int,
        parent_y: int,
        parent_w: int,
        parent_h: int,
        found: dict[str, Control],
        seen_srcs: set[str],
    ) -> None:
        node.w = _extent(node.raw_w, parent_w)
        node.h = _extent(node.raw_h, parent_h)
        node.x = parent_x + _axis(node.raw_x, parent_w, node.w, "x")
        node.y = parent_y + _axis(node.raw_y, parent_h, node.h, "y")
        if node.cid and node.cid not in found:
            found[node.cid] = node

        children = list(node.children)
        # A <subform src="..."> pulls in another form file's controls at this offset.
        if node.src and node.src not in seen_srcs:
            seen_srcs = seen_srcs | {node.src}
            sub = self.by_file.get(node.src)
            if sub is not None:
                if node.w == 0:
                    node.w = _extent(sub.raw_w, parent_w)
                if node.h == 0:
                    node.h = _extent(sub.raw_h, parent_h)
                children.extend(sub.children)

        for child in children:
            self._resolve_into(child, node.x, node.y, node.w, node.h, found, seen_srcs)


if __name__ == "__main__":
    import sys

    lib = FormLibrary(Path(sys.argv[1] if len(sys.argv) > 1 else "data/forms"))
    key = sys.argv[2] if len(sys.argv) > 2 else "mainmenu"
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 1280
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 720
    for cid, c in sorted(lib.resolve(key, w, h).items()):
        print(f"{cid:34s} {c.tag:14s} rect=({c.x},{c.y},{c.w},{c.h}) centre={c.centre} text={c.text}")
