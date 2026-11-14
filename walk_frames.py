
import bpy
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/male/base_male.fbx', use_anim=False)
base_arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
base_arm.select_set(True)
for m in meshes: m.select_set(True)
bpy.context.view_layer.objects.active = base_arm
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/animations/walk.fbx', use_anim=True)
anim_arm = [o for o in bpy.data.objects if o not in before and o.type == 'ARMATURE'][0]
action = anim_arm.animation_data.action

fc_x = None; fc_y = None; fc_z = None
for fc in action.fcurves:
    if 'mixamorig:Hips' in fc.data_path and 'location' in fc.data_path:
        if fc.array_index == 0: fc_x = fc
        elif fc.array_index == 1: fc_y = fc
        elif fc.array_index == 2: fc_z = fc

for i in range(len(fc_x.keyframe_points)):
    x = fc_x.keyframe_points[i].co[1]
    y = fc_y.keyframe_points[i].co[1]
    z = fc_z.keyframe_points[i].co[1]
    fc_x.keyframe_points[i].co[1] = x * 0.01
    fc_y.keyframe_points[i].co[1] = -z * 0.01
    fc_z.keyframe_points[i].co[1] = y * 0.01

base_arm.animation_data_create()
base_arm.animation_data.action = action
body = meshes[0]
depsgraph = bpy.context.evaluated_depsgraph_get()

for f in range(0, 32, 4):
    bpy.context.scene.frame_set(f)
    bpy.context.view_layer.update()
    eval_body = body.evaluated_get(depsgraph)
    eval_mesh = eval_body.to_mesh()
    zs = [v.co.z for v in eval_mesh.vertices]
    eval_body.to_mesh_clear()
    print('Walk frame', f, 'min Z:', min(zs), 'max Z:', max(zs))
