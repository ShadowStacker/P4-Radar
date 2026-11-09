#!/usr/bin/env python3
"""Builds the map overlay: country borders, state/province lines, coastlines
and (US) controlled airspace, cut into 5 x 5 degree cells.

Two products:
  geo/v1/<cell>.bin                 detailed cells, downloaded by the radar on
                                    demand from this repository (raw.githubusercontent.com)
                                    and kept on its SD card
  main/data/geo_world_data.cpp      a coarse world in the firmware: what a cell
                                    shows until (or unless) its detailed file arrives,
                                    plus the list of cells that have a detailed file

Sources:
  Natural Earth 1:10m (detailed) and 1:50m (coarse), public domain:
    https://www.naturalearthdata.com
  FAA Class B, C and D airspace (public domain), from the FAA's ArcGIS service
    https://adds-faa.opendata.arcgis.com

Cell id = row * 72 + col, row = floor((lat + 90) / 5), col = floor((lon + 180) / 5).

A cell's payload (raw DEFLATE once stored) is "PRG5" (or "PRG4") followed by records:
  u8 kind            0 border, 1 state/province line, 2 coastline, 3 airspace
  [kind 3 only]      u8 class (0 B, 1 C, 2 D), varint floor and ceiling (100s of ft MSL)
  varint n           points (>= 2)
  n x (zigzag varint dlon, zigzag varint dlat)
                     1e-5 degree steps ("PRG5", detailed) or 1e-4 ("PRG4", coarse);
                     the first point is relative to the cell's
                     south-west corner, the rest to the point before
A line crossing a cell edge is kept in both cells (the segment that crosses
is in each), so neighbouring cells join up.

Usage:
  python3 scripts/build_geo.py [--cache DIR]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import urllib.parse
import urllib.request
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_TILES = ROOT / "geo" / "v1"
OUT_CPP = ROOT / "main" / "data" / "geo_world_data.cpp"

NE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/"
FAA = ("https://services6.arcgis.com/ssFJjBXIUyZDrSYZ/arcgis/rest/services/"
       "Class_Airspace/FeatureServer/0/query")

CELL_DEG = 5
ROWS, COLS = 180 // CELL_DEG, 360 // CELL_DEG
# Coordinate steps: detailed cells 1e-5 degree (about 1 m), coarse ones 1e-4
# (about 11 m; their lines are only good to a kilometre anyway). The magic
# says which: "PRG5" or "PRG4".
DETAIL_Q, COARSE_Q = 1e5, 1e4

BORDER, STATE, COAST, AIRSPACE = 0, 1, 2, 3
DETAIL = {  # layer: (Natural Earth file, Douglas-Peucker tolerance in metres)
    BORDER: ("ne_10m_admin_0_boundary_lines_land", 100),
    STATE: ("ne_10m_admin_1_states_provinces_lines", 100),
    COAST: ("ne_10m_coastline", 100),
}
COARSE = {
    BORDER: ("ne_50m_admin_0_boundary_lines_land", 1000),
    STATE: ("ne_50m_admin_1_states_provinces_lines", 1000),
    COAST: ("ne_50m_coastline", 1000),
}
AIRSPACE_TOL_M = 50
CLASSES = {"B": 0, "C": 1, "D": 2}


# --- sources ---------------------------------------------------------------------

def fetch(url: str, path: Path) -> Path:
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        print("downloading", url)
        with urllib.request.urlopen(url, timeout=300) as r:
            path.write_bytes(r.read())
    return path


def natural_earth(name: str, cache: Path) -> dict:
    return json.loads(fetch(NE + name + ".geojson", cache / (name + ".geojson")).read_text())


def faa_airspace(cache: Path) -> list[dict]:
    path = cache / "faa_class_bcd.geojson"
    if not path.exists():
        feats, off = [], 0
        while True:
            q = urllib.parse.urlencode({
                "where": "CLASS IN ('B','C','D')",
                "outFields": "NAME,CLASS,LOWER_VAL,LOWER_UOM,LOWER_CODE,UPPER_VAL,UPPER_UOM,UPPER_CODE",
                "outSR": 4326, "geometryPrecision": 5, "f": "geojson",
                "resultOffset": off, "resultRecordCount": 1000})
            print("downloading FAA airspace, from", off)
            with urllib.request.urlopen(FAA + "?" + q, timeout=300) as r:
                page = json.load(r)["features"]
            feats += page
            if len(page) < 1000:
                break
            off += 1000
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"type": "FeatureCollection", "features": feats}))
    return json.loads(path.read_text())["features"]


def lines_of(geom: dict | None) -> list[list[list[float]]]:
    if not geom:
        return []
    t, c = geom["type"], geom["coordinates"]
    if t == "LineString":
        return [c]
    if t in ("MultiLineString", "Polygon"):
        return c
    if t == "MultiPolygon":
        return [ring for poly in c for ring in poly]
    return []


def feet_100(val, uom, code) -> int:
    if code == "SFC" or val is None:
        return 0
    ft = float(val) * (100.0 if uom == "FL" else 1.0)
    return max(0, min(255 * 4, round(ft / 100)))


# --- geometry ----------------------------------------------------------------------

def simplify(pts: list[list[float]], tol_m: float) -> list[list[float]]:
    """Douglas-Peucker, distances measured with longitude scaled by cos(latitude)."""
    if tol_m <= 0 or len(pts) < 3:
        return pts
    tol = tol_m / 111320.0
    k = math.cos(math.radians(pts[0][1]))
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        ax, ay = pts[a][0] * k, pts[a][1]
        bx, by = pts[b][0] * k, pts[b][1]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy)
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i][0] * k, pts[i][1]
            d = abs(dx * (ay - py) - dy * (ax - px)) / length if length > 0 else math.hypot(px - ax, py - ay)
            if d > best:
                best, bi = d, i
        if best > tol:
            keep[bi] = True
            stack += [(a, bi), (bi, b)]
    return [p for p, kp in zip(pts, keep) if kp]


def cell_of(row: int, col: int) -> int:
    return row * COLS + col


def row_col(lat: float, lon: float) -> tuple[int, int]:
    return (min(ROWS - 1, max(0, int(math.floor((lat + 90) / CELL_DEG)))),
            min(COLS - 1, max(0, int(math.floor((lon + 180) / CELL_DEG)))))


def cut(pts: list[list[float]]) -> dict[int, list[list[list[float]]]]:
    """Splits a line into runs per cell: every segment goes to each cell its box touches."""
    runs: dict[int, list[list[list[float]]]] = {}
    last: dict[int, int] = {}  # cell -> index of the last segment added to it
    for i in range(len(pts) - 1):
        a, b = pts[i], pts[i + 1]
        if abs(a[0] - b[0]) > 180:  # wraps around the antimeridian: not a real segment
            continue
        r0, c0 = row_col(min(a[1], b[1]), min(a[0], b[0]))
        r1, c1 = row_col(max(a[1], b[1]), max(a[0], b[0]))
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                cell = cell_of(r, c)
                if last.get(cell) == i - 1:
                    runs[cell][-1].append(b)
                else:
                    runs.setdefault(cell, []).append([a, b])
                last[cell] = i
    return runs


# --- encoding ------------------------------------------------------------------------

def varint(v: int, out: bytearray) -> None:
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)


def zigzag(v: int) -> int:
    return (v << 1) ^ (v >> 63)


def encode_run(cell: int, run: list[list[float]], head: bytes, out: bytearray, q_: float) -> None:
    row, col = divmod(cell, COLS)
    x0, y0 = round((col * CELL_DEG - 180) * q_), round((row * CELL_DEG - 90) * q_)
    pts = []
    for p in run:  # drop points that round onto the one before
        q = (round(p[0] * q_), round(p[1] * q_))
        if not pts or q != pts[-1]:
            pts.append(q)
    if len(pts) < 2:
        return
    out += head
    varint(len(pts), out)
    px, py = x0, y0
    for x, y in pts:
        varint(zigzag(x - px), out)
        varint(zigzag(y - py), out)
        px, py = x, y


def raw_deflate(b: bytes) -> bytes:
    c = zlib.compressobj(9, zlib.DEFLATED, -15)
    return c.compress(b) + c.flush()


def build(layers: dict, airspace: list[dict] | None, cache: Path, q_: float) -> dict[int, bytes]:
    cells: dict[int, bytearray] = {}
    for kind, (name, tol) in layers.items():
        for ft in natural_earth(name, cache)["features"]:
            for line in lines_of(ft.get("geometry")):
                for cell, runs in cut(simplify(line, tol)).items():
                    for run in runs:
                        encode_run(cell, run, bytes([kind]), cells.setdefault(cell, bytearray()), q_)
    for ft in airspace or []:
        p = ft["properties"]
        cls = CLASSES.get(p.get("CLASS"))
        if cls is None:
            continue
        head = bytearray([AIRSPACE, cls])
        varint(feet_100(p.get("LOWER_VAL"), p.get("LOWER_UOM"), p.get("LOWER_CODE")), head)
        varint(feet_100(p.get("UPPER_VAL"), p.get("UPPER_UOM"), p.get("UPPER_CODE")), head)
        for ring in lines_of(ft.get("geometry")):
            for cell, runs in cut(simplify(ring, AIRSPACE_TOL_M)).items():
                for run in runs:
                    encode_run(cell, run, bytes(head), cells.setdefault(cell, bytearray()), q_)
    magic = b"PRG5" if q_ == DETAIL_Q else b"PRG4"
    return {c: magic + bytes(b) for c, b in cells.items()}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", type=Path, default=ROOT / "build" / "geo_src")
    args = ap.parse_args()

    detail = build(DETAIL, faa_airspace(args.cache), args.cache, DETAIL_Q)
    coarse = build(COARSE, None, args.cache, COARSE_Q)

    OUT_TILES.mkdir(parents=True, exist_ok=True)
    for old in OUT_TILES.glob("*.bin"):
        old.unlink()
    total = 0
    for cell, payload in sorted(detail.items()):
        z = raw_deflate(payload)
        (OUT_TILES / f"{cell}.bin").write_bytes(z)
        total += len(z)

    bits = bytearray((ROWS * COLS + 7) // 8)
    for cell in detail:
        bits[cell >> 3] |= 1 << (cell & 7)
    blob, offsets = bytearray(), []
    for cell in range(ROWS * COLS):
        offsets.append(len(blob))
        if cell in coarse:
            blob += raw_deflate(coarse[cell])
    offsets.append(len(blob))

    lines = [
        "// Generated by scripts/build_geo.py from Natural Earth (public domain) - do not edit.",
        '#include "data/geo_world.h"',
        "",
        "namespace data {",
        "",
        "// Cells with a detailed file in geo/v1 (bit cell % 8 of byte cell / 8).",
        "const uint8_t kGeoDetailCells[] = {",
    ]
    for k in range(0, len(bits), 24):
        lines.append("  " + ", ".join(str(b) for b in bits[k:k + 24]) + ",")
    lines += ["};", "", "// Coarse cell c is kGeoWorld[kGeoWorldOffset[c] .. kGeoWorldOffset[c + 1]) (empty: nothing there).",
              "const uint32_t kGeoWorldOffset[] = {"]
    for k in range(0, len(offsets), 16):
        lines.append("  " + ", ".join(str(o) for o in offsets[k:k + 16]) + ",")
    lines += ["};", "", "const uint8_t kGeoWorld[] = {"]
    for k in range(0, len(blob), 24):
        lines.append("  " + ", ".join(str(b) for b in blob[k:k + 24]) + ",")
    lines += ["};", "", "}  // namespace data", ""]
    OUT_CPP.write_text("\n".join(lines))
    print(f"detailed: {len(detail)} cells, {total // 1024} KB in {OUT_TILES.relative_to(ROOT)} "
          f"(largest {max(len(raw_deflate(p)) for p in detail.values()) // 1024} KB)")
    print(f"coarse: {len(coarse)} cells, {len(blob) // 1024} KB + {len(offsets) * 4 // 1024} KB index in the firmware")


if __name__ == "__main__":
    main()
