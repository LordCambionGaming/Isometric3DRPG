
import bpy
bpy.ops.wm.read_homefile(use_empty=True)
bpy.ops.import_scene.fbx(filepath='tools/mixamo/animations/walk.fbx', use_anim=True)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
action = arm.animation_data.action
for fc in action.fcurves:
    if 'mixamorig:Hips' in fc.data_path and 'location' in fc.data_path:
        vals = [kp.co[1] for kp in fc.keyframe_points]
        print(fc.data_path, fc.array_index, 'start:', vals[0], 'end:', vals[-1], 'range:', max(vals)-min(vals))
