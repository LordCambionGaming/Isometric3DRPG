#!/usr/bin/env python3
"""Compone un villaggio casuale (ma riproducibile col seed) dagli asset separati.

    python3 compose_village.py --manifest assets/models/village/manifest.json --seed 7 --houses 8 \
        --out village_layout.json

Output: JSON con una lista di istanze {asset, pos[x,y,z], yaw_deg, scale}. Coordinate glTF (Y-up);
yaw = rotazione attorno a +Y (il fronte delle case guarda +Z a yaw 0). I file .glb sono relativi
alla cartella `models` (padre di `village/`).
"""
import argparse, json, math, random
import os


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
MODEL_ROOT = os.path.join(ROOT, "assets", "models")


def rot(x, z, a):
    return x * math.cos(a) + z * math.sin(a), -x * math.sin(a) + z * math.cos(a)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=os.path.join(MODEL_ROOT, "village", "manifest.json"))
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--houses", type=int, default=7)
    ap.add_argument("--out", default=os.path.join(MODEL_ROOT, "village", "layout.json"))
    a = ap.parse_args()
    rnd = random.Random(a.seed)
    with open(a.manifest, encoding="utf-8") as manifest_file:
        man = json.load(manifest_file)
    H, P, PR = man["houses"], man["parts"], man["props"]
    inst, placed = [], []          # placed: (x, z, radius)

    def add(asset, x, z, yaw=0.0, y=0.0, scale=1.0, **metadata):
        instance = {"asset": asset, "pos": [round(x, 3), y, round(z, 3)],
                    "yaw_deg": round(math.degrees(yaw), 1), "scale": scale}
        instance.update(metadata)
        inst.append(instance)

    def free(x, z, r):
        return all(math.hypot(x - px, z - pz) >= r + pr for px, pz, pr in placed)

    # Lascia liberi i tile di spawn e di uscita dalla piazza.
    centre = rnd.choice(["well", "pond"])
    add(PR[centre]["file"], 4.0, 4.0, collision_radius=PR[centre]["radius"])
    placed.append((4.0, 4.0, PR[centre]["radius"] + 1.0))

    names = list(H)
    sites = [
        ((0.0, -20.0), 0.0), ((14.0, -14.0), -math.pi / 2),
        ((20.0, 0.0), -math.pi / 2), ((14.0, 14.0), -math.pi / 2),
        ((0.0, 20.0), math.pi), ((-14.0, 14.0), math.pi / 2),
        ((-20.0, 0.0), math.pi / 2), ((-14.0, -14.0), math.pi / 2),
    ]
    chosen = [names[i % len(names)] for i in range(min(a.houses, len(sites)))]
    rnd.shuffle(chosen)
    for index, n in enumerate(chosen):
        h = H[n]
        (x, z), yaw = sites[index]
        r = h["radius"] + 2.0
        placed.append((x, z, r))
        pal = rnd.choice(h["variants"])
        door_x, door_z = rot(h["door"]["step_pos"][0], h["door"]["step_pos"][2], yaw)
        door_x, door_z = x + door_x, z + door_z
        front_x, front_z = rot(0.0, 1.0, yaw)
        if abs(front_x) > abs(front_z):
            road_path = [[0.0, 0.0], [0.0, round(door_z, 3)],
                         [round(door_x, 3), round(door_z, 3)]]
        else:
            road_path = [[0.0, 0.0], [round(door_x, 3), 0.0],
                         [round(door_x, 3), round(door_z, 3)]]
        add(pal, x, z, yaw, kind=n, half_width=h["half_width"],
            half_depth=h["half_depth"], door_step_pos=h["door"]["step_pos"],
            collision_radius=h["radius"], road_path=road_path)
        for (x0, z0), (x1, z1) in zip(road_path, road_path[1:]):
            steps = int(max(abs(x1 - x0), abs(z1 - z0)))
            if steps == 0:
                continue
            for step in range(steps + 1):
                amount = step / steps
                placed.append((x0 + (x1 - x0) * amount, z0 + (z1 - z0) * amount, 0.8))
        for slot, s in h["slots"].items():
            if slot == "smoke" and "chimney" not in h["slots"]:
                continue
            if slot in ("awning", "hanging_sign") and rnd.random() < 0.3:
                continue
            asset = s["asset"]
            if slot == "awning":                                    # stessa palette della casa
                asset = rnd.choice(P[asset.split("/")[-1]]["variants"])
            else:
                asset = "village/" + asset + ".glb"
            dx, dz = rot(s["pos"][0], s["pos"][2], yaw)
            add(asset, x + dx, z + dz, yaw, y=s["pos"][1])
        # Decori ai lati dell'ingresso, fuori dal corridoio del sentiero.
        for side, prop in zip((-1, 1), rnd.sample(
                ["barrel", "bench", "flower_patch", "pumpkin_patch", "crate_stack", "bush"], 2)):
            local_x = side * (h["half_width"] + 1.0)
            local_z = h["half_depth"] + 1.1
            ox, oz = rot(local_x, local_z, yaw)
            prop_x, prop_z = x + ox, z + oz
            if free(prop_x, prop_z, PR[prop]["radius"]):
                add(PR[prop]["file"], prop_x, prop_z, yaw,
                    collision_radius=PR[prop]["radius"])

    # natura attorno
    outer_radius = 20.0
    for _ in range(a.houses * 6):
        prop = rnd.choice(["tree_round", "tree_pine", "tree_round", "bush", "rock", "mushroom_red", "flower_patch"])
        for _ in range(60):
            ang, rad = rnd.uniform(0, 2 * math.pi), rnd.uniform(outer_radius * 0.3, outer_radius * 1.35)
            x, z = rad * math.cos(ang), rad * math.sin(ang)
            if abs(x) < 3.0 or abs(z) < 3.0:
                continue
            r = PR[prop]["radius"] + 0.3
            if free(x, z, r):
                placed.append((x, z, r))
                scale = round(rnd.uniform(0.85, 1.25), 2)
                add(PR[prop]["file"], x, z, rnd.uniform(0, 2 * math.pi), scale=scale,
                    collision_radius=PR[prop]["radius"])
                break
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as output_file:
        json.dump({"seed": a.seed, "instances": inst}, output_file, indent=1)
    print("%d istanze -> %s" % (len(inst), a.out))


if __name__ == "__main__":
    main()
