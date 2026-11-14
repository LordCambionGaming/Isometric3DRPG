# Inspect FBX files
import re

def inspect_fbx(path):
    with open(path, 'rb') as f:
        content = f.read()
    # FBX node names often appear as Model::name
    matches = re.findall(b'Model::([a-zA-Z0-9_:]+)', content)
    unique_bones = sorted(list(set(m.decode('ascii', errors='ignore') for m in matches)))
    return unique_bones

for fbx in ['tools/mixamo/animations/idle.fbx', 'tools/mixamo/male/base_male.fbx']:
    bones = inspect_fbx(fbx)
    print(f"=== {fbx} ({len(bones)} bones) ===")
    hand_bones = [b for b in bones if any(k in b.lower() for k in ['hand', 'finger', 'thumb', 'index', 'mid', 'ring', 'pinky'])]
    print("Hand/finger bones count:", len(hand_bones))
    print("Sample hand bones:", hand_bones[:15])
    print("All bones:")
    for b in bones:
        print(" ", b)
