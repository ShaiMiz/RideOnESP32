"""Writes KiCad 9 schematic files (.kicad_sch, .kicad_sym, .kicad_pro) from the design in design.py.

Style: every pin gets a short wire stub and a net label, so the schematic reads as a
wiring spec (each pin says where it goes) and the netlist comes straight from the labels.
Nets used on more than one sheet get global labels, the rest local labels.
"""
import json
import math
import re
import uuid
from pathlib import Path

GRID = 1.27
PIN_LEN = 2.54
STUB = 5.08
CHAR_W = 1.2  # KiCad's stroke font at 1.27 mm, measured on the PDF
FONT = 1.27
NS = uuid.UUID("6f1c5a52-6e0b-4d39-9a59-2b8f0b6b3a10")
LIB = "rideonesp32"
PAPERS = [("A3", 420.0, 297.0), ("A2", 594.0, 420.0), ("A1", 841.0, 594.0)]


def uid(*parts):
    return str(uuid.uuid5(NS, "/".join(str(p) for p in parts)))


def q(s):
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def snap(v, g=GRID):
    return round(round(v / g) * g, 4)


def fmt(v):
    v = round(v, 4)
    return str(int(v)) if v == int(v) else f"{v:g}"


def text_w(s, size=FONT):
    return len(s) * CHAR_W * size / FONT


# ---------------------------------------------------------------- s-expressions

def parse_sexp(text):
    tokens = re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+', text)
    stack, cur = [], []
    for t in tokens:
        if t == "(":
            stack.append(cur)
            cur = []
        elif t == ")":
            done = cur
            cur = stack.pop()
            cur.append(done)
        else:
            cur.append(t[1:-1].replace('\\"', '"') if t.startswith('"') else t)
    return cur


def find(node, key):
    return [n for n in node if isinstance(n, list) and n and n[0] == key]


# ---------------------------------------------------------------- symbols

class Pin:
    def __init__(s, number, name, x, y, angle, etype):
        s.number, s.name, s.x, s.y, s.angle, s.etype = number, name, x, y, angle, etype


class Symbol:
    """A library symbol: its lib_symbols text and its pins (library coordinates, y up)."""

    def __init__(s, lib_id, text, pins, bbox, ref_prefix, power=False):
        s.lib_id, s.text, s.pins, s.bbox, s.ref_prefix, s.power = lib_id, text, pins, bbox, ref_prefix, power

    def pin(s, key):
        for p in s.pins:
            if p.number == key:
                return p
        named = [p for p in s.pins if p.name == key]
        if len(named) == 1:
            return named[0]
        raise KeyError(f"{s.lib_id}: no pin {key!r}")


def load_stock(path):
    text = Path(path).read_text()
    syms = {}
    body = text[text.index("(symbol"):]
    # split top-level blocks by paren depth
    depth, start = 0, None
    i = 0
    while i < len(body):
        c = body[i]
        if c == '"':
            i += 1
            while body[i] != '"':
                i += 2 if body[i] == "\\" else 1
        elif c == "(":
            if depth == 0:
                start = i
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                blk = body[start:i + 1]
                node = parse_sexp(blk)[0]
                lib_id = node[1]
                pins, xs, ys = [], [], []
                for sub in find(node, "symbol"):
                    for p in find(sub, "pin"):
                        at = find(p, "at")[0]
                        num = find(p, "number")[0][1]
                        name = find(p, "name")[0][1]
                        pins.append(Pin(num, name, float(at[1]), float(at[2]), int(float(at[3])), p[1]))
                    for kind in ("rectangle", "polyline", "circle", "arc"):
                        for g in find(sub, kind):
                            for pt in re.findall(r"\(\s*(?:xy|start|end|center|mid)\s+([-\d.]+)\s+([-\d.]+)", _dump(g)):
                                xs.append(float(pt[0]))
                                ys.append(float(pt[1]))
                for p in pins:
                    xs.append(p.x)
                    ys.append(p.y)
                ref = [pr for pr in find(node, "property") if pr[1] == "Reference"][0][2]
                power = bool(find(node, "power"))
                syms[lib_id] = Symbol(lib_id, blk, pins, (min(xs), max(xs), min(ys), max(ys)), ref, power)
        i += 1
    return syms


def _dump(node):
    if isinstance(node, list):
        return "(" + " ".join(_dump(n) for n in node) + ")"
    return str(node)


def effects(size=FONT, hide=False, justify=None):
    j = f" (justify {justify})" if justify else ""
    h = " (hide yes)" if hide else ""
    return f"(effects (font (size {fmt(size)} {fmt(size)})){j}{h})"


def module_symbol(name, left, right, ref="U", width=None, descr="", top_text=None):
    """A box symbol for a module or board. left/right: lists of (number, name, etype); None = gap."""
    rows = max(len(left), len(right))
    lw = max([text_w(p[1]) for p in left if p] + [0])
    rw = max([text_w(p[1]) for p in right if p] + [0])
    w = width or max(15.24, snap(lw + rw + 5.08, 2.54))
    w = snap(w, 2.54)
    if (w / 2.54) % 2:
        w += 2.54
    half = w / 2
    ytop = snap(((rows - 1) / 2) * 2.54, 2.54)
    pins, lines = [], []
    for side, lst in (("L", left), ("R", right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            num, pname, et = p
            y = ytop - i * 2.54
            if side == "L":
                x, ang = -half - PIN_LEN, 0
            else:
                x, ang = half + PIN_LEN, 180
            pins.append(Pin(num, pname, x, y, ang, et))
            lines.append(
                f"(pin {et} line (at {fmt(x)} {fmt(y)} {ang}) (length {fmt(PIN_LEN)}) "
                f"(name {q(pname)} {effects()}) (number {q(num)} {effects()}))")
    y1, y2 = ytop + 2.54, ytop - rows * 2.54
    body = (f"(rectangle (start {fmt(-half)} {fmt(y1)}) (end {fmt(half)} {fmt(y2)}) "
            f"(stroke (width 0.254) (type default)) (fill (type background)))")
    extra = ""
    if top_text:
        extra = f"(text {q(top_text)} (at 0 {fmt(y2 + 1.27)} 0) {effects(1.0)})"
    lib_id = f"{LIB}:{name}"
    same = all(p.number == p.name for p in pins)
    hide_nums = " (pin_numbers (hide yes))" if same else ""
    text = (f"(symbol {q(lib_id)}{hide_nums} (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)\n"
            f"  (property \"Reference\" {q(ref)} (at 0 {fmt(y1 + 1.27)} 0) {effects()})\n"
            f"  (property \"Value\" {q(name)} (at 0 {fmt(y2 - 1.27)} 0) {effects()})\n"
            f"  (property \"Footprint\" \"\" (at 0 0 0) {effects(hide=True)})\n"
            f"  (property \"Datasheet\" \"\" (at 0 0 0) {effects(hide=True)})\n"
            f"  (property \"Description\" {q(descr)} (at 0 0 0) {effects(hide=True)})\n"
            f"  (symbol {q(name + '_0_1')} {body} {extra})\n"
            f"  (symbol {q(name + '_1_1')}\n    " + "\n    ".join(lines) + ")\n)")
    return Symbol(lib_id, text, pins, (-half - PIN_LEN, half + PIN_LEN, y2, y1), ref)


# ---------------------------------------------------------------- geometry

def rot_pin(p, rot):
    """Library pin -> (dx, dy) offset on the sheet (y down) and the outward direction on the sheet."""
    r = math.radians(rot)
    qx = p.x * math.cos(r) - p.y * math.sin(r)
    qy = p.x * math.sin(r) + p.y * math.cos(r)
    a = math.radians(p.angle + rot)
    out = (-round(math.cos(a)), round(math.sin(a)))
    return (round(qx, 4), round(-qy, 4)), out


def rot_bbox(bb, rot):
    x0, x1, y0, y1 = bb
    pts = []
    for x in (x0, x1):
        for y in (y0, y1):
            r = math.radians(rot)
            qx = x * math.cos(r) - y * math.sin(r)
            qy = x * math.sin(r) + y * math.cos(r)
            pts.append((qx, -qy))
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), max(xs), min(ys), max(ys)


# ---------------------------------------------------------------- sheet writer

class Sheet:
    def __init__(s, name, filename, title, page, comments=()):
        s.name, s.filename, s.title, s.page, s.comments = name, filename, title, page, list(comments)
        s.columns = []  # list of lists of items
        s.items = []    # emitted s-expressions
        s.paper = None

    def column(s, *items, title=None, gap=None):
        s.columns.append(dict(items=list(items), title=title))


def label_len(net, is_global):
    return text_w(net) + (3.2 if is_global else 0.6)


class Writer:
    def __init__(s, project, symbols, root_uuid, date, rev):
        s.project, s.symbols, s.root_uuid, s.date, s.rev = project, symbols, root_uuid, date, rev
        s.global_nets = set()
        s.pwr_n = 0

    # extents of a placed part, relative to its origin, including stubs and labels
    def part_extent(s, part):
        sym = s.symbols[part.sym]
        x0, x1, y0, y1 = rot_bbox(sym.bbox, part.rot)
        y0 -= 3.0  # reference text above
        y1 += 3.0  # value text below
        for key, net in part.pins.items():
            p = sym.pin(key)
            (dx, dy), (ox, oy) = rot_pin(p, part.rot)
            if net is None:
                continue
            L = STUB + label_len(net, net in s.global_nets)
            ex, ey = dx + ox * L, dy + oy * L
            x0, x1 = min(x0, ex), max(x1, ex)
            y0, y1 = min(y0, ey - 1.5), max(y1, ey + 1.5)
        return x0, x1, y0, y1

    def layout(s, sheet):
        margin_x, colgap, rowgap = 15.0, 7.62, 5.08
        margin_top = 22.0 + 2.6 * len(sheet.comments)
        placed, cols = [], []
        x = margin_x
        band_top = margin_top
        for col in sheet.columns:
            if col["title"] == "---":   # start a new band of columns below the ones so far
                band_top = max(c[2] for c in cols) + 4.0
                x = margin_x
                continue
            width = 0
            exts = []
            for it in col["items"]:
                e = (0, it.width, 0, it.height()) if isinstance(it, Note) else s.part_extent(it)
                exts.append(e)
                width = max(width, e[1] - e[0])
            if col["title"]:
                width = max(width, text_w(col["title"], 2.0))
            y = band_top
            if col["title"]:
                placed.append(("title", col["title"], x, y))
                y += 7.0
            for it, e in zip(col["items"], exts):
                if isinstance(it, Note):
                    placed.append(("note", it, x, y))
                    y += e[3] + rowgap
                else:
                    ox = snap(x - e[0], 2.54)
                    oy = snap(y - e[2], 2.54)
                    placed.append(("part", it, ox, oy))
                    y = oy + e[3] + rowgap
            cols.append((x, x + width, y))
            x += width + colgap
        for name, w, h in PAPERS:
            ok = max(c[1] for c in cols) + margin_x <= w
            for c0, c1, bottom in cols:
                limit = h - 12 if c1 < w - 190 else h - 50   # keep clear of the title block
                ok = ok and bottom <= limit
            if ok:
                sheet.paper = (name, w, h)
                break
        else:
            raise SystemExit(f"{sheet.name}: content does not fit A1")
        for i, line in enumerate(sheet.comments):
            placed.append(("comment", line, margin_x, 15.0 + 2.6 * i))
        return placed

    def emit_part(s, sheet, path, part, ox, oy):
        sym = s.symbols[part.sym]
        out = []
        u = uid(s.project, "sym", part.ref)
        x0, x1, y0, y1 = rot_bbox(sym.bbox, part.rot)
        props = [("Reference", part.ref, (ox, oy + y0 - 1.8), False),
                 ("Value", part.value, (ox, oy + y1 + 2.0), False),
                 ("Footprint", "", (ox, oy), True), ("Datasheet", part.datasheet, (ox, oy), True),
                 ("Description", part.descr, (ox, oy), True)]
        for k, v in part.fields.items():
            props.append((k, v, (ox, oy), True))
        fang = (360 - part.rot) % 360
        ptxt = "\n".join(f"  (property {q(k)} {q(v)} (at {fmt(px)} {fmt(py)} {fang}) {effects(hide=h)})"
                         for k, v, (px, py), h in props)
        pins_txt = "\n".join(f"  (pin {q(p.number)} (uuid {q(uid(u, 'pin', p.number))}))" for p in sym.pins)
        dnp = "yes" if part.dnp else "no"
        in_bom = "yes" if part.in_bom else "no"
        out.append(
            f"(symbol (lib_id {q(sym.lib_id)}) (at {fmt(ox)} {fmt(oy)} {part.rot}) (unit 1)\n"
            f"  (exclude_from_sim no) (in_bom {in_bom}) (on_board yes) (dnp {dnp})\n"
            f"  (uuid {q(u)})\n{ptxt}\n{pins_txt}\n"
            f"  (instances (project {q(s.project)} (path {q(path)} (reference {q(part.ref)}) (unit 1))))\n)")
        for key, net in part.pins.items():
            p = sym.pin(key)
            (dx, dy), (odx, ody) = rot_pin(p, part.rot)
            px, py = ox + dx, oy + dy
            if net is None:
                out.append(f"(no_connect (at {fmt(px)} {fmt(py)}) (uuid {q(uid(u, 'nc', key))}))")
                continue
            ex, ey = px + odx * STUB, py + ody * STUB
            out.append(f"(wire (pts (xy {fmt(px)} {fmt(py)}) (xy {fmt(ex)} {fmt(ey)})) "
                       f"(stroke (width 0) (type default)) (uuid {q(uid(u, 'w', key))}))")
            out.append(s.label(net, ex, ey, (odx, ody), uid(u, "l", key)))
        missing = [p.number for p in sym.pins if p.number not in part.pins]
        if missing:
            raise SystemExit(f"{part.ref}: pins not assigned: {missing}")
        return out

    def label(s, net, x, y, out, u):
        ang = {(1, 0): 0, (-1, 0): 180, (0, -1): 90, (0, 1): 270}[out]
        just = {0: "left", 180: "right", 90: "left", 270: "right"}[ang]
        if net in s.global_nets:
            return (f"(global_label {q(net)} (shape passive) (at {fmt(x)} {fmt(y)} {ang}) (fields_autoplaced yes) "
                    f"{effects(justify=just)} (uuid {q(u)}) "
                    f"(property \"Intersheetrefs\" \"${{INTERSHEET_REFS}}\" (at {fmt(x)} {fmt(y)} 0) {effects(hide=True)}))")
        return (f"(label {q(net)} (at {fmt(x)} {fmt(y)} {ang}) {effects(justify=just + ' bottom')} "
                f"(uuid {q(u)}))")

    def emit_note(s, note, x, y, key):
        out = []
        if note.title:
            out.append(f"(text {q(note.title)} (exclude_from_sim no) (at {fmt(x)} {fmt(y)} 0) "
                       f"(effects (font (size 1.6 1.6) (bold yes)) (justify left top)) (uuid {q(uid(key, 't'))}))")
            y += 3.0
        out.append(f"(text {q(chr(10).join(note.lines))} (exclude_from_sim no) (at {fmt(x)} {fmt(y)} 0) "
                   f"{effects(justify='left top')} (uuid {q(uid(key, 'b'))}))")
        return out

    def title_block(s, sheet):
        c = " (comment 1 \"Draft v0.3, not for building the car-facing side until the TBDs close\")"
        return (f"(title_block (title {q(sheet.title)}) (date {q(s.date)}) (rev {q(s.rev)}) "
                f"(company \"RideOnESP32 · Can-Am Maverick X3 24 V 4WD\"){c})")

    def sheet_file(s, sheet, sheet_uuid, path, lib_ids, extra_items=(), placed=None):
        items = list(extra_items)
        for kind, it, x, y in placed or []:
            if kind == "part":
                items += s.emit_part(sheet, path, it, x, y)
            elif kind == "note":
                items += s.emit_note(it, x, y, uid(sheet.filename, "note", it.title, x, y))
            elif kind == "comment":
                items.append(f"(text {q(it)} (exclude_from_sim no) (at {fmt(x)} {fmt(y)} 0) "
                             f"(effects (font (size 1.5 1.5) (italic yes)) (justify left top)) (uuid {q(uid(sheet.filename, 'cm', it))}))")
            else:
                items.append(f"(text {q(it)} (exclude_from_sim no) (at {fmt(x)} {fmt(y)} 0) "
                             f"(effects (font (size 2 2) (bold yes)) (justify left top)) (uuid {q(uid(sheet.filename, 'col', it))}))")
        libs = "\n".join(s.symbols[l].text for l in sorted(lib_ids))
        paper = sheet.paper[0]
        return (f"(kicad_sch (version 20250114) (generator \"rideonesp32_gen\") (generator_version \"9.0\")\n"
                f"(uuid {q(sheet_uuid)})\n(paper {q(paper)})\n{s.title_block(sheet)}\n"
                f"(lib_symbols\n{libs}\n)\n" + "\n".join(items) + "\n" +
                ("(sheet_instances (path \"/\" (page \"1\")))\n" if path == "/" + s.root_uuid and sheet.page == "1" else "") +
                "(embedded_fonts no)\n)\n")


class Note:
    def __init__(s, title, lines, width=None):
        s.title, s.lines = title, lines
        s.width = max([text_w(l) for l in lines] + [text_w(title or "", 1.6)])

    def height(s):
        return (3.0 if s.title else 0) + len(s.lines) * 2.1 + 1


def project_file(name, sheets):
    """Minimal .kicad_pro; KiCad fills in its defaults on first open."""
    return json.dumps({
        "meta": {"filename": f"{name}.kicad_pro", "version": 3},
        "erc": {"erc_exclusions": [], "meta": {"version": 0}, "pin_map": [],
                "rule_severities": {"single_global_label": "ignore", "lib_symbol_issues": "warning",
                                    "lib_symbol_mismatch": "ignore", "footprint_link_issues": "ignore"}},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [[u, n] for u, n in sheets],
        "text_variables": {},
    }, indent=2) + "\n"
