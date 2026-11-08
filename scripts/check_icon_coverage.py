#!/usr/bin/env python3
"""Checks which aircraft types in live traffic have no icon of their own.

Fetches the aircraft within 250 nm of a few dozen busy places from the
adsb.fi open data API (one request every 2 s, as its usage terms ask),
then lists the ICAO type codes that main/core/aircraft_types.cpp doesn't
know, most common first, and how many aircraft only get a size-class icon.

  python3 scripts/check_icon_coverage.py [--places N]
"""
import argparse
import collections
import json
import os
import re
import time
import urllib.request

PLACES = [  # lat, lon: big hubs, general aviation and military areas
    (40.64, -73.78), (33.94, -118.41), (41.98, -87.90), (33.64, -84.43), (32.90, -97.04), (39.86, -104.67),
    (47.45, -122.31), (25.79, -80.29), (44.25, -121.15), (33.43, -112.01), (61.17, -149.99), (21.32, -157.92),
    (36.24, -115.03), (36.94, -76.29), (43.68, -79.63), (19.44, -99.07), (51.47, -0.45), (50.04, 8.56),
    (52.31, 4.76), (49.01, 2.55), (40.47, -3.56), (41.27, 28.75), (49.44, 7.60), (25.25, 55.36), (28.56, 77.10),
    (1.36, 103.99), (22.31, 113.91), (35.55, 139.78), (-33.94, 151.17), (-23.43, -46.47), (-26.13, 28.24),
    (45.47, -73.74),
]

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--places', type=int, default=len(PLACES))
    args = ap.parse_args()
    table = open(os.path.join(ROOT, 'main', 'core', 'aircraft_types.cpp')).read()
    known = dict(re.findall(r'\{"([A-Z0-9]+)", Icon::(\w+)\}', table))
    mask = {e: m for m, _sz, e in re.findall(r'\{"([a-z0-9_]+)", (\d+)\},  // (\w+)', table)}
    seen = collections.Counter()
    desc = {}
    for lat, lon in PLACES[:args.places]:
        url = f'https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/250'
        req = urllib.request.Request(url, headers={'User-Agent': 'plane-radar-p4 icon coverage check'})
        try:
            data = json.load(urllib.request.urlopen(req, timeout=30))
        except Exception as e:  # noqa: BLE001
            print(f'{lat},{lon}: {e}')
            continue
        for a in data.get('ac', []):
            t = (a.get('t') or '').upper()
            if t:
                seen[t] += 1
                desc.setdefault(t, a.get('desc', ''))
        time.sleep(2)
    total = sum(seen.values())
    missing = [(t, n) for t, n in seen.most_common() if t not in known]
    generic = sum(n for t, n in seen.items() if t in known and not mask[known[t]].startswith(('x_', 'g_')))
    print(f'{total} aircraft with a type, {len(seen)} types')
    print(f'{sum(n for _, n in missing)} aircraft ({len(missing)} types) not in the table:')
    for t, n in missing:
        print(f'  {n:5d}  {t:5s}  {desc[t]}')
    print(f'{generic} aircraft on a class icon (gliders, balloons, vehicles...)')


if __name__ == '__main__':
    main()
