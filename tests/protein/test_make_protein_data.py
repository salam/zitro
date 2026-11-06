#!/usr/bin/env python3
"""Tests the generator's parsing and its checks, with no downloads. Run by tests/protein/run.sh."""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import make_protein_data as m  # noqa: E402

failures = 0


def check(condition, what):
    global failures
    if not condition:
        failures += 1
        print("  FAIL", what)


def raises(fn, *args):
    try:
        fn(*args)
    except ValueError:
        return True
    return False


def atom(record, serial, name, alt, res, chain, seq, x, y, z, el):
    return f"{record:<6}{serial:>5} {name:<4}{alt:1}{res:>3} {chain:1}{seq:>4}    {x:>8.3f}{y:>8.3f}{z:>8.3f}  1.00 10.00          {el:>2}"


HEADER = (
    "HEADER    OXYGEN TRANSPORT                        07-MAR-84   4HHB              \n"
    "TITLE     THE CRYSTAL STRUCTURE OF HUMAN DEOXYHAEMOGLOBIN\n"
    "COMPND    MOL_ID: 1; MOLECULE: HEMOGLOBIN\n"
    "SEQRES   1 A    3  ALA MSE GLY                                                \n"
)

# Checks parse_backbone against each rule in turn
PDB = HEADER + "\n".join(
    [
        atom("ATOM", 1, " N", "", "ALA", "A", 1, 0, 0, 0, "N"),
        atom("ATOM", 2, " CA", "A", "ALA", "A", 1, 1.0, 0, 0, "C"),
        atom("ATOM", 3, " CA", "B", "ALA", "A", 1, 50.0, 50, 50, "C"),  # Second alternate position
        atom("HETATM", 4, " CA", "", "MSE", "A", 2, 4.0, 0, 0, "C"),  # Modified residue, in SEQRES
        atom("ATOM", 5, " CA", "", "GLY", "A", 3, 7.0, 0, 0, "C"),
        atom("ATOM", 6, " CA", "", "GLY", "A", 40, 30.0, 0, 0, "C"),  # Gap
        "TER",
        atom("HETATM", 7, "CA", "", "CA", "A", 201, 1, 1, 1, "CA"),  # Calcium, not an alpha carbon
        atom("HETATM", 8, " P", "", "PO4", "A", 202, 2, 2, 2, "P"),  # Ligand with phosphorus
        atom("HETATM", 9, " CA", "", "XYZ", "A", 203, 3, 3, 3, "C"),  # Ligand that's not in SEQRES
        atom("HETATM", 10, " O", "", "HOH", "A", 301, 4, 4, 4, "O"),
        atom("ATOM", 11, " P", "", " DG", "B", 1, 0, 10, 0, "P"),
        atom("ATOM", 12, " P", "", " DC", "B", 2, 6, 10, 0, "P"),
        "ENDMDL",
        atom("ATOM", 13, " CA", "", "ALA", "C", 1, 9, 9, 9, "C"),  # Second model
    ]
) + "\n"

chains = m.parse_backbone(PDB)
check([c for c, _ in chains] == ["A", "B"], "first model only, chains in file order")
a = chains[0][1]
check(len(a) == 4, "CA of protein residues, once each, no ligands or calcium")
check([p[0] for p in a] == [1.0, 4.0, 7.0, 30.0], "first alternate position, modified residue kept")
check([p[3] for p in a] == [False, False, False, True], "a missing stretch is marked as a gap")
b = chains[1][1]
check(len(b) == 2 and b[0][4] == "P" and not b[1][3], "P atoms for DNA, 6 A is no gap")

text = m.format_structure("4HHB", chains)
lines = text.splitlines()
check(lines[0] == "ZITRO-PROTEIN 1 4HHB 6 2", "header")
check(lines[1] == "C 4" and "G" in lines and lines[-1] == "END", "chain, gap and end markers")
check(raises(m.format_structure, "4HHB", [("A", [])]), "empty model refused")
many = [("A", [(float(i), 0.0, 0.0, False, "CA") for i in range(m.MAX_POINTS + 1)])]
check(raises(m.format_structure, "4HHB", many), "over the limit refused")

# A wrong ID is an invented model: each of these stops the run
check(m.check_record("4HHB", PDB, r"HA?EMOGLOBIN"), "right record passes")
check(raises(m.check_record, "4HHB", PDB, r"LYSOZYME"), "another protein's title refused")
check(raises(m.check_record, "1LYZ", PDB, r"HA?EMOGLOBIN"), "file of another entry refused")
check(raises(m.check_record, "4HHB", "<html>404</html>", r"."), "not a PDB file refused")
obsolete = HEADER + "OBSLTE     21-OCT-82 4HHB      3CAT\n"
check(raises(m.check_record, "4HHB", obsolete, r"HA?EMOGLOBIN"), "obsolete entry refused")

entry = ("4HHB", "Haemoglobin", "Hemoglobin", [], "X")
check(raises(m.build_index, [entry, entry]), "same ID twice refused")
check(raises(m.build_index, [entry, ("1LYZ", "Lysozym", "Hemoglobin", [], "X")]), "one name for two IDs refused")
check(raises(m.build_index, [("4HHB", "Hb", "Hb", ["1lyz"], "X")]), "a name that looks like an ID refused")
check(raises(m.build_index, [("4HHB", "Hb", "Hb", ["a;b"], "X")]), "separator in a name refused")
check(raises(m.build_index, [("4HHB", "Hb" * 20, "Hb", [], "X")]), "label too long refused")
check(raises(m.build_index, [("4HHB", "Hämoglobin", "Hb", [], "X")]), "non-ASCII label refused")
index = m.build_index([entry])
check(index == "ZITRO-PROTEIN-INDEX 1 1\n4HHB|Haemoglobin|Hemoglobin|haemoglobin;hemoglobin\nEND\n", "index format")

# Same rules as proteinNormalise in protein_core.h
for given, wanted in [("Hemoglobin", "hemoglobin"), ("  DNA  ", "dna"), ("HIV-1  protease", "hiv 1 protease"),
                      ("Hämoglobin", "haemoglobin"), ("Übörß", "ueboerss"), ("a|b", None),
                      ("α-amylase", None), ("---", "")]:
    check(m.normalise(given) == wanted, f"normalise {given!r}")

# The real list
ids = [s[0] for s in m.STRUCTURES]
check(30 <= len(ids) <= 50, "about 30 to 50 structures")
check("1CAT" not in ids, "1CAT is obsolete: 7CAT replaces it")
m.build_index(m.STRUCTURES)

print("generator:", "ok" if failures == 0 else f"{failures} failures")
sys.exit(1 if failures else 0)
