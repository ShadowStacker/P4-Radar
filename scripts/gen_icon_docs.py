#!/usr/bin/env python3
"""Writes docs/AIRCRAFT_ICONS.md: every aircraft icon, what it's for and its
ICAO type codes, from the same data as the firmware (main/core/
aircraft_types.{h,cpp} and the SVGs in assets/aircraft_icons). The page
shows tinted copies of the SVGs (docs/icons/), readable in light and dark
mode.

  python3 scripts/gen_icon_docs.py
"""
import collections
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ASSETS = os.path.join(ROOT, 'assets', 'aircraft_icons')
ICON_DIR = os.path.join(ROOT, 'docs', 'icons')
OUT = os.path.join(ROOT, 'docs', 'AIRCRAFT_ICONS.md')
TINT = '#3f9d5c'  # green, readable on GitHub's light and dark backgrounds

SECTION_GROUP = {
    'Airliners': 'Airliners and regional jets',
    'Regional turboprops': 'Regional turboprops',
    'Business jets': 'Business jets',
    'General aviation singles': 'Light aircraft: singles',
    'General aviation twins': 'Light aircraft: twins and utility',
    'Classic airliners and transports': 'Classic airliners and transports',
    'Warbirds and classic fighters': 'Warbirds and classic fighters',
    'Experimental and aerobatic': 'Experimental and aerobatic',
    'The amphibians above carry their propeller on a pylon over the wing.': 'Light aircraft: singles',
    'Classic jets': 'Military jets',
    'Bombers, tankers, transports, patrol': 'Military transports, bombers, tankers and patrol',
    'Helicopters: rotor diameter, length, body width, where the cabin ends, blades': 'Helicopters',
}
# The last catalogue section ("the busiest types") mixes kinds.
KEY_GROUP = {
    'b738': 'Airliners and regional jets', 'a320': 'Airliners and regional jets', 'b789': 'Airliners and regional jets',
    'crj9': 'Airliners and regional jets', 'b763': 'Airliners and regional jets', 'a333': 'Airliners and regional jets',
    'e195': 'Airliners and regional jets', 'e295': 'Airliners and regional jets', 'e145': 'Airliners and regional jets',
    'b744': 'Airliners and regional jets', 'a388': 'Airliners and regional jets', 'a343': 'Airliners and regional jets',
    'md11': 'Airliners and regional jets', 'f100': 'Airliners and regional jets',
    'c17': 'Military transports, bombers, tankers and patrol', 'c130': 'Military transports, bombers, tankers and patrol',
    'c27j': 'Military transports, bombers, tankers and patrol',
    'be20': 'Light aircraft: twins and utility', 'b190': 'Regional turboprops', 'p06t': 'Light aircraft: twins and utility',
    'fa7x': 'Business jets', 'glf5': 'Business jets', 'g150': 'Business jets', 'c650': 'Business jets', 'ea50': 'Business jets',
    'dh8a': 'Regional turboprops', 'cl415': 'Classic airliners and transports', 'sling': 'Light aircraft: singles',
}
ORDER = ['Airliners and regional jets', 'Regional turboprops', 'Business jets', 'Light aircraft: singles',
         'Light aircraft: twins and utility', 'Classic airliners and transports', 'Warbirds and classic fighters',
         'Experimental and aerobatic', 'Military jets', 'Military transports, bombers, tankers and patrol',
         'Helicopters', 'Families (shared shapes)', 'Other ADS-B Radar icons', 'Size classes (when the type is unknown)']


def svg_path(mask):
    for d in (ASSETS, os.path.join(ASSETS, 'generated')):
        p = os.path.join(d, mask + '.svg')
        if os.path.exists(p):
            return p
    return None


def tinted(svg):
    svg = re.sub(r'<\?xml[^>]*>\s*', '', svg)
    svg = svg.replace('fill="#000"', f'fill="{TINT}"').replace('stroke="#000"', f'stroke="{TINT}"')
    if 'fill="' + TINT not in svg:  # the ADS-B Radar icons: paths with the default (black) fill
        svg = re.sub(r'<svg\b', f'<svg fill="{TINT}"', svg, count=1)
        svg = re.sub(r'fill="(#000000|#000|black)"', f'fill="{TINT}"', svg)
    return svg


def main():
    h = open(os.path.join(ROOT, 'main', 'core', 'aircraft_types.h')).read()
    c = open(os.path.join(ROOT, 'main', 'core', 'aircraft_types.cpp')).read()
    icons = re.findall(r'^  (\w+),  // (.*)$', h, re.M)            # enum, description (in order)
    info = {e: (m, int(sz)) for m, sz, e in re.findall(r'\{"([a-z0-9_]+)", (\d+)\},  // (\w+)', c)}
    types = collections.defaultdict(list)
    for t, e in re.findall(r'\{"([A-Z0-9]+)", Icon::(\w+)\}', c):
        types[e].append(t)

    # Which catalogue section each x_ icon comes from.
    section, key_section = None, {}
    for line in open(os.path.join(ROOT, 'scripts', 'aircraft_catalog.py')):
        m = re.match(r'# (.+)$', line)
        if m and not line.startswith('# ---'):
            section = m.group(1).strip()
        m = re.match(r"add\('([a-z0-9_]+)'", line)
        if m:
            key_section[m.group(1)] = section

    groups = collections.defaultdict(list)
    for i, (e, desc) in enumerate(icons):
        mask, size = info[e]
        if mask.startswith('x_'):
            key = mask[2:]
            g = KEY_GROUP.get(key) or SECTION_GROUP.get(key_section.get(key, ''), 'Other')
        elif mask.startswith('g_'):
            g = 'Families (shared shapes)'
        elif i < 14:
            g = 'Size classes (when the type is unknown)'
        else:
            g = 'Other ADS-B Radar icons'
        groups[g].append((e, desc, mask, size, sorted(types.get(e, []))))

    os.makedirs(ICON_DIR, exist_ok=True)
    for f in os.listdir(ICON_DIR):
        if f.endswith('.svg'):
            os.remove(os.path.join(ICON_DIR, f))
    written = set()

    n_icons = sum(len(v) for v in groups.values())
    n_types = sum(len(v) for v in types.values())
    out = ['# Aircraft icons', '',
           f'The radar draws each aircraft with an icon for its type: **{n_icons} icons** for '
           f'**{n_types} ICAO type codes**. This page lists them all, generated from the firmware\'s own table '
           '(`scripts/gen_icon_docs.py`).', '',
           '**How an icon is picked.** The feed gives most aircraft an ICAO type designator (`B738`, `C172`, '
           '`H60`). If the code is in the table, that icon is used; close variants share one (737-800, MAX 8 and '
           'P-8 are one icon). If the feed has no type, or one not in the table, the aircraft gets a '
           '[size class](#size-classes-when-the-type-is-unknown) icon from its ADS-B emitter category (light, '
           'large, heavy, rotorcraft, glider...).', '',
           '**Size.** Icons are drawn at the sizes below (pixels on the 800 px screen at 100 % aircraft size), from '
           'the aircraft\'s real size: a Robinson R22 is small, an A380 big. *Settings > Display > Aircraft size* '
           'scales them all, and zoomed out with many aircraft a smaller set is used.', '',
           '**Credits.** Icons by ADS-B Radar for macOS - https://adsb-radar.com - '
           'https://apps.apple.com/app/id1538149835 (the size classes and a few others below). The rest are drawn '
           'in the same style from each aircraft\'s real dimensions by `scripts/aircraft_catalog.py` and '
           '`scripts/aircraft_families.py`.', '',
           'Missing a type, or a type with the wrong icon? The table is `scripts/gen_aircraft_types.py` and '
           '`scripts/aircraft_catalog.py`; `scripts/check_icon_coverage.py` lists live types without one.', '',
           '## Contents', '']
    for g in ORDER:
        if groups.get(g):
            anchor = re.sub(r'[^a-z0-9 -]', '', g.lower()).replace(' ', '-')
            out.append(f'- [{g}](#{anchor}) ({len(groups[g])})')
    out.append('')
    for g in ORDER + sorted(set(groups) - set(ORDER)):
        rows = groups.get(g)
        if not rows:
            continue
        out += [f'## {g}', '', '| Icon | Aircraft | Type codes | Size |', '|:---:|---|---|---:|']
        for e, desc, mask, size, tcodes in rows:
            src = svg_path(mask)
            if src and mask not in written:
                with open(os.path.join(ICON_DIR, mask + '.svg'), 'w') as f:
                    f.write(tinted(open(src).read()))
                written.add(mask)
            img = f'<img src="icons/{mask}.svg" width="48" height="48" alt="{mask}">' if src else ''
            codes = ' '.join(f'`{t}`' for t in tcodes) if tcodes else '*(by emitter category)*'
            out.append(f'| {img} | {desc} | {codes} | {size} px |')
        out.append('')
    open(OUT, 'w').write('\n'.join(out))
    print(f'{n_icons} icons, {n_types} types, {len(written)} images -> {os.path.relpath(OUT, ROOT)}')


if __name__ == '__main__':
    main()
