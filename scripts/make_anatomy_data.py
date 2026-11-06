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
        "source_url": "https://dbarchive.biosciencedbc.jp/en/bodyparts3d/download.html",
        "licence_url": "https://dbarchive.biosciencedbc.jp/en/bodyparts3d/lic.html",
        "changes": "Selected parts of the mesh, reduced to a low polygon count, recoloured, quantised.",
        "note": SHARE_ALIKE_NOTE,
    },
    "hra": {
        "short": "HuBMAP HRA CC BY",
        "attribution": "3D Reference Organ for Lung, Male, v1.3, Human Reference Atlas, HuBMAP, licensed under CC Attribution 4.0 International",
        "source_url": "https://lod.humanatlas.io/ref-organ/lung-male/v1.3/",
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
    """A closed part with a negative volume is inside out. The volume of an open part depends on where the origin is,
    so it says nothing: those keep the winding of the source."""
    edges = np.sort(f[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2), axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    closed = bool(np.all(counts == 2))
    return f[:, [0, 2, 1]] if closed and signed_volume(v, f) < 0 else f


def simplify(v, f, target):
    target = max(int(target), 4)
    if len(f) <= target:
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


def rotate_y(parts, degrees):
    """Turns the model about the vertical. The view opens at a fixed tilt, so this picks the side seen first."""
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    for p in parts:
        v = p["v"]
        p["v"] = np.column_stack([c * v[:, 0] + s * v[:, 2], v[:, 1], -s * v[:, 0] + c * v[:, 2]])


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


def write_model(out_dir, model_id, label_de, label_en, source_key, palette, parts, schematic=False, extra=None, short=None):
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

    short = short or CREDITS[source_key]["short"]
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


BONES_SKULL = (
    r"Frontal bone|(Left|Right) parietal bone|Occipital bone|(Left|Right) temporal bone|Sphenoid bone|Ethmoid|Vomer"
    r"|(Left|Right) (maxilla|zygomatic bone|nasal bone|lacrimal bone|palatine bone|inferior nasal concha)|Mandible"
)
BONES_BODY = (
    r"(Left|Right) (clavicle|scapula|humerus|radius|ulna|hip bone|femur|patella|tibia|fibula|talus|calcaneus)"
    r"|(Left|Right) (cuboid bone|(lateral|intermediate|medial) cuneiform bone)|Navicular bone of (left|right) foot"
    r"|(Left|Right) (first|second|third|fourth|fifth) (metacarpal|metatarsal) bone"
    r"|(Proximal|Middle|Distal) phalanx of (left|right) .*"
    r"|(Left|Right) (scaphoid|lunate|triquetral|pisiform|trapezium|trapezoid|capitate|hamate)"
    r"|(First|Second|Third|Fourth|Fifth|Sixth|Seventh|Eighth|Ninth|Tenth|Eleventh|Twelfth) (cervical|thoracic|lumbar) vertebra"
    r"|Atlas|Axis|Sacrum|Coccyx|Manubrium|Body of sternum|Xiphoid process"
    r"|(Left|Right) (first|second|third|fourth|fifth|sixth|seventh|eighth|ninth|tenth|eleventh|twelfth) rib"
)
HAND_BONES = (
    r"Right (first|second|third|fourth|fifth) metacarpal bone"
    r"|(Proximal|Middle|Distal) phalanx of right (thumb|index finger|middle finger|ring finger|little finger)"
    r"|Right (scaphoid|lunate|triquetral|pisiform|trapezium|trapezoid|capitate|hamate)"
)


def make_heart(args, folder):
    """Heart muscle shape and the great vessels. BodyParts3D models the ventricles by their cavities."""
    palette = [(178, 52, 52), (222, 60, 48), (64, 90, 196), (240, 140, 70)]
    rules = [
        (r"Cavity of (left|right) (ventricle|atrium)|Wall of (left|right) atrium", 0),
        (r"Ascending aorta|Arch of aorta", 1),
        (r"Pulmonary trunk|(Left|Right) pulmonary artery|Superior vena cava", 2),
        (r"Trunk of right coronary artery|Trunk of anterior interventricular branch of left coronary artery"
         r"|Circumflex branch of left coronary artery", 3),
    ]
    parts = select_bp3d(folder, args.names(folder), rules)
    return write_model(args.out, "heart", "Herz", "Heart", "bp3d", palette, build(parts, 1400, 40))


def make_lungs(args):
    path = os.path.join(args.sources, "hra", "lung-male.glb")
    parts = select_glb(
        path,
        [
            (r"VH_M_left_.*bronchopulmonary_segment", 0),
            (r"VH_M_right_.*bronchopulmonary_segment", 0),
            (r"VH_M_hilum.*", 1),
        ],
    )
    return write_model(args.out, "lungs", "Lunge", "Lungs", "hra", [(232, 150, 150), (150, 70, 80)],
                       build(parts, 1400, 20, unit_tolerance=1e-5))


def make_brain(args, folder):
    palette = [(232, 176, 168), (205, 130, 130), (190, 150, 120), (178, 120, 120)]
    rules = [
        (r"(Left|Right) (superior frontal gyrus|middle frontal gyrus|inferior frontal gyrus|precentral gyrus|postcentral gyrus"
         r"|superior parietal lobule|angular gyrus|supramarginal gyrus|occipital lobe|superior temporal gyrus"
         r"|middle temporal gyrus|inferior temporal gyrus|fusiform gyrus|parahippocampal gyrus|cingulate gyrus|insula)", 0),
        (r"White matter of (left|right) cerebral hemisphere", 3),
        (r"Cerebellum", 1),
        (r"Pons|Midbrain|Medulla oblongata", 2),
    ]
    parts = select_bp3d(folder, args.names(folder), rules)
    rotate_y(parts, 60)  # Opens side-on: the view in which a brain looks most like one
    return write_model(args.out, "brain", "Gehirn", "Brain", "bp3d", palette, build(parts, 1500, 30))


def make_liver(args, folder):
    rules = [(r"Hepatovenous segment .*|Caudate lobe of liver", 0)]
    parts = select_bp3d(folder, args.names(folder), rules)
    return write_model(args.out, "liver", "Leber", "Liver", "bp3d", [(136, 50, 42)], build(parts, 1200, 40))


def make_skeleton(args, folder):
    palette = [(236, 226, 202), (176, 196, 206)]
    rules = [(BONES_SKULL, 0), (BONES_BODY, 0), (r"(Left|Right) (first|second|third|fourth|fifth|sixth|seventh) costal cartilage", 1)]
    parts = select_bp3d(folder, args.names(folder), rules)
    return write_model(args.out, "skeleton", "Skelett", "Skeleton", "bp3d", palette, build(parts, 2000, 6))


def make_skull(args, folder):
    palette = [(236, 226, 202), (250, 250, 240)]
    parts = select_bp3d(folder, args.names(folder), [(BONES_SKULL, 0)])
    isa = os.path.join(args.sources, "bp3d", "isa")
    parts += select_bp3d(isa, args.names(isa), [(r".*secondary (central |lateral )?(incisor|canine|premolar|first premolar|second premolar|molar) tooth|.*secondary (molar|premolar) tooth", 1)])
    return write_model(args.out, "skull", "Schädel", "Skull", "bp3d", palette, build(parts, 1500, 10),
                       short="BodyParts3D")  # The umlaut needs the wider font: with "CC BY" the name itself would be cut


def make_eye(args, folder):
    """The clear cornea would hide what makes an eye an eye, so it's left out and the iris shows through the opening."""
    palette = [(240, 240, 232), (58, 112, 170), (16, 16, 20), (150, 52, 40)]
    rules = [(r"Right sclera", 0), (r"Right iris", 1), (r"Right lens", 2)]
    parts = select_bp3d(folder, args.names(folder), rules)
    rotate_y(parts, -22)  # Opens looking almost at you
    return write_model(args.out, "eye", "Auge", "Eye", "bp3d", palette, build(parts, 1000, 120, cull=False))


def make_hand(args, folder):
    parts = select_bp3d(folder, args.names(folder), [(HAND_BONES, 0)])
    rotate_z(parts, 180)  # Fingers up: the model stands in anatomical position with the hands hanging down
    rotate_y(parts, -22)  # Opens with the flat of the hand towards you
    return write_model(args.out, "hand", "Handknochen", "Hand bones", "bp3d", [(236, 226, 202)], build(parts, 1200, 16))


# ---- Schematic cells -----------------------------------------------------------------------------------------------
# No real scan of these exists in a usable form. They're drawn from what every textbook describes: which parts a cell
# has and roughly where. Sizes and positions are not measurements, so the file carries the schematic flag and the screen
# says "Vereinfachtes Schema" / "Simplified schematic" the whole time. Fixed numbers below, no randomness and no AI.


def uv_sphere(centre, radii, rows, cols):
    verts = [(0.0, 1.0, 0.0)]
    for r in range(1, rows):
        polar = math.pi * r / rows
        for c in range(cols):
            az = 2 * math.pi * c / cols
            verts.append((math.sin(polar) * math.cos(az), math.cos(polar), math.sin(polar) * math.sin(az)))
    verts.append((0.0, -1.0, 0.0))
    faces = []
    for c in range(cols):
        faces.append((0, 1 + (c + 1) % cols, 1 + c))
    for r in range(rows - 2):
        for c in range(cols):
            a = 1 + r * cols + c
            b = 1 + r * cols + (c + 1) % cols
            d = a + cols
            e = b + cols
            faces.append((a, b, e))
            faces.append((a, e, d))
    last = len(verts) - 1
    base = 1 + (rows - 2) * cols
    for c in range(cols):
        faces.append((last, base + c, base + (c + 1) % cols))
    v = np.array(verts) * np.array(radii) + np.array(centre)
    return v, np.array(faces, dtype=np.int64)


def align_y(v, f, direction):
    """Turns a shape built along y to point along direction."""
    d = np.array(direction, dtype=np.float64)
    d /= np.linalg.norm(d)
    y = np.array([0.0, 1.0, 0.0])
    axis = np.cross(y, d)
    s = np.linalg.norm(axis)
    c = float(y @ d)
    if s < 1e-9:
        return (v if c > 0 else v * np.array([1, -1, -1])), f
    k = axis / s
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    R = np.eye(3) + s * K + (1 - c) * K @ K
    return (R @ v.T).T, f


def ellipsoid(centre, radii, rows, cols, direction=None):
    v, f = uv_sphere((0, 0, 0), radii, rows, cols)
    if direction is not None:
        v, f = align_y(v, f, direction)
    return v + np.array(centre), f


def tube(points, radii, sides):
    """A closed tube through points, with a radius at each."""
    points = [np.array(p, dtype=np.float64) for p in points]
    verts, faces = [], []
    frame = None
    for i, p in enumerate(points):
        t = points[min(i + 1, len(points) - 1)] - points[max(i - 1, 0)]
        t /= np.linalg.norm(t)
        if frame is None:
            helper = np.array([0.0, 0.0, 1.0]) if abs(t[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
            frame = np.cross(t, helper)
        frame = frame - t * (frame @ t)
        frame /= np.linalg.norm(frame)
        other = np.cross(t, frame)
        for s in range(sides):
            a = 2 * math.pi * s / sides
            verts.append(p + (math.cos(a) * frame + math.sin(a) * other) * radii[i])
    n = len(points)
    for i in range(n - 1):
        for s in range(sides):
            a = i * sides + s
            b = i * sides + (s + 1) % sides
            faces.append((a, a + sides, b + sides))
            faces.append((a, b + sides, b))
    start = len(verts)
    verts.append(points[0])
    verts.append(points[-1])
    for s in range(sides):
        faces.append((start, s, (s + 1) % sides))
        faces.append((start + 1, (n - 1) * sides + (s + 1) % sides, (n - 1) * sides + s))
    return np.array(verts), np.array(faces, dtype=np.int64)


def surface_of_revolution(profile, cols):
    """Closed surface from (radius, height) points running top to bottom on the axis; radius 0 at both ends."""
    rows = len(profile)
    verts = [(0.0, profile[0][1], 0.0)]
    for r, h in profile[1:-1]:
        for c in range(cols):
            a = 2 * math.pi * c / cols
            verts.append((r * math.cos(a), h, r * math.sin(a)))
    verts.append((0.0, profile[-1][1], 0.0))
    faces = []
    for c in range(cols):
        faces.append((0, 1 + (c + 1) % cols, 1 + c))
    for r in range(rows - 3):
        for c in range(cols):
            a = 1 + r * cols + c
            b = 1 + r * cols + (c + 1) % cols
            d = a + cols
            e = b + cols
            faces.append((a, b, e))
            faces.append((a, e, d))
    last = len(verts) - 1
    base = 1 + (rows - 3) * cols
    for c in range(cols):
        faces.append((last, base + c, base + (c + 1) % cols))
    return np.array(verts), np.array(faces, dtype=np.int64)


def part(name, mesh, color):
    return {"name": name, "source": "schematic", "v": mesh[0], "f": mesh[1], "color": color}


def cut_shell(radii, rows, cols, thickness, keep_back_of=0.25):
    """Cell membrane as a bowl: the front is cut away so the parts inside can be seen. A thick wall with an outer and
    an inner face, closed at the rim, so culling works from every side."""
    outer_v, outer_f = uv_sphere((0, 0, 0), radii, rows, cols)
    limit = keep_back_of * radii[2]
    keep = outer_v[outer_f].mean(axis=1)[:, 2] < limit
    fo = outer_f[keep]
    inner_scale = 1.0 - thickness
    inner_v = outer_v * inner_scale
    fi = fo[:, [0, 2, 1]] + len(outer_v)
    v = np.concatenate([outer_v, inner_v])
    edges = {}
    for tri in fo:
        for k in range(3):
            a, b = int(tri[k]), int(tri[(k + 1) % 3])
            edges[(a, b)] = True
    rim = []
    n = len(outer_v)
    for (a, b) in edges:
        if (b, a) not in edges:
            rim.append((a, n + a, n + b))
            rim.append((a, n + b, b))
    f = np.concatenate([fo, fi, np.array(rim, dtype=np.int64)])
    return v, f


def merge(meshes):
    vs, fs, base = [], [], 0
    for v, f in meshes:
        vs.append(v)
        fs.append(f + base)
        base += len(v)
    return np.concatenate(vs), np.concatenate(fs)


def make_animal_cell(args):
    palette = [(240, 190, 150), (120, 70, 150), (70, 40, 110), (226, 96, 60), (120, 180, 210), (240, 220, 110)]
    parts = [part("cell membrane, cut open", cut_shell((1.0, 0.88, 0.95), 12, 20, 0.05), 0)]
    parts.append(part("nucleus", ellipsoid((-0.12, 0.05, -0.15), (0.34, 0.32, 0.34), 8, 12), 1))
    parts.append(part("nucleolus", ellipsoid((-0.05, 0.1, 0.13), (0.11, 0.11, 0.11), 5, 7), 2))
    for i, (c, d) in enumerate([((0.48, 0.32, 0.05), (1, 1, 0)), ((0.55, -0.22, -0.1), (1, -0.4, 0.3)),
                                ((-0.55, -0.3, 0.1), (1, 0.6, 0)), ((0.1, -0.52, 0.12), (1, 0.1, 0.4))]):
        parts.append(part(f"mitochondrion {i + 1}", ellipsoid(c, (0.07, 0.19, 0.07), 5, 7, direction=d), 3))
    # Endoplasmic reticulum as flat sacs around the nucleus
    for i, (c, r) in enumerate([((-0.55, 0.32, -0.02), 0.22), ((-0.42, 0.5, -0.05), 0.2), ((0.18, 0.5, -0.05), 0.18)]):
        parts.append(part(f"endoplasmic reticulum {i + 1}", ellipsoid(c, (r, 0.035, 0.14), 4, 8), 4))
    for i, c in enumerate([(0.32, 0.02, 0.28), (0.38, -0.42, 0.3), (-0.3, -0.5, 0.25), (-0.62, 0.02, 0.22), (0.25, 0.3, 0.32)]):
        parts.append(part(f"vesicle {i + 1}", ellipsoid(c, (0.05, 0.05, 0.05), 3, 5), 5))
    return write_model(args.out, "animal-cell", "Tierzelle", "Animal cell", "schematic", palette, parts, schematic=True,
                       extra={"schematic_of": "Which parts an animal cell has and roughly where, as in school textbooks. Not measured."})


def make_plant_cell(args):
    palette = [(90, 150, 70), (178, 220, 150), (120, 70, 150), (70, 40, 110), (60, 170, 70), (120, 180, 230), (226, 96, 60)]
    # Box-shaped cell wall, cut open at the front, with an inner membrane colour
    size = (1.0, 0.68, 0.5)
    wall = []
    # Back and four sides as solid slabs
    t = 0.05
    def slab(lo, hi):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        v = np.array([[x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0], [x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]], dtype=np.float64)
        f = np.array([[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [2, 3, 7], [2, 7, 6], [0, 4, 7], [0, 7, 3], [1, 2, 6], [1, 6, 5]], dtype=np.int64)
        return v, f
    sx, sy, sz = size
    wall.append(slab((-sx, -sy, -sz), (sx, sy, -sz + t)))
    wall.append(slab((-sx, -sy, -sz), (-sx + t, sy, sz * 0.3)))
    wall.append(slab((sx - t, -sy, -sz), (sx, sy, sz * 0.3)))
    wall.append(slab((-sx, sy - t, -sz), (sx, sy, sz * 0.3)))
    wall.append(slab((-sx, -sy, -sz), (sx, -sy + t, sz * 0.3)))
    parts = [part("cell wall, cut open", merge(wall), 0)]
    parts.append(part("vacuole", ellipsoid((0.2, 0.0, -0.08), (0.58, 0.44, 0.28), 8, 14), 5))
    parts.append(part("nucleus", ellipsoid((-0.62, 0.22, 0.0), (0.2, 0.2, 0.2), 7, 10), 2))
    parts.append(part("nucleolus", ellipsoid((-0.6, 0.24, 0.17), (0.06, 0.06, 0.06), 4, 6), 3))
    for i, (c, d) in enumerate([((-0.62, -0.32, 0.12), (1, 0.3, 0)), ((-0.25, -0.52, 0.14), (1, 0.1, 0.2)), ((0.25, -0.54, 0.14), (1, -0.1, 0)),
                                ((0.72, -0.42, 0.1), (0.3, 1, 0)), ((0.82, 0.05, 0.12), (0.1, 1, 0)), ((0.7, 0.5, 0.12), (1, 0.6, 0)),
                                ((0.2, 0.54, 0.14), (1, 0, 0.1)), ((-0.25, 0.54, 0.14), (1, 0.1, 0))]):
        parts.append(part(f"chloroplast {i + 1}", ellipsoid(c, (0.06, 0.13, 0.06), 4, 7, direction=d), 4))
    for i, (c, d) in enumerate([((-0.28, -0.12, 0.14), (1, 0.8, 0)), ((-0.82, -0.02, 0.1), (0.2, 1, 0))]):
        parts.append(part(f"mitochondrion {i + 1}", ellipsoid(c, (0.04, 0.09, 0.04), 4, 6, direction=d), 6))
    return write_model(args.out, "plant-cell", "Pflanzenzelle", "Plant cell", "schematic", palette, parts, schematic=True,
                       extra={"schematic_of": "Which parts a plant cell has and roughly where, as in school textbooks. Not measured."})


def make_red_blood_cell(args):
    """Biconcave disc after the formula of Evans and Fung (1972), a fit to measured cells. Still schematic: one idealised
    shape, not a scan. z(r) = +-0.5 * sqrt(1 - s^2) * (c0 + c1 s^2 + c2 s^4), s = r / R, R = 3.91 micrometres.
    c1: one paper seen (arXiv 1605.04509) prints 7.38. 7.83 is used because it reproduces the reported cell height of
    2.58 micrometres, where 7.38 gives 2.41. The original paper was not read."""
    R, c0, c1, c2 = 3.91, 0.81, 7.83, -4.39
    steps = 12
    top = []
    for i in range(steps + 1):
        s = math.sin(i / steps * math.pi / 2)  # More points at the rim, where it curves fastest
        h = 0.5 * math.sqrt(max(0.0, 1 - s * s)) * (c0 + c1 * s ** 2 + c2 * s ** 4)
        top.append((s * R, h))
    profile = top[:-1] + [top[-1]] + [(r, -h) for r, h in reversed(top[:-1])]
    v, f = surface_of_revolution(profile, 22)
    # Stand it up a little: side-on it reads as a disc with a dimple
    parts = [part("biconcave disc", (v, f), 0)]
    rotate_x(parts, 62)
    return write_model(args.out, "red-blood-cell", "Rote Blutzelle", "Red blood cell", "schematic", [(205, 40, 44)], parts, schematic=True,
                       extra={"schematic_of": "Idealised shape from the Evans and Fung (1972) formula, fitted to measured cells. Not a scan."})


def make_neuron(args):
    palette = [(150, 110, 200), (70, 40, 110), (235, 225, 190), (240, 190, 90)]
    parts = [part("cell body", ellipsoid((-0.62, 0.0, 0.0), (0.17, 0.15, 0.15), 7, 10), 0)]
    parts.append(part("nucleus", ellipsoid((-0.62, 0.03, 0.12), (0.06, 0.06, 0.05), 4, 6), 1))
    # Dendrites: fixed branching, short and getting thinner
    dendrites = [
        [(-0.70, 0.12, 0.02), (-0.82, 0.30, 0.05), (-0.92, 0.44, 0.10), (-0.98, 0.56, 0.08)],
        [(-0.82, 0.30, 0.05), (-0.72, 0.46, -0.02), (-0.68, 0.60, -0.06)],
        [(-0.78, 0.02, 0.05), (-0.94, 0.06, 0.12), (-1.06, 0.16, 0.16)],
        [(-0.94, 0.06, 0.12), (-1.04, -0.04, 0.06), (-1.14, -0.10, 0.02)],
        [(-0.70, -0.11, 0.02), (-0.80, -0.28, -0.06), (-0.94, -0.42, -0.08), (-1.00, -0.54, -0.12)],
        [(-0.80, -0.28, -0.06), (-0.70, -0.46, -0.02), (-0.66, -0.58, 0.03)],
        [(-0.58, 0.14, -0.06), (-0.50, 0.32, -0.12), (-0.40, 0.44, -0.14)],
        [(-0.60, -0.14, -0.06), (-0.50, -0.30, 0.04), (-0.38, -0.40, 0.08)],
    ]
    for i, pts in enumerate(dendrites):
        radii = [0.032 * (1 - 0.65 * k / (len(pts) - 1)) for k in range(len(pts))]
        parts.append(part(f"dendrite {i + 1}", tube(pts, radii, 5), 0))
    # Axon, thin between the myelin sheaths
    def axon_at(x):
        return (x, 0.05 * math.sin(x * 4.0), 0.03 * math.cos(x * 3.0))
    xs = [-0.46 + i * 0.1 for i in range(13)]
    parts.append(part("axon", tube([axon_at(x) for x in xs], [0.016] * len(xs), 5), 0))
    for i in range(5):
        a, b = -0.40 + i * 0.2, -0.40 + i * 0.2 + 0.16
        pts = [axon_at(a + (b - a) * k / 4) for k in range(5)]
        parts.append(part(f"myelin sheath {i + 1}", tube(pts, [0.03, 0.05, 0.052, 0.05, 0.03], 7), 2))
    # Axon terminals
    end = axon_at(0.74)
    tips = [(0.98, 0.26, 0.08), (1.04, 0.08, -0.08), (1.02, -0.12, 0.08), (0.94, -0.28, -0.05)]
    for i, tip in enumerate(tips):
        mid = ((end[0] + tip[0]) / 2, (end[1] + tip[1]) / 2 + 0.02, (end[2] + tip[2]) / 2)
        parts.append(part(f"axon branch {i + 1}", tube([end, mid, tip], [0.016, 0.013, 0.011], 4), 0))
        parts.append(part(f"axon terminal {i + 1}", ellipsoid(tip, (0.035, 0.035, 0.035), 3, 5), 3))
    return write_model(args.out, "neuron", "Nervenzelle", "Neuron", "schematic", palette, parts, schematic=True,
                       extra={"schematic_of": "Cell body, dendrites, axon with myelin, terminals, as in school textbooks. Not measured."})


def recipes(args):
    partof = os.path.join(args.sources, "bp3d", "partof")
    return {
        "stomach": lambda: make_bp3d(args, partof, "stomach", "Magen", "Stomach", [(232, 150, 140)], [(r"Stomach", 0)], 900, 900),
        "kidney": lambda: make_bp3d(args, partof, "kidney", "Niere", "Kidney", [(150, 62, 58)], [(r"Right kidney", 0)], 900, 900),
        "liver": lambda: make_liver(args, partof),
        "heart": lambda: make_heart(args, partof),
        "lungs": lambda: make_lungs(args),
        "brain": lambda: make_brain(args, partof),
        "skeleton": lambda: make_skeleton(args, partof),
        "skull": lambda: make_skull(args, partof),
        "eye": lambda: make_eye(args, partof),
        "hand": lambda: make_hand(args, partof),
        "animal-cell": lambda: make_animal_cell(args),
        "plant-cell": lambda: make_plant_cell(args),
        "red-blood-cell": lambda: make_red_blood_cell(args),
        "neuron": lambda: make_neuron(args),
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
