# Anatomy: organs and cells in 3D

## Requirements

- `model` = `anatomy:<id>`. Closed list, case ignored, nothing else forgiven: `heart`, `brain`, `lungs`, `stomach`, `liver`, `kidney`, `skeleton`, `skull`, `eye`, `hand`, `animal-cell`, `plant-cell`, `red-blood-cell`, `neuron`.
- The prompt rule sets it only when someone asks how a body part or cell looks inside or is built, or asks for a 3D model. "What does the heart do?" stays a photo. No reproductive organs and no nude figures: none is on the list, and the data script takes parts only from a whitelist of names, so nothing else in the source sets can reach a file.
- An id not on the list, a failed download or a file that fails the check: `fetch()` returns false and the normal picture shows.
- The file name comes from the closed list, never from the query.
- Files: `EDU_DATA_BASE_URL "anatomy/<id>.bin"`, format `ZAN1`, described at the top of `anatomy_core.h`. At most 2500 vertices, 3500 triangles, 8 colours; `ANATOMY_BODY_LIMIT` is 39,620 bytes, in PSRAM.
- Before use the whole file is checked: magic, flags, counts against the caps before any multiplication, exact length, CRC-32, id equals the requested id, text fields terminated and valid UTF-8, every index in range, no degenerate triangle, colour index in range, coordinates within radius 1 plus rounding.
- Label from the file, in the device language. Footer: label and source short form.
- A file with the schematic flag shows "Schema" / "Schematic" as its source, and "Vereinfachtes Schema" / "Simplified schematic" on an amber tag in every frame. The firmware decides this from the flag, so a schematic can't show as data.
- Drawing: flat-shaded faces with a fixed light, faces turned away dropped, far to near by 256 depth buckets, cost linear in triangles whatever the turn. Working arrays in internal RAM if there's room, otherwise PSRAM: 12 KB for the largest model, 22.5 KB at the file limits. All freed in `end()` and `unload()`.
- `turnsWithGyro` true. Button B: zoom, 3 steps. Button C: back to the text.
- Budget: 25 frames per second. See the table. Every 5 s the serial log prints `anatomy <id> triangles= drawnMax= zoom= fps= drawMs= drawMaxMs= work= sprite= freeHeap=`. **Not yet measured on a stick.**

| id | triangles | most drawn in a frame (180 turns tested on the host) | bytes | source |
| --- | --- | --- | --- | --- |
| heart | 1383 | 641 | 14,331 | BodyParts3D |
| brain | 1463 | 730 | 15,191 | BodyParts3D |
| lungs | 1376 | 719 | 15,056 | Human Reference Atlas |
| stomach | 900 | 457 | 9,132 | BodyParts3D |
| liver | 1189 | 650 | 12,175 | BodyParts3D |
| kidney | 900 | 535 | 9,132 | BodyParts3D |
| skeleton | 1802 | 781 | 20,870 | BodyParts3D |
| skull | 1538 | 758 | 16,154 | BodyParts3D |
| eye | 998 | 503 | 10,172 | BodyParts3D |
| hand | 1174 | 605 | 12,214 | BodyParts3D |
| animal-cell | 1340 | 676 | 14,408 | Schematic |
| plant-cell | 820 | 394 | 8,536 | Schematic |
| red-blood-cell | 1012 | 462 | 10,252 | Schematic |
| neuron | 1072 | 515 | 11,128 | Schematic |

Limits of the shapes, on purpose or from the data:

- **Organs show their outer shape.** They aren't cut open, even when the question was about the inside. Only the cell schematics show an inside.
- **Heart:** BodyParts3D has no ventricle wall. The body of the heart is the cavities and the atrial walls, with the great vessels and three main coronary arteries.
- **Eye:** the clear cornea is left out so the iris can be seen; the lens is drawn dark so the opening reads as a pupil.
- **Brain:** the weakest shape. Each fold is reduced on its own, which leaves hard facets, and it reads best from the side, so it opens side-on.
- **Lungs:** some small gaps remain between the segments.
- **Skeleton:** over the 1500 target because it has 214 parts. Its triangles are small. It has no hyoid and is drawn in the source's standing pose.
- Colours are chosen to be sensible, not measured.
- Kidney, hand and eye are the right-hand ones of the source body.

Build: `python -I scripts/make_anatomy_data.py --sources <folder>` needs numpy and fast-simplification. Tests: `tests/anatomy/run.sh`. Check by eye: build `tests/anatomy/render_bin.cpp`, then `tests/anatomy/preview.py`, which draws the real files at 240x135 with the device's own pipeline.

## User guide

Ask for a body part in 3D, or ask how a cell looks inside: "Zeig mir das Herz in 3D", "Zeig mir das Gehirn in 3D", "Wie ist eine Pflanzenzelle aufgebaut?"

Zitro knows the heart, brain, lungs, stomach, liver, kidney, skeleton, skull, eye and hand bones. It also knows the animal cell, plant cell, red blood cell and nerve cell. For anything else, or when a model can't be loaded, you see a picture as usual.

Turn the stick and the model turns with it. Button B zooms in. Button C shows the text answer. Button A asks something new.

The organs are real shapes from anatomy research, made simple enough for this small screen. They show the outside of the organ. The cells are drawn from what school books describe. They carry the sign "Vereinfachtes Schema", because they are simple drawings and not measurements.

## Release note

- Turn a real heart, brain or skeleton in your hand, and look inside a cell: ask Zitro how it looks in 3D.

## Sources and licences

**BodyParts3D 4.0** (Database Center for Life Science): heart, brain, stomach, liver, kidney, skeleton, skull, eye, hand. Downloaded from https://dbarchive.biosciencedbc.jp/en/bodyparts3d/download.html (`partof_BP3D_4.0_obj_99.zip`, and `isa_BP3D_4.0_obj_99.zip` for the teeth only).

- Required credit: "BodyParts3D, © The Database Center for Life Science licensed under CC Attribution 4.0 International".
- Changes to indicate: selected parts, reduced polygon count, recoloured, quantised.
- **The licence statements conflict.** The licence page and README (https://dbarchive.biosciencedbc.jp/en/bodyparts3d/lic.html, updated 2025-02-27) say CC BY 4.0. The header inside each OBJ file, dated 2013, says CC BY-SA 2.1 Japan. `index.json` states both and also offers the derived files under CC BY-SA 4.0, which satisfies both readings: clause 5.2 of the 2.1 Japan licence allows a later version with the same licence elements. Only the owner can decide whether to keep that or rely on the newer statement.

**Human Reference Atlas, 3D Reference Organ for Lung, Male, v1.3** (HuBMAP): lungs. https://lod.humanatlas.io/ref-organ/lung-male/v1.3/, file https://cdn.humanatlas.io/digital-objects/ref-organ/lung-male/v1.3/assets/3d-vh-m-lung.glb. The record gives CC BY 4.0 with no fixed wording, so `index.json` credits it by name. Used because BodyParts3D has bronchial trees and arteries for the lungs but no outer surface.

The bottom line is too short for the full credits, so it says "BodyParts3D CC BY" or "HuBMAP HRA CC BY". The skull says only "BodyParts3D": "Schädel" needs the wider font for its umlaut, and with "CC BY" as well the name itself would be cut. The full wording is in `webpage2/data/anatomy/index.json`, on the server beside the files.

**Schematics:** animal cell, plant cell, neuron and red blood cell are not data. `scripts/make_anatomy_data.py` generates them from fixed numbers, following what textbooks describe. Sizes and positions are not measurements. The red blood cell uses the Evans and Fung shape formula (Microvasc. Res. 4, 335, 1972) with R 3.91, C0 0.81, C2 7.83, C4 -4.39. The original paper wasn't read. One paper seen (arXiv 1605.04509) prints C2 as 7.38; 7.83 was kept because it gives the reported cell height of 2.58 µm, where 7.38 gives 2.41.

**Looked at, not used:** Z-Anatomy: its GitHub repository carries a GPL-3 licence, and its models weren't examined. NIH 3D: licences differ per model, and none was downloaded. Smithsonian Open Access: not examined. Human Reference Atlas heart: tried, and it looked worse than BodyParts3D here (no great vessels).
