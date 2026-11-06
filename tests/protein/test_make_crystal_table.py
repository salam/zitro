#!/usr/bin/env python3
"""Tests the crystal generator's parsing and its checks, with no downloads. Run by tests/protein/run.sh."""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import make_crystal_table as m  # noqa: E402

failures = 0


def check(condition, what):
    global failures
    if not condition:
        failures += 1
        print("  FAIL", what)


def raises(fn, *args):
    try:
        fn(*args)
    except (ValueError, KeyError):
        return True
    return False


CIF = """#comment line
data_1000041
_cod_database_code 1000041
_publ_section_title
;
Sodium chloride structure
;
_chemical_formula_sum            'Cl Na'
_chemical_name_systematic        'Sodium chloride'
_cell_length_a                   5.62(1)
_cell_length_b                   5.62
_cell_length_c                   5.62
_cell_angle_alpha                90
_cell_angle_beta                 90
_cell_angle_gamma                90
_cell_formula_units_Z            4
loop_
_symmetry_equiv_pos_as_xyz
x,y,z
x,y+1/2,z+1/2
x+1/2,y,z+1/2
x+1/2,y+1/2,z
loop_
_atom_site_label
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
_atom_site_occupancy
Na1 Na+1 0.0 0.0 0.0 1.
Cl1 Cl-1 0.5 0.5 0.5 1.
"""

SALT = next(c for c in m.CRYSTALS if c["key"] == "salt")

values, loops = m.parse_cif(CIF)
check(values["_cell_formula_units_Z"] == "4", "single value")
check(values["_chemical_formula_sum"] == "Cl Na", "quoted value")
check(loops["_atom_site_label"] == ["Na1", "Cl1"], "loop")
check(m.strip_uncertainty("5.62(1)") == 5.62, "uncertainty in brackets")
atoms = m.expand_cell(values, loops, [])
check(len(atoms) == 8, "symmetry fills the cell: 4 Na and 4 Cl")
check(len({a[1:] for a in atoms}) == 8, "no atom twice")
m.check_crystal(SALT, values, loops, atoms)

# Symmetry operations are read, never run
op = m.parse_symop("-x+1/2, y-z, 0.25+z")
check(op[0] == (-1, 0, 0, 0.5) and op[1] == (0, 1, -1, 0.0) and op[2] == (0, 0, 1, 0.25), "symmetry operation")
for bad in ["x,y", "x,y,z,x", "__import__('os').system('x'),y,z", "x*2,y,z", "a,b,c", ",,", "x/0,y,z", "1/0,y,z"]:
    check(raises(m.parse_symop, bad), f"refused: {bad}")
check(m.wrap_fraction(-1e-17) == 0.0, "a rounding error below 0 isn't 1")
check(m.wrap_fraction(-0.25) == 0.75 and m.wrap_fraction(1.5) == 0.5, "into the cell")

# Each of these stops the run


def with_changes(*pairs):
    text = CIF
    for a, b in pairs:
        assert a in text
        text = text.replace(a, b)
    return m.parse_cif(text)


def refused(*pairs):
    v, l = with_changes(*pairs)
    return raises(lambda: m.check_crystal(SALT, v, l, m.expand_cell(v, l, [])))


check(refused(("_cod_database_code 1000041", "_cod_database_code 1000042")), "another record")
check(refused(("Sodium chloride", "Potassium bromide")), "another name")
check(refused(("'Cl Na'", "'Br K'")), "another formula")
check(refused(("_cell_length_a                   5.62(1)", "_cell_length_a                   6.60")), "another cell")
check(refused(("x+1/2,y+1/2,z\n", "")), "symmetry that doesn't give the formula")
check(refused(("Cl1 Cl-1 0.5 0.5 0.5 1.", "Cl1 Cl-1 0.4 0.5 0.5 1.")), "distance against the published one")
check(refused(("Cl1 Cl-1 0.5 0.5 0.5 1.", "Cl1 Cl-1 0.5 0.5 0.5 0.5")), "part-filled site")
check(raises(m.parse_cif, "loop_\n_a\n_b\n1 2 3\n"), "uneven loop")
check(raises(m.element_of, "123", ""), "no element")
check(m.element_of("O1", "O-2") == "O" and m.element_of("D1", "") == "H" and m.element_of("Cl1", "") == "Cl", "elements")

# The list: every crystal the prompt names, and names that mean one thing
keys = [c["key"] for c in m.CRYSTALS]
for key in ("salt", "diamond", "ice", "graphite"):
    check(key in keys, f"{key} listed")
check(len(set(c["cod"] for c in m.CRYSTALS)) == len(keys), "each its own record")
check(m.c_string("abc") == '"abc"', "plain text")
check(raises(m.c_string, 'a"b'), "a quote can't break out of the C string")

print("crystal generator:", "ok" if failures == 0 else f"{failures} failures")
sys.exit(1 if failures else 0)
