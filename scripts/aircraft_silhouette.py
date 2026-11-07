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

    def poly(self, pts, mirror=False):
        for sign in ((1, -1) if mirror else (1,)):
            q = [self.P(sign * x, y) for x, y in pts]
            d = 'M ' + ' L '.join(f'{a:.1f} {b:.1f}' for a, b in q) + ' Z'
            self.parts.append(f'<path d="{d}"/>')

    def ellipse(self, x, y, rx, ry, mirror=False, angle=0):
        for sign in ((1, -1) if mirror else (1,)):
            X, Y = self.P(sign * x, y)
            rot = f' transform="rotate({sign * angle:.1f} {X:.1f} {Y:.1f})"' if angle else ''
            self.parts.append(f'<ellipse cx="{X:.1f}" cy="{Y:.1f}" rx="{rx * self.s:.1f}" ry="{ry * self.s:.1f}"{rot}/>')

    def fuselage(self, width, nose=None, tail=None, tail_width=None, start=0.0, end=None):
        """A smooth body from `start` to `end` (default: the full length)."""
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
        full = pts + [(-x, y) for x, y in reversed(pts)]
        self.poly(full)

    def wing(self, root_le, root_chord, half_span, sweep_deg, tip_chord, root_x=0.0, dihedral_y=0.0):
        """Left/right pair; `root_le` metres back from the nose; sweep of the leading edge."""
        dx = half_span - root_x
        tip_le = root_le + dx * math.tan(math.radians(sweep_deg)) + dihedral_y
        pts = [(root_x, root_le), (half_span, tip_le), (half_span, tip_le + tip_chord), (root_x, root_le + root_chord)]
        self.poly(pts, mirror=True)

    def jet_pod(self, x, y, length, width, mirror=True):
        self.ellipse(x, y + length / 2, width / 2, length / 2, mirror=mirror)

    def prop(self, x, y, diameter, mirror=True, thick=None):
        """A propeller disc seen from above: a thin bar across."""
        big = max(self.span, self.length)
        t = thick if thick is not None else max(diameter * 0.08, big * 0.045)
        d = max(diameter * 1.15, big * 0.16)
        self.poly([(x - d / 2, y - t / 2), (x + d / 2, y - t / 2), (x + d / 2, y + t / 2),
                   (x - d / 2, y + t / 2)], mirror=mirror and x != 0)

    def nacelle(self, x, y, length, width, mirror=True):
        self.ellipse(x, y + length / 2, width / 2, length / 2, mirror=mirror)

    def blade(self, cx, cy, radius, angle_deg, width):
        a = math.radians(angle_deg)
        ux, uy = math.sin(a), -math.cos(a)  # along the blade
        px, py = -uy, ux                    # across
        hw = width / 2
        pts = [(cx + px * hw, cy + py * hw), (cx + ux * radius + px * hw * 0.7, cy + uy * radius + py * hw * 0.7),
               (cx + ux * radius - px * hw * 0.7, cy + uy * radius - py * hw * 0.7), (cx - px * hw, cy - py * hw)]
        self.poly(pts)

    def rotor(self, cx, cy, radius, blades, width, offset_deg=45):
        width = max(width, max(self.span, self.length) * 0.045)
        for i in range(blades):
            self.blade(cx, cy, radius, offset_deg + i * 360 / blades, width)
        self.ellipse(cx, cy, width * 0.9, width * 0.9)

    def svg(self):
        body = '\n  '.join(self.parts)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{int(BOX)}" height="{int(BOX)}" '
                f'viewBox="0 0 {int(BOX)} {int(BOX)}">\n <g fill="#000" stroke="#000" stroke-width="3" '
                f'stroke-linejoin="round">\n  {body}\n </g>\n</svg>\n')
