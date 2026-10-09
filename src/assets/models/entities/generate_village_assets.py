#!/usr/bin/env python3
"""Village asset kit - stile "fiaba" per bambini.

Produce, come GLB separati e riutilizzabili da codice:

  village/<casa>.glb              case (solo guscio + decorazioni proprie), palette di default
  village/variants/<casa>_<pal>.glb   stesse case in 6 palette di colore
  village/parts/*.glb             parti attaccabili: camini, fumo, tettoie a righe,
                                  insegna, campanile
  village/props/*.glb             oggetti: alberi, funghi, pozzo, recinti, botti, ...
  village/manifest.json           dimensioni, porta, "slot" di attacco e palette
  villagers/<ruolo>.glb           abitanti rigged (testa grande, guance, occhi lucidi)

Convenzioni (come generate_player.py): authoring Z-up, fronte = -Y; nei GLB (Y-up)
il fronte guarda +Z e l'origine e' a terra al centro dell'asset.
I materiali hanno nomi "di ruolo" (wall, roof, trim, shutter, ...): in gioco si puo'
ricolorare a runtime senza rigenerare.

Uso:  python3 generate_village_assets.py [--out DIR] [--only houses|parts|props|villagers]
"""

import json
import math
import os
import sys

import generate_player as gp

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
MODEL_ROOT = os.path.join(ROOT, "assets", "models")

# ---------------------------------------------------------------------------
# Abitanti
# ---------------------------------------------------------------------------
VILLAGERS = {
    "blacksmith": {
        "skin": (0.84, 0.58, 0.42), "hair": (0.30, 0.18, 0.10), "eye": (0.20, 0.50, 0.55),
        "tunic": (0.85, 0.42, 0.20), "pants": (0.30, 0.26, 0.28), "leather": (0.45, 0.28, 0.14),
        "sole": (0.20, 0.14, 0.10), "armor": (0.62, 0.64, 0.68), "gold": (0.95, 0.70, 0.22),
    },
    "mayor": {
        "skin": (0.95, 0.76, 0.62), "hair": (0.70, 0.40, 0.20), "eye": (0.25, 0.45, 0.80),
        "tunic": (0.30, 0.42, 0.85), "pants": (0.30, 0.30, 0.55), "leather": (0.50, 0.30, 0.18),
        "sole": (0.20, 0.14, 0.10), "armor": (0.75, 0.30, 0.40), "gold": (1.00, 0.80, 0.25),
    },
    "cleric": {
        "skin": (0.88, 0.67, 0.50), "hair": (0.55, 0.40, 0.28), "eye": (0.30, 0.60, 0.50),
        "tunic": (0.96, 0.93, 0.85), "pants": (0.70, 0.62, 0.85), "leather": (0.55, 0.40, 0.28),
        "sole": (0.20, 0.14, 0.10), "armor": (0.85, 0.78, 0.95), "gold": (1.00, 0.82, 0.30),
    },
    "farmer": {
        "skin": (0.86, 0.62, 0.43), "hair": (0.45, 0.30, 0.15), "eye": (0.35, 0.55, 0.30),
        "tunic": (0.40, 0.72, 0.32), "pants": (0.38, 0.45, 0.80), "leather": (0.55, 0.36, 0.18),
        "sole": (0.20, 0.14, 0.10), "armor": (0.80, 0.62, 0.30), "gold": (0.97, 0.80, 0.35),
    },
    "innkeeper": {
        "skin": (0.95, 0.72, 0.55), "hair": (0.75, 0.30, 0.15), "eye": (0.45, 0.35, 0.18),
        "tunic": (0.90, 0.30, 0.30), "pants": (0.45, 0.30, 0.22), "leather": (0.96, 0.92, 0.82),
        "sole": (0.20, 0.14, 0.10), "armor": (0.60, 0.40, 0.25), "gold": (0.95, 0.72, 0.25),
    },
}
VILLAGER_EXTRA = {
    "leather_dark": (0.30, 0.20, 0.14), "wood": (0.55, 0.36, 0.18),
    "iron": (0.55, 0.58, 0.62), "white": (1.0, 1.0, 1.0), "blush": (0.98, 0.52, 0.50),
}
HEAD_SCALE = 1.55         # testa "chibi": e' la leva principale contro l'effetto Playmobil
HEAD_PIVOT_Z = 1.54
EYE_SCALE = 1.3


def add_villager_accessories(mesh, role):
    if role == "blacksmith":
        mesh.bone = "Chest"
        gp.frustum(mesh, "leather", 0.0, -0.139, 0.79, 1.25, 0.31, 0.035, 0.34, 0.045)
        mesh.bone = "Hand.R"
        gp.prism(mesh, "leather_dark", -0.235, 0.0, 0.78, 1.07, 0.035, 0.035, n=6)
        gp.frustum(mesh, "armor", -0.235, 0.0, 1.02, 1.17, 0.045, 0.16, 0.045, 0.14)
        mesh.bone = "Head"      # fascia rossa in testa
        gp.prism(mesh, "tunic", 0.0, 0.0, 1.715, 1.745, 0.088, 0.096, n=10)
    elif role == "mayor":
        mesh.bone = "Chest"
        gp.frustum(mesh, "armor", 0.0, 0.09, 0.91, 1.42, 0.43, 0.08, 0.31, 0.07)
        mesh.bone = "Head"
        gp.prism(mesh, "gold", 0.0, 0.0, 1.76, 1.82, 0.083, 0.083, n=10)
        for x in (-0.07, 0.0, 0.07):
            gp.frustum(mesh, "gold", x, 0.0, 1.80, 1.86, 0.025, 0.025, 0.012, 0.012)
    elif role == "cleric":
        mesh.bone = "Hips"
        gp.frustum(mesh, "tunic", 0.0, 0.0, 0.53, 1.02, 0.47, 0.30, 0.34, 0.22)
        mesh.bone = "Chest"
        gp.prism(mesh, "gold", 0.0, -0.15, 1.02, 1.23, 0.018, 0.018, n=6)
        gp.frustum(mesh, "gold", 0.0, -0.15, 1.13, 1.17, 0.12, 0.025)
        mesh.bone = "Head"
        gp.frustum(mesh, "leather", 0.0, 0.0, 1.69, 1.81, 0.17, 0.18, 0.11, 0.13)
    elif role == "farmer":
        mesh.bone = "Head"
        gp.frustum(mesh, "gold", 0.0, 0.0, 1.70, 1.74, 0.28, 0.27, 0.28, 0.27)
        gp.frustum(mesh, "hair", 0.0, 0.0, 1.73, 1.84, 0.15, 0.15, 0.11, 0.11)
        mesh.bone = "Hand.L"
        gp.prism(mesh, "wood", 0.235, 0.0, 0.78, 1.21, 0.018, 0.018, n=6)
        gp.frustum(mesh, "iron", 0.235, 0.0, 1.17, 1.25, 0.09, 0.035, 0.07, 0.03)
    else:
        mesh.bone = "Chest"
        gp.frustum(mesh, "leather", 0.0, -0.15, 0.79, 1.20, 0.32, 0.035, 0.34, 0.04)
        mesh.bone = "Head"
        gp.prism(mesh, "hair", 0.0, 0.0, 1.72, 1.81, 0.13, 0.13, n=8)
        mesh.bone = "Hand.R"
        gp.prism(mesh, "gold", -0.235, -0.03, 0.80, 0.98, 0.055, 0.055, n=8)


def kidify_head(mesh):
    """Testa grande, occhi piu' grandi con riflesso, guance rosa. Skinning rigido: basta
    scalare i poligoni dell'osso Head attorno al suo perno (accessori inclusi)."""
    S = HEAD_SCALE

    def sc(p):
        return (p[0] * S, p[1] * S, HEAD_PIVOT_Z + (p[2] - HEAD_PIVOT_Z) * S)

    mesh.polys = [(m, [sc(p) for p in poly], b) if b == "Head" else (m, poly, b)
                  for m, poly, b in mesh.polys]
    # centro e fronte di ciascun occhio (dopo lo scale della testa)
    info = {}
    for sign in (1, -1):
        pts = [p for m, poly, b in mesh.polys if b == "Head" and m == "eye"
               and (sum(q[0] for q in poly) / len(poly) > 0) == (sign > 0) for p in poly]
        info[sign] = ((min(p[0] for p in pts) + max(p[0] for p in pts)) / 2,
                      min(p[1] for p in pts),
                      (min(p[2] for p in pts) + max(p[2] for p in pts)) / 2)
    out = []
    for m, poly, b in mesh.polys:
        if b == "Head" and m == "eye":
            cx, ymin, cz = info[1 if sum(q[0] for q in poly) / len(poly) > 0 else -1]
            poly = [(cx + (p[0] - cx) * EYE_SCALE, p[1] - 0.004, cz + (p[2] - cz) * EYE_SCALE)
                    for p in poly]
        out.append((m, poly, b))
    mesh.polys = out
    for sign, (cx, ymin, cz) in info.items():
        mesh.bone = "Head"
        gp.frustum(mesh, "white", cx + 0.008, ymin - 0.005, cz + 0.004, cz + 0.018, 0.014, 0.006)
        add_poly_prism(mesh, "blush", octagon(cx * 1.15, cz - 0.058, 0.019), ymin, ymin + 0.012, bone="Head")


def generate_villagers(out_root):
    old_palette, old_order, old_anims = gp.MATERIALS, gp.MAT_ORDER, gp.ANIMATIONS
    out_dir = os.path.join(out_root, "villagers")
    os.makedirs(out_dir, exist_ok=True)
    try:
        for role, palette in VILLAGERS.items():
            gp.MATERIALS = dict(palette)
            gp.MATERIALS.update(VILLAGER_EXTRA)
            gp.MAT_ORDER = list(gp.MATERIALS)

            def work_pose(t, role=role):
                swing = math.sin(2.0 * math.pi * t / 1.25)
                pose = {"Lknee": 3, "Rknee": 3, "chest_pitch": -2.0 + 2.0 * swing,
                        "spine_pitch": 1.0 + swing, "head_pitch": -1.0 - swing}
                if role == "blacksmith":
                    pose.update({"Rswing": 48.0 + 17.0 * swing, "Rabd": 8.0,
                                 "Relbow": 72.0 + 12.0 * swing, "Rhand": -10.0})
                elif role == "farmer":
                    pose.update({"Lswing": 34.0 + 16.0 * swing, "Labd": 24.0,
                                 "Lelbow": 44.0 + 10.0 * swing, "Lhand": 8.0})
                return pose

            mesh = gp.build_player()
            add_villager_accessories(mesh, role)
            kidify_head(mesh)
            gp.ANIMATIONS = old_anims + [("Work", 1.25, 30, work_pose, {"loop": True})]
            gp.write_glb(mesh, os.path.join(out_dir, role + ".glb"), rigged=True)
    finally:
        gp.MATERIALS, gp.MAT_ORDER, gp.ANIMATIONS = old_palette, old_order, old_anims


# ---------------------------------------------------------------------------
# Geometria di base
# ---------------------------------------------------------------------------
# Se in gioco tetti/frontoni risultassero "al rovescio", invertire questo flag.
OUTWARD_CCW = True


def _newell(pts):
    nx = ny = nz = 0.0
    for i in range(len(pts)):
        x0, y0, z0 = pts[i]
        x1, y1, z1 = pts[(i + 1) % len(pts)]
        nx += (y0 - y1) * (z0 + z1)
        ny += (z0 - z1) * (x0 + x1)
        nz += (x0 - x1) * (y0 + y1)
    return nx, ny, nz


def add_solid(mesh, material, faces, bone="Hips"):
    """Solido convesso: ogni faccia viene orientata verso l'esterno."""
    mesh.bone = bone
    allp = [p for f in faces for p in f]
    c = [sum(p[i] for p in allp) / len(allp) for i in range(3)]
    for face in faces:
        pts = list(face)
        fc = [sum(p[i] for p in pts) / len(pts) for i in range(3)]
        nx, ny, nz = _newell(pts)
        d = nx * (fc[0] - c[0]) + ny * (fc[1] - c[1]) + nz * (fc[2] - c[2])
        if (d < 0) == OUTWARD_CCW:
            pts.reverse()
        mesh.add(material, pts, [(0, i, i + 1) for i in range(1, len(pts) - 1)])


def add_poly_prism(mesh, material, poly, y0, y1, bone="Hips"):
    """Poligono convesso nel piano XZ estruso lungo Y."""
    extrude(mesh, material, poly, "y", y0, y1, bone)


def extrude(mesh, material, poly, axis, c0, c1, bone="Hips"):
    """Poligono convesso (u, z) estruso: axis 'y' -> u=x, c=y ; axis 'x' -> u=y, c=x."""
    if axis == "y":
        mp = lambda u, z, c: (u, c, z)
    else:
        mp = lambda u, z, c: (c, u, z)
    a = [mp(u, z, c0) for u, z in poly]
    b = [mp(u, z, c1) for u, z in poly]
    faces = [a, b]
    for i in range(len(poly)):
        j = (i + 1) % len(poly)
        faces.append([a[i], a[j], b[j], b[i]])
    add_solid(mesh, material, faces, bone)


def add_ellipsoid(mesh, material, cx, cy, cz, rx, ry, rz, seg=8, rings=4):
    top, bot = (cx, cy, cz + rz), (cx, cy, cz - rz)
    rs = []
    for i in range(1, rings):
        phi = math.pi * i / rings
        r, z = math.sin(phi), cz + rz * math.cos(phi)
        rs.append([(cx + rx * r * math.cos(2 * math.pi * (k + 0.5) / seg),
                    cy + ry * r * math.sin(2 * math.pi * (k + 0.5) / seg), z) for k in range(seg)])
    faces = []
    for k in range(seg):
        j = (k + 1) % seg
        faces.append([top, rs[0][k], rs[0][j]])
        faces.append([bot, rs[-1][j], rs[-1][k]])
        for i in range(len(rs) - 1):
            faces.append([rs[i][k], rs[i][j], rs[i + 1][j], rs[i + 1][k]])
    add_solid(mesh, material, faces)


def octagon(cx, cz, r):
    return [(cx + r * math.cos(math.pi / 8 + k * math.pi / 4),
             cz + r * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]


def circle(r, zc, n=10):
    return [(r * math.cos(2 * math.pi * k / n), zc + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def arch(z0, zc, r, n=6):
    """Rettangolo da z0 a zc + semicerchio di raggio r: finestra a sesto tondo (u relativo)."""
    pts = [(-r, z0), (r, z0)]
    pts += [(r * math.cos(math.pi * k / n), zc + r * math.sin(math.pi * k / n)) for k in range(n + 1)]
    return pts


# ---------------------------------------------------------------------------
# Materiali e palette
# ---------------------------------------------------------------------------
BASE_MATS = {
    "stone": (0.66, 0.62, 0.56), "wall": (0.97, 0.90, 0.72), "trim": (0.52, 0.32, 0.18),
    "roof": (0.82, 0.32, 0.22), "roof_dark": (0.64, 0.22, 0.17), "shutter": (0.28, 0.60, 0.62),
    "door": (0.62, 0.36, 0.17), "glass": (1.0, 0.87, 0.45), "iron": (0.32, 0.34, 0.38),
    "gold": (0.96, 0.75, 0.22), "wood": (0.66, 0.43, 0.22), "wood_dark": (0.45, 0.28, 0.14),
    "hay": (0.94, 0.79, 0.30), "leaf": (0.42, 0.74, 0.30), "leaf_dark": (0.22, 0.56, 0.32),
    "bark": (0.50, 0.34, 0.21), "flower_a": (0.96, 0.40, 0.52), "flower_b": (1.0, 0.83, 0.26),
    "flower_c": (0.64, 0.46, 0.92), "mush_red": (0.90, 0.22, 0.22), "white": (0.98, 0.97, 0.94),
    "brick": (0.80, 0.40, 0.28), "awn_a": (0.92, 0.30, 0.30), "awn_b": (0.99, 0.95, 0.86),
    "water": (0.38, 0.74, 0.92), "pumpkin": (0.98, 0.56, 0.14),
    "glow": (1.0, 0.55, 0.18),
}
PALETTES = {
    "cream": dict(wall=(0.98, 0.91, 0.72), roof=(0.84, 0.34, 0.24), roof_dark=(0.66, 0.24, 0.18),
                  shutter=(0.28, 0.62, 0.64), awn_a=(0.92, 0.30, 0.30)),
    "mint": dict(wall=(0.74, 0.92, 0.80), roof=(0.58, 0.38, 0.70), roof_dark=(0.44, 0.27, 0.56),
                 shutter=(0.96, 0.62, 0.70), awn_a=(0.60, 0.40, 0.75)),
    "sky": dict(wall=(0.72, 0.86, 0.97), roof=(0.95, 0.52, 0.38), roof_dark=(0.80, 0.38, 0.28),
                shutter=(0.98, 0.84, 0.35), awn_a=(0.30, 0.55, 0.92)),
    "peach": dict(wall=(1.0, 0.82, 0.68), roof=(0.25, 0.62, 0.62), roof_dark=(0.17, 0.47, 0.50),
                  shutter=(0.90, 0.35, 0.40), awn_a=(0.25, 0.62, 0.62)),
    "lilac": dict(wall=(0.86, 0.80, 0.96), roof=(0.96, 0.74, 0.25), roof_dark=(0.80, 0.56, 0.16),
                  shutter=(0.40, 0.70, 0.45), awn_a=(0.96, 0.74, 0.25)),
    "brick": dict(wall=(0.92, 0.58, 0.40), roof=(0.38, 0.48, 0.68), roof_dark=(0.27, 0.35, 0.54),
                  shutter=(0.97, 0.90, 0.70), awn_a=(0.38, 0.48, 0.68)),
}


def palette(name=None, **over):
    p = dict(BASE_MATS)
    if name:
        p.update(PALETTES[name])
    p.update(over)
    return p


class use_palette:
    def __init__(self, pal):
        self.pal = pal

    def __enter__(self):
        self.old = (gp.MATERIALS, gp.MAT_ORDER)
        gp.MATERIALS, gp.MAT_ORDER = dict(self.pal), list(self.pal)

    def __exit__(self, *a):
        gp.MATERIALS, gp.MAT_ORDER = self.old


# ---------------------------------------------------------------------------
# Builder
# ---------------------------------------------------------------------------
class Builder:
    def __init__(self, name, category):
        self.name, self.category = name, category
        self.mesh = gp.Mesh()
        self.mesh.bone = "Hips"
        self.meta = {}

    def box(self, mat, cx, cy, z0, z1, w, d):
        self.mesh.bone = "Hips"
        gp.frustum(self.mesh, mat, cx, cy, z0, z1, w, d)

    def cyl(self, mat, cx, cy, z0, z1, r, n=10, sy=1.0):
        self.mesh.bone = "Hips"
        gp.prism(self.mesh, mat, cx, cy, z0, z1, r, r, n=n, sy=sy)

    def cone(self, mat, cx, cy, z0, z1, r0, r1=0.03, n=8):
        self.mesh.bone = "Hips"
        gp.prism(self.mesh, mat, cx, cy, z0, z1, r0, r1, n=n)

    def ell(self, mat, cx, cy, cz, rx, ry=None, rz=None, seg=8, rings=4):
        add_ellipsoid(self.mesh, mat, cx, cy, cz, rx, rx if ry is None else ry,
                      rx if rz is None else rz, seg, rings)

    def poly(self, mat, pts, axis, c0, c1):
        extrude(self.mesh, mat, pts, axis, c0, c1)

    def awning(self, y0, width, depth, z_wall, z_front, stripe=0.5, mat=("awn_a", "awn_b")):
        """Tettoia a strisce inclinata: attaccata a y0 (muro), sporge verso -Y."""
        n = max(2, int(round(width / stripe)))
        sw = width / n
        for i in range(n):
            x0 = -width / 2 + i * sw
            m = mat[i % 2]
            self.poly(m, [(y0, z_wall), (y0 - depth, z_front), (y0 - depth, z_front + 0.10),
                          (y0, z_wall + 0.10)], "x", x0, x0 + sw)
            self.box(m, x0 + sw / 2, y0 - depth + 0.025, z_front - 0.20, z_front + 0.04, sw, 0.05)


# ---------------------------------------------------------------------------
# Casa
# ---------------------------------------------------------------------------
class House(Builder):
    WALL_T, FOUND_H, FLOOR_Z = 0.30, 0.30, 0.42
    DOOR_W, DOOR_H = 1.20, 2.10
    OVERHANG, ROOF_T, STONE = 0.42, 0.20, 1.10
    # profilo del tetto "da fiaba": ripido al colmo, piu' piatto in gronda, punta rialzata
    PROFILE = [(0.0, 0.0), (0.35, 0.45), (0.70, 0.80), (0.92, 0.945), (1.0, 0.915)]

    def __init__(self, name, hw, hd, wh, rh, rows=((1.55, 2.35),), gable_window=False,
                 glass="glass"):
        super().__init__(name, "house")
        self.hw, self.hd, self.wh, self.rh = hw, hd, wh, rh
        self.rows, self.gable_window, self.glass = rows, gable_window, glass
        self.dt = self.FLOOR_Z + self.DOOR_H
        X = hw + self.OVERHANG
        self.X = X
        self.ridge = wh + 0.08 + self.drop(hw)

    def drop(self, x):
        xf = min(x / self.X, 1.0)
        for (a, da), (b, db) in zip(self.PROFILE, self.PROFILE[1:]):
            if xf <= b:
                return (da + (db - da) * (xf - a) / (b - a)) * self.rh * 1.05
        return self.PROFILE[-1][1] * self.rh * 1.05

    def wpos(self, face, u, out, z):
        if face == "front":
            return (u, -self.hd - out, z)
        if face == "back":
            return (u, self.hd + out, z)
        if face == "right":
            return (self.hw + out, u, z)
        return (-self.hw - out, u, z)

    def wbox(self, mat, face, u, out0, out1, z0, z1, span):
        mid, thick = (out0 + out1) / 2.0, out1 - out0
        x, y, _ = self.wpos(face, u, mid, 0)
        if face in ("front", "back"):
            self.box(mat, x, y, z0, z1, span, thick)
        else:
            self.box(mat, x, y, z0, z1, thick, span)

    def wpoly(self, mat, face, u, out0, out1, poly):
        pts = [(u + du, z) for du, z in poly]
        if face in ("front", "back"):
            a, b = self.wpos(face, 0, out0, 0)[1], self.wpos(face, 0, out1, 0)[1]
            self.poly(mat, pts, "y", a, b)
        else:
            a, b = self.wpos(face, 0, out0, 0)[0], self.wpos(face, 0, out1, 0)[0]
            self.poly(mat, pts, "x", a, b)

    def wall(self, cx, cy, z0, z1, w, d):
        s = self.STONE
        if z1 <= s:
            self.box("stone", cx, cy, z0, z1, w, d)
        elif z0 >= s:
            self.box("wall", cx, cy, z0, z1, w, d)
        else:
            self.box("stone", cx, cy, z0, s, w, d)
            self.box("wall", cx, cy, s, z1, w, d)

    def window(self, face, u, z0, z1, w=0.7):
        r, t = w / 2, 0.10
        zc = z1 - r
        self.wpoly("trim", face, u, 0.0, 0.07, [(du, z) for du, z in arch(z0 - t, zc, r + t)])
        self.wpoly(self.glass, face, u, 0.0, 0.11, arch(z0, zc, r))
        self.wbox("trim", face, u, 0.0, 0.14, z0, zc + r, 0.04)
        zm = (z0 + zc) / 2
        self.wbox("trim", face, u, 0.0, 0.14, zm - 0.02, zm + 0.02, w)
        for s in (-1, 1):
            self.wbox("shutter", face, u + s * (r + t + 0.13), 0.0, 0.08, z0 - t, zc + r * 0.6, 0.26)
        self.wbox("wood_dark", face, u, 0.0, 0.30, z0 - t - 0.24, z0 - t - 0.04, w + 0.35)
        cols = ["flower_a", "flower_b", "flower_c", "flower_a", "flower_b"]
        for k in range(5):
            uu = u - 0.30 + k * 0.15
            x, y, z = self.wpos(face, uu, 0.15, z0 - t + 0.01)
            self.ell(cols[k], x, y, z, 0.07, seg=6, rings=3)
            x, y, z = self.wpos(face, uu + 0.075, 0.15, z0 - t - 0.06)
            self.ell("leaf", x, y, z, 0.06, seg=6, rings=3)

    # ---- guscio ----
    def shell(self):
        hw, hd, wh = self.hw, self.hd, self.wh
        T, F, FL, dw, dt = self.WALL_T, self.FOUND_H, self.FLOOR_Z, self.DOOR_W, self.dt

        self.box("stone", 0.0, 0.0, 0.0, F, 2 * hw + 0.4, 2 * hd + 0.4)
        self.box("wood", 0.0, 0.0, F, FL, 2 * (hw - T) + 0.02, 2 * (hd - T) + 0.02)
        self.cyl("stone", 0.0, -hd - 0.45, 0.0, 0.16, 0.95, n=10, sy=0.55)   # gradino tondo

        self.wall(-hw + T / 2, 0.0, F, wh, T, 2 * hd)
        self.wall(hw - T / 2, 0.0, F, wh, T, 2 * hd)
        self.wall(0.0, hd - T / 2, F, wh, 2 * hw, T)
        side_w, side_c = hw - dw / 2, (hw + dw / 2) / 2
        self.wall(-side_c, -hd + T / 2, F, wh, side_w, T)
        self.wall(side_c, -hd + T / 2, F, wh, side_w, T)
        self.wall(0.0, -hd + T / 2, dt, wh, dw, T)

        # porta con oblo' e pomello
        self.box("door", 0.0, -hd + 0.15, FL, dt, dw, 0.10)
        for s in (-1, 1):
            self.wbox("trim", "front", s * (dw / 2 + 0.07), 0.0, 0.10, FL, dt + 0.14, 0.14)
        self.wbox("trim", "front", 0.0, 0.0, 0.10, dt, dt + 0.14, dw + 0.28)
        self.wpoly("trim", "front", 0.0, -0.11, -0.05, circle(0.21, FL + 1.50))
        self.wpoly("glass", "front", 0.0, -0.11, -0.03, circle(0.15, FL + 1.50))
        x, y, z = self.wpos("front", 0.38, -0.12, FL + 0.95)
        self.ell("gold", x, y, z, 0.05, seg=6, rings=3)
        # tettuccio sopra la porta
        ex = dw / 2 + 0.35
        self.poly("roof", [(-hd, dt + 0.28), (-hd - 0.70, dt + 0.08), (-hd - 0.70, dt + 0.18),
                           (-hd, dt + 0.38)], "x", -ex, ex)

        # travi: pali d'angolo, cordoli, fasce
        for sx in (-1, 1):
            for sy in (-1, 1):
                self.box("trim", sx * hw, sy * hd, F, wh + 0.06, 0.20, 0.20)
        for sy in (-1, 1):
            self.box("trim", 0.0, sy * hd, wh - 0.14, wh + 0.06, 2 * hw + 0.20, 0.20)
        for sx in (-1, 1):
            self.box("trim", sx * hw, 0.0, wh - 0.14, wh + 0.06, 0.20, 2 * hd + 0.20)
        belts = [(self.STONE - 0.05, self.STONE + 0.07)]
        if len(self.rows) > 1:
            zb = (self.rows[0][1] + self.rows[1][0]) / 2
            belts.append((zb - 0.07, zb + 0.07))
        lo, hi = dw / 2 + 0.14, hw
        for b0, b1 in belts:
            for s in (-1, 1):
                self.wbox("trim", "front", s * (lo + hi) / 2, 0.0, 0.08, b0, b1, hi - lo)
            self.wbox("trim", "back", 0.0, 0.0, 0.08, b0, b1, 2 * hw)
            self.wbox("trim", "left", 0.0, 0.0, 0.08, b0, b1, 2 * hd)
            self.wbox("trim", "right", 0.0, 0.0, 0.08, b0, b1, 2 * hd)

        self.roof()

        # finestre
        for i, (z0, z1) in enumerate(self.rows):
            fu = (-(hw - 1.0), hw - 1.0)
            for u in fu:
                self.window("front", u, z0, z1)
            for face in ("right", "left"):
                self.window(face, -0.9, z0, z1)
            self.window("back", 0.0, z0, z1)

    def roof(self):
        hw, hd, wh = self.hw, self.hd, self.wh
        o, th, X, ridge = self.OVERHANG, self.ROOF_T, self.X, self.ridge
        xs = [a * X for a, _ in self.PROFILE]
        zs = [ridge - self.drop(x) for x in xs]
        y = hd + o
        n = len(xs) - 1
        for sg in (-1, 1):
            for i in range(n):
                poly = [(sg * xs[i], zs[i]), (sg * xs[i + 1], zs[i + 1]),
                        (sg * xs[i + 1], zs[i + 1] + th), (sg * xs[i], zs[i] + th)]
                self.poly("roof_dark" if i == n - 1 else "roof", poly, "y", -y, y)
                big = [(sg * xs[i], zs[i] - 0.03), (sg * xs[i + 1], zs[i + 1] - 0.03),
                       (sg * xs[i + 1], zs[i + 1] + th + 0.03), (sg * xs[i], zs[i] + th + 0.03)]
                self.poly("trim", big, "y", y, y + 0.06)      # frontalini
                self.poly("trim", big, "y", -y - 0.06, -y)
        self.box("trim", 0.0, 0.0, ridge + th - 0.04, ridge + th + 0.10, 0.34, 2 * y + 0.12)

        # frontoni: strisce verticali che seguono il sotto-tetto
        bx = [x for x in xs if x < hw] + [hw]
        for sg in (-1, 1):
            for i in range(len(bx) - 1):
                xa, xb = bx[i], bx[i + 1]
                poly = [(sg * xa, wh), (sg * xb, wh),
                        (sg * xb, ridge - self.drop(xb)), (sg * xa, ridge - self.drop(xa))]
                add_poly_prism(self.mesh, "wall", poly, -hd, -hd + 0.24)
                add_poly_prism(self.mesh, "wall", poly, hd - 0.24, hd)
        if self.gable_window:
            zc = wh + 0.45 * (ridge - wh)
            for face in ("front", "back"):
                self.wpoly("trim", face, 0.0, 0.0, 0.10, circle(0.50, zc))
                self.wpoly(self.glass, face, 0.0, 0.0, 0.16, circle(0.40, zc))
                self.wbox("trim", face, 0.0, 0.0, 0.18, zc - 0.40, zc + 0.40, 0.05)
                self.wbox("trim", face, 0.0, 0.0, 0.18, zc - 0.025, zc + 0.025, 0.80)


# ---------------------------------------------------------------------------
# Case: varianti con elementi propri
# ---------------------------------------------------------------------------
def build_forge(h):
    wh = h.wh
    h.wbox("wood", "front", 0.0, 0.0, 0.07, wh + 0.30, wh + 1.05, 0.09)
    h.wbox("iron", "front", 0.0, 0.0, 0.10, wh + 0.88, wh + 1.14, 0.50)
    h.wbox("gold", "front", 0.0, 0.0, 0.12, wh + 0.95, wh + 1.07, 0.16)


def build_hall(h):
    wh, hd = h.wh, h.hd
    shield = [(-0.30, wh + 1.00), (0.30, wh + 1.00), (0.30, wh + 0.62), (0.0, wh + 0.36), (-0.30, wh + 0.62)]
    add_poly_prism(h.mesh, "gold", shield, -hd - 0.07, -hd + 0.03)
    top = h.ridge + h.ROOF_T + 0.10          # bandierina sul colmo
    h.cyl("trim", 0.0, 0.0, top, top + 1.5, 0.04, n=6)
    h.poly("awn_a", [(0.0, top + 1.45), (-0.8, top + 1.20), (0.0, top + 0.95)], "x", -0.02, 0.02)
    h.ell("gold", 0.0, 0.0, top + 1.52, 0.07, seg=6, rings=3)


def build_chapel(h):
    wh, hd = h.wh, h.hd
    zc = wh + 0.45 * (h.ridge - wh)
    h.wpoly("trim", "front", 0.0, 0.0, 0.12, octagon(0.0, zc, 0.62))
    h.wpoly("glass", "front", 0.0, 0.0, 0.18, octagon(0.0, zc, 0.50))
    h.wbox("trim", "front", 0.0, 0.0, 0.20, zc - 0.03, zc + 0.03, 1.0)
    h.wbox("trim", "front", 0.0, 0.0, 0.20, zc - 0.50, zc + 0.50, 0.06)
    for sx in (-1, 1):                         # contrafforti bassi e tondi
        for y in (-1.3, 1.3):
            h.cyl("stone", sx * (h.hw + 0.22), y, 0.0, 2.4, 0.30, n=8)
            h.cone("stone", sx * (h.hw + 0.22), y, 2.4, 2.9, 0.34, 0.05, n=8)


def build_inn(h):
    pass


def build_farmhouse(h):
    wh = h.wh
    h.wbox("door", "front", 0.0, 0.0, 0.08, wh + 0.35, wh + 0.95, 0.60)     # portello del fienile
    h.wbox("trim", "front", 0.0, 0.0, 0.12, wh + 0.95, wh + 1.03, 0.76)
    for s in (-1, 1):
        h.wbox("trim", "front", s * 0.34, 0.0, 0.12, wh + 0.35, wh + 0.95, 0.06)


def build_plain(h):
    pass


# nome: (hw, hd, wh, rh, palette, righe finestre, finestra frontone, vetro, builder, slot)
HOUSES = {
    "forge": dict(hw=3.0, hd=2.5, wh=3.0, rh=1.5, pal="brick", build=build_forge, glass="glow",
                  chimney=(+1, 0.9), awning=True),
    "hall": dict(hw=2.5, hd=2.5, wh=3.2, rh=1.9, pal="lilac", build=build_hall, awning=True),
    "chapel": dict(hw=3.0, hd=2.5, wh=3.3, rh=2.3, pal="sky", build=build_chapel, rows=((1.5, 2.6),),
                   tower=True),
    "inn": dict(hw=2.5, hd=2.5, wh=3.0, rh=1.6, pal="peach", build=build_inn, gable=True,
                chimney=(-1, 0.9), awning=True, sign=True),
    "farmhouse": dict(hw=3.0, hd=2.5, wh=3.0, rh=1.5, pal="mint", build=build_farmhouse,
                      chimney=(-1, 0.9)),
    "cottage": dict(hw=2.2, hd=2.0, wh=2.8, rh=1.9, pal="cream", build=build_plain, chimney=(+1, 0.7)),
    "townhouse": dict(hw=2.4, hd=2.2, wh=5.0, rh=1.7, pal="lilac", build=build_plain, gable=True,
                      rows=((1.55, 2.35), (3.9, 4.7)), chimney=(+1, 0.7)),
}
CHIMNEY_HEIGHTS = (4.5, 5.5, 6.5, 8.0)
AWNING_WIDTHS = (4.4, 4.8, 5.0, 6.0)


def gl(p):
    return [round(p[0], 3), round(p[2], 3), round(-p[1], 3)]


def build_house(name):
    c = HOUSES[name]
    h = House(name, c["hw"], c["hd"], c["wh"], c["rh"], rows=c.get("rows", ((1.55, 2.35),)),
              gable_window=c.get("gable", False), glass=c.get("glass", "glass"))
    h.shell()
    c["build"](h)
    hw, hd, wh = h.hw, h.hd, h.wh
    slots = {}
    if c.get("chimney"):
        side, cy = c["chimney"]
        w = 0.9 if hw >= 2.5 else 0.7
        top_need = wh + h.rh + 0.5
        cls = next((x for x in CHIMNEY_HEIGHTS if x >= top_need), CHIMNEY_HEIGHTS[-1])
        cx = side * (hw + w / 2 - 0.15)
        slots["chimney"] = {"asset": "parts/chimney_%s" % str(cls).replace(".", "_"),
                            "pos": gl((cx, cy, 0)), "width": w}
        slots["smoke"] = {"asset": "parts/smoke", "pos": gl((cx, cy, cls + 0.25))}
    if c.get("awning"):
        slots["awning"] = {"asset": "parts/awning_w%s" % str(2 * hw).replace(".", "_"),
                           "pos": gl((0, -hd, 0)), "note": "sostituisce il tettuccio della porta"}
    if c.get("sign"):
        slots["hanging_sign"] = {"asset": "parts/hanging_sign", "pos": gl((hw - 0.9, -hd, 2.7))}
    if c.get("tower"):
        slots["tower"] = {"asset": "parts/bell_tower", "pos": gl((0, hd + 0.3, 0))}
    h.meta = {
        "palette_default": c["pal"], "half_width": hw, "half_depth": hd,
        "door": {"pos": gl((0, -hd, h.FLOOR_Z)), "width": h.DOOR_W, "height": h.DOOR_H,
                 "step_pos": gl((0, -hd - 0.45, 0))},
        "front_dir": [0, 0, 1], "slots": slots,
    }
    return h


# ---------------------------------------------------------------------------
# Parti attaccabili
# ---------------------------------------------------------------------------
def build_chimney(height):
    b = Builder("chimney_%s" % str(height).replace(".", "_"), "part")
    w = 0.9
    b.box("stone", 0, 0, 0.0, 0.35, w + 0.25, w + 0.25)
    b.box("brick", 0, 0, 0.30, height, w, w)
    z = 0.9
    while z < height - 0.4:
        b.box("stone", 0, 0, z, z + 0.08, w + 0.06, w + 0.06)
        z += 0.8
    b.box("stone", 0, 0, height, height + 0.16, w + 0.20, w + 0.20)
    for dx in (-0.2, 0.2):
        b.cyl("roof_dark", dx, 0, height + 0.16, height + 0.42, 0.13, n=8)
    b.meta = {"top": height + 0.42}
    return b


def build_smoke():
    b = Builder("smoke", "part")
    for i, (dx, z, r) in enumerate(((0.0, 0.1, 0.16), (0.12, 0.55, 0.22), (0.30, 1.05, 0.30), (0.60, 1.65, 0.38))):
        b.ell("white", dx, 0, z, r, r * 0.9, r * 0.8, seg=8, rings=4)
    return b


def build_awning(width):
    b = Builder("awning_w%s" % str(width).replace(".", "_"), "part")
    depth = 1.5
    b.awning(0.0, width, depth, 2.95, 2.62)
    for s in (-1, 1):
        b.cyl("wood", s * (width / 2 - 0.25), -depth + 0.15, 0.0, 2.66, 0.08, n=6)
    return b


def build_hanging_sign():
    b = Builder("hanging_sign", "part")
    b.box("iron", 0, -0.55, 0.0, 0.08, 0.06, 1.1)           # braccio (origine sul muro, z=0 = attacco)
    b.box("iron", 0, -0.55, -0.12, -0.04, 0.06, 1.1)
    for s in (-0.3, 0.3):
        b.box("iron", s, -0.55, -0.60, -0.04, 0.03, 0.03)
    b.box("wood", 0, -0.55, -0.80, -0.30, 0.80, 0.08)
    b.cyl("gold", 0, -0.62, -0.68, -0.44, 0.13, n=8)          # boccale
    b.ell("white", 0, -0.62, -0.38, 0.14, 0.14, 0.07, seg=8, rings=3)
    return b


def build_bell_tower():
    b = Builder("bell_tower", "part")
    b.cyl("stone", 0, 0, 0.0, 0.30, 1.25, n=8)
    b.cyl("wall", 0, 0, 0.30, 6.00, 1.0, n=8)
    b.cyl("trim", 0, 0, 3.0, 3.12, 1.05, n=8)
    b.cyl("stone", 0, 0, 6.00, 6.25, 1.20, n=8)
    for a in range(4):
        ang = a * math.pi / 2
        cx, cy = 0.95 * math.cos(ang), 0.95 * math.sin(ang)
        b.cyl("door", cx, cy, 4.2, 5.4, 0.28, n=6)
    b.ell("gold", 0, 0, 4.7, 0.30, 0.30, 0.35)                   # campana
    b.cone("roof", 0, 0, 6.25, 8.70, 1.45, 0.05, n=8)
    b.ell("gold", 0, 0, 8.80, 0.14)
    b.box("gold", 0, 0, 8.85, 9.65, 0.08, 0.08)
    b.box("gold", 0, 0, 9.25, 9.37, 0.45, 0.08)
    return b


# ---------------------------------------------------------------------------
# Props
# ---------------------------------------------------------------------------
def p_barrel():
    b = Builder("barrel", "prop")
    b.cyl("wood", 0, 0, 0.0, 0.85, 0.30)
    b.ell("wood", 0, 0, 0.42, 0.34, 0.34, 0.45, seg=10, rings=4)
    for z in (0.14, 0.62):
        b.cyl("iron", 0, 0, z, z + 0.08, 0.33)
    b.cyl("wood_dark", 0, 0, 0.85, 0.88, 0.27)
    return b


def p_crate_stack():
    b = Builder("crate_stack", "prop")
    for (x, y, z, s) in ((0, 0, 0.0, 0.7), (0.75, 0.1, 0.0, 0.6), (0.3, 0.05, 0.7, 0.55)):
        b.box("wood", x, y, z, z + s, s, s)
        b.box("wood_dark", x, y, z + s * 0.45, z + s * 0.55, s + 0.02, s + 0.02)
    return b


def p_bench():
    b = Builder("bench", "prop")
    b.box("wood", 0, 0, 0.45, 0.53, 1.5, 0.42)
    b.box("wood", 0, 0.18, 0.75, 0.83, 1.5, 0.06)
    for s in (-1, 1):
        b.box("wood_dark", s * 0.62, 0, 0.0, 0.45, 0.10, 0.36)
        b.box("wood_dark", s * 0.62, 0.18, 0.45, 0.85, 0.08, 0.06)
    return b


def p_anvil():
    b = Builder("anvil", "prop")
    b.cyl("wood", 0, 0, 0.0, 0.50, 0.32, n=8)
    b.box("iron", 0, 0, 0.50, 0.62, 0.36, 0.26)
    b.box("iron", 0, 0, 0.62, 0.72, 0.20, 0.20)
    b.box("iron", 0, 0, 0.72, 0.84, 0.85, 0.28)
    b.cone("iron", 0.55, 0, 0.72, 0.84, 0.13, 0.05, n=6)
    return b


def p_haystack():
    b = Builder("haystack", "prop")
    b.cyl("hay", 0, 0, 0.0, 0.55, 0.95, n=10)
    b.cone("hay", 0, 0, 0.55, 1.75, 0.92, 0.05, n=10)
    b.cyl("trim", 0, 0, 0.45, 0.52, 0.97, n=10)
    return b


def p_well():
    b = Builder("well", "prop")
    b.cyl("stone", 0, 0, 0.0, 0.85, 0.85, n=10)
    b.cyl("water", 0, 0, 0.80, 0.86, 0.62, n=10)
    b.cyl("stone", 0, 0, 0.85, 0.95, 0.90, n=10)
    for s in (-1, 1):
        b.box("wood", s * 0.75, 0, 0.0, 2.1, 0.14, 0.14)
    b.box("wood_dark", 0, 0, 1.5, 1.6, 1.6, 0.10)
    b.cyl("iron", 0, 0, 1.2, 1.4, 0.05, n=6)
    for sg in (-1, 1):
        b.poly("roof", [(0.0, 2.9), (sg * 1.25, 2.0), (sg * 1.25, 2.2), (0.0, 3.1)], "y", -0.95, 0.95)
    b.box("trim", 0, 0, 3.0, 3.15, 0.2, 1.9)
    b.box("wood", 0.0, 0, 0.95, 1.20, 0.30, 0.30)
    return b


def p_fence():
    b = Builder("fence_2m", "prop")
    for x in (-1.0, 0.0, 1.0):
        b.box("wood", x, 0, 0.0, 0.95, 0.14, 0.14)
        b.cone("wood", x, 0, 0.95, 1.07, 0.10, 0.02, n=4)
    for z in (0.35, 0.7):
        b.box("wood_dark", 0, 0, z, z + 0.10, 2.0, 0.06)
    return b


def p_lantern_post():
    b = Builder("lantern_post", "prop")
    b.cyl("iron", 0, 0, 0.0, 2.2, 0.06, n=6)
    b.cyl("iron", 0, 0, 0.0, 0.2, 0.14, n=6)
    b.box("iron", 0.18, 0, 2.05, 2.10, 0.4, 0.05)
    b.ell("glass", 0.36, 0, 1.95, 0.14, 0.14, 0.18)
    b.cone("iron", 0.36, 0, 2.08, 2.30, 0.18, 0.03, n=6)
    return b


def p_tree_round():
    b = Builder("tree_round", "prop")
    b.cone("bark", 0, 0, 0.0, 1.5, 0.28, 0.18, n=6)
    b.ell("leaf", 0, 0, 2.5, 1.3, 1.3, 1.1)
    b.ell("leaf_dark", 0.8, 0.3, 2.0, 0.85, 0.85, 0.7)
    b.ell("leaf", -0.7, -0.4, 3.0, 0.8, 0.8, 0.7)
    for (x, y, z) in ((1.0, -0.6, 2.8), (-0.9, 0.5, 2.3), (0.2, -1.2, 2.2), (-0.3, 0.9, 3.35)):
        b.ell("mush_red", x, y, z, 0.12, seg=6, rings=3)
    return b


def p_tree_pine():
    b = Builder("tree_pine", "prop")
    b.cone("bark", 0, 0, 0.0, 1.0, 0.24, 0.16, n=6)
    for z0, z1, r in ((0.8, 2.3, 1.3), (1.8, 3.2, 1.0), (2.7, 4.2, 0.72)):
        b.cone("leaf_dark", 0, 0, z0, z1, r, 0.04, n=8)
    b.ell("gold", 0, 0, 4.25, 0.12, seg=6, rings=3)
    return b


def p_bush():
    b = Builder("bush", "prop")
    b.ell("leaf", 0, 0, 0.45, 0.65, 0.6, 0.5)
    b.ell("leaf_dark", 0.5, 0.1, 0.35, 0.45, 0.45, 0.38)
    b.ell("leaf", -0.45, -0.1, 0.35, 0.42, 0.42, 0.36)
    for (x, y, z, m) in ((0.1, -0.5, 0.65, "flower_a"), (-0.3, -0.35, 0.78, "flower_b"), (0.45, -0.3, 0.6, "flower_c")):
        b.ell(m, x, y, z, 0.08, seg=6, rings=3)
    return b


def p_mushroom():
    b = Builder("mushroom_red", "prop")
    b.cyl("white", 0, 0, 0.0, 0.40, 0.12, n=8)
    b.ell("mush_red", 0, 0, 0.42, 0.45, 0.45, 0.30, seg=10, rings=4)
    for (x, y) in ((0.18, 0.0), (-0.12, 0.17), (-0.1, -0.2), (0.0, 0.0)):
        r = math.hypot(x, y)
        z = 0.42 + 0.30 * math.sqrt(max(0.0, 1 - (r / 0.45) ** 2))
        b.ell("white", x, y, z - 0.01, 0.055, 0.055, 0.03, seg=6, rings=3)
    return b


def p_flowers():
    b = Builder("flower_patch", "prop")
    cols = ["flower_a", "flower_b", "flower_c", "flower_a", "flower_b", "flower_c", "flower_a"]
    pos = [(0, 0), (0.35, 0.2), (-0.3, 0.25), (0.15, -0.35), (-0.35, -0.2), (0.5, -0.2), (-0.1, 0.5)]
    b.cyl("leaf", 0, 0, 0.0, 0.08, 0.7, n=8)
    for (x, y), m in zip(pos, cols):
        b.cyl("leaf", x, y, 0.05, 0.28, 0.015, n=4)
        b.ell(m, x, y, 0.32, 0.075, 0.075, 0.06, seg=6, rings=3)
        b.ell("flower_b" if m != "flower_b" else "white", x, y, 0.35, 0.03, seg=4, rings=2)
    return b


def p_pumpkins():
    b = Builder("pumpkin_patch", "prop")
    for (x, y, r) in ((0, 0, 0.32), (0.55, 0.15, 0.24), (-0.4, 0.3, 0.2)):
        b.ell("pumpkin", x, y, r * 0.8, r, r, r * 0.8, seg=8, rings=4)
        b.cyl("leaf_dark", x, y, r * 1.5, r * 1.5 + 0.1, 0.04, n=5)
    return b


def p_signpost():
    b = Builder("signpost", "prop")
    b.cyl("wood_dark", 0, 0, 0.0, 2.1, 0.07, n=6)
    for z, d in ((1.75, 1), (1.40, -1)):
        pts = [(-0.5 * d if d > 0 else -0.5, z), (0.4, z), (0.65, z + 0.12), (0.4, z + 0.24), (-0.5, z + 0.24)]
        if d < 0:
            pts = [(-x, zz) for x, zz in pts]
        b.poly("wood", pts, "y", -0.06, 0.06)
    b.ell("gold", 0, 0, 2.15, 0.09, seg=6, rings=3)
    return b


def p_market_stall():
    b = Builder("market_stall", "prop")
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box("wood", sx * 1.1, sy * 0.6, 0.0, 2.3 if sy < 0 else 2.0, 0.10, 0.10)
    b.box("wood", 0, -0.5, 0.0, 0.9, 2.2, 0.5)
    b.box("wood_dark", 0, -0.5, 0.9, 0.97, 2.4, 0.65)
    b.awning(0.6, 2.5, 1.35, 2.0, 2.38 - 0.34 * 0 - 0.18)
    for i, (x, m) in enumerate(((-0.7, "mush_red"), (-0.3, "flower_b"), (0.1, "leaf"), (0.5, "pumpkin"), (0.85, "mush_red"))):
        b.ell(m, x, -0.5, 1.07, 0.12, seg=6, rings=3)
    b.cyl("wood", 0.9, -0.3, 0.0, 0.3, 0.2, n=6)
    return b


def p_rock():
    b = Builder("rock", "prop")
    b.ell("stone", 0, 0, 0.3, 0.7, 0.55, 0.38, seg=7, rings=3)
    b.ell("stone", 0.6, 0.2, 0.18, 0.36, 0.3, 0.22, seg=6, rings=3)
    b.ell("leaf", -0.2, 0.0, 0.62, 0.3, 0.25, 0.08, seg=6, rings=3)
    return b


def p_stepping_stone():
    b = Builder("stepping_stone", "prop")
    b.cyl("stone", 0, 0, 0.0, 0.08, 0.5, n=8)
    b.cyl("white", 0, 0, 0.08, 0.09, 0.4, n=8)
    return b


def p_pond():
    b = Builder("pond", "prop")
    b.cyl("water", 0, 0, 0.0, 0.05, 1.9, n=14, sy=0.75)
    for k in range(10):
        a = 2 * math.pi * k / 10
        b.ell("stone", 1.95 * math.cos(a), 1.5 * math.sin(a), 0.12, 0.28, 0.24, 0.2, seg=6, rings=3)
    b.cyl("leaf", 0.5, 0.2, 0.05, 0.08, 0.28, n=8)
    b.ell("flower_a", 0.5, 0.2, 0.14, 0.09, seg=6, rings=3)
    b.cyl("leaf", -0.7, -0.3, 0.05, 0.08, 0.22, n=8)
    return b


PROPS = [p_barrel, p_crate_stack, p_bench, p_anvil, p_haystack, p_well, p_fence, p_lantern_post,
         p_tree_round, p_tree_pine, p_bush, p_mushroom, p_flowers, p_pumpkins, p_signpost,
         p_market_stall, p_rock, p_stepping_stone, p_pond]


# ---------------------------------------------------------------------------
# Scrittura + manifest
# ---------------------------------------------------------------------------
def info(b):
    lo, hi = gp.bounds(b.mesh)
    return {"category": b.category, "min": [round(lo[0], 3), round(lo[2], 3), round(-hi[1], 3)],
            "max": [round(hi[0], 3), round(hi[2], 3), round(-lo[1], 3)],
            "size": [round(hi[0] - lo[0], 3), round(hi[2] - lo[2], 3), round(hi[1] - lo[1], 3)],
            "radius": round(max(hi[0] - lo[0], hi[1] - lo[1]) / 2, 3), **b.meta}


def write(b, out_root, rel, pal):
    path = os.path.join(out_root, "village", rel + ".glb")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with use_palette(pal):
        gp.write_glb(b.mesh, path, rigged=False)


def generate_village(out_root, only):
    manifest_path = os.path.join(out_root, "village", "manifest.json")
    man = {"coordinate_system": "glTF Y-up, fronte +Z, origine a terra al centro",
           "palettes": {k: {m: list(v) for m, v in PALETTES[k].items()} for k in PALETTES},
           "base_materials": {k: list(v) for k, v in BASE_MATS.items()},
           "houses": {}, "parts": {}, "props": {}}
    if os.path.exists(manifest_path):
        try:
            old = json.load(open(manifest_path))
            for k in ("houses", "parts", "props"):
                man[k] = old.get(k, {})
        except ValueError:
            pass
    if only in (None, "houses"):
        for name, c in HOUSES.items():
            h = build_house(name)
            write(h, out_root, name, palette(c["pal"]))
            variants = []
            for pn in PALETTES:
                write(h, out_root, "variants/%s_%s" % (name, pn), palette(pn))
                variants.append("village/variants/%s_%s.glb" % (name, pn))
            d = info(h)
            d["file"] = "village/%s.glb" % name
            d["variants"] = variants
            man["houses"][name] = d
    if only in (None, "parts"):
        parts = [build_chimney(x) for x in CHIMNEY_HEIGHTS] + [build_smoke(), build_hanging_sign(), build_bell_tower()]
        parts += [build_awning(w) for w in AWNING_WIDTHS]
        for b in parts:
            write(b, out_root, "parts/" + b.name, palette("cream"))
            d = info(b)
            d["file"] = "village/parts/%s.glb" % b.name
            if b.name.startswith("awning"):
                d["variants"] = []
                for pn in PALETTES:
                    write(b, out_root, "parts/variants/%s_%s" % (b.name, pn), palette(pn))
                    d["variants"].append("village/parts/variants/%s_%s.glb" % (b.name, pn))
            man["parts"][b.name] = d
    if only in (None, "props"):
        for fn in PROPS:
            b = fn()
            write(b, out_root, "props/" + b.name, palette())
            d = info(b)
            d["file"] = "village/props/%s.glb" % b.name
            man["props"][b.name] = d
    with open(manifest_path, "w") as f:
        json.dump(man, f, indent=1)
    print("Wrote", manifest_path)


def main():
    argv = sys.argv[1:]
    out_root = MODEL_ROOT
    only = None
    if "--out" in argv:
        out_root = argv[argv.index("--out") + 1]
    if "--only" in argv:
        only = argv[argv.index("--only") + 1]
    if only in (None, "houses", "parts", "props"):
        generate_village(out_root, only)
    if only in (None, "villagers"):
        generate_villagers(out_root)


if __name__ == "__main__":
    main()
