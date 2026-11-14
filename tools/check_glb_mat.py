import struct
import json

def check(path):
    print("=== " + path + " ===")
    with open(path, "rb") as f:
        data = f.read()
    magic, ver, length = struct.unpack("<4sII", data[:12])
    c_len, c_type = struct.unpack("<II", data[12:20])
    gltf = json.loads(data[20:20+c_len])
    for i, m in enumerate(gltf.get("materials", [])):
        print(f"  mat [{i}] {m.get('name')}: {m.get('pbrMetallicRoughness', {}).get('baseColorFactor')}")
    for m in gltf.get("meshes", []):
        for p in m.get("primitives", []):
            mi = p.get("material")
            mn = gltf["materials"][mi]["name"] if mi is not None else "None"
            print(f"  mesh {m.get('name')} -> mat [{mi}] {mn}")

check("assets/models/characters/bases/human_male_base.glb")
check("assets/models/characters/bases/human_female_base.glb")
