#!/usr/bin/env python3
"""Generates sketch/sketch_jan12a/crystal_table.h: real crystal lattices from the Crystallography Open Database.

For each crystal it downloads one COD record, checks that it is what it should be (name, formula, cell,
and nearest-atom distances against published values), applies the record's symmetry operations and
writes all atoms of one unit cell. The stick repeats that cell and draws it. Nothing is written if a
single check fails: a wrong record is an invented model.

Licence: all COD data is public domain, CC0 (https://www.crystallography.net/cod/). COD asks users to
acknowledge the original authors, so the table and docs/features/protein.md name each record and its paper.

Usage: python3 -I scripts/make_crystal_table.py [--cache DIR]
Downloads with curl into the cache directory (a new temporary one by default).
"""
import argparse
import itertools
import math
import os
import re
import shlex
import subprocess
import sys
import tempfile

USER_AGENT = "Zitro"
DUPLICATE_TOLERANCE = 0.005  # In cell fractions; symmetry copies closer than this are one atom

# key, German label, English label, more names, COD ID, what the record must call itself, sum formula,
# cell a to check, times the cell is repeated, lines to draw (atom, atom, longest in Angstrom),
# published distances to check (atom, atom, Angstrom), atoms to leave out because the record has only part-filled sites
CRYSTALS = [
    dict(key="salt", de="Kochsalz", en="Table salt", names=["table salt", "sodium chloride", "nacl", "kochsalz", "salz", "halite", "rock salt", "steinsalz"],
         cod=1000041, name=r"sodium chloride", formula="Cl Na", a=5.62, repeat=(1, 1, 1), bonds=[("Na", "Cl", 3.0)], check=[("Na", "Cl", 2.81)]),
    dict(key="diamond", de="Diamant", en="Diamond", names=["diamant"],
         cod=9008564, name=r"diamond", formula="C", a=3.56679, repeat=(1, 1, 1), bonds=[("C", "C", 1.7)], check=[("C", "C", 1.545)]),
    dict(key="graphite", de="Graphit", en="Graphite", names=["graphit"],
         cod=9011577, name=r"graphite", formula="C", a=2.464, repeat=(3, 3, 1), bonds=[("C", "C", 1.6)], check=[("C", "C", 1.42)]),
    dict(key="ice", de="Eis (nur O-Atome)", en="Ice (O atoms only)", names=["eis", "water ice", "ice ih", "wassereis", "snow", "schnee", "snowflake", "schneeflocke"],
         cod=1538173, name=r"ice|electron density distribution in ice", formula="H2 O", a=4.506, repeat=(2, 2, 1), bonds=[("O", "O", 3.0)], check=[("O", "O", 2.75)], skip=["H"]),
    dict(key="quartz", de="Quarz", en="Quartz", names=["quarz", "silicon dioxide", "siliziumdioxid", "bergkristall", "rock crystal"],
         cod=5000035, name=r"quartz", formula="O2 Si", a=4.91239, repeat=(2, 2, 1), bonds=[("Si", "O", 1.8)], check=[("Si", "O", 1.61)]),
    dict(key="calcite", de="Kalkspat", en="Calcite", names=["kalkspat", "calcit", "kalzit", "calcium carbonate", "kalziumkarbonat", "calciumcarbonat"],
         cod=9000095, name=r"calcite", formula="C Ca O3", a=4.99, repeat=(1, 1, 1), bonds=[("C", "O", 1.4), ("Ca", "O", 2.5)], check=[("C", "O", 1.28), ("Ca", "O", 2.36)]),
    dict(key="pyrite", de="Pyrit", en="Pyrite", names=["pyrit", "fools gold", "katzengold", "iron pyrite", "eisenkies"],
         cod=9000594, name=r"pyrite", formula="Fe S2", a=5.4166, repeat=(1, 1, 1), bonds=[("Fe", "S", 2.4), ("S", "S", 2.3)], check=[("Fe", "S", 2.26), ("S", "S", 2.16)]),
    dict(key="fluorite", de="Flussspat", en="Fluorite", names=["flussspat", "fluorit", "calcium fluoride", "kalziumfluorid", "calciumfluorid"],
         cod=1000043, name=r"calcium fluoride|fluorite", formula="Ca F2", a=5.462, repeat=(1, 1, 1), bonds=[("Ca", "F", 2.5)], check=[("Ca", "F", 2.365)]),
    dict(key="copper", de="Kupfer", en="Copper", names=["kupfer"],
         cod=9008468, name=r"copper", formula="Cu", a=3.61496, repeat=(1, 1, 1), bonds=[("Cu", "Cu", 2.7)], check=[("Cu", "Cu", 2.556)]),
    dict(key="gold", de="Gold", en="Gold", names=[],
         cod=9008463, name=r"gold", formula="Au", a=4.07825, repeat=(1, 1, 1), bonds=[("Au", "Au", 3.0)], check=[("Au", "Au", 2.884)]),
    dict(key="silver", de="Silber", en="Silver", names=["silber"],
         cod=9008459, name=r"silver", formula="Ag", a=4.0862, repeat=(1, 1, 1), bonds=[("Ag", "Ag", 3.0)], check=[("Ag", "Ag", 2.889)]),
    dict(key="iron", de="Eisen", en="Iron", names=["eisen"],
         cod=5000217, name=r"iron", formula="Fe", a=2.8608, repeat=(2, 2, 2), bonds=[("Fe", "Fe", 2.6)], check=[("Fe", "Fe", 2.477)]),
    dict(key="silicon", de="Silizium", en="Silicon", names=["silizium", "silicium"],
         cod=9008565, name=r"silicon", formula="Si", a=5.4307, repeat=(1, 1, 1), bonds=[("Si", "Si", 2.5)], check=[("Si", "Si", 2.352)]),
]


def strip_uncertainty(text):
    return float(re.sub(r"\([0-9]+\)$", "", text))


def tokenize_cif(text):
    """CIF tokens, without comments and with quoted text and ;-blocks kept whole."""
    tokens = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith(";"):
            block = [line[1:]]
            i += 1
            while i < len(lines) and not lines[i].startswith(";"):
                block.append(lines[i])
                i += 1
            tokens.append("\n".join(block))
            i += 1
            continue
        lexer = shlex.shlex(line, posix=True)
        lexer.whitespace_split = True
        lexer.commenters = "#"
        lexer.quotes = "'\""
        lexer.escape = ""
        try:
            tokens.extend(lexer)
        except ValueError:  # An apostrophe inside a word
            tokens.extend(line.split("#")[0].split())
        i += 1
    return tokens


def parse_cif(text):
    """Single values and loops of a CIF. {tag: value}, and loops as {tag: [values]}."""
    tokens = tokenize_cif(text)
    values, loops = {}, {}
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token == "loop_":
            i += 1
            tags = []
            while i < len(tokens) and tokens[i].startswith("_"):
                tags.append(tokens[i])
                i += 1
            rows = []
            while i < len(tokens) and not tokens[i].startswith("_") and tokens[i] != "loop_" and not tokens[i].startswith("data_"):
                rows.append(tokens[i])
                i += 1
            if not tags or len(rows) % len(tags):
                raise ValueError("loop with an uneven number of values")
            for k, tag in enumerate(tags):
                loops[tag] = rows[k::len(tags)]
        elif token.startswith("_") and i + 1 < len(tokens):
            values[token] = tokens[i + 1]
            i += 2
        else:
            i += 1
    return values, loops


def parse_symop(op):
    """'x+1/2,-y,z' as ((rx, ry, rz, t), ...). Parsed by hand: a downloaded file is never evaluated."""
    parts = op.replace(" ", "").lower().split(",")
    if len(parts) != 3:
        raise ValueError(f"bad symmetry operation {op!r}")
    result = []
    for part in parts:
        coefficients = {"x": 0, "y": 0, "z": 0}
        shift = 0.0
        terms = re.findall(r"[+-]?[^+-]+", part)
        if "".join(terms) != part or not terms:
            raise ValueError(f"bad symmetry operation {op!r}")
        for term in terms:
            match = re.fullmatch(r"([+-]?)(?:([xyz])|([0-9]*\.?[0-9]+)(?:/([0-9]+))?)", term)
            if not match:
                raise ValueError(f"bad symmetry operation {op!r}")
            sign = -1 if match.group(1) == "-" else 1
            if match.group(2):
                coefficients[match.group(2)] += sign
            else:
                denominator = float(match.group(4) or 1)
                if denominator == 0:
                    raise ValueError(f"bad symmetry operation {op!r}")
                shift += sign * float(match.group(3)) / denominator
        result.append((coefficients["x"], coefficients["y"], coefficients["z"], shift))
    return result


def element_of(label, type_symbol):
    text = type_symbol or label
    match = re.match(r"[A-Z][a-z]?", text)
    if not match:
        raise ValueError(f"no element in {text!r}")
    symbol = match.group(0)
    return "H" if symbol == "D" else symbol


def wrap_fraction(value):
    """Into 0 to 1, with anything a rounding error from 1 as 0: at 1.0 the atom on the near face would be lost."""
    value %= 1.0
    return 0.0 if value > 1.0 - 1e-5 else value


def expand_cell(values, loops, skip):
    """All atoms of one unit cell, as [(symbol, x, y, z)] with fractions in 0 to 1."""
    ops_text = loops.get("_symmetry_equiv_pos_as_xyz") or loops.get("_space_group_symop_operation_xyz")
    if not ops_text:
        raise ValueError("no symmetry operations")
    ops = [parse_symop(op) for op in ops_text]
    labels = loops["_atom_site_label"]
    types = loops.get("_atom_site_type_symbol", [""] * len(labels))
    occupancy = loops.get("_atom_site_occupancy", ["1"] * len(labels))
    atoms = []
    for i, label in enumerate(labels):
        symbol = element_of(label, types[i])
        if symbol in skip:
            continue
        if occupancy[i] not in (".", "?") and abs(strip_uncertainty(occupancy[i]) - 1) > 1e-6:
            raise ValueError(f"{label}: part-filled site: there is no one true place to draw it")
        site = [strip_uncertainty(loops[f"_atom_site_fract_{axis}"][i]) for axis in "xyz"]
        for op in ops:
            image = [wrap_fraction(r[0] * site[0] + r[1] * site[1] + r[2] * site[2] + r[3]) for r in op]
            for other in atoms:
                if other[0] == symbol and all(min(abs(image[k] - other[k + 1]), 1 - abs(image[k] - other[k + 1])) < DUPLICATE_TOLERANCE for k in range(3)):
                    break
            else:
                atoms.append((symbol, image[0], image[1], image[2]))
    return atoms


def cell_matrix(a, b, c, alpha, beta, gamma):
    """Columns are the cell's axes in Angstrom. Same formula as crystalCellMatrix in crystal_core.h."""
    ca, cb, cg = (math.cos(math.radians(v)) for v in (alpha, beta, gamma))
    sg = math.sin(math.radians(gamma))
    cy = (ca - cb * cg) / sg
    cz = math.sqrt(1 - cb * cb - cy * cy)
    return ((a, b * cg, c * cb), (0.0, b * sg, c * cy), (0.0, 0.0, c * cz))


def shortest_distances(atoms, matrix):
    """Shortest distance for each pair of elements, across cell borders."""
    best = {}
    for i, one in enumerate(atoms):
        for j, two in enumerate(atoms):
            for shift in itertools.product((-1, 0, 1), repeat=3):
                if i == j and shift == (0, 0, 0):
                    continue
                d = [two[k + 1] + shift[k] - one[k + 1] for k in range(3)]
                p = [sum(matrix[r][k] * d[k] for k in range(3)) for r in range(3)]
                distance = math.sqrt(sum(v * v for v in p))
                key = tuple(sorted((one[0], two[0])))
                best[key] = min(best.get(key, 1e9), distance)
    return best


def check_crystal(crystal, values, loops, atoms):
    """Raises ValueError unless this record is the right crystal."""
    if values.get("_cod_database_code") != str(crystal["cod"]):
        raise ValueError(f"record says it is {values.get('_cod_database_code')}")
    names = " ".join(values.get(t, "") for t in ("_chemical_name_mineral", "_chemical_name_common", "_chemical_name_systematic", "_publ_section_title"))
    if not re.search(crystal["name"], names, re.I):
        raise ValueError(f"'{crystal['name']}' not in '{names}'")
    if values.get("_chemical_formula_sum") != crystal["formula"]:
        raise ValueError(f"formula is {values.get('_chemical_formula_sum')}")
    if abs(strip_uncertainty(values["_cell_length_a"]) - crystal["a"]) > 1e-4:
        raise ValueError(f"cell a is {values['_cell_length_a']}")
    z = strip_uncertainty(values["_cell_formula_units_Z"])
    formula = {}
    for part in crystal["formula"].split():
        match = re.fullmatch(r"([A-Z][a-z]?)([0-9]*)", part)
        formula[match.group(1)] = int(match.group(2) or 1)
    for symbol, count in formula.items():
        if symbol in crystal.get("skip", []):
            continue
        have = sum(1 for atom in atoms if atom[0] == symbol)
        if have != z * count:
            raise ValueError(f"{have} {symbol} atoms in the cell, the formula says {z * count}")
    matrix = cell_matrix(*(strip_uncertainty(values[t]) for t in ("_cell_length_a", "_cell_length_b", "_cell_length_c", "_cell_angle_alpha", "_cell_angle_beta", "_cell_angle_gamma")))
    best = shortest_distances(atoms, matrix)
    for one, two, published in crystal["check"]:
        got = best.get(tuple(sorted((one, two))))
        if got is None or abs(got - published) > 0.03:
            raise ValueError(f"{one}-{two} is {got} Angstrom, published {published}")
    return matrix, best


def atomic_numbers(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    block = text[text.index("ATOM_SYMBOLS"):]
    block = block[:block.index("};")]
    symbols = re.findall(r'"([A-Za-z]*)"', block)
    return {symbol: i for i, symbol in enumerate(symbols) if symbol}


def citation(values):
    title = " ".join(values.get("_publ_section_title", "").split())
    journal = values.get("_journal_name_full", "")
    year = values.get("_journal_year", "")
    return f"{title} ({journal}, {year})"


def c_string(text):
    if not text.isascii() or '"' in text or "\\" in text or "\n" in text:
        raise ValueError(f"can't be a plain C string: {text!r}")
    return f'"{text}"'


def build_table(crystals, numbers):
    """The header's text. crystals: list of (crystal, values, atoms)."""
    out = ["// Generated by scripts/make_crystal_table.py from the Crystallography Open Database (CC0). Do not edit.",
           "// Atoms of one unit cell, as fractions of the cell, in the record's own coordinates.",
           "#pragma once", "", f"#define CRYSTAL_COUNT {len(crystals)}", ""]
    entries = []
    for crystal, values, atoms in crystals:
        key = crystal["key"]
        out.append(f"// {key}: COD {crystal['cod']}: {' '.join(citation(values).encode('ascii', 'replace').decode().split())}")
        out.append(f"static const CrystalSite CRYSTAL_SITES_{key.upper()}[] = {{")
        for symbol, x, y, z in atoms:
            out.append(f"  {{{x:.5f}f, {y:.5f}f, {z:.5f}f, {numbers[symbol]}}},  // {symbol}")
        out.append("};")
        out.append(f"static const CrystalBondRule CRYSTAL_BONDS_{key.upper()}[] = {{")
        for one, two, longest in crystal["bonds"]:
            out.append(f"  {{{numbers[one]}, {numbers[two]}, {longest:.2f}f}},  // {one}-{two}")
        out.append("};")
        cell = ", ".join(f"{strip_uncertainty(values[t]):.5f}f" for t in ("_cell_length_a", "_cell_length_b", "_cell_length_c", "_cell_angle_alpha", "_cell_angle_beta", "_cell_angle_gamma"))
        names = [key, crystal["de"], crystal["en"]] + crystal["names"]
        entries.append(
            f'  {{{c_string(key)}, {c_string(";".join(names))}, {c_string(crystal["de"])}, {c_string(crystal["en"])}, '
            f'"COD {crystal["cod"]}", {cell}, {crystal["repeat"][0]}, {crystal["repeat"][1]}, {crystal["repeat"][2]}, '
            f"CRYSTAL_SITES_{key.upper()}, {len(atoms)}, CRYSTAL_BONDS_{key.upper()}, {len(crystal['bonds'])}}},"
        )
    out += ["", "static const CrystalEntry CRYSTAL_ENTRIES[CRYSTAL_COUNT] = {"] + entries + ["};", ""]
    return "\n".join(out)


def download(url, path):
    if not os.path.exists(path):
        partial = path + ".part"
        subprocess.run(["curl", "-sSfL", "-A", USER_AGENT, "-m", "60", "-o", partial, url], check=True)
        os.replace(partial, path)
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", help="directory for downloads, kept between runs")
    parser.add_argument("--out", default=os.path.join(here, "..", "sketch", "sketch_jan12a", "crystal_table.h"))
    args = parser.parse_args()
    cache = args.cache or tempfile.mkdtemp(prefix="zitro_cod_")
    os.makedirs(cache, exist_ok=True)
    numbers = atomic_numbers(os.path.join(here, "..", "sketch", "sketch_jan12a", "atom_table.h"))
    done, failures = [], []
    for crystal in CRYSTALS:
        cod = crystal["cod"]
        try:
            text = download(f"https://www.crystallography.net/cod/{cod}.cif", os.path.join(cache, f"{cod}.cif"))
            values, loops = parse_cif(text)
            atoms = expand_cell(values, loops, crystal.get("skip", []))
            _, best = check_crystal(crystal, values, loops, atoms)
            done.append((crystal, values, atoms))
            shown = ", ".join(f"{a}-{b} {d:.3f}" for (a, b), d in sorted(best.items()))
            print(f"{crystal['key']}: COD {cod}, {len(atoms)} atoms per cell, {shown}\n   {citation(values)}")
        except (ValueError, KeyError, subprocess.CalledProcessError) as e:
            failures.append(f"{crystal['key']} (COD {cod}): {e!r}")
    if failures:
        print("Nothing written. Failed:\n  " + "\n  ".join(failures), file=sys.stderr)
        return 1
    table = build_table(done, numbers)
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(table)
    print(f"{args.out}: {len(table)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
