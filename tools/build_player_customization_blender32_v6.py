"""
PLAYER GENDER BASES - Blender 3.2.0+ (v6: eyes in the eye pockets, flat chest colour, separate faceted hair layers)
=================================================================================================================
Builds the two generic adult human bases (male / female) from the Mixamo-rigged FBX sources and a set of
HAIR LAYERS that fit the head surface exactly. Skin, hair and eye colours are chosen in the game.

Output (assets/models/characters/bases):
  human_male_base.glb,  human_female_base.glb          nude body + eyes (with lashes) + brows + flat areola/nipple colour
  hair_male_<style>.glb, hair_female_<style>.glb       one faceted hair layer each, same skeleton as the body
  character_customization.json                         roles, palettes, hair list (with files)
  human_gender_bases_review.blend                      review scene (body + hair of every style)

Hair is a separate file on purpose: with a helmet / hood the game simply does not draw the hair model.
Expected source files:  tools/mixamo/male/base_male.fbx, tools/mixamo/female/base_female.fbx,
                        tools/mixamo/animations/*.fbx (optional). Original FBX files are never modified.
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

# Each hairstyle is its own GLB layer (same skeleton as the body); "bald" needs no file.
HAIR_STYLES = {
    "male": ["crop", "side_part", "medium", "tied_back"],
    "female": ["ponytail", "bob", "long", "braid"],
}
HAIR_LABELS = {
    "crop": "Crop", "side_part": "Side part", "medium": "Medium", "tied_back": "Tied back",
    "ponytail": "Ponytail", "bob": "Bob", "long": "Long", "braid": "Braid",
}
# areola / nipple = skin colour * these (flat colour, no geometry); the game applies the same factors
AREOLA_MUL = (0.88, 0.73, 0.72)
NIPPLE_MUL = (0.82, 0.63, 0.62)
# eyebrows = hair colour * this (darker, natural); the game applies the same factor
BROW_MUL = (0.62, 0.60, 0.58)
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


# ======================================================================================
# V6: eyes sit INSIDE the mesh's own eye pockets, soft brows, flat chest colour (no 3D)
# ======================================================================================
from mathutils.kdtree import KDTree


def clamp01(x):
    return max(0.0, min(1.0, x))


def lerp(a, b, t):
    return a + (b - a) * t


def attach_mesh_ex(name, world_verts, faces, mats, face_mats, armature, collection,
                   weight_fn, smooth=False, recalc=False):
    """Weighted mesh with several material slots and per-vertex weights.

    weight_fn(Vector world_position) -> {bone_name: weight}. Normals are NOT auto-recalculated unless
    recalc=True (open surfaces such as scalp shells would be flipped by the heuristic).
    """
    arm_inv = armature.matrix_world.inverted()
    local_verts = [tuple(arm_inv @ Vector(v)) for v in world_verts]
    mesh = bpy.data.meshes.new(name + "_Mesh")
    mesh.from_pydata(local_verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    for m in mats:
        mesh.materials.append(m)
    for poly, mi in zip(mesh.polygons, face_mats):
        poly.material_index = mi
        poly.use_smooth = smooth
    groups = {}
    for vi, v in enumerate(world_verts):
        weights = weight_fn(Vector(v))
        total = sum(weights.values()) or 1.0
        for bone, w in weights.items():
            if w <= 1e-4:
                continue
            if bone not in groups:
                groups[bone] = obj.vertex_groups.new(name=bone)
            groups[bone].add([vi], w / total, "REPLACE")
    obj.parent = armature
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.matrix_world = armature.matrix_world.copy()
    modifier = obj.modifiers.new("HeadDeform", "ARMATURE")
    modifier.object = armature
    if recalc and len(mesh.polygons) > 4:
        bm = bmesh.new()
        bm.from_mesh(mesh)
        try:
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            bm.to_mesh(mesh)
            mesh.update()
        finally:
            bm.free()
    return obj


def fix_winding_outward(verts, faces, outward_fn):
    """Flip every face whose normal points against outward_fn(face_center)."""
    out = []
    for f in faces:
        pts = [Vector(verts[i]) for i in f]
        c = sum(pts, Vector()) / len(pts)
        n = (pts[1] - pts[0]).cross(pts[2] - pts[0])
        if n.dot(outward_fn(c)) < 0:
            f = tuple(reversed(f))
        out.append(tuple(f))
    return out


# measured on tools/mixamo/{male,female}/base_*.fbx : centre of the eye aperture (fractions of head box)
EYE_PRESET = {
    "male":   {"x": 0.2011, "z": 0.5554},
    "female": {"x": 0.2054, "z": 0.5252},
}


def head_polygons(meshes, head_bone_name, min_weight=0.3):
    """World-space polygons of the faces mostly weighted to the head bone."""
    target = canonical_bone_name(head_bone_name)
    out = []
    for obj in meshes:
        idx = {g.index for g in obj.vertex_groups if canonical_bone_name(g.name) == target}
        if not idx:
            continue
        ok = []
        for v in obj.data.vertices:
            ok.append(any(m.group in idx and m.weight >= min_weight for m in v.groups))
        mw = obj.matrix_world
        for poly in obj.data.polygons:
            if sum(1 for i in poly.vertices if ok[i]) >= len(poly.vertices) - 1:
                out.append([mw @ obj.data.vertices[i].co for i in poly.vertices])
    return out


def locate_eye_apertures(meshes, head_bone_name, head_bounds, front_sign, gender):
    """Find both eye apertures from the mesh itself.

    The base meshes have, per eye, an almond slit made of very thin faces (height < 2 mm) that opens on a
    ~1-2 cm deep pocket. The centre of those slit faces is the eye centre; the front-most slit face gives
    the plane of the lids ('rim').
    """
    width = head_bounds["xmax"] - head_bounds["xmin"]
    height = head_bounds["zmax"] - head_bounds["zmin"]
    cx = (head_bounds["xmin"] + head_bounds["xmax"]) * 0.5
    preset = EYE_PRESET[gender]
    polys = head_polygons(meshes, head_bone_name)
    result = {}
    for side, sign in (("L", -1.0), ("R", 1.0)):
        px = cx + sign * preset["x"] * width
        pz = head_bounds["zmin"] + preset["z"] * height
        slit = []
        for pts in polys:
            c = sum(pts, Vector()) / len(pts)
            if abs(c.x - px) > 0.020 or abs(c.z - pz) > 0.007 or c.y * front_sign < 0.04:
                continue
            ph = max(p.z for p in pts) - min(p.z for p in pts)
            pw = max(p.x for p in pts) - min(p.x for p in pts)
            if ph < 0.0022 and pw > 0.004:
                slit.append((c, pts))
        if len(slit) >= 2:
            xs = [p.x for _, pts in slit for p in pts]
            zs = [p.z for _, pts in slit for p in pts]
            ex = (min(xs) + max(xs)) * 0.5
            ez = (min(zs) + max(zs)) * 0.5
            half_w = (max(xs) - min(xs)) * 0.5
            rim = max(c.y * front_sign for c, _ in slit)
        else:
            log("Eye {}: slit not found, using measured preset.".format(side))
            ex, ez, half_w, rim = px, pz, 0.016, None
        result[side] = {"x": ex, "z": ez, "half_w": half_w, "rim_f": rim, "sign": sign}
    return result


def add_eyes(meshes, head_bone_name, head_bounds, front_sign, tree, armature, collection, mats, gender):
    """Simple, friendly eyes that fill the eye aperture of the mesh (and its pocket).

    One thin, slightly domed plate per eye: warm-white sclera, a big round iris, a pupil, one catch-light and a smooth
    upper lid line. Everything is placed from the aperture measured on the mesh itself, so the eyes stay inside the
    eye zone of the head.
    """
    head_bone = find_bone(armature, "Head")
    if head_bone is None:
        log("Eyes skipped: no Head bone found.")
        return {}
    apertures = locate_eye_apertures(meshes, head_bone_name, head_bounds, front_sign, gender)
    seg = 22
    male = gender == "male"
    for side, ap in apertures.items():
        half_w = max(ap["half_w"] * 1.06, 0.0166)
        half_h = 0.0088 if male else 0.0080
        ri = half_h * (0.92 if male else 0.86)               # big round iris (women: narrower, more almond)
        rp = ri * 0.46
        iris_v = -0.0003
        rim_f = ap["rim_f"]
        if rim_f is None:
            hit, _ = strict_surface_hit(tree, ap["x"], ap["z"] + 0.006, head_bounds, front_sign)
            rim_f = (hit.y * front_sign) if hit else 0.0
        plane_f = rim_f - 0.0004
        sign = ap["sign"]

        def surface_f(x, z):
            hit, _n = strict_surface_hit(tree, x, z, head_bounds, front_sign)
            return (hit.y * front_sign) if hit else plane_f

        def plate_point(u, v, rho, extra=0.0):
            x, z = ap["x"] + u, ap["z"] + v
            f = max(surface_f(x, z) + 0.0007, plane_f) + 0.0016 * (1.0 - min(1.0, rho) ** 2) + extra
            return (x, f * front_sign, z)

        def boundary_radius(phi):
            return 1.0 / math.sqrt((math.cos(phi) / half_w) ** 2 + (math.sin(phi) / half_h) ** 2)

        verts, faces, fmats = [], [], []
        verts.append(plate_point(0.0, iris_v, 0.0, 0.0008))
        rings = []

        def add_ring(point_fn, extra):
            ids = []
            for k in range(seg):
                phi = 2.0 * math.pi * k / seg
                u, v = point_fn(phi)
                rho = math.sqrt((u / half_w) ** 2 + (v / half_h) ** 2)
                ids.append(len(verts))
                verts.append(plate_point(u, v, rho, extra))
            rings.append(ids)
        add_ring(lambda p: (math.cos(p) * rp, iris_v + math.sin(p) * rp), 0.0008)             # pupil edge
        add_ring(lambda p: (math.cos(p) * ri, iris_v + math.sin(p) * ri), 0.0004)             # iris edge
        def mid_pt(p):
            r = boundary_radius(p)
            return ((math.cos(p) * ri + math.cos(p) * r) * 0.5, (iris_v + math.sin(p) * ri + math.sin(p) * r) * 0.5)
        add_ring(mid_pt, 0.0)
        add_ring(lambda p: (math.cos(p) * boundary_radius(p) * 0.995, math.sin(p) * boundary_radius(p) * 0.995), 0.0)
        for k in range(seg):
            faces.append((0, rings[0][k], rings[0][(k + 1) % seg])); fmats.append(2)              # pupil
        for r, mat_i in enumerate((1, 0, 0)):                                                     # iris, sclera, sclera
            for k in range(seg):
                a_, b_ = rings[r][k], rings[r][(k + 1) % seg]
                c_, d_ = rings[r + 1][(k + 1) % seg], rings[r + 1][k]
                faces.append((a_, b_, c_, d_)); fmats.append(mat_i)
        c0 = len(verts)                                                                           # one catch-light
        hu, hv, hr = -0.32 * ri, iris_v + 0.36 * ri, ri * 0.25
        verts.append(plate_point(hu, hv, 0.2, 0.0011))
        for k in range(6):
            a = 2.0 * math.pi * k / 6
            verts.append(plate_point(hu + math.cos(a) * hr, hv + math.sin(a) * hr, 0.2, 0.0011))
        for k in range(6):
            faces.append((c0, c0 + 1 + k, c0 + 1 + (k + 1) % 6)); fmats.append(3)
        # smooth upper lid line: tapers to nothing at both corners, thickest over the middle; slightly covers the iris
        outer_phi = 0.0 if sign > 0 else math.pi
        direction = 1.0 if sign > 0 else -1.0
        m = 14
        lid_in, lid_out = [], []
        t_max = 0.0030 if male else 0.0044
        for k in range(m):
            sp = k / (m - 1)
            phi = outer_phi + direction * math.pi * sp
            r = boundary_radius(phi)
            u, v = math.cos(phi) * r, math.sin(phi) * r
            droop = lerp(0.99, 0.90, math.sin(math.pi * sp))
            thick = 0.0004 + t_max * math.sin(math.pi * sp) ** 0.7
            lid_in.append(len(verts)); verts.append(plate_point(u * droop, v * droop, 1.0, 0.0013))
            lid_out.append(len(verts)); verts.append(plate_point(u + math.cos(phi) * thick, v + math.sin(phi) * thick, 1.0, 0.0014))
        for k in range(m - 1):
            faces.append((lid_in[k], lid_out[k], lid_out[k + 1], lid_in[k + 1])); fmats.append(2)
        faces = fix_winding_outward(verts, faces, lambda c: Vector((0.0, float(front_sign), 0.0)))
        attach_mesh_ex("Eye_" + side, verts, faces,
                       [mats["sclera"], mats["iris"], mats["pupil"], mats["glint"]], fmats,
                       armature, collection, lambda p, bn=head_bone.name: {bn: 1.0}, smooth=True)
    return apertures


def surface_ribbon(name, rows, tree, bounds, front_sign, mat, armature, collection, weight_fn):
    """rows: list of rows, each a list of (x, z, lift). Conforms to the surface by ray casting."""
    verts, faces = [], []
    grid = []
    for row in rows:
        ids = []
        for x, z, lift in row:
            hit, normal = strict_surface_hit(tree, x, z, bounds, front_sign)
            if hit is None:
                return None
            ids.append(len(verts))
            verts.append(tuple(hit + normal * lift))
        grid.append(ids)
    for r in range(len(grid) - 1):
        for c in range(len(grid[r]) - 1):
            faces.append((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
    faces = fix_winding_outward(verts, faces, lambda c: Vector((0.0, float(front_sign), 0.0)))
    return attach_mesh_ex(name, verts, faces, [mat], [0] * len(faces), armature, collection,
                          weight_fn, smooth=False)


def add_brows(apertures, head_bounds, front_sign, tree, armature, collection, mats, gender):
    """Thin, high, softly arched brows (anime style); they take the hair colour."""
    head_bone = find_bone(armature, "Head")
    if head_bone is None or not apertures:
        return
    hw = lambda p, bn=head_bone.name: {bn: 1.0}
    male = gender == "male"
    for side, ap in apertures.items():
        sign = ap["sign"]
        n = 9
        rows_low, rows_mid, rows_up = [], [], []
        base_z = ap["z"] + (0.0245 if male else 0.0232)
        for i in range(n):
            s = i / (n - 1)                        # 0 inner (nose) -> 1 outer
            x = ap["x"] - sign * 0.019 * (1.0 - s) + sign * 0.024 * s
            arch = (0.0036 if male else 0.0050) * math.sin(math.pi * min(1.0, s * 0.95 + 0.02)) - 0.0016 * s
            hh = (0.0026 if male else 0.0029) * (1.0 - 0.60 * s) + 0.0006
            z = base_z + arch
            rows_low.append((x, z - hh, 0.0006))
            rows_mid.append((x, z, 0.0014))
            rows_up.append((x, z + hh, 0.0006))
        surface_ribbon("Brow_" + side, [rows_low, rows_mid, rows_up], tree, head_bounds, front_sign,
                       mats["brow"], armature, collection, hw)


# fractions of the BODY height: where the centre of the chest mass is (measured on the base meshes)
CHEST_PRESET = {
    "male":   {"x": 0.0438, "z": 0.7887},   # pec centre: 3rd vertex above the lower pec ledge (z 1.476 on base_male)
    "female": {"x": 0.0374, "z": 0.7445},
}


# Optional manual override: vertex index (of the source FBX mesh) where the nipple goes; the mirror vertex is found
# automatically. None = automatic (female: sharpest breast tip, male: pec-centre vertex). Example: {"male": 1074}
NIPPLE_VERTEX = {"male": 1074, "female": None}


def mirror_xz(meshes, x, z):
    best = None
    for obj in meshes:
        mw = obj.matrix_world
        for v in obj.data.vertices:
            w = mw @ v.co
            d2 = (w.x + x) ** 2 + (w.z - z) ** 2
            if best is None or d2 < best[0]:
                best = (d2, w.x, w.z)
    return best[1], best[2]


def find_chest_peak_vertex(meshes, body_bounds, front_sign, gender, min_convexity=0.006):
    """The 'punta' of the breast: the vertex that sticks out most from its neighbours (along its normal).

    Returns {"L": (x, z), "R": (x, z)} or None when the mesh has no distinct peak (typically the male pecs).
    """
    height = body_bounds["zmax"] - body_bounds["zmin"]
    p = CHEST_PRESET[gender]
    cx_body = (body_bounds["xmin"] + body_bounds["xmax"]) * 0.5
    ex, ez = p["x"] * height, body_bounds["zmin"] + p["z"] * height
    result = {}
    forced = NIPPLE_VERTEX.get(gender)
    if forced is not None:
        w = meshes[0].matrix_world @ meshes[0].data.vertices[forced].co
        mx_, mz_ = mirror_xz(meshes, w.x, w.z)
        return {"R" if w.x > 0 else "L": (w.x, w.z), "L" if w.x > 0 else "R": (mx_, mz_)}
    for side, sign in (("L", -1.0), ("R", 1.0)):
        best = None
        if gender == "male":
            # the pectoral has no single sharp tip: use the vertex that sits in the nipple column, above the
            # lower pec ledge (nearest vertex to the measured position)
            tx, tz = cx_body + sign * ex, ez
            near = None
            for obj in meshes:
                mw = obj.matrix_world
                for v in obj.data.vertices:
                    w = mw @ v.co
                    if (mw.to_3x3() @ v.normal).normalized().y * front_sign < 0.15:
                        continue
                    d2 = (w.x - tx) ** 2 + (w.z - tz) ** 2
                    if near is None or d2 < near[0]:
                        near = (d2, w.x, w.z)
            if near is None or near[0] > 0.02 ** 2:
                return None
            result[side] = (near[1], near[2])
            continue
        for obj in meshes:
            mw = obj.matrix_world
            me = obj.data
            pos = [mw @ v.co for v in me.vertices]
            nrm = [(mw.to_3x3() @ v.normal).normalized() for v in me.vertices]
            nb = [set() for _ in pos]
            for e in me.edges:
                a, b = e.vertices
                nb[a].add(b)
                nb[b].add(a)
            for i, v in enumerate(pos):
                if abs(v.x - (cx_body + sign * ex)) > 0.040 or abs(v.z - ez) > 0.060:
                    continue
                if nrm[i].y * front_sign < 0.15 or not nb[i]:
                    continue
                avg = sum((pos[j] for j in nb[i]), Vector()) / len(nb[i])
                convex = (v - avg).dot(nrm[i])
                if best is None or convex > best[0]:
                    best = (convex, v.x, v.z)
        if best is None or best[0] < min_convexity:
            return None
        result[side] = (best[1], best[2])
    return result


def find_chest_apex(tree, body_bounds, front_sign, gender):
    """Centre of the pectoral / breast mass = centroid of the most forward cells around the preset."""
    height = body_bounds["zmax"] - body_bounds["zmin"]
    p = CHEST_PRESET[gender]
    cx_body = (body_bounds["xmin"] + body_bounds["xmax"]) * 0.5
    ex, ez = p["x"] * height, body_bounds["zmin"] + p["z"] * height
    result = {}
    for side, sign in (("L", -1.0), ("R", 1.0)):
        cells = []
        for ix in range(-14, 15):
            for iz in range(-14, 15):
                x = cx_body + sign * ex + ix * 0.002
                z = ez + iz * 0.002
                hit, normal = strict_surface_hit(tree, x, z, body_bounds, front_sign)
                if hit is not None:
                    cells.append((hit.y * front_sign, x, z))
        if not cells:
            result[side] = (cx_body + sign * ex, ez)
            continue
        fmax = max(c[0] for c in cells)
        top = [c for c in cells if c[0] >= fmax - 0.0022]
        ax_ = sum(c[1] for c in top) / len(top)
        az_ = sum(c[2] for c in top) / len(top)
        # The most forward line of these meshes is the lower ledge of the pectoral / breast. The visual centre
        # of the mass is half way up the frontal plateau above it: walk up until the profile breaks.
        prof = []
        for k in range(0, 17):
            hit, normal = strict_surface_hit(tree, ax_, az_ + k * 0.005, body_bounds, front_sign)
            prof.append(hit.y * front_sign if hit else None)
        top_dz = 0.038
        for k in range(1, 16):
            if prof[k] is not None and prof[k + 1] is not None and prof[k] - prof[k + 1] > 0.0022:
                top_dz = k * 0.005
                break
        result[side] = (ax_, az_ + top_dz * 0.5)
    return result


def flat_disc(name, cx, cz, radius, tree, bounds, front_sign, mat, armature, collection, weight_fn,
              lift, segments=16):
    """Flat colour patch that follows the skin (no volume): rings sampled on the real surface."""
    verts, faces = [], []
    ring_scale = (1.0, 0.66, 0.33)
    for f in ring_scale:
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            hit, normal = strict_surface_hit(tree, cx + math.cos(a) * radius * f, cz + math.sin(a) * radius * f,
                                             bounds, front_sign)
            if hit is None:
                return None
            verts.append(tuple(hit + normal * lift))
    hit, normal = strict_surface_hit(tree, cx, cz, bounds, front_sign)
    if hit is None:
        return None
    center = len(verts)
    verts.append(tuple(hit + normal * lift))
    for r in range(len(ring_scale) - 1):
        for i in range(segments):
            a = r * segments + i
            b = r * segments + (i + 1) % segments
            c = (r + 1) * segments + (i + 1) % segments
            d = (r + 1) * segments + i
            faces.append((a, b, c, d))
    last = (len(ring_scale) - 1) * segments
    for i in range(segments):
        faces.append((last + i, last + (i + 1) % segments, center))
    faces = fix_winding_outward(verts, faces, lambda c: Vector((0.0, float(front_sign), 0.0)))
    return attach_mesh_ex(name, verts, faces, [mat], [0] * len(faces), armature, collection, weight_fn)


def skin_weights_sampler(meshes):
    """weight_fn that copies the bone weights of the nearest body vertex (so a patch moves with the skin)."""
    pts, info = [], []
    for obj in meshes:
        names = {g.index: g.name for g in obj.vertex_groups}
        mw = obj.matrix_world
        for v in obj.data.vertices:
            pts.append(mw @ v.co)
            info.append({names[m.group]: m.weight for m in v.groups if m.group in names and m.weight > 0.01})
    kd = KDTree(len(pts))
    for i, p in enumerate(pts):
        kd.insert(p, i)
    kd.balance()

    def fn(position):
        _, idx, _ = kd.find(position)
        w = info[idx]
        if not w:
            return {}
        top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        return dict(top)
    return fn


def add_chest_colour(body_bounds, front_sign, tree, meshes, armature, collection, mats, gender):
    """Areola + nipple as FLAT colour decals centred on the pectoral/breast mass. No geometry volume."""
    weights = skin_weights_sampler(meshes)
    apex = find_chest_peak_vertex(meshes, body_bounds, front_sign, gender)
    if apex is None:
        apex = find_chest_apex(tree, body_bounds, front_sign, gender)
    areola_r = 0.0150 if gender == "female" else 0.0128
    nipple_r = areola_r * 0.46
    for side, (x, z) in apex.items():
        if flat_disc("Areola_" + side, x, z, areola_r, tree, body_bounds, front_sign, mats["areola"],
                     armature, collection, weights, lift=0.0007) is None:
            log("Areola {} skipped: surface ray missed.".format(side))
            continue
        flat_disc("Nipple_" + side, x, z, nipple_r, tree, body_bounds, front_sign, mats["nipple"],
                  armature, collection, weights, lift=0.0011, segments=12)


# ======================================================================================
# V6 HAIR: skull-following shell + flat, rounded locks that flow over the scalp (no cones)
# ======================================================================================

def sm(x):
    x = clamp01(x)
    return x * x * (3.0 - 2.0 * x)


def hash01(i, salt=0):
    """Deterministic pseudo random in [0, 1) so every build is identical."""
    v = math.sin(i * 12.9898 + salt * 78.233) * 43758.5453
    return v - math.floor(v)


def table_value(table, a):
    """Piecewise smooth interpolation over |azimuth| (degrees). table = [(az, value), ...] sorted by az."""
    a = abs(a)
    if a <= table[0][0]:
        return table[0][1]
    for (a0, v0), (a1, v1) in zip(table, table[1:]):
        if a <= a1:
            t = (a - a0) / (a1 - a0) if a1 > a0 else 1.0
            return lerp(v0, v1, sm(t))
    return table[-1][1]


class HairFrame:
    """Samples the head surface along rays that leave the skull centre."""

    def __init__(self, tree, head_bounds, front_sign, eye_z):
        self.tree = tree
        self.fs = front_sign
        cx = (head_bounds["xmin"] + head_bounds["xmax"]) * 0.5
        hit_f, _ = strict_surface_hit(tree, cx, eye_z, head_bounds, front_sign)
        hit_b, _ = strict_back_hit(tree, cx, eye_z, head_bounds, front_sign)
        yf = hit_f.y if hit_f else head_bounds["ymin"]
        yb = hit_b.y if hit_b else head_bounds["ymax"]
        self.C = Vector((cx, (yf + yb) * 0.5, eye_z + 0.004))
        self.head_bounds = head_bounds
        self.fwd = Vector((0.0, float(front_sign), 0.0))
        self.back = -self.fwd
        self.right = Vector((1.0, 0.0, 0.0))
        self.up = Vector((0.0, 0.0, 1.0))
        self.down = -self.up
        self._cache = {}

    def dir(self, az, el):
        a, e = math.radians(az), math.radians(el)
        return Vector((math.sin(a) * math.cos(e), self.fs * math.cos(a) * math.cos(e), math.sin(e)))

    def angles(self, d):
        d = d.normalized()
        return (math.degrees(math.atan2(d.x, self.fs * d.y)), math.degrees(math.asin(max(-1.0, min(1.0, d.z)))))

    def point(self, az, el):
        key = (round(az, 2), round(el, 2))
        if key in self._cache:
            return self._cache[key]
        d = self.dir(az, el)
        loc, normal, _idx, _dist = self.tree.ray_cast(self.C, d, 0.6)
        if loc is None:
            loc, normal = self.C + d * 0.09, d.copy()
        if normal.dot(d) < 0:
            normal = -normal
        res = (loc, normal.normalized(), d)
        self._cache[key] = res
        return res

    def hull_point(self, az, el, spread=9.0):
        """Skull point pushed out to the largest radius found in the neighbourhood: a coarse shell built on it
        always stays above the little bumps / facets of the head (no skin showing through)."""
        key = ("h", round(az, 2), round(el, 2), spread)
        if key in self._cache:
            return self._cache[key]
        loc0, n0, d0 = self.point(az, el)
        best = (loc0 - self.C).length
        for daz, delv in ((spread, 0.0), (-spread, 0.0), (0.0, spread), (0.0, -spread),
                          (spread * 0.7, spread * 0.7), (-spread * 0.7, spread * 0.7),
                          (spread * 0.7, -spread * 0.7), (-spread * 0.7, -spread * 0.7)):
            l1, _n1, _d1 = self.point(az + daz, max(-80.0, min(89.0, el + delv)))
            best = max(best, (l1 - self.C).length)
        res = (self.C + d0 * best, n0, d0)
        self._cache[key] = res
        return res

    def surface(self, az, el, t):
        """Point t metres above the skull surface (normal blended with the radial direction)."""
        loc, n, d = self.hull_point(az, el)
        out = (n * 0.45 + d * 0.55).normalized()
        return loc + out * t, out

    def surface_dir(self, d, t):
        az, el = self.angles(d)
        return self.surface(az, el, t)


def make_weight_fn(armature, low_z=None, high_z=None):
    head = find_bone(armature, "Head")
    spine = None
    for cand in ("Spine2", "Spine1", "Chest"):
        spine = find_bone(armature, cand)
        if spine:
            break
    head_name = head.name
    if low_z is None or spine is None:
        return lambda p: {head_name: 1.0}
    spine_name = spine.name

    def fn(p):
        t = sm((p.z - low_z) / max(1e-6, high_z - low_z))
        return {head_name: t, spine_name: 1.0 - t} if t < 0.999 else {head_name: 1.0}
    return fn


class HairBuilder:
    def __init__(self, frame, armature, collection, mat, weight_fn, name="Hair", avoid_tree=None, avoid_z=None):
        self.avoid_tree = avoid_tree
        self.avoid_z = avoid_z
        self.protect = set()
        self.f = frame
        self.armature, self.collection, self.mat = armature, collection, mat
        self.weight_fn = weight_fn
        self.name = name
        self.verts, self.faces = [], []
        self.parts = 0

    def _add(self, verts, faces, protect=()):
        base = len(self.verts)
        self.protect |= {base + i for i in protect}
        self.verts += [tuple(v) for v in verts]
        self.faces += [tuple(i + base for i in f) for f in faces]
        self.parts += 1

    def push_out_of_body(self, margin=0.006):
        """Hair must lie ON the skin, never inside it: every vertex below the chin that is inside the body (or closer
        than `margin` to it) is moved out along the surface normal."""
        tree = self.avoid_tree
        if tree is None:
            return 0
        moved = 0
        for i, v in enumerate(self.verts):
            p = Vector(v)
            if self.avoid_z is not None and p.z > self.avoid_z:
                continue
            for _ in range(3):
                loc, normal, _idx, dist = tree.find_nearest(p, 0.08)
                if loc is None:
                    break
                inside = (p - loc).dot(normal) < 0.0
                if not inside and dist >= margin:
                    break
                p = loc + normal.normalized() * margin
                moved += 1
            self.verts[i] = tuple(p)
        return moved

    def _push_out(self, margin=0.0050):
        """Keep every free hair vertex outside the body (neck, shoulders, back, ears): hair never enters the skin."""
        tree = self.avoid_tree
        if tree is None:
            return 0
        moved = 0
        for _ in range(3):
            changed = False
            for i, v in enumerate(self.verts):
                if i in self.protect:
                    continue
                p = Vector(v)
                if self.avoid_z is not None and p.z > self.avoid_z:
                    continue
                loc, nrm, _idx, dist = tree.find_nearest(p)
                if loc is None:
                    continue
                inside = (p - loc).dot(nrm) < 0.0
                if inside or dist < margin:
                    self.verts[i] = tuple(loc + nrm * margin)
                    changed = True
                    moved += 1
            if not changed:
                break
        return moved

    def finish(self):
        if not self.faces:
            return None
        self._push_out()
        moved = self.push_out_of_body()
        if moved:
            log("Hair {}: {} vertices moved out of the body".format(self.name, moved))
        return attach_mesh_ex(self.name, self.verts, self.faces, [self.mat], [0] * len(self.faces),
                              self.armature, self.collection, self.weight_fn, smooth=False)

    # --- shell: skull-following cap, optionally with a hanging curtain -----------------------------------------------
    def shell(self, hairline, thickness, n_az=32, fracs=(0.0, 0.10, 0.22, 0.38, 0.56, 0.74, 0.90, 0.97),
              skirt=None, wave=None, buried=0.0022, skirt_rows=7):
        """hairline(az) -> elevation (deg) of the lower edge. thickness(az, el, frac) -> metres.
        skirt(az) -> (length, flare, curl) : curtain that hangs below the hairline (bob, long hair).
        wave(az, t) -> sideways offset (m) of the curtain (t = 0..1 down the curtain)."""
        f = self.f
        verts, faces = [], []
        grid = []
        azs = [-180.0 + 360.0 * i / n_az for i in range(n_az)]
        for az in azs:
            el0 = hairline(az)
            col = []
            for fr in fracs:
                el = lerp(el0, 90.0, fr)
                p, _ = f.surface(az, el, thickness(az, el, fr))
                col.append(len(verts))
                verts.append(tuple(p))
            grid.append(col)
        pole_p, _ = f.surface(0.0, 90.0, thickness(0.0, 90.0, 1.0))
        pole = len(verts)
        verts.append(tuple(pole_p))
        top = len(fracs) - 1
        body = []
        for i in range(n_az):
            j = (i + 1) % n_az
            for r in range(top):
                body.append((grid[i][r], grid[j][r], grid[j][r + 1], grid[i][r + 1]))
            body.append((grid[i][top], grid[j][top], pole))
        body = fix_winding_outward(verts, body, lambda c: c - f.C)
        edge = []
        protect = []
        if skirt is None:
            buried_ids = []
            for az in azs:
                p, _ = f.surface(az, hairline(az), -buried)
                buried_ids.append(len(verts))
                protect.append(len(verts))
                verts.append(tuple(p))
            for i in range(n_az):
                j = (i + 1) % n_az
                edge.append((buried_ids[i], buried_ids[j], grid[j][0], grid[i][0]))
            edge = fix_winding_outward(verts, edge, lambda c: Vector((0.0, 0.0, -1.0)))
        else:
            rows = skirt_rows
            ring_prev = [grid[i][0] for i in range(n_az)]
            for r in range(1, rows + 1):
                t = r / rows
                ring = []
                for i, az in enumerate(azs):
                    length, flare, curl = skirt(az)
                    base = Vector(verts[grid[i][0]])
                    radial = Vector((base.x - f.C.x, base.y - f.C.y, 0.0))
                    radial = radial / (radial.length or 1.0)
                    side = radial.cross(Vector((0.0, 0.0, 1.0)))
                    w = wave(az, t) if wave else 0.0
                    p = (base + Vector((0.0, 0.0, -length * t)) + radial * (flare * math.sin(t * math.pi * 0.5) - curl * t * t)
                         + side * w)
                    ring.append(len(verts))
                    verts.append(tuple(p))
                for i in range(n_az):
                    j = (i + 1) % n_az
                    edge.append((ring_prev[i], ring_prev[j], ring[j], ring[i]))
                ring_prev = ring
            edge = fix_winding_outward(verts, edge, lambda c: Vector((c.x - f.C.x, c.y - f.C.y, 0.0)))
            # inner lining + bottom lip, so the curtain is closed
            inner_prev = ring_prev
            lip = []
            for i, az in enumerate(azs):
                length, flare, curl = skirt(az)
                base = Vector(verts[ring_prev[i]])
                radial = Vector((base.x - f.C.x, base.y - f.C.y, 0.0))
                radial = radial / (radial.length or 1.0)
                p = base - radial * 0.006 + Vector((0, 0, 0.004))
                lip.append(len(verts))
                verts.append(tuple(p))
            for i in range(n_az):
                j = (i + 1) % n_az
                edge.append((lip[i], lip[j], ring_prev[j], ring_prev[i]))
            edge[-n_az:] = fix_winding_outward(verts, edge[-n_az:], lambda c: Vector((0.0, 0.0, -1.0)))
        self._add(verts, body + edge, protect=protect)

    # --- flowing locks that lie on the scalp -----------------------------------------------------------------------
    def strand(self, root_dir, flow, length, width, thick=0.008, h0=0.004, h1=0.004, taper=0.38,
               steps=7, sides=6, tip_round=True):
        """A flat lock that follows `flow` (callable: (d, t) -> world direction vector) across the skull."""
        f = self.f
        d = Vector(root_dir).normalized()
        pts, nrm, tang = [], [], []
        total = max(0.02, length)
        step = total / steps
        for k in range(steps + 1):
            t = k / steps
            h = lerp(h0, h1, t * t)
            p, out = f.surface_dir(d, h)
            pts.append(p)
            nrm.append(out)
            v = Vector(flow(d, t))
            tv = v - d * v.dot(d)
            if tv.length < 1e-6:
                tv = Vector((0.0, 0.0, -1.0)) - d * d.z * -1.0
            tv.normalize()
            tang.append(tv)
            r = max(0.06, (p - f.C).length)
            d = (d + tv * (step / r)).normalized()
        verts, faces = [], []
        radii = []
        for k in range(len(pts)):
            t = k / steps
            wprof = lerp(1.0, taper, t ** 1.1)
            tprof = lerp(1.0, 0.55, t)
            radii.append((width * 0.5 * wprof, thick * 0.5 * tprof))
        if tip_round:
            pts.append(pts[-1] + tang[-1] * (radii[-1][0] * 0.8))
            nrm.append(nrm[-1])
            tang.append(tang[-1])
            radii.append((radii[-1][0] * 0.45, radii[-1][1] * 0.6))
        for k, c in enumerate(pts):
            lat = tang[k].cross(nrm[k])
            if lat.length < 1e-6:
                lat = Vector((1.0, 0.0, 0.0))
            lat.normalize()
            nn = lat.cross(tang[k]).normalized()
            rx, ry = radii[k]
            for s in range(sides):
                a = 2.0 * math.pi * s / sides
                verts.append(tuple(c + lat * (math.cos(a) * rx) + nn * (math.sin(a) * ry)))
        for r in range(len(pts) - 1):
            for s in range(sides):
                a = r * sides + s
                b = r * sides + (s + 1) % sides
                c2 = (r + 1) * sides + (s + 1) % sides
                d2 = (r + 1) * sides + s
                faces.append((a, b, c2, d2))
        faces.append(tuple((len(pts) - 1) * sides + s for s in range(sides)))
        faces = self._orient_tube(verts, faces, pts, sides)
        self._add(verts, faces)

    def _orient_tube(self, verts, faces, pts, sides):
        out = []
        for fi, face in enumerate(faces):
            ring = min(fi // sides, len(pts) - 1) if fi < (len(pts) - 1) * sides else len(pts) - 1
            center = pts[ring] if len(face) == sides else (pts[ring] + pts[min(ring + 1, len(pts) - 1)]) * 0.5
            vs = [Vector(verts[i]) for i in face]
            c = sum(vs, Vector()) / len(vs)
            n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
            outward = c - center
            if len(face) == sides:                                  # tip cap: along the lock direction
                outward = pts[-1] - pts[-2]
            if n.dot(outward) < 0:
                face = tuple(reversed(face))
            out.append(face)
        return out

    # --- hanging masses (ponytail, long front locks, braid segments) --------------------------------------------------
    def hang(self, p0, dir0, length, width, thick, gravity=0.9, taper=0.45, sway=0.0, sway_phase=0.0, steps=8,
             sides=6, up_hint=None):
        """Flat strand hanging from p0: starts along dir0 and bends down under 'gravity'."""
        pts = [Vector(p0)]
        d = Vector(dir0).normalized()
        step = length / steps
        for k in range(steps):
            t = (k + 1) / steps
            d = (d + Vector((0.0, 0.0, -1.0)) * (gravity * step / max(0.04, length) * 2.2)).normalized()
            nxt = pts[-1] + d * step
            if sway:
                nxt = nxt + self.f.right * (sway * math.sin(t * 2.0 * math.pi * 1.2 + sway_phase) * step)
            pts.append(nxt)
        self.tube(pts, [(width * 0.5 * lerp(1.0, taper, (k / steps) ** 1.2), thick * 0.5 * lerp(1.0, 0.6, k / steps))
                        for k in range(steps + 1)], sides, up_hint=up_hint, round_tip=True)

    def tube(self, pts, radii, sides=6, up_hint=None, round_tip=False, twists=None):
        pts = list(pts)
        radii = list(radii)
        twists = list(twists) if twists else [0.0] * len(pts)
        if round_tip:
            tan = (pts[-1] - pts[-2]).normalized()
            pts.append(pts[-1] + tan * radii[-1][0] * 0.8)
            radii.append((radii[-1][0] * 0.45, radii[-1][1] * 0.55))
            twists.append(twists[-1])
        verts, faces = [], []
        prev_u = Vector(up_hint) if up_hint is not None else self.f.right.copy()
        for idx, c in enumerate(pts):
            if idx == 0:
                tan = (pts[1] - c).normalized()
            elif idx == len(pts) - 1:
                tan = (c - pts[idx - 1]).normalized()
            else:
                tan = (pts[idx + 1] - pts[idx - 1]).normalized()
            u = prev_u - tan * prev_u.dot(tan)
            if u.length < 1e-5:
                u = Vector((0.0, 0.0, 1.0)) - tan * tan.z
            u.normalize()
            v = tan.cross(u).normalized()
            prev_u = u
            rx, ry = radii[idx]
            tw = twists[idx]
            for s in range(sides):
                a = 2.0 * math.pi * s / sides + tw
                verts.append(tuple(c + u * (math.cos(a) * rx) + v * (math.sin(a) * ry)))
        for r in range(len(pts) - 1):
            for s in range(sides):
                a = r * sides + s
                b = r * sides + (s + 1) % sides
                c2 = (r + 1) * sides + (s + 1) % sides
                d = (r + 1) * sides + s
                faces.append((a, b, c2, d))
        faces.append(tuple((len(pts) - 1) * sides + s for s in range(sides)))
        faces.append(tuple(reversed(range(sides))))
        faces = self._orient_tube(verts, faces[:-1], pts, sides) + [self._cap_base(verts, faces[-1], pts)]
        self._add(verts, faces)

    def _cap_base(self, verts, face, pts):
        vs = [Vector(verts[i]) for i in face]
        n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
        return face if n.dot(pts[0] - pts[1]) > 0 else tuple(reversed(face))

    def blob(self, center, radii, seg=8, rings=5, basis=None):
        """Low-poly ellipsoid (bun, hair-tie)."""
        c0 = Vector(center)
        axis = Matrix.Identity(3) if basis is None else basis
        rx, ry, rz = radii
        verts, faces = [], []
        verts.append(tuple(c0 + axis @ Vector((0, 0, rz))))
        for r in range(1, rings):
            th = math.pi * r / rings
            for s in range(seg):
                ph = 2.0 * math.pi * s / seg
                verts.append(tuple(c0 + axis @ Vector((math.sin(th) * math.cos(ph) * rx,
                                                       math.sin(th) * math.sin(ph) * ry, math.cos(th) * rz))))
        verts.append(tuple(c0 + axis @ Vector((0, 0, -rz))))
        for s in range(seg):
            faces.append((0, 1 + s, 1 + (s + 1) % seg))
        for r in range(rings - 2):
            for s in range(seg):
                a = 1 + r * seg + s
                b = 1 + r * seg + (s + 1) % seg
                c = 1 + (r + 1) * seg + (s + 1) % seg
                d = 1 + (r + 1) * seg + s
                faces.append((a, b, c, d))
        last = 1 + (rings - 2) * seg
        for s in range(seg):
            faces.append((last + s, last + (s + 1) % seg, len(verts) - 1))
        faces = fix_winding_outward(verts, faces, lambda c: c - c0)
        self._add(verts, faces)


# ---- hairstyles (each gets a HairBuilder and its HairFrame) ------------------------------------------------------

def jit(i, salt, amp):
    return (hash01(i, salt) - 0.5) * 2.0 * amp


def bezier(p0, p1, p2, p3, n):
    out = []
    for k in range(n):
        t = k / (n - 1)
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t ** 2, t ** 3
        out.append(p0 * a + p1 * b + p2 * c + p3 * d)
    return out


def attract(target):
    t = Vector(target).normalized()
    return lambda d, tt: (t - d) * 3.0


def mix(*pairs):
    """Weighted sum of world direction vectors."""
    return lambda d, t: sum((Vector(v) * w for v, w in pairs), Vector())


def swirl(f, back=0.3, spin=1.0):
    return lambda d, t: f.up.cross(d) * spin + f.back * back


def basis_along(axis):
    z = Vector(axis).normalized()
    x = z.cross(Vector((0, 0, 1)))
    if x.length < 1e-4:
        x = Vector((1, 0, 0))
    x.normalize()
    y = z.cross(x).normalized()
    m = Matrix(((x.x, y.x, z.x), (x.y, y.y, z.y), (x.z, y.z, z.z)))
    return m


# ===================================================== FACETED STYLES ===============================================
# One coarse shell per style (16 columns x 4 rows) -> big flat facets and an angular outline, plus a few solid extras
# (bun, ponytail, plait). Column i sits at azimuth -180 + 22.5 * i  (0 = front, +-90 = ears, 180 = back).
N_COL = 16
COARSE = dict(n_az=N_COL, fracs=(0.0, 0.34, 0.66, 0.90))


def col_fn(values):
    return lambda az: values[int(round((az + 180.0) / 22.5)) % N_COL]


def bump(az, el, az0, el0, w_az, w_el, amount):
    return amount * math.exp(-(((az - az0) / w_az) ** 2 + ((el - el0) / w_el) ** 2))


def faceted_tail(hb, f, root, d_out, length, width, thick, segs=6, sides=6, lean=0.0):
    """Solid faceted ponytail: arcs out of the head then falls."""
    pts = bezier(root, root + d_out * 0.045, root + f.back * 0.075 + f.up * 0.012,
                 root + f.back * (0.050 + lean) + f.down * (length - 0.035), segs + 1)
    radii = [(width * 0.5 * lerp(1.0, 0.40, (i / segs) ** 1.2), thick * 0.5 * lerp(1.0, 0.55, i / segs)) for i in range(segs + 1)]
    hb.tube(pts, radii, sides=sides, up_hint=f.right, round_tip=False)


# --------------------------------------------------- MALE ---------------------------------------------------------
# column order: -180 -157 -135 -112 -90 -67 -45 -22  0  22  45  67  90 112 135 157
def male_crop(hb, f):
    """Short crop: faded sides, tall top, pointed fringe locks."""
    el0 = col_fn([4,   3,   6,  12,  22,  32,  44,  28,  40, 28, 44, 32, 22, 12,  6,  3])
    hb.shell(el0, lambda az, el, fr: 0.0075 + 0.0100 * sm((el - 18) / 55.0) + bump(az, el, 0, 64, 50, 24, 0.017), **COARSE)


def male_side_part(hb, f):
    """Side part: deep groove on one side, big swept volume and long locks on the other."""
    el0 = col_fn([4,   3,   6,  12,  20,  24,  38,  22,  34, 30, 46, 32, 22, 12,  6,  3])
    hb.shell(el0, lambda az, el, fr: 0.0075 + 0.0090 * sm((el - 18) / 55.0)
             + bump(az, el, -38, 58, 40, 24, 0.030) - bump(az, el, 24, 68, 13, 34, 0.011), **COARSE)


def male_medium(hb, f):
    """Ear-length cut with jagged layered ends over the ears and nape."""
    el0 = col_fn([-10, -8, -6,  0,   6,  18,  38,  28,  36, 28, 38, 18,  6,  0, -6, -8])
    ends = [0.075, 0.025, 0.070, 0.030, 0.050, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.050, 0.030, 0.070, 0.025]
    hb.shell(el0, lambda az, el, fr: 0.0105 + 0.0085 * sm((el - 4) / 60.0) + bump(az, el, -20, 60, 50, 24, 0.008),
             skirt=lambda az: (ends[int(round((az + 180.0) / 22.5)) % N_COL], 0.008, 0.002), skirt_rows=2, **COARSE)


def male_tied_back(hb, f):
    """Hair slicked back into a faceted bun."""
    el0 = col_fn([6,   5,   8,  12,  24,  36,  46,  52,  56, 52, 46, 36, 24, 12,  8,  5])
    hb.shell(el0, lambda az, el, fr: 0.0075 + 0.0070 * sm((el - 30) / 50.0) + bump(az, el, 180, 52, 45, 24, 0.010), **COARSE)
    d = f.dir(180, 46)
    p, out = f.surface_dir(d, 0.0)
    hb.blob(p + out * 0.028 + f.back * 0.006, (0.040, 0.040, 0.034), seg=6, rings=3, basis=basis_along(out))
    hb.blob(p + out * 0.006, (0.032, 0.032, 0.011), seg=6, rings=2, basis=basis_along(out))


# --------------------------------------------------- FEMALE -------------------------------------------------------
def female_ponytail(hb, f):
    """Smooth top, pointed side-swept fringe and a faceted high ponytail."""
    el0 = col_fn([6,   5,   8,  12,  22,  34,  44,  30,  40, 28, 46, 34, 22, 12,  8,  5])
    hb.shell(el0, lambda az, el, fr: 0.0080 + 0.0075 * sm((el - 24) / 52.0) + bump(az, el, 180, 54, 40, 24, 0.013), **COARSE)
    d = f.dir(180, 52)
    p, out = f.surface_dir(d, 0.0)
    root = p + out * 0.012
    faceted_tail(hb, f, root, f.up * 0.5 + f.back * 0.5, 0.31, 0.066, 0.040, segs=6, sides=6)
    hb.blob(root + f.up * 0.005 + f.back * 0.004, (0.034, 0.034, 0.014), seg=6, rings=2, basis=basis_along(f.up + f.back * 0.6))


def female_bob(hb, f):
    """Chin-length bob: jagged curtain ends and a pointed fringe."""
    el0 = col_fn([-28, -26, -24, -20, -18, 4,  40,  28,  38, 28, 40,  4, -18, -20, -24, -26])
    ends = [0.100, 0.060, 0.095, 0.058, 0.090, 0.050, 0.0, 0.0, 0.0, 0.0, 0.0, 0.050, 0.090, 0.058, 0.095, 0.060]
    hb.shell(el0, lambda az, el, fr: 0.0115 + 0.0075 * sm((el - 6) / 58.0) + bump(az, el, 0, 62, 70, 24, 0.008),
             skirt=lambda az: (ends[int(round((az + 180.0) / 22.5)) % N_COL], 0.016, 0.010), skirt_rows=2, **COARSE)


def female_long(hb, f):
    """Long hair, high centre part, open face and jagged ends that flare away from the shoulders."""
    el0 = col_fn([-36, -34, -30, -22, -14,  12,  42,  52,  62, 52, 42, 12, -14, -22, -30, -34])
    ends = [0.36, 0.20, 0.34, 0.18, 0.26, 0.16, 0.0, 0.0, 0.0, 0.0, 0.0, 0.16, 0.26, 0.18, 0.34, 0.20]
    hb.shell(el0, lambda az, el, fr: 0.0110 + 0.0070 * sm((el - 6) / 58.0) + bump(az, el, 0, 70, 80, 26, 0.006)
             - bump(az, el, 0, 74, 8, 40, 0.007),
             skirt=lambda az: (ends[int(round((az + 180.0) / 22.5)) % N_COL], 0.030, 0.0), skirt_rows=3, **COARSE)


def female_braid(hb, f):
    """Smooth swept-back top with a pointed fringe and one thick faceted plait."""
    el0 = col_fn([-4,  -3,   2,   8,  16,  28,  42,  34,  44, 34, 42, 28, 16,  8,  2, -3])
    hb.shell(el0, lambda az, el, fr: 0.0080 + 0.0065 * sm((el - 24) / 52.0) + bump(az, el, 180, 30, 40, 26, 0.009), **COARSE)
    p, out = f.surface_dir(f.dir(180, 14), 0.0)
    start = p + f.back * 0.014
    n = 10
    pts, radii, tw = [], [], []
    for k in range(n):
        t = k / (n - 1)
        c = start + Vector((0.0, 0.0, -0.300 * t)) + f.back * (0.016 * math.sin(t * math.pi))
        c = c + f.right * ((0.0075 if k % 2 == 0 else -0.0075) * lerp(1.0, 0.5, t))
        pts.append(c)
        rr = lerp(0.025, 0.013, t)
        radii.append((rr, rr * 0.8))
        tw.append(k * math.pi * 0.25)
    hb.tube(pts, radii, sides=5, up_hint=f.right, round_tip=False, twists=tw)
    hb.blob(pts[-1] + Vector((0, 0, 0.002)), (0.015, 0.015, 0.008), seg=6, rings=2)


STYLE_FUNCS = {
    ("male", "crop"): male_crop,
    ("male", "side_part"): male_side_part,
    ("male", "medium"): male_medium,
    ("male", "tied_back"): male_tied_back,
    ("female", "ponytail"): female_ponytail,
    ("female", "bob"): female_bob,
    ("female", "long"): female_long,
    ("female", "braid"): female_braid,
}
LONG_STYLES = {("female", "ponytail"), ("female", "long"), ("female", "braid"), ("female", "bob")}


def add_hair(head_bounds, front_sign, armature, collection, mats, gender, style, head_tree, eye_z, avoid_tree=None):
    head_bone = find_bone(armature, "Head")
    if head_bone is None or head_tree is None:
        log("Hair skipped: missing Head bone or head surface.")
        return None
    func = STYLE_FUNCS.get((gender, style))
    if func is None:
        raise ValueError("Stile capelli non riconosciuto: {} / {}".format(gender, style))
    frame = HairFrame(head_tree, head_bounds, front_sign, eye_z)
    low_z, high_z = None, None
    if (gender, style) in LONG_STYLES:
        hp = armature.matrix_world @ armature.pose.bones[head_bone.name].head
        low_z, high_z = hp.z - 0.20, hp.z - 0.01
    weights = make_weight_fn(armature, low_z, high_z)
    hb = HairBuilder(frame, armature, collection, mats["hair"], weights, name="Hair_{}_{}".format(gender, style),
                     avoid_tree=avoid_tree, avoid_z=head_bounds["zmin"] + 0.025)
    func(hb, frame)
    obj = hb.finish()
    log("Hair {}/{}: {} parts, {} faces".format(gender, style, hb.parts, len(hb.faces)))
    return obj


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
    # Original mesh coordinates, vertex groups, armature and rest pose are preserved untouched.
    body_prefix = "Human_{}_Base".format(gender.title())
    recolor_body(meshes, body_prefix, skin_color)
    head_bone_name, head_pts, head_bounds = get_head_bounds(meshes, armature)
    body_bounds = bounds_of_points(world_vertices(meshes))
    front_sign = detect_front_sign(head_pts, head_bounds)
    log("{}: altezza {:.3f}, testa {:.3f} x {:.3f} x {:.3f}, fronte Y={}".format(
        gender, body_bounds["zmax"] - body_bounds["zmin"],
        head_bounds["xmax"] - head_bounds["xmin"], head_bounds["ymax"] - head_bounds["ymin"],
        head_bounds["zmax"] - head_bounds["zmin"], front_sign))
    mats = {
        "hair": make_material(collection_name + "_Hair", hair_color, roughness=0.78),
        "brow": make_material(body_prefix + "_Brows", tuple(hair_color[:3]) + (1.0,), roughness=0.84),
        "sclera": make_material(body_prefix + "_Sclera", (0.985, 0.965, 0.925, 1.0), roughness=0.48),
        "iris": make_material(body_prefix + "_Iris", iris_color, roughness=0.42),
        "pupil": make_material(body_prefix + "_Pupil", (0.018, 0.013, 0.012, 1.0), roughness=0.28),
        "glint": make_material(body_prefix + "_Glint", (0.98, 0.94, 0.82, 1.0), roughness=0.22),
        # Flat colour only (no geometry): the game derives both from the skin tone, see the manifest.
        "areola": make_material(body_prefix + "_Areola", tuple(skin_color[i] * AREOLA_MUL[i] for i in range(3)) + (1.0,), roughness=0.82),
        "nipple": make_material(body_prefix + "_Nipple", tuple(skin_color[i] * NIPPLE_MUL[i] for i in range(3)) + (1.0,), roughness=0.82),
    }
    full_tree = make_bvh(meshes)
    try:
        head_tree = make_bvh_for_bones(meshes, [head_bone_name])
    except Exception as exc:
        log("Head-only BVH unavailable ({}); use full-body BVH for face surface sampling.".format(exc))
        head_tree = full_tree

    apertures = add_eyes(meshes, head_bone_name, head_bounds, front_sign, head_tree, armature, collection, mats, gender)
    add_brows(apertures, head_bounds, front_sign, head_tree, armature, collection, mats, gender)
    add_chest_colour(body_bounds, front_sign, full_tree, meshes, armature, collection, mats, gender)
    hair_obj = None
    if hair_style and hair_style != "bald":
        eye_z = apertures["L"]["z"] if apertures else None
        hair_obj = add_hair(head_bounds, front_sign, armature, collection, mats, gender, hair_style, head_tree, eye_z, full_tree)
    animations_added = attach_animation_library(armature, animation_library)
    armature.data.pose_position = "POSE" if animations_added else "REST"
    if armature.animation_data:
        armature.animation_data.action = None
    bpy.context.view_layer.update()
    return collection, armature, animations_added, hair_obj


def export_objects(objects, armature, output_file, expect_animations):
    """Export exactly `objects` (armature included) as one GLB."""
    output_file.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
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
    if not output_file.exists() or output_file.stat().st_size < 3000:
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


ROLE_TOKENS = {
    "skin": ["_skin"], "hair": ["_hair"], "brows": ["_brows"],
    "sclera": ["_sclera"], "iris": ["_iris"], "pupil": ["_pupil"], "glint": ["_glint"],
    "areola": ["_areola"], "nipple": ["_nipple"],
}


def material_role_indices(glb_path):
    doc = read_glb_json(glb_path)
    out = {}
    for i, mat in enumerate(doc.get("materials", [])):
        name = mat.get("name", "").lower()
        for role, tokens in ROLE_TOKENS.items():
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
    log("Output GLB: {}".format(OUTPUT_DIR))
    animation_library = import_animation_library(ANIMATION_DIR)
    if "Walk" not in animation_library:
        log("ATTENZIONE: nessuna Walking pulita trovata; walking_briefcase.fbx viene ignorata.")
    if not animation_library:
        log("ATTENZIONE: nessuna animazione valida trovata; i GLB saranno riggati ma senza clip.")

    specs = [
        ("male", MALE_FBX, SKIN_MALE, HAIR_MALE, IRIS_MALE),
        ("female", FEMALE_FBX, SKIN_FEMALE, HAIR_FEMALE, IRIS_FEMALE),
    ]
    skin_palette = {
        "porcelain": [0.86, 0.70, 0.60, 1.0], "fair_warm": [0.77, 0.515, 0.365, 1.0],
        "tan": [0.66, 0.43, 0.30, 1.0], "warm_medium": [0.57, 0.335, 0.225, 1.0],
        "olive": [0.47, 0.31, 0.20, 1.0], "brown": [0.38, 0.23, 0.16, 1.0],
        "deep_brown": [0.30, 0.18, 0.13, 1.0], "dark": [0.19, 0.105, 0.075, 1.0],
    }
    manifest = {
        "format": "glb",
        "source_format": "fbx_intermediate_only",
        "notes": [
            "Il gioco carica solo GLB, mai i FBX sorgente.",
            "Ogni personaggio = UN corpo base nudo (human_<genere>_base.glb) + UN layer capelli opzionale (hair_<genere>_<stile>.glb).",
            "I capelli sono un file separato: con un copricapo basta non disegnare il modello capelli.",
            "Capelli e corpo hanno lo STESSO scheletro (stessi nomi e ordine delle ossa): anima il modello capelli con lo stesso frame di animazione del corpo.",
            "I capelli lunghi sono pesati su Head/Spine2: restano morbidi sulle spalle quando il personaggio si muove.",
            "Colori: pelle, capelli e occhi si impostano a runtime per ruolo materiale (material_indices), uguali per tutti i personaggi (palettes).",
            "Occhi: placca semplice nella fessura oculare della mesh (sclera, iride, pupilla, riflesso, linea palpebrale); nessuna cavita' visibile.",
            "Colori regolabili a runtime (slider o palette): imposta il colore di ogni ruolo con material_indices; i ruoli derivati si calcolano dalla pelle (controls).",
            "Capezzoli: se non sono sul vertice voluto, imposta NIPPLE_VERTEX nello script (indice vertice del FBX).",
            "Capezzoli: solo colore, nessuna geometria. Ruoli 'areola' e 'nipple' = colore pelle * moltiplicatori (skin_details nel manifest).",
            "Le animazioni importate dipendono dai file presenti nella cartella animations; walking_briefcase viene ignorata."
        ],
        "skin_detail_multipliers": {"areola": list(AREOLA_MUL), "nipple": list(NIPPLE_MUL)},
        "controls": {
            "skin": {"roles": ["skin"], "derived_from_skin": {"areola": list(AREOLA_MUL), "nipple": list(NIPPLE_MUL)}, "ui": "slider (luminosita'/tono) o palette"},
            "hair": {"roles": ["hair"], "derived_from_hair": {"brows": list(BROW_MUL)}, "ui": "slider (tinta/luminosita') o palette"},
            "eyes": {"roles": ["iris"], "ui": "slider (tinta) o palette"}
        },
        "palettes": {
            "skin": skin_palette,
            "skin_details": {k: {"areola": [round(v[i] * AREOLA_MUL[i], 4) for i in range(3)] + [1.0],
                                 "nipple": [round(v[i] * NIPPLE_MUL[i], 4) for i in range(3)] + [1.0]}
                             for k, v in skin_palette.items()},
            "hair": {
                "black": [0.025, 0.018, 0.016, 1.0], "dark_brown": [0.10, 0.040, 0.020, 1.0],
                "chestnut": [0.30, 0.085, 0.030, 1.0], "dark_blonde": [0.50, 0.32, 0.11, 1.0],
                "blonde": [0.72, 0.46, 0.13, 1.0], "ginger": [0.55, 0.16, 0.05, 1.0],
                "auburn": [0.48, 0.075, 0.025, 1.0], "silver": [0.48, 0.50, 0.55, 1.0],
                "white": [0.86, 0.86, 0.88, 1.0], "platinum": [0.84, 0.77, 0.61, 1.0]
            },
            "eyes": {
                "blue": [0.10, 0.32, 0.82, 1.0], "green": [0.10, 0.42, 0.22, 1.0],
                "brown": [0.25, 0.105, 0.035, 1.0], "dark_brown": [0.14, 0.06, 0.03, 1.0],
                "hazel": [0.45, 0.28, 0.08, 1.0], "amber": [0.62, 0.38, 0.08, 1.0],
                "gray": [0.34, 0.41, 0.48, 1.0], "violet": [0.40, 0.16, 0.68, 1.0]
            }
        },
        "bases": {},
        "hair_styles": {"male": [], "female": []},
    }
    built_count = 0
    for gender, src_path, skin_color, hair_color, iris_color in specs:
        base_exported = False
        for hair_style in HAIR_STYLES[gender]:
            coll_name = "Human_{}_{}".format(gender.title(), hair_style.title())
            collection, armature, clips, hair_obj = build_one(gender, hair_style, src_path, coll_name,
                                                              skin_color, hair_color, iris_color, animation_library)
            if EXPORT_GLB and not base_exported:
                base_file = "human_{}_base.glb".format(gender)
                body_objects = [o for o in collection.objects if o is not hair_obj]
                export_objects(body_objects, armature, OUTPUT_DIR / base_file, bool(clips))
                manifest["bases"][gender] = {
                    "file": base_file, "animation_clips": clips,
                    "material_indices": material_role_indices(OUTPUT_DIR / base_file),
                }
                base_exported = True
            hair_file = "hair_{}_{}.glb".format(gender, hair_style)
            entry = {"id": hair_style, "label": HAIR_LABELS[hair_style], "file": hair_file}
            if EXPORT_GLB and hair_obj is not None:
                export_objects([armature, hair_obj], armature, OUTPUT_DIR / hair_file, False)
                entry["material_indices"] = material_role_indices(OUTPUT_DIR / hair_file)
            manifest["hair_styles"][gender].append(entry)
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
    log("Completato: 2 corpi base + {} layer capelli.".format(built_count))
    log("NOTA: walking_briefcase.fbx è ignorata; per la clip Walk serve un file Walking.fbx senza prop.")


try:
    main()
except Exception:
    log("ERRORE — traceback completo:")
    traceback.print_exc()
    raise
