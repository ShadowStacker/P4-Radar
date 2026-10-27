#!/usr/bin/env python3
"""Generate the embedded airport + runway dataset from OurAirports.

Includes large, medium and small airports (open, non-heliport runways).
Airports are sorted by latitude and runways by airport, so the radar can
binary-search the visible band instead of scanning everything. Output:
  main/data/airports_data.cpp

Runways without surveyed end coordinates (common for small strips) are drawn
through the airport position using the heading from their designator
("17/35" -> 170 deg) or the published heading, and flagged as approximate.

Usage:
  python3 scripts/build_airports.py                 # download from GitHub mirror
  python3 scripts/build_airports.py --csv-dir DIR   # use local airports.csv/runways.csv
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import re
import unicodedata
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_CPP = ROOT / "main" / "data" / "airports_data.cpp"

BASE_URL = "https://raw.githubusercontent.com/davidmegginson/ourairports-data/main/"
TYPES = {"large_airport": 0, "medium_airport": 1, "small_airport": 2}
EARTH_R_M = 6371008.8
PAVED = re.compile(r"ASP|CON|PEM|BIT|PAV|TAR|MAC", re.I)

FLAG_PAVED = 1
FLAG_APPROX = 2


def load_csv(name: str, csv_dir: Path | None) -> list[dict[str, str]]:
    if csv_dir is not None:
        text = (csv_dir / name).read_text(encoding="utf-8")
    else:
        with urllib.request.urlopen(BASE_URL + name, timeout=120) as resp:
            text = resp.read().decode("utf-8")
    return list(csv.DictReader(io.StringIO(text)))


def fnum(s: str | None) -> float | None:
    if s is None or not s.strip():
        return None
    try:
        return float(s)
    except ValueError:
        return None


def is_helipad_ident(s: str) -> bool:
    s = s.strip().upper()
    return bool(s) and s[0] == "H" and (len(s) == 1 or s[1:].isdigit() or s[1] in "-_")


def destination(lat: float, lon: float, brg_deg: float, dist_m: float) -> tuple[float, float]:
    d = dist_m / EARTH_R_M
    b = math.radians(brg_deg)
    p1 = math.radians(lat)
    l1 = math.radians(lon)
    p2 = math.asin(math.sin(p1) * math.cos(d) + math.cos(p1) * math.sin(d) * math.cos(b))
    l2 = l1 + math.atan2(math.sin(b) * math.sin(d) * math.cos(p1),
                         math.cos(d) - math.sin(p1) * math.sin(p2))
    return math.degrees(p2), (math.degrees(l2) + 540.0) % 360.0 - 180.0


def designator_heading(ident: str) -> float | None:
    """'17L' -> 170, '9' -> 90, 'E' -> 90 (some strips are named by direction)."""
    ident = ident.strip().upper()
    m = re.match(r"^(\d{1,2})[LRC]?$", ident)
    if m:
        n = int(m.group(1))
        return (n * 10.0) % 360.0 if 1 <= n <= 36 else None
    return {"N": 0.0, "NE": 45.0, "E": 90.0, "SE": 135.0, "S": 180.0, "SW": 225.0, "W": 270.0,
            "NW": 315.0}.get(ident)


def c_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def ascii_only(s: str) -> str:
    # The UI font only carries ASCII; drop accents rather than render boxes.
    return unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv-dir", type=Path, default=None)
    args = ap.parse_args()

    airports = load_csv("airports.csv", args.csv_dir)
    runways = load_csv("runways.csv", args.csv_dir)

    picked: list[dict] = []
    for row in airports:
        t = row["type"]
        if t not in TYPES:
            continue
        lat, lon = fnum(row["latitude_deg"]), fnum(row["longitude_deg"])
        if lat is None or lon is None:
            continue
        # Prefer the code pilots use: ICAO, then GPS code, then FAA/local code.
        code = ""
        for key in ("icao_code", "gps_code", "local_code", "ident"):
            c = (row.get(key) or "").strip()
            if c and len(c) <= 7:
                code = c
                break
        picked.append({
            "src_ident": row["ident"],
            "ident": code,
            "iata": (row.get("iata_code") or "").strip()[:3],
            "name": ascii_only(row["name"].strip())[:32],
            "lat": lat, "lon": lon,
            "cls": TYPES[t],
        })
    picked.sort(key=lambda a: (a["lat"], a["lon"]))
    index = {a["src_ident"]: i for i, a in enumerate(picked)}

    rws: list[tuple] = []
    for row in runways:
        idx = index.get(row["airport_ident"])
        if idx is None or row.get("closed") == "1":
            continue
        le_id, he_id = row.get("le_ident") or "", row.get("he_ident") or ""
        if is_helipad_ident(le_id) or is_helipad_ident(he_id):
            continue
        length_ft = fnum(row.get("length_ft")) or 0.0
        if length_ft < 800:
            continue
        length_m = length_ft * 0.3048
        flags = FLAG_PAVED if PAVED.search(row.get("surface") or "") else 0

        le = (fnum(row["le_latitude_deg"]), fnum(row["le_longitude_deg"]))
        he = (fnum(row["he_latitude_deg"]), fnum(row["he_longitude_deg"]))
        hdg = fnum(row.get("le_heading_degT"))
        if None not in le and None not in he:
            pass
        elif None not in le and hdg is not None:
            he = destination(le[0], le[1], hdg, length_m)
        elif None not in he and fnum(row.get("he_heading_degT")) is not None:
            le = destination(he[0], he[1], fnum(row.get("he_heading_degT")), length_m)
        else:
            # No surveyed ends: centre it on the airport, along the designator heading.
            if hdg is None:
                hdg = designator_heading(le_id)
            if hdg is None:
                continue
            a = picked[idx]
            le = destination(a["lat"], a["lon"], hdg + 180.0, length_m / 2)
            he = destination(a["lat"], a["lon"], hdg, length_m / 2)
            flags |= FLAG_APPROX
        rws.append((idx, round(le[0] * 1e7), round(le[1] * 1e7),
                    round(he[0] * 1e7), round(he[1] * 1e7), min(int(length_m), 65535), flags))
    rws.sort(key=lambda r: r[0])

    lines = [
        "// Generated by scripts/build_airports.py from OurAirports (public domain).",
        "// Do not edit by hand.",
        '#include "data/airports.h"',
        "",
        "namespace data {",
        "",
        f"const size_t kAirportCount = {len(picked)};",
        f"const size_t kRunwayCount = {len(rws)};",
        "",
        "// Sorted by latitude.",
        "const Airport kAirports[] = {",
    ]
    for a in picked:
        lines.append(
            f"  {{{c_str(a['ident'])}, {c_str(a['iata'])}, {c_str(a['name'])}, "
            f"{round(a['lat'] * 1e7)}, {round(a['lon'] * 1e7)}, {a['cls']}}},")
    lines += ["};", "", "// Sorted by airport_idx.", "const Runway kRunways[] = {"]
    for r in rws:
        lines.append("  {%d, %d, %d, %d, %d, %d, %d}," % r)
    lines += ["};", "", "}  // namespace data", ""]
    OUT_CPP.parent.mkdir(parents=True, exist_ok=True)
    OUT_CPP.write_text("\n".join(lines), encoding="utf-8")
    counts = [sum(1 for a in picked if a["cls"] == c) for c in range(3)]
    approx = sum(1 for r in rws if r[6] & FLAG_APPROX)
    print(f"wrote {OUT_CPP}: {len(picked)} airports (large {counts[0]}, medium {counts[1]}, "
          f"small {counts[2]}), {len(rws)} runways ({approx} approximate)")


if __name__ == "__main__":
    main()
