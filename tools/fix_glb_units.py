#!/usr/bin/env python3
"""
fix_glb_units.py — Converte GLB da unità Blender (cm) a glTF standard (metri).

I GLB esportati da Mixamo/Blender con armatura scalata 0.01 hanno:
  - Vertici già in metri (corretti)
  - Bone translations in cm (×100 rispetto ai vertici)
  - InverseBindMatrices con scale 100 e translations in cm
  - Animation translations in cm

Questo script:
  1. Moltiplica bone node translations per 0.01 (cm → m)
  2. Moltiplica animation sampler translation outputs per 0.01
  3. Riscala le inverseBindMatrices: rimuove la scala 100 (righe/colonne ×0.01)

Uso:
  python fix_glb_units.py input.glb output.glb
  python fix_glb_units.py --inplace *.glb
"""

import sys
import json
import struct
import os
import math
import copy

SCALE = 0.01  # cm → m

def read_glb(path):
    with open(path, 'rb') as f:
        data = f.read()
    magic, version, length = struct.unpack_from('<III', data, 0)
    assert magic == 0x46546C67, "Not a GLB file"
    # JSON chunk
    json_len, json_type = struct.unpack_from('<II', data, 12)
    assert json_type == 0x4E4F534A, "First chunk is not JSON"
    json_bytes = data[20:20+json_len]
    j = json.loads(json_bytes.decode('utf-8'))
    # BIN chunk (may not exist)
    bin_offset = 20 + json_len
    bin_data = b''
    if bin_offset + 8 <= len(data):
        bin_len, bin_type = struct.unpack_from('<II', data, bin_offset)
        if bin_type == 0x004E4942:
            bin_data = data[bin_offset+8 : bin_offset+8+bin_len]
    return j, bin_data, data

def write_glb(path, j, bin_data):
    json_bytes = json.dumps(j, separators=(',', ':')).encode('utf-8')
    # Pad JSON to 4 bytes
    pad = (4 - len(json_bytes) % 4) % 4
    json_bytes += b' ' * pad
    total = 12 + 8 + len(json_bytes) + (8 + len(bin_data) if bin_data else 0)
    out = struct.pack('<III', 0x46546C67, 2, total)
    out += struct.pack('<II', len(json_bytes), 0x4E4F534A)
    out += json_bytes
    if bin_data:
        # Pad bin to 4 bytes
        pad_bin = (4 - len(bin_data) % 4) % 4
        bin_padded = bin_data + b'\x00' * pad_bin
        out += struct.pack('<II', len(bin_padded), 0x004E4942)
        out += bin_padded
    with open(path, 'wb') as f:
        f.write(out)

def read_floats(bin_data, accessor, buffer_views, count=None, offset_elem=0):
    bv = buffer_views[accessor['bufferView']]
    bv_offset = bv.get('byteOffset', 0)
    acc_offset = accessor.get('byteOffset', 0)
    byte_stride = bv.get('byteStride', 0)
    
    type_map = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
    n_comp = type_map[accessor['type']]
    n_elem = accessor['count'] if count is None else count
    
    fmt = f'<{n_comp}f'
    elem_size = n_comp * 4
    stride = byte_stride if byte_stride else elem_size
    
    start = bv_offset + acc_offset + offset_elem * stride
    result = []
    for i in range(n_elem):
        off = start + i * stride
        vals = struct.unpack_from(fmt, bin_data, off)
        result.append(list(vals))
    return result

def write_floats(bin_data, accessor, buffer_views, values):
    bv = buffer_views[accessor['bufferView']]
    bv_offset = bv.get('byteOffset', 0)
    acc_offset = accessor.get('byteOffset', 0)
    byte_stride = bv.get('byteStride', 0)
    
    type_map = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
    n_comp = type_map[accessor['type']]
    
    fmt = f'<{n_comp}f'
    elem_size = n_comp * 4
    stride = byte_stride if byte_stride else elem_size
    
    bin_list = bytearray(bin_data)
    start = bv_offset + acc_offset
    for i, vals in enumerate(values):
        off = start + i * stride
        struct.pack_into(fmt, bin_list, off, *vals)
    return bytes(bin_list)

def already_fixed(j):
    """Controlla se il file e' gia' stato fixato (hips translation < 1.0)."""
    nodes = j.get('nodes', [])
    skins = j.get('skins', [])
    if not skins:
        return False
    joints = skins[0].get('joints', [])
    if not joints:
        return False
    # Il primo joint (Hips) deve avere translation < 10 se gia' in metri
    hip_node = nodes[joints[0]] if joints[0] < len(nodes) else None
    if hip_node:
        trans = hip_node.get('translation', [])
        if trans and abs(trans[2]) < 10.0:  # Z < 10m means already fixed
            return True
    return False

def fix_glb(input_path, output_path):
    print(f"\nProcessing: {input_path}")
    j, bin_data, raw = read_glb(input_path)
    
    if already_fixed(j):
        print("  Already fixed (bone translations < 10 units), skipping.")
        if input_path != output_path:
            import shutil
            shutil.copy2(input_path, output_path)
        return

    nodes = j.get('nodes', [])
    skins = j.get('skins', [])
    animations = j.get('animations', [])
    accessors = j.get('accessors', [])
    buffer_views = j.get('bufferViews', [])

    bin_data = bytearray(bin_data)

    # ===== 1. Fix bone node translations =====
    # Collect all joint indices from all skins
    joint_set = set()
    for skin in skins:
        for ji in skin.get('joints', []):
            joint_set.add(ji)

    fixed_nodes = 0
    for ni in joint_set:
        if ni >= len(nodes):
            continue
        node = nodes[ni]
        t = node.get('translation')
        if t:
            orig = [t[0], t[1], t[2]]
            nodes[ni]['translation'] = [t[0]*SCALE, t[1]*SCALE, t[2]*SCALE]
            fixed_nodes += 1
    print(f"  Fixed {fixed_nodes} bone node translations (÷100)")

    # ===== 2. Fix inverseBindMatrices =====
    # IBMs are MAT4 in column-major order.
    # Original: scale=100 (bone in cm, skin in m). After fix: scale should be 1, translation in m.
    # IBM transforms from model space to bone local space.
    # If bone is now in meters: new_IBM = scale(0.01) * old_IBM * scale(100)
    # Simpler: the translation part (column 3, rows 0-2) of IBM must be ÷100,
    # and the scale/rotation part stays the same (since we're not changing model space).
    # Actually the full transform: IBM * v_model → bone_local
    # Old bone local in cm. New bone local in m = old/100.
    # So new_IBM = scale(0.01) * old_IBM  →  multiply each row by 0.01... no.
    # IBM is 4x4 col-major. IBM = [R|t; 0|1] where R has scale 100 (to undo the 0.01 armature scale).
    # New IBM (bone in meters) = scale(0.01) * old_IBM
    # This means: multiply each element of col 0,1,2 by 0.01 (scale rows), 
    # and col 3 (translation) by 0.01 too.
    # Net effect: multiply ALL 16 floats by 0.01.
    for skin in skins:
        ibm_idx = skin.get('inverseBindMatrices')
        if ibm_idx is None:
            continue
        acc = accessors[ibm_idx]
        if acc['type'] != 'MAT4':
            continue
        mats = read_floats(bin_data, acc, buffer_views)
        fixed_mats = []
        for mat in mats:
            # mat is 16 floats col-major. Scale entire matrix by 0.01.
            fixed_mats.append([v * SCALE for v in mat])
        bin_data = bytearray(write_floats(bytes(bin_data), acc, buffer_views, fixed_mats))
        print(f"  Fixed {len(mats)} inverseBindMatrices (×0.01)")

    # ===== 3. Fix animation translation channels =====
    fixed_anim_channels = 0
    for anim in animations:
        samplers = anim.get('samplers', [])
        for ch in anim.get('channels', []):
            target = ch.get('target', {})
            if target.get('path') != 'translation':
                continue
            node_idx = target.get('node')
            if node_idx not in joint_set:
                continue
            smp = samplers[ch['sampler']]
            out_acc_idx = smp['output']
            acc = accessors[out_acc_idx]
            if acc['type'] != 'VEC3':
                continue
            vals = read_floats(bin_data, acc, buffer_views)
            scaled = [[v[0]*SCALE, v[1]*SCALE, v[2]*SCALE] for v in vals]
            bin_data = bytearray(write_floats(bytes(bin_data), acc, buffer_views, scaled))
            fixed_anim_channels += 1
    print(f"  Fixed {fixed_anim_channels} animation translation channels (÷100)")

    write_glb(output_path, j, bytes(bin_data))
    print(f"  Written: {output_path}")

def main():
    args = sys.argv[1:]
    inplace = '--inplace' in args
    if inplace:
        args = [a for a in args if a != '--inplace']
    
    if not args:
        print("Usage: python fix_glb_units.py [--inplace] input.glb [output.glb]")
        print("       python fix_glb_units.py --inplace file1.glb file2.glb ...")
        sys.exit(1)
    
    if inplace:
        for path in args:
            if not os.path.exists(path):
                print(f"File not found: {path}")
                continue
            fix_glb(path, path)
    elif len(args) == 2:
        fix_glb(args[0], args[1])
    elif len(args) == 1:
        base, ext = os.path.splitext(args[0])
        fix_glb(args[0], base + '_fixed' + ext)
    else:
        print("Error: specify --inplace for multiple files, or input + output paths")
        sys.exit(1)

if __name__ == '__main__':
    main()
