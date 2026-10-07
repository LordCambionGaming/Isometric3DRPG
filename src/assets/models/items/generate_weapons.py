#!/usr/bin/env python3
"""
Procedural low-poly weapons for the generate_player.py character: Longsword, Dagger, Bow (+ Arrow).
Pure Python, no Blender needed.  Must sit in the same folder as generate_player.py (it reuses its helpers).

Usage:
    python3 tools/generate_weapons.py --out-dir assets/models/weapons

Writes longsword.glb, dagger.glb, bow.glb, arrow.glb (the names used by Items.cpp: models/items/<key>.glb).

Mounting (the important part)
    Longsword / Dagger / Bow are authored directly in the *hand bone's* local space (Y-up glTF, origin =
    the Hand joint).  To equip: parent the model to bone Hand.R (sword, dagger) or Hand.L (bow) with an
    IDENTITY transform.  The grip is already placed in the fist.  Arrow is a free projectile model.
        Longsword, Dagger -> Hand.R   (blade tilted up 20 deg from the fingers' forward axis, edge toward the
                                       front when the forearm is raised)
        Bow               -> Hand.L   (stave runs along the hand's X axis: vertical during the Bow animation;
                                       carry it on the back at other times, it is not meant for Idle/Walk)
    Flat colours only: NO baked shading, vertex colours are plain white (the game does its own lighting).
    Normals are exported so an engine light can be used.

Bow rig (bow.glb, 5 joints, rigid / per-vertex skinning)
    Root (grip) -> LimbTop, LimbBottom (flex), String (the nock point) -> ArrowRoot (arrow, scaled 0 when not nocked)
    Animation "Bow" (1.7 s, 60 fps) is baked from the character's own Bow pose, so the string really follows
    the right hand, the limbs flex, the arrow appears when nocked and disappears at the release (release_time
    in extras).  Play it in sync with the player's "Bow" animation.  Ignore it and you get a static bow.
"""
import math
import struct
import json
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "entities")))
import generate_player as gp

W_MATERIALS = {
    "steel":        (0.80, 0.82, 0.86),
    "steel_dark":   (0.52, 0.55, 0.62),
    "gold":         (0.80, 0.62, 0.18),
    "leather":      (0.32, 0.20, 0.10),
    "leather_dark": (0.20, 0.12, 0.06),
    "wood":         (0.55, 0.34, 0.15),
    "wood_dark":    (0.33, 0.20, 0.09),
    "string":       (0.90, 0.86, 0.72),
    "feather":      (0.80, 0.18, 0.15),
}
W_ORDER = list(W_MATERIALS.keys())

GRIP_OFFSET = (0.0, -0.05, 0.0)     # fist centre, in Hand bone space (hand joint -> middle of the hand mesh)
BLADE_TILT = 20.0                   # degrees, blade pointing up from the forward axis of the hand
STRING_GRIP = 0.09                  # distance from the Hand.R joint to where the fingers pinch the string

# bow geometry (bow frame B: +Z = stave up, +Y = toward the target, belly/string toward -Y)
BRACE = 0.14                        # grip -> string distance, unstrung-at-rest
LIMB_PIVOT = 0.07
LIMB_FLEX = 14.0                    # max degrees each limb bends back at full draw
ARROW_LEN = 0.90
FULL_DRAW = 0.70                    # nock travel (m) that counts as 100% draw for the limb flex


# ---------------------------------------------------------------------------------------------------
# small vector helpers
# ---------------------------------------------------------------------------------------------------
def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return ((1, 0, 0), (0, c, -s), (0, s, c))

def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return ((c, -s, 0), (s, c, 0), (0, 0, 1))

def mv(M, v):
    return tuple(sum(M[i][j] * v[j] for j in range(3)) for i in range(3))

def mm(A, B):
    return tuple(tuple(sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)) for i in range(3))

def mt(M):
    return tuple(tuple(M[j][i] for j in range(3)) for i in range(3))

def vadd(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def vsub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])

def qmat(q):
    x, y, z, w = q
    return ((1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)),
            (2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
            (2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)))


def transform_polys(m, start, fn):
    """Apply a point transform to every polygon added to mesh m after index `start`."""
    for i in range(start, len(m.polys)):
        mat, pts, bone = m.polys[i]
        m.polys[i] = (mat, [fn(p) for p in pts], bone)


def loft(m, mat, sections):
    """Closed loft through (z, [(x, y), ...]) sections that all have the same number of points (CCW seen from +Z)."""
    n = len(sections[0][1])
    rings = [[(x, y, z) for x, y in pts] for z, pts in sections]
    m.add(mat, rings[0] + rings[-1], [tuple(reversed(range(n))), tuple(range(n, 2 * n))])
    for a, b in zip(rings, rings[1:]):
        v = a + b
        m.add(mat, v, [(k, (k + 1) % n, n + (k + 1) % n, n + k) for k in range(n)])


def blade_section(w, t):
    """Flattened diamond: thin along X (flats face +-X), sharp edges at +-Y."""
    return [(0, -w / 2), (t / 2, 0), (0, w / 2), (-t / 2, 0)]


def string_segment(m, mat, p_lo, p_hi, bone_lo, bone_hi, r=0.0045):
    """Thin square-section bar whose two ends are skinned to different joints (lo end = lower Z)."""
    ring = [(-r, -r), (r, -r), (r, r), (-r, r)]
    v = [(p_lo[0] + x, p_lo[1] + y, p_lo[2]) for x, y in ring] + [(p_hi[0] + x, p_hi[1] + y, p_hi[2]) for x, y in ring]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    for f in faces:
        m.polys.append((mat, [v[i] for i in f], [(bone_lo if i < 4 else bone_hi) for i in f]))


# ---------------------------------------------------------------------------------------------------
# weapon geometry (each in its own natural authoring frame, +Z along the long axis)
# ---------------------------------------------------------------------------------------------------
def build_longsword():
    m = gp.Mesh()
    m.bone = "Weapon"
    # grip + wraps
    gp.prism(m, "leather", 0, 0, -0.075, 0.075, 0.017, 0.017, n=8)
    for z in (-0.045, -0.005, 0.035):
        gp.prism(m, "leather_dark", 0, 0, z, z + 0.014, 0.0195, 0.0195, n=8)
    # pommel
    gp.prism(m, "gold", 0, 0, -0.112, -0.075, 0.020, 0.030, n=8)
    gp.prism(m, "gold", 0, 0, -0.130, -0.112, 0.012, 0.020, n=8)
    # crossguard (along Y = blade width), with upturned ends
    gp.frustum(m, "gold", 0, 0, 0.075, 0.103, 0.030, 0.240, 0.030, 0.230)
    for s in (-1, 1):
        gp.frustum(m, "gold", 0, s * 0.118, 0.073, 0.112, 0.030, 0.026, 0.026, 0.020)
    gp.prism(m, "gold", 0, 0, 0.103, 0.125, 0.026, 0.016, n=8)
    # blade: ricasso -> long taper -> point
    loft(m, "steel", [
        (0.125, blade_section(0.054, 0.016)),
        (0.300, blade_section(0.050, 0.014)),
        (0.640, blade_section(0.040, 0.011)),
        (0.790, blade_section(0.028, 0.008)),
        (0.880, blade_section(0.002, 0.002)),
    ])
    # fuller (darker centre ridge on both flats)
    for s in (-1, 1):
        gp.frustum(m, "steel_dark", s * 0.0055, 0, 0.16, 0.66, 0.003, 0.012, 0.003, 0.009)
    return m


def build_dagger():
    m = gp.Mesh()
    m.bone = "Weapon"
    gp.prism(m, "leather", 0, 0, -0.055, 0.055, 0.0155, 0.0155, n=8)
    for z in (-0.03, 0.005):
        gp.prism(m, "leather_dark", 0, 0, z, z + 0.011, 0.018, 0.018, n=8)
    gp.prism(m, "gold", 0, 0, -0.080, -0.055, 0.017, 0.024, n=8)
    gp.frustum(m, "gold", 0, 0, 0.055, 0.075, 0.026, 0.110, 0.026, 0.104)
    for s in (-1, 1):
        gp.frustum(m, "gold", 0, s * 0.056, 0.052, 0.082, 0.026, 0.020, 0.022, 0.014)
    loft(m, "steel", [
        (0.075, blade_section(0.040, 0.012)),
        (0.190, blade_section(0.033, 0.011)),
        (0.300, blade_section(0.017, 0.007)),
        (0.340, blade_section(0.002, 0.002)),
    ])
    return m


def build_arrow_parts(m, bone, tail_z=0.0):
    """Arrow along +Z, tail at tail_z (call transform_polys afterwards to orient it)."""
    start = len(m.polys)
    old = m.bone
    m.bone = bone
    shaft_end = tail_z + ARROW_LEN - 0.055
    gp.prism(m, "wood", 0, 0, tail_z, shaft_end, 0.0050, 0.0050, n=6)
    gp.prism(m, "wood_dark", 0, 0, tail_z - 0.008, tail_z + 0.012, 0.0060, 0.0050, n=6)          # nock
    gp.prism(m, "steel", 0, 0, shaft_end, shaft_end + 0.055, 0.0085, 0.0012, n=6)                  # head
    for k in range(3):                                                                              # fletching
        a = k * 2 * math.pi / 3
        s = len(m.polys)
        gp.frustum(m, "feather", 0, 0.012, tail_z + 0.018, tail_z + 0.105, 0.0025, 0.020, 0.0025, 0.012)
        R = rot_z(a)
        transform_polys(m, s, lambda p, R=R: mv(R, p))
    m.bone = old
    return start


def build_arrow():
    m = gp.Mesh()
    build_arrow_parts(m, "Weapon")
    return m


def build_quiver():
    m = gp.Mesh()
    m.bone = "Quiver"
    gp.prism(m, "leather", 0, 0, 0.0, 0.66, 0.105, 0.135, n=8)
    gp.prism(m, "leather_dark", 0, 0, 0.18, 0.22, 0.139, 0.142, n=8)
    gp.prism(m, "gold", 0, 0, 0.47, 0.495, 0.141, 0.144, n=8)
    gp.prism(m, "leather_dark", 0, 0, 0.58, 0.68, 0.143, 0.143, n=8)
    gp.frustum(m, "gold", 0, 0.139, 0.28, 0.36, 0.070, 0.022, 0.070, 0.022)
    for x in (-0.045, 0.0, 0.045):
        gp.prism(m, "wood", x, 0, 0.56, 0.91, 0.005, 0.005, n=5)
        gp.frustum(m, "feather", x, 0, 0.78, 0.89, 0.008, 0.024, 0.008, 0.014)
    return m


def build_bow():
    m = gp.Mesh()
    m.bone = "Root"
    gp.prism(m, "leather", 0, 0, -0.075, 0.075, 0.019, 0.019, n=8)
    for z in (-0.050, 0.030):
        gp.prism(m, "leather_dark", 0, 0, z, z + 0.015, 0.0215, 0.0215, n=8)
    gp.frustum(m, "wood_dark", -0.024, 0.0, 0.040, 0.085, 0.020, 0.030, 0.014, 0.022)     # arrow shelf
    # limbs: sheared frusta, curve toward -Y (toward the archer)
    seg = [  # z0, z1, y0, y1, w0, d0, w1, d1
        (0.070, 0.200, 0.000, -0.010, 0.034, 0.040, 0.030, 0.034),
        (0.200, 0.340, -0.010, -0.040, 0.030, 0.034, 0.026, 0.028),
        (0.340, 0.470, -0.040, -0.088, 0.026, 0.028, 0.020, 0.022),
        (0.470, 0.560, -0.088, -BRACE, 0.020, 0.022, 0.013, 0.015),
    ]
    for sgn, bone in ((1, "LimbTop"), (-1, "LimbBottom")):
        m.bone = bone
        for z0, z1, y0, y1, w0, d0, w1, d1 in seg:
            mat = "wood" if z0 < 0.4 else "wood_dark"
            if sgn > 0:
                gp.frustum(m, mat, 0, y0, z0, z1, w0, d0, w1, d1, 0, y1 - y0)
            else:
                gp.frustum(m, mat, 0, y1, -z1, -z0, w1, d1, w0, d0, 0, y0 - y1)
        # tip cap
        if sgn > 0:
            gp.frustum(m, "gold", 0, -BRACE, 0.560, 0.585, 0.013, 0.015, 0.008, 0.009)
        else:
            gp.frustum(m, "gold", 0, -BRACE, -0.585, -0.560, 0.008, 0.009, 0.013, 0.015)
    # string: tip (limb joint) -> nock (String joint)
    string_segment(m, "string", (0, -BRACE, 0.0), (0, -BRACE, 0.585), "String", "LimbTop")
    string_segment(m, "string", (0, -BRACE, -0.585), (0, -BRACE, 0.0), "LimbBottom", "String")
    # nocked arrow: built along +Z then laid along +Y, tail on the nock point, riding on the shelf side
    s = len(m.polys)
    build_arrow_parts(m, "ArrowRoot")
    transform_polys(m, s, lambda p: (p[0] - 0.024, p[2] - BRACE, -p[1]))
    return m


# ---------------------------------------------------------------------------------------------------
# mounting transforms (weapon frame -> Hand bone space, glTF Y-up)
# ---------------------------------------------------------------------------------------------------
def make_blade_mount():
    R = rot_x(-math.radians(BLADE_TILT))        # blade +Z tilts toward +Y (up)
    return (lambda p: vadd(mv(R, p), GRIP_OFFSET)), (lambda d: mv(R, d))


def bow_mount_pt(p):                            # B (x right, y target, z up) -> hand: (z, -y, x)
    return (p[2] + GRIP_OFFSET[0], -p[1] + GRIP_OFFSET[1], p[0] + GRIP_OFFSET[2])


def bow_mount_dir(d):
    return (d[2], -d[1], d[0])


def arrow_mount_pt(p):
    return (p[0], p[1], p[2] - ARROW_LEN / 2)


# ---------------------------------------------------------------------------------------------------
# bow rig + baked animation
# ---------------------------------------------------------------------------------------------------
BOW_JOINTS = [   # name, parent, rest position in hand space
    ("Root",       None,    GRIP_OFFSET),
    ("LimbTop",    "Root",  bow_mount_pt((0, 0, LIMB_PIVOT))),
    ("LimbBottom", "Root",  bow_mount_pt((0, 0, -LIMB_PIVOT))),
    ("String",     "Root",  bow_mount_pt((0, -BRACE, 0))),
    ("ArrowRoot",  "String", bow_mount_pt((0, -BRACE, 0))),
]
REST_NOCK = BOW_JOINTS[3][2]


def _fk_bones(P):
    """Pure-Python FK of the player skeleton -> {bone: (world_pos, world_rot)} (glTF space)."""
    r, hips = gp.solve(P)
    world = {b[0]: (b[2][0], b[2][2], -b[2][1]) for b in gp.SKELETON}
    out = {}
    for name, parent, _ in gp.SKELETON:
        R = qmat(r[name])
        if parent is None:
            out[name] = (tuple(hips), R)
        else:
            pp, pR = out[parent]
            out[name] = (vadd(pp, mv(pR, vsub(world[name], world[parent]))), mm(pR, R))
    return out


def _smooth(u):
    u = min(max(u, 0.0), 1.0)
    return u * u * (3 - 2 * u)


def bow_pose(t):
    """Joint pose of the bow at time t, derived from the player's Bow animation."""
    bones = _fk_bones(gp.pose_bow(t))
    hl_p, hl_R = bones["Hand.L"]
    hr_p, hr_R = bones["Hand.R"]
    grip_w = vadd(hr_p, mv(hr_R, (0, -STRING_GRIP, 0)))
    nock_h = mv(mt(hl_R), vsub(grip_w, hl_p))                 # right-hand pinch point, in the bow-hand frame
    rest = REST_NOCK
    rel = gp.BOW_RELEASE_TIME
    nock_start = 0.65
    if t < rel:
        w = _smooth((t - nock_start) / 0.07)
        nock = tuple(rest[i] + w * (nock_h[i] - rest[i]) for i in range(3))
    else:                                                     # string snaps back and vibrates
        tau = t - rel
        # last pulled position fades out, then the string rings along the bow axis
        ring = 0.05 * math.exp(-tau / 0.06) * math.cos(2 * math.pi * tau / 0.11)
        nock = (rest[0], rest[1] + ring, rest[2])             # +Y_hand = toward the archer
    pull = math.sqrt(sum((nock[i] - rest[i]) ** 2 for i in range(3)))
    flex = math.radians(LIMB_FLEX) * min(1.0, pull / FULL_DRAW)
    if t >= rel:
        flex = 0.0 if (t - rel) > 0.05 else flex * (1 - (t - rel) / 0.05)
    nocked = nock_start <= t < rel
    s = 1.0 if nocked else 0.0
    return {
        "LimbTop":    {"r": _qz(+flex)},
        "LimbBottom": {"r": _qz(-flex)},
        "String":     {"t": vsub(nock, BOW_JOINTS[0][2])},
        "ArrowRoot":  {"s": (s, s, s) if s else (0.0001, 0.0001, 0.0001)},
    }


def _qz(a):
    return (0.0, 0.0, math.sin(a / 2), math.cos(a / 2))


# ---------------------------------------------------------------------------------------------------
# GLB writer (flat colours, white vertex colours, optional skeleton + animation)
# ---------------------------------------------------------------------------------------------------
def write_weapon_glb(path, name, mesh, to_pt, to_dir, joints=None, anim=None, extras=None):
    blob = bytearray()
    views, accessors, prims, mats = [], [], [], []

    def add_acc(data, ctype, count, atype, target=None, mn=None, mx=None, normalized=False):
        while len(blob) % 4:
            blob.append(0)
        v = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            v["target"] = target
        blob.extend(data)
        views.append(v)
        a = {"bufferView": len(views) - 1, "componentType": ctype, "count": count, "type": atype}
        if normalized:
            a["normalized"] = True
        if mn is not None:
            a["min"], a["max"] = mn, mx
        accessors.append(a)
        return len(accessors) - 1

    jidx = {j[0]: i for i, j in enumerate(joints)} if joints else {}
    for mname in W_ORDER:
        polys = [(p, b) for mt_, p, b in mesh.polys if mt_ == mname]
        if not polys:
            continue
        pos, nor, jnt, idx = [], [], [], []
        for poly, bone in polys:
            n = to_dir(gp.face_normal(poly))
            base = len(pos)
            for k, p in enumerate(poly):
                pos.append(to_pt(p))
                nor.append(n)
                if joints:
                    jnt.append(jidx[bone[k] if isinstance(bone, list) else bone])
            for k in range(1, len(poly) - 1):
                idx += [base, base + k, base + k + 1]
        assert len(pos) < 65535
        pmin = [min(p[i] for p in pos) for i in range(3)]
        pmax = [max(p[i] for p in pos) for i in range(3)]
        attrs = {
            "POSITION": add_acc(b"".join(struct.pack("<3f", *p) for p in pos), 5126, len(pos), "VEC3", 34962, pmin, pmax),
            "NORMAL": add_acc(b"".join(struct.pack("<3f", *p) for p in nor), 5126, len(nor), "VEC3", 34962),
            "COLOR_0": add_acc(b"\xff\xff\xff\xff" * len(pos), 5121, len(pos), "VEC4", 34962, normalized=True),
        }
        if joints:
            attrs["JOINTS_0"] = add_acc(b"".join(struct.pack("<4B", j, 0, 0, 0) for j in jnt), 5121, len(jnt), "VEC4", 34962)
            attrs["WEIGHTS_0"] = add_acc(b"".join(struct.pack("<4f", 1, 0, 0, 0) for _ in jnt), 5126, len(jnt), "VEC4", 34962)
        a_idx = add_acc(b"".join(struct.pack("<H", i) for i in idx), 5123, len(idx), "SCALAR", 34963)
        r, g, b = W_MATERIALS[mname]
        mats.append({"name": mname, "pbrMetallicRoughness": {
            "baseColorFactor": [r, g, b, 1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0}})
        prims.append({"attributes": attrs, "indices": a_idx, "material": len(mats) - 1, "mode": 4})

    gltf = {"asset": {"version": "2.0", "generator": "generate_weapons.py"}, "scene": 0,
            "meshes": [{"name": name, "primitives": prims}], "materials": mats}
    root = {"name": name, "mesh": 0}
    if extras:
        root["extras"] = extras
    if not joints:
        gltf["nodes"] = [root]
        gltf["scenes"] = [{"nodes": [0]}]
    else:
        root["skin"] = 0
        nodes = [root]
        for i, (jn, parent, wp) in enumerate(joints):
            pw = joints[jidx[parent]][2] if parent else (0, 0, 0)
            nodes.append({"name": jn, "translation": [round(wp[k] - pw[k], 6) for k in range(3)]})
        for i, (jn, parent, _) in enumerate(joints):
            if parent:
                nodes[1 + jidx[parent]].setdefault("children", []).append(1 + i)
        ibm = b"".join(struct.pack("<16f", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -w[0], -w[1], -w[2], 1) for _, _, w in joints)
        a_ibm = add_acc(ibm, 5126, len(joints), "MAT4")
        gltf["nodes"] = nodes
        gltf["scenes"] = [{"nodes": [0, 1]}]
        gltf["skins"] = [{"name": name + "Skin", "joints": [1 + i for i in range(len(joints))],
                          "skeleton": 1, "inverseBindMatrices": a_ibm}]
        if anim:
            aname, duration, fps, fn, aextras = anim
            n = int(round(duration * fps))
            times = [k / fps for k in range(n + 1)]
            samples = [fn(t) for t in times]
            a_time = add_acc(struct.pack("<%df" % len(times), *times), 5126, len(times), "SCALAR", mn=[0.0], mx=[times[-1]])
            samplers, channels = [], []
            for i, (jn, parent, wp) in enumerate(joints):
                rest_t = nodes[1 + i]["translation"]
                for chan_path, key, default in (("translation", "t", None), ("rotation", "r", (0, 0, 0, 1)), ("scale", "s", (1, 1, 1))):
                    vals = []
                    for smp in samples:
                        v = smp.get(jn, {}).get(key)
                        if chan_path == "translation":
                            v = tuple(rest_t[k] + (v[k] if v else 0.0) for k in range(3))
                        elif v is None:
                            v = default
                        vals.append(v)
                    ncomp = 4 if chan_path == "rotation" else 3
                    acc = add_acc(b"".join(struct.pack("<%df" % ncomp, *v) for v in vals), 5126, len(vals),
                                  "VEC4" if ncomp == 4 else "VEC3")
                    samplers.append({"input": a_time, "output": acc, "interpolation": "LINEAR"})
                    channels.append({"sampler": len(samplers) - 1, "target": {"node": 1 + i, "path": chan_path}})
            gltf["animations"] = [{"name": aname, "samplers": samplers, "channels": channels, "extras": aextras}]

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
    tris = sum(len(p) - 2 for _, p, _b in mesh.polys)
    print("Wrote %s (%d bytes, %d tris%s)" % (path, total, tris, ", rigged + animated" if joints else ""))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = argv[argv.index("--out-dir") + 1] if "--out-dir" in argv else "assets/models/weapons"
    if "--only" in argv:
        only = argv[argv.index("--only") + 1]
        if only == "bow":
            write_weapon_glb(os.path.join(out, "bow.glb"), "Bow", build_bow(), bow_mount_pt, bow_mount_dir,
                             joints=BOW_JOINTS,
                             anim=("Bow", 1.7, 60, bow_pose, {"loop": False, "release_time": gp.BOW_RELEASE_TIME}),
                             extras={"mount_bone": "Hand.L", "transform": "identity", "height_m": 1.17,
                                     "note": "play animation 'Bow' in sync with the player's 'Bow'; hide/spawn arrow at release_time"})
            return
        if only != "quiver":
            raise SystemExit("Unsupported item: %s" % only)
        write_weapon_glb(os.path.join(out, "quiver.glb"), "Quiver", build_quiver(), lambda p: p, lambda d: d,
                         extras={"height_m": 0.91, "note": "Low-poly leather quiver with three arrows"})
        return

    pt, dr = make_blade_mount()
    write_weapon_glb(os.path.join(out, "longsword.glb"), "Longsword", build_longsword(), pt, dr,
                     extras={"mount_bone": "Hand.R", "transform": "identity", "length_m": 1.01,
                             "blade_tilt_deg": BLADE_TILT, "grip_axis": "Z (blade), edge toward -Y"})
    write_weapon_glb(os.path.join(out, "dagger.glb"), "Dagger", build_dagger(), pt, dr,
                     extras={"mount_bone": "Hand.R", "transform": "identity", "length_m": 0.42,
                             "blade_tilt_deg": BLADE_TILT, "grip_axis": "Z (blade), edge toward -Y"})
    write_weapon_glb(os.path.join(out, "bow.glb"), "Bow", build_bow(), bow_mount_pt, bow_mount_dir,
                     joints=BOW_JOINTS,
                     anim=("Bow", 1.7, 60, bow_pose, {"loop": False, "release_time": gp.BOW_RELEASE_TIME}),
                     extras={"mount_bone": "Hand.L", "transform": "identity", "height_m": 1.17,
                             "note": "play animation 'Bow' in sync with the player's 'Bow'; hide/spawn arrow at release_time"})
    write_weapon_glb(os.path.join(out, "arrow.glb"), "Arrow", build_arrow(), arrow_mount_pt, lambda d: d,
                     extras={"length_m": ARROW_LEN, "note": "origin at the middle of the shaft, points along +Z"})


if __name__ == "__main__":
    main()
