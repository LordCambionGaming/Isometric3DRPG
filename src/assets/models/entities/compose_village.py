#!/usr/bin/env python3
"""Compone un villaggio casuale (ma riproducibile col seed) dagli asset separati.

    python3 compose_village.py --manifest assets/models/village/manifest.json --seed 7 --houses 8 \
        --out village_layout.json

Output: JSON con una lista di istanze {asset, pos[x,y,z], yaw_deg, scale}. Coordinate glTF (Y-up);
yaw = rotazione attorno a +Y (il fronte delle case guarda +Z a yaw 0). I file .glb sono relativi
alla cartella `models` (padre di `village/`).
"""
import argparse, json, math, random


def rot(x, z, a):
    return x * math.cos(a) + z * math.sin(a), -x * math.sin(a) + z * math.cos(a)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default="assets/models/village/manifest.json")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--houses", type=int, default=7)
    ap.add_argument("--out", default="village_layout.json")
    a = ap.parse_args()
    rnd = random.Random(a.seed)
    man = json.load(open(a.manifest))
    H, P, PR = man["houses"], man["parts"], man["props"]
    inst, placed = [], []          # placed: (x, z, radius)

    def add(asset, x, z, yaw=0.0, y=0.0, scale=1.0):
        inst.append({"asset": asset, "pos": [round(x, 3), y, round(z, 3)],
                     "yaw_deg": round(math.degrees(yaw), 1), "scale": scale})

    def free(x, z, r):
        return all(math.hypot(x - px, z - pz) >= r + pr for px, pz, pr in placed)

    # piazza centrale: pozzo o laghetto
    centre = rnd.choice(["well", "pond"])
    add(PR[centre]["file"], 0, 0)
    placed.append((0, 0, PR[centre]["radius"] + 1.0))

    names = list(H)
    chosen = [names[i % len(names)] for i in range(a.houses)]
    rnd.shuffle(chosen)
    R = 14.0 + 1.6 * a.houses
    for n in chosen:
        h = H[n]
        r = h["radius"] + 2.0
        for _ in range(300):
            ang, rad = rnd.uniform(0, 2 * math.pi), rnd.uniform(R * 0.45, R)
            x, z = rad * math.cos(ang), rad * math.sin(ang)
            if free(x, z, r):
                break
        else:
            continue
        placed.append((x, z, r))
        yaw = math.atan2(-x, -z) + rnd.uniform(-0.25, 0.25)       # fronte verso il centro
        pal = rnd.choice(h["variants"])
        add(pal, x, z, yaw)
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
        # qualche oggetto davanti alla casa
        fx, fz = rot(0, h["half_depth"] + 2.2, yaw)
        for prop in rnd.sample(["barrel", "bench", "flower_patch", "pumpkin_patch", "crate_stack", "bush"], 2):
            ox, oz = rot(rnd.uniform(-2, 2), 0, yaw)
            add(PR[prop]["file"], x + fx + ox, z + fz + oz, yaw + rnd.uniform(-0.4, 0.4))

    # natura attorno
    for _ in range(a.houses * 6):
        prop = rnd.choice(["tree_round", "tree_pine", "tree_round", "bush", "rock", "mushroom_red", "flower_patch"])
        for _ in range(60):
            ang, rad = rnd.uniform(0, 2 * math.pi), rnd.uniform(R * 0.3, R * 1.5)
            x, z = rad * math.cos(ang), rad * math.sin(ang)
            r = PR[prop]["radius"] + 0.3
            if free(x, z, r):
                placed.append((x, z, r))
                add(PR[prop]["file"], x, z, rnd.uniform(0, 2 * math.pi), scale=round(rnd.uniform(0.85, 1.25), 2))
                break
    json.dump({"seed": a.seed, "instances": inst}, open(a.out, "w"), indent=1)
    print("%d istanze -> %s" % (len(inst), a.out))


if __name__ == "__main__":
    main()
