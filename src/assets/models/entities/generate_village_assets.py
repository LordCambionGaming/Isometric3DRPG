#!/usr/bin/env python3
"""Generate the village's static house GLBs and rigged resident variants."""

import math
import os
import sys

import generate_player as gp

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
MODEL_ROOT = os.path.join(ROOT, "assets", "models")

BASE_PALETTE = dict(gp.MATERIALS)

VILLAGERS = {
    "blacksmith": {
        "skin": (0.66, 0.43, 0.29), "hair": (0.16, 0.12, 0.09), "eye": (0.26, 0.43, 0.47),
        "tunic": (0.24, 0.15, 0.12), "pants": (0.16, 0.14, 0.13), "leather": (0.19, 0.13, 0.10),
        "sole": (0.08, 0.07, 0.06), "armor": (0.34, 0.35, 0.36), "gold": (0.61, 0.36, 0.12),
    },
    "mayor": {
        "skin": (0.83, 0.65, 0.51), "hair": (0.53, 0.31, 0.17), "eye": (0.21, 0.36, 0.55),
        "tunic": (0.12, 0.20, 0.34), "pants": (0.15, 0.17, 0.22), "leather": (0.26, 0.17, 0.12),
        "sole": (0.08, 0.07, 0.06), "armor": (0.38, 0.31, 0.22), "gold": (0.89, 0.66, 0.20),
    },
    "cleric": {
        "skin": (0.75, 0.55, 0.40), "hair": (0.38, 0.27, 0.18), "eye": (0.31, 0.43, 0.38),
        "tunic": (0.68, 0.62, 0.47), "pants": (0.38, 0.34, 0.27), "leather": (0.30, 0.23, 0.17),
        "sole": (0.08, 0.07, 0.06), "armor": (0.73, 0.68, 0.56), "gold": (0.74, 0.57, 0.21),
    },
    "farmer": {
        "skin": (0.74, 0.52, 0.36), "hair": (0.28, 0.20, 0.12), "eye": (0.30, 0.37, 0.24),
        "tunic": (0.25, 0.36, 0.20), "pants": (0.30, 0.22, 0.15), "leather": (0.38, 0.25, 0.13),
        "sole": (0.08, 0.07, 0.06), "armor": (0.43, 0.34, 0.20), "gold": (0.74, 0.53, 0.20),
    },
    "innkeeper": {
        "skin": (0.82, 0.62, 0.45), "hair": (0.36, 0.16, 0.12), "eye": (0.36, 0.32, 0.20),
        "tunic": (0.40, 0.14, 0.12), "pants": (0.22, 0.16, 0.12), "leather": (0.28, 0.17, 0.10),
        "sole": (0.08, 0.07, 0.06), "armor": (0.34, 0.24, 0.16), "gold": (0.68, 0.46, 0.17),
    },
}

HOUSE_MATERIALS = {
    "stone": (0.43, 0.44, 0.42),
    "plaster": (0.52, 0.52, 0.49),
    "timber": (0.20, 0.15, 0.11),
    "roof": (0.20, 0.24, 0.25),
    "roof_light": (0.34, 0.19, 0.14),
    "glass": (0.12, 0.28, 0.34),
    "door": (0.16, 0.09, 0.05),
    "iron": (0.22, 0.23, 0.23),
    "gold": (0.69, 0.49, 0.17),
    "wood": (0.40, 0.25, 0.13),
    "hay": (0.62, 0.50, 0.22),
}


def add_box(mesh, material, cx, cy, z0, z1, width, depth, bone="Hips"):
    mesh.bone = bone
    gp.frustum(mesh, material, cx, cy, z0, z1, width, depth)


def add_villager_accessories(mesh, role):
    if role == "blacksmith":
        mesh.bone = "Chest"
        gp.frustum(mesh, "leather", 0.0, -0.139, 0.79, 1.25, 0.31, 0.035, 0.34, 0.045)
        mesh.bone = "Hand.R"
        gp.prism(mesh, "leather_dark", -0.235, 0.0, 0.78, 1.07, 0.035, 0.035, n=6)
        gp.frustum(mesh, "armor", -0.235, 0.0, 1.02, 1.17, 0.045, 0.16, 0.045, 0.14)
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


def generate_villagers():
    old_palette = gp.MATERIALS
    old_order = gp.MAT_ORDER
    old_animations = gp.ANIMATIONS
    out_dir = os.path.join(MODEL_ROOT, "villagers")
    os.makedirs(out_dir, exist_ok=True)
    try:
        for role, palette in VILLAGERS.items():
            gp.MATERIALS = dict(palette)
            gp.MATERIALS.update({
                "leather_dark": (0.11, 0.08, 0.06),
                "wood": (0.31, 0.19, 0.10),
                "iron": (0.38, 0.40, 0.41),
            })
            gp.MAT_ORDER = list(gp.MATERIALS)
            def work_pose(t):
                swing = math.sin(2.0 * math.pi * t / 1.25)
                pose = {
                    "Lknee": 3, "Rknee": 3,
                    "chest_pitch": -2.0 + 2.0 * swing,
                    "spine_pitch": 1.0 + swing,
                    "head_pitch": -1.0 - swing,
                }
                if role == "blacksmith":
                    pose.update({"Rswing": 48.0 + 17.0 * swing, "Rabd": 8.0,
                                 "Relbow": 72.0 + 12.0 * swing, "Rhand": -10.0})
                elif role == "farmer":
                    pose.update({"Lswing": 34.0 + 16.0 * swing, "Labd": 24.0,
                                 "Lelbow": 44.0 + 10.0 * swing, "Lhand": 8.0})
                return pose

            mesh = gp.build_player()
            add_villager_accessories(mesh, role)
            gp.ANIMATIONS = old_animations + [
                ("Work", 1.25, 30, work_pose, {"loop": True})
            ]
            gp.write_glb(mesh, os.path.join(out_dir, role + ".glb"), rigged=True)
    finally:
        gp.MATERIALS = old_palette
        gp.MAT_ORDER = old_order
        gp.ANIMATIONS = old_animations


# ---------------------------------------------------------------------------
# Case del villaggio
# ---------------------------------------------------------------------------
# Convenzione: X = larghezza, Y = profondita' (fronte = -Y), Z = altezza.
# Se in gioco tetti/frontoni risultassero "al rovescio" (invisibili da fuori),
# basta invertire questo flag.
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


def add_solid(mesh, material, faces):
    """Solido convesso: ogni faccia viene orientata verso l'esterno."""
    mesh.bone = "Hips"
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


def add_poly_prism(mesh, material, poly, y0, y1):
    """Poligono convesso nel piano XZ estruso lungo Y."""
    a = [(x, y0, z) for x, z in poly]
    b = [(x, y1, z) for x, z in poly]
    faces = [a, b]
    for i in range(len(poly)):
        j = (i + 1) % len(poly)
        faces.append([a[i], a[j], b[j], b[i]])
    add_solid(mesh, material, faces)


def add_pyramid(mesh, material, cx, cy, z0, z1, half):
    b = [(cx - half, cy - half, z0), (cx + half, cy - half, z0),
         (cx + half, cy + half, z0), (cx - half, cy + half, z0)]
    apex = (cx, cy, z1)
    faces = [b] + [[b[i], b[(i + 1) % 4], apex] for i in range(4)]
    add_solid(mesh, material, faces)


def octagon(cx, cz, r):
    return [(cx + r * math.cos(math.pi / 8 + k * math.pi / 4),
             cz + r * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]


class House:
    WALL_T = 0.30
    FOUND_H = 0.30
    FLOOR_Z = 0.42
    DOOR_W = 1.20
    DOOR_H = 2.10
    OVERHANG = 0.42
    ROOF_T = 0.18
    STONE_COURSE = 1.10

    def __init__(self, hw, hd, wall_h, roof_h, roof_material):
        self.hw, self.hd, self.wh, self.rh = hw, hd, wall_h, roof_h
        self.roof_material = roof_material
        self.mesh = gp.Mesh()
        self.mesh.bone = "Hips"
        self.door_top = self.FLOOR_Z + self.DOOR_H

    # ---- primitive -------------------------------------------------------
    def box(self, mat, cx, cy, z0, z1, w, d):
        add_box(self.mesh, mat, cx, cy, z0, z1, w, d)

    def cyl(self, mat, cx, cy, z0, z1, r, n=10):
        self.mesh.bone = "Hips"
        gp.prism(self.mesh, mat, cx, cy, z0, z1, r, r, n=n)

    def wbox(self, mat, face, u, out0, out1, z0, z1, span):
        """Box attaccato a una parete. out0/out1 = distanza dal piano esterno
        del muro (positivo = verso fuori), u = posizione lungo il muro."""
        mid, thick = (out0 + out1) / 2.0, out1 - out0
        if face == "front":
            self.box(mat, u, -self.hd - mid, z0, z1, span, thick)
        elif face == "back":
            self.box(mat, u, self.hd + mid, z0, z1, span, thick)
        elif face == "right":
            self.box(mat, self.hw + mid, u, z0, z1, thick, span)
        else:
            self.box(mat, -self.hw - mid, u, z0, z1, thick, span)

    def wall(self, cx, cy, z0, z1, w, d):
        """Muro: zoccolo in pietra + intonaco sopra."""
        s = self.STONE_COURSE
        if z1 <= s:
            self.box("stone", cx, cy, z0, z1, w, d)
        elif z0 >= s:
            self.box("plaster", cx, cy, z0, z1, w, d)
        else:
            self.box("stone", cx, cy, z0, s, w, d)
            self.box("plaster", cx, cy, s, z1, w, d)

    # ---- elementi ---------------------------------------------------------
    def window(self, face, u, z0, z1, w=0.7):
        t = 0.10
        self.wbox("glass", face, u, 0.0, 0.05, z0, z1, w)
        for s in (-1, 1):
            self.wbox("timber", face, u + s * (w + t) / 2, 0.0, 0.12, z0 - t, z1 + t, t)
        self.wbox("timber", face, u, 0.0, 0.12, z1, z1 + t, w)
        self.wbox("timber", face, u, 0.0, 0.12, z0 - t, z0, w)
        zm = (z0 + z1) / 2
        self.wbox("timber", face, u, 0.0, 0.08, zm - 0.025, zm + 0.025, w)
        self.wbox("timber", face, u, 0.0, 0.08, z0, z1, 0.05)
        self.wbox("timber", face, u, 0.0, 0.20, z0 - t - 0.06, z0 - t, w + 2 * t + 0.10)

    def chimney(self, side, cy, w=0.9, extra=0.5):
        cx = side * (self.hw + w / 2 - 0.15)
        top = self.wh + self.rh + extra
        self.box("stone", cx, cy, 0.0, 0.35, w + 0.25, w + 0.25)
        self.box("stone", cx, cy, 0.30, top, w, w)
        self.box("stone", cx, cy, top, top + 0.16, w + 0.20, w + 0.20)
        self.box("iron", cx, cy, top + 0.16, top + 0.20, w - 0.30, w - 0.30)

    def canopy(self, mat, depth, post_u, z0=2.72, z1=2.90, post_mat="timber"):
        """Tettoia davanti alla facciata, appoggiata al muro e a due pali."""
        self.wbox(mat, "front", 0.0, 0.0, depth, z0, z1, 2 * self.hw)
        for s in (-1, 1):
            self.wbox(post_mat, "front", s * post_u, depth - 0.28, depth - 0.08,
                      0.0, z0 + 0.04, 0.20)

    def barrel(self, cx, cy, r=0.30, h=0.85):
        self.cyl("wood", cx, cy, 0.0, h, r)
        for z in (0.16, h - 0.24):
            self.cyl("iron", cx, cy, z, z + 0.08, r + 0.02)

    def bench(self, cx, cy, length=1.4):
        self.box("wood", cx, cy, 0.45, 0.53, length, 0.4)
        for s in (-1, 1):
            self.box("wood", cx + s * (length / 2 - 0.12), cy, 0.0, 0.45, 0.08, 0.34)

    # ---- guscio -----------------------------------------------------------
    def shell(self, windows=True):
        hw, hd, wh, rh = self.hw, self.hd, self.wh, self.rh
        T, F, FL = self.WALL_T, self.FOUND_H, self.FLOOR_Z
        dw = self.DOOR_W
        dt = self.door_top

        # fondazione, pavimento (solo interno, niente facce coplanari) e gradino
        self.box("stone", 0.0, 0.0, 0.0, F, 2 * hw + 0.4, 2 * hd + 0.4)
        self.box("wood", 0.0, 0.0, F, FL, 2 * (hw - T) + 0.02, 2 * (hd - T) + 0.02)
        self.box("stone", 0.0, -hd - 0.55, 0.0, 0.16, dw + 0.7, 0.70)

        # muri
        self.wall(-hw + T / 2, 0.0, F, wh, T, 2 * hd)
        self.wall(hw - T / 2, 0.0, F, wh, T, 2 * hd)
        self.wall(0.0, hd - T / 2, F, wh, 2 * hw, T)
        side_w = hw - dw / 2
        side_c = (hw + dw / 2) / 2
        self.wall(-side_c, -hd + T / 2, F, wh, side_w, T)
        self.wall(side_c, -hd + T / 2, F, wh, side_w, T)
        self.wall(0.0, -hd + T / 2, dt, wh, dw, T)  # architrave: chiude sopra la porta

        # porta (riempie esattamente il vano) + cornice + maniglia
        self.box("door", 0.0, -hd + 0.15, FL, dt, dw, 0.10)
        for s in (-1, 1):
            self.wbox("timber", "front", s * (dw / 2 + 0.07), 0.0, 0.10, FL, dt + 0.14, 0.14)
        self.wbox("timber", "front", 0.0, 0.0, 0.10, dt, dt + 0.14, dw + 0.28)
        self.wbox("iron", "front", 0.38, -0.14, -0.05, 1.30, 1.40, 0.06)

        # travi: pali d'angolo, cordoli di gronda, fascia sopra lo zoccolo
        for sx in (-1, 1):
            for sy in (-1, 1):
                self.box("timber", sx * hw, sy * hd, F, wh + 0.06, 0.20, 0.20)
        for sy in (-1, 1):
            self.box("timber", 0.0, sy * hd, wh - 0.14, wh + 0.06, 2 * hw + 0.20, 0.20)
        for sx in (-1, 1):
            self.box("timber", sx * hw, 0.0, wh - 0.14, wh + 0.06, 0.20, 2 * hd + 0.20)
        belt0, belt1 = self.STONE_COURSE - 0.05, self.STONE_COURSE + 0.07
        lo, hi = dw / 2 + 0.14, hw
        for s in (-1, 1):
            self.wbox("timber", "front", s * (lo + hi) / 2, 0.0, 0.08, belt0, belt1, hi - lo)
        self.wbox("timber", "back", 0.0, 0.0, 0.08, belt0, belt1, 2 * hw)
        self.wbox("timber", "left", 0.0, 0.0, 0.08, belt0, belt1, 2 * hd)
        self.wbox("timber", "right", 0.0, 0.0, 0.08, belt0, belt1, 2 * hd)

        # frontoni: sagoma "a casetta" che segue ESATTAMENTE il sotto-tetto
        under_ridge = wh + 0.06 + rh
        gable = [(-hw, wh), (hw, wh), (hw, wh + 0.06), (0.0, under_ridge), (-hw, wh + 0.06)]
        add_poly_prism(self.mesh, "plaster", gable, -hd, -hd + 0.24)
        add_poly_prism(self.mesh, "plaster", gable, hd - 0.24, hd)

        # tetto: due falde con spessore reale + colmo
        s = rh / hw
        o, th = self.OVERHANG, self.ROOF_T
        y = hd + o
        z_eave = wh + 0.06 - s * o
        for sg in (-1, 1):
            poly = [(sg * (hw + o), z_eave), (0.0, under_ridge),
                    (0.0, under_ridge + th), (sg * (hw + o), z_eave + th)]
            add_poly_prism(self.mesh, self.roof_material, poly, -y, y)
        self.box("timber", 0.0, 0.0, under_ridge + th - 0.05, under_ridge + th + 0.08,
                 0.28, 2 * y)

        if windows:
            for sx in (-1, 1):
                self.window("front", sx * hw * 0.58, 1.55, 2.35)
                self.window("right" if sx > 0 else "left", -0.9, 1.55, 2.35)
            self.window("back", 0.0, 1.55, 2.35)


# ---------------------------------------------------------------------------
# Varianti
# ---------------------------------------------------------------------------
def build_forge(h):
    hw, hd, wh = h.hw, h.hd, h.wh
    h.chimney(+1, 0.9)
    h.canopy("wood", 1.5, hw - 0.2)
    # ceppo + incudine, tutto appoggiato a terra sotto la tettoia
    ax, ay = 1.6, -(hd + 0.9)
    h.cyl("wood", ax, ay, 0.0, 0.50, 0.30)
    h.box("iron", ax, ay, 0.50, 0.62, 0.34, 0.24)
    h.box("iron", ax, ay, 0.62, 0.72, 0.20, 0.20)
    h.box("iron", ax, ay, 0.72, 0.82, 0.80, 0.26)
    h.barrel(-2.1, -(hd + 0.8))
    # martello sul frontone
    h.wbox("wood", "front", 0.0, 0.0, 0.07, wh + 0.45, wh + 1.00, 0.08)
    h.wbox("iron", "front", 0.0, 0.0, 0.10, wh + 0.90, wh + 1.05, 0.40)


def build_hall(h):
    hw, hd, wh = h.hw, h.hd, h.wh
    h.canopy("roof_light", 1.6, hw - 0.25, post_mat="stone")
    h.box("stone", hw - 0.25, -(hd + 1.32), 0.0, 0.25, 0.4, 0.4)
    h.box("stone", -(hw - 0.25), -(hd + 1.32), 0.0, 0.25, 0.4, 0.4)
    shield = [(-0.28, wh + 1.00), (0.28, wh + 1.00), (0.28, wh + 0.65),
              (0.0, wh + 0.40), (-0.28, wh + 0.65)]
    add_poly_prism(h.mesh, "gold", shield, -hd - 0.07, -hd + 0.03)


def build_chapel(h):
    hw, hd, wh = h.hw, h.hd, h.wh
    # finestre lancet frontali e laterali
    for sx in (-1, 1):
        h.window("front", sx * hw * 0.58, 1.50, 2.55)
        h.window("right" if sx > 0 else "left", 0.0, 1.50, 2.70)
    # contrafforti (fuori dal muro, a terra)
    for sx in (-1, 1):
        for y in (-1.3, 1.3):
            h.wbox("stone", "right" if sx > 0 else "left", y, 0.0, 0.40, 0.0, 2.80, 0.50)
            h.wbox("stone", "right" if sx > 0 else "left", y, 0.0, 0.50, 2.80, 2.95, 0.60)
    # rosone sul frontone
    zc = wh + 0.95
    add_poly_prism(h.mesh, "timber", octagon(0.0, zc, 0.62), -hd - 0.10, -hd + 0.05)
    add_poly_prism(h.mesh, "glass", octagon(0.0, zc, 0.50), -hd - 0.14, -hd + 0.05)
    h.wbox("timber", "front", 0.0, 0.0, 0.16, zc - 0.025, zc + 0.025, 1.0)
    h.wbox("timber", "front", 0.0, 0.0, 0.16, zc - 0.50, zc + 0.50, 0.05)
    # campanile sul retro, a terra, che attraversa il tetto
    cy = hd + 0.3
    h.box("stone", 0.0, cy, 0.0, 0.30, 2.1, 2.1)
    h.box("stone", 0.0, cy, 0.30, 7.00, 1.8, 1.8)
    h.box("stone", 0.0, cy, 7.00, 7.20, 2.1, 2.1)
    for sx in (-1, 1):
        h.box("door", sx * 0.92, cy, 6.00, 6.90, 0.08, 0.60)
    h.box("door", 0.0, cy - 0.92, 6.00, 6.90, 0.60, 0.08)
    h.box("door", 0.0, cy + 0.92, 6.00, 6.90, 0.60, 0.08)
    add_pyramid(h.mesh, "roof", 0.0, cy, 7.20, 9.30, 1.0)
    # croce sulla punta della guglia (sullo stesso asse della torre)
    h.box("gold", 0.0, cy, 9.20, 9.95, 0.10, 0.10)
    h.box("gold", 0.0, cy, 9.52, 9.66, 0.50, 0.10)


def build_inn(h):
    hw, hd, wh = h.hw, h.hd, h.wh
    h.chimney(-1, 0.9, w=0.8)
    h.canopy("roof_light", 1.4, hw - 0.25)
    # insegna appesa al bordo della tettoia, con due tiranti
    h.wbox("wood", "front", 1.30, 1.30, 1.38, 1.95, 2.55, 0.90)
    for s in (-1, 1):
        h.wbox("iron", "front", 1.30 + s * 0.35, 1.30, 1.38, 2.55, 2.74, 0.04)
    h.barrel(-1.3, -(hd + 0.65))
    h.barrel(-1.9, -(hd + 0.65))
    h.bench(1.75, -(hd + 0.45))


def build_farmhouse(h):
    hw, hd, wh = h.hw, h.hd, h.wh
    h.chimney(-1, 0.9, w=0.8)
    h.bench(-1.75, -(hd + 0.45))
    h.barrel(1.5, -(hd + 0.6), r=0.28, h=0.8)
    h.box("wood", 2.2, -(hd + 0.5), 0.0, 0.6, 0.8, 0.8)
    h.box("wood", 2.2, -(hd + 0.5), 0.6, 1.1, 0.6, 0.6)
    # covone di fieno
    h.cyl("hay", hw + 1.1, 0.8, 0.0, 0.75, 0.75)
    h.cyl("hay", hw + 1.1, 0.8, 0.75, 1.20, 0.50)
    h.cyl("hay", hw + 1.1, 0.8, 1.20, 1.50, 0.22)


HOUSES = {
    # nome: (half_w, half_d, wall_h, roof_h, materiale tetto, builder, finestre)
    "forge": (3.0, 2.5, 3.0, 1.45, "roof", build_forge, True),
    "hall": (2.5, 2.5, 3.2, 1.8, "roof_light", build_hall, True),
    "chapel": (3.0, 2.5, 3.3, 2.1, "roof", build_chapel, False),
    "inn": (2.5, 2.5, 3.0, 1.55, "roof_light", build_inn, True),
    "farmhouse": (3.0, 2.5, 2.8, 1.35, "roof", build_farmhouse, True),
}


def generate_houses():
    old_palette = gp.MATERIALS
    old_order = gp.MAT_ORDER
    gp.MATERIALS = dict(HOUSE_MATERIALS)
    gp.MAT_ORDER = list(HOUSE_MATERIALS.keys())
    out_dir = os.path.join(MODEL_ROOT, "village")
    os.makedirs(out_dir, exist_ok=True)
    try:
        for name, (hw, hd, wh, rh, roof, builder, windows) in HOUSES.items():
            house = House(hw, hd, wh, rh, roof)
            house.shell(windows=windows)
            builder(house)
            gp.write_glb(house.mesh, os.path.join(out_dir, name + ".glb"), rigged=False)
    finally:
        gp.MATERIALS = old_palette
        gp.MAT_ORDER = old_order


def main():
    generate_houses()
    generate_villagers()


if __name__ == "__main__":
    main()
