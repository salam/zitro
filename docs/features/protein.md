# Proteins, DNA and crystals in 3D

Kinds `protein` and `crystal`. Files: `feature_protein.ino`, `protein_core.h`, `feature_crystal.ino`, `crystal_core.h`, generated `crystal_table.h`, `scripts/make_protein_data.py`, `scripts/make_crystal_table.py`, `webpage2/data/protein/`, `tests/protein/`.

## Requirements

### Proteins, enzymes, DNA

- `protein:<name>` resolves through `protein/index.txt` on the Zitro server, exactly: lower case, umlauts spelled out, hyphens and repeated spaces as one space. There is no fuzzy match, no search and no fallback to RCSB.
- A name that isn't listed: `fetch` returns false, so the picture shows. The same goes for a PDB ID as the query (`protein:4HHB`), general words (`protein:enzyme`), a broken or cut-off file, and a file naming another record.
- The AI never supplies a PDB ID. The ID comes only from the index, and must be 4 characters, a digit 1-9 followed by digits or capitals, before it becomes a file name.
- The bottom line shows the short name in the device language and `RCSB PDB <id>`. Names are plain ASCII (`Haemoglobin`, `Antikoerper`): the small footer font has no umlauts, and in the larger font the name would be cut to fit the source. The generator refuses labels over 22 characters or with other characters.
- Backbone only: CA atoms for protein, P atoms for DNA and RNA. First model, first alternate position per residue. Modified residues count when the chain's `SEQRES` lists them; ligands, water and calcium don't.
- A stretch missing from the record stays a gap (CA-CA over 4.6 A, or any step involving P over 9 A): no line crosses it. An isolated point is drawn as a dot.
- At most 1500 points, 64 chains, 32 KB per file, 16 KB for the index. Two downloads inside `EDU_LOOKUP_DEADLINE_MS`.
- Drawing: thick line along the backbone, blue to red from start to end, restarting per chain, depth-sorted by line segment. Farther lines are darker. Line width is 5, 3 or 2 pixels by size of the model, one more when zoomed.
- `turnsWithGyro` true. Button B zooms, Button C goes back to the text.
- Data and work buffers live in PSRAM; everything is freed in `unload`/`end`. No network while the view is open.
- `scripts/make_protein_data.py` checks each ID before writing anything: file header names that entry, no `OBSLTE` record, the entry exists in the current archive (`data.rcsb.org`), and title or molecule name match the expected word. One failure writes nothing. Names must be unique across records and must not look like a PDB ID.

What a model shows is the deposited record (the asymmetric unit), not always the complete assembly in the body: catalase is a tetramer in the body and 2 chains in 7CAT. Records of parts are labelled as such where the name would mislead (`Kollagen-Modell`, `Keratin-Stueck`, `Titin-Stueck`, `Corona-Spike mit ACE2`).

Changes from the 3D view design (`docs/superpowers/specs/2026-11-05-3d-view-design.md`), as the user asked: the table lives on the server, not in firmware; there is no RCSB search for other names; DNA is `protein:dna`, not a kind of its own.

### Crystals

- `crystal:<name>` is a closed list in firmware, needing no network: `salt`, `diamond`, `graphite`, `ice`, `quartz`, `calcite`, `pyrite`, `fluorite`, `copper`, `gold`, `silver`, `iron`, `silicon`, plus listed English and German names (`table salt`, `Kochsalz`). Anything else: `fetch` returns false, so the picture shows.
- Atoms are computed on the stick from the published unit cell: `crystal_table.h` holds the cell and all atoms of one cell, expanded from the COD record's symmetry operations by `scripts/make_crystal_table.py`. The stick repeats the cell (1 to 3 times per axis), including atoms on the far faces.
- Bonds are drawn by distance rules per crystal, limited to at most 200 atoms and 400 bonds. In metals the lines mark nearest neighbours, and in ice the O-O lines are hydrogen bonds.
- Atoms are depth-sorted filled circles in element colours (`elementColor565`) with a legend, like molecules. The bottom line shows the name and `COD <id>`.
- **Ice shows oxygen only**, and says so on screen ("Eis (nur O-Atome)"). The record's hydrogen sites are half filled, so there is no one true place to draw them.
- The generator refuses a record if: its code, name, formula or cell differ from what's expected; the atoms in the cell don't match formula times Z; a shown distance is more than 0.03 A from the published value; or a site other than ice's hydrogens is part-filled. One failure writes nothing. Symmetry operations are parsed, never evaluated.

### Checking on the stick

Serial `model protein:hemoglobin` and `model crystal:salt` open a result without voice. Leaving the view prints its frames per second. The drawing speed has not been measured yet: 1424 points (`protein:cas9`) is the heaviest model. If it's too slow, cut `proteinLineWidth` first.

Regenerate with `python3 -I scripts/make_protein_data.py` and `python3 -I scripts/make_crystal_table.py`. Tests: `tests/protein/run.sh`.

### Prompt

Both kinds set `promptRule`: only for questions about structure or explicit 3D requests. "What is an enzyme?", "What is insulin for?", "What is a diamond?" stay a photo.

## User guide

Ask how something is built, or ask for 3D:

- "Wie ist Hämoglobin aufgebaut?", "Zeig mir DNA in 3D"
- "Wie ist Kochsalz aufgebaut?", "Show me a diamond in 3D"

Zitro then shows a real model that you turn with your hand. It stays where you turned it; after 3 seconds lying still, it turns slowly by itself.

**Proteins and DNA** are a coloured ribbon that follows the molecule's chain. Each chain starts blue and ends red. Zitro knows about 50 famous ones, such as hemoglobin, insulin, lysozyme, GFP, antibodies, enzymes like pepsin and amylase, and DNA. Is yours not there? You get the usual picture. Models come from the Zitro server, so they need WiFi.

**Crystals** show the atoms in their real places, each element in its own colour. Zitro knows salt, diamond, graphite, ice, quartz, calcite, pyrite, fluorite, copper, gold, silver, iron and silicon. Ice shows only its oxygen atoms.

Questions like "What is an enzyme?" or "What is a diamond?" still get a photo.

The bottom line names what you see and where it comes from, such as "Haemoglobin - RCSB PDB 4HHB".

| Button | What it does |
| --- | --- |
| A | Ask something new |
| B | Zoom in, three steps |
| C | Show the written answer |

## Release note

- Turn real proteins, DNA and crystals in your hand: ask "How is hemoglobin built?" or "Show me a diamond in 3D".

## Sources and licences

**RCSB PDB:** the PDB archive and RCSB's APIs are CC0 1.0 (https://www.rcsb.org/pages/usage-policy, checked 2026-11-06). Attribution to the original authors is encouraged, not required: `webpage2/data/protein/SOURCES.txt` lists title and authors per entry. Molecule of the Month art, which has its own licence, is not used.

**Crystallography Open Database:** all data is public domain under CC0 (https://www.crystallography.net/cod/, checked 2026-11-06). Users should acknowledge the original authors, so each record is below and in `crystal_table.h`.

| Crystal | COD | a (Angstrom) | Paper |
| --- | --- | --- | --- |
| salt | 1000041 | 5.62 | Abrahams, Bernstein: Accuracy of an automatic diffractometer, Acta Cryst. 1965 |
| diamond | 9008564 | 3.56679 | Wyckoff: Crystal Structures, 2nd ed., 1963 |
| graphite | 9011577 | 2.464 | Structure of graphite by neutron diffraction, Nature 1975 |
| ice | 1538173 | 4.506 | The electron density distribution in ice Ih, J. Chem. Phys. 1990 |
| quartz | 5000035 | 4.91239 | Crystal structures of quartz and magnesium germanate, J. Appl. Cryst. 1988 |
| calcite | 9000095 | 4.99 | Crystallographic tables for the rhombohedral carbonates, Am. Mineralogist 1961 |
| pyrite | 9000594 | 5.4166 | Crystal structure refinement of a weakly anisotropic pyrite, Am. Mineralogist 1977 |
| fluorite | 1000043 | 5.462 | Defect structure of calcium fluoride containing excess anions, J. Phys. C 1971 |
| copper | 9008468 | 3.61496 | Wyckoff: Crystal Structures, 2nd ed., 1963 |
| gold | 9008463 | 4.07825 | Wyckoff: Crystal Structures, 2nd ed., 1963 |
| silver | 9008459 | 4.0862 | Wyckoff: Crystal Structures, 2nd ed., 1963 |
| iron | 5000217 | 2.8608 | A low-temperature X-ray camera, J. Sci. Instruments 1954 |
| silicon | 9008565 | 5.4307 | Wyckoff: Crystal Structures, 2nd ed., 1963 |

**1CAT is obsolete.** The brief named it for catalase and its file still downloads, but RCSB removed the entry in 1982 (its record says `OBSLTE`, replaced by 3CAT, itself no longer current). 7CAT, beef liver catalase, is used instead. The generator refuses obsolete entries, which also ruled out 1ACE.
