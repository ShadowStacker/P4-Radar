#!/usr/bin/env python3
"""Generates the extra aircraft icons (assets/aircraft_icons/generated/*.svg).

The ADS-B Radar icon set (https://adsb-radar.com) covers airliners and a few
classes; these fill the gaps (propeller singles and twins, helicopters by
rotor layout, military types) in the same style: solid top-view silhouettes,
nose up, in a 512 px box. Each is drawn from the real aircraft's span,
length and layout (aircraft_silhouette.py), so a twin turboprop shows two
propellers on its wing and a jet shows its engine pods.

  python3 scripts/aircraft_families.py
then
  node scripts/gen_aircraft_icons.js
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft_silhouette import Shape  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'aircraft_icons', 'generated')

FAMILIES = {}


def family(name):
    def wrap(fn):
        FAMILIES[name] = fn
        return fn
    return wrap


# --- Light aircraft --------------------------------------------------------

@family('g_single_low')  # Piper PA-28, Cirrus, Diamond DA40, Bonanza, Mooney
def _():
    s = Shape(10.7, 7.3)
    s.fuselage(1.2, nose=1.0, tail=2.6)
    s.wing(2.4, 1.6, 5.35, 0, 1.4)
    s.prop(0, 0.15, 1.9, mirror=False)
    s.wing(6.3, 0.85, 1.9, 0, 0.75)
    return s


@family('g_single_high')  # Cessna 150-210
def _():
    s = Shape(11.0, 8.3)
    s.fuselage(1.15, nose=1.1, tail=3.4, tail_width=0.3)
    s.wing(2.3, 1.6, 5.5, 0, 1.1)
    s.prop(0, 0.15, 1.9, mirror=False)
    s.wing(7.2, 0.95, 1.7, 8, 0.75)
    return s


@family('g_turboprop_single')  # Pilatus PC-12, TBM, PA-46, T-6, PC-21
def _():
    s = Shape(16.3, 14.4)
    s.fuselage(1.6, nose=2.4, tail=5.0)
    s.wing(5.2, 2.4, 8.15, 2, 1.1)
    s.prop(0, 0.15, 2.7, mirror=False)
    s.wing(12.9, 1.3, 2.8, 18, 0.8)
    return s


@family('g_turboprop_single_high')  # Cessna Caravan, Kodiak, Beaver, Otter, PC-6
def _():
    s = Shape(15.9, 11.5)
    s.fuselage(1.7, nose=1.8, tail=4.0)
    s.wing(3.4, 2.0, 7.95, 0, 1.3)
    s.prop(0, 0.15, 2.7, mirror=False)
    s.wing(9.9, 1.4, 3.1, 4, 1.0)
    return s


@family('g_twin_piston')  # Baron, Seneca, Seminole, Cessna 310/340, DA42
def _():
    s = Shape(11.5, 9.1)
    s.fuselage(1.25, nose=1.3, tail=3.2)
    s.wing(3.2, 1.7, 5.75, 2, 1.0)
    s.nacelle(1.95, 2.2, 2.4, 0.75)
    s.prop(1.95, 2.15, 1.9)
    s.wing(7.9, 0.95, 2.1, 6, 0.7)
    return s


@family('g_twin_turboprop')  # Beech 99, Metro, Jetstream 31, Cessna Conquest, MU-2
def _():
    s = Shape(14.0, 13.6)
    s.fuselage(1.45, nose=1.6, tail=4.5)
    s.wing(4.9, 2.3, 7.0, 3, 1.1)
    s.nacelle(2.5, 3.5, 3.8, 0.95)
    s.prop(2.5, 3.45, 2.4)
    s.wing(12.0, 1.4, 2.9, 12, 0.7)
    return s


@family('g_twin_high')  # Twin Otter, Islander, Do 228, Let 410, CASA 212
def _():
    s = Shape(19.8, 15.8)
    s.fuselage(1.6, nose=1.8, tail=5.5)
    s.wing(4.2, 2.0, 9.9, 0, 2.0)
    s.nacelle(3.1, 3.1, 3.4, 0.9)
    s.prop(3.1, 3.05, 2.6)
    s.wing(13.6, 1.8, 3.2, 0, 1.4)
    return s


# --- Jets ------------------------------------------------------------------

@family('g_b707')  # 707, KC-135, E-6, C-135
def _():
    s = Shape(39.9, 41.5)
    s.fuselage(3.8, nose=4.5, tail=10.0)
    s.wing(14.5, 9.0, 19.95, 35, 2.4)
    for x in (6.8, 12.6):
        s.jet_pod(x, 13.0 + (x - 1.9) * 0.7, 4.6, 1.5)
    s.wing(35.5, 4.0, 6.8, 35, 1.5)
    return s


@family('g_awacs')  # E-3 Sentry: 707 with a rotodome
def _():
    s = Shape(44.4, 46.6)
    s.fuselage(3.8, nose=4.5, tail=11.0)
    s.wing(17.0, 9.5, 22.2, 35, 2.5)
    for x in (7.5, 14.0):
        s.jet_pod(x, 15.5 + (x - 1.9) * 0.7, 5.0, 1.6)
    s.wing(40.0, 4.2, 7.5, 35, 1.6)
    s.ellipse(0, 31.0, 4.55, 4.55)
    return s


@family('g_hawkeye')  # E-2 Hawkeye / C-2: twin turboprop, rotodome, four fins
def _():
    s = Shape(24.6, 17.6)
    s.fuselage(2.4, nose=2.2, tail=4.0, tail_width=1.2)
    s.wing(5.6, 3.2, 12.3, 3, 1.5)
    s.nacelle(3.6, 4.0, 5.0, 1.3)
    s.prop(3.6, 3.9, 4.1)
    s.wing(15.4, 1.8, 4.0, 5, 1.6)
    s.ellipse(0, 9.5, 3.65, 3.65)
    return s


@family('g_b52')
def _():
    s = Shape(56.4, 48.5)
    s.fuselage(3.0, nose=4.0, tail=12.0, tail_width=0.8)
    s.wing(17.0, 12.0, 28.2, 35, 4.0)
    for x in (10.0, 19.0):
        s.jet_pod(x, 14.0 + (x - 1.5) * 0.7, 5.5, 2.6)
    s.wing(43.0, 5.0, 10.0, 35, 2.0)
    return s


@family('g_b1')  # B-1B, wings swept back
def _():
    s = Shape(24.1, 44.5)
    s.fuselage(3.4, nose=7.0, tail=8.0, tail_width=1.0)
    s.poly([(1.7, 11.0), (5.0, 19.0), (5.0, 26.0), (1.7, 28.0)], mirror=True)  # blended glove
    s.wing(19.0, 6.5, 12.05, 67, 2.0, root_x=4.5)
    s.jet_pod(3.6, 25.0, 7.0, 2.4)
    s.wing(39.0, 4.0, 6.7, 40, 1.5)
    return s


@family('g_b2')  # B-2 flying wing
def _():
    s = Shape(52.4, 21.0)
    s.poly([(0, 0), (26.2, 15.5), (26.2, 16.5), (20.0, 21.0), (13.0, 16.0), (6.5, 21.0), (0, 16.5)], mirror=True)
    return s


@family('g_mq9')  # MQ-9 Reaper / MQ-1: long straight wings, pusher, V-tail
def _():
    s = Shape(20.1, 11.0)
    s.fuselage(1.1, nose=1.4, tail=2.6, tail_width=0.4)
    s.wing(4.4, 1.6, 10.05, 0, 1.0)
    s.prop(0, 10.6, 2.8, mirror=False)
    s.poly([(0.2, 8.8), (2.9, 9.9), (2.9, 10.5), (0.2, 9.9)], mirror=True)
    return s


# --- Fighters and trainers ----------------------------------------------------

@family('g_fighter_single')  # F-16, J-10-like single fin, cropped delta
def _():
    s = Shape(9.96, 15.06)
    s.fuselage(1.6, nose=3.2, tail=3.0, tail_width=0.9)
    s.wing(5.6, 6.0, 4.98, 40, 1.1)
    s.wing(12.3, 2.8, 2.8, 40, 0.9)
    s.poly([(0.9, 4.0), (0.25, 4.0), (0.25, 7.0)], mirror=True)
    return s


@family('g_fighter_twin')  # F-15, F/A-18, F-22, F-35, Su-27 family, MiG-29: twin fins
def _():
    s = Shape(13.6, 18.3)
    s.fuselage(2.2, nose=3.5, tail=2.5, tail_width=1.6)
    s.wing(8.4, 4.8, 6.8, 26, 1.6)
    s.poly([(1.1, 3.6), (0.4, 3.6), (1.1, 9.0)], mirror=True)
    s.wing(15.0, 2.6, 3.6, 40, 1.2)
    s.poly([(1.4, 12.6), (2.2, 15.8), (1.85, 16.0), (1.1, 13.0)], mirror=True)
    return s


@family('g_fighter_delta')  # Eurofighter, Rafale, Gripen: delta with canards
def _():
    s = Shape(10.95, 15.96)
    s.fuselage(1.9, nose=3.4, tail=1.0, tail_width=1.3)
    s.wing(6.8, 8.6, 5.48, 53, 1.2)
    s.wing(3.9, 1.8, 2.3, 50, 0.8)
    return s


@family('g_attack')  # A-10: straight wing, engines on the back
def _():
    s = Shape(17.5, 16.3)
    s.fuselage(1.6, nose=1.6, tail=3.5, tail_width=0.6)
    s.wing(5.6, 3.0, 8.75, 0, 1.6)
    s.jet_pod(1.9, 8.8, 3.6, 1.5)
    s.wing(14.3, 1.6, 2.8, 0, 1.6)
    s.poly([(2.5, 14.0), (3.0, 14.0), (3.0, 16.2), (2.5, 16.2)], mirror=True)
    return s


@family('g_jet_trainer')  # T-38, Hawk, L-39, M-346, Alpha Jet, T-45
def _():
    s = Shape(9.4, 12.4)
    s.fuselage(1.4, nose=2.6, tail=3.2, tail_width=0.6)
    s.wing(5.0, 2.8, 4.7, 26, 1.2)
    s.wing(10.4, 1.5, 2.3, 30, 0.8)
    return s


# --- Helicopters ---------------------------------------------------------------

def heli(rotor_d, length, body_w, body_end, blades, stab=1.0, offset=45):
    s = Shape(rotor_d, length)
    r = rotor_d / 2
    s.fuselage(body_w, start=r * 0.55, end=body_end, nose=body_w * 0.8, tail=body_w * 1.4, tail_width=body_w * 0.4)
    s.fuselage(body_w * 0.3, start=body_end - body_w, end=length - 0.2, nose=0.1, tail=0.5, tail_width=body_w * 0.25)
    s.wing(length - 1.4, 0.6, stab, 0, 0.45)
    s.rotor(0, r, r, blades, rotor_d * 0.04)  # angles by blade count (see Shape.rotor)
    return s


@family('g_heli_2')  # Robinson R22/R44, Bell 206: two blades
def _():
    return heli(10.1, 11.7, 1.3, 8.0, 2, stab=0.8, offset=55)


@family('g_heli_3')  # AS350/H125, AS355, EC130: three blades
def _():
    return heli(10.7, 12.9, 1.6, 8.8, 3, stab=1.2, offset=40)


@family('g_heli_4')  # EC135/H135, H145, Bell 407/429, A109, S-76
def _():
    return heli(10.2, 12.2, 1.7, 8.6, 4, stab=1.2)


@family('g_heli_5')  # AW139, H160, EC225, Mi-8/17: five blades
def _():
    return heli(13.8, 16.6, 2.3, 11.4, 5, stab=1.6, offset=0)


@family('g_heli_large')  # UH-60, S-92, NH90, Super Puma: big four-blade
def _():
    return heli(16.4, 19.8, 2.6, 13.0, 4, stab=2.2)


@family('g_heli_tandem')  # CH-47 Chinook
def _():
    s = Shape(18.3, 30.1)
    s.fuselage(4.9, start=5.0, end=25.1, nose=2.4, tail=2.0, tail_width=3.9)
    s.rotor(0, 9.15, 9.15, 3, 0.9, offset_deg=60)   # a V forward
    s.rotor(0, 20.95, 9.15, 3, 0.9, offset_deg=0)   # a V back
    return s


@family('g_heli_attack')  # AH-64 Apache, AH-1: narrow body, stub wings
def _():
    s = heli(14.6, 17.7, 1.6, 12.0, 4, stab=1.7)
    s.poly([(0.6, 6.4), (2.6, 6.6), (2.6, 7.6), (0.6, 7.6)], mirror=True)
    return s


@family('g_heli_coax')  # Kamov Ka-27/32/52: two coaxial rotors, twin fins
def _():
    s = Shape(15.9, 12.3)
    s.fuselage(2.6, start=2.4, end=12.0, nose=1.6, tail=3.0, tail_width=1.2)
    s.wing(10.6, 1.0, 2.3, 0, 1.0)
    s.rotor(0, 7.95, 7.95, 6, 0.7)  # two three-blade rotors
    return s


if __name__ == '__main__':
    from aircraft_catalog import CATALOG  # noqa: E402  (the type-specific icons)
    os.makedirs(OUT, exist_ok=True)
    for old in os.listdir(OUT):
        if old.endswith('.svg'):
            os.remove(os.path.join(OUT, old))
    for name, fn in FAMILIES.items():
        with open(os.path.join(OUT, name + '.svg'), 'w') as f:
            f.write(fn().svg())
    for key, _desc, _types, fn in CATALOG:
        with open(os.path.join(OUT, 'x_' + key + '.svg'), 'w') as f:
            f.write(fn().svg())
    print(f'{len(FAMILIES)} family + {len(CATALOG)} type icons -> {os.path.relpath(OUT)}')
