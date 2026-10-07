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
}


def add_box(mesh, material, cx, cy, z0, z1, width, depth, bone="Hips"):
    mesh.bone = bone
    gp.frustum(mesh, material, cx, cy, z0, z1, width, depth)


def add_triangle(mesh, material, a, b, c):
    mesh.bone = "Hips"
    mesh.add(material, [a, b, c], [(0, 1, 2)])


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


def house_mesh(name, half_width, half_depth, wall_height, roof_height, roof_material):
    mesh = gp.Mesh()
    mesh.bone = "Hips"
    front = -half_depth
    back = half_depth
    door_width = 1.15
    add_box(mesh, "stone", 0.0, 0.0, 0.0, 0.28, half_width * 2.0 + 0.35, half_depth * 2.0 + 0.35)
    add_box(mesh, "wood", 0.0, 0.0, 0.28, 0.42, half_width * 2.0, half_depth * 2.0)

    add_box(mesh, "stone", -half_width + 0.12, 0.0, 0.42, wall_height, 0.24, half_depth * 2.0)
    add_box(mesh, "stone", half_width - 0.12, 0.0, 0.42, wall_height, 0.24, half_depth * 2.0)
    add_box(mesh, "stone", 0.0, back - 0.12, 0.42, wall_height, half_width * 2.0, 0.24)
    side_wall_width = half_width - door_width * 0.5
    side_wall_center = (half_width + door_width * 0.5) * 0.5
    add_box(mesh, "stone", -side_wall_center, front + 0.12, 0.42, wall_height, side_wall_width, 0.24)
    add_box(mesh, "stone", side_wall_center, front + 0.12, 0.42, wall_height, side_wall_width, 0.24)
    add_box(mesh, "stone", 0.0, front + 0.12, wall_height - 0.35, wall_height, door_width, 0.24)

    add_box(mesh, "door", 0.0, front - 0.025, 0.42, 2.15, door_width - 0.12, 0.09)
    add_box(mesh, "iron", 0.34, front - 0.09, 1.35, 1.43, 0.055, 0.055)

    for x in (-half_width + 0.15, half_width - 0.15):
        add_box(mesh, "timber", x, front - 0.035, 0.42, wall_height, 0.17, 0.12)
        add_box(mesh, "timber", x, back + 0.035, 0.42, wall_height, 0.17, 0.12)
    add_box(mesh, "timber", 0.0, front - 0.035, wall_height - 0.10, wall_height + 0.1, half_width * 2.0, 0.13)
    add_box(mesh, "timber", 0.0, back + 0.035, wall_height - 0.10, wall_height + 0.1, half_width * 2.0, 0.13)

    for x in (-half_width * 0.58, half_width * 0.58):
        add_box(mesh, "timber", x, front - 0.05, 1.62, 2.42, 0.9, 0.12)
        add_box(mesh, "glass", x, front - 0.12, 1.72, 2.3, 0.58, 0.05)
        add_box(mesh, "timber", x, front - 0.16, 1.99, 2.04, 0.62, 0.07)
        add_box(mesh, "timber", x, front - 0.16, 1.67, 2.38, 0.06, 0.07)

    eave = wall_height
    ridge = wall_height + roof_height
    overhang = 0.42
    roof_front = front - overhang
    roof_back = back + overhang
    add_triangle(mesh, roof_material, (-half_width - overhang, roof_front, eave),
                 (0.0, roof_front, ridge), (0.0, roof_back, ridge))
    add_triangle(mesh, roof_material, (-half_width - overhang, roof_front, eave),
                 (0.0, roof_back, ridge), (-half_width - overhang, roof_back, eave))
    add_triangle(mesh, roof_material, (0.0, roof_front, ridge),
                 (half_width + overhang, roof_front, eave), (half_width + overhang, roof_back, eave))
    add_triangle(mesh, roof_material, (0.0, roof_front, ridge),
                 (half_width + overhang, roof_back, eave), (0.0, roof_back, ridge))
    add_triangle(mesh, "stone", (-half_width, front, eave), (0.0, front, ridge), (half_width, front, eave))
    add_triangle(mesh, "stone", (-half_width, back, eave), (half_width, back, eave), (0.0, back, ridge))
    add_box(mesh, "timber", 0.0, 0.0, ridge - 0.04, ridge + 0.08, 0.13, (half_depth + overhang) * 2.0)

    return mesh


def generate_houses():
    old_palette = gp.MATERIALS
    old_order = gp.MAT_ORDER
    gp.MATERIALS = dict(HOUSE_MATERIALS)
    gp.MAT_ORDER = list(HOUSE_MATERIALS.keys())
    out_dir = os.path.join(MODEL_ROOT, "village")
    os.makedirs(out_dir, exist_ok=True)
    houses = {
        "forge": (3.0, 2.5, 3.0, 1.45, "roof"),
        "hall": (2.5, 2.5, 3.2, 1.8, "roof_light"),
        "chapel": (3.0, 2.5, 3.3, 2.1, "roof"),
        "inn": (2.5, 2.5, 3.0, 1.55, "roof_light"),
        "farmhouse": (3.0, 2.5, 2.8, 1.35, "roof"),
    }
    try:
        for name, dimensions in houses.items():
            mesh = house_mesh(name, *dimensions)
            if name == "forge":
                add_box(mesh, "stone", 2.0, 1.0, 2.8, 5.4, 0.85, 0.85)
                add_box(mesh, "iron", 2.0, 1.0, 2.2, 2.9, 1.1, 0.9)
                add_box(mesh, "stone", 2.0, 1.0, 5.35, 5.58, 1.0, 1.0)
                add_box(mesh, "wood", 0.0, -3.0, 2.35, 2.55, 3.6, 0.32)
                add_box(mesh, "iron", 1.55, -3.45, 0.42, 0.78, 0.72, 0.5)
            elif name == "hall":
                for x in (-1.5, 1.5):
                    add_box(mesh, "stone", x, -3.0, 0.42, 3.45, 0.32, 0.32)
                add_box(mesh, "timber", 0.0, -3.0, 3.35, 3.65, 3.5, 0.55)
                add_box(mesh, "roof_light", 0.0, -3.0, 3.65, 3.82, 4.0, 0.85)
                add_box(mesh, "gold", 0.0, -3.3, 2.15, 3.05, 0.12, 0.1)
                add_box(mesh, "gold", 0.0, -3.32, 3.0, 3.1, 0.55, 0.08)
            elif name == "chapel":
                add_box(mesh, "stone", 0.0, 1.0, 3.9, 6.2, 0.95, 0.95)
                for a, b in (((-0.95, 0.05), (0.95, 0.05)),
                             ((0.95, 0.05), (0.95, 1.95)),
                             ((0.95, 1.95), (-0.95, 1.95)),
                             ((-0.95, 1.95), (-0.95, 0.05))):
                    add_triangle(mesh, "roof", (a[0], a[1], 6.2), (b[0], b[1], 6.2),
                                 (0.0, 1.0, 8.15))
                add_box(mesh, "gold", 0.0, 0.55, 8.1, 8.65, 0.10, 0.12)
                add_box(mesh, "gold", 0.0, 0.55, 8.32, 8.46, 0.48, 0.10)
                for x in (-2.45, 2.45):
                    add_box(mesh, "stone", x, -1.7, 0.42, 2.65, 0.38, 0.62)
                add_box(mesh, "glass", 0.0, -2.66, 2.1, 2.8, 0.82, 0.08)
                for x in (-1.8, 1.8):
                    add_box(mesh, "glass", x, -2.64, 1.55, 2.45, 0.12, 0.18)
            elif name == "inn":
                for x in (-1.45, 1.45):
                    add_box(mesh, "timber", x, -3.1, 0.42, 2.7, 0.16, 0.18)
                add_box(mesh, "timber", 0.0, -3.1, 2.55, 2.75, 3.1, 0.2)
                add_box(mesh, "roof_light", 0.0, -3.2, 2.75, 2.94, 3.6, 0.72)
                add_box(mesh, "timber", 0.0, -3.5, 2.0, 3.1, 1.2, 0.16)
                add_box(mesh, "wood", 0.0, -3.62, 2.15, 2.78, 0.98, 0.09)
            else:
                add_box(mesh, "wood", 2.0, 1.2, 0.42, 2.8, 1.0, 1.0)
                add_box(mesh, "roof", 2.0, 1.2, 2.8, 3.7, 1.2, 1.2)
                add_box(mesh, "stone", -2.0, 1.2, 2.2, 4.25, 0.62, 0.62)
                add_box(mesh, "stone", -2.0, 1.2, 4.18, 4.4, 0.78, 0.78)
                add_box(mesh, "wood", 0.0, -3.0, 0.42, 0.62, 2.0, 0.32)
            gp.write_glb(mesh, os.path.join(out_dir, name + ".glb"), rigged=False)
    finally:
        gp.MATERIALS = old_palette
        gp.MAT_ORDER = old_order


def main():
    generate_houses()
    generate_villagers()


if __name__ == "__main__":
    main()
