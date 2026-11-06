#!/usr/bin/env python3
"""Builds webpage2/data/protein/ from the RCSB PDB: an index and one compact file per structure.

Backbone only: CA atoms for protein, P atoms for DNA and RNA. First model, first alternate position.
Every ID is checked against its PDB record before anything is written: a wrong ID is an invented
model, so one failed check stops the whole run and writes nothing. Search is never used, because
RCSB's search ranks mutants first ("lysozyme" gives 168L).

Licence: the PDB archive and RCSB's APIs are CC0 1.0 (https://www.rcsb.org/pages/usage-policy).
Attribution to the original authors is encouraged, so SOURCES.txt lists them for each entry.

Usage: python3 -I scripts/make_protein_data.py [--cache DIR] [--out DIR]
Downloads with curl into the cache directory (a new temporary one by default).
"""
import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile

MAX_POINTS = 1500
MAX_LABEL = 22  # With " - RCSB PDB 4HHB" this just fits the small font on the bottom line
GAP_CA = 4.6  # Angstrom; a longer step isn't a real CA-CA bond but a missing stretch of the chain
GAP_P = 9.0
USER_AGENT = "Zitro"

# PDB ID, German label, English label, more names, what its title or molecule name must say.
# Labels are plain ASCII: the small footer font has no umlauts, and the larger one would cut the name.
STRUCTURES = [
    ("4HHB", "Haemoglobin", "Hemoglobin", ["haemoglobin", "hemoglobin", "hemoglobin a"], r"HA?EMOGLOBIN"),
    ("1MBN", "Myoglobin", "Myoglobin", [], r"MYOGLOBIN"),
    ("1LYZ", "Lysozym", "Lysozyme", ["lysozyme", "lysozym"], r"LYSOZYME"),
    ("4INS", "Insulin", "Insulin", ["insuline"], r"INSULIN"),
    ("1EMA", "GFP Leuchtprotein", "GFP glowing protein",
     ["gfp", "green fluorescent protein", "gruen fluoreszierendes protein", "leuchtprotein"],
     r"GREEN FLUORESCENT PROTEIN"),
    ("7CAT", "Katalase", "Catalase", ["catalase", "katalase"], r"CATALASE"),
    ("2PTN", "Trypsin", "Trypsin", [], r"TRYPSIN"),
    ("4CHA", "Chymotrypsin", "Chymotrypsin", [], r"CHYMOTRYPSIN"),
    ("4PEP", "Pepsin", "Pepsin", [], r"PEPSIN"),
    ("1SMD", "Speichel-Amylase", "Salivary amylase",
     ["amylase", "alpha amylase", "speichelamylase", "ptyalin"], r"AMYLASE"),
    ("1HPL", "Lipase", "Lipase", ["pancreatic lipase", "pankreas lipase"], r"LIPASE"),
    ("1CAG", "Kollagen-Modell", "Collagen model", ["collagen", "kollagen", "collagen like peptide"], r"COLLAGEN"),
    ("3TNU", "Keratin-Stueck", "Keratin piece", ["keratin"], r"KERATIN"),
    ("1IGT", "Antikoerper", "Antibody",
     ["antibody", "antikoerper", "immunoglobulin", "immunglobulin", "igg"], r"IMMUNOGLOBULIN|ANTIBODY"),
    ("1J6Z", "Aktin", "Actin", ["actin", "aktin"], r"ACTIN"),
    ("2MYS", "Myosin", "Myosin", [], r"MYOSIN"),
    ("1TUB", "Tubulin", "Tubulin", [], r"TUBULIN"),
    ("1BG2", "Kinesin", "Kinesin", [], r"KINESIN"),
    ("1UBQ", "Ubiquitin", "Ubiquitin", [], r"UBIQUITIN"),
    ("1HRC", "Cytochrom c", "Cytochrome c", ["cytochrome c", "cytochrom c", "cytochrome"], r"CYTOCHROME C"),
    ("7RSA", "Ribonuklease A", "Ribonuclease A",
     ["ribonuclease", "ribonuclease a", "ribonuklease", "ribonuklease a", "rnase", "rnase a"], r"RIBONUCLEASE A"),
    ("1PPB", "Thrombin", "Thrombin", [], r"THROMBIN"),
    ("1AKE", "Adenylatkinase", "Adenylate kinase", ["adenylate kinase", "adenylatkinase"], r"ADENYLATE KINASE"),
    ("6LU7", "Corona-Protease", "Covid main protease",
     ["covid protease", "covid main protease", "coronavirus protease", "corona protease", "sars cov 2 main protease"],
     r"COVID-19 MAIN PROTEASE"),
    ("6M0J", "Corona-Spike mit ACE2", "Covid spike with ACE2",
     ["spike protein", "spike", "coronavirus spike", "covid spike", "corona spike", "spike protein ace2", "ace2"],
     r"SPIKE"),
    ("1HVR", "HIV-Protease", "HIV protease", ["hiv protease", "hiv 1 protease"], r"HIV-1 PROTEASE"),
    ("1NN2", "Neuraminidase", "Neuraminidase", ["flu neuraminidase", "influenza neuraminidase", "grippe neuraminidase"],
     r"NEURAMINIDASE"),
    ("8ADH", "Alkohol-Dehydrogenase", "Alcohol dehydrogenase",
     ["alcohol dehydrogenase", "alkohol dehydrogenase", "alkoholdehydrogenase", "adh"], r"ALCOHOL DEHYDROGENASE"),
    ("1LCI", "Luciferase", "Luciferase",
     ["firefly luciferase", "gluehwuermchen luciferase", "luziferase"],
     r"LUCIFERASE"),
    ("3RUB", "RuBisCO", "RuBisCO", ["rubisco", "ribulose bisphosphate carboxylase"], r"RIBULOSE"),
    ("2MLT", "Melittin (Bienengift)", "Melittin (bee venom)", ["melittin"], r"MELITTIN"),
    ("1MSI", "Frostschutz-Protein", "Antifreeze protein",
     ["antifreeze protein", "frostschutzprotein", "frostschutz protein", "fish antifreeze protein"], r"ANTIFREEZE"),
    ("1F88", "Rhodopsin", "Rhodopsin", ["sehpurpur"], r"RHODOPSIN"),
    ("1J4N", "Aquaporin", "Aquaporin", ["water channel", "wasserkanal", "aquaporin 1"], r"AQUAPORIN|AQP1"),
    ("1BL8", "Kaliumkanal", "Potassium channel", ["potassium channel", "kaliumkanal", "kcsa"], r"POTASSIUM CHANNEL"),
    ("1CEL", "Cellulase", "Cellulase", ["cellobiohydrolase", "zellulase"], r"CELLOBIOHYDROLASE"),
    ("1SBT", "Subtilisin", "Subtilisin", [], r"SUBTILISIN"),
    ("9PAP", "Papain", "Papain", [], r"PAPAIN"),
    ("3LA4", "Urease", "Urease", [], r"UREASE"),
    ("4CMS", "Chymosin (Lab)", "Chymosin (rennin)", ["chymosin", "rennin", "labferment"], r"CHYMOSIN"),
    ("1TAQ", "Taq-Polymerase", "Taq polymerase", ["taq polymerase", "taq dna polymerase", "dna polymerase"], r"TAQ DNA POLYMERASE"),
    ("4UN3", "Cas9 an DNA", "Cas9 on DNA", ["cas9", "crispr", "crispr cas9", "genschere", "gene scissors"], r"CAS9"),
    ("1AOI", "Nukleosom", "Nucleosome", ["nucleosome", "nukleosom", "histone", "histon", "histones"], r"NUCLEOSOME"),
    ("1BNA", "DNA", "DNA",
     ["dna", "dns", "dna double helix", "dna doppelhelix", "double helix", "doppelhelix", "b dna",
      "deoxyribonucleic acid", "desoxyribonukleinsaeure", "dna molecule", "dna molekuel"], r"B-DNA"),
    ("1EHZ", "tRNA", "tRNA", ["trna", "transfer rna", "rna", "rns", "ribonucleic acid"], r"TRNA|TRANSFER RNA"),
    ("1AO6", "Serumalbumin", "Serum albumin", ["serum albumin", "serumalbumin", "albumin"], r"SERUM ALBUMIN"),
    ("1THW", "Thaumatin", "Thaumatin", [], r"THAUMATIN"),
    ("1TIT", "Titin-Stueck", "Titin piece", ["titin"], r"TITIN"),
    ("3HHR", "Wachstumshormon", "Growth hormone",
     ["growth hormone", "wachstumshormon", "human growth hormone"], r"GROWTH HORMONE"),
    ("1TUP", "p53 an DNA", "p53 on DNA", ["p53", "tumor suppressor p53"], r"P53"),
]


def normalise(name):
    """Same rules as proteinNormalise in protein_core.h. None if a character isn't allowed."""
    for a, b in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("Ä", "ae"), ("Ö", "oe"), ("Ü", "ue"), ("ß", "ss")):
        name = name.replace(a, b)
    out = []
    for c in name:
        if c in "-_ \t":
            if out and out[-1] != " ":
                out.append(" ")
        elif c in "|;" or ord(c) < 0x20 or ord(c) >= 0x7F:
            return None
        else:
            out.append(c.lower())
    return "".join(out).strip()


def looks_like_pdb_id(text):
    return re.fullmatch(r"[1-9][0-9a-zA-Z]{3}", text) is not None


def pdb_text_fields(pdb):
    """Title and molecule names from the record's header, in upper case."""
    title, compnd = [], []
    for line in pdb.splitlines():
        if line.startswith("TITLE "):
            title.append(line[10:80].strip())
        elif line.startswith("COMPND"):
            compnd.append(line[10:80].strip())
        elif line.startswith(("ATOM  ", "HETATM")):
            break
    return " ".join(title).upper(), " ".join(compnd).upper()


def check_record(pdb_id, pdb, pattern):
    """Raises ValueError unless this file is the right, current record."""
    if not pdb.startswith("HEADER") or pdb[62:66] != pdb_id:
        raise ValueError(f"{pdb_id}: file header names another entry")
    if re.search(r"^OBSLTE", pdb, re.M):
        raise ValueError(f"{pdb_id}: this entry is obsolete")
    title, compnd = pdb_text_fields(pdb)
    if not re.search(pattern, title + " " + compnd):
        raise ValueError(f"{pdb_id}: '{pattern}' not in title '{title}' or molecule '{compnd}'")
    return title


def parse_backbone(pdb):
    """Chains of backbone points: [(chain id, [(x, y, z, gap_before, kind), ...]), ...], first model only."""
    seqres = {}
    chains = {}
    order = []
    seen = set()
    for line in pdb.splitlines():
        record = line[:6]
        if record == "SEQRES":
            seqres.setdefault(line[11], set()).update(line[19:70].split())
        elif record == "ENDMDL":
            break
        elif record in ("ATOM  ", "HETATM"):
            name = line[12:16]
            if name not in (" CA ", " P  "):  # Calcium is "CA  ", so it never matches
                continue
            chain = line[21]
            if record == "HETATM" and line[17:20].strip() not in seqres.get(chain, ()):
                continue  # A ligand. Modified residues of the chain itself are listed in SEQRES.
            key = (chain, line[22:27])
            if key in seen:
                continue  # Another alternate position
            seen.add(key)
            point = (float(line[30:38]), float(line[38:46]), float(line[46:54]))
            if not all(math.isfinite(v) for v in point):
                raise ValueError("coordinate is not a number")
            if chain not in chains:
                chains[chain] = []
                order.append(chain)
            chains[chain].append(point + ("CA" if name == " CA " else "P",))
    result = []
    for chain in order:
        points = []
        previous = None
        for x, y, z, kind in chains[chain]:
            gap = False
            if previous is not None:
                limit = GAP_CA if kind == "CA" and previous[3] == "CA" else GAP_P
                gap = math.dist((x, y, z), previous[:3]) > limit
            points.append((x, y, z, gap, kind))
            previous = (x, y, z, kind)
        result.append((chain, points))
    return result


def format_structure(pdb_id, chains):
    """The compact file. Coordinates in tenths of an Angstrom, around the centre of the structure."""
    total = sum(len(points) for _, points in chains)
    if total < 2:
        raise ValueError(f"{pdb_id}: no backbone")
    if total > MAX_POINTS:
        raise ValueError(f"{pdb_id}: {total} points, the limit is {MAX_POINTS}")
    mean = [sum(p[i] for _, points in chains for p in points) / total for i in range(3)]
    lines = [f"ZITRO-PROTEIN 1 {pdb_id} {total} {len(chains)}"]
    for _, points in chains:
        lines.append(f"C {len(points)}")
        for x, y, z, gap, _kind in points:
            if gap:
                lines.append("G")
            lines.append(f"{round((x - mean[0]) * 10)} {round((y - mean[1]) * 10)} {round((z - mean[2]) * 10)}")
    lines.append("END")
    return "\n".join(lines) + "\n"


def build_index(structures):
    """The index file. Raises ValueError for a name that can't be resolved cleanly."""
    lines = [f"ZITRO-PROTEIN-INDEX 1 {len(structures)}"]
    owner = {}
    ids = set()
    for pdb_id, de, en, more, _pattern in structures:
        if pdb_id in ids:
            raise ValueError(f"{pdb_id}: listed twice")
        ids.add(pdb_id)
        for label in (de, en):
            if len(label) > MAX_LABEL or not label.isascii() or "|" in label or ";" in label:
                raise ValueError(f"{pdb_id}: label '{label}' doesn't fit the bottom line")
        names = []
        for name in [de, en] + more:
            key = normalise(name)
            if not key:
                raise ValueError(f"{pdb_id}: name '{name}' has characters that can't be matched")
            if looks_like_pdb_id(key):
                raise ValueError(f"{pdb_id}: name '{name}' looks like a PDB ID; the AI must not pass IDs")
            if key in owner and owner[key] != pdb_id:
                raise ValueError(f"name '{key}' means both {owner[key]} and {pdb_id}")
            owner[key] = pdb_id
            if key not in names:
                names.append(key)
        lines.append(f"{pdb_id}|{de}|{en}|{';'.join(names)}")
    lines.append("END")
    return "\n".join(lines) + "\n"


def download(url, path):
    if not os.path.exists(path):
        partial = path + ".part"
        subprocess.run(["curl", "-sSfL", "-A", USER_AGENT, "-m", "60", "-o", partial, url], check=True)
        os.replace(partial, path)
    with open(path, encoding="latin-1") as f:
        return f.read()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", help="directory for downloads, kept between runs")
    parser.add_argument("--out", default=os.path.join(here, "..", "webpage2", "data", "protein"))
    args = parser.parse_args()
    cache = args.cache or tempfile.mkdtemp(prefix="zitro_pdb_")
    os.makedirs(cache, exist_ok=True)

    files = {}
    sources = ["Backbones from the RCSB PDB, licence CC0 1.0: https://www.rcsb.org/pages/usage-policy",
               "ID | name | points | chains | PDB title | authors", ""]
    failures = []
    for pdb_id, de, en, more, pattern in STRUCTURES:
        try:
            pdb = download(f"https://files.rcsb.org/download/{pdb_id}.pdb", os.path.join(cache, pdb_id + ".pdb"))
            title = check_record(pdb_id, pdb, pattern)
            # An obsolete entry can still be served as a file, but its record is gone from the current archive
            entry = json.loads(download(f"https://data.rcsb.org/rest/v1/core/entry/{pdb_id}",
                                        os.path.join(cache, pdb_id + ".json")))
            if entry.get("rcsb_id") != pdb_id:
                raise ValueError(f"{pdb_id}: not in the current archive")
            chains = parse_backbone(pdb)
            files[pdb_id.lower() + ".txt"] = format_structure(pdb_id, chains)
            total = sum(len(points) for _, points in chains)
            authors = ", ".join(a.get("name", "") for a in entry.get("audit_author", []))
            sources.append(f"{pdb_id} | {en} | {total} | {len(chains)} | {title} | {authors}")
            print(f"{pdb_id} {en}: {total} points, {len(chains)} chains: {title}")
        except (ValueError, subprocess.CalledProcessError, json.JSONDecodeError) as e:
            failures.append(f"{pdb_id}: {e}")
    if failures:
        print("Nothing written. Failed:\n  " + "\n  ".join(failures), file=sys.stderr)
        return 1
    files["index.txt"] = build_index(STRUCTURES)
    files["SOURCES.txt"] = "\n".join(sources) + "\n"
    os.makedirs(args.out, exist_ok=True)
    for name, text in files.items():
        with open(os.path.join(args.out, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    size = sum(len(t) for t in files.values())
    print(f"{len(files)} files, {size} bytes, largest {max(len(t) for t in files.values())} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
