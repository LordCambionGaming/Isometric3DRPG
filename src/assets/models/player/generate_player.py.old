#!/usr/bin/env python3
"""
Procedural low-poly fantasy RPG player (1.8 m tall, <= 0.6 m footprint), static or rigged + animated.

Usage (static mesh, identical geometry, no rig - works inside Blender or with plain Python):
    python3 tools/generate_player.py --out assets/models/player/player.glb

Usage (rigged mesh with Idle / Walk / Attack, pure Python, no Blender needed):
    python3 tools/generate_player.py --animated --out assets/models/player/player_animated.glb

Conventions
    * Authoring space is Blender's: +Z up, character faces -Y, feet on Z=0, origin at feet centre.
    * glTF export maps (x, y, z) -> (x, z, -y), so in the game the model is Y-up, faces +Z,
      and its pivot (0,0,0) is at the feet.  +X is the character's LEFT side.
    * 1 unit = 1 metre.
    * Flat colours per material + baked directional shading in vertex colours
      (the game currently draws unlit; BAKE_SHADING=False disables the baking).

Rig (17 joints, rigid 1-bone-per-part skinning, one skin)
    Hips (root, 0.95 m) -> Spine -> Chest -> Neck -> Head
                                    Chest -> UpperArm.L/R -> Forearm.L/R -> Hand.L/R
                           Hips  -> Thigh.L/R -> Shin.L/R -> Foot.L/R
    Animations (glTF order, in-place, root motion is NOT baked in):
        0 Idle    2.4 s  loop
        1 Walk    1.0 s  loop (two steps), feet kept on the ground by a small FK solver
        2 Attack  0.8 s  one-shot, right-hand overhead strike, impact at 0.34 s, ends back in the Idle pose
"""
import math
import struct
import json
import sys
import os

try:
    import bpy
    HAVE_BPY = True
except ImportError:
    HAVE_BPY = False

TARGET_HEIGHT = 1.8
MAX_FOOTPRINT = 0.6
BAKE_SHADING = True

MATERIALS = {
    "skin":    (0.87, 0.67, 0.52),
    "hair":    (0.93, 0.72, 0.22),
    "eye":     (0.22, 0.55, 0.95),
    "tunic":   (0.17, 0.28, 0.38),
    "pants":   (0.20, 0.17, 0.15),
    "leather": (0.32, 0.20, 0.10),
    "sole":    (0.12, 0.09, 0.07),
    "armor":   (0.46, 0.49, 0.54),
    "gold":    (0.80, 0.62, 0.18),
}
MAT_ORDER = list(MATERIALS.keys())

HIP_Z, THIGH_Z, KNEE_Z, ANKLE_Z = 0.95, 0.92, 0.47, 0.08
THIGH_LEN = THIGH_Z - KNEE_Z
SHIN_LEN = KNEE_Z - ANKLE_Z
HIP_TO_THIGH = HIP_Z - THIGH_Z
FOOT_HEEL, FOOT_TOE = 0.09, 0.18

SKELETON = [
    ("Hips",  None,    (0.0, 0.0, HIP_Z)),
    ("Spine", "Hips",  (0.0, 0.0, 1.02)),
    ("Chest", "Spine", (0.0, 0.0, 1.18)),
    ("Neck",  "Chest", (0.0, 0.0, 1.46)),
    ("Head",  "Neck",  (0.0, 0.0, 1.54)),
]
for _s, _side in ((+1, "L"), (-1, "R")):
    SKELETON += [
        ("UpperArm." + _side, "Chest",              (0.235 * _s, 0.0, 1.42)),
        ("Forearm." + _side,  "UpperArm." + _side,  (0.235 * _s, 0.0, 1.14)),
        ("Hand." + _side,     "Forearm." + _side,   (0.235 * _s, 0.0, 0.88)),
        ("Thigh." + _side,    "Hips",               (0.095 * _s, 0.0, THIGH_Z)),
        ("Shin." + _side,     "Thigh." + _side,     (0.095 * _s, 0.0, KNEE_Z)),
        ("Foot." + _side,     "Shin." + _side,      (0.095 * _s, 0.0, ANKLE_Z)),
    ]
BONE_INDEX = {b[0]: i for i, b in enumerate(SKELETON)}

class Mesh:
    def __init__(self):
        self.polys = []
        self.bone = None

    def add(self, mat, verts, faces):
        for f in faces:
            self.polys.append((mat, [verts[i] for i in f], self.bone))


def frustum(m, mat, cx, cy, z0, z1, w0, d0, w1=None, d1=None, dx1=0.0, dy1=0.0):
    w1 = w0 if w1 is None else w1
    d1 = d0 if d1 is None else d1
    b = [(-w0 / 2, -d0 / 2), (w0 / 2, -d0 / 2), (w0 / 2, d0 / 2), (-w0 / 2, d0 / 2)]
    t = [(-w1 / 2, -d1 / 2), (w1 / 2, -d1 / 2), (w1 / 2, d1 / 2), (-w1 / 2, d1 / 2)]
    v = [(cx + x, cy + y, z0) for x, y in b] + [(cx + dx1 + x, cy + dy1 + y, z1) for x, y in t]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    m.add(mat, v, faces)


def prism(m, mat, cx, cy, z0, z1, r0, r1, n=8, sy=1.0):
    ring0, ring1 = [], []
    for k in range(n):
        a = (k + 0.5) * 2 * math.pi / n
        ring0.append((cx + r0 * math.cos(a), cy + r0 * math.sin(a) * sy, z0))
        ring1.append((cx + r1 * math.cos(a), cy + r1 * math.sin(a) * sy, z1))
    v = ring0 + ring1
    faces = [tuple(reversed(range(n))), tuple(range(n, 2 * n))]
    for k in range(n):
        j = (k + 1) % n
        faces.append((k, j, n + j, n + k))
    m.add(mat, v, faces)


def build_player():
    m = Mesh()

    for s, side in ((+1, "L"), (-1, "R")):
        x = 0.095 * s
        m.bone = "Thigh." + side
        frustum(m, "pants", x, 0.0, 0.45, 0.95, 0.125, 0.150, 0.155, 0.180)

        m.bone = "Shin." + side
        frustum(m, "pants", x, 0.0, 0.24, 0.49, 0.105, 0.125, 0.125, 0.150)
        frustum(m, "leather", x, 0.0, 0.10, 0.27, 0.115, 0.135, 0.125, 0.145)
        frustum(m, "leather", x, 0.0, 0.26, 0.285, 0.130, 0.150)

        m.bone = "Foot." + side
        frustum(m, "leather", x, -0.045, 0.02, 0.11, 0.105, 0.270, 0.100, 0.200, dy1=0.030)
        frustum(m, "sole", x, -0.045, 0.0, 0.025, 0.108, 0.275)

    m.bone = "Hips"
    frustum(m, "pants", 0, 0, 0.80, 1.00, 0.300, 0.190, 0.310, 0.200)
    frustum(m, "tunic", 0, 0, 0.76, 1.00, 0.370, 0.230, 0.310, 0.205)
    frustum(m, "leather", 0, 0, 0.97, 1.04, 0.325, 0.215)
    frustum(m, "gold", 0, -0.1105, 0.985, 1.03, 0.050, 0.020)

    m.bone = "Spine"
    frustum(m, "tunic", 0, 0, 0.98, 1.20, 0.310, 0.200, 0.335, 0.210)

    m.bone = "Chest"
    frustum(m, "tunic", 0, 0, 1.18, 1.48, 0.335, 0.210, 0.400, 0.235)
    frustum(m, "armor", 0, 0, 1.24, 1.46, 0.345, 0.222, 0.410, 0.250)
    frustum(m, "gold", 0, 0, 1.225, 1.250, 0.350, 0.228)
    prism(m, "armor", 0, 0, 1.46, 1.50, 0.080, 0.062, n=8)

    for s, side in ((+1, "L"), (-1, "R")):
        x = 0.235 * s
        m.bone = "UpperArm." + side
        frustum(m, "armor", 0.24 * s, 0.0, 1.33, 1.47, 0.115, 0.160, 0.100, 0.140)
        frustum(m, "tunic", x, 0.0, 1.12, 1.40, 0.078, 0.088, 0.088, 0.098)

        m.bone = "Forearm." + side
        frustum(m, "skin", x, 0.0, 0.88, 1.17, 0.062, 0.072, 0.074, 0.084)
        frustum(m, "leather", x, 0.0, 0.92, 1.08, 0.078, 0.088, 0.086, 0.096)

        m.bone = "Hand." + side
        frustum(m, "skin", x, -0.005, 0.77, 0.89, 0.065, 0.080, 0.070, 0.085)

    m.bone = "Neck"
    prism(m, "skin", 0, 0, 1.44, 1.56, 0.048, 0.048, n=6)

    # HEAD ONLY — young fairy-tale knight (original storybook-prince look: soft but
    # manly jaw, bright blue eyes, golden swept hair with a side fringe).
    # Everything above this block is the untouched body; only the Head bone is built
    # here.  Skull and hair are lofted from the same profile table, so the hair
    # follows the cranium by construction instead of floating on top of it.
    m.bone = "Head"

    HN = 12                                                   # segments around the head
    ANG = [(15.0 + 360.0 / HN * k) * math.pi / 180.0 for k in range(HN)]  # flat front/back/side facets

    def sp(c, p):                                             # superellipse helper (p=2 -> ellipse)
        return math.copysign(abs(c) ** (2.0 / p), c)

    def vsub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
    def vadd(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
    def vmul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
    def vdot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
    def vcross(a, b):
        return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    def vunit(a):
        ln = math.sqrt(vdot(a, a)) or 1.0
        return (a[0] / ln, a[1] / ln, a[2] / ln)

    # Skull / face profile, chin -> crown: (z, half-width x, half-depth y, centre y, squareness).
    # Last ring is virtual (only lets the hair dome extrapolate above the skull cap).
    SKULL = [
        (1.545, 0.047, 0.064, -0.014, 2.3),    # under-jaw, swallows the neck top
        (1.564, 0.052, 0.070, -0.020, 2.3),    # chin
        (1.592, 0.064, 0.078, -0.016, 2.4),    # jaw / mouth level (squarer = manly)
        (1.622, 0.072, 0.087, -0.007, 2.3),    # soft cheeks
        (1.656, 0.0745, 0.091, -0.003, 2.2),   # eye level, widest
        (1.690, 0.0735, 0.093,  0.000, 2.1),   # brow / temples
        (1.722, 0.069, 0.090,  0.003, 2.0),    # forehead
        (1.750, 0.059, 0.078,  0.005, 2.0),
        (1.772, 0.042, 0.056,  0.006, 2.0),    # skull cap
        (1.792, 0.012, 0.020,  0.006, 2.0),    # virtual
    ]

    def skull_at(z):
        if z <= SKULL[0][0]:
            return SKULL[0][1:]
        for i in range(len(SKULL) - 1):
            a, b = SKULL[i], SKULL[i + 1]
            if z <= b[0]:
                u = (z - a[0]) / (b[0] - a[0])
                return tuple(a[k] + (b[k] - a[k]) * u for k in range(1, 5))
        return SKULL[-1][1:]

    def loft(mat, rings, n, a0=15.0, roll=0.0, pivot=(0.0, 0.0)):
        # rings: (z, half-width, half-depth, centre x, centre y, squareness)
        cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
        v = []
        for z, ax, by, cx, cy, p in rings:
            for k in range(n):
                a = math.radians(a0 + 360.0 / n * k)
                x, y = cx + ax * sp(math.cos(a), p), cy + by * sp(math.sin(a), p)
                if roll:
                    dx, dz = x - pivot[0], z - pivot[1]
                    x, z2 = pivot[0] + dx * cr + dz * sr, pivot[1] - dx * sr + dz * cr
                else:
                    z2 = z
                v.append((x, y, z2))
        faces = [tuple(reversed(range(n)))]
        for r in range(len(rings) - 1):
            for k in range(n):
                j = (k + 1) % n
                faces.append((r * n + k, r * n + j, (r + 1) * n + j, (r + 1) * n + k))
        faces.append(tuple(range((len(rings) - 1) * n, len(rings) * n)))
        m.add(mat, v, faces)

    def sweep(mat, path, tip=False):
        # Tapered ribbon along a polyline: path = [(point, outward normal, width, thickness)].
        # With tip=True the last entry is just the apex position.
        n = len(path)
        full = n - 1 if tip else n
        verts, centres = [], []
        for i in range(full):
            p, nrm, w, h = path[i]
            T = vunit(vsub(path[min(i + 1, n - 1)][0], path[max(i - 1, 0)][0]))
            S = vunit(vcross(T, nrm))
            N2 = vunit(vcross(S, T))
            for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
                verts.append(vadd(vadd(p, vmul(S, sx * w / 2)), vmul(N2, sy * h / 2)))
            centres.append(p)

        def orient(f, ref):
            pts = [verts[i] for i in f]
            cen = tuple(sum(q[c] for q in pts) / len(pts) for c in range(3))
            return f if vdot(face_normal(pts), vsub(cen, ref)) >= 0 else tuple(reversed(f))

        faces = []
        for i in range(full - 1):
            ref = vmul(vadd(centres[i], centres[i + 1]), 0.5)
            for c in range(4):
                d = (c + 1) % 4
                faces.append(orient((i * 4 + c, i * 4 + d, (i + 1) * 4 + d, (i + 1) * 4 + c), ref))
        faces.append(orient((0, 1, 2, 3), centres[1]))
        if tip:
            verts.append(path[-1][0])
            last = (full - 1) * 4
            for c in range(4):
                faces.append(orient((last + c, last + (c + 1) % 4, full * 4), centres[-1]))
        else:
            faces.append(orient(((full - 1) * 4, (full - 1) * 4 + 1, (full - 1) * 4 + 2, (full - 1) * 4 + 3),
                                centres[-2]))
        m.add(mat, verts, faces)

    def ring_poly(z):
        ax, by, cy, p = skull_at(z)
        return [(ax * sp(math.cos(a), p), cy + by * sp(math.sin(a), p)) for a in ANG]

    def face_y(x, z):                                         # y of the front of the face at (x, z)
        pts, best = ring_poly(z), None
        for i in range(HN):
            (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % HN]
            if x0 != x1 and min(x0, x1) <= x <= max(x0, x1):
                y = y0 + (y1 - y0) * (x - x0) / (x1 - x0)
                best = y if best is None else min(best, y)
        return best

    # --- Skull: rounded cranium, soft cheeks, tapered but manly jaw -------------------
    loft("skin", [(z, ax, by, 0.0, cy, p) for z, ax, by, cy, p in SKULL[:-1]], HN)

    # --- Eyes: big almond blue eyes, slightly proud of the face ------------------------
    EYE_Z = 1.658
    for s in (+1, -1):
        ex = 0.034 * s
        ey = face_y(ex, EYE_Z) + 0.0005
        loft("eye", [(EYE_Z - 0.011, 0.0085, 0.0035, ex, ey, 2.0),
                     (EYE_Z,         0.0155, 0.0068, ex, ey, 2.0),
                     (EYE_Z + 0.011, 0.0105, 0.0042, ex, ey, 2.0)],
             8, a0=22.5, roll=3.0 * s, pivot=(ex, EYE_Z))

    # --- Eyebrows: soft arch, friendly ---------------------------------------------------
    FRONT = (0.0, -1.0, 0.0)
    for s in (+1, -1):
        pts = [(0.012, 1.683, 0.0105, 0.0060), (0.029, 1.691, 0.0115, 0.0065),
               (0.046, 1.691, 0.0095, 0.0060), (0.059, 1.683, 0.0050, 0.0040)]
        sweep("hair", [((x * s, face_y(x * s, z) - 0.0005, z), FRONT, w, h) for x, z, w, h in pts])

    # --- Nose: small and simple wedge ----------------------------------------------------
    sweep("skin", [((0.0, face_y(0.0, 1.664) - 0.0010, 1.664), FRONT, 0.010, 0.006),
                   ((0.0, face_y(0.0, 1.647) - 0.0035, 1.647), FRONT, 0.014, 0.012),
                   ((0.0, face_y(0.0, 1.631) - 0.0050, 1.631), FRONT, 0.021, 0.018)])

    # --- Mouth: small lip band with a gentle smile ---------------------------------------
    MZ = 1.602
    mouth = [(-0.0230, MZ + 0.0050, 0.0030, 0.0040), (-0.0125, MZ + 0.0010, 0.0075, 0.0065),
             (0.0, MZ - 0.0018, 0.0095, 0.0070),
             (0.0125, MZ + 0.0010, 0.0075, 0.0065), (0.0230, MZ + 0.0050, 0.0030, 0.0040)]
    sweep("skin", [((x, face_y(x, z) - 0.0005, z), FRONT, w, h) for x, z, w, h in mouth])

    # --- Ears: small, set just behind the temples ---------------------------------------
    for s in (+1, -1):
        cx0 = 0.0775 * s
        loft("skin", [(1.628, 0.0040, 0.0060, cx0,             0.014, 2.0),
                      (1.650, 0.0065, 0.0115, cx0 + 0.0015 * s, 0.014, 2.0),
                      (1.674, 0.0040, 0.0060, cx0 + 0.0030 * s, 0.014, 2.0)], 6, a0=30.0)

    # --- Golden hair: thin shell hugging the skull + a few readable locks ------------------
    # Hairline per column (front-left of the face is +x): high forehead, short sideburns in
    # front of the ears, ears left free, tidy tapered nape.
    EDGE = [1.684, 1.656, 1.626, 1.626, 1.656, 1.684, 1.660, 1.686, 1.718, 1.718, 1.686, 1.660]
    ZTOP = 1.788
    LEVELS = (0.0, 0.30, 0.58, 0.80, 1.0)

    def hair_t(z):                                            # shell thickness, thicker on the crown
        return 0.006 + 0.006 * min(max((z - 1.70) / 0.06, 0.0), 1.0)

    hv = []
    for u in LEVELS:
        for k in range(HN):
            z = EDGE[k] + (ZTOP - EDGE[k]) * u
            ax, by, cy, p = skull_at(z)
            t = hair_t(z)
            hv.append(((ax + t) * sp(math.cos(ANG[k]), p), cy + (by + t) * sp(math.sin(ANG[k]), p), z))
    RIM = len(hv)
    for k in range(HN):                                       # inner rim, resting on the scalp
        ax, by, cy, p = skull_at(EDGE[k])
        hv.append(((ax - 0.001) * sp(math.cos(ANG[k]), p), cy + (by - 0.001) * sp(math.sin(ANG[k]), p), EDGE[k]))
    APEX = len(hv)
    hv.append((0.0, skull_at(ZTOP)[2], ZTOP + 0.005))
    hf = []
    for j in range(len(LEVELS) - 1):
        for k in range(HN):
            kk = (k + 1) % HN
            hf.append((j * HN + k, j * HN + kk, (j + 1) * HN + kk, (j + 1) * HN + k))
    top = (len(LEVELS) - 1) * HN
    for k in range(HN):
        hf.append((top + k, top + (k + 1) % HN, APEX))
        hf.append((k, RIM + k, RIM + (k + 1) % HN, (k + 1) % HN))
    m.add("hair", hv, hf)

    def surf(a_deg, z):                                       # point on the skull surface
        ax, by, cy, p = skull_at(z)
        a = math.radians(a_deg)
        return (ax * sp(math.cos(a), p), cy + by * sp(math.sin(a), p), z)

    def surf_n(a_deg, z):
        p0 = surf(a_deg, z)
        n = vunit(vcross(vsub(surf(a_deg + 2.0, z), surf(a_deg - 2.0, z)),
                         vsub(surf(a_deg, z + 0.002), surf(a_deg, z - 0.002))))
        if vdot(n, vsub(p0, (0.0, skull_at(z)[2], 1.690))) < 0:
            n = vmul(n, -1.0)
        return p0, n

    def lock(specs):                                          # (angle, z, width, thickness, lift)
        path = []
        for a_deg, z, w, h, lift in specs:
            p0, n = surf_n(a_deg, z)
            path.append((vadd(p0, vmul(n, hair_t(z) + h * 0.5 - 0.002 + lift)), n, w, h))
        sweep("hair", path, tip=True)

    # Dominant side-swept fringe: from the part, over the forehead, ending above the left brow.
    lock([(235, 1.775, 0.016, 0.009, 0.000), (258, 1.757, 0.040, 0.018, 0.000),
          (280, 1.738, 0.042, 0.018, 0.000), (300, 1.720, 0.031, 0.014, 0.000),
          (316, 1.708, 0.017, 0.009, 0.002), (326, 1.704, 0.0, 0.0, 0.005)])
    # Second, shorter lock layered under it.
    lock([(248, 1.762, 0.016, 0.009, 0.000), (268, 1.743, 0.026, 0.012, 0.000),
          (288, 1.724, 0.022, 0.010, 0.000), (303, 1.713, 0.012, 0.006, 0.002),
          (312, 1.709, 0.0, 0.0, 0.004)])
    # Short lock at the right temple, hugging the head.
    lock([(215, 1.768, 0.014, 0.008, 0.000), (206, 1.749, 0.022, 0.011, 0.000),
          (198, 1.730, 0.019, 0.009, 0.000), (194, 1.712, 0.011, 0.006, 0.001),
          (192, 1.703, 0.0, 0.0, 0.003)])
    # Two subtle tufts (left/back) so the haircut reads tidy-but-tousled; tips barely lift.
    lock([(40, 1.770, 0.016, 0.008, 0.000), (28, 1.748, 0.024, 0.011, 0.000),
          (15, 1.726, 0.020, 0.009, 0.002), (8, 1.708, 0.0, 0.0, 0.006)])
    lock([(80, 1.768, 0.018, 0.009, 0.000), (88, 1.745, 0.026, 0.012, 0.000),
          (96, 1.721, 0.020, 0.010, 0.002), (102, 1.700, 0.0, 0.0, 0.007)])

    return m


LIGHT = (-0.35, -0.5, 0.8)
_l = math.sqrt(sum(c * c for c in LIGHT))
LIGHT = tuple(c / _l for c in LIGHT)


def face_normal(pts):
    nx = ny = nz = 0.0
    for i in range(len(pts)):
        x0, y0, z0 = pts[i]
        x1, y1, z1 = pts[(i + 1) % len(pts)]
        nx += (y0 - y1) * (z0 + z1)
        ny += (z0 - z1) * (x0 + x1)
        nz += (x0 - x1) * (y0 + y1)
    ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return (nx / ln, ny / ln, nz / ln)


def shade(n):
    if not BAKE_SHADING:
        return 1.0
    d = max(0.0, sum(a * b for a, b in zip(n, LIGHT)))
    return min(1.0, (0.58 + 0.42 * d) / 0.92)


def bounds(mesh):
    xs = [p[0] for _, poly, _b in mesh.polys for p in poly]
    ys = [p[1] for _, poly, _b in mesh.polys for p in poly]
    zs = [p[2] for _, poly, _b in mesh.polys for p in poly]
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def report(mesh):
    lo, hi = bounds(mesh)
    print("Blender-space bounds  min=(%.3f, %.3f, %.3f)  max=(%.3f, %.3f, %.3f)" % (*lo, *hi))
    print("Height %.3f m | width(x) %.3f m | depth(y) %.3f m | tris %d" % (
        hi[2] - lo[2], hi[0] - lo[0], hi[1] - lo[1],
        sum(len(p) - 2 for _, p, _b in mesh.polys)))
    assert abs(lo[2]) < 1e-6, "feet must sit on Z=0"
    assert abs((hi[2] - lo[2]) - TARGET_HEIGHT) < 0.01, "height must be ~1.8 m"
    assert (hi[0] - lo[0]) <= MAX_FOOTPRINT + 1e-6 and (hi[1] - lo[1]) <= MAX_FOOTPRINT + 1e-6, \
        "footprint must be <= 0.6 m"
    assert all(b in BONE_INDEX for _, _p, b in mesh.polys), "every polygon needs a valid bone"


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qx(a): return (math.sin(a / 2), 0.0, 0.0, math.cos(a / 2))
def qy(a): return (0.0, math.sin(a / 2), 0.0, math.cos(a / 2))
def qz(a): return (0.0, 0.0, math.sin(a / 2), math.cos(a / 2))


D = math.radians


def leg_low(a1, k, f):
    a2 = a1 - k
    drop = HIP_TO_THIGH + THIGH_LEN * math.cos(a1) + SHIN_LEN * math.cos(a2)
    low = 0.0
    for dz, dy in ((-FOOT_HEEL, -ANKLE_Z), (FOOT_TOE, -ANKLE_Z)):
        y = dz * math.sin(f) + dy * math.cos(f)
        low = min(low, y)
    return -drop + low


def solve(P):
    g = lambda k: P.get(k, 0.0)
    angs, lows = {}, []
    for s in "LR":
        a1, k, f = D(g(s + "thigh")), D(g(s + "knee")), D(g(s + "foot"))
        angs[s] = (a1, k, f)
        lows.append(leg_low(a1, k, f))
    hips_y = -min(lows)

    r = {}
    r["Hips"] = qy(D(g("hips_yaw")))
    r["Spine"] = qmul(qy(D(g("spine_yaw"))), qx(D(g("spine_pitch"))))
    r["Chest"] = qmul(qy(D(g("chest_yaw"))), qx(D(g("chest_pitch"))))
    r["Neck"] = qx(D(g("neck_pitch")))
    r["Head"] = qmul(qy(D(g("head_yaw"))), qx(D(g("head_pitch"))))
    for s, sign in (("L", 1.0), ("R", -1.0)):
        r["UpperArm." + s] = qmul(qz(sign * D(g(s + "abd"))), qx(-D(g(s + "swing"))))
        r["Forearm." + s] = qx(-D(g(s + "elbow")))
        r["Hand." + s] = qx(D(g(s + "hand")))
        a1, k, f = angs[s]
        r["Thigh." + s] = qmul(qy(-D(g("hips_yaw"))), qx(-a1))
        r["Shin." + s] = qx(k)
        r["Foot." + s] = qx(a1 - k - f)
    return r, (g("hips_dx"), hips_y, g("hips_dz"))


def pose_idle(t):
    T = 2.4
    w = 2 * math.pi * t / T
    b = math.sin(w)
    P = {"Lknee": 3, "Rknee": 3,
         "chest_pitch": -1.2 * b, "spine_pitch": 0.5 * b,
         "neck_pitch": 0.6 * b, "head_yaw": 2.0 * math.sin(w), "hips_yaw": 0.6 * math.sin(w),
         "spine_yaw": -0.8 * math.sin(w), "hips_dx": 0.004 * math.sin(w)}
    for s, ph in (("L", 0.4), ("R", 1.0)):
        P[s + "abd"] = 5 + 1.0 * b
        P[s + "elbow"] = 10 + 1.5 * math.sin(w + ph)
        P[s + "swing"] = 1.5 * math.sin(w + ph)
    return P


def pose_walk(t):
    ph = (t / 1.0) % 1.0
    two_pi = 2 * math.pi
    A, AA = 24.0, 24.0
    P = {"hips_yaw": -5 * math.cos(two_pi * ph), "chest_yaw": 6 * math.cos(two_pi * ph),
         "spine_yaw": 2 * math.cos(two_pi * ph), "head_yaw": -3 * math.cos(two_pi * ph),
         "spine_pitch": 4, "chest_pitch": 1, "neck_pitch": -2,
         "hips_dx": 0.012 * math.sin(two_pi * ph)}
    for s, off in (("L", 0.0), ("R", 0.5)):
        p = (ph + off) % 1.0
        P[s + "thigh"] = A * math.cos(two_pi * p)
        if p < 0.5:
            P[s + "knee"] = 4 + 6 * math.sin(two_pi * p)
        else:
            P[s + "knee"] = 4 + 52 * math.sin(two_pi * (p - 0.5))
        P[s + "foot"] = 8 * max(0.0, math.cos(two_pi * p)) ** 2
        sw = -AA * math.cos(two_pi * p)
        P[s + "swing"] = sw
        P[s + "abd"] = 4
        P[s + "elbow"] = 14 + 10 * max(0.0, sw / AA)
    return P


ATTACK_BASE = {"Lknee": 3, "Rknee": 3, "Labd": 5, "Rabd": 5, "Lelbow": 10, "Relbow": 10}


def _K(**kw):
    d = dict(ATTACK_BASE)
    d.update(kw)
    return d


def _smooth(u): return u * u * (3 - 2 * u)
def _ease_in(u): return u * u
def _ease_out(u): return 1 - (1 - u) * (1 - u)


ATTACK_HIT_TIME = 0.34
ATTACK_KEYS = [
    (0.00, _smooth, _K()),
    (0.20, _smooth, _K(Rswing=140, Rabd=12, Relbow=80, Lswing=15, Labd=18, Lelbow=35,
                       hips_yaw=-10, spine_yaw=-10, chest_yaw=-20, spine_pitch=-2, chest_pitch=-5, head_yaw=12,
                       Lthigh=4, Lknee=14, Rthigh=-4, Rknee=14)),
    (0.34, _ease_in, _K(Rswing=70, Rabd=6, Relbow=20, Lswing=-20, Labd=24, Lelbow=40,
                        hips_yaw=10, spine_yaw=10, chest_yaw=20, spine_pitch=12, chest_pitch=8,
                        head_pitch=4, head_yaw=-18,
                        Lthigh=32, Lknee=30, Rthigh=-22, Rknee=8)),
    (0.46, _ease_out, _K(Rswing=85, Rabd=8, Relbow=18, Lswing=-22, Labd=24, Lelbow=40,
                         hips_yaw=12, spine_yaw=10, chest_yaw=26, spine_pitch=14, chest_pitch=8,
                         head_pitch=4, head_yaw=-22,
                         Lthigh=32, Lknee=30, Rthigh=-22, Rknee=8)),
    (0.62, _smooth, _K(Rswing=35, Rabd=6, Relbow=25, Lswing=-8, Labd=12, Lelbow=25,
                       hips_yaw=5, spine_yaw=5, chest_yaw=12, spine_pitch=7, chest_pitch=4,
                       head_pitch=2, head_yaw=-10,
                       Lthigh=14, Lknee=18, Rthigh=-10, Rknee=10)),
    (0.80, _smooth, _K()),
]


def pose_attack(t):
    keys = ATTACK_KEYS
    t = min(max(t, keys[0][0]), keys[-1][0])
    for i in range(len(keys) - 1):
        t0, _, a = keys[i]
        t1, ease, b = keys[i + 1]
        if t <= t1:
            e = ease((t - t0) / (t1 - t0))
            names = set(a) | set(b)
            return {n: a.get(n, 0.0) + (b.get(n, 0.0) - a.get(n, 0.0)) * e for n in names}
    return dict(keys[-1][2])


ANIMATIONS = [
    ("Idle",   2.4, 30, pose_idle,   {"loop": True}),
    ("Walk",   1.0, 30, pose_walk,   {"loop": True}),
    ("Attack", 0.8, 60, pose_attack, {"loop": False, "hit_time": ATTACK_HIT_TIME}),
]


def sample_animation(fn, duration, fps):
    n = int(round(duration * fps))
    times = [k / fps for k in range(n + 1)]
    rots = {b[0]: [] for b in SKELETON}
    hips = []
    for t in times:
        r, h = solve(fn(t))
        for name, q in r.items():
            if rots[name]:
                p = rots[name][-1]
                if sum(a * b for a, b in zip(p, q)) < 0:
                    q = tuple(-c for c in q)
            rots[name].append(q)
        hips.append(h)
    return times, rots, hips


def write_glb(mesh, path, rigged=False):
    def to_y_up(p):
        return (p[0], p[2], -p[1])

    blob = bytearray()
    views, accessors, prims, mats = [], [], [], []

    def add_view(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        off = len(blob)
        blob.extend(data)
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if target:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    def add_acc(data, ctype, count, atype, target=None, mn=None, mx=None, normalized=False):
        a = {"bufferView": add_view(data, target), "componentType": ctype, "count": count, "type": atype}
        if normalized:
            a["normalized"] = True
        if mn is not None:
            a["min"], a["max"] = mn, mx
        accessors.append(a)
        return len(accessors) - 1

    for name in MAT_ORDER:
        polys = [(p, b) for mat, p, b in mesh.polys if mat == name]
        if not polys:
            continue
        pos, nor, col, jnt, idx = [], [], [], [], []
        for poly, bone in polys:
            n = face_normal(poly)
            b = shade(n)
            base = len(pos)
            for p in poly:
                pos.append(to_y_up(p))
                nor.append(to_y_up(n))
                col.append((int(round(b * 255)),) * 3 + (255,))
                jnt.append(BONE_INDEX[bone])
            for k in range(1, len(poly) - 1):
                idx += [base, base + k, base + k + 1]
        assert len(pos) < 65535
        pmin = [min(p[i] for p in pos) for i in range(3)]
        pmax = [max(p[i] for p in pos) for i in range(3)]
        a_pos = add_acc(b"".join(struct.pack("<3f", *p) for p in pos), 5126, len(pos), "VEC3", 34962, pmin, pmax)
        a_nor = add_acc(b"".join(struct.pack("<3f", *p) for p in nor), 5126, len(nor), "VEC3", 34962)
        a_col = add_acc(b"".join(struct.pack("<4B", *c) for c in col), 5121, len(col), "VEC4", 34962, normalized=True)
        attrs = {"POSITION": a_pos, "NORMAL": a_nor, "COLOR_0": a_col}
        if rigged:
            attrs["JOINTS_0"] = add_acc(b"".join(struct.pack("<4B", j, 0, 0, 0) for j in jnt), 5121, len(jnt), "VEC4", 34962)
            attrs["WEIGHTS_0"] = add_acc(b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 0.0) for _ in jnt), 5126, len(jnt), "VEC4", 34962)
        a_idx = add_acc(b"".join(struct.pack("<H", i) for i in idx), 5123, len(idx), "SCALAR", 34963)
        r, g, bl = MATERIALS[name]
        mats.append({"name": name, "pbrMetallicRoughness": {
            "baseColorFactor": [r, g, bl, 1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0}})
        prims.append({"attributes": attrs, "indices": a_idx, "material": len(mats) - 1, "mode": 4})

    gltf = {
        "asset": {"version": "2.0", "generator": "generate_player.py"},
        "scene": 0,
        "meshes": [{"name": "Player", "primitives": prims}],
        "materials": mats,
    }

    if not rigged:
        gltf["scenes"] = [{"nodes": [0]}]
        gltf["nodes"] = [{"name": "Player", "mesh": 0}]
    else:
        world = [to_y_up(b[2]) for b in SKELETON]
        nodes = [{"name": "Player", "mesh": 0, "skin": 0}]
        for i, (name, parent, _) in enumerate(SKELETON):
            pw = world[BONE_INDEX[parent]] if parent else (0.0, 0.0, 0.0)
            nodes.append({"name": name, "translation": [round(world[i][k] - pw[k], 6) for k in range(3)]})
        for i, (name, parent, _) in enumerate(SKELETON):
            if parent:
                nodes[1 + BONE_INDEX[parent]].setdefault("children", []).append(1 + i)
        ibm = b"".join(struct.pack("<16f", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -w[0], -w[1], -w[2], 1) for w in world)
        a_ibm = add_acc(ibm, 5126, len(SKELETON), "MAT4")
        gltf["nodes"] = nodes
        gltf["scenes"] = [{"nodes": [0, 1]}]
        gltf["skins"] = [{"name": "PlayerSkin", "joints": [1 + i for i in range(len(SKELETON))],
                          "skeleton": 1, "inverseBindMatrices": a_ibm}]

        anims = []
        for aname, duration, fps, fn, extras in ANIMATIONS:
            times, rots, hips = sample_animation(fn, duration, fps)
            n = len(times)
            a_time = add_acc(struct.pack("<%df" % n, *times), 5126, n, "SCALAR", mn=[times[0]], mx=[times[-1]])
            a_scale = add_acc(struct.pack("<3f", 1, 1, 1) * n, 5126, n, "VEC3")
            samplers, channels = [], []

            def chan(node, path, acc):
                samplers.append({"input": a_time, "output": acc, "interpolation": "LINEAR"})
                channels.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})

            for i, (bname, parent, _) in enumerate(SKELETON):
                node = 1 + i
                rest = nodes[node]["translation"]
                if bname == "Hips":
                    tr = b"".join(struct.pack("<3f", h[0], h[1], h[2]) for h in hips)
                else:
                    tr = struct.pack("<3f", *rest) * n
                chan(node, "translation", add_acc(tr, 5126, n, "VEC3"))
                chan(node, "rotation", add_acc(b"".join(struct.pack("<4f", *q) for q in rots[bname]), 5126, n, "VEC4"))
                chan(node, "scale", a_scale)
            anims.append({"name": aname, "samplers": samplers, "channels": channels, "extras": extras})
        gltf["animations"] = anims

    blob.extend(b"\x00" * (-len(blob) % 4))
    gltf["accessors"] = accessors
    gltf["bufferViews"] = views
    gltf["buffers"] = [{"byteLength": len(blob)}]
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(js), b"JSON") + js)
        f.write(struct.pack("<I4s", len(blob), b"BIN\x00") + bytes(blob))
    print("Wrote %s (%d bytes, %s)" % (path, total, "rigged, 3 animations" if rigged else "static"))


def export_with_blender(mesh, path):
    bpy.ops.wm.read_factory_settings(use_empty=True)

    bl_mats = []
    for name in MAT_ORDER:
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        r, g, b = MATERIALS[name]
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = 1.0
        bsdf.inputs["Metallic"].default_value = 0.0
        bl_mats.append(mat)

    verts, faces, face_mat, face_shade = [], [], [], []
    for mat_name, poly, _bone in mesh.polys:
        base = len(verts)
        verts += poly
        faces.append(tuple(range(base, base + len(poly))))
        face_mat.append(MAT_ORDER.index(mat_name))
        face_shade.append(shade(face_normal(poly)))

    me = bpy.data.meshes.new("Player")
    me.from_pydata(verts, [], faces)
    me.update()
    for mat in bl_mats:
        me.materials.append(mat)
    for poly, mi, sh in zip(me.polygons, face_mat, face_shade):
        poly.material_index = mi
        poly.use_smooth = False
    if BAKE_SHADING:
        attr = me.color_attributes.new("Col", 'BYTE_COLOR', 'CORNER')
        for poly, sh in zip(me.polygons, face_shade):
            for li in poly.loop_indices:
                attr.data[li].color = (sh, sh, sh, 1.0)

    obj = bpy.data.objects.new("Player", me)
    bpy.context.scene.collection.objects.link(obj)
    obj.location = (0, 0, 0)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    props = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
    kw = dict(filepath=path, export_format='GLB', export_yup=True, export_apply=True,
              export_materials='EXPORT', export_cameras=False, export_lights=False)
    if "export_animations" in props:
        kw["export_animations"] = False
    if "export_vertex_color" in props:
        kw["export_vertex_color"] = 'ACTIVE'
    elif "export_colors" in props:
        kw["export_colors"] = True
    kw = {k: v for k, v in kw.items() if k in props}
    bpy.ops.export_scene.gltf(**kw)
    print("Wrote %s (Blender %s)" % (path, bpy.app.version_string))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    animated = "--animated" in argv
    out = "assets/models/player/player_animated.glb" if animated else "assets/models/player/player.glb"
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    mesh = build_player()
    report(mesh)
    if animated:
        write_glb(mesh, out, rigged=True)
    elif HAVE_BPY:
        export_with_blender(mesh, out)
    else:
        write_glb(mesh, out, rigged=False)


if __name__ == "__main__":
    main()