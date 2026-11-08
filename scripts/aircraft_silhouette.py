"""Top-view aircraft silhouettes from a few real dimensions (metres), as SVG.

Nose up, filled shapes in a 512 x 512 box, the convention of the ADS-B Radar
icon set (https://adsb-radar.com) the radar also uses, scaled so the larger
of span and length fills ~98 % of the box. Small parts (propellers, rotor
blades, thin wings) are drawn bolder than life: the radar shows these icons
at 20-60 px, where a true-to-scale propeller would be under a pixel.
"""
import math

BOX = 512.0


class Shape:
    def __init__(self, span, length):
        self.span, self.length = span, length
        self.s = BOX * 0.98 / max(span, length)  # px per metre
        self.cx = BOX / 2
        self.top = (BOX - length * self.s) / 2   # y of the nose
        self.parts = []                          # svg elements

    # coordinates: x metres right of the centreline, y metres back from the nose
    def P(self, x, y):
        return (self.cx + x * self.s, self.top + y * self.s)

    def poly(self, pts, mirror=False, round_r=0.0):
        """A filled polygon; `round_r` (metres) rounds its corners (wing and tail tips, fins)."""
        for sign in ((1, -1) if mirror else (1,)):
            q = [self.P(sign * x, y) for x, y in pts]
            if round_r <= 0 or len(q) < 3:
                d = 'M ' + ' L '.join(f'{a:.1f} {b:.1f}' for a, b in q) + ' Z'
            else:
                r = round_r * self.s
                segs = []
                n = len(q)
                for i in range(n):
                    p0, p1, p2 = q[i - 1], q[i], q[(i + 1) % n]
                    l1 = math.hypot(p0[0] - p1[0], p0[1] - p1[1]) or 1.0
                    l2 = math.hypot(p2[0] - p1[0], p2[1] - p1[1]) or 1.0
                    t1, t2 = min(r / l1, 0.45), min(r / l2, 0.45)
                    a = (p1[0] + (p0[0] - p1[0]) * t1, p1[1] + (p0[1] - p1[1]) * t1)
                    b = (p1[0] + (p2[0] - p1[0]) * t2, p1[1] + (p2[1] - p1[1]) * t2)
                    segs.append((a, p1, b))
                d = f'M {segs[0][2][0]:.1f} {segs[0][2][1]:.1f}'
                for a, c, b in segs[1:] + segs[:1]:
                    d += f' L {a[0]:.1f} {a[1]:.1f} Q {c[0]:.1f} {c[1]:.1f} {b[0]:.1f} {b[1]:.1f}'
                d += ' Z'
            self.parts.append(f'<path d="{d}"/>')

    def ellipse(self, x, y, rx, ry, mirror=False, angle=0):
        for sign in ((1, -1) if mirror else (1,)):
            X, Y = self.P(sign * x, y)
            rot = f' transform="rotate({sign * angle:.1f} {X:.1f} {Y:.1f})"' if angle else ''
            self.parts.append(f'<ellipse cx="{X:.1f}" cy="{Y:.1f}" rx="{rx * self.s:.1f}" ry="{ry * self.s:.1f}"{rot}/>')

    def fuselage(self, width, nose=None, tail=None, tail_width=None, start=0.0, end=None, x=0.0):
        """A smooth body from `start` to `end` (default: the full length), centred `x` metres right."""
        end = self.length if end is None else end
        L = end - start
        nose = nose if nose is not None else min(L * 0.12, width * 1.6)
        tail = tail if tail is not None else L * 0.25
        tw = tail_width if tail_width is not None else width * 0.18
        w = width / 2
        pts = []
        n = 10
        for i in range(n + 1):  # nose: an ellipse quarter
            t = i / n
            pts.append((w * math.sin(t * math.pi / 2), start + nose * (1 - math.cos(t * math.pi / 2))))
        pts.append((w, end - tail))
        for i in range(1, n + 1):  # tail cone, gently tapering
            t = i / n
            pts.append((w + (tw / 2 - w) * (1 - math.cos(t * math.pi / 2)), end - tail + tail * t))
        full = [(x + px, py) for px, py in pts] + [(x - px, py) for px, py in reversed(pts)]
        self.poly(full)

    def wing(self, root_le, root_chord, half_span, sweep_deg, tip_chord, root_x=0.0, dihedral_y=0.0, kink=None,
             round_tip=None):
        """Left/right pair; `root_le` metres back from the nose; sweep of the leading edge.

        `kink`: fraction of the half span out to which the trailing edge runs
        straight across from the body (the inboard "Yehudi" of airliner wings),
        before sweeping back to the tip. Tips are rounded (`round_tip` metres,
        default about half the tip chord).
        """
        dx = half_span - root_x
        tsw = math.tan(math.radians(sweep_deg))
        tip_le = root_le + dx * tsw + dihedral_y
        r = round_tip if round_tip is not None else tip_chord * 0.5
        if kink:
            xk = root_x + dx * kink
            le_k = root_le + (xk - root_x) * tsw
            c_k = root_chord + (tip_chord - root_chord) * kink
            te_k = le_k + c_k
            te_root = max(te_k, root_le + root_chord)
            pts = [(root_x, root_le), (half_span, tip_le), (half_span, tip_le + tip_chord), (xk, te_k), (root_x, te_root)]
        else:
            pts = [(root_x, root_le), (half_span, tip_le), (half_span, tip_le + tip_chord), (root_x, root_le + root_chord)]
        self.poly(pts, mirror=True, round_r=r)

    def jet_pod(self, x, y, length, width, mirror=True, pylon=True):
        """An engine pod: round inlet in front, tapering to the exhaust; on a pylon to the wing behind it."""
        if pylon:
            pw = width * 0.28
            self.poly([(x - pw / 2, y + length * 0.5), (x + pw / 2, y + length * 0.5), (x + pw / 2, y + length * 1.15),
                       (x - pw / 2, y + length * 1.15)], mirror=mirror, round_r=pw * 0.4)
        w = width / 2
        pts = []
        n = 12
        for i in range(n + 1):  # front half: elliptical inlet lip
            t = i / n * math.pi / 2
            pts.append((w * math.sin(t), length * 0.45 * (1 - math.cos(t))))
        pts += [(w * 0.92, length * 0.7), (w * 0.62, length)]
        full = [(x + px, y + py) for px, py in pts] + [(x - px, y + py) for px, py in reversed(pts)]
        self.poly(full, mirror=mirror, round_r=w * 0.25)

    def prop(self, x, y, diameter, mirror=True, thick=None):
        """A spinning propeller seen from above: a thin disc tapering to its tips, with a spinner."""
        big = max(self.span, self.length)
        t = thick if thick is not None else max(diameter * 0.08, big * 0.04)
        d = max(diameter * 1.1, big * 0.15)
        for sign in ((1, -1) if mirror and x != 0 else (1,)):
            self.ellipse(sign * x, y, d / 2, t / 2)
            self.ellipse(sign * x, y, t * 0.75, t * 0.95)  # spinner

    def ring(self, x, y, r_out, r_in, mirror=False):
        """A filled ring (a propeller disc seen from above, an open circle)."""
        for sign in ((1, -1) if mirror and x != 0 else (1,)):
            X, Y = self.P(sign * x, y)
            ro, ri = r_out * self.s, r_in * self.s
            d = (f'M {X - ro:.1f} {Y:.1f} A {ro:.1f} {ro:.1f} 0 1 0 {X + ro:.1f} {Y:.1f} '
                 f'A {ro:.1f} {ro:.1f} 0 1 0 {X - ro:.1f} {Y:.1f} Z '
                 f'M {X - ri:.1f} {Y:.1f} A {ri:.1f} {ri:.1f} 0 1 1 {X + ri:.1f} {Y:.1f} '
                 f'A {ri:.1f} {ri:.1f} 0 1 1 {X - ri:.1f} {Y:.1f} Z')
            self.parts.append(f'<path fill-rule="evenodd" d="{d}"/>')

    def nacelle(self, x, y, length, width, mirror=True):
        self.ellipse(x, y + length / 2, width / 2, length / 2, mirror=mirror)

    def blade(self, cx, cy, radius, angle_deg, width):
        a = math.radians(angle_deg)
        ux, uy = math.sin(a), -math.cos(a)  # along the blade
        px, py = -uy, ux                    # across
        hw = width / 2
        pts = [(cx + px * hw * 0.8, cy + py * hw * 0.8), (cx + ux * radius * 0.25 + px * hw, cy + uy * radius * 0.25 + py * hw),
               (cx + ux * radius + px * hw * 0.75, cy + uy * radius + py * hw * 0.75),
               (cx + ux * radius - px * hw * 0.75, cy + uy * radius - py * hw * 0.75),
               (cx + ux * radius * 0.25 - px * hw, cy + uy * radius * 0.25 - py * hw), (cx - px * hw * 0.8, cy - py * hw * 0.8)]
        self.poly(pts, round_r=hw * 0.7)

    # Blade angles that keep every blade off the nose and the tail boom (a
    # blade lying along the body just looks like part of it).
    BLADE_OFFSET = {2: 45, 3: 90, 4: 45, 5: 18, 6: 30, 7: 13, 8: 22.5}

    def rotor(self, cx, cy, radius, blades, width, offset_deg=None):
        if offset_deg is None:
            offset_deg = self.BLADE_OFFSET.get(blades, 45)
        width = max(width, max(self.span, self.length) * 0.045)
        for i in range(blades):
            self.blade(cx, cy, radius, offset_deg + i * 360 / blades, width)
        self.ellipse(cx, cy, width * 0.9, width * 0.9)

    def svg(self):
        body = '\n  '.join(self.parts)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(BOX)}" height="{int(BOX)}" '
                f'viewBox="0 0 {int(BOX)} {int(BOX)}">\n <g fill="#000" stroke="#000" stroke-width="3" '
                f'stroke-linejoin="round">\n  {body}\n </g>\n</svg>\n')
