
import bpy
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/animations/idle.fbx', use_anim=True)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
action = arm.animation_data.action
for fc in action.fcurves:
    if 'mixamorig:Hips' in fc.data_path and 'location' in fc.data_path and fc.array_index == 1:
        vals = [kp.co[1] for kp in fc.keyframe_points]
        print('Idle raw Y values:', min(vals), max(vals))

bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/animations/walk.fbx', use_anim=True)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
action = arm.animation_data.action
for fc in action.fcurves:
    if 'mixamorig:Hips' in fc.data_path and 'location' in fc.data_path and fc.array_index == 1:
        vals = [kp.co[1] for kp in fc.keyframe_points]
        print('Walk raw Y values:', min(vals), max(vals))
