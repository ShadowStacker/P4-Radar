#!/usr/bin/env python3
"""The type-specific aircraft icons: one entry per aircraft (or close family),
drawn from its real span and length with a few templates (jets, propeller
aircraft, biplanes, twin booms, canard pushers, fighters, helicopters).

Each entry: key (the icon/mask name, "x_" + key), a description, the ICAO
type designators it's for, and the drawing. scripts/gen_aircraft_types.py
turns the list into the C++ type table; aircraft_families.py writes the SVGs.
Same style as the ADS-B Radar icons (https://adsb-radar.com): solid top-view
silhouettes, nose up; propellers and rotor blades bolder than life.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft_silhouette import Shape  # noqa: E402

CATALOG = []  # (key, description, types, builder)


def add(key, desc, types, fn):
    CATALOG.append((key, desc, types, fn))


def tan(d):
    return math.tan(math.radians(d))


# --- templates -------------------------------------------------------------------

def jet(span, L, fw, *, wpos=0.38, root=None, tip=None, sweep=27, pods=(0.34,), pod_len=None, pod_w=None,
        rear=0, rear_y=0.70, tpos=0.89, thalf=None, troot=None, tsweep=None, tip_tanks=0.0, nose=None, tail=None,
        tail_w=None, over_wing=False, fat=1.0, canard=None):
    s = Shape(span, L)
    s.fuselage(fw * fat, nose=nose or fw * 1.8, tail=tail or L * 0.22, tail_width=tail_w)
    root = root or span * 0.17
    tip = tip if tip is not None else root * 0.3
    wy = wpos * L
    # Swept wings get the straight inboard trailing edge of real airliner wings.
    s.wing(wy, root, span / 2, sweep, tip, kink=0.32 if sweep >= 18 and not rear else None)
    pod_len = pod_len or L * 0.11
    pod_w = pod_w or fw * 0.5
    for f in pods:
        x = f * span / 2
        le = wy + (x - fw / 2) * tan(sweep)
        y = le + root * 0.25 if over_wing else le - pod_len * 0.45
        s.jet_pod(x, y, pod_len, pod_w, pylon=not over_wing)
    if rear:
        xs = [fw / 2 + pod_w * 0.42] if rear in (2, 3) else [fw / 2 + pod_w * 0.42, fw / 2 + pod_w * 1.4]
        for x in xs:
            s.jet_pod(x, rear_y * L, pod_len, pod_w, pylon=False)  # on the fuselage
    thalf = thalf or span * 0.19
    troot = troot or L * 0.1
    s.wing(tpos * L - troot, troot, thalf, tsweep if tsweep is not None else sweep + 6, troot * 0.42)
    if tip_tanks:
        s.ellipse(span / 2 - tip_tanks * 0.15, wy + (span / 2 - fw / 2) * tan(sweep) + tip * 0.3, tip_tanks * 0.28,
                  tip_tanks, mirror=True)
    if canard:
        cy, chalf = canard
        s.wing(cy, chalf * 0.6, chalf, 30, chalf * 0.25)
    return s


def elliptic_wing(s, y_qc, root, half):
    """An elliptical wing (Spitfire): chord shrinking as an ellipse, quarter chord straight."""
    pts_le, pts_te = [], []
    n = 16
    for i in range(n + 1):
        x = half * i / n
        c = root * math.sqrt(max(0.0, 1 - (x / half) ** 2))
        c = max(c, root * 0.08)
        pts_le.append((x, y_qc - c * 0.25))
        pts_te.append((x, y_qc + c * 0.75))
    s.poly(pts_le + list(reversed(pts_te)), mirror=True)


def prop(span, L, fw, *, wpos=0.30, root=None, tip=None, sweep=0, nacelles=(), nac_len=None, nac_w=None,
         prop_d=None, nose_prop=None, pusher=False, tpos=0.90, thalf=None, troot=None, ttip=None, tsweep=4,
         fins=False, elliptic=False, tip_tanks=0.0, nose=None, tail=None, tail_w=None, vtail=False, fat=1.0,
         stab=True):
    s = Shape(span, L)
    s.fuselage(fw * fat, nose=nose or fw * 0.9, tail=tail or L * 0.32, tail_width=tail_w)
    root = root or span * 0.15
    tip = tip if tip is not None else root * 0.65
    wy = wpos * L
    if elliptic:
        elliptic_wing(s, wy + root * 0.25, root, span / 2)
    else:
        s.wing(wy, root, span / 2, sweep, tip)
    nac_len = nac_len or L * 0.2
    nac_w = nac_w or fw * 0.6
    pd = prop_d or span * 0.16
    for x in nacelles:
        y = wy + (x - fw / 2) * tan(sweep) - nac_len * 0.35
        s.nacelle(x, y, nac_len, nac_w)
        s.prop(x, y - 0.05, pd)
    if nose_prop if nose_prop is not None else not nacelles and not pusher:
        s.prop(0, 0.1, pd, mirror=False)
    if pusher:
        s.prop(0, L - 0.1, pd, mirror=False)
    if stab:
        thalf = thalf or span * 0.19
        troot = troot or L * 0.11
        ttip = ttip if ttip is not None else troot * 0.7
        if vtail:
            s.wing(tpos * L - troot, troot, thalf * 0.85, 30, ttip * 0.8)
        else:
            s.wing(tpos * L - troot, troot, thalf, tsweep, ttip)
        if fins:
            s.poly([(thalf - 0.05 * thalf, tpos * L - troot * 1.15), (thalf + 0.06 * thalf, tpos * L - troot * 1.15),
                    (thalf + 0.06 * thalf, tpos * L + troot * 0.1), (thalf - 0.05 * thalf, tpos * L + troot * 0.1)],
                   mirror=True)
    if tip_tanks:
        s.ellipse(span / 2, wy + root * 0.35, tip_tanks * 0.3, tip_tanks, mirror=True)
    return s


def biplane(span, L, fw, *, chord=None, lower=0.9, stagger=0.5, tpos=0.92, thalf=None, nacelles=(), prop_d=None):
    s = Shape(span, L)
    s.fuselage(fw, nose=fw * 0.8, tail=L * 0.35, tail_width=fw * 0.25)
    c = chord or span * 0.15
    wy = L * 0.18
    s.wing(wy, c, span / 2, 0, c)
    s.wing(wy + c * stagger, c * 0.9, span / 2 * lower, 0, c * 0.9)
    for x in nacelles:
        s.nacelle(x, wy - L * 0.08, L * 0.2, fw * 0.6)
        s.prop(x, wy - L * 0.08, prop_d or span * 0.14)
    if not nacelles:
        s.prop(0, 0.1, prop_d or span * 0.22, mirror=False)
    thalf = thalf or span * 0.2
    s.wing(tpos * L - L * 0.12, L * 0.12, thalf, 6, L * 0.08)
    return s


def twin_boom(span, L, *, boom_x, boom_len, boom_w, pod_w, pod_len, wpos=0.35, root=None, tip=None, props='front',
              prop_d=None, stab_y=None, stab_chord=None, jets=False):
    """P-38, OV-10, Vampire, Cessna 337: two tail booms, a stabilizer between them."""
    s = Shape(span, L)
    s.fuselage(pod_w, start=0.0, end=pod_len, nose=pod_w * 1.1, tail=pod_len * 0.3, tail_width=pod_w * 0.5)
    root = root or span * 0.17
    tip = tip if tip is not None else root * 0.55
    wy = wpos * L
    s.wing(wy, root, span / 2, 0, tip)
    # booms as long ellipses
    s.ellipse(boom_x, L - boom_len / 2, boom_w / 2, boom_len / 2, mirror=True)
    sc = stab_chord or L * 0.1
    sy = stab_y if stab_y is not None else L - sc
    s.poly([(0, sy), (boom_x + boom_w * 0.4, sy), (boom_x + boom_w * 0.4, sy + sc), (0, sy + sc)], mirror=True,
           round_r=sc * 0.3)
    pd = prop_d or span * 0.2
    if props in ('front', 'both') and not jets:
        s.prop(boom_x, L - boom_len - 0.05, pd)
    if props in ('pusher', 'both') and not jets:
        s.prop(0, pod_len + 0.05, pd, mirror=False)
    if props == 'nose':
        s.prop(0, 0.1, pd, mirror=False)
    return s


def canard_pusher(span, L, fw, *, wing_y, root, tip, sweep, canard_y, canard_half, prop_d=None, winglets=True,
                  twin_engines=None):
    """Long-EZ, VariEze, Velocity, Avanti-like: canard in front, wing and pusher prop at the back."""
    s = Shape(span, L)
    s.fuselage(fw, nose=fw * 1.6, tail=L * 0.25, tail_width=fw * 0.4)
    s.wing(wing_y, root, span / 2, sweep, tip)
    s.wing(canard_y, canard_half * 0.28, canard_half, 0, canard_half * 0.22)
    if winglets:
        x = span / 2
        y = wing_y + (x - fw / 2) * tan(sweep)
        s.poly([(x - span * 0.015, y - tip * 0.3), (x + span * 0.015, y - tip * 0.3), (x + span * 0.015, y + tip),
                (x - span * 0.015, y + tip)], mirror=True, round_r=span * 0.012)
    pd = prop_d or span * 0.2
    if twin_engines:
        x, ny, nl = twin_engines
        s.nacelle(x, ny, nl, fw * 0.5)
        s.prop(x, ny + nl, pd)
    else:
        s.prop(0, L - 0.1, pd, mirror=False)
    return s


def fighter(span, L, fw, *, wpos, root, tip, sweep, stab=True, fins=0, canard=None, lex=0.0, tail_w=None,
            stab_half=None, stab_root=None, nose=None, delta=False, tip_tanks=0.0, inlets=False):
    s = Shape(span, L)
    s.fuselage(fw, nose=nose or L * 0.2, tail=L * 0.12, tail_width=tail_w or fw * 0.6)
    s.wing(wpos * L, root, span / 2, sweep, tip)
    if lex:
        s.poly([(fw / 2 + lex * 0.15, L * 0.22), (fw / 2 * 0.9, L * 0.22), (fw / 2 + lex, wpos * L + 0.3)], mirror=True)
    if inlets:
        s.poly([(fw / 2, L * 0.25), (fw / 2 + fw * 0.3, L * 0.27), (fw / 2 + fw * 0.3, wpos * L), (fw / 2, wpos * L)],
               mirror=True)
    if stab:
        sh = stab_half or span * 0.3
        sr = stab_root or L * 0.15
        s.wing(L * 0.97 - sr, sr, sh, sweep + 5, sr * 0.35)
    if fins:
        fx = fins
        y0 = L * 0.72
        s.poly([(fx, y0), (fx + 0.25, y0), (fx + 0.6, L * 0.93), (fx + 0.3, L * 0.95)], mirror=True, round_r=0.15)
    if canard:
        cy, ch = canard
        s.wing(cy, ch * 0.7, ch, 45, ch * 0.25)
    if tip_tanks:
        s.ellipse(span / 2, wpos * L + root * 0.4, tip_tanks * 0.28, tip_tanks, mirror=True)
    return s


def heli(rotor_d, length, body_w, body_end, blades, *, stab=1.0, offset=40, fenestron=False, stubs=0.0, boom_w=None,
         tail_rotor=True):
    s = Shape(rotor_d, length)
    r = rotor_d / 2
    body_start = max(0.0, r - (body_end - r) * 0.55)
    s.fuselage(body_w, start=body_start, end=body_end, nose=body_w * 0.8, tail=body_w * 1.4, tail_width=body_w * 0.4)
    bw = boom_w or body_w * 0.3
    s.fuselage(bw, start=body_end - body_w, end=length - 0.2, nose=0.1, tail=0.5,
               tail_width=bw * (2.6 if fenestron else 0.8))
    s.wing(length - 1.4, 0.6, stab, 0, 0.45)
    if stubs:
        s.poly([(body_w * 0.4, r - 0.3), (body_w * 0.4 + stubs, r - 0.1), (body_w * 0.4 + stubs, r + 0.9),
                (body_w * 0.4, r + 0.9)], mirror=True, round_r=0.3)
    if tail_rotor and not fenestron:
        s.poly([(bw * 0.5, length - 1.0), (bw * 0.5 + 0.3, length - 1.0), (bw * 0.5 + 0.3, length + 0.0),
                (bw * 0.5, length + 0.0)], round_r=0.12)
    s.rotor(0, r, r, blades, rotor_d * 0.04)  # angles by blade count (see Shape.rotor)
    return s


# --- the catalogue -------------------------------------------------------------------
# Dimensions in metres (span, length, fuselage width), from the type's published data.

# Airliners
add('a318', 'Airbus A318', 'A318', lambda: jet(34.1, 31.4, 3.95, sweep=25))
add('a319', 'Airbus A319', 'A319 A19N', lambda: jet(35.8, 33.8, 3.95, sweep=25))
add('a321', 'Airbus A321', 'A321 A21N', lambda: jet(35.8, 44.5, 3.95, sweep=25, wpos=0.40))
add('a220_100', 'Airbus A220-100', 'BCS1', lambda: jet(35.1, 35.0, 3.5, sweep=25))
add('a220_300', 'Airbus A220-300', 'BCS3', lambda: jet(35.1, 38.7, 3.5, sweep=25))
add('c919', 'COMAC C919', 'C919', lambda: jet(35.8, 38.9, 3.96, sweep=25))
add('b732', 'Boeing 737-200', 'B731 B732', lambda: jet(28.4, 30.5, 3.76, sweep=25, pods=(0.28,), pod_len=4.5, pod_w=1.2))
add('b733', 'Boeing 737-300', 'B733', lambda: jet(28.9, 33.4, 3.76, sweep=25, pods=(0.32,), pod_w=1.8))
add('b734', 'Boeing 737-400', 'B734', lambda: jet(28.9, 36.4, 3.76, sweep=25, pods=(0.32,), pod_w=1.8))
add('b735', 'Boeing 737-500', 'B735', lambda: jet(28.9, 31.0, 3.76, sweep=25, pods=(0.32,), pod_w=1.8))
add('b736', 'Boeing 737-600', 'B736', lambda: jet(34.3, 31.2, 3.76, sweep=25))
add('b737', 'Boeing 737-700 / MAX 7', 'B737 B37M E737', lambda: jet(34.3, 33.6, 3.76, sweep=25))
add('b739', 'Boeing 737-900 / MAX 9 / MAX 10', 'B739 B39M B3XM', lambda: jet(35.9, 42.1, 3.76, sweep=25, wpos=0.40))
add('b752', 'Boeing 757-200', 'B752', lambda: jet(38.05, 47.3, 3.76, sweep=25, wpos=0.37))
add('b753', 'Boeing 757-300', 'B753', lambda: jet(38.05, 54.4, 3.76, sweep=25, wpos=0.40))
add('b762', 'Boeing 767-200', 'B762', lambda: jet(47.6, 48.5, 5.03, sweep=31))
add('b764', 'Boeing 767-400', 'B764', lambda: jet(51.9, 61.4, 5.03, sweep=31, wpos=0.40))
add('b772', 'Boeing 777-200', 'B772 B77L', lambda: jet(60.9, 63.7, 6.2, sweep=31))
add('b773', 'Boeing 777-300', 'B773', lambda: jet(60.9, 73.9, 6.2, sweep=31, wpos=0.40))
add('b77w', 'Boeing 777-300ER', 'B77W', lambda: jet(64.8, 73.9, 6.2, sweep=31, wpos=0.40, tip=2.0))
add('b779', 'Boeing 777X', 'B778 B779', lambda: jet(71.8, 76.7, 6.2, sweep=31, wpos=0.40, tip=1.6))
add('b788', 'Boeing 787-8', 'B788', lambda: jet(60.1, 56.7, 5.77, sweep=32, tip=1.5))
add('b78x', 'Boeing 787-10', 'B78X', lambda: jet(60.1, 68.3, 5.77, sweep=32, tip=1.5, wpos=0.40))
add('a300', 'Airbus A300', 'A306 A30B', lambda: jet(44.8, 54.1, 5.64, sweep=28))
add('a310', 'Airbus A310', 'A310', lambda: jet(43.9, 46.7, 5.64, sweep=28))
add('a332', 'Airbus A330-200', 'A332 A337', lambda: jet(60.3, 58.8, 5.64, sweep=30))
add('a339', 'Airbus A330neo', 'A338 A339', lambda: jet(64.0, 63.7, 5.64, sweep=30, tip=2.2))
add('a359', 'Airbus A350-900', 'A359', lambda: jet(64.75, 66.8, 5.96, sweep=31, tip=1.8))
add('a35k', 'Airbus A350-1000', 'A35K', lambda: jet(64.75, 73.8, 5.96, sweep=31, tip=1.8, wpos=0.40))
add('a346', 'Airbus A340-500/600', 'A345 A346', lambda: jet(63.5, 75.4, 5.64, sweep=30, wpos=0.40, pods=(0.3, 0.6)))
add('b748', 'Boeing 747-8', 'B748', lambda: jet(68.4, 76.3, 6.5, sweep=37, pods=(0.32, 0.62)))
add('b74s', 'Boeing 747SP', 'B74S', lambda: jet(59.6, 56.3, 6.5, sweep=37, pods=(0.32, 0.62), wpos=0.33))
add('beluga', 'Airbus Beluga', 'A3ST', lambda: jet(44.8, 56.2, 5.6, sweep=28, fat=1.7, nose=5.0))
add('belugaxl', 'Airbus BelugaXL', 'A337', lambda: jet(60.3, 63.1, 5.6, sweep=30, fat=1.8, nose=6.0))
add('blcf', 'Boeing Dreamlifter', 'BLCF', lambda: jet(64.4, 71.7, 6.5, sweep=37, pods=(0.32, 0.62), fat=1.4))
add('dc10', 'McDonnell Douglas DC-10 / KC-10', 'DC10', lambda: jet(50.4, 55.5, 6.0, sweep=35, pods=(0.36,)))
add('l1011', 'Lockheed L-1011 TriStar', 'L101', lambda: jet(47.4, 54.2, 5.97, sweep=35, pods=(0.38,), rear=2, rear_y=0.78))
add('b712', 'Boeing 717', 'B712', lambda: jet(28.4, 37.8, 3.34, sweep=24, pods=(), rear=2, wpos=0.42, tpos=0.95))
add('md80', 'McDonnell Douglas MD-80', 'MD81 MD82 MD83 MD87 MD88', lambda: jet(32.8, 45.1, 3.34, sweep=24, pods=(), rear=2, wpos=0.44, tpos=0.95))
add('md90', 'McDonnell Douglas MD-90', 'MD90', lambda: jet(32.9, 46.5, 3.34, sweep=24, pods=(), rear=2, wpos=0.44, tpos=0.95, pod_w=2.0))
add('dc9', 'Douglas DC-9', 'DC91 DC92 DC93 DC94 DC95', lambda: jet(28.5, 36.4, 3.34, sweep=24, pods=(), rear=2, wpos=0.42, tpos=0.95))
add('f100', 'Fokker 100', 'F100', lambda: jet(28.1, 35.5, 3.3, sweep=17, pods=(), rear=2, wpos=0.44, tpos=0.95))
add('f70', 'Fokker 70', 'F70', lambda: jet(28.1, 30.9, 3.3, sweep=17, pods=(), rear=2, wpos=0.42, tpos=0.95))
add('b722', 'Boeing 727-200', 'B722', lambda: jet(32.9, 46.7, 3.76, sweep=32, pods=(), rear=3, wpos=0.38, tpos=0.95))
add('b721', 'Boeing 727-100', 'B721', lambda: jet(32.9, 40.6, 3.76, sweep=32, pods=(), rear=3, wpos=0.36, tpos=0.95))
add('bae146', 'BAe 146 / Avro RJ85', 'B461 B462 RJ70 RJ85', lambda: jet(26.3, 28.6, 3.56, sweep=15, pods=(0.3, 0.55), wpos=0.36))
add('bae146_300', 'BAe 146-300 / Avro RJ100', 'B463 RJ1H', lambda: jet(26.3, 31.0, 3.56, sweep=15, pods=(0.3, 0.55), wpos=0.38))
add('t154', 'Tupolev Tu-154', 'T154', lambda: jet(37.6, 47.9, 3.8, sweep=35, pods=(), rear=3, wpos=0.40, tpos=0.95))
add('il62', 'Ilyushin Il-62', 'IL62', lambda: jet(43.2, 53.1, 3.75, sweep=35, pods=(), rear=4, wpos=0.40, tpos=0.95))
add('yk42', 'Yakovlev Yak-42', 'YK42', lambda: jet(34.9, 36.4, 3.8, sweep=23, pods=(), rear=3, wpos=0.40, tpos=0.95))
add('crj2', 'Bombardier CRJ100/200', 'CRJ1 CRJ2', lambda: jet(21.2, 26.8, 2.7, sweep=26, pods=(), rear=2, wpos=0.42, tpos=0.95))
add('crj7', 'Bombardier CRJ700', 'CRJ7', lambda: jet(23.2, 32.3, 2.7, sweep=26, pods=(), rear=2, wpos=0.44, tpos=0.95))
add('e170', 'Embraer 170', 'E170', lambda: jet(26.0, 29.9, 3.0, sweep=23))
add('e175', 'Embraer 175', 'E175 E75L E75S', lambda: jet(26.0, 31.7, 3.0, sweep=23))
add('e190', 'Embraer 190', 'E190', lambda: jet(28.7, 36.2, 3.0, sweep=23))
add('e290', 'Embraer E190-E2', 'E290', lambda: jet(33.7, 36.2, 3.0, sweep=23, tip=1.2))
add('e135', 'Embraer ERJ 135 / Legacy', 'E135 E35L', lambda: jet(20.0, 26.3, 2.28, sweep=23, pods=(), rear=2, wpos=0.42, tpos=0.95))
add('su95', 'Sukhoi Superjet 100', 'SU95', lambda: jet(27.8, 29.9, 3.46, sweep=25))
add('arj21', 'COMAC ARJ21', 'AJ27', lambda: jet(27.3, 33.5, 3.14, sweep=25, pods=(), rear=2, wpos=0.42, tpos=0.95))

# Regional turboprops
add('dh8c', 'Dash 8-300', 'DH8C', lambda: prop(27.4, 25.7, 2.7, wpos=0.34, nacelles=(4.1,), tpos=0.97, thalf=4.0, troot=2.0, tail=8.0))
add('dh8d', 'Dash 8 Q400', 'DH8D', lambda: prop(28.4, 32.8, 2.7, wpos=0.38, nacelles=(4.2,), tpos=0.97, thalf=4.0, troot=2.0, tail=9.0, prop_d=4.1))
add('atr42', 'ATR 42', 'AT43 AT44 AT45 AT46', lambda: prop(24.6, 22.7, 2.57, wpos=0.36, nacelles=(4.1,), tpos=0.97, thalf=3.6, troot=1.8, tail=7.0))
add('atr72', 'ATR 72', 'AT72 AT73 AT75 AT76', lambda: prop(27.1, 27.2, 2.57, wpos=0.38, nacelles=(4.1,), tpos=0.97, thalf=3.6, troot=1.8, tail=8.0))
add('sf34', 'Saab 340', 'SF34', lambda: prop(21.4, 19.7, 2.3, wpos=0.36, nacelles=(3.2,), tpos=0.97, tsweep=20, tail=6.0))
add('sb20', 'Saab 2000', 'SB20', lambda: prop(24.8, 27.3, 2.3, wpos=0.40, nacelles=(3.5,), tpos=0.97, tsweep=20, tail=8.0, prop_d=3.8))
add('d328', 'Dornier 328', 'D328', lambda: prop(21.0, 21.3, 2.4, wpos=0.34, nacelles=(3.0,), tpos=0.97, thalf=3.0, troot=1.6, tail=7.0))
add('j328', 'Dornier 328JET', 'J328', lambda: jet(21.0, 21.3, 2.4, sweep=4, pods=(0.28,), wpos=0.34, root=2.6, tip=1.2, tpos=0.97))
add('e120', 'Embraer Brasilia', 'E120', lambda: prop(19.8, 20.0, 2.28, wpos=0.36, nacelles=(3.0,), tpos=0.97, thalf=3.3, troot=1.6, tail=6.5))
add('js41', 'Jetstream 41', 'JS41', lambda: prop(18.4, 19.3, 1.98, wpos=0.40, nacelles=(2.8,), tpos=0.97, tail=6.0))
add('f50', 'Fokker 50 / F27', 'F27 F50', lambda: prop(29.0, 25.3, 2.7, wpos=0.36, nacelles=(3.9,), tail=7.0))
add('dhc7', 'Dash 7', 'DHC7', lambda: prop(28.4, 24.6, 2.6, wpos=0.34, nacelles=(3.2, 6.6), tpos=0.97, thalf=4.0, troot=2.2, tail=7.5))
add('an26', 'Antonov An-24/26/32, MA60', 'AN24 AN26 AN32 MA60', lambda: prop(29.2, 23.5, 2.9, wpos=0.34, nacelles=(3.9,), tail=7.0))
add('c295', 'CASA C-295', 'C295', lambda: prop(25.8, 24.5, 2.9, wpos=0.34, nacelles=(3.5,), tail=7.5, thalf=4.0))
add('cn235', 'CASA CN-235', 'CN35', lambda: prop(25.8, 21.4, 2.9, wpos=0.34, nacelles=(3.5,), tail=6.5, thalf=4.0))

# Business jets
def bizjet(span, L, fw, sweep=20, **kw):
    kw.setdefault('pods', ())
    kw.setdefault('rear', 2)
    kw.setdefault('wpos', 0.40)
    kw.setdefault('tpos', 0.95)
    kw.setdefault('pod_len', L * 0.16)
    kw.setdefault('pod_w', fw * 0.55)
    kw.setdefault('root', span * 0.17)
    kw.setdefault('tail', L * 0.28)
    return jet(span, L, fw, sweep=sweep, **kw)


add('c510', 'Citation Mustang', 'C510', lambda: bizjet(13.2, 12.4, 1.55, sweep=4, tip=0.9))
add('c525', 'CitationJet CJ1/M2', 'C525 C526 C25M', lambda: bizjet(14.3, 13.0, 1.55, sweep=4, tip=0.9))
add('c25a', 'Citation CJ2', 'C25A', lambda: bizjet(15.1, 14.3, 1.55, sweep=4, tip=0.9))
add('c25b', 'Citation CJ3', 'C25B', lambda: bizjet(16.3, 15.6, 1.55, sweep=4, tip=0.9))
add('c25c', 'Citation CJ4', 'C25C', lambda: bizjet(15.5, 16.3, 1.55, sweep=13, tip=0.8))
add('c500', 'Citation I/II/Bravo', 'C500 C501 C550 C551 C55B', lambda: bizjet(15.9, 14.4, 1.6, sweep=2, tip=1.0))
add('c560', 'Citation V/Ultra/Encore', 'C560', lambda: bizjet(15.9, 14.9, 1.6, sweep=2, tip=1.0))
add('c56x', 'Citation Excel/XLS', 'C56X', lambda: bizjet(17.2, 15.8, 1.7, sweep=2, tip=1.0))
add('c680', 'Citation Sovereign', 'C680', lambda: bizjet(19.3, 19.4, 1.7, sweep=16, tip=1.1))
add('c68a', 'Citation Latitude', 'C68A', lambda: bizjet(22.0, 19.0, 1.95, sweep=16, tip=1.1))
add('c700', 'Citation Longitude', 'C700', lambda: bizjet(20.8, 22.3, 1.95, sweep=27, tip=1.0))
add('c750', 'Citation X', 'C750', lambda: bizjet(19.4, 22.0, 1.7, sweep=37, tip=0.9))
add('lj35', 'Learjet 31/35/36', 'LJ31 LJ35 LJ36', lambda: bizjet(12.0, 14.8, 1.6, sweep=13, tip=1.0, tip_tanks=1.8))
add('lj45', 'Learjet 40/45/70/75', 'LJ40 LJ45 LJ70 LJ75', lambda: bizjet(14.6, 17.7, 1.6, sweep=13, tip=1.0))
add('lj60', 'Learjet 55/60', 'LJ55 LJ60', lambda: bizjet(13.4, 17.9, 1.75, sweep=13, tip=1.0))
add('e50p', 'Phenom 100', 'E50P', lambda: bizjet(12.3, 12.8, 1.6, sweep=10, tip=0.8))
add('e55p', 'Phenom 300', 'E55P', lambda: bizjet(16.2, 15.9, 1.6, sweep=12, tip=0.9))
add('e545', 'Praetor 500 / Legacy 450', 'E545', lambda: bizjet(20.3, 19.7, 2.0, sweep=20, tip=1.0))
add('e550', 'Praetor 600 / Legacy 500', 'E550', lambda: bizjet(21.5, 20.7, 2.0, sweep=20, tip=1.0))
add('hdjt', 'HondaJet', 'HDJT', lambda: jet(12.1, 13.0, 1.5, sweep=4, pods=(0.42,), over_wing=True, wpos=0.40, root=2.0, tip=0.8, tpos=0.95, pod_len=2.4, pod_w=0.85))
add('sf50', 'Cirrus Vision Jet', 'SF50', lambda: prop(11.8, 9.4, 1.6, wpos=0.34, root=1.7, tip=0.8, nose_prop=False, vtail=True, tpos=0.98))
add('pc24', 'Pilatus PC-24', 'PC24', lambda: bizjet(17.0, 16.9, 1.7, sweep=12, tip=1.0))
add('be40', 'Beechjet 400 / Hawker 400', 'BE40', lambda: bizjet(13.3, 14.8, 1.6, sweep=20, tip=0.9))
add('prm1', 'Beech Premier', 'PRM1', lambda: bizjet(13.6, 14.0, 1.7, sweep=20, tip=0.9))
add('h25b', 'Hawker 800/900', 'H25A H25B H25C', lambda: bizjet(15.7, 15.6, 1.8, sweep=20, tip=1.0))
add('g280', 'Gulfstream G280', 'G280', lambda: bizjet(19.2, 20.4, 2.1, sweep=27, tip=1.0))
add('glf4', 'Gulfstream IV/G450', 'GLF4', lambda: bizjet(23.7, 27.2, 2.4, sweep=27, tip=1.1))
add('glf6', 'Gulfstream G650', 'GLF6', lambda: bizjet(30.4, 30.4, 2.6, sweep=36, tip=1.2))
add('ga7c', 'Gulfstream G700/G800/G500/G600', 'GA5C GA6C GA7C GA8C', lambda: bizjet(31.4, 33.5, 2.6, sweep=36, tip=1.2))
add('glex', 'Bombardier Global Express/6000', 'GLEX', lambda: bizjet(28.7, 30.3, 2.5, sweep=35, tip=1.2))
add('gl5t', 'Bombardier Global 5000/5500', 'GL5T', lambda: bizjet(28.7, 29.5, 2.5, sweep=35, tip=1.2))
add('gl7t', 'Bombardier Global 7500/8000', 'GL7T GL8T', lambda: bizjet(31.7, 33.8, 2.5, sweep=35, tip=1.2))
add('cl30', 'Challenger 300/350', 'CL30 CL35', lambda: bizjet(19.5, 20.9, 2.2, sweep=27, tip=1.0))
add('cl60', 'Challenger 600/650', 'CL60', lambda: bizjet(19.6, 20.9, 2.5, sweep=25, tip=1.1))
add('fa8x', 'Falcon 8X', 'FA8X', lambda: bizjet(26.3, 24.5, 2.3, sweep=34, rear=3, tip=1.0))
add('f2th', 'Falcon 2000', 'F2TH', lambda: bizjet(21.4, 20.2, 2.3, sweep=24, tip=1.0))
add('fa50', 'Falcon 50/900', 'FA50 F900', lambda: bizjet(19.3, 20.2, 2.3, sweep=24, rear=3, tip=1.0))
add('fa6x', 'Falcon 6X', 'FA6X', lambda: bizjet(25.9, 25.7, 2.6, sweep=33, tip=1.1))
add('fa20', 'Falcon 10/20', 'FA10 FA20', lambda: bizjet(16.3, 17.2, 2.0, sweep=30, tip=1.0))

# General aviation singles
def high_single(span, L, fw=1.15, **kw):
    kw.setdefault('wpos', 0.28)
    kw.setdefault('root', 1.6)
    kw.setdefault('tip', 1.15)
    kw.setdefault('tail', L * 0.42)
    kw.setdefault('tail_w', 0.3)
    return prop(span, L, fw, **kw)


def low_single(span, L, fw=1.2, **kw):
    kw.setdefault('wpos', 0.32)
    kw.setdefault('root', 1.65)
    kw.setdefault('tip', 1.25)
    kw.setdefault('tail', L * 0.36)
    return prop(span, L, fw, **kw)


add('c152', 'Cessna 150/152', 'C150 C152', lambda: high_single(10.2, 7.3))
add('c172', 'Cessna 172', 'C172 C72R', lambda: high_single(11.0, 8.3))
add('c182', 'Cessna 182/206', 'C182 C206 C205 C207 T206 C82R C82S C82T', lambda: high_single(11.0, 8.8, fw=1.25))
add('c210', 'Cessna 210', 'C210 P210 T210', lambda: high_single(11.2, 8.6, fw=1.25, tip=1.0))
add('c177', 'Cessna Cardinal', 'C177 C77R', lambda: high_single(10.8, 8.4, fw=1.2, tsweep=0))
add('c180', 'Cessna 180/185/170 (taildragger)', 'C180 C185 C170 C195', lambda: high_single(11.0, 7.9))
add('pa18', 'Piper Super Cub', 'PA18 PA20 PA22 PA12 PA14', lambda: high_single(10.7, 6.9, fw=1.0, root=1.6, tip=1.6))
add('j3', 'Piper J-3 Cub', 'J3', lambda: high_single(10.7, 6.8, fw=0.9, root=1.6, tip=1.6, thalf=1.6))
add('bl8', 'Citabria / Decathlon', 'CH7A CH7B BL8', lambda: high_single(10.5, 6.9, fw=1.0, root=1.5, tip=1.5))
add('husk', 'Aviat Husky', 'HUSK', lambda: high_single(10.8, 7.0, fw=1.0, root=1.6, tip=1.5))
add('m7', 'Maule', 'M5 M6 M7 MT7', lambda: high_single(10.2, 7.2, fw=1.1, root=1.6, tip=1.5))
add('ga8', 'GippsAero Airvan', 'GA8', lambda: high_single(12.4, 8.9, fw=1.35, root=1.7, tip=1.5))
add('pa32', 'Piper Saratoga/Cherokee Six', 'PA32 P32R P32T', lambda: low_single(11.0, 8.4, fw=1.3))
add('pa38', 'Piper Tomahawk', 'PA38', lambda: low_single(10.4, 7.0, fw=1.15, root=1.25, tip=1.25, tpos=0.95))
add('pa24', 'Piper Comanche', 'PA24', lambda: low_single(11.0, 7.6, fw=1.2, root=1.8, tip=1.0))
add('pa46', 'Piper Malibu/Meridian/M500-M700', 'PA46 P46T M700', lambda: low_single(13.1, 8.8, fw=1.35, root=1.9, tip=1.1))
add('sr22', 'Cirrus SR20/SR22', 'SR20 SR22 S22T', lambda: low_single(11.7, 7.9, fw=1.3, root=1.5, tip=0.85))
add('da40', 'Diamond DA40', 'DA40', lambda: low_single(11.9, 8.0, fw=1.15, root=1.4, tip=0.8, tpos=0.97, tail=3.6, tail_w=0.25))
add('da20', 'Diamond DA20', 'DA20 DV20', lambda: low_single(10.9, 7.2, fw=1.1, root=1.4, tip=0.8, tpos=0.97, tail=3.3, tail_w=0.25))
add('be35', 'Beech Bonanza V-tail', 'BE35', lambda: low_single(10.2, 8.1, fw=1.25, root=2.0, tip=1.0, vtail=True))
add('be36', 'Beech Bonanza A36', 'BE33 BE36 BT36', lambda: low_single(10.2, 8.4, fw=1.25, root=2.0, tip=1.0))
add('m20', 'Mooney M20', 'M20P M20T M20J M20K M20R', lambda: low_single(11.0, 8.1, fw=1.15, root=1.7, tip=1.0, tsweep=-12))
add('rv7', "Van's RV-6/7/8/9/14", 'RV4 RV6 RV7 RV8 RV9 RV14', lambda: low_single(7.7, 6.2, fw=1.05, root=1.4, tip=1.4))
add('rv10', "Van's RV-10", 'RV10', lambda: low_single(9.6, 7.4, fw=1.2, root=1.5, tip=1.5))
add('rv12', "Van's RV-12, Tecnam Sierra, JMB VL3", 'RV12 SIRA VL3', lambda: low_single(8.1, 6.0, fw=1.15, root=1.4, tip=1.4))
add('lnc4', 'Lancair IV / Evolution', 'LNC4 EVOT LNC2', lambda: low_single(9.2, 7.9, fw=1.2, root=1.6, tip=0.8))
add('pa28', 'Piper PA-28 Cherokee/Warrior/Archer/Arrow', 'PA28 P28A P28B P28R P28T P28U', lambda: low_single(10.7, 7.3))
add('tb20', 'Socata Trinidad/Tobago', 'TB9 TB10 TB20 TB21 TOBA', lambda: low_single(9.8, 7.7, fw=1.25))
add('dr40', 'Robin DR400', 'DR40', lambda: low_single(8.7, 6.96, fw=1.15, root=1.7, tip=1.0))
add('g115', 'Grob 115/120', 'G115 G120', lambda: low_single(10.0, 7.6, fw=1.1, root=1.5, tip=1.0))
add('aa5', 'Grumman Tiger/Traveler, Globe Swift', 'AA5 AA1 GC1', lambda: low_single(9.6, 6.9, fw=1.15))
add('tbm9', 'Daher TBM 700/850/900, Epic', 'TBM7 TBM8 TBM9 EPIC', lambda: low_single(12.8, 10.7, fw=1.5, root=2.0, tip=1.0, wpos=0.34, prop_d=2.3))
add('kodi', 'Quest Kodiak', 'KODI', lambda: high_single(13.7, 10.4, fw=1.5, root=1.9, tip=1.7, prop_d=2.4))
add('pc6', 'Pilatus Porter', 'PC6T PC6P', lambda: high_single(15.9, 11.0, fw=1.3, root=1.9, tip=1.9, prop_d=2.6))
add('dhc2', 'DHC-2 Beaver', 'DHC2', lambda: high_single(14.6, 9.2, fw=1.4, root=1.6, tip=1.6, prop_d=2.6))
add('dhc3', 'DHC-3 Otter', 'DHC3 DH3T', lambda: high_single(17.7, 12.8, fw=1.6, root=1.95, tip=1.95, prop_d=3.0))
add('c208', 'Cessna Caravan', 'C208', lambda: high_single(15.9, 11.5, fw=1.7, root=2.0, tip=1.3, prop_d=2.7))
add('pc12', 'Pilatus PC-12', 'PC12', lambda: low_single(16.3, 14.4, fw=1.6, root=2.4, tip=1.1, wpos=0.36, tpos=0.97, prop_d=2.7, tail=5.0))
add('at8t', 'Air Tractor (crop duster)', 'AT3T AT4T AT5T AT6T AT8T AT01', lambda: low_single(15.8, 10.9, fw=1.3, root=2.0, tip=2.0, wpos=0.33, prop_d=2.8))
add('pa25', 'Piper Pawnee', 'PA25 PA36', lambda: low_single(11.0, 7.5, fw=1.1, root=1.6, tip=1.6))

# General aviation twins
def light_twin(span, L, fw=1.25, nac=1.95, **kw):
    kw.setdefault('wpos', 0.34)
    kw.setdefault('root', 1.75)
    kw.setdefault('tip', 1.1)
    kw.setdefault('nac_len', L * 0.28)
    kw.setdefault('nac_w', 0.75)
    kw.setdefault('tail', L * 0.36)
    return prop(span, L, fw, nacelles=(nac,), **kw)


add('pa23', 'Piper Aztec/Apache', 'PA23', lambda: light_twin(11.3, 9.5, nac=2.0))
add('pa34', 'Piper Seneca', 'PA34', lambda: light_twin(11.9, 8.7, nac=2.0))
add('pa44', 'Piper Seminole', 'PA44', lambda: light_twin(11.8, 8.4, nac=1.9, tpos=0.97))
add('be58', 'Beech Baron', 'BE55 BE58 BE95 B58T', lambda: light_twin(11.5, 9.1))
add('be76', 'Beech Duchess', 'BE76', lambda: light_twin(11.6, 8.9, nac=1.9, tpos=0.97))
add('c310', 'Cessna 310/320', 'C303 C310 C320', lambda: light_twin(11.3, 9.7, nac=1.9, tip_tanks=1.3))
add('c340', 'Cessna 340/335', 'C335 C340', lambda: light_twin(11.6, 10.5, nac=2.0, tip_tanks=1.3))
add('c414', 'Cessna 401/402/414', 'C401 C402 C411 C414', lambda: light_twin(13.4, 11.1, nac=2.2, fw=1.4))
add('c421', 'Cessna 421/404', 'C421 C404', lambda: light_twin(12.5, 11.1, nac=2.2, fw=1.45))
add('da42', 'Diamond DA42', 'DA42', lambda: light_twin(13.4, 8.6, nac=1.75, fw=1.15, tpos=0.97, root=1.45, tip=0.8))
add('da62', 'Diamond DA62', 'DA62', lambda: light_twin(14.6, 9.2, nac=1.85, fw=1.25, tpos=0.97, root=1.5, tip=0.85))
add('p68', 'Vulcanair/Partenavia P68', 'P68', lambda: light_twin(12.0, 9.4, nac=1.75, root=1.55, tip=1.55))
add('pa31', 'Piper Navajo/Chieftain', 'PA31 PA3T', lambda: light_twin(12.4, 9.9, nac=2.1, fw=1.4))
add('aest', 'Piper Aerostar', 'AEST', lambda: light_twin(11.2, 10.6, nac=1.8, fw=1.2, root=1.6, tip=0.9))
add('b350', 'King Air 350', 'B350 BE30', lambda: light_twin(17.7, 14.2, nac=2.8, fw=1.6, tpos=0.97, root=2.3, tip=1.2, prop_d=2.5, nac_len=4.2))
add('be9l', 'King Air 90/100', 'BE9L BE9T BE10', lambda: light_twin(15.3, 10.8, nac=2.5, fw=1.55, root=2.1, tip=1.1, prop_d=2.4, nac_len=3.5))
add('sw4', 'Fairchild Metro/Merlin', 'SW2 SW3 SW4', lambda: light_twin(17.4, 18.1, nac=2.6, fw=1.6, root=2.2, tip=1.1, prop_d=2.6, nac_len=4.0, tail=6.0))
add('js31', 'Jetstream 31/32', 'JS31 JS32', lambda: light_twin(15.9, 14.4, nac=2.7, fw=1.98, root=2.2, tip=1.1, prop_d=2.6, nac_len=3.8, tail=5.0))
add('mu2', 'Mitsubishi MU-2', 'MU2', lambda: prop(11.9, 12.0, 1.6, wpos=0.40, root=1.5, tip=1.0, nacelles=(2.3,), nac_len=3.4, tip_tanks=1.3, prop_d=2.4))
add('c441', 'Cessna Conquest', 'C425 C441', lambda: light_twin(15.0, 11.9, nac=2.5, fw=1.5, root=2.0, tip=1.0, prop_d=2.4, nac_len=3.6))
add('pay3', 'Piper Cheyenne', 'PAY1 PAY2 PAY3 PAY4', lambda: light_twin(14.5, 10.6, nac=2.3, fw=1.4, tpos=0.97, tip_tanks=1.2, prop_d=2.3, nac_len=3.3))
add('p180', 'Piaggio Avanti', 'P180', lambda: canard_pusher(14.0, 14.4, 1.95, wing_y=8.0, root=1.6, tip=0.9, sweep=0, canard_y=1.6, canard_half=2.2, winglets=False, twin_engines=(2.3, 7.4, 3.2), prop_d=2.2))
add('c337', 'Cessna Skymaster (push-pull)', 'C337', lambda: twin_boom(11.6, 9.1, boom_x=1.6, boom_len=5.0, boom_w=0.35, pod_w=1.3, pod_len=5.5, wpos=0.30, root=1.6, tip=1.2, props='both', prop_d=1.9))
add('l410', 'Let 410', 'L410', lambda: prop(19.98, 14.4, 1.95, wpos=0.30, root=2.1, tip=1.6, nacelles=(2.5,), nac_len=3.6, prop_d=2.4))
add('bn2', 'Britten-Norman Islander', 'BN2P BN2T', lambda: prop(14.9, 10.9, 1.2, wpos=0.28, root=2.1, tip=2.1, nacelles=(1.9,), nac_len=3.0, prop_d=2.0))
add('d228', 'Dornier 228', 'D228', lambda: prop(17.0, 16.6, 1.6, wpos=0.30, root=2.0, tip=1.4, nacelles=(2.4,), nac_len=3.4, prop_d=2.7))
add('c212', 'CASA C-212', 'C212', lambda: prop(20.3, 16.2, 2.3, wpos=0.30, root=2.5, tip=1.6, nacelles=(3.0,), nac_len=3.8, prop_d=2.8))
add('sc7', 'Shorts Skyvan / 330 / 360', 'SC7 SH33 SH36', lambda: prop(19.8, 12.2, 2.0, wpos=0.30, root=1.8, tip=1.8, nacelles=(2.4,), nac_len=3.0, prop_d=2.6, fins=True))

# Classic airliners and transports
add('dc3', 'Douglas DC-3 / C-47', 'DC3 C47 DC3T', lambda: prop(29.0, 19.7, 2.4, wpos=0.25, root=4.3, tip=1.6, sweep=12, nacelles=(2.9,), nac_len=4.2, prop_d=3.5, tail=7.0, thalf=4.2, troot=2.2, ttip=1.0))
add('dc4', 'Douglas DC-4 / C-54', 'DC4', lambda: prop(35.8, 28.6, 3.0, wpos=0.30, root=4.6, tip=1.8, sweep=5, nacelles=(4.3, 9.0), prop_d=4.0, tail=9.0, thalf=6.0))
add('dc6', 'Douglas DC-6/DC-7', 'DC6 DC7', lambda: prop(35.8, 32.2, 3.2, wpos=0.32, root=4.6, tip=1.8, sweep=5, nacelles=(4.3, 9.0), prop_d=4.0, tail=10.0, thalf=6.0))
add('c46', 'Curtiss C-46 Commando', 'C46', lambda: prop(32.9, 23.3, 3.0, wpos=0.28, root=4.6, tip=1.6, sweep=8, nacelles=(3.8,), prop_d=4.2, tail=8.0, thalf=5.0))
add('coni', 'Lockheed Constellation', 'CONI L049 L749 L1049', lambda: prop(37.5, 35.4, 3.0, wpos=0.32, root=5.0, tip=1.6, sweep=6, nacelles=(4.6, 9.5), prop_d=4.6, tail=12.0, thalf=6.3, fins=True))
add('l188', 'Lockheed Electra', 'L188', lambda: prop(30.2, 31.9, 3.3, wpos=0.34, root=4.5, tip=2.0, nacelles=(4.0, 8.5), prop_d=4.1, tail=10.0, thalf=5.5))
add('visc', 'Vickers Viscount', 'VISC', lambda: prop(28.6, 26.1, 3.0, wpos=0.32, root=4.0, tip=1.6, nacelles=(3.8, 7.6), prop_d=3.0, tail=9.0, thalf=4.5))
add('ju52', 'Junkers Ju 52 (trimotor)', 'JU52', lambda: prop(29.3, 18.9, 2.0, wpos=0.25, root=4.2, tip=2.0, nacelles=(4.1,), nac_len=3.0, prop_d=3.2, nose_prop=True, tail=6.0, thalf=4.4))
add('an2', 'Antonov An-2 (biplane)', 'AN2', lambda: biplane(18.2, 12.7, 1.9, chord=2.4, lower=0.79))
add('be18', 'Beech 18 (twin tail)', 'BE18', lambda: prop(14.5, 10.7, 1.4, wpos=0.30, root=2.6, tip=1.0, sweep=8, nacelles=(2.2,), nac_len=3.2, prop_d=2.5, fins=True, thalf=3.0))
add('be17', 'Beech Staggerwing (biplane)', 'BE17', lambda: biplane(9.8, 7.9, 1.2, chord=1.4, lower=0.95, stagger=-0.8))
add('g21', 'Grumman Goose', 'G21 G44', lambda: prop(14.9, 11.7, 1.6, wpos=0.25, root=2.1, tip=1.4, nacelles=(2.0,), nac_len=2.6, prop_d=2.4, tail=4.5))
add('la4', 'Lake Buccaneer (pusher amphibian)', 'LA4 LA25', lambda: prop(11.6, 7.6, 1.2, wpos=0.32, root=1.55, tip=1.2, nose_prop=False, pusher=False, tpos=0.98, tail=3.0))
add('pby', 'Consolidated Catalina', 'PBY CAT', lambda: prop(31.7, 19.5, 3.2, wpos=0.20, root=3.3, tip=2.0, nacelles=(3.4,), nac_len=4.5, prop_d=3.6, tip_tanks=1.8, tail=7.0, thalf=4.5))
add('dhc6', 'DHC-6 Twin Otter', 'DHC6', lambda: prop(19.8, 15.8, 1.6, wpos=0.27, root=2.0, tip=2.0, nacelles=(3.1,), nac_len=3.4, prop_d=2.6, tail=5.5, thalf=3.2, troot=1.8, ttip=1.4))

# Warbirds and classic fighters
add('p51', 'P-51 Mustang', 'P51', lambda: prop(11.3, 9.8, 1.0, wpos=0.30, root=2.6, tip=1.2, prop_d=3.4, tail=4.2, thalf=2.4, troot=1.4, ttip=0.8))
add('spit', 'Supermarine Spitfire', 'SPIT', lambda: prop(11.2, 9.1, 1.0, wpos=0.25, root=2.5, elliptic=True, prop_d=3.2, tail=4.0, thalf=2.0, troot=1.3, ttip=0.6))
add('huri', 'Hawker Hurricane', 'HURI', lambda: prop(12.2, 9.8, 1.2, wpos=0.28, root=2.6, tip=1.3, prop_d=3.4, tail=4.2, thalf=2.4, troot=1.4))
add('me09', 'Messerschmitt Bf 109', 'ME09', lambda: prop(9.9, 9.0, 0.95, wpos=0.30, root=2.1, tip=1.0, prop_d=3.0, tail=4.0, thalf=1.9, troot=1.2))
add('fw90', 'Focke-Wulf Fw 190', 'FW90', lambda: prop(10.5, 9.0, 1.2, wpos=0.25, root=2.4, tip=1.1, prop_d=3.3, tail=3.8, thalf=2.0, troot=1.2))
add('zero', 'Mitsubishi Zero', 'ZERO A6M', lambda: prop(12.0, 9.1, 1.2, wpos=0.27, root=2.4, tip=1.0, prop_d=2.9, tail=4.0, thalf=2.2, troot=1.2))
add('cors', 'Vought F4U Corsair', 'CORS F4U', lambda: prop(12.5, 10.2, 1.3, wpos=0.30, root=2.9, tip=1.3, prop_d=4.0, tail=4.2, thalf=2.6, troot=1.5))
add('p47', 'P-47 Thunderbolt', 'P47', lambda: prop(12.4, 11.0, 1.5, wpos=0.28, root=2.8, elliptic=True, prop_d=3.9, tail=4.5, thalf=2.5, troot=1.5))
add('p40', 'P-40 Warhawk', 'P40', lambda: prop(11.4, 9.7, 1.1, wpos=0.28, root=2.6, tip=1.2, prop_d=3.4, tail=4.0, thalf=2.2, troot=1.4))
add('p38', 'P-38 Lightning (twin boom)', 'P38', lambda: twin_boom(15.9, 11.5, boom_x=2.4, boom_len=10.5, boom_w=0.9, pod_w=1.1, pod_len=6.0, wpos=0.30, root=2.6, tip=1.2, props='front', prop_d=3.5))
add('b17', 'B-17 Flying Fortress', 'B17', lambda: prop(31.6, 22.7, 2.4, wpos=0.25, root=5.6, tip=2.0, sweep=8, nacelles=(3.9, 7.8), prop_d=3.5, tail=8.0, thalf=6.0, troot=3.4, ttip=1.4))
add('b24', 'B-24 Liberator', 'B24', lambda: prop(33.5, 20.6, 2.3, wpos=0.25, root=3.6, tip=1.6, nacelles=(3.5, 7.3), prop_d=3.5, tail=7.0, thalf=3.9, fins=True))
add('b25', 'B-25 Mitchell', 'B25', lambda: prop(20.6, 16.1, 1.6, wpos=0.25, root=3.4, tip=1.4, nacelles=(2.9,), nac_len=4.6, prop_d=3.8, tail=6.0, thalf=3.3, fins=True))
add('b29', 'B-29 Superfortress', 'B29', lambda: prop(43.1, 30.2, 2.9, wpos=0.28, root=5.2, tip=1.6, nacelles=(5.3, 10.5), prop_d=5.0, tail=10.0, thalf=7.0, troot=3.8))
add('lanc', 'Avro Lancaster', 'LANC', lambda: prop(31.1, 21.1, 1.9, wpos=0.25, root=5.0, tip=2.0, nacelles=(3.5, 7.4), prop_d=3.9, tail=7.0, thalf=4.0, fins=True))
add('mosq', 'de Havilland Mosquito', 'MOSQ', lambda: prop(16.5, 13.6, 1.4, wpos=0.30, root=3.4, tip=1.3, nacelles=(2.4,), nac_len=4.0, prop_d=3.7, tail=5.0, thalf=3.0))
add('t6', 'North American T-6 Texan/Harvard', 'T6', lambda: prop(12.8, 8.8, 1.2, wpos=0.30, root=2.5, tip=1.1, sweep=8, prop_d=2.7, tail=3.6, thalf=2.5, troot=1.3))
add('t28', 'North American T-28 Trojan', 'T28', lambda: prop(12.2, 10.1, 1.3, wpos=0.32, root=2.5, tip=1.2, prop_d=3.1, tail=4.0, thalf=2.5, troot=1.3))
add('stearman', 'Boeing Stearman (biplane)', 'ST75', lambda: biplane(9.8, 7.6, 1.0, chord=1.4, lower=0.95))
add('dh82', 'de Havilland Tiger Moth (biplane)', 'DH82', lambda: biplane(8.9, 7.3, 0.9, chord=1.3, lower=0.98, stagger=0.4))
add('pitts', 'Pitts Special (biplane)', 'PTS1 PTS2', lambda: biplane(6.1, 5.4, 0.9, chord=1.1, lower=0.96, stagger=0.6))
add('yk52', 'Yak-52 / Yak-18', 'YK52 YK18', lambda: prop(9.3, 7.7, 1.2, wpos=0.30, root=1.9, tip=1.2, prop_d=2.4, tail=3.3, thalf=1.8))
add('a1sky', 'Douglas A-1 Skyraider', 'A1', lambda: prop(15.3, 11.8, 1.5, wpos=0.30, root=3.4, tip=1.5, prop_d=4.1, tail=4.8, thalf=2.7, troot=1.6))

# Experimental and aerobatic
add('lgez', 'Rutan Long-EZ / VariEze / Cozy', 'LGEZ VEZE COZY', lambda: canard_pusher(7.9, 5.1, 0.85, wing_y=2.9, root=1.3, tip=0.6, sweep=25, canard_y=0.6, canard_half=1.8))
add('velo', 'Velocity', 'VELO', lambda: canard_pusher(8.9, 5.9, 1.2, wing_y=3.4, root=1.5, tip=0.7, sweep=25, canard_y=0.8, canard_half=2.0))
add('e300', 'Extra 300/330', 'E300 E330 E200', lambda: low_single(8.0, 7.1, fw=1.0, root=1.9, tip=1.1, tsweep=8))
add('su26', 'Sukhoi Su-26/29/31', 'SU26 SU29 SU31', lambda: low_single(7.8, 6.8, fw=1.0, root=1.9, tip=1.1))
add('cp23', 'CAP 10/232', 'CP10 CP23', lambda: low_single(8.1, 6.8, fw=1.0, root=1.7, tip=1.2))
add('kitfox', 'Kitfox / Zenith / light sport high wing', 'KFOX CH70 CH60 C42 SAVG ECHO P92', lambda: high_single(9.8, 5.8, fw=1.0, root=1.4, tip=1.4))
add('xcub', 'CubCrafters Carbon/XCub', 'XCUB CC11 CC18 CC19', lambda: high_single(10.4, 7.0, fw=1.0, root=1.6, tip=1.6))
add('glasair', 'Glasair', 'GLAS GLST', lambda: low_single(7.3, 6.2, fw=1.05, root=1.4, tip=0.8))
add('icona5', 'ICON A5 / SeaRey', 'ICON SRAY', lambda: prop(10.8, 7.0, 1.2, wpos=0.34, root=1.4, tip=1.0, nose_prop=False, pusher=False, tpos=0.98, tail=3.0))


def with_prop(s_fn, x, y, d):
    s = s_fn()
    s.prop(x, y, d, mirror=False)
    return s


# The amphibians above carry their propeller on a pylon over the wing.
CATALOG[:] = [c for c in CATALOG if c[0] not in ('la4', 'icona5')]
add('la4', 'Lake Buccaneer (pusher amphibian)', 'LA4 LA25', lambda: with_prop(
    lambda: prop(11.6, 7.6, 1.2, wpos=0.32, root=1.55, tip=1.2, nose_prop=False, tpos=0.98, tail=3.0), 0, 3.4, 1.9))
add('icona5', 'ICON A5 / SeaRey', 'ICON SRAY', lambda: with_prop(
    lambda: prop(10.8, 7.0, 1.2, wpos=0.34, root=1.4, tip=1.0, nose_prop=False, tpos=0.98, tail=3.0), 0, 4.0, 1.8))

# Classic jets
add('f86', 'F-86 Sabre', 'F86', lambda: fighter(11.3, 11.4, 1.5, wpos=0.36, root=3.1, tip=1.2, sweep=35, stab_half=2.4, stab_root=1.4, nose=1.5))
add('mg15', 'MiG-15/17', 'MG15 MG17', lambda: fighter(10.1, 10.1, 1.5, wpos=0.36, root=3.0, tip=1.1, sweep=40, stab_half=1.7, stab_root=1.3, nose=1.4))
add('mg21', 'MiG-21', 'MG21', lambda: fighter(7.2, 15.8, 1.3, wpos=0.47, root=5.5, tip=0.4, sweep=57, stab_half=1.9, stab_root=1.6))
add('f104', 'F-104 Starfighter', 'F104', lambda: fighter(6.7, 16.7, 1.4, wpos=0.45, root=2.9, tip=1.6, sweep=18, stab_half=1.8, stab_root=1.5, tip_tanks=1.8))
add('vamp', 'de Havilland Vampire (twin boom)', 'VAMP', lambda: twin_boom(11.6, 9.4, boom_x=1.7, boom_len=5.0, boom_w=0.35, pod_w=1.5, pod_len=5.6, wpos=0.32, root=2.6, tip=1.2, jets=True))
add('hunt', 'Hawker Hunter', 'HUNT', lambda: fighter(10.3, 14.0, 1.5, wpos=0.38, root=4.0, tip=1.2, sweep=40, stab_half=2.2, stab_root=1.6))
add('l39', 'Aero L-39/L-159', 'L39 L159', lambda: fighter(9.5, 12.1, 1.3, wpos=0.40, root=2.6, tip=1.4, sweep=6, stab_half=2.2, stab_root=1.5, tip_tanks=1.6, inlets=True))
add('a4', 'A-4 Skyhawk', 'A4', lambda: fighter(8.4, 12.2, 1.3, wpos=0.40, root=4.6, tip=0.9, sweep=33, stab_half=1.7, stab_root=1.4, inlets=True))
add('f4', 'F-4 Phantom II', 'F4', lambda: fighter(11.7, 19.2, 2.0, wpos=0.42, root=5.6, tip=1.3, sweep=45, stab_half=2.6, stab_root=2.0, inlets=True))
add('f14', 'F-14 Tomcat', 'F14', lambda: fighter(19.5, 19.1, 2.0, wpos=0.40, root=3.5, tip=1.3, sweep=25, fins=1.4, stab_half=3.5, stab_root=2.8, inlets=True, lex=1.6))
add('f15', 'F-15 Eagle', 'F15', lambda: fighter(13.1, 19.4, 2.2, wpos=0.44, root=6.0, tip=1.5, sweep=45, fins=1.5, stab_half=4.3, stab_root=2.6, inlets=True))
add('f18', 'F/A-18 Hornet / Super Hornet', 'F18 F18S EA18', lambda: fighter(13.6, 18.3, 2.0, wpos=0.46, root=4.8, tip=1.6, sweep=26, fins=1.3, stab_half=3.4, stab_root=2.6, lex=1.2))
add('f22', 'F-22 Raptor', 'F22', lambda: fighter(13.6, 18.9, 2.4, wpos=0.44, root=8.0, tip=1.2, sweep=42, fins=1.6, stab_half=4.4, stab_root=2.5, inlets=True))
add('f35', 'F-35 Lightning II', 'F35', lambda: fighter(10.7, 15.7, 2.2, wpos=0.44, root=6.0, tip=1.2, sweep=34, fins=1.1, stab_half=3.4, stab_root=2.4, inlets=True))
add('su27', 'Sukhoi Su-27/30/35', 'SU27 SU30 SU33 SU35 J11 J15', lambda: fighter(14.7, 21.9, 2.0, wpos=0.44, root=5.5, tip=1.2, sweep=42, fins=1.8, stab_half=4.2, stab_root=2.5, lex=2.0))
add('mg29', 'MiG-29', 'MG29 MG35', lambda: fighter(11.4, 17.3, 2.0, wpos=0.44, root=4.6, tip=1.0, sweep=42, fins=1.5, stab_half=3.8, stab_root=2.2, lex=1.6))
add('tornado', 'Panavia Tornado', 'TOR', lambda: fighter(13.9, 16.7, 2.0, wpos=0.42, root=3.0, tip=1.0, sweep=25, stab_half=3.6, stab_root=2.8, inlets=True, lex=1.0))
add('jaguar', 'SEPECAT Jaguar', 'JAGR', lambda: fighter(8.7, 16.8, 1.6, wpos=0.45, root=3.6, tip=1.1, sweep=40, stab_half=2.4, stab_root=2.0, inlets=True))
add('mir2', 'Mirage 2000 / Mirage III', 'MIR2 MIR3 M2000', lambda: fighter(9.1, 14.4, 1.5, wpos=0.38, root=8.5, tip=0.6, sweep=58, stab=False))
add('draken', 'Saab Draken', 'DRAK', lambda: fighter(9.4, 15.4, 1.5, wpos=0.30, root=10.0, tip=1.0, sweep=70, stab=False))
add('viggen', 'Saab Viggen', 'VIGN', lambda: fighter(10.6, 16.4, 1.6, wpos=0.55, root=6.5, tip=1.2, sweep=50, stab=False, canard=(4.5, 2.8)))
add('rafale', 'Dassault Rafale', 'RFAL', lambda: fighter(10.9, 15.3, 1.9, wpos=0.45, root=7.0, tip=1.0, sweep=48, stab=False, canard=(4.6, 2.0)))
add('gripen', 'Saab Gripen', 'GRIF JAS3', lambda: fighter(8.4, 14.1, 1.6, wpos=0.47, root=6.5, tip=0.9, sweep=45, stab=False, canard=(4.4, 1.6)))
add('eufi', 'Eurofighter Typhoon', 'EUFI', lambda: fighter(10.95, 15.96, 1.9, wpos=0.42, root=8.6, tip=1.2, sweep=53, stab=False, canard=(3.9, 2.0)))
add('j10', 'Chengdu J-10', 'J10', lambda: fighter(9.8, 16.4, 1.8, wpos=0.45, root=7.5, tip=1.0, sweep=55, stab=False, canard=(4.6, 2.0)))
add('f16', 'F-16 Fighting Falcon', 'F16 F2', lambda: fighter(9.96, 15.06, 1.6, wpos=0.37, root=6.0, tip=1.1, sweep=40, stab_half=2.8, stab_root=2.8, lex=0.6))
add('f5', 'F-5 / T-38', 'F5 T38', lambda: fighter(8.1, 14.4, 1.4, wpos=0.45, root=3.3, tip=0.8, sweep=32, stab_half=2.1, stab_root=1.6, tip_tanks=0.0))
add('hawk', 'BAE Hawk / T-45', 'HAWK T45', lambda: fighter(9.4, 12.4, 1.4, wpos=0.40, root=2.8, tip=1.2, sweep=26, stab_half=2.3, stab_root=1.5, inlets=True))
add('ajet', 'Dassault Alpha Jet', 'AJET', lambda: fighter(9.1, 12.3, 1.4, wpos=0.40, root=3.0, tip=1.0, sweep=28, stab_half=2.2, stab_root=1.5, inlets=True))
add('m346', 'M-346 / Yak-130', 'M346 YK13', lambda: fighter(9.7, 11.5, 1.6, wpos=0.42, root=3.5, tip=1.0, sweep=31, stab_half=2.4, stab_root=1.6, lex=1.0))
add('t7', 'Boeing T-7 Red Hawk', 'T7', lambda: fighter(10.2, 14.1, 1.7, wpos=0.42, root=3.8, tip=1.1, sweep=30, fins=1.0, stab_half=2.7, stab_root=1.8, lex=1.0))
add('a10', 'A-10 Thunderbolt II', 'A10', lambda: jet(17.5, 16.3, 1.6, wpos=0.34, root=3.0, tip=1.6, sweep=0, pods=(), rear=2, rear_y=0.55, pod_w=1.5, pod_len=3.6, tpos=0.95, thalf=2.9, troot=1.6, tsweep=0))
add('av8b', 'Harrier', 'AV8B HAR2 HAR', lambda: fighter(9.25, 14.1, 1.8, wpos=0.40, root=3.6, tip=1.0, sweep=36, stab_half=2.1, stab_root=1.8, inlets=True))
add('f117', 'F-117 Nighthawk', 'F117', lambda: fighter(13.2, 20.1, 2.2, wpos=0.25, root=12.0, tip=1.4, sweep=67, stab=False, fins=1.0))
add('sr71', 'SR-71 Blackbird', 'SR71', lambda: jet(16.9, 32.7, 1.6, wpos=0.40, root=15.0, tip=1.5, sweep=52, pods=(0.42,), pod_len=10.0, pod_w=1.6, thalf=0.1, troot=0.1))
add('u2', 'Lockheed U-2', 'U2', lambda: jet(31.4, 19.2, 1.6, wpos=0.36, root=2.6, tip=1.0, sweep=3, pods=(), thalf=3.2, troot=1.5, tsweep=10))

# Bombers, tankers, transports, patrol
add('c5', 'C-5 Galaxy', 'C5 C5M', lambda: jet(67.9, 75.5, 7.0, wpos=0.36, sweep=25, pods=(0.32, 0.58), thalf=10.0, tpos=0.98))
add('a400', 'Airbus A400M', 'A400', lambda: prop(42.4, 45.1, 5.6, wpos=0.34, root=6.0, tip=2.5, sweep=15, nacelles=(6.0, 11.5), nac_len=6.0, prop_d=5.3, tail=14.0, tpos=0.98, thalf=9.0, troot=4.0))
add('p3', 'P-3 Orion / L-188', 'P3', lambda: prop(30.4, 35.6, 3.4, wpos=0.34, root=4.6, tip=2.0, nacelles=(4.0, 8.4), prop_d=4.1, tail=11.0, thalf=6.0))
add('c160', 'Transall C-160', 'C160', lambda: prop(40.0, 32.4, 4.0, wpos=0.32, root=4.5, tip=2.4, nacelles=(5.3,), nac_len=6.0, prop_d=5.5, tail=10.0, thalf=7.0, troot=3.5))
add('an124', 'Antonov An-124', 'A124', lambda: jet(73.3, 69.1, 7.3, wpos=0.36, sweep=32, pods=(0.30, 0.55), thalf=11.0, tpos=0.98))
add('an225', 'Antonov An-225', 'A225', lambda: jet(88.4, 84.0, 7.3, wpos=0.36, sweep=32, pods=(0.24, 0.42, 0.60), thalf=16.0, tpos=0.98))
add('il76', 'Ilyushin Il-76', 'IL76 A50', lambda: jet(50.5, 46.6, 4.8, wpos=0.34, sweep=25, pods=(0.32, 0.56), thalf=8.6, tpos=0.98))
add('e7', 'Boeing E-7 Wedgetail', 'E737', lambda: jet(35.8, 33.6, 3.76, sweep=25, canard=None))
add('q4', 'RQ-4 Global Hawk', 'Q4 RQ4', lambda: jet(39.9, 14.5, 1.6, wpos=0.40, root=2.2, tip=0.8, sweep=3, pods=(), rear=0, thalf=2.8, troot=1.4, tsweep=20))
add('mq9', 'MQ-9 Reaper / MQ-1', 'MQ9 Q9 MQ1 Q1 HRON', lambda: prop(20.1, 11.0, 1.1, wpos=0.40, root=1.6, tip=1.0, pusher=True, vtail=True, tpos=0.95, thalf=2.9))
add('kc135', 'KC-135 / Boeing 707 / E-6', 'B701 B703 B720 K35R K35E K35T C135 E6 R135', lambda: jet(39.9, 41.5, 3.76, sweep=35, pods=(0.34, 0.63)))
add('kc46', 'KC-46 Pegasus / Boeing 767-200', 'K46 KC46', lambda: jet(48.1, 50.5, 5.03, sweep=31))

CATALOG[:] = [c for c in CATALOG if c[0] != 'e7']  # a 737 from above; shares that icon

# Helicopters: rotor diameter, length, body width, where the cabin ends, blades
add('r22', 'Robinson R22', 'R22', lambda: heli(7.7, 8.8, 1.1, 5.8, 2, offset=55))
add('r44', 'Robinson R44', 'R44', lambda: heli(10.1, 11.7, 1.3, 7.6, 2, offset=55))
add('r66', 'Robinson R66', 'R66', lambda: heli(10.1, 11.8, 1.4, 7.8, 2, offset=55))
add('b06', 'Bell 206 JetRanger / LongRanger', 'B06 B06T', lambda: heli(10.2, 11.8, 1.3, 7.6, 2, offset=55))
add('b505', 'Bell 505', 'B505', lambda: heli(11.3, 12.9, 1.5, 8.2, 2, offset=55))
add('b47g', 'Bell 47 (bubble)', 'B47G', lambda: heli(11.3, 13.2, 1.8, 5.8, 2, boom_w=0.5))
add('uh1', 'Bell UH-1 Huey / 204/205/212', 'UH1 B204 B205 B212', lambda: heli(14.6, 17.4, 2.6, 11.2, 2, offset=55))
add('ah1', 'Bell AH-1 Cobra', 'H1 AH1', lambda: heli(14.6, 17.7, 1.0, 11.2, 2, offset=55, stubs=1.6))
add('b222', 'Bell 222/230', 'B222 B230', lambda: heli(12.1, 15.3, 1.9, 9.6, 2, offset=55))
add('b407', 'Bell 407', 'B407', lambda: heli(10.7, 12.7, 1.4, 8.0, 4, offset=30))
add('b412', 'Bell 412', 'B412', lambda: heli(14.0, 17.1, 2.6, 11.0, 4, offset=30))
add('b429', 'Bell 427/429', 'B427 B429', lambda: heli(11.0, 13.1, 1.6, 8.4, 4, offset=30))
add('b525', 'Bell 525', 'B525', lambda: heli(16.5, 20.0, 2.6, 13.0, 5, offset=20))
add('h500', 'MD 500', 'H500 H520 H530', lambda: heli(8.1, 9.4, 1.4, 5.6, 5, offset=20, boom_w=0.35))
add('md900', 'MD 900 Explorer (NOTAR)', 'EXPL MD60 MD90X', lambda: heli(10.3, 11.8, 1.6, 7.6, 5, offset=20, fenestron=True, tail_rotor=False))
add('as50', 'Airbus AS350 / H125', 'AS50 H125', lambda: heli(10.7, 12.9, 1.6, 8.4, 3, offset=40))
add('as55', 'Airbus AS355 TwinStar', 'AS55', lambda: heli(10.7, 12.9, 1.7, 8.6, 3, offset=40))
add('ec30', 'Airbus EC130 / H130', 'EC30 H130', lambda: heli(10.7, 12.6, 1.9, 8.4, 3, offset=40, fenestron=True))
add('ec20', 'Airbus EC120 Colibri', 'EC20', lambda: heli(10.0, 11.5, 1.5, 7.4, 3, offset=40, fenestron=True))
add('ec35', 'Airbus EC135 / H135', 'EC35 H135', lambda: heli(10.2, 12.2, 1.7, 8.2, 4, offset=30, fenestron=True))
add('ec45', 'Airbus H145 / BK117', 'EC45 H145 BK17', lambda: heli(11.0, 13.6, 1.8, 8.8, 4, offset=30))
add('as65', 'Airbus Dauphin / H155', 'AS65 EC55 H155', lambda: heli(11.9, 13.7, 2.0, 9.0, 4, offset=30, fenestron=True))
add('h160', 'Airbus H160', 'H160', lambda: heli(13.4, 15.7, 2.2, 10.2, 5, offset=20, fenestron=True))
add('ec75', 'Airbus H175', 'EC75 H175', lambda: heli(14.8, 18.1, 2.6, 12.0, 5, offset=20))
add('ec25', 'Airbus H225 / Super Puma', 'EC25 AS32 AS3B H215 H225', lambda: heli(16.2, 19.5, 2.6, 13.0, 4, offset=30))
add('gazl', 'Aerospatiale Gazelle', 'GAZL', lambda: heli(10.5, 11.9, 1.6, 7.4, 3, offset=40, fenestron=True))
add('alo3', 'Alouette II/III, Lama', 'ALO2 ALO3 LAMA', lambda: heli(11.0, 12.8, 1.6, 7.0, 3, offset=40, boom_w=0.4))
add('a109', 'Leonardo A109 / A119', 'A109 A119', lambda: heli(11.0, 13.0, 1.6, 8.4, 4, offset=30))
add('a139', 'Leonardo AW139', 'A139', lambda: heli(13.8, 16.7, 2.2, 10.8, 5, offset=20))
add('a169', 'Leonardo AW169', 'A169', lambda: heli(12.1, 14.7, 2.1, 9.6, 5, offset=20))
add('a189', 'Leonardo AW189 / AW149', 'A189 A149', lambda: heli(14.6, 17.6, 2.4, 11.4, 5, offset=20))
add('eh10', 'Leonardo AW101 / Merlin', 'EH10', lambda: heli(18.6, 22.8, 2.9, 15.0, 5, offset=20))
add('s76', 'Sikorsky S-76', 'S76', lambda: heli(13.4, 16.0, 2.1, 10.4, 4, offset=30))
add('s92', 'Sikorsky S-92 / H-92', 'S92 H92', lambda: heli(17.2, 20.9, 2.9, 14.0, 4, offset=30))
add('h60', 'Sikorsky UH-60 Black Hawk / S-70', 'H60 S70 UH60 MH60', lambda: heli(16.4, 19.8, 2.4, 13.0, 4, offset=30))
add('h53', 'Sikorsky CH-53', 'H53 H53S CH53', lambda: heli(24.0, 30.2, 3.0, 20.0, 7, offset=10))
add('s61', 'Sikorsky S-61 / Sea King', 'S61 H3', lambda: heli(18.9, 22.2, 2.6, 15.0, 5, offset=20))
add('h47', 'Boeing CH-47 Chinook', 'H47 CH47', lambda: _tandem(18.3, 30.1, 3.8))
add('h46', 'Boeing CH-46 Sea Knight', 'H46', lambda: _tandem(15.2, 25.7, 2.2))
add('h64', 'Boeing AH-64 Apache', 'H64 AH64', lambda: heli(14.6, 17.7, 1.4, 11.6, 4, offset=30, stubs=2.4))
add('tigr', 'Airbus Tiger', 'TIGR', lambda: heli(13.0, 15.8, 1.3, 10.4, 4, offset=30, stubs=2.2))
add('a129', 'Leonardo A129 Mangusta', 'A129', lambda: heli(11.9, 14.3, 1.1, 9.4, 4, offset=30, stubs=1.8))
add('mi8', 'Mil Mi-8/17', 'MI8 MI17 MI14', lambda: heli(21.3, 25.3, 2.5, 17.0, 5, offset=20))
add('mi24', 'Mil Mi-24/35 Hind', 'MI24 MI35', lambda: heli(17.3, 21.4, 1.7, 14.0, 5, offset=20, stubs=3.0))
add('mi26', 'Mil Mi-26', 'MI26', lambda: heli(32.0, 40.0, 3.3, 27.0, 8, offset=10))
add('mi28', 'Mil Mi-28', 'MI28', lambda: heli(17.2, 21.2, 1.5, 13.8, 5, offset=20, stubs=2.6))
add('ka52', 'Kamov Ka-50/52', 'KA50 KA52', lambda: _coax(14.5, 13.5, 1.8))
add('ka32', 'Kamov Ka-27/32', 'KA27 KA32 KA26', lambda: _coax(15.9, 12.3, 2.6))
add('nh90', 'NHIndustries NH90', 'NH90', lambda: heli(16.3, 19.6, 2.6, 13.0, 4, offset=30))
add('lynx', 'Westland Lynx / AW159', 'LYNX A159', lambda: heli(12.8, 15.2, 2.0, 9.6, 4, offset=30))
add('en28', 'Enstrom 280/480', 'EN28 EN48', lambda: heli(9.8, 8.9, 1.5, 5.4, 3, offset=40))
add('h269', 'Schweizer 300 / Hughes 269', 'H269 S300', lambda: heli(8.2, 9.4, 1.5, 4.8, 3, boom_w=0.45))
add('g2ca', 'Guimbal Cabri G2', 'G2CA', lambda: heli(7.2, 8.0, 1.2, 4.8, 3, offset=40, fenestron=True))


def _tandem(rotor_d, length, body_w):
    s = Shape(rotor_d, length)
    r = rotor_d / 2
    w = body_w * 1.3  # a little wider than life: it's mostly hidden under the blades
    s.fuselage(w, start=r * 0.55, end=length - r * 0.55, nose=w * 0.5, tail=w * 0.4, tail_width=w * 0.8)
    # One blade of each rotor lies over the body (hidden): the front rotor
    # shows a V pointing forward, the rear one a V pointing back.
    s.rotor(0, r, r, 3, rotor_d * 0.05, offset_deg=60)
    s.rotor(0, length - r, r, 3, rotor_d * 0.05, offset_deg=0)
    return s


def _coax(rotor_d, length, body_w):
    s = Shape(rotor_d, length)
    r = rotor_d / 2
    s.fuselage(body_w, start=max(0.0, r - 2.6), end=length - 0.2, nose=body_w * 0.6, tail=length * 0.35,
               tail_width=body_w * 0.5)
    s.wing(length - 1.6, 1.0, body_w * 0.9 + 0.6, 0, 1.0)
    s.rotor(0, r, r, 6, rotor_d * 0.045)  # two three-blade rotors: six blades from above
    return s


# The busiest types, drawn to match their family members (they used to show
# the ADS-B Radar airliner icons, which beside these looked generic).
add('a320', 'Airbus A320 / A320neo', 'A320 A20N', lambda: jet(35.8, 37.6, 3.95, sweep=25))
add('b738', 'Boeing 737-800 / MAX 8 / P-8', 'B738 B38M P8', lambda: jet(35.8, 39.5, 3.76, sweep=25, wpos=0.39))
add('b789', 'Boeing 787-9', 'B789', lambda: jet(60.1, 62.8, 5.77, sweep=32, tip=1.5, wpos=0.39))
add('crj9', 'Bombardier CRJ900/1000', 'CRJ9 CRJX', lambda: jet(24.9, 36.4, 2.7, sweep=26, pods=(), rear=2, wpos=0.45, tpos=0.95))
add('b763', 'Boeing 767-300', 'B763', lambda: jet(47.6, 54.9, 5.03, sweep=31, wpos=0.39))
add('a333', 'Airbus A330-300', 'A333', lambda: jet(60.3, 63.7, 5.64, sweep=30, wpos=0.39))
add('e195', 'Embraer 195', 'E195', lambda: jet(28.7, 38.7, 3.0, sweep=23, wpos=0.40))
add('e295', 'Embraer E195-E2', 'E295', lambda: jet(35.1, 41.5, 3.0, sweep=23, wpos=0.40, tip=1.2))
add('e145', 'Embraer ERJ 145', 'E145 E45X', lambda: jet(20.0, 29.9, 2.28, sweep=23, pods=(), rear=2, wpos=0.44, tpos=0.95))
add('b744', 'Boeing 747-400 / classic', 'B741 B742 B743 B744 B74R B74D', lambda: jet(64.4, 70.7, 6.5, sweep=37, pods=(0.32, 0.62)))
add('a388', 'Airbus A380', 'A388', lambda: jet(79.8, 72.7, 7.1, sweep=33, pods=(0.30, 0.55)))
add('a343', 'Airbus A340-200/300', 'A342 A343', lambda: jet(60.3, 63.7, 5.64, sweep=30, wpos=0.39, pods=(0.3, 0.6)))
add('md11', 'McDonnell Douglas MD-11', 'MD11', lambda: jet(51.7, 61.6, 6.0, sweep=35, pods=(0.36,), wpos=0.40, rear=0))
add('c17', 'Boeing C-17 Globemaster III', 'C17', lambda: jet(51.7, 53.0, 6.9, sweep=25, pods=(0.32, 0.56), tpos=0.98, thalf=9.0))
add('c130', 'Lockheed C-130 Hercules, An-12, Il-18', 'C130 C30J AN12 IL18', lambda: prop(40.4, 29.8, 4.3, wpos=0.30, root=4.9, tip=2.5, nacelles=(5.2, 10.4), prop_d=4.1, tail=9.0, thalf=8.0, troot=3.4))
add('c27j', 'Alenia C-27J Spartan', 'C27J', lambda: prop(28.7, 22.7, 3.3, wpos=0.32, root=3.0, tip=1.7, nacelles=(3.8,), nac_len=5.0, prop_d=4.1, tail=7.5, thalf=5.0))
add('be20', 'King Air 200/250/260', 'BE20', lambda: light_twin(16.6, 13.3, nac=2.6, fw=1.55, tpos=0.97, root=2.2, tip=1.1, prop_d=2.5, nac_len=3.8))
add('b190', 'Beech 1900', 'B190', lambda: light_twin(17.7, 17.6, nac=2.7, fw=1.6, tpos=0.97, root=2.3, tip=1.2, prop_d=2.8, nac_len=4.0, tail=6.0))
add('fa7x', 'Falcon 7X', 'FA7X', lambda: bizjet(26.2, 23.2, 2.3, sweep=34, rear=3, tip=1.0))
add('glf5', 'Gulfstream II/III/V/G550', 'GLF2 GLF3 GLF5', lambda: bizjet(28.5, 29.4, 2.4, sweep=27, tip=1.1))
add('g150', 'Gulfstream G100/G150/G200', 'ASTR G150 GALX', lambda: bizjet(16.9, 16.9, 1.9, sweep=27, tip=1.0))
add('c650', 'Citation III/VI/VII', 'C650', lambda: bizjet(16.3, 16.9, 1.7, sweep=25, tip=1.0))
add('ea50', 'Eclipse 500/550', 'EA50', lambda: bizjet(11.6, 10.1, 1.4, sweep=4, tip=0.8))
add('dh8a', 'Dash 8-100/200', 'DH8A DH8B', lambda: prop(25.9, 22.3, 2.7, wpos=0.34, nacelles=(4.0,), tpos=0.97, thalf=3.8, troot=2.0, tail=7.0))
add('cl415', 'Canadair CL-215/415', 'CL2T CL4T CL2P', lambda: prop(28.6, 19.8, 2.6, wpos=0.28, root=3.5, tip=3.5, nacelles=(3.8,), nac_len=4.0, prop_d=4.0, tip_tanks=1.5, tail=6.5, thalf=4.5))
add('p06t', 'Tecnam P2006T', 'P06T', lambda: prop(11.4, 8.7, 1.2, wpos=0.30, root=1.4, tip=1.4, nacelles=(1.6,), nac_len=2.4, prop_d=1.8, tpos=0.97))
add('sling', 'Sling 2/4/TSi', 'SLG2 SLG4 SLGT', lambda: low_single(9.2, 7.0, fw=1.1, root=1.4, tip=1.1))
