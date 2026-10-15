"""
PLAYER GENDER BASES — Blender 3.2.0 (conforming hair, simple eyes v5)
===================================
Builds exactly TWO generic adult human bases (male and female) from the user's
Mixamo-rigged FBX source meshes. Class is intentionally NOT encoded in the body:
warrior / thief / ranger will be selected separately in the game via equipment.

What this script does:
- imports and preserves the original Mixamo armature and skin weights;
- preserves the source armature, rest pose, vertex weights and hand geometry as imported;
- recolors the existing body and adds readable white-and-iris eyes plus correctly placed round chest details;
- adds a head-conforming scalp cap and a rear hair shell to prevent bald backs, plus volumetric low-poly locks;
- optionally imports matching FBX animation clips from tools/mixamo/animations;
- exports human_male_base.glb and human_female_base.glb plus a Blender review scene.

Expected source files:
  tools/mixamo/male/base_male.fbx
  tools/mixamo/female/base_female.fbx
  tools/mixamo/animations/*.fbx   (optional; Without Skin Mixamo animations)

Output:
  assets/models/characters/bases/human_male_base.glb
  assets/models/characters/bases/human_female_base.glb
  assets/models/characters/bases/human_gender_bases_review.blend

No clothing, armor, or weapons are created. Original FBX files are never modified. Source skeleton and body vertex positions are preserved.
Blender 3.2+.
"""
import bpy
import bmesh
import json
import struct
import math
import os
import re
import traceback
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

# ----------------------------- SETTINGS -----------------------------
KNOWN_PROJECT_ROOT = Path(r"C:\Users\tiger\OneDrive\Desktop\Isometric3DRPG")
MALE_REL = Path("tools/mixamo/male/base_male.fbx")
FEMALE_REL = Path("tools/mixamo/female/base_female.fbx")
ANIMATION_REL = Path("tools/mixamo/animations")
OUTPUT_REL = Path("assets/models/characters/bases")

# Each hair is a separate full-body GLB variant so the game can select it without FBX.
HAIR_STYLES = {
    "male": ["short_spiky", "side_swept", "medium_swept", "tied_back"],
    "female": ["ponytail", "bob", "long_flow", "braid"],
}
HAIR_LABELS = {
    "short_spiky": "Short spiky", "side_swept": "Side-swept",
    "medium_swept": "Medium swept", "tied_back": "Tied back",
    "ponytail": "Ponytail", "bob": "Bob cut", "long_flow": "Long flow",
    "braid": "Braid",
}
IMPORT_AVAILABLE_ANIMATIONS = True
EXPORT_GLB = True
SAVE_REVIEW_BLEND = True
# Low-poly hair palette and stylized skin colors (adult base models).
SKIN_MALE = (0.57, 0.335, 0.225, 1.0)
SKIN_FEMALE = (0.77, 0.515, 0.365, 1.0)
HAIR_MALE = (0.16, 0.060, 0.025, 1.0)
HAIR_FEMALE = (0.58, 0.105, 0.035, 1.0)
IRIS_MALE = (0.16, 0.42, 0.66, 1.0)
IRIS_FEMALE = (0.20, 0.48, 0.28, 1.0)
# --------------------------------------------------------------------


def log(msg):
    print("[PLAYER BASES] " + str(msg))


def resolve_project_paths():
    """Find the project root without relying on Blender's sometimes-relative __file__."""
    candidates = []
    try:
        raw = globals().get("__file__", "")
        if raw:
            p = Path(os.path.abspath(str(raw)))
            if p.exists():
                candidates.append(p.parent)
    except Exception:
        pass
    try:
        for text_block in bpy.data.texts:
            if text_block.filepath:
                p = Path(bpy.path.abspath(text_block.filepath))
                if p.exists() and p.name.lower().startswith("build_player_gender_bases"):
                    candidates.append(p.parent)
    except Exception:
        pass
    candidates.extend([
        KNOWN_PROJECT_ROOT / "tools",
        Path(os.getcwd()) / "tools",
        Path(os.getcwd()),
    ])
    for candidate in candidates:
        if candidate.name.lower() == "tools":
            root = candidate.parent
            if (root / MALE_REL).is_file() and (root / FEMALE_REL).is_file():
                return root
        root = candidate
        if (root / MALE_REL).is_file() and (root / FEMALE_REL).is_file():
            return root
        if (candidate / "mixamo/male/base_male.fbx").is_file() and (candidate / "mixamo/female/base_female.fbx").is_file():
            return candidate.parent if candidate.name.lower() == "tools" else candidate
    return KNOWN_PROJECT_ROOT


PROJECT_ROOT = resolve_project_paths()
MALE_FBX = PROJECT_ROOT / MALE_REL
FEMALE_FBX = PROJECT_ROOT / FEMALE_REL
ANIMATION_DIR = PROJECT_ROOT / ANIMATION_REL
OUTPUT_DIR = PROJECT_ROOT / OUTPUT_REL


def make_material(name, rgba, roughness=0.72, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = rgba
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.node_tree else None
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
    return mat


def delete_collection_if_exists(name):
    collection = bpy.data.collections.get(name)
    if not collection:
        return
    for obj in list(collection.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.collections.remove(collection)


def move_objects_to_collection(objects, collection):
    for obj in objects:
        for old in list(obj.users_collection):
            old.objects.unlink(obj)
        collection.objects.link(obj)


def import_rigged_fbx(filepath, collection_name):
    if not filepath.is_file():
        raise FileNotFoundError("FBX non trovato: {}".format(filepath))
    delete_collection_if_exists(collection_name)
    before_objects = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(filepath), use_anim=False, automatic_bone_orientation=False)
    imported = [obj for obj in bpy.data.objects if obj not in before_objects]
    if not imported:
        raise RuntimeError("Blender non ha importato oggetti da {}".format(filepath))
    collection = bpy.data.collections.new(collection_name)
    bpy.context.scene.collection.children.link(collection)
    move_objects_to_collection(imported, collection)
    armatures = [obj for obj in imported if obj.type == "ARMATURE"]
    meshes = [obj for obj in imported if obj.type == "MESH"]
    if not armatures or not meshes:
        raise RuntimeError("Il FBX deve contenere sia Armature sia Mesh: {}".format(filepath))
    armature = armatures[0]
    armature.name = collection_name + "_Rig"
    armature.data.name = collection_name + "_Skeleton"
    armature.data.pose_position = "REST"
    bpy.context.view_layer.update()
    return collection, imported, armature, meshes


def find_bone(armature, wanted):
    wanted = wanted.lower()
    for bone in armature.data.bones:
        name = bone.name.lower()
        if name == wanted or name.endswith(":" + wanted) or name.endswith("_" + wanted):
            return bone
    return None


def bounds_of_points(points):
    if not points:
        raise RuntimeError("Nessun vertice disponibile per calcolare i limiti del corpo.")
    return {
        "xmin": min(p.x for p in points), "xmax": max(p.x for p in points),
        "ymin": min(p.y for p in points), "ymax": max(p.y for p in points),
        "zmin": min(p.z for p in points), "zmax": max(p.z for p in points),
    }


def world_vertices(meshes):
    pts = []
    for obj in meshes:
        pts.extend([obj.matrix_world @ v.co for v in obj.data.vertices])
    return pts


def points_weighted_to_bone(meshes, bone_name):
    pts = []
    for obj in meshes:
        group = obj.vertex_groups.get(bone_name)
        if group is None:
            continue
        group_index = group.index
        for vertex in obj.data.vertices:
            weight = 0.0
            for membership in vertex.groups:
                if membership.group == group_index:
                    weight = membership.weight
                    break
            if weight > 0.12:
                pts.append(obj.matrix_world @ vertex.co)
    return pts


def smooth_bell(value, center, width):
    if width <= 0:
        return 0.0
    t = (value - center) / width
    return math.exp(-0.5 * t * t)


def reshape_body(meshes, gender):
    """Subtle shape-key-like proportions applied directly to the original mesh vertices.
    The existing vertex groups and armature modifier are preserved.
    """
    pts = world_vertices(meshes)
    b = bounds_of_points(pts)
    height = max(1e-6, b["zmax"] - b["zmin"])
    width = max(1e-6, b["xmax"] - b["xmin"])
    cx = (b["xmin"] + b["xmax"]) * 0.5
    cy = (b["ymin"] + b["ymax"]) * 0.5
    for obj in meshes:
        inv = obj.matrix_world.inverted()
        for vertex in obj.data.vertices:
            p = obj.matrix_world @ vertex.co
            zn = (p.z - b["zmin"]) / height
            xn = abs(p.x - cx) / width
            relx = p.x - cx
            rely = p.y - cy
            # Masks localize shape changes: upper torso, waist, hips and limb silhouettes.
            torso_center = max(0.0, 1.0 - (xn / 0.27) ** 2)
            arm_region = min(1.0, max(0.0, (xn - 0.16) / 0.12))
            thigh_region = min(1.0, max(0.0, (xn - 0.035) / 0.12))
            chest = smooth_bell(zn, 0.715, 0.075) * torso_center
            shoulder = smooth_bell(zn, 0.775, 0.065)
            waist = smooth_bell(zn, 0.555, 0.060) * torso_center
            hip = smooth_bell(zn, 0.475, 0.075) * torso_center
            biceps = smooth_bell(zn, 0.685, 0.085) * arm_region
            thighs = smooth_bell(zn, 0.335, 0.090) * thigh_region
            if gender == "male":
                sx = 1.0 + 0.105 * chest + 0.085 * shoulder + 0.055 * biceps + 0.035 * thighs - 0.075 * waist
                sy = 1.0 + 0.070 * chest + 0.045 * biceps + 0.025 * thighs - 0.025 * waist
            else:
                sx = 1.0 + 0.030 * shoulder + 0.040 * chest + 0.065 * hip + 0.035 * thighs - 0.115 * waist
                sy = 1.0 + 0.035 * chest + 0.055 * hip + 0.020 * thighs - 0.025 * waist
            new_world = Vector((cx + relx * sx, cy + rely * sy, p.z))
            vertex.co = inv @ new_world
        obj.data.update()
    bpy.context.view_layer.update()


def recolor_body(meshes, name, rgba):
    skin = make_material(name + "_Skin", rgba, roughness=0.78)
    for obj in meshes:
        obj.data.materials.clear()
        obj.data.materials.append(skin)
        for poly in obj.data.polygons:
            poly.material_index = 0
            poly.use_smooth = False
    return skin


def get_head_bounds(meshes, armature):
    head_bone = find_bone(armature, "Head")
    if head_bone is None:
        raise RuntimeError("Lo scheletro non contiene un osso Head riconoscibile.")
    pts = points_weighted_to_bone(meshes, head_bone.name)
    if len(pts) < 30:
        all_pts = world_vertices(meshes)
        all_bounds = bounds_of_points(all_pts)
        height = all_bounds["zmax"] - all_bounds["zmin"]
        cutoff = all_bounds["zmax"] - height * 0.17
        pts = [p for p in all_pts if p.z >= cutoff]
    return head_bone.name, pts, bounds_of_points(pts)


def make_bvh(meshes):
    verts = []
    faces = []
    for obj in meshes:
        base = len(verts)
        verts.extend([tuple(obj.matrix_world @ v.co) for v in obj.data.vertices])
        for poly in obj.data.polygons:
            indices = [base + i for i in poly.vertices]
            if len(indices) == 3:
                faces.append(tuple(indices))
            else:
                for i in range(1, len(indices) - 1):
                    faces.append((indices[0], indices[i], indices[i + 1]))
    if not verts or not faces:
        raise RuntimeError("Impossibile costruire la superficie di riferimento del viso.")
    return BVHTree.FromPolygons(verts, faces, all_triangles=True)



def canonical_bone_name(name):
    return name.lower().split(":")[-1].replace("mixamorig", "")


def make_bvh_for_bones(meshes, bone_names, min_weight=0.08):
    """Build a BVH from triangles weighted mainly to the requested bones.
    This prevents chest/eye details from ray-hitting a hand, arm, or unrelated body part.
    """
    targets = {canonical_bone_name(name) for name in bone_names}
    verts, faces = [], []
    for obj in meshes:
        group_indices = set()
        for group in obj.vertex_groups:
            if canonical_bone_name(group.name) in targets:
                group_indices.add(group.index)
        if not group_indices:
            continue
        weighted = []
        for vertex in obj.data.vertices:
            weighted.append(any(m.group in group_indices and m.weight >= min_weight
                                for m in vertex.groups))
        matrix = obj.matrix_world
        for poly in obj.data.polygons:
            ids = list(poly.vertices)
            if sum(1 for idx in ids if weighted[idx]) < min(2, len(ids)):
                continue
            # Fan-triangulate selected triangles/quads/polygons into an isolated BVH.
            for i in range(1, len(ids) - 1):
                tri_ids = (ids[0], ids[i], ids[i + 1])
                base = len(verts)
                verts.extend([tuple(matrix @ obj.data.vertices[idx].co) for idx in tri_ids])
                faces.append((base, base + 1, base + 2))
    if not faces:
        raise RuntimeError("Nessuna superficie trovata per le ossa: {}".format(", ".join(bone_names)))
    return BVHTree.FromPolygons(verts, faces, all_triangles=True)


def strict_surface_hit(tree, x, z, bounds, front_sign):
    """Ray-hit only. Unlike surface_hit(), never fabricates a fallback point in empty space."""
    cy = (bounds["ymin"] + bounds["ymax"]) * 0.5
    depth = max(0.02, bounds["ymax"] - bounds["ymin"])
    origin = Vector((x, cy + front_sign * (depth * 2.0 + 0.20), z))
    direction = Vector((0.0, -float(front_sign), 0.0))
    loc, normal, face_index, distance = tree.ray_cast(origin, direction, depth * 5.0 + 1.0)
    if loc is None:
        return None, None
    outward = Vector((0.0, float(front_sign), 0.0))
    if normal.dot(outward) < 0:
        normal.negate()
    normal.normalize()
    return loc, normal


def strict_side_hit(tree, xmid, width, y, z, side_sign):
    origin = Vector((xmid + side_sign * (width * 2.0 + 0.20), y, z))
    direction = Vector((-float(side_sign), 0.0, 0.0))
    loc, normal, face_index, distance = tree.ray_cast(origin, direction, width * 5.0 + 1.0)
    if loc is None:
        return None, None
    outward = Vector((float(side_sign), 0.0, 0.0))
    if normal.dot(outward) < 0:
        normal.negate()
    normal.normalize()
    return loc, normal


def strict_back_hit(tree, x, z, bounds, front_sign):
    cy = (bounds["ymin"] + bounds["ymax"]) * 0.5
    depth = max(0.02, bounds["ymax"] - bounds["ymin"])
    origin = Vector((x, cy - front_sign * (depth * 2.0 + 0.20), z))
    direction = Vector((0.0, float(front_sign), 0.0))
    loc, normal, face_index, distance = tree.ray_cast(origin, direction, depth * 5.0 + 1.0)
    if loc is None:
        return None, None
    outward = Vector((0.0, -float(front_sign), 0.0))
    if normal.dot(outward) < 0:
        normal.negate()
    normal.normalize()
    return loc, normal

def detect_front_sign(head_points, bounds):
    """Detect which Y side is the face by looking for the forward-projecting nose region."""
    cx = (bounds["xmin"] + bounds["xmax"]) * 0.5
    cy = (bounds["ymin"] + bounds["ymax"]) * 0.5
    h = max(1e-6, bounds["zmax"] - bounds["zmin"])
    w = max(1e-6, bounds["xmax"] - bounds["xmin"])
    central = [p.y for p in head_points
               if abs(p.x - cx) < w * 0.11
               and bounds["zmin"] + h * 0.30 < p.z < bounds["zmin"] + h * 0.60]
    if len(central) < 4:
        return -1
    front_extent = cy - min(central)
    back_extent = max(central) - cy
    # For normal Mixamo/Blender imports, face is usually -Y. Only reverse when the
    # central nose region clearly projects more strongly toward +Y.
    if back_extent > front_extent * 1.10:
        return 1
    return -1


def surface_hit(tree, x, z, body_bounds, front_sign):
    cy = (body_bounds["ymin"] + body_bounds["ymax"]) * 0.5
    depth = max(0.02, body_bounds["ymax"] - body_bounds["ymin"])
    origin = Vector((x, cy + front_sign * (depth * 1.8 + 0.15), z))
    direction = Vector((0.0, -float(front_sign), 0.0))
    loc, normal, face_index, distance = tree.ray_cast(origin, direction, depth * 5.0 + 1.0)
    if loc is None:
        # Fallback only if a ray misses due to a deeply recessed stylized socket.
        loc = Vector((x, cy + front_sign * depth * 0.43, z))
        normal = Vector((0.0, float(front_sign), 0.0))
    front_dir = Vector((0.0, float(front_sign), 0.0))
    if normal.dot(front_dir) < 0:
        normal.negate()
    normal.normalize()
    return loc, normal


def attach_weighted_mesh(name, world_verts, faces, mat, armature, collection, bone_name):
    arm_inv = armature.matrix_world.inverted()
    local_verts = [tuple(arm_inv @ Vector(v)) for v in world_verts]
    mesh = bpy.data.meshes.new(name + "_Mesh")
    mesh.from_pydata(local_verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    mesh.materials.append(mat)
    for poly in mesh.polygons:
        poly.use_smooth = False
    group = obj.vertex_groups.new(name=bone_name)
    if mesh.vertices:
        group.add(list(range(len(mesh.vertices))), 1.0, "REPLACE")
    obj.parent = armature
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.matrix_world = armature.matrix_world.copy()
    modifier = obj.modifiers.new("HeadDeform", "ARMATURE")
    modifier.object = armature
    if len(mesh.polygons) > 4:
        bm = bmesh.new()
        bm.from_mesh(mesh)
        try:
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            bm.to_mesh(mesh)
            mesh.update()
        finally:
            bm.free()
    return obj


def surface_basis(normal):
    n = normal.normalized()
    u = Vector((1.0, 0.0, 0.0))
    u = u - n * u.dot(n)
    if u.length < 1e-5:
        u = Vector((0.0, 1.0, 0.0))
        u = u - n * u.dot(n)
    u.normalize()
    v = Vector((0.0, 0.0, 1.0))
    v = v - n * v.dot(n) - u * v.dot(u)
    if v.length < 1e-5:
        v = n.cross(u)
    v.normalize()
    if v.z < 0:
        v.negate()
    return u, v, n


def surface_lens(name, center, normal, rx, rz, mat, armature, collection, bone_name,
                 offset=0.00025, segments=14, almond=True, angle=0.0):
    """Shallow, low-poly inset eye detail; depth scales with its actual size."""
    u, v, n = surface_basis(normal)
    ca, sa = math.cos(angle), math.sin(angle)
    center = center + n * offset
    # Keep facial decals shallow: eye whites/iris should read as painted stylized eyes, not beads.
    dome = min(0.00120, max(0.00022, min(abs(rx), abs(rz)) * 0.115))
    rings = [(1.00, 0.0), (0.72, dome * 0.34), (0.36, dome * 0.70)]
    verts = []
    for rf, depth in rings:
        for i in range(segments):
            theta = 2.0 * math.pi * i / segments
            ex = math.cos(theta) * rx * rf
            ez = math.sin(theta) * rz * rf
            if almond:
                ez *= 0.58 + 0.42 * abs(math.sin(theta))
            ex2, ez2 = ex * ca - ez * sa, ex * sa + ez * ca
            verts.append(tuple(center + u * ex2 + v * ez2 + n * depth))
    center_index = len(verts)
    verts.append(tuple(center + n * dome))
    faces = []
    for ring in range(len(rings) - 1):
        for i in range(segments):
            a = ring * segments + i
            b = ring * segments + (i + 1) % segments
            c = (ring + 1) * segments + (i + 1) % segments
            d = (ring + 1) * segments + i
            faces.append((a, b, c, d))
    last_ring = (len(rings) - 1) * segments
    for i in range(segments):
        faces.append((last_ring + i, last_ring + (i + 1) % segments, center_index))
    return attach_weighted_mesh(name, verts, faces, mat, armature, collection, bone_name)

def extruded_polygon(name, points_xz, y_center, thickness, front_sign, mat, armature, collection, bone_name):
    pts = list(points_xz)
    if len(pts) < 3:
        return None
    n = len(pts)
    # Center the solid near the front side, but keep enough volume that it is not a paper plane.
    y_front = y_center + front_sign * thickness * 0.5
    y_back = y_center - front_sign * thickness * 0.5
    verts = [(x, y_front, z) for x, z in pts] + [(x, y_back, z) for x, z in pts]
    faces = [tuple(range(n)), tuple(range(n, 2 * n))]
    for i in range(n):
        j = (i + 1) % n
        faces.append((i, j, n + j, n + i))
    return attach_weighted_mesh(name, verts, faces, mat, armature, collection, bone_name)


def make_tapered_hair_strand(name, centers, radii, front_sign, mat, armature, collection, bone_name, sides=7):
    """Tapered low-poly lock whose sections are perpendicular to the vertical hair strand."""
    verts, faces = [], []
    for (cx, cy, cz), (rx, ry) in zip(centers, radii):
        for i in range(sides):
            a = 2.0 * math.pi * i / sides
            verts.append((cx + math.cos(a) * rx,
                          cy + math.sin(a) * ry,
                          cz))
    for r in range(len(centers) - 1):
        for i in range(sides):
            a = r * sides + i
            b = r * sides + (i + 1) % sides
            c = (r + 1) * sides + (i + 1) % sides
            d = (r + 1) * sides + i
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(centers) - 1) * sides + i for i in range(sides)))
    return attach_weighted_mesh(name, verts, faces, mat, armature, collection, bone_name)


def make_hair_lock(name, centers, radii, mat, armature, collection, bone_name, sides=7):
    """Volumetric tapered lock along a 3D path; unlike polygon cards it has real thickness.

    The control path and the radius profile do not always have the same number of
    samples (e.g. four-point swept fringes used a three-point thickness profile).
    Resample the profile to the path length so every ring has a radius pair.
    """
    points = [Vector(p) for p in centers]
    if len(points) < 2:
        raise ValueError("Hair lock {!r} needs at least two path points".format(name))
    profile = [tuple(map(float, radius)) for radius in radii]
    if not profile:
        raise ValueError("Hair lock {!r} has no radius profile".format(name))
    if len(profile) == 1:
        profile = profile * len(points)
    elif len(profile) != len(points):
        # Linear interpolation of each radius component across the available profile.
        sampled = []
        for sample_idx in range(len(points)):
            t = sample_idx * (len(profile) - 1) / float(len(points) - 1)
            lo = int(math.floor(t))
            hi = min(lo + 1, len(profile) - 1)
            blend = t - lo
            rx = profile[lo][0] * (1.0 - blend) + profile[hi][0] * blend
            ry = profile[lo][1] * (1.0 - blend) + profile[hi][1] * blend
            sampled.append((max(rx, 1e-5), max(ry, 1e-5)))
        profile = sampled
    verts, faces = [], []
    previous_u = Vector((1.0, 0.0, 0.0))
    for idx, center in enumerate(points):
        if idx == 0:
            tangent = (points[1] - center).normalized()
        elif idx == len(points) - 1:
            tangent = (center - points[idx - 1]).normalized()
        else:
            tangent = (points[idx + 1] - points[idx - 1]).normalized()
        u = previous_u - tangent * previous_u.dot(tangent)
        if u.length < 1e-5:
            u = Vector((0.0, 1.0, 0.0)) - tangent * Vector((0.0, 1.0, 0.0)).dot(tangent)
        if u.length < 1e-5:
            u = Vector((0.0, 0.0, 1.0)) - tangent * Vector((0.0, 0.0, 1.0)).dot(tangent)
        u.normalize()
        v = tangent.cross(u).normalized()
        previous_u = u
        rx, ry = profile[idx]
        for side_index in range(sides):
            angle = 2.0 * math.pi * side_index / sides
            point = center + u * (math.cos(angle) * rx) + v * (math.sin(angle) * ry)
            verts.append(tuple(point))
    for ring in range(len(points) - 1):
        for side_index in range(sides):
            a = ring * sides + side_index
            b = ring * sides + (side_index + 1) % sides
            c = (ring + 1) * sides + (side_index + 1) % sides
            d = (ring + 1) * sides + side_index
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(points) - 1) * sides + i for i in range(sides)))
    return attach_weighted_mesh(name, verts, faces, mat, armature, collection, bone_name)


def add_eyes(head_bounds, surface_bounds, front_sign, tree, armature, collection, mats, gender):
    """Simple friendly low-poly eyes: clear warm-white sclera, one colored iris, dark pupil.

    Eye pieces sit a few millimeters in front of the eye socket so the cavity does not
    swallow them or cause z-fighting. No dark socket ring is added.
    """
    head_bone = find_bone(armature, "Head")
    if head_bone is None:
        log("Eyes skipped: no Head bone found.")
        return
    xmin, xmax = head_bounds["xmin"], head_bounds["xmax"]
    zmin, zmax = head_bounds["zmin"], head_bounds["zmax"]
    width, height = xmax - xmin, zmax - zmin
    cx = (xmin + xmax) * 0.5

    # Center them in the existing sockets. A little larger/raised makes the white readable.
    eye_z = zmin + height * 0.605
    eye_sep = width * 0.185
    eye_rx = width * (0.098 if gender == "male" else 0.101)
    eye_rz = height * (0.048 if gender == "male" else 0.050)
    for side, sign in (("L", -1.0), ("R", 1.0)):
        eye_x = cx + sign * eye_sep
        hit, normal = strict_surface_hit(tree, eye_x, eye_z, surface_bounds, front_sign)
        eye_z_local = eye_z
        if hit is None:
            for dz in (height * 0.015, -height * 0.015, height * 0.028, -height * 0.028):
                hit, normal = strict_surface_hit(tree, eye_x, eye_z + dz, surface_bounds, front_sign)
                if hit is not None:
                    eye_z_local = eye_z + dz
                    break
        if hit is None:
            log("Eye {} skipped: head surface ray missed; no floating geometry created.".format(side))
            continue

        u, v, n = surface_basis(normal)
        # White shape covers the socket floor; intentionally simple and slightly almond-shaped.
        surface_lens("EyeWhite_" + side, hit, normal,
                     eye_rx, eye_rz, mats["sclera"], armature, collection, head_bone.name,
                     offset=0.0028, segments=14, almond=True)

        # One visible iris dot and a very small pupil. Avoid multiple rings/high-contrast outlines.
        iris_center = hit + u * (-sign * eye_rx * 0.055)
        surface_lens("EyeIris_" + side, iris_center, normal,
                     eye_rx * 0.31, eye_rz * 0.48, mats["iris"],
                     armature, collection, head_bone.name,
                     offset=0.0045, segments=12, almond=False)
        pupil_center = iris_center + u * (-sign * eye_rx * 0.015)
        surface_lens("EyePupil_" + side, pupil_center, normal,
                     eye_rx * 0.125, eye_rz * 0.25, mats["pupil"],
                     armature, collection, head_bone.name,
                     offset=0.0058, segments=10, almond=False)
        # Single small catchlight; restrained so the eyes stay graphic rather than glossy.
        glint_center = iris_center + u * (-eye_rx * 0.055) + v * (eye_rz * 0.13)
        surface_lens("EyeGlint_" + side, glint_center, normal,
                     eye_rx * 0.050, eye_rz * 0.080, mats["glint"],
                     armature, collection, head_bone.name,
                     offset=0.0068, segments=6, almond=False)

        # Brows are subtle; eye expression should come from the eye itself, not dark sockets.
        brow_z = eye_z_local + eye_rz * 1.42
        brow_hit, brow_normal = strict_surface_hit(tree, eye_x, brow_z, surface_bounds, front_sign)
        if brow_hit is not None:
            surface_lens("Brow_" + side, brow_hit, brow_normal,
                         eye_rx * 0.66, eye_rz * 0.075, mats["brow"],
                         armature, collection, head_bone.name,
                         offset=0.0010, segments=10, almond=True,
                         angle=-sign * 0.035)

def make_conforming_round_patch(name, cx, cz, radius, tree, bounds, front_sign,
                                mat, armature, collection, bone_name, lift, segments=14):
    """A genuinely round circular patch that conforms point-by-point to the torso surface."""
    ring_specs = [(1.0, lift * 0.15), (0.68, lift * 0.58), (0.30, lift * 0.88)]
    verts, faces = [], []
    for factor, extra in ring_specs:
        for i in range(segments):
            theta = 2.0 * math.pi * i / segments
            x = cx + math.cos(theta) * radius * factor
            z = cz + math.sin(theta) * radius * factor
            hit, normal = strict_surface_hit(tree, x, z, bounds, front_sign)
            if hit is None:
                return False
            verts.append(tuple(hit + normal * extra))
    center_hit, center_normal = strict_surface_hit(tree, cx, cz, bounds, front_sign)
    if center_hit is None:
        return False
    center_index = len(verts)
    verts.append(tuple(center_hit + center_normal * lift))
    for ring in range(len(ring_specs) - 1):
        for i in range(segments):
            a = ring * segments + i
            b = ring * segments + (i + 1) % segments
            c = (ring + 1) * segments + (i + 1) % segments
            d = (ring + 1) * segments + i
            faces.append((a, b, c, d))
    last_ring = (len(ring_specs) - 1) * segments
    for i in range(segments):
        faces.append((last_ring + i, last_ring + (i + 1) % segments, center_index))
    attach_weighted_mesh(name, verts, faces, mat, armature, collection, bone_name)
    return True


def add_nipples(chest_bounds, body_bounds, front_sign, tree, armature, collection, mats, gender,
                chest_bone_name=None):
    """Small round, surface-conforming anatomical details; not floating ovals or beads."""
    chest = armature.data.bones.get(chest_bone_name) if chest_bone_name else None
    if chest is None:
        for candidate in ("Chest", "Spine2", "UpperChest", "Spine1", "Spine"):
            chest = find_bone(armature, candidate)
            if chest is not None:
                break
    if chest is None:
        log("Chest details skipped: no existing torso bone; skeleton unchanged.")
        return
    chest_width = chest_bounds["xmax"] - chest_bounds["xmin"]
    chest_height = chest_bounds["zmax"] - chest_bounds["zmin"]
    if chest_width <= 0 or chest_height <= 0:
        log("Chest details skipped: invalid chest bounds.")
        return

    cx = (chest_bounds["xmin"] + chest_bounds["xmax"]) * 0.5
    # On the pectoral plane, about the middle of the upper chest; avoid shoulder/arm area.
    z = chest_bounds["zmin"] + chest_height * 0.48
    side_offset = chest_width * (0.185 if gender == "male" else 0.19)
    # ~2 cm areola diameter and ~8 mm nipple diameter for a 1.7–1.9 m body.
    areola_r = max(0.0085, min(0.0115, chest_width * 0.026))
    nipple_r = areola_r * 0.39
    for side, sign in (("L", -1.0), ("R", 1.0)):
        x = cx + sign * side_offset
        if not make_conforming_round_patch("Areola_" + side, x, z, areola_r,
                tree, chest_bounds, front_sign, mats["areola"], armature, collection,
                chest.name, lift=0.00022, segments=16):
            log("Areola {} skipped: chest surface ray missed; no floating patch created.".format(side))
            continue
        if not make_conforming_round_patch("Nipple_" + side, x, z, nipple_r,
                tree, chest_bounds, front_sign, mats["nipple"], armature, collection,
                chest.name, lift=0.00048, segments=14):
            log("Nipple {} skipped: chest surface ray missed.".format(side))

def add_hair(head_bounds, head_points, front_sign, armature, collection, mats, gender,
             style="default", head_tree=None):
    """Four distinct, head-conforming low-poly hairstyles per gender.
    The cap follows measured head cross-sections; fringe/side locks begin on ray-hit skin points.
    All geometry is weighted to the existing Head bone. No new bones are added.
    """
    head_bone = find_bone(armature, "Head")
    if head_bone is None:
        log("Hair skipped: no existing Head bone.")
        return
    bone = head_bone.name
    xmid = (head_bounds["xmin"] + head_bounds["xmax"]) * 0.5
    ymid = (head_bounds["ymin"] + head_bounds["ymax"]) * 0.5
    zlow, zhigh = head_bounds["zmin"], head_bounds["zmax"]
    width = head_bounds["xmax"] - head_bounds["xmin"]
    depth = head_bounds["ymax"] - head_bounds["ymin"]
    height = zhigh - zlow
    if min(width, depth, height) <= 0:
        raise RuntimeError("Head bounds invalidi: capelli non generabili.")
    if head_tree is None:
        # Safer than inventing coordinates: caller should provide the head-only BVH.
        log("Hair skipped: missing head-only surface BVH (prevents floating hair).")
        return

    # Scalp cap: cross-sections measured from the real head mesh, with only a tiny outward offset.
    def measured_radii(zfrac, fallback_x, fallback_y):
        z = zlow + height * zfrac
        band = [p for p in head_points if abs(p.z - z) <= height * 0.075]
        if len(band) >= 6:
            rx = max(width * 0.12, (max(p.x for p in band) - min(p.x for p in band)) * 0.5 * 1.018)
            ry = max(depth * 0.12, (max(p.y for p in band) - min(p.y for p in band)) * 0.5 * 1.018)
            return rx, ry
        return width * fallback_x, depth * fallback_y

    rings = [(0.605, 0.42, 0.43), (0.69, 0.47, 0.48),
             (0.79, 0.48, 0.48), (0.89, 0.40, 0.40), (0.955, 0.25, 0.24)]
    sides = 18
    verts, faces = [], []
    for ring_idx, (zfrac, fx, fy) in enumerate(rings):
        rx, ry = measured_radii(zfrac, fx, fy)
        for i in range(sides):
            ang = 2.0 * math.pi * i / sides
            sin_a, cos_a = math.sin(ang), math.cos(ang)
            front_weight = max(0.0, -sin_a * front_sign)
            back_weight = max(0.0, sin_a * front_sign)
            z = zlow + height * zfrac + height * (0.012 * front_weight - 0.012 * back_weight)
            x = xmid + cos_a * rx * 1.012
            y = ymid + front_sign * sin_a * ry * 1.012
            verts.append((x, y, z))
    top_idx = len(verts)
    verts.append((xmid, ymid, zhigh + height * 0.008))
    for ring in range(len(rings) - 1):
        for i in range(sides):
            a = ring * sides + i
            b = ring * sides + (i + 1) % sides
            c = (ring + 1) * sides + (i + 1) % sides
            d = (ring + 1) * sides + i
            faces.append((a, b, c, d))
    last = (len(rings) - 1) * sides
    for i in range(sides):
        faces.append((last + i, last + (i + 1) % sides, top_idx))
    # Closed lower edge is embedded just inside the scalp line.
    inner = len(verts)
    for i in range(sides):
        ang = 2.0 * math.pi * i / sides
        verts.append((xmid + math.cos(ang) * width * 0.34,
                      ymid + front_sign * math.sin(ang) * depth * 0.34,
                      zlow + height * 0.61))
    for i in range(sides):
        faces.append((i, inner + i, inner + (i + 1) % sides, (i + 1) % sides))
    faces.append(tuple(reversed(range(inner, inner + sides))))
    attach_weighted_mesh("Hair_ScalpCap", verts, faces, mats["hair"], armature, collection, bone)

    def add_back_hair_shell():
        """Drape a polygonal hair shell onto the actual rear scalp from crown to nape.

        The grid is ray-projected against the head-only BVH; each point follows the actual
        rear silhouette. This explicitly covers the bald rear area while leaving the face open.
        """
        # (height fraction, half-width as a fraction of total head width).
        rows = [(0.16, 0.18), (0.22, 0.27), (0.29, 0.35), (0.36, 0.41),
                (0.43, 0.45), (0.50, 0.47), (0.57, 0.46), (0.63, 0.43),
                (0.68, 0.39), (0.72, 0.33)]
        columns = 19
        grid = []
        verts_back = []
        faces_back = []
        for zfrac, halfwidth_frac in rows:
            row = []
            z = zlow + height * zfrac
            for col in range(columns):
                xfrac = -halfwidth_frac + 2.0 * halfwidth_frac * col / float(columns - 1)
                x = xmid + width * xfrac
                hit, normal = strict_back_hit(head_tree, x, z, head_bounds, front_sign)
                if hit is None:
                    row.append(None)
                    continue
                point = hit + normal * 0.0018
                row.append(len(verts_back))
                verts_back.append(tuple(point))
            grid.append(row)

        desired_back_y = -front_sign
        for r in range(len(grid) - 1):
            for c in range(columns - 1):
                cell = [grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]]
                valid = [idx for idx in cell if idx is not None]
                if len(valid) < 3:
                    continue
                # Grid order BL -> BR -> TR -> TL has a -Y normal. Reverse when rear is +Y.
                if desired_back_y > 0:
                    valid = list(reversed(valid))
                faces_back.append(tuple(valid))
        if verts_back and faces_back:
            attach_weighted_mesh("Hair_BackShell", verts_back, faces_back, mats["hair"],
                                 armature, collection, bone)
            log("Rear hair shell created: conforms to scalp and reaches the nape.")
        else:
            log("Rear hair shell skipped: no valid rear head samples.")

    add_back_hair_shell()

    def front_strand(name, path, thickness=0.052, offset=0.0035):
        # path points are (x fraction of head width, z fraction of head height).
        centers = []
        for xf, zf in path:
            x = xmid + width * xf
            z = zlow + height * zf
            hit, normal = strict_surface_hit(head_tree, x, z, head_bounds, front_sign)
            if hit is None:
                # Retry just inside the expected forehead surface; never use an arbitrary fallback position.
                for dzf in (-0.018, 0.018, -0.035, 0.035):
                    hit, normal = strict_surface_hit(head_tree, x, zlow + height * (zf + dzf), head_bounds, front_sign)
                    if hit is not None:
                        break
            if hit is None:
                return False
            centers.append(tuple(hit + normal * offset))
        rx = max(width * thickness, 0.004)
        ry = max(depth * 0.060, 0.003)
        radii = [(rx * 0.78, ry * 0.70), (rx, ry), (rx * 0.78, ry * 0.70)]
        make_hair_lock(name, centers, radii, mats["hair"], armature, collection, bone, sides=7)
        return True

    def side_lock(name, side_sign, path, thickness=0.060):
        # path points are (Y fraction from front toward back, Z fraction of head height).
        centers = []
        for yf, zf in path:
            y = ymid + front_sign * depth * yf
            z = zlow + height * zf
            hit, normal = strict_side_hit(head_tree, xmid, width, y, z, side_sign)
            if hit is None:
                return False
            centers.append(tuple(hit + normal * 0.0035))
        rx = max(width * thickness, 0.005)
        ry = max(depth * 0.060, 0.0035)
        radii = [(rx * 0.75, ry * 0.9), (rx, ry), (rx * 0.55, ry * 0.7)]
        make_hair_lock(name, centers, radii, mats["hair"], armature, collection, bone, sides=8)
        return True

    def back_lock(name, path, thickness=0.075):
        # path points are (X fraction, Z fraction); each control point is sampled on the rear scalp.
        centers = []
        for xf, zf in path:
            x = xmid + width * xf
            z = zlow + height * zf
            hit, normal = strict_back_hit(head_tree, x, z, head_bounds, front_sign)
            if hit is None:
                return False
            centers.append(tuple(hit + normal * 0.004))
        rx = max(width * thickness, 0.005)
        ry = max(depth * 0.075, 0.004)
        # One radius pair for every path sample; taper gradually toward the tip.
        count = len(centers)
        radii = []
        for i in range(count):
            t = i / max(1, count - 1)
            taper = 1.0 - 0.40 * t
            radii.append((rx * taper, ry * (1.0 - 0.25 * t)))
        make_hair_lock(name, centers, radii, mats["hair"], armature, collection, bone, sides=8)
        return True

    def add_small_spikes():
        # Short, rooted crown tufts: start inside the cap and rise only a little.
        for i, xf in enumerate((-0.28, -0.10, 0.12, 0.30)):
            root = (xmid + width * xf, ymid + front_sign * depth * 0.035, zhigh - height * 0.10)
            bend = (root[0] + width * 0.025, root[1] + front_sign * depth * 0.015, zhigh + height * 0.015)
            tip = (root[0] + width * 0.055, root[1], zhigh + height * (0.035 if i % 2 else 0.055))
            radii = [(width * 0.055, depth * 0.055), (width * 0.038, depth * 0.040), (width * 0.004, depth * 0.005)]
            make_hair_lock("Hair_CrownTuft_{:02d}".format(i), [root, bend, tip], radii,
                           mats["hair"], armature, collection, bone, sides=7)

    if gender == "male":
        if style == "short_spiky":
            add_small_spikes()
            for i, xf in enumerate((-0.24, -0.08, 0.08, 0.22)):
                front_strand("Hair_ShortFringe_{:02d}".format(i),
                             [(xf - 0.035, 0.81), (xf + 0.005, 0.75), (xf + 0.018, 0.69)], 0.040)
            side_lock("Hair_ShortSide_L", -1, [(0.12, 0.76), (0.13, 0.61), (0.10, 0.48)], 0.045)
            side_lock("Hair_ShortSide_R", 1, [(0.12, 0.76), (0.13, 0.61), (0.10, 0.48)], 0.045)
        elif style == "side_swept":
            for i, path in enumerate((
                [(-0.32,0.82),(-0.12,0.78),(0.10,0.70),(0.23,0.68)],
                [(-0.24,0.87),(-0.02,0.82),(0.18,0.75),(0.28,0.72)],
                [(-0.08,0.88),(0.12,0.83),(0.27,0.78)],)):
                front_strand("Hair_SideSweep_{:02d}".format(i), path, 0.062)
            side_lock("Hair_LongSide_L", -1, [(0.20,0.80),(0.13,0.62),(0.10,0.40)], 0.065)
            side_lock("Hair_ShortSide_R", 1, [(0.18,0.78),(0.13,0.60),(0.10,0.50)], 0.045)
        elif style == "medium_swept":
            front_strand("Hair_MediumPart_L", [(-0.28,0.84),(-0.19,0.78),(-0.08,0.68)], 0.055)
            front_strand("Hair_MediumPart_R", [(0.00,0.88),(0.12,0.80),(0.23,0.69)], 0.060)
            side_lock("Hair_MediumSide_L", -1, [(0.21,0.82),(0.12,0.61),(0.09,0.25)], 0.075)
            side_lock("Hair_MediumSide_R", 1, [(0.18,0.81),(0.09,0.61),(0.06,0.31)], 0.070)
            back_lock("Hair_MediumBack", [(-0.16,0.72),(-0.12,0.52),(0.02,0.34)], 0.060)
        elif style == "tied_back":
            front_strand("Hair_TiedPart_L", [(-0.30,0.84),(-0.21,0.79),(-0.15,0.73)], 0.045)
            front_strand("Hair_TiedPart_R", [(0.04,0.86),(0.13,0.81),(0.19,0.75)], 0.045)
            side_lock("Hair_TiedSide_L", -1, [(0.22,0.82),(0.12,0.65),(0.10,0.44)], 0.055)
            side_lock("Hair_TiedSide_R", 1, [(0.20,0.82),(0.12,0.65),(0.10,0.44)], 0.055)
            back_lock("Hair_Ponytail", [(0.02,0.76),(0.10,0.61),(0.14,0.43),(0.16,0.23),(0.11,0.08)], 0.075)
        else:
            raise ValueError("Stile capelli maschile non riconosciuto: " + style)
    else:
        if style == "ponytail":
            front_strand("Hair_Pony_Bang_L", [(-0.29,0.84),(-0.20,0.77),(-0.12,0.67)], 0.052)
            front_strand("Hair_Pony_Bang_R", [(0.03,0.88),(0.10,0.81),(0.18,0.72)], 0.055)
            side_lock("Hair_PonySide_L", -1, [(0.22,0.82),(0.13,0.61),(0.08,0.29)], 0.060)
            side_lock("Hair_PonySide_R", 1, [(0.20,0.82),(0.13,0.61),(0.08,0.31)], 0.060)
            back_lock("Hair_HighPonytail", [(0.00,0.78),(0.07,0.63),(0.10,0.44),(0.12,0.24),(0.08,0.08)], 0.070)
        elif style == "bob":
            # Distinct blunt fringe and jaw-length side panels; no tied-back tail.
            for i, xf in enumerate((-0.25, -0.12, 0.02, 0.16, 0.27)):
                front_strand("Hair_BobFringe_{:02d}".format(i),
                             [(xf,0.83),(xf + 0.01,0.76),(xf + 0.012,0.70)], 0.050)
            side_lock("Hair_BobPanel_L", -1, [(0.26,0.82),(0.16,0.58),(0.12,0.19)], 0.085)
            side_lock("Hair_BobPanel_R", 1, [(0.25,0.82),(0.15,0.58),(0.12,0.19)], 0.085)
            back_lock("Hair_BobBack", [(-0.18,0.67),(0.00,0.57),(0.18,0.67)], 0.075)
        elif style == "long_flow":
            front_strand("Hair_LongCurtain_L", [(-0.24,0.87),(-0.20,0.78),(-0.12,0.65)], 0.048)
            front_strand("Hair_LongCurtain_R", [(0.22,0.87),(0.18,0.78),(0.11,0.65)], 0.048)
            side_lock("Hair_LongFlow_L", -1, [(0.22,0.82),(0.10,0.58),(0.08,0.14)], 0.085)
            side_lock("Hair_LongFlow_R", 1, [(0.21,0.82),(0.10,0.58),(0.08,0.14)], 0.085)
            back_lock("Hair_LongBack_L", [(-0.24,0.65),(-0.25,0.43),(-0.22,0.20),( -0.18,0.04)], 0.070)
            back_lock("Hair_LongBack_R", [(0.22,0.65),(0.25,0.43),(0.21,0.20),(0.18,0.04)], 0.070)
        elif style == "braid":
            front_strand("Hair_BraidSweep_L", [(-0.29,0.85),(-0.15,0.79),(0.03,0.70)], 0.055)
            front_strand("Hair_BraidSweep_R", [(0.04,0.88),(0.16,0.80),(0.24,0.74)], 0.050)
            side_lock("Hair_BraidSide_L", -1, [(0.23,0.82),(0.13,0.63),(0.11,0.42)], 0.055)
            # Thick plait follows the rear of the head into the neck; alternating X gives a braided silhouette.
            back_lock("Hair_Braid_Main", [(-0.27,0.70),(-0.31,0.58),(-0.25,0.46),(-0.31,0.34),(-0.25,0.22),(-0.27,0.12)], 0.055)
            back_lock("Hair_Braid_WeaveA", [(-0.29,0.67),(-0.23,0.55),(-0.30,0.43),(-0.24,0.31),(-0.29,0.19)], 0.026)
            back_lock("Hair_Braid_WeaveB", [(-0.25,0.63),(-0.31,0.51),(-0.24,0.39),(-0.30,0.27),(-0.24,0.15)], 0.026)
        else:
            raise ValueError("Stile capelli femminile non riconosciuto: " + style)

def animation_label(filename):
    stem = Path(filename).stem.lower().replace(" ", "_").replace("-", "_")
    if "briefcase" in stem or "suitcase" in stem:
        return None
    if "idle" in stem:
        return "Idle"
    if "walk" in stem:
        return "Walk"
    if "run" in stem or "sprint" in stem:
        return "Run"
    if "hit" in stem or "hurt" in stem or "damage" in stem:
        return "Hurt"
    if "bow" in stem or "archery" in stem:
        return "Bow"
    if "attack2" in stem or "two_hand" in stem or "twohand" in stem or "2h" in stem:
        return "Attack2H"
    if "attack1" in stem or "one_hand" in stem or "onehand" in stem or "1h" in stem or "slash" in stem:
        return "Attack1H"
    if "attack" in stem or "sword" in stem:
        return "Attack"
    return None


def remove_objects(objects):
    for obj in objects:
        if obj.name in bpy.data.objects:
            bpy.data.objects.remove(obj, do_unlink=True)


def import_animation_library(directory):
    """Load one copy of each recognized Mixamo action. Clips are reused by both bases."""
    library = {}
    if not IMPORT_AVAILABLE_ANIMATIONS or not directory.is_dir():
        log("Cartella animazioni non trovata o import disabilitato; i GLB saranno solo riggati.")
        return library
    paths = sorted(directory.glob("*.fbx"))
    for path in paths:
        label = animation_label(path.name)
        if label is None:
            log("Animazione ignorata (nome non idoneo o prop): {}".format(path.name))
            continue
        if label in library:
            log("Clip duplicata per {} ignorata: {}".format(label, path.name))
            continue
        before_objects = set(bpy.data.objects)
        before_actions = set(bpy.data.actions)
        try:
            bpy.ops.import_scene.fbx(filepath=str(path), use_anim=True, automatic_bone_orientation=False)
            imported = [obj for obj in bpy.data.objects if obj not in before_objects]
            armatures = [obj for obj in imported if obj.type == "ARMATURE"]
            action = None
            for arm in armatures:
                if arm.animation_data and arm.animation_data.action:
                    action = arm.animation_data.action
                    break
            if action is None:
                new_actions = [a for a in bpy.data.actions if a not in before_actions and len(a.fcurves) > 0]
                if new_actions:
                    action = new_actions[-1]
            if action is None:
                log("Nessuna Action leggibile nel file: {}".format(path.name))
                remove_objects(imported)
                continue
            copied = action.copy()
            copied.name = "_MixamoLibrary_" + label
            library[label] = copied
            log("Animazione caricata: {} <- {} (frame {}-{})".format(label, path.name,
                int(copied.frame_range[0]), int(copied.frame_range[1])))
            remove_objects(imported)
            # Clean up source action datablocks from the imported FBX, keeping our copy.
            for action_data in list(bpy.data.actions):
                if action_data not in before_actions and action_data != copied and action_data.users == 0:
                    bpy.data.actions.remove(action_data)
        except Exception as exc:
            log("Impossibile importare {}: {}".format(path.name, exc))
            remove_objects([obj for obj in bpy.data.objects if obj not in before_objects])
    return library


def remap_action_bone_paths(action, armature):
    """Ensure Action bone paths match the target Mixamo rig; skip if the hierarchy differs."""
    target_names = {b.name for b in armature.pose.bones}
    all_ok = True
    for fcurve in action.fcurves:
        match = re.search(r'pose\.bones\["([^"]+)"\]', fcurve.data_path)
        if not match:
            continue
        source_name = match.group(1)
        if source_name in target_names:
            continue
        suffix = source_name.split(":")[-1].split("_")[-1]
        candidate = None
        for name in target_names:
            if name == suffix or name.endswith(":" + suffix) or name.endswith("_" + suffix):
                candidate = name
                break
        if candidate is None:
            all_ok = False
        else:
            fcurve.data_path = fcurve.data_path.replace('["' + source_name + '"]', '["' + candidate + '"]')
    if not all_ok:
        # Caller checks for unresolved paths and omits the clip rather than exporting a broken one.
        for fcurve in action.fcurves:
            match = re.search(r'pose\.bones\["([^"]+)"\]', fcurve.data_path)
            if match and match.group(1) not in target_names:
                return False
    return True


def attach_animation_library(armature, library):
    if not library:
        return []
    ad = armature.animation_data_create()
    ad.action = None
    added = []
    for label, source_action in library.items():
        action = source_action.copy()
        action.name = armature.name + "_" + label
        if not remap_action_bone_paths(action, armature):
            log("Clip {} ignorata: i nomi delle ossa non coincidono con {}.".format(label, armature.name))
            bpy.data.actions.remove(action)
            continue
        track = ad.nla_tracks.new()
        track.name = label
        start = int(action.frame_range[0])
        strip = track.strips.new(label, start, action)
        strip.name = label
        strip.blend_type = "REPLACE"
        added.append(label)
    ad.action = None
    if added:
        log("{} riceve le clip: {}".format(armature.name, ", ".join(added)))
    return added


def build_one(gender, hair_style, source_file, collection_name, skin_color, hair_color, iris_color, animation_library):
    collection, imported, armature, meshes = import_rigged_fbx(source_file, collection_name)
    log("Importato {}: {} mesh, rig {}".format(gender, len(meshes), armature.name))
    # Preserve original mesh coordinates, vertex groups, armature and rest pose.
    # Previous global-space reshape distorted hands/proportions, so it is deliberately not applied.
    recolor_body(meshes, collection_name, skin_color)
    head_bone_name, head_pts, head_bounds = get_head_bounds(meshes, armature)
    body_bounds = bounds_of_points(world_vertices(meshes))
    front_sign = detect_front_sign(head_pts, head_bounds)
    log("{}: altezza {:.3f}, testa {:.3f} x {:.3f} x {:.3f}, fronte Y={}".format(
        gender, body_bounds["zmax"]-body_bounds["zmin"],
        head_bounds["xmax"]-head_bounds["xmin"], head_bounds["ymax"]-head_bounds["ymin"],
        head_bounds["zmax"]-head_bounds["zmin"], front_sign))
    mats = {
        "hair": make_material(collection_name + "_Hair", hair_color, roughness=0.78),
        "brow": make_material(collection_name + "_Brows", tuple(hair_color[:3]) + (1.0,), roughness=0.84),
        "eye_rim": make_material(collection_name + "_EyeRim", (0.34, 0.22, 0.16, 1.0), roughness=0.85),  # retained for compatibility; not drawn
        "sclera": make_material(collection_name + "_Sclera", (1.0, 0.995, 0.965, 1.0), roughness=0.48),
        "iris": make_material(collection_name + "_Iris", iris_color, roughness=0.42),
        "pupil": make_material(collection_name + "_Pupil", (0.018, 0.013, 0.012, 1.0), roughness=0.28),
        "glint": make_material(collection_name + "_Glint", (0.98, 0.94, 0.82, 1.0), roughness=0.22),
        # Delicate skin-adjacent tones; visible detail without oversized/high-contrast marks.
        "areola": make_material(collection_name + "_Areola", (skin_color[0]*0.88, skin_color[1]*0.73, skin_color[2]*0.72, 1.0), roughness=0.82),
        "nipple": make_material(collection_name + "_Nipple", (skin_color[0]*0.82, skin_color[1]*0.63, skin_color[2]*0.62, 1.0), roughness=0.82),
    }
    full_tree = make_bvh(meshes)
    try:
        head_tree = make_bvh_for_bones(meshes, [head_bone_name])
    except Exception as exc:
        log("Head-only BVH unavailable ({}); use full-body BVH for face surface sampling.".format(exc))
        head_tree = full_tree
    torso_bones = [b.name for b in armature.data.bones
                   if canonical_bone_name(b.name) in ("hips", "pelvis", "spine", "spine1", "spine2", "chest", "upperchest")]
    torso_points = []
    for torso_bone_name in torso_bones:
        torso_points.extend(points_weighted_to_bone(meshes, torso_bone_name))
    if len(torso_points) < 20:
        torso_points = [p for p in world_vertices(meshes)
                        if body_bounds["zmin"] + (body_bounds["zmax"]-body_bounds["zmin"])*0.52 < p.z <
                           body_bounds["zmin"] + (body_bounds["zmax"]-body_bounds["zmin"])*0.83]
    torso_bounds = bounds_of_points(torso_points)
    try:
        torso_tree = make_bvh_for_bones(meshes, torso_bones)
    except Exception as exc:
        log("Torso-only BVH unavailable ({}); using body BVH with strict chest coordinates.".format(exc))
        torso_tree = full_tree

    # Use the upper-chest bone alone for anatomy placement; using the entire torso put details
    # too low/out toward the sides on some Mixamo proportions.
    chest_bone = None
    for candidate in ("Chest", "Spine2", "UpperChest", "Spine1", "Spine"):
        chest_bone = find_bone(armature, candidate)
        if chest_bone is not None:
            break
    chest_bounds = torso_bounds
    chest_tree = torso_tree
    if chest_bone is not None:
        chest_pts = points_weighted_to_bone(meshes, chest_bone.name)
        if len(chest_pts) >= 20:
            chest_bounds = bounds_of_points(chest_pts)
        try:
            chest_tree = make_bvh_for_bones(meshes, [chest_bone.name])
        except Exception as exc:
            log("Chest-only BVH unavailable ({}); using torso BVH.".format(exc))
            chest_tree = torso_tree

    add_eyes(head_bounds, head_bounds, front_sign, head_tree, armature, collection, mats, gender)
    add_nipples(chest_bounds, body_bounds, front_sign, chest_tree, armature, collection, mats, gender,
                chest_bone.name if chest_bone else None)
    add_hair(head_bounds, head_pts, front_sign, armature, collection, mats, gender, hair_style, head_tree)
    animations_added = attach_animation_library(armature, animation_library)
    # Rest pose is neutral when the Blender review scene opens.
    armature.data.pose_position = "POSE" if animations_added else "REST"
    if armature.animation_data:
        armature.animation_data.action = None
    bpy.context.view_layer.update()
    return collection, armature, animations_added


def export_collection(collection, armature, output_file, expect_animations):
    output_file.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in collection.objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    kwargs = dict(filepath=str(output_file), export_format="GLB", use_selection=True,
                  export_skins=True, export_yup=True, export_animations=bool(expect_animations))
    if expect_animations:
        kwargs["export_nla_strips"] = True
    try:
        bpy.ops.export_scene.gltf(**kwargs)
    except TypeError as exc:
        log("Opzioni exporter Blender non riconosciute ({}); riprovo con opzioni di base.".format(exc))
        basic = dict(filepath=str(output_file), export_format="GLB", use_selection=True,
                     export_animations=bool(expect_animations))
        try:
            bpy.ops.export_scene.gltf(**basic)
        except Exception as fallback_exc:
            log("Esportazione con animazioni fallita: {}; riprovo GLB riggato senza animazioni.".format(fallback_exc))
            bpy.ops.export_scene.gltf(
                filepath=str(output_file), export_format="GLB", use_selection=True, export_animations=False)
    if not output_file.exists() or output_file.stat().st_size < 10000:
        raise RuntimeError("Esportazione GLB mancante o troppo piccola: {}".format(output_file))
    log("GLB creato: {} ({} byte)".format(output_file, output_file.stat().st_size))


def read_glb_json(path):
    """Read the JSON chunk from a GLB so material names can be mapped for runtime recoloring."""
    raw = path.read_bytes()
    if len(raw) < 20 or raw[:4] != b"glTF":
        return {}
    chunk_len = struct.unpack_from("<I", raw, 12)[0]
    chunk_type = struct.unpack_from("<I", raw, 16)[0]
    if chunk_type != 0x4E4F534A:
        return {}
    return json.loads(raw[20:20 + chunk_len].decode("utf-8").rstrip(" \t\r\n\0"))


def material_role_indices(glb_path):
    doc = read_glb_json(glb_path)
    out = {}
    role_tokens = {
        "skin": ["_skin"], "hair": ["_hair"], "brows": ["_brows"],
        "eye_rim": ["_eyerim"], "sclera": ["_sclera"], "iris": ["_iris"],
        "pupil": ["_pupil"], "glint": ["_glint"],
    }
    for i, mat in enumerate(doc.get("materials", [])):
        name = mat.get("name", "").lower()
        for role, tokens in role_tokens.items():
            if role not in out and any(token in name for token in tokens):
                out[role] = i
    return out


def main():
    if bpy.app.version < (3, 2, 0):
        raise RuntimeError("Serve Blender 3.2.0 o superiore.")
    missing = [str(p) for p in (MALE_FBX, FEMALE_FBX) if not p.is_file()]
    if missing:
        raise FileNotFoundError(
            "Non trovo i FBX sorgente usati SOLO da Blender/Mixamo:\n{}\n\n"
            "Estrai tools.zip in Isometric3DRPG così esistono tools\\mixamo\\male\\base_male.fbx e "
            "tools\\mixamo\\female\\base_female.fbx.\nProgetto rilevato: {}".format(
                "\n".join(missing), PROJECT_ROOT))
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    log("Blender {}".format(bpy.app.version_string))
    log("Input FBX (intermedi), output finale GLB: {}".format(OUTPUT_DIR))
    animation_library = import_animation_library(ANIMATION_DIR)
    if "Walk" not in animation_library:
        log("ATTENZIONE: nessuna Walking pulita trovata; walking_briefcase.fbx viene ignorata. Scarica una Walking senza prop per includerla.")
    if not animation_library:
        log("ATTENZIONE: nessuna animazione valida trovata; i GLB saranno riggati ma senza clip.")

    specs = [
        ("male", MALE_FBX, SKIN_MALE, HAIR_MALE, IRIS_MALE),
        ("female", FEMALE_FBX, SKIN_FEMALE, HAIR_FEMALE, IRIS_FEMALE),
    ]
    manifest = {
        "format": "glb",
        "source_format": "fbx_intermediate_only",
        "notes": [
            "Il gioco deve caricare i GLB, mai i FBX sorgente.",
            "La geometria capelli è scelta caricando il GLB della variante.",
            "Gli indici materiali sono per GLB e permettono di ricolorare pelle/capelli/iride in raylib.",
            "La revisione preserva la geometria del corpo originale e la gerarchia delle ossa; capelli, occhi e dettagli del torace usano pesi verso ossa esistenti.",
            "La V5 proietta una shell di capelli sulla parte posteriore della testa, aumenta leggermente la leggibilita degli occhi e posiziona i dettagli del torace rispetto all osso superiore del torso.",
            "Le animazioni importate dipendono dai file presenti nella cartella animations; walking_briefcase viene ignorata."
        ],
        "hair_styles": {
            "male": [{"id": s, "label": HAIR_LABELS[s], "file": "human_male_{}.glb".format(s)} for s in HAIR_STYLES["male"]],
            "female": [{"id": s, "label": HAIR_LABELS[s], "file": "human_female_{}.glb".format(s)} for s in HAIR_STYLES["female"]],
        },
        "palettes": {
            "skin": {
                "porcelain": [0.86, 0.70, 0.60, 1.0], "fair_warm": [0.77, 0.515, 0.365, 1.0],
                "warm_medium": [0.57, 0.335, 0.225, 1.0], "olive": [0.47, 0.31, 0.20, 1.0],
                "deep_brown": [0.30, 0.18, 0.13, 1.0], "dark": [0.19, 0.105, 0.075, 1.0]
            },
            "hair": {
                "black": [0.025, 0.018, 0.016, 1.0], "dark_brown": [0.10, 0.040, 0.020, 1.0],
                "chestnut": [0.30, 0.085, 0.030, 1.0], "blonde": [0.72, 0.46, 0.13, 1.0],
                "auburn": [0.48, 0.075, 0.025, 1.0], "silver": [0.48, 0.50, 0.55, 1.0],
                "platinum": [0.84, 0.77, 0.61, 1.0]
            },
            "eyes": {
                "blue": [0.10, 0.32, 0.82, 1.0], "green": [0.10, 0.42, 0.22, 1.0],
                "brown": [0.25, 0.105, 0.035, 1.0], "hazel": [0.45, 0.28, 0.08, 1.0],
                "gray": [0.34, 0.41, 0.48, 1.0], "violet": [0.40, 0.16, 0.68, 1.0]
            }
        },
        "assets": {}
    }
    built_count = 0
    for gender, src_path, skin_color, hair_color, iris_color in specs:
        for hair_style in HAIR_STYLES[gender]:
            coll_name = "Human_{}_{}".format(gender.title(), hair_style.title())
            collection, armature, clips = build_one(gender, hair_style, src_path, coll_name,
                                                    skin_color, hair_color, iris_color, animation_library)
            filename = "human_{}_{}.glb".format(gender, hair_style)
            glb = OUTPUT_DIR / filename
            if EXPORT_GLB:
                export_collection(collection, armature, glb, bool(clips))
            manifest["assets"][filename] = {
                "gender": gender,
                "hair_style": hair_style,
                "animation_clips": clips,
                "material_indices": material_role_indices(glb) if EXPORT_GLB and glb.exists() else {},
            }
            armature.location.x = built_count * 1.15
            built_count += 1
            bpy.context.view_layer.update()

    manifest_path = OUTPUT_DIR / "character_customization.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    log("Manifest di personalizzazione creato: {}".format(manifest_path))
    if SAVE_REVIEW_BLEND:
        blend = OUTPUT_DIR / "human_gender_bases_review.blend"
        bpy.ops.wm.save_as_mainfile(filepath=str(blend))
        log("Scena di revisione salvata: {}".format(blend))
    log("Completato: {} varianti GLB. Scheletri sorgente preservati; FBX solo come sorgenti di lavoro.".format(built_count))
    log("NOTA: walking_briefcase.fbx è ignorata; per la clip Walk serve un file Walking.fbx senza prop.")


try:
    main()
except Exception:
    log("ERRORE — traceback completo:")
    traceback.print_exc()
    raise
