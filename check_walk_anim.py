
import bpy
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/male/base_male.fbx', use_anim=False)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True)
for m in meshes: m.select_set(True)
bpy.context.view_layer.objects.active = arm
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

print('Walk Hips keyframe count:', len(fc_x.keyframe_points))
min_z = 999; max_z = -999
for i in range(len(fc_x.keyframe_points)):
    x = fc_x.keyframe_points[i].co[1] * 0.01
    y = -fc_z.keyframe_points[i].co[1] * 0.01
    z = fc_y.keyframe_points[i].co[1] * 0.01
    min_z = min(min_z, z)
    max_z = max(max_z, z)
print('Walk converted Hips Z range:', min_z, max_z)
