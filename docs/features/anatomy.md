# Anatomy: organs and cells in 3D

## Requirements

- `model` = `anatomy:<id>` or `anatomy:<id>-inside`. Closed list, case of the id ignored, nothing else forgiven: `heart`, `brain`, `lungs`, `stomach`, `liver`, `kidney`, `skeleton`, `skull`, `eye`, `hand`, `animal-cell`, `plant-cell`, `red-blood-cell`, `neuron`. `-inside` exists for all but `stomach`, `skeleton` and `hand`.
- The prompt rule: "show me in 3D" sets `anatomy:<id>`. "What's inside", "how does it look inside" and "how is it built" set `anatomy:<id>-inside`, which opens the cutaway. "What does the heart do?" stays a photo. No reproductive organs and no nude figures: none is on the list, and the data script takes parts only from a whitelist of names, so nothing else in the source sets can reach a file.
- An id not on the list, `-inside` for something with no inside (`stomach`, `skeleton`, `hand`), a failed download or a file that fails the check: `fetch()` returns false and the normal picture shows.
- File names come from the closed list, never from the query.
- Files: `EDU_DATA_BASE_URL "anatomy/<id>.bin"` for the outside, `"anatomy/<id>-cut.bin"` for the cutaway. A cell has one file, `<id>.bin`, which is already cut open. Format `ZAN1`, described at the top of `anatomy_core.h`. At most 2500 vertices, 3500 triangles, 8 colours, 8 legend lines; `ANATOMY_BODY_LIMIT` is 39,949 bytes per file, in PSRAM.
- `fetch()` downloads what was asked for first, and that one must be there. Then the other view, within the same 10 seconds; if that one fails there's just no second view, and Button C goes straight to the text. The two downloads were not timed on a stick.
- Before use the whole file is checked: magic, flags, counts against the caps before any multiplication, exact length, CRC-32, id equals the requested id, text fields terminated and valid UTF-8, every index in range, no degenerate triangle, colour index in range, coordinates within radius 1 plus rounding. Also: the cut flag matches the view being loaded; the schematic flag matches the closed list, so a cell can't show as data and an organ can't show as a drawing; first filled triangle in range; legend count, colours and ASCII-only words.
- Label from the file, in the device language, plus ", innen" / ", inside" for a cutaway. Footer: label and source short form. Each view brings its own source: the outside of the heart says BodyParts3D, its cut says HuBMAP HRA.
- A file with the schematic flag shows "Schema" / "Schematic" as its source, and "Vereinfachtes Schema" / "Simplified schematic" on an amber tag in every frame. The firmware decides this from the flag, so a schematic can't show as data.
- Button A: new question. Button B: zoom, 3 steps. Button C: from the view asked for to the other view, if there is one, and then to the text answer. From the text it opens the view asked for again. So `heart`: outside, inside, text. `heart-inside`: inside, outside, text. `stomach` or a cell: straight to the text.
- Outside view: `turnsWithGyro` as before, with the idle spin.
- Cutaway: faces the viewer at all times, or there would be nothing to see but the back of half an organ. The frame undoes the shared idle spin, which is what made the old cell look like a plain ball, and rocks gently by 0.2 radians instead. The hand can turn it up to 55 degrees away from the cut, and there it stops. `model_view.ino` is unchanged. Zoom step 0 shows the legend, with the model on the left. Steps 1 and 2 show only the model, in the middle.
- Legend: one colour chip and one word per line, in DejaVu12, which is ASCII only, so words are chosen that need no umlaut. The data script measures each word in pixels against the column. A line without a chip is a note ("Gewebe entfernt" / "Tissue removed").
- Drawing: flat-shaded faces with a fixed light, faces turned away dropped, far to near by 256 depth buckets, cost linear in triangles whatever the turn. The filled faces of a cut lie on the cut plane, so they are always in front: they're drawn last, in file order, at one fixed brightness. Working arrays in internal RAM if there's room, otherwise PSRAM, sized once for the larger of the two views. All freed in `end()` and `unload()`.
- Budget: measured on the stick, 21-24 frames per second at up to 625 drawn triangles with `drawMs` 6-8 ms. Every cutaway is kept below 625 at all allowed turns; `tests/anatomy` fails if one isn't. The legend adds text drawing to each frame at zoom step 0. **Cutaways and legend are not yet measured on a stick.** Every 5 s the serial log prints `anatomy <id> view= triangles= drawnMax= zoom= fps= drawMs= drawMaxMs= work= sprite= freeHeap=`.

| file | triangles | most drawn in a frame (host, all allowed turns and zooms) | bytes | source |
| --- | --- | --- | --- | --- |
| heart | 1383 | 643 | 14,331 | BodyParts3D |
| heart inside | 966 | 538 | 11,832 | Human Reference Atlas |
| brain | 1463 | 733 | 15,191 | BodyParts3D |
| brain inside | 1152 | 609 | 13,145 | BodyParts3D |
| lungs | 1376 | 718 | 15,056 | Human Reference Atlas |
| lungs inside | 1242 | 587 | 13,384 | Human Reference Atlas |
| liver | 1189 | 650 | 12,175 | BodyParts3D |
| liver inside | 1263 | 608 | 13,420 | BodyParts3D |
| kidney | 900 | 542 | 9,132 | BodyParts3D |
| kidney inside | 1061 | 544 | 12,997 | Human Reference Atlas |
| skull | 1538 | 762 | 16,154 | BodyParts3D |
| skull inside | 1084 | 543 | 12,609 | BodyParts3D |
| eye | 998 | 508 | 10,172 | BodyParts3D |
| eye inside | 991 | 573 | 12,157 | Human Reference Atlas |
| stomach | 900 | 468 | 9,132 | BodyParts3D |
| skeleton | 1802 | 766 | 20,870 | BodyParts3D |
| hand | 1174 | 611 | 12,214 | BodyParts3D |
| animal-cell | 1102 | 535 | 12,934 | Schematic |
| plant-cell | 1325 | 557 | 14,628 | Schematic |
| red-blood-cell | 875 | 362 | 9,729 | Schematic |
| neuron | 1140 | 344 | 13,310 | Schematic |

What each cutaway shows, all of it from the source named:

- **Heart:** the textbook four-chamber cut, a plane through both valves and the tip of the left ventricle. Chambers with their real wall thickness, septum, valves, papillary muscles. The outside view has great vessels; the source of the cut doesn't.
- **Eye:** front to back through the middle. Sclera, choroid with ciliary body, retina, vitreous, lens, iris, cornea. The water in front of the lens is left out, so it's a gap.
- **Kidney:** along the long axis through the hilum, 6 mm behind the middle, where the pyramids show best. Cortex with columns, pyramids, papillae, hilum, renal pelvis.
- **Brain:** from the side, 3 mm beside the middle. Cortex, white matter, corpus callosum, ventricles, thalamus, caudate, putamen, pallidus, cerebellum, brain stem. Facets are hard, as in the outside view.
- **Skull:** down the middle. The inside of the real bones and the teeth.
- **Liver:** portal vein, hepatic veins, hepatic arteries, bile ducts and gall bladder, inside the far half of the liver surface. The screen says "Gewebe entfernt": the near half of the tissue is left out. It's not hollow.
- **Lungs:** the bronchial tree inside the far half of the lung surface, also marked "Gewebe entfernt". This is the weakest one: thin branches turn to spikes at this budget, and the small gaps between segments show as holes.
- **No cutaway:** stomach (one closed outer surface in BodyParts3D, no second source with an inside; cutting it would show either a solid lump or an empty bag, and neither is in the data), skeleton and hand (bones, with nothing modelled inside them).

Rules for the cuts, in `scripts/make_anatomy_data.py`:

- A cut is a flat plane through the real surfaces. A part gets a filled cut only if it's a closed solid and its cut closes into loops; everything else stays open, and `index.json` lists `filled_parts` and `left_open_parts` per file. Today left open: kidney hilum, column, pelvis and one pyramid; iris. The fill says "this part is solid here". Fine detail inside a solid (bone marrow, lung tissue) isn't in the data and isn't drawn.
- Colours are chosen to be sensible, not measured.
- Nothing is added that isn't in the source: no folds of the stomach, no extra vessels, no marrow.

Limits of the outside shapes, on purpose or from the data:

- **Heart:** BodyParts3D has no ventricle wall. The body of the heart is the cavities and the atrial walls, with the great vessels and three main coronary arteries.
- **Eye:** the clear cornea is left out so the iris can be seen; the lens is drawn dark so the opening reads as a pupil.
- **Brain:** the weakest shape. Each fold is reduced on its own, which leaves hard facets, and it reads best from the side, so it opens side-on.
- **Lungs:** some small gaps remain between the segments.
- **Skeleton:** over the 1500 target because it has 214 parts. Its triangles are small. It has no hyoid and is drawn in the source's standing pose.
- Kidney, hand and eye are the right-hand ones of the source body.

Build: `python -I scripts/make_anatomy_data.py --sources <folder> [names]` needs numpy, fast-simplification and mapbox-earcut. Names are ids, or `heart-cut` and so on for cutaways. Tests: `tests/anatomy/run.sh`. Check by eye: build `tests/anatomy/render_bin.cpp`, then `tests/anatomy/preview.py`, which draws the real files at 240x135 with the device's own pipeline. Every cutaway here was looked at that way, at its start and at its limits.

## User guide

Ask for a body part in 3D, or ask what's inside: "Zeig mir das Herz in 3D", "Was ist im Herz?", "Wie sieht das Auge innen aus?", "Wie ist eine Pflanzenzelle aufgebaut?"

Zitro knows the heart, brain, lungs, stomach, liver, kidney, skeleton, skull, eye and hand bones. It also knows the animal cell, plant cell, red blood cell and nerve cell. For anything else, or when a model can't be loaded, you see a picture as usual.

Turn the stick and the model turns with it. Button B zooms in. Button A asks something new.

Button C shows the other view: from the outside to the inside, or the other way round. Press it again for the text answer.

The inside view is cut open, with the name of each part beside it in its colour. It always stays turned towards you. Zoom in and the words make room for the model.

Heart, brain, kidney, eye, liver, lungs and skull have a real inside, from anatomy research, made simple enough for this small screen. With liver and lungs, part of the tissue is taken away so you can see the tubes inside: the screen says "Gewebe entfernt". The stomach, skeleton and hand have no inside view, because the research models don't have one.

The cells are always shown open. They are drawn from what school books describe, and they carry the sign "Vereinfachtes Schema", because they are simple drawings and not measurements.

## Release note

- Look inside! Ask "Was ist im Herz?" and Zitro cuts it open: real chambers and valves, brain, eye, kidney and more. Cells open up too, with every part named.

## Sources and licences

**BodyParts3D 4.0** (Database Center for Life Science): the outside of heart, brain, stomach, liver, kidney, skeleton, skull, eye, hand, and the cutaways of brain, liver and skull. Downloaded from https://dbarchive.biosciencedbc.jp/en/bodyparts3d/download.html (`partof_BP3D_4.0_obj_99.zip`, and `isa_BP3D_4.0_obj_99.zip` for the teeth and the deep parts of the brain).

- Required credit: "BodyParts3D, © The Database Center for Life Science licensed under CC Attribution 4.0 International".
- Changes to indicate: selected parts, reduced polygon count, recoloured, quantised; cutaways also cut open with the cut of closed parts filled.
- **The licence statements conflict.** The licence page and README (https://dbarchive.biosciencedbc.jp/en/bodyparts3d/lic.html, updated 2025-02-27) say CC BY 4.0. The header inside each OBJ file, dated 2013, says CC BY-SA 2.1 Japan. `index.json` states both and also offers the derived files under CC BY-SA 4.0, which satisfies both readings: clause 5.2 of the 2.1 Japan licence allows a later version with the same licence elements. Only the owner can decide whether to keep that or rely on the newer statement.

**Human Reference Atlas, 3D Reference Organs** (HuBMAP). Each record gives CC BY 4.0 with no fixed wording, and names its creators, so `index.json` credits each by title and creators. Changes to indicate: selected parts, reduced polygon count, recoloured, quantised, cut open with the cut of closed parts filled.

- **Lung, Male, v1.3** (Kristen Browne, Heidi Schlehlein): lungs, and the bronchial tree of their cutaway. https://lod.humanatlas.io/ref-organ/lung-male/v1.3/. Used because BodyParts3D has bronchial trees and arteries for the lungs but no outer surface.
- **Heart, Male, v1.3** (Kristen Browne): heart cutaway. https://lod.humanatlas.io/ref-organ/heart-male/v1.3/. Chambers with wall thickness, septum, valves, papillary muscles, which BodyParts3D doesn't have as walls.
- **Kidney, Male, Right, v1.3** and **Renal Pelvis, Male, Right, v1.0** (Kristen Browne, Heidi Schlehlein): kidney cutaway. https://lod.humanatlas.io/ref-organ/kidney-male-right/v1.3/, https://lod.humanatlas.io/ref-organ/renal-pelvis-male-right/v1.0/. BodyParts3D's kidney is one closed surface.
- **Eye, Male, Right, v1.3** (Kristen Browne, Heidi Schlehlein): eye cutaway. https://lod.humanatlas.io/ref-organ/eye-male-right/v1.3/.

The bottom line is too short for the full credits, so it says "BodyParts3D CC BY" or "HuBMAP HRA CC BY". It's the source of the view on screen. The skull says only "BodyParts3D": "Schädel" needs the wider font for its umlaut, and with "CC BY" as well the name itself would be cut. The full wording is in `webpage2/data/anatomy/index.json`, on the server beside the files.

**Schematics:** animal cell, plant cell, neuron and red blood cell are not data. `scripts/make_anatomy_data.py` generates them from fixed numbers, following what textbooks describe, and cuts them open. Sizes and positions are not measurements. The red blood cell has no parts inside, because a grown one has no nucleus: its cut shows the real dip of its shape, and the screen says "Kein Zellkern". The red blood cell uses the Evans and Fung shape formula (Microvasc. Res. 4, 335, 1972) with R 3.91, C0 0.81, C2 7.83, C4 -4.39. The original paper wasn't read. One paper seen (arXiv 1605.04509) prints C2 as 7.38; 7.83 was kept because it gives the reported cell height of 2.58 µm, where 7.38 gives 2.41.

**Looked at, not used:** Z-Anatomy: its GitHub repository carries a GPL-3 licence, and its models weren't examined. NIH 3D: licences differ per model, and none was downloaded. Smithsonian Open Access: not examined. Human Reference Atlas heart for the outside: tried, and it looked worse than BodyParts3D there (no great vessels). Its inside is used, see above. Human Reference Atlas liver: only surface segments, no vessels, so BodyParts3D was used. Human Reference Atlas has no stomach.
