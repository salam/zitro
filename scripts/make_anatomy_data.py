#!/usr/bin/env python3
"""Builds webpage2/data/anatomy/ for Zitro: real organ meshes reduced to low-poly files the stick can draw.

Sources (see docs/features/anatomy.md for licences; downloaded by hand into --sources):
  bp3d/partof/*.obj  BodyParts3D 4.0, PART-OF tree: partof_BP3D_4.0_obj_99.zip
  bp3d/isa/*.obj     BodyParts3D 4.0, IS-A tree: isa_BP3D_4.0_obj_99.zip (teeth only)
  hra/*.glb          Human Reference Atlas: lung-male.glb, heart-male.glb, kidney-male-right.glb,
                     renal-pelvis-male-right.glb, eye-male-right.glb (renamed from the 3d-vh-m-*.glb on the CDN)

Parts are chosen by name from a closed whitelist, so nothing else in those large sets (which also hold the
reproductive organs of the male body they were modelled on) can end up in a file.

Pipeline for an outside model: load parts -> weld -> orient outward -> reduce -> drop what can't be seen from outside
-> reduce to budget -> centre, scale to radius 1 -> quantise -> write and re-check.

Pipeline for a cutaway ("<id>-cut", or a cell): weld -> orient -> turn so the cut is z = 0 -> reduce -> cut -> fill the
cut of each closed solid -> drop what no allowed view can reach -> quantise. Reducing comes first, so the fill keeps
meeting the edges of the cut. Only what the source has is shown: a part is filled only if it's a closed solid whose cut
closes into loops, and anything else stays open.

Needs numpy, fast-simplification and mapbox-earcut. Files read from --sources are untrusted: run with python -I.
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
import mapbox_earcut
import numpy as np

SHARE_ALIKE_NOTE = (
    "The BodyParts3D licence page (updated 2025-02-27) says CC BY 4.0. The headers inside the OBJ files, dated 2013, "
    "still say CC BY-SA 2.1 Japan. These derived files are therefore also offered under CC BY-SA 4.0 "
    "(https://creativecommons.org/licenses/by-sa/4.0/), which satisfies both: clause 5.2 of the 2.1 Japan licence allows "
    "a later version with the same licence elements."
)


def hra_credit(what, path, creators, cut=False):
    changes = "Selected parts of the mesh, reduced to a low polygon count, recoloured, quantised."
    if cut:
        changes += " Cut open along a plane; the cut of each closed part filled."
    return {
        "short": "HuBMAP HRA CC BY",
        "attribution": f"3D Reference Organ for {what}, by {creators}, Human Reference Atlas, HuBMAP, licensed under CC Attribution 4.0 International",
        "source_url": f"https://lod.humanatlas.io/ref-organ/{path}",
        "licence_url": "https://creativecommons.org/licenses/by/4.0/",
        "changes": changes,
    }


CREDITS = {
    "bp3d": {
        "short": "BodyParts3D CC BY",
        "attribution": "BodyParts3D, (c) The Database Center for Life Science licensed under CC Attribution 4.0 International",
        "source_url": "https://dbarchive.biosciencedbc.jp/en/bodyparts3d/download.html",
        "licence_url": "https://dbarchive.biosciencedbc.jp/en/bodyparts3d/lic.html",
        "changes": "Selected parts of the mesh, reduced to a low polygon count, recoloured, quantised. Cutaways: cut open along a plane; the cut of each closed part filled.",
        "note": SHARE_ALIKE_NOTE,
    },
    "hra": hra_credit("Lung, Male, v1.3", "lung-male/v1.3", "Kristen Browne, Heidi Schlehlein"),
    "hra-heart": hra_credit("Heart, Male, v1.3", "heart-male/v1.3", "Kristen Browne", cut=True),
    "hra-kidney": hra_credit(
        "Kidney, Male, Right, v1.3 and 3D Reference Organ for Renal Pelvis, Male, Right, v1.0",
        "kidney-male-right/v1.3 and https://lod.humanatlas.io/ref-organ/renal-pelvis-male-right/v1.0",
        "Kristen Browne, Heidi Schlehlein",
        cut=True,
    ),
    "hra-eye": hra_credit("Eye, Male, Right, v1.3", "eye-male-right/v1.3", "Kristen Browne, Heidi Schlehlein", cut=True),
    "schematic": {
        "short": "Schema",
        "attribution": "Simplified schematic drawn by this script from textbook cell structure. Not measured data.",
        "licence_url": "",
        "changes": "",
    },
}


NOTE = 0xFF  # A legend line without a colour chip


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


def visible_faces(parts, directions=None, grid=220, samples=400000, seed=7):
    """Which faces can be seen from outside at all. Faces no view reaches, such as the walls between two liver
    segments, would only cost triangles and could show through. Point splatting in a depth grid.
    directions: towards the viewer; every way round if not given."""
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
    for d in fibonacci_directions(96) if directions is None else directions:
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
    weights = np.array([face_areas(p["v"], p["f"]).sum() ** 0.7 * p.get("weight", 1.0) for p in parts])
    wanted = np.maximum(floor, weights / weights.sum() * total)
    return (wanted * min(1.0, total / wanted.sum())).astype(int)


# ---- Cutting ---------------------------------------------------------------------------------------------------------
# Everything here works in a frame where the cut is the plane z = 0 and the viewer looks from +z at what's behind it.

CUT_VIEW_LIMIT_DEG = 55  # Same as ANATOMY_CUT_LIMIT in anatomy_core.h: how far the view can turn away from the cut


def cut_frame(origin, normal, up):
    """Rows x, y, n: a proper rotation, so windings stay as they were."""
    n = np.array(normal, dtype=np.float64)
    n /= np.linalg.norm(n)
    y = np.array(up, dtype=np.float64)
    y -= n * (y @ n)
    y /= np.linalg.norm(y)
    return np.array(origin, dtype=np.float64), np.stack([np.cross(y, n), y, n])


def clip_behind(v, f):
    """Keeps z < 0. Returns vertices, faces and the directed edges along the cut. Neighbours share the new vertices."""
    d = v[:, 2].copy()
    d[np.abs(d) < 1e-9] = -1e-9  # Nothing lies exactly on the plane
    verts = [p for p in v]
    made = {}

    def crossing(a, b):
        key = (a, b) if a < b else (b, a)
        if key not in made:
            t = d[a] / (d[a] - d[b])
            q = v[a] + (v[b] - v[a]) * t
            q[2] = 0.0
            made[key] = len(verts)
            verts.append(q)
        return made[key]

    faces, edges = [], []
    for tri in f:
        behind = d[tri] < 0
        count = int(behind.sum())
        if count == 3:
            faces.append((int(tri[0]), int(tri[1]), int(tri[2])))
        elif count:
            odd = int(np.argmax(behind if count == 1 else ~behind))
            a, b, c = int(tri[odd]), int(tri[(odd + 1) % 3]), int(tri[(odd + 2) % 3])
            ab, ca = crossing(a, b), crossing(c, a)
            if count == 1:
                faces.append((a, ab, ca))
                edges.append((ab, ca))
            else:
                faces.append((ab, b, c))
                faces.append((ab, c, ca))
                edges.append((ca, ab))
    return np.array(verts), np.array(faces, dtype=np.int64).reshape(-1, 3), edges


def cut_loops(edges):
    """Chains the edges of a cut into closed loops. None if any edge is left over: then the part isn't watertight here."""
    following = {}
    for a, b in edges:
        following.setdefault(a, []).append(b)
    loops = []
    while following:
        start = next(iter(following))
        loop = [start]
        while True:
            options = following.get(loop[-1])
            if not options:
                return None
            n = options.pop()
            if not options:
                del following[loop[-1]]
            if n == start:
                break
            if n in loop:  # Pinched: a figure of eight. Close off the smaller ring.
                at = loop.index(n)
                ring = loop[at:]
                if len(ring) >= 3:
                    loops.append(ring)
                loop = loop[: at + 1]
                continue
            loop.append(n)
        if len(loop) >= 3:
            loops.append(loop)
    return loops


def polygon_area(p):
    return 0.5 * float(p[:, 0] @ np.roll(p[:, 1], -1) - p[:, 1] @ np.roll(p[:, 0], -1))


def point_in_polygon(pt, poly):
    xs, ys = poly[:, 0], poly[:, 1]
    xn, yn = np.roll(xs, -1), np.roll(ys, -1)
    span = np.where(yn == ys, 1e-30, yn - ys)
    return int((((ys > pt[1]) != (yn > pt[1])) & (pt[0] < (xn - xs) * (pt[1] - ys) / span + xs)).sum()) % 2 == 1


def fill_loops(v, loops):
    """Faces towards +z over the area inside the loops, holes by the even-odd rule."""
    polys = [v[np.array(loop)][:, :2] for loop in loops]
    order = sorted(range(len(loops)), key=lambda i: -abs(polygon_area(polys[i])))
    depth, parent = {}, {}
    for pos, i in enumerate(order):
        depth[i], parent[i] = 0, None
        for j in order[:pos]:
            if point_in_polygon(polys[i][0], polys[j]):
                depth[i] += 1
                if depth[j] % 2 == 0:
                    parent[i] = j
    faces = []
    for i in order:
        if depth[i] % 2:
            continue
        holes = [j for j in order if depth[j] % 2 and parent[j] == i]
        ids = list(loops[i])
        rings = [len(ids)]
        for h in holes:
            ids += loops[h]
            rings.append(len(ids))
        pts = v[np.array(ids)][:, :2]
        tris = mapbox_earcut.triangulate_float64(pts, np.array(rings, dtype=np.uint32)).reshape(-1, 3)
        for t in tris:
            a, b, c = int(ids[t[0]]), int(ids[t[1]]), int(ids[t[2]])
            area = (v[b, 0] - v[a, 0]) * (v[c, 1] - v[a, 1]) - (v[b, 1] - v[a, 1]) * (v[c, 0] - v[a, 0])
            faces.append((a, b, c) if area > 0 else (a, c, b))
    return np.array(faces, dtype=np.int64).reshape(-1, 3)


def is_closed(f):
    """No edge belongs to only one face. A pinched edge shared by four is still closed: it comes from the reduction, and
    the loops of the cut are checked again afterwards."""
    if len(f) == 0:
        return False
    edges = np.sort(f[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2), axis=1)
    return bool(np.all(np.unique(edges, axis=0, return_counts=True)[1] >= 2))


def cone_directions(limit_deg, count=240):
    all_directions = fibonacci_directions(count)
    return all_directions[all_directions[:, 2] >= math.cos(math.radians(limit_deg))]


def most_facing(parts, limit_deg):
    """The most triangles turned to the viewer in any allowed view: what the stick has to draw."""
    normals = []
    for p in parts:
        a, b, c = p["v"][p["f"][:, 0]], p["v"][p["f"][:, 1]], p["v"][p["f"][:, 2]]
        normals.append(np.cross(b - a, c - a))
    normals = np.concatenate(normals)
    return int(max(((normals @ d) > 0).sum() for d in cone_directions(limit_deg)))


def cut_once(prepared, total, floor):
    """One go at the budget. Returns shells, then fills in the order of the parts, so later parts paint over earlier."""
    budget = budgets(prepared, total, floor)
    shells, fills, left_open = [], [], []
    for p, b in zip(prepared, budget):
        v, f = simplify(p["v"], p["f"], int(b))
        mode = p["mode"]
        if mode == "whole":
            shells.append({**p, "v": v, "f": f})
            continue
        v, f = weld(v, f, 1e-7)
        v2, f2, edges = clip_behind(v, f)
        if len(f2) == 0:
            continue
        loops = cut_loops(edges) if mode == "solid" and is_closed(f) and edges else None
        if mode == "solid" and edges and loops is None:
            left_open.append(p["name"])
        if mode == "shell":  # Tissue left out on purpose: its far side, seen from inside, stays as the backdrop
            f2 = np.concatenate([f2, f2[:, [0, 2, 1]]])
        filled = fill_loops(v2, loops) if loops else None
        if "shell_filter" in p:
            f2 = f2[p["shell_filter"](v2[f2].mean(axis=1))]
        shells.append({**p, "v": v2, "f": f2})
        if loops:
            fills.append({**p, "v": v2, "f": filled, "fill": True, "color": p.get("fill_color", p["color"])})
    return shells, fills, left_open


def build_cut(parts, origin, axes, total, floor, drawn_limit=660, limit_deg=CUT_VIEW_LIMIT_DEG):
    """parts: dicts with name, v, f, color and mode: solid (cut, and filled if closed), shell (cut, not filled, both
    sides, for tissue left out on purpose), lining (cut, not filled, as it is) or whole (not cut, for what the cut would
    only spoil). shell_filter: drops surfaces after the fill is made, for a wall lined in another colour."""
    prepared = []
    for p in parts:
        v, f = weld(p["v"], p["f"], p.get("tolerance", 1e-5))
        if len(f) == 0:
            continue
        if p["mode"] != "lining":  # A lining faces inwards on purpose
            f = orient_outward(v, f)
        prepared.append({**p, "v": (v - origin) @ axes.T, "f": f})
    size = np.abs(np.concatenate([p["v"] for p in prepared])).max()
    for _ in range(6):
        shells, fills, left_open = cut_once(prepared, total, floor)
        # Only faces that no allowed view can reach go; a fill always stays
        everything = shells + fills
        keep = visible_faces(everything, directions=cone_directions(limit_deg))
        kept = []
        for p, k in zip(shells, keep[: len(shells)]):
            if k.sum() >= 1:
                v, f = compact(p["v"], p["f"][k])
                kept.append({**p, "v": v, "f": f})
        result = kept + [{**p, **dict(zip(("v", "f"), compact(p["v"], p["f"])))} for p in fills if len(p["f"])]
        drawn = most_facing(result, limit_deg)
        if drawn <= drawn_limit:
            break
        total = int(total * max(0.6, drawn_limit / drawn * 0.97))
    if left_open:
        print(f"  left open, not closed: {sorted(set(left_open))}")
    print(f"  budget {total}, most facing {drawn}, size {size:.3f}")
    return result, sorted(set(left_open))


def write_cut(args, model_id, label_de, label_en, source_key, palette, parts, legend, schematic=False, left_open=(), extra=None,
              file_suffix="-cut", short=None):
    return write_model(args.out, model_id, label_de, label_en, source_key, palette, parts, schematic=schematic, extra=extra,
                       short=short, cut={"file": model_id + file_suffix, "legend": legend, "open": left_open})


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


# DejaVu12 on the stick: pixels per letter from space to tilde, from the M5GFX font file.
DEJAVU12_ADVANCE = (
    5, 6, 6, 11, 9, 12, 11, 4, 6, 6, 7, 11, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 5, 5, 11, 11, 11, 7,
    14, 9, 9, 9, 10, 9, 8, 10, 10, 4, 4, 8, 7, 11, 10, 10, 9, 10, 9, 9, 8, 10, 9, 12, 8, 8, 10, 6, 5, 6, 11, 7,
    7, 9, 9, 8, 9, 9, 5, 9, 9, 4, 4, 8, 4, 12, 9, 9, 9, 9, 6, 8, 6, 9, 7, 10, 7, 7, 6, 9, 5, 9, 11,
)
LEGEND_WORD_PIXELS = 240 - 124  # Same as ANATOMY_LEGEND_X, chip and gap in anatomy_view.h
LEGEND_NOTE_PIXELS = 240 - 112


def write_model(out_dir, model_id, label_de, label_en, source_key, palette, parts, schematic=False, extra=None, short=None,
                cut=None):
    """cut: {"file": name without .bin, "front_from": first filled triangle, "legend": [(colour, de, en)], colour or NOTE}.
    Without cut: a plain outside view."""
    all_v = np.concatenate([p["v"] for p in parts])
    centre = (all_v.min(0) + all_v.max(0)) / 2
    radius = np.linalg.norm(all_v - centre, axis=1).max()
    vs, fs, cs, base = [], [], [], 0
    fills = 0
    for p in parts:
        q = np.round((p["v"] - centre) / radius * 32767).astype(np.int64)
        vs.append(q)
        fs.append(p["f"] + base)
        cs.append(np.full(len(p["f"]), p["color"], dtype=np.uint8))
        base += len(q)
    v = np.concatenate(vs)
    f = np.concatenate(fs)
    c = np.concatenate(cs)
    is_fill = np.concatenate([np.full(len(p["f"]), bool(p.get("fill"))) for p in parts])
    # Quantising can collapse tiny triangles
    good = (f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])
    f, c, is_fill = f[good], c[good], is_fill[good]
    if len(v) > 2500 or len(f) > 3500:
        raise SystemExit(f"{model_id}: {len(v)} vertices, {len(f)} triangles is over the file limit")

    def text(s, size):
        b = s.encode("utf-8")
        if len(b) >= size:
            raise SystemExit(f"{model_id}: '{s}' does not fit {size} bytes")
        return b.ljust(size, b"\0")

    short = short or CREDITS[source_key]["short"]
    pal = [rgb565(*rgb) for rgb in palette] + [0] * (8 - len(palette))
    flags = (1 if schematic else 0) | (2 if cut else 0)
    front_from = 0
    if cut:
        fills = int(is_fill.sum())
        if fills:
            first = int(np.argmax(is_fill))
            if not is_fill[first:].all():
                raise SystemExit(f"{model_id}: fills must come last")
            front_from = first
    head = struct.pack("<4sBBHHH", b"ZAN1", flags, len(palette), len(v), len(f), front_from)
    head += text(model_id, 16) + text(label_de, 24) + text(label_en, 24) + text(short, 24)
    head += struct.pack("<8H", *pal)
    assert len(head) == 116
    body = head + v.astype("<i2").tobytes() + f.astype("<u2").tobytes() + c.astype("u1").tobytes()
    if cut:
        legend = cut["legend"]
        if not 1 <= len(legend) <= 8:
            raise SystemExit(f"{model_id}: {len(legend)} legend lines")
        body += bytes([len(legend)])
        for colour, de, en in legend:
            for word in (de, en):
                room = LEGEND_NOTE_PIXELS if colour == NOTE else LEGEND_WORD_PIXELS
                if not word.isascii() or sum(DEJAVU12_ADVANCE[ord(c) - 32] for c in word) > room:
                    raise SystemExit(f"{model_id}: legend word '{word}' must be ASCII and fit {room} pixels")
            if colour != NOTE and colour >= len(palette):
                raise SystemExit(f"{model_id}: legend colour {colour}")
            body += bytes([colour]) + text(de, 20) + text(en, 20)
    body += struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)
    file_name = (cut["file"] if cut else model_id) + ".bin"
    with open(os.path.join(out_dir, file_name), "wb") as out:
        out.write(body)
    entry = {
        "id": model_id,
        "file": file_name,
        "label_de": label_de,
        "label_en": label_en,
        "schematic": schematic,
        "triangles": int(len(f)),
        "vertices": int(len(v)),
        "bytes": len(body),
        "source": source_key,
        "parts": sorted({p["name"] for p in parts}),
    }
    if cut:
        entry["filled_triangles"] = fills
        entry["filled_parts"] = sorted({p["name"] for p in parts if p.get("fill")})
        entry["left_open_parts"] = sorted(set(cut.get("open", [])))
        entry["legend"] = [{"de": de, "en": en} for _, de, en in cut["legend"]]
    if extra:
        entry.update(extra)
    print(f"{file_name}: {len(parts)} parts, {len(f)} triangles ({fills} filled), {len(v)} vertices, {len(body)} bytes")
    return entry


def write_index(out_dir, entries):
    used = {e["source"] for e in entries}
    index = {
        "format": "ZAN1, see sketch/sketch_jan12a/anatomy_core.h",
        "credits": {k: CREDITS[k] for k in sorted(used)},
        "models": sorted(entries, key=lambda e: e["file"]),
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
                entries[e["file"]] = e
    for e in new_entries:
        entries[e["file"]] = e
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
BRAIN_GYRI = (
    r"(Left|Right) (superior frontal gyrus|middle frontal gyrus|inferior frontal gyrus|precentral gyrus|postcentral gyrus"
    r"|superior parietal lobule|angular gyrus|supramarginal gyrus|occipital lobe|superior temporal gyrus"
    r"|middle temporal gyrus|inferior temporal gyrus|fusiform gyrus|parahippocampal gyrus|cingulate gyrus|insula)"
)
TEETH = r".*secondary (central |lateral )?(incisor|canine|premolar|first premolar|second premolar|molar) tooth|.*secondary (molar|premolar) tooth"
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
        (BRAIN_GYRI, 0),
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
    parts += select_bp3d(isa, args.names(isa), [(TEETH, 1)])
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


# ---- Cutaways of organs --------------------------------------------------------------------------------------------


def named(parts, mode, color, **more):
    return [{**p, "mode": mode, "color": color, **more} for p in parts]


def cut_centre(parts):
    all_v = np.concatenate([p["v"] for p in parts])
    return (all_v.min(0) + all_v.max(0)) / 2


def glb_parts(path, pattern):
    return select_glb(path, [(pattern, 0)])


def make_heart_cut(args):
    """Human Reference Atlas: chambers with real wall thickness, septum, valves, papillary muscles. The textbook
    four-chamber cut: a plane through both valves and the tip of the left ventricle."""
    path = os.path.join(args.sources, "hra", "heart-male.glb")
    palette = [(200, 44, 44), (236, 112, 92), (58, 78, 196), (112, 150, 240), (244, 196, 176), (246, 226, 90), (250, 250, 240)]
    get = lambda pattern: glb_parts(path, pattern)
    left_vent, left_atr = get("VH_M_heart_left_ventricle"), get("VH_M_left_cardiac_atrium")
    right_vent, right_atr = get("VH_M_heart_right_ventricle"), get("VH_M_right_cardiac_atrium")
    septum = get("VH_M_interventricular_septum")
    valves, papillary = get("VH_M_(mitral|tricuspid|aortic|pulmonary)_valve"), get("VH_M_papillary_muscle_of_heart_.*")
    mitral = get("VH_M_mitral_valve")[0]["v"].mean(0)
    tricuspid = get("VH_M_tricuspid_valve")[0]["v"].mean(0)
    base = (mitral + tricuspid) / 2
    wall = left_vent[0]["v"]
    apex = wall[np.argmax(np.linalg.norm(wall - base, axis=1))]
    normal = np.cross(mitral - apex, tricuspid - apex)
    if normal[2] < 0:  # The cut looks in from the front of the body
        normal = -normal
    origin, axes = cut_frame(base, normal, base - apex)
    parts = (named(left_atr, "solid", 1) + named(right_atr, "solid", 3) + named(left_vent, "solid", 0) + named(right_vent, "solid", 2)
             + named(septum, "solid", 4) + named(valves, "solid", 5) + named(papillary, "solid", 6))
    result, left_open = build_cut(parts, origin, axes, 2600, 12)
    legend = [(1, "Linker Vorhof", "Left atrium"), (0, "Linke Kammer", "Left ventricle"), (3, "Rechter Vorhof", "Right atrium"),
              (2, "Rechte Kammer", "Right ventricle"), (4, "Scheidewand", "Septum"), (5, "Herzklappen", "Valves"),
              (6, "Papillarmuskeln", "Papillary muscle")]
    return write_cut(args, "heart", "Herz", "Heart", "hra-heart", palette, result, legend, left_open=left_open)


def make_kidney_cut(args):
    """Human Reference Atlas: outer cortex, columns, pyramids, papillae, hilum, renal pelvis. Cut along the long axis
    through the hilum, as in a textbook."""
    path = os.path.join(args.sources, "hra", "kidney-male-right.glb")
    palette = [(176, 74, 62), (112, 52, 136), (246, 208, 96), (92, 172, 216), (246, 246, 130)]
    cortex = glb_parts(path, "VH_M_outer_cortex_of_kidney_R|VH_M_renal_column_R")
    pyramids, papillae = glb_parts(path, "VH_M_renal_pyramid_[a-j]"), glb_parts(path, "VH_M_renal_papilla_R_[a-j]")
    hilum = glb_parts(path, "VH_M_hilum_of_kidney_R")
    pelvis = select_glb(os.path.join(args.sources, "hra", "renal-pelvis-male-right.glb"), [("VH_M_renal_pelvis_R", 0)])
    points = cortex[0]["v"]
    centre = points.mean(0)
    long_axis = np.linalg.svd(points - centre, full_matrices=False)[2][0]
    towards = hilum[0]["v"].mean(0) - centre
    towards -= long_axis * (towards @ long_axis)
    normal = np.cross(long_axis, towards)
    if normal[2] < 0:
        normal = -normal
    up = long_axis if long_axis[1] > 0 else -long_axis
    normal = normal / np.linalg.norm(normal)
    origin, axes = cut_frame(centre - normal * 0.006, normal, up)  # 6 mm back: this is where the pyramids show best
    parts = (named(cortex, "solid", 0) + named(hilum, "solid", 3) + named(pyramids, "solid", 1) + named(papillae, "solid", 2)
             + named(pelvis, "solid", 4))
    result, left_open = build_cut(parts, origin, axes, 2600, 12)
    legend = [(0, "Rinde", "Cortex"), (1, "Pyramiden", "Pyramids"), (2, "Papillen", "Papillae"), (3, "Nierenpforte", "Hilum"),
              (4, "Nierenbecken", "Renal pelvis")]
    return write_cut(args, "kidney", "Niere", "Kidney", "hra-kidney", palette, result, legend, left_open=left_open)


def make_eye_cut(args):
    """Human Reference Atlas: a cut through the middle of the eye from front to back. The clear water between cornea
    and iris is left out, which is how it looks: a gap."""
    path = os.path.join(args.sources, "hra", "eye-male-right.glb")
    palette = [(238, 238, 228), (150, 60, 50), (240, 146, 116), (176, 210, 228), (248, 226, 110), (64, 112, 184), (150, 214, 240)]
    get = lambda pattern: glb_parts(path, pattern)
    lens = get("VH_M_lens_R")
    sclera = get("VH_M_sclera_R")
    centre = cut_centre(sclera)
    axis = lens[0]["v"].mean(0) - centre
    normal = np.cross(axis, [0.0, 1.0, 0.0])
    if normal[2] < 0:
        normal = -normal
    origin, axes = cut_frame((centre + lens[0]["v"].mean(0)) / 2, normal, [0, 1, 0])
    parts = (named(sclera, "solid", 0) + named(get("VH_M_optic_choroid_R|VH_M_ciliary_body_R"), "solid", 1)
             + named(get("VH_M_retina_R"), "solid", 2) + named(get("VH_M_vitreous_humor_R"), "solid", 3)
             + named(get("VH_M_cornea_R"), "solid", 6) + named(get("VH_M_iris_R"), "solid", 5) + named(lens, "solid", 4))
    result, left_open = build_cut(parts, origin, axes, 2400, 30)
    legend = [(6, "Hornhaut", "Cornea"), (5, "Iris", "Iris"), (4, "Linse", "Lens"), (3, "Klares Gel", "Clear gel"), (2, "Netzhaut", "Retina"),
              (1, "Aderhaut", "Choroid"), (0, "Lederhaut", "Sclera")]
    return write_cut(args, "eye", "Auge", "Eye", "hra-eye", palette, result, legend, left_open=left_open)


def make_brain_cut(args, partof, isa):
    """BodyParts3D: a cut just beside the middle, from the side. Deep parts from its IS-A tree."""
    palette = [(232, 176, 168), (238, 228, 206), (250, 250, 240), (84, 164, 236), (120, 200, 120), (205, 130, 130), (190, 150, 120), (232, 170, 80)]
    cortex = select_bp3d(partof, args.names(partof), [(BRAIN_GYRI, 0)])
    pick = lambda pattern: select_bp3d(isa, args.names(isa), [(pattern, 0)])
    # The two trees share parts: each element only once
    def once(parts):
        seen, out = set(), []
        for p in parts:
            if p["name"] not in seen:
                seen.add(p["name"])
                out.append(p)
        return out
    parts = (named(cortex, "solid", 0)
             + named(once(pick(r"White matter of (left|right) cerebral hemisphere")), "solid", 1)
             + named(once(pick(r"Callosum")), "solid", 2)
             + named(once(pick(r"(Left|Right) (caudate nucleus|putamen|globus pallidus)")), "solid", 7)
             + named(once(pick(r"(Left|Right) thalamus")), "solid", 4)
             + named(once(pick(r"(Left|Right) lateral ventricle|Third ventricle|Fourth ventricle|Cerebral aqueduct")), "solid", 3)
             + named(once(pick(r"Cerebellum")), "solid", 5)
             + named(once(pick(r"Pons|Midbrain|Medulla oblongata")), "solid", 6))
    centre = cut_centre(parts)
    origin, axes = cut_frame(centre + np.array([3.0, 0, 0]), [-1, 0, 0], [0, 1, 0])  # 3 mm to the patient's left of the middle
    result, left_open = build_cut(parts, origin, axes, 2600, 12)
    legend = [(0, "Hirnrinde", "Cortex"), (1, "Hirnmark", "White matter"), (2, "Balken", "Callosum"),
              (3, "Hirnkammern", "Ventricles"), (4, "Thalamus", "Thalamus"), (7, "Kerne", "Deep nuclei"), (5, "Kleinhirn", "Cerebellum"),
              (6, "Hirnstamm", "Brain stem")]
    return write_cut(args, "brain", "Gehirn", "Brain", "bp3d", palette, result, legend, left_open=left_open)


def make_liver_cut(args, partof):
    """BodyParts3D has the real vessels inside the liver. The front of the liver tissue is left out, and the screen says so."""
    palette = [(150, 72, 62), (70, 110, 230), (140, 90, 220), (232, 72, 60), (90, 200, 90)]
    tissue = select_bp3d(partof, args.names(partof), [(r"Hepatovenous segment .*|Caudate lobe of liver", 0)])
    parts = named(tissue, "shell", 0, weight=0.3)
    tree = [(r"(Hepatic portal vein|Pre-hepatic portal vein|Left portal vein|Right portal vein|.* branch of (left|right) portal vein)", 1),
            (r"((Left|Middle|Right) hepatic vein|.* tributary of (left|middle|right) hepatic vein)", 2),
            (r"(Common hepatic artery|Hepatic artery proper|(Left|Right) hepatic artery|.* segmental hepatic artery)", 3),
            (r"((Left|Right|Common) hepatic duct|Cystic duct|Gallbladder|.* tributary of (left|right) hepatic biliary tree)", 4)]
    for pattern, color in tree:
        parts += named(select_bp3d(partof, args.names(partof), [(pattern, 0)]), "whole", color)
    origin, axes = cut_frame(cut_centre(parts[:len(tissue)]), [0, 0, 1], [0, 1, 0])
    result, left_open = build_cut(parts, origin, axes, 2600, 8)
    legend = [(1, "Pfortader", "Portal vein"), (2, "Lebervenen", "Hepatic veins"), (3, "Leberarterie", "Hepatic artery"), (4, "Gallenwege", "Bile ducts"),
              (0, "Leber", "Liver"), (NOTE, "Gewebe entfernt", "Tissue removed")]
    return write_cut(args, "liver", "Leber", "Liver", "bp3d", palette, result, legend, left_open=left_open)


def make_lungs_cut(args):
    """Human Reference Atlas: the bronchial tree in the lung. The front of the lung tissue is left out, and the screen says so."""
    path = os.path.join(args.sources, "hra", "lung-male.glb")
    palette = [(232, 150, 150), (244, 240, 208)]
    tissue = glb_parts(path, r"VH_M_(left|right)_.*bronchopulmonary_segment")
    tree = glb_parts(path, r"VH_M_(left|right)_.*_bronchus|VH_M_right_posterior_basal|VH_M_bronchus")
    # Most triangles for the tree: it's what this view is about, and thin branches fall apart first
    parts = named(tissue, "shell", 0, tolerance=1e-5, weight=0.25) + named(tree, "whole", 1, tolerance=1e-5, weight=2.0)
    origin, axes = cut_frame(cut_centre(tissue), [0, 0, 1], [0, 1, 0])
    result, left_open = build_cut(parts, origin, axes, 2600, 8)
    legend = [(1, "Bronchien", "Bronchi"), (0, "Lunge", "Lung"), (NOTE, "Gewebe entfernt", "Tissue removed")]
    return write_cut(args, "lungs", "Lunge", "Lungs", "hra", palette, result, legend, left_open=left_open)


def make_skull_cut(args, partof, isa):
    """BodyParts3D: cut down the middle. Shows the inside of the real bones and where the brain sits."""
    palette = [(236, 226, 202), (250, 250, 240)]
    bones = select_bp3d(partof, args.names(partof), [(BONES_SKULL, 0)])
    teeth = select_bp3d(isa, args.names(isa), [(TEETH, 0)])
    parts = named(bones, "solid", 0) + named(teeth, "solid", 1)
    centre = cut_centre(parts)
    origin, axes = cut_frame(centre + np.array([0.5, 0, 0]), [1, 0, 0], [0, 1, 0])
    result, left_open = build_cut(parts, origin, axes, 2600, 8)
    legend = [(0, "Knochen", "Bone"), (1, "Zahn", "Tooth")]
    return write_cut(args, "skull", "Schädel", "Skull", "bp3d", palette, result, legend, left_open=left_open, short="BodyParts3D")


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


def merge(meshes):
    vs, fs, base = [], [], 0
    for v, f in meshes:
        vs.append(v)
        fs.append(f + base)
        base += len(v)
    return np.concatenate(vs), np.concatenate(fs)


def inside_out(mesh):
    return mesh[0], mesh[1][:, [0, 2, 1]]


def box(lo, hi):
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    v = np.array([[x0, y0, z0], [x1, y0, z0], [x1, y1, z0], [x0, y1, z0], [x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]], dtype=np.float64)
    f = np.array([[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [2, 3, 7], [2, 7, 6], [0, 4, 7], [0, 7, 3], [1, 2, 6], [1, 6, 5]], dtype=np.int64)
    return v, f


def subdivide(mesh, times):
    """Splits long box faces, so the fill of the cut meets edges that are not too far apart."""
    v, f = mesh
    for _ in range(times):
        verts = [p for p in v]
        made = {}

        def middle(a, b):
            key = (min(a, b), max(a, b))
            if key not in made:
                made[key] = len(verts)
                verts.append((v[a] + v[b]) / 2)
            return made[key]

        faces = []
        for a, b, c in f.tolist():
            ab, bc, ca = middle(a, b), middle(b, c), middle(c, a)
            faces += [(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)]
        v, f = np.array(verts), np.array(faces, dtype=np.int64)
    return v, f


def schematic_part(name, mesh, color, mode="solid", **more):
    return {"name": name, "source": "schematic", "v": mesh[0], "f": mesh[1], "color": color, "mode": mode, "tolerance": 1e-6, **more}


def write_cell(args, model_id, de, en, palette, parts, legend, about):
    origin, axes = cut_frame((0, 0, 0), (0, 0, 1), (0, 1, 0))
    result, left_open = build_cut(parts, origin, axes, 6000, 8)
    return write_cut(args, model_id, de, en, "schematic", palette, result, legend, schematic=True, left_open=left_open, file_suffix="",
                     extra={"schematic_of": about})


def make_animal_cell(args):
    """Open down the front, with the nucleus and the larger parts cut through. What's behind the cut stays whole."""
    palette = [(214, 150, 110), (250, 226, 196), (128, 76, 170), (62, 34, 104), (232, 96, 56), (84, 170, 226), (246, 222, 96), (112, 200, 120)]
    radii = (1.0, 0.88, 0.95)
    inner = inside_out(ellipsoid((0, 0, 0), tuple(r * 0.96 for r in radii), 12, 22))
    membrane = merge([ellipsoid((0, 0, 0), radii, 12, 22), inner])
    outer_only = lambda c: (c ** 2 / np.array(radii) ** 2).sum(axis=1) > 0.98 ** 2 * 0.99
    parts = [schematic_part("cell membrane", membrane, 0, shell_filter=outer_only)]
    parts.append(schematic_part("cytoplasm, drawn on the inside of the membrane", inner, 1, mode="lining"))
    parts.append(schematic_part("nucleus", ellipsoid((-0.14, 0.04, -0.1), (0.36, 0.34, 0.36), 9, 14), 2))
    parts.append(schematic_part("nucleolus", ellipsoid((-0.1, 0.08, -0.02), (0.13, 0.13, 0.13), 6, 9), 3))
    for i, (c, d) in enumerate([((0.46, 0.36, -0.08), (1, 1, 0)), ((0.6, -0.14, -0.1), (1, -0.4, 0.1)), ((-0.52, -0.44, -0.1), (1, 0.6, 0)), ((0.14, -0.56, -0.08), (1, 0.1, 0.1))]):
        parts.append(schematic_part(f"mitochondrion {i + 1}", ellipsoid(c, (0.085, 0.2, 0.085), 6, 8, direction=d), 4))
    # Flat sacs of the endoplasmic reticulum next to the nucleus
    for i, (c, r) in enumerate([((-0.62, 0.3, -0.05), 0.2), ((-0.46, 0.5, -0.05), 0.2), ((-0.16, 0.6, -0.05), 0.18)]):
        parts.append(schematic_part(f"endoplasmic reticulum {i + 1}", ellipsoid(c, (r, 0.04, 0.16), 4, 8), 5))
    for i, (c, r) in enumerate([((0.3, 0.1, -0.06), 0.2), ((0.34, 0.0, -0.06), 0.17), ((0.38, -0.1, -0.06), 0.13)]):
        parts.append(schematic_part(f"golgi apparatus {i + 1}", ellipsoid(c, (r, 0.03, 0.14), 4, 8), 7))
    for i, c in enumerate([(0.34, -0.4, -0.3), (-0.3, -0.62, -0.25), (-0.72, -0.1, -0.25), (0.14, 0.52, -0.3), (0.68, 0.14, -0.3)]):
        parts.append(schematic_part(f"vesicle {i + 1}", ellipsoid(c, (0.055, 0.055, 0.055), 4, 6), 6))
    legend = [(0, "Zellmembran", "Membrane"), (2, "Zellkern", "Nucleus"), (3, "Nukleolus", "Nucleolus"),
              (4, "Mitochondrien", "Mitochondria"), (5, "Retikulum", "Reticulum"), (7, "Golgi", "Golgi"), (6, "Vesikel", "Vesicles")]
    return write_cell(args, "animal-cell", "Tierzelle", "Animal cell", palette, parts, legend,
                      "Which parts an animal cell has and roughly where, as in school textbooks. Not measured.")


def make_plant_cell(args):
    palette = [(84, 146, 66), (208, 232, 176), (120, 70, 150), (62, 34, 104), (40, 150, 56), (120, 190, 240), (232, 96, 56)]
    sx, sy, sz = 1.0, 0.68, 0.5
    t = 0.06
    inner = inside_out(subdivide(box((-sx + t, -sy + t, -sz + t), (sx - t, sy - t, sz - t)), 2))
    wall = merge([subdivide(box((-sx, -sy, -sz), (sx, sy, sz)), 2), inner])
    outer_only = lambda c: (np.abs(c[:, 0]) > sx - t / 2) | (np.abs(c[:, 1]) > sy - t / 2) | (c[:, 2] < -sz + t / 2)
    parts = [schematic_part("cell wall", wall, 0, shell_filter=outer_only)]
    parts.append(schematic_part("cytoplasm, drawn on the inside of the wall", inner, 1, mode="lining"))
    parts.append(schematic_part("vacuole", ellipsoid((0.2, 0.0, -0.06), (0.58, 0.42, 0.32), 9, 16), 5))
    parts.append(schematic_part("nucleus", ellipsoid((-0.64, 0.22, -0.04), (0.22, 0.22, 0.22), 8, 12), 2))
    parts.append(schematic_part("nucleolus", ellipsoid((-0.62, 0.24, 0.0), (0.075, 0.075, 0.075), 5, 7), 3))
    for i, (c, d) in enumerate([((-0.62, -0.34, -0.08), (1, 0.3, 0)), ((-0.25, -0.52, -0.08), (1, 0.1, 0)), ((0.25, -0.54, -0.08), (1, -0.1, 0)),
                                ((0.74, -0.42, -0.08), (0.3, 1, 0)), ((0.86, 0.05, -0.08), (0.1, 1, 0)), ((0.72, 0.5, -0.08), (1, 0.6, 0)),
                                ((0.2, 0.54, -0.08), (1, 0, 0)), ((-0.25, 0.54, -0.08), (1, 0.1, 0))]):
        parts.append(schematic_part(f"chloroplast {i + 1}", ellipsoid(c, (0.07, 0.14, 0.07), 5, 8, direction=d), 4))
    for i, (c, d) in enumerate([((-0.3, -0.18, -0.08), (1, 0.8, 0)), ((-0.86, -0.08, -0.08), (0.2, 1, 0))]):
        parts.append(schematic_part(f"mitochondrion {i + 1}", ellipsoid(c, (0.05, 0.1, 0.05), 4, 7, direction=d), 6))
    legend = [(0, "Zellwand", "Cell wall"), (5, "Vakuole", "Vacuole"), (2, "Zellkern", "Nucleus"),
              (3, "Nukleolus", "Nucleolus"), (4, "Chloroplasten", "Chloroplasts"), (6, "Mitochondrien", "Mitochondria")]
    return write_cell(args, "plant-cell", "Pflanzenzelle", "Plant cell", palette, parts, legend,
                      "Which parts a plant cell has and roughly where, as in school textbooks. Not measured.")


def make_red_blood_cell(args):
    """Biconcave disc after the formula of Evans and Fung (1972), a fit to measured cells. Still schematic: one idealised
    shape, not a scan. z(r) = +-0.5 * sqrt(1 - s^2) * (c0 + c1 s^2 + c2 s^4), s = r / R, R = 3.91 micrometres.
    c1: one paper seen (arXiv 1605.04509) prints 7.38. 7.83 is used because it reproduces the reported cell height of
    2.58 micrometres, where 7.38 gives 2.41. The original paper was not read.
    Cut through the middle, which shows the real dip of that shape. A grown red blood cell has no nucleus, so there's none."""
    R, c0, c1, c2 = 3.91, 0.81, 7.83, -4.39
    steps = 12
    top = []
    for i in range(steps + 1):
        s = math.sin(i / steps * math.pi / 2)  # More points at the rim, where it curves fastest
        h = 0.5 * math.sqrt(max(0.0, 1 - s * s)) * (c0 + c1 * s ** 2 + c2 * s ** 4)
        top.append((s * R, h))
    profile = top[:-1] + [top[-1]] + [(r, -h) for r, h in reversed(top[:-1])]
    # Tipped towards the viewer: the far half shows the dip from above, behind the cut through the middle
    parts = [schematic_part("biconcave disc", surface_of_revolution(profile, 28), 1, fill_color=0)]
    rotate_x(parts, 38)
    legend = [(1, "Zellmembran", "Membrane"), (0, "Blutfarbstoff", "Hemoglobin"), (NOTE, "Kein Zellkern", "No nucleus")]
    return write_cell(args, "red-blood-cell", "Rote Blutzelle", "Red blood cell", [(226, 56, 56), (150, 26, 34)], parts, legend,
                      "Idealised shape from the Evans and Fung (1972) formula, fitted to measured cells. Not a scan.")


def make_neuron(args):
    """The cell body and the myelin are cut through. Dendrites and branches stay whole: there's nothing to show inside them."""
    palette = [(160, 118, 214), (62, 34, 104), (236, 226, 190), (246, 190, 84), (232, 96, 56)]
    parts = [schematic_part("cell body", ellipsoid((-0.62, 0.0, 0.0), (0.2, 0.18, 0.18), 8, 12), 0)]
    parts.append(schematic_part("nucleus", ellipsoid((-0.62, 0.02, 0.0), (0.08, 0.08, 0.08), 5, 8), 1))
    # Dendrites: fixed branching, short and getting thinner
    dendrites = [
        [(-0.70, 0.12, -0.02), (-0.82, 0.30, -0.05), (-0.92, 0.44, -0.10), (-0.98, 0.56, -0.08)],
        [(-0.82, 0.30, -0.05), (-0.72, 0.46, -0.02), (-0.68, 0.60, -0.06)],
        [(-0.78, 0.02, -0.05), (-0.94, 0.06, -0.12), (-1.06, 0.16, -0.16)],
        [(-0.94, 0.06, -0.12), (-1.04, -0.04, -0.06), (-1.14, -0.10, -0.02)],
        [(-0.70, -0.11, -0.02), (-0.80, -0.28, -0.06), (-0.94, -0.42, -0.08), (-1.00, -0.54, -0.12)],
        [(-0.80, -0.28, -0.06), (-0.70, -0.46, -0.02), (-0.66, -0.58, -0.03)],
        [(-0.58, 0.14, -0.06), (-0.50, 0.32, -0.12), (-0.40, 0.44, -0.14)],
        [(-0.60, -0.14, -0.06), (-0.50, -0.30, -0.04), (-0.38, -0.40, -0.08)],
    ]
    for i, pts in enumerate(dendrites):
        radii = [0.034 * (1 - 0.65 * k / (len(pts) - 1)) for k in range(len(pts))]
        parts.append(schematic_part(f"dendrite {i + 1}", tube(pts, radii, 5), 0, mode="whole"))

    def axon_at(x):
        return (x, 0.05 * math.sin(x * 4.0), 0.0)

    for i in range(5):
        a, b = -0.40 + i * 0.2, -0.40 + i * 0.2 + 0.16
        pts = [axon_at(a + (b - a) * k / 4) for k in range(5)]
        parts.append(schematic_part(f"myelin sheath {i + 1}", tube(pts, [0.035, 0.058, 0.06, 0.058, 0.035], 8), 2))
    xs = [-0.46 + i * 0.1 for i in range(13)]
    parts.append(schematic_part("axon", tube([axon_at(x) for x in xs], [0.02] * len(xs), 6), 3))
    # Axon terminals
    end = axon_at(0.74)
    tips = [(0.98, 0.26, -0.08), (1.04, 0.08, -0.08), (1.02, -0.12, -0.08), (0.94, -0.28, -0.05)]
    for i, tip in enumerate(tips):
        mid = ((end[0] + tip[0]) / 2, (end[1] + tip[1]) / 2 + 0.02, (end[2] + tip[2]) / 2)
        parts.append(schematic_part(f"axon branch {i + 1}", tube([end, mid, tip], [0.018, 0.015, 0.012], 4), 3, mode="whole"))
        parts.append(schematic_part(f"axon terminal {i + 1}", ellipsoid(tip, (0.04, 0.04, 0.04), 4, 6), 4, mode="whole"))
    legend = [(0, "Zellleib", "Cell body"), (1, "Zellkern", "Nucleus"), (3, "Axon", "Axon"), (2, "Myelin", "Myelin"), (4, "Endknopf", "Terminal")]
    return write_cell(args, "neuron", "Nervenzelle", "Neuron", palette, parts, legend,
                      "Cell body, dendrites, axon with myelin, terminals, as in school textbooks. Not measured.")


def recipes(args):
    partof = os.path.join(args.sources, "bp3d", "partof")
    isa = os.path.join(args.sources, "bp3d", "isa")
    return {
        "heart-cut": lambda: make_heart_cut(args),
        "kidney-cut": lambda: make_kidney_cut(args),
        "eye-cut": lambda: make_eye_cut(args),
        "brain-cut": lambda: make_brain_cut(args, partof, isa),
        "liver-cut": lambda: make_liver_cut(args, partof),
        "lungs-cut": lambda: make_lungs_cut(args),
        "skull-cut": lambda: make_skull_cut(args, partof, isa),
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
