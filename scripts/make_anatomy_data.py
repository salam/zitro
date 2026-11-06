#!/usr/bin/env python3
"""Builds webpage2/data/anatomy/ for Zitro: real organ meshes reduced to low-poly files the stick can draw.

Sources (see docs/features/anatomy.md for licences; downloaded by hand into --sources):
  bp3d/partof/*.obj  BodyParts3D 4.0, PART-OF tree: partof_BP3D_4.0_obj_99.zip
  bp3d/isa/*.obj     BodyParts3D 4.0, IS-A tree: isa_BP3D_4.0_obj_99.zip (teeth only)
  hra/*.glb          Human Reference Atlas v1.3: 3d-vh-m-lung.glb, 3d-vh-m-heart.glb

Parts are chosen by name from a closed whitelist, so nothing else in those large sets (which also hold the
reproductive organs of the male body they were modelled on) can end up in a file.

Pipeline for a model: load parts -> weld -> orient outward -> reduce -> drop what can't be seen from outside
-> reduce to budget -> centre, scale to radius 1 -> quantise -> write and re-check.

Needs numpy and fast-simplification. Files read from --sources are untrusted: run with python -I.
"""
import argparse
import json
import math
import os
import re
import struct
import sys
import zlib

import fast_simplification
import numpy as np

SHARE_ALIKE_NOTE = (
    "The BodyParts3D licence page (updated 2025-02-27) says CC BY 4.0. The headers inside the OBJ files, dated 2013, "
    "still say CC BY-SA 2.1 Japan. These derived files are therefore also offered under CC BY-SA, which satisfies both."
)
CREDITS = {
    "bp3d": {
        "short": "BodyParts3D CC BY",
        "attribution": "BodyParts3D, (c) The Database Center for Life Science licensed under CC Attribution 4.0 International",
        "licence_url": "https://dbarchive.biosciencedbc.jp/en/bodyparts3d/lic.html",
        "changes": "Selected parts of the mesh, reduced to a low polygon count, recoloured, quantised.",
        "note": SHARE_ALIKE_NOTE,
    },
    "hra": {
        "short": "HuBMAP HRA CC BY",
        "attribution": "3D Reference Organ, Human Reference Atlas v1.3, HuBMAP, licensed under CC Attribution 4.0 International",
        "licence_url": "https://creativecommons.org/licenses/by/4.0/",
        "changes": "Selected parts of the mesh, reduced to a low polygon count, recoloured, quantised.",
    },
    "schematic": {
        "short": "Schema",
        "attribution": "Simplified schematic drawn by this script from textbook cell structure. Not measured data.",
        "licence_url": "",
        "changes": "",
    },
}


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


# ---- Loading -------------------------------------------------------------------------------------------------------


def load_obj(path):
    verts, faces = [], []
    with open(path, errors="replace") as f:
        for line in f:
            if line.startswith("v "):
                verts.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("f "):
                idx = [int(tok.split("/")[0]) - 1 for tok in line.split()[1:]]
                for k in range(1, len(idx) - 1):
                    faces.append((idx[0], idx[k], idx[k + 1]))
    return np.array(verts, dtype=np.float64), np.array(faces, dtype=np.int64)


def obj_names(folder):
    """English name of each element file, from its own header."""
    names = {}
    for fn in sorted(os.listdir(folder)):
        if not fn.endswith(".obj"):
            continue
        with open(os.path.join(folder, fn), errors="replace") as f:
            for line in f:
                if line.startswith("# English name"):
                    names[fn[:-4]] = line.split(":", 1)[1].strip()
                    break
                if line.startswith("v "):
                    break
    return names


def bp3d_to_view(v):
    """BodyParts3D: x to the patient's left, y to the back, z up. View: x right, y up, z towards the viewer.
    A proper rotation, so the patient's left stays on the viewer's right and the winding stays as it was."""
    return np.column_stack([v[:, 0], v[:, 2], -v[:, 1]])


def load_glb(path):
    """Mesh per node name, world coordinates. Handles the node matrix-free files of the HRA."""
    with open(path, "rb") as f:
        raw = f.read()
    magic, version, _ = struct.unpack("<4sII", raw[:12])
    if magic != b"glTF" or version != 2:
        raise ValueError("not glTF 2")
    json_len, _ = struct.unpack("<I4s", raw[12:20])
    gltf = json.loads(raw[20:20 + json_len])
    bin_at = 20 + json_len
    bin_len = struct.unpack("<I", raw[bin_at:bin_at + 4])[0]
    blob = raw[bin_at + 8:bin_at + 8 + bin_len]
    dtypes = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}
    widths = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}

    def accessor(i):
        a = gltf["accessors"][i]
        view = gltf["bufferViews"][a["bufferView"]]
        dt = np.dtype(dtypes[a["componentType"]])
        width = widths[a["type"]]
        start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = view.get("byteStride", width * dt.itemsize)
        if stride == width * dt.itemsize:
            out = np.frombuffer(blob, dt, a["count"] * width, start)
        else:
            rows = [np.frombuffer(blob, dt, width, start + k * stride) for k in range(a["count"])]
            out = np.concatenate(rows)
        return out.reshape(-1, width) if width > 1 else out

    def local(node):
        if "matrix" in node:
            return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
        t = node.get("translation", [0, 0, 0])
        x, y, z, w = node.get("rotation", [0, 0, 0, 1])
        s = node.get("scale", [1, 1, 1])
        rot = np.array(
            [
                [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
            ]
        )
        m = np.eye(4)
        m[:3, :3] = rot * np.array(s)
        m[:3, 3] = t
        return m

    parts = {}

    def walk(i, parent):
        node = gltf["nodes"][i]
        world = parent @ local(node)
        if "mesh" in node:
            vs, fs, base = [], [], 0
            for prim in gltf["meshes"][node["mesh"]]["primitives"]:
                v = accessor(prim["attributes"]["POSITION"]).astype(np.float64)
                tri = accessor(prim["indices"]).astype(np.int64).reshape(-1, 3)
                vs.append((world[:3, :3] @ v.T).T + world[:3, 3])
                fs.append(tri + base)
                base += len(v)
            parts[node.get("name", str(i))] = (np.concatenate(vs), np.concatenate(fs))
        for child in node.get("children", []):
            walk(child, world)

    for root in gltf["scenes"][gltf.get("scene", 0)]["nodes"]:
        walk(root, np.eye(4))
    return parts


# ---- Mesh steps ----------------------------------------------------------------------------------------------------


def weld(v, f, tolerance):
    keys = np.round(v / tolerance).astype(np.int64)
    _, first, inverse = np.unique(keys, axis=0, return_index=True, return_inverse=True)
    inverse = inverse.reshape(-1)
    f2 = inverse[f]
    good = (f2[:, 0] != f2[:, 1]) & (f2[:, 1] != f2[:, 2]) & (f2[:, 0] != f2[:, 2])
    return v[first], f2[good]


def signed_volume(v, f):
    a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
    return float(np.einsum("ij,ij->", a, np.cross(b, c))) / 6.0


def face_areas(v, f):
    return 0.5 * np.linalg.norm(np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]]), axis=1)


def orient_outward(v, f):
    """Closed parts with a negative volume are inside out."""
    return f[:, [0, 2, 1]] if signed_volume(v, f) < 0 else f


def simplify(v, f, target):
    if len(f) <= target or target < 4:
        return v, f
    out = fast_simplification.simplify(v, f.astype(np.int32), target_count=int(target), agg=7)
    nv, nf = np.asarray(out[0], dtype=np.float64), np.asarray(out[1], dtype=np.int64)
    return nv, nf


def fibonacci_directions(n):
    golden = math.pi * (3 - math.sqrt(5))
    out = []
    for i in range(n):
        y = 1 - 2 * (i + 0.5) / n
        r = math.sqrt(max(0, 1 - y * y))
        out.append((math.cos(i * golden) * r, y, math.sin(i * golden) * r))
    return np.array(out)


def visible_faces(parts, directions=96, grid=220, samples=400000, seed=7):
    """Which faces can be seen from outside at all. Faces no view reaches, such as the walls between two liver
    segments, would only cost triangles and could show through. Point splatting in a depth grid."""
    rng = np.random.default_rng(seed)
    points, owner = [], []
    all_v = np.concatenate([p["v"] for p in parts])
    centre = (all_v.min(0) + all_v.max(0)) / 2
    radius = np.linalg.norm(all_v - centre, axis=1).max()
    total = sum(face_areas(p["v"], p["f"]).sum() for p in parts)
    for pi, p in enumerate(parts):
        v, f = p["v"], p["f"]
        areas = face_areas(v, f)
        count = np.maximum(1, np.round(areas / total * samples).astype(np.int64))
        idx = np.repeat(np.arange(len(f)), count)
        r1, r2 = np.sqrt(rng.random(len(idx))), rng.random(len(idx))
        a, b, c = v[f[idx, 0]], v[f[idx, 1]], v[f[idx, 2]]
        pts = (1 - r1)[:, None] * a + (r1 * (1 - r2))[:, None] * b + (r1 * r2)[:, None] * c
        points.append(pts)
        owner.append(np.stack([np.full(len(idx), pi), idx], axis=1))
    pts = (np.concatenate(points) - centre) / radius
    own = np.concatenate(owner)
    seen = np.zeros(len(pts), dtype=bool)
    cell = 2.0 / grid
    for d in fibonacci_directions(directions):
        helper = np.array([0.0, 1.0, 0.0]) if abs(d[1]) < 0.9 else np.array([1.0, 0.0, 0.0])
        u = np.cross(helper, d)
        u /= np.linalg.norm(u)
        w = np.cross(d, u)
        ix = np.clip(((pts @ u + 1) / cell).astype(np.int64), 0, grid - 1)
        iy = np.clip(((pts @ w + 1) / cell).astype(np.int64), 0, grid - 1)
        depth = pts @ d
        key = ix * grid + iy
        top = np.full(grid * grid, -9.0)
        np.maximum.at(top, key, depth)
        seen |= depth > top[key] - cell * 1.5
    keep = [np.zeros(len(p["f"]), dtype=bool) for p in parts]
    for pi, fi in own[seen]:
        keep[pi][fi] = True
    return keep


def compact(v, f):
    used, inverse = np.unique(f, return_inverse=True)
    return v[used], inverse.reshape(f.shape)


def budgets(parts, total, floor):
    """Triangles per part. Grows slower than area, so small parts such as finger bones keep their shape."""
    weights = np.array([face_areas(p["v"], p["f"]).sum() ** 0.7 for p in parts])
    wanted = np.maximum(floor, weights / weights.sum() * total)
    return (wanted * min(1.0, total / wanted.sum())).astype(int)


# ---- Models --------------------------------------------------------------------------------------------------------


def select_bp3d(folder, names, rules):
    """rules: (regex on the English name, palette index). First match wins. Every part is listed in the index."""
    parts = []
    for element, name in sorted(names.items()):
        for pattern, color in rules:
            if re.fullmatch(pattern, name, flags=re.IGNORECASE):
                v, f = load_obj(os.path.join(folder, element + ".obj"))
                parts.append({"name": name, "source": element, "v": bp3d_to_view(v), "f": f, "color": color})
                break
    return parts


def select_glb(path, rules):
    parts = []
    for name, (v, f) in load_glb(path).items():
        for pattern, color in rules:
            if re.fullmatch(pattern, name, flags=re.IGNORECASE):
                parts.append({"name": name, "source": os.path.basename(path), "v": v, "f": f, "color": color})
                break
    return parts


def rotate_x(parts, degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    for p in parts:
        v = p["v"]
        p["v"] = np.column_stack([v[:, 0], c * v[:, 1] - s * v[:, 2], s * v[:, 1] + c * v[:, 2]])


def rotate_z(parts, degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    for p in parts:
        v = p["v"]
        p["v"] = np.column_stack([c * v[:, 0] - s * v[:, 1], s * v[:, 0] + c * v[:, 1], v[:, 2]])


def build(parts, total, floor, unit_tolerance=0.01, cull=True, intermediate=6):
    prepared = []
    for p in parts:
        v, f = weld(p["v"], p["f"], unit_tolerance)
        if len(f) == 0:
            continue
        f = orient_outward(v, f)
        prepared.append({**p, "v": v, "f": f})
    budget = budgets(prepared, total, floor)
    reduced = []
    for p, b in zip(prepared, budget):
        v, f = simplify(p["v"], p["f"], int(max(b * intermediate, 400)))
        reduced.append({**p, "v": v, "f": f})
    if cull:
        keep = visible_faces(reduced)
        survivors = []
        for p, k in zip(reduced, keep):
            if k.sum() >= 4:
                v, f = compact(p["v"], p["f"][k])
                survivors.append({**p, "v": v, "f": f})
        reduced = survivors
    budget = budgets(reduced, total, floor)
    final = []
    for p, b in zip(reduced, budget):
        v, f = simplify(p["v"], p["f"], int(b))
        if len(f):
            v, f = compact(v, f)
            final.append({**p, "v": v, "f": f})
    return final


def write_model(out_dir, model_id, label_de, label_en, source_key, palette, parts, schematic=False, extra=None):
    all_v = np.concatenate([p["v"] for p in parts])
    centre = (all_v.min(0) + all_v.max(0)) / 2
    radius = np.linalg.norm(all_v - centre, axis=1).max()
    vs, fs, cs, base = [], [], [], 0
    for p in parts:
        q = np.round((p["v"] - centre) / radius * 32767).astype(np.int64)
        vs.append(q)
        fs.append(p["f"] + base)
        cs.append(np.full(len(p["f"]), p["color"], dtype=np.uint8))
        base += len(q)
    v = np.concatenate(vs)
    f = np.concatenate(fs)
    c = np.concatenate(cs)
    # Quantising can collapse tiny triangles
    good = (f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])
    f, c = f[good], c[good]
    if len(v) > 2500 or len(f) > 3500:
        raise SystemExit(f"{model_id}: {len(v)} vertices, {len(f)} triangles is over the file limit")

    def text(s, size):
        b = s.encode("utf-8")
        if len(b) >= size:
            raise SystemExit(f"{model_id}: '{s}' does not fit {size} bytes")
        return b.ljust(size, b"\0")

    short = CREDITS[source_key]["short"]
    pal = [rgb565(*rgb) for rgb in palette] + [0] * (8 - len(palette))
    head = struct.pack("<4sBBHHH", b"ZAN1", 1 if schematic else 0, len(palette), len(v), len(f), 0)
    head += text(model_id, 16) + text(label_de, 24) + text(label_en, 24) + text(short, 24)
    head += struct.pack("<8H", *pal)
    assert len(head) == 116
    body = head + v.astype("<i2").tobytes() + f.astype("<u2").tobytes() + c.astype("u1").tobytes()
    body += struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)
    path = os.path.join(out_dir, model_id + ".bin")
    with open(path, "wb") as out:
        out.write(body)
    entry = {
        "id": model_id,
        "file": model_id + ".bin",
        "label_de": label_de,
        "label_en": label_en,
        "schematic": schematic,
        "triangles": int(len(f)),
        "vertices": int(len(v)),
        "bytes": len(body),
        "source": source_key,
        "parts": sorted({p["name"] for p in parts}),
    }
    if extra:
        entry.update(extra)
    print(f"{model_id}: {len(parts)} parts, {len(f)} triangles, {len(v)} vertices, {len(body)} bytes")
    return entry


def write_index(out_dir, entries):
    used = {e["source"] for e in entries}
    index = {
        "format": "ZAN1, see sketch/sketch_jan12a/anatomy_core.h",
        "credits": {k: CREDITS[k] for k in sorted(used)},
        "models": sorted(entries, key=lambda e: e["id"]),
    }
    with open(os.path.join(out_dir, "index.json"), "w") as f:
        json.dump(index, f, indent=1, ensure_ascii=False)
        f.write("\n")


def merge_entries(out_dir, new_entries):
    path = os.path.join(out_dir, "index.json")
    entries = {}
    if os.path.exists(path):
        with open(path) as f:
            for e in json.load(f).get("models", []):
                entries[e["id"]] = e
    for e in new_entries:
        entries[e["id"]] = e
    write_index(out_dir, list(entries.values()))


# ---- Recipes -------------------------------------------------------------------------------------------------------


def recipes(args):
    partof = os.path.join(args.sources, "bp3d", "partof")
    return {
        "stomach": lambda: make_bp3d(args, partof, "stomach", "Magen", "Stomach", [(232, 150, 140)], [(r"Stomach", 0)], 900, 900),
        "kidney": lambda: make_bp3d(args, partof, "kidney", "Niere", "Kidney", [(150, 62, 58)], [(r"Right kidney", 0)], 900, 900),
    }


def make_bp3d(args, folder, model_id, de, en, palette, rules, total, floor, cull=True):
    parts = select_bp3d(folder, args.names(folder), rules)
    if not parts:
        raise SystemExit(f"{model_id}: nothing matched")
    return write_model(args.out, model_id, de, en, "bp3d", palette, build(parts, total, floor, cull=cull))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sources", required=True, help="folder with bp3d/partof, bp3d/isa and hra")
    parser.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "webpage2", "data", "anatomy"))
    parser.add_argument("models", nargs="*", help="only these ids; default all")
    args = parser.parse_args()
    cache = {}

    def names(folder):
        if folder not in cache:
            cache[folder] = obj_names(folder)
        return cache[folder]

    args.names = names
    os.makedirs(args.out, exist_ok=True)
    table = recipes(args)
    wanted = args.models or list(table)
    entries = []
    for model_id in wanted:
        if model_id not in table:
            raise SystemExit(f"unknown model {model_id}")
        entries.append(table[model_id]())
    merge_entries(args.out, entries)


if __name__ == "__main__":
    sys.exit(main())
