# Ground the gallery end-wall bands

The landmark review found two legacy lower-band meshes floating across the
entry and exit around head height. The north band obscured the doorway and
architecture behind it. Three separate gold-looking wound cubes and blue
overhead strips also remain visibly unfinished; they are not corrected by
this change.

`Z09WoundLandmarks-20260915-094300-dd5c52e7` recorded their saved component
inventory and fixed views, exited 0 and reported no Python errors. The six
wound/slit markers are untagged StaticMeshActors with one StaticMeshComponent
and no attachment. This is component evidence, not a proof about all possible
level references or live narrative interactions.

The two band actors now receive a custom split plinth: two five-metre basalt
assemblies separated by a six-metre doorway. The base has dressed moldings,
jointed stone, ivory returns and a narrow gold cap register. The south assembly
starts at (0,25375,-1500) cm and faces north; the north starts at
(0,30825,-1500) cm and faces south. Both use unit scale and rise 80 cm from the
floor. This intentionally redesigns the former floating-art placement.

Their actor and component identities remain. Their old decorative collision
is intentionally replaced with NoCollision and disabled navigation participation.
Native walls, floors and connectors remain unchanged. The preview compares all
other 3,138 actor transforms/collision states, and the existing floor verifier
checks 25 nearby native components. These checks do not constitute a live route
or collision-contact test.

Source: `Art/Source/Aurelion/Z09PlinthKit/` and `build_z09_plinth_kit.py`.
The clean FBX round trip passed. Independent exported-mesh rays test 25 clear
samples across the opening and six solid controls. Additional rays ensure the
gold and ivory detail faces are exposed. The final limited axis-aligned
coplanar-face audit reports zero overlaps.

Initial preview `Z09PlinthPreview-20260915-094821-99089b88` demonstrated the
improved doorway sightline but had buried gold/ivory detail. Their depth was
corrected; a resulting overlap at the lower molding was then removed by
shortening the ivory return. Neither intermediate source was saved into the map.
The final source retains the same overall footprint and opening.

This work follows the August TDD's white-stone/functional-gold architectural
language. Remaining wound imagery needs an authored Eclipse overlay, rather
than accepting the existing floating blocks as final art. Lighting, materials,
live narrative behavior, performance and the wider 90% goal remain unfinished.

Final preview `Z09PlinthRefined-20260915-095149-a31fa334` passed placement,
other-actor preservation and native-floor checks. Its north detail was inspected.
Save run `Z09PlinthSaved-20260915-095416-394bf715` backed up and saved the map.
Both exited 0 without Python errors. The final mesh contains 6,392 source
triangles, two UV layers, three material slots and no collision hulls; Nanite
uses position precision 10 and a full-geometry fallback. These are asset
properties, not a measured performance qualification.

Fresh reload `Z09PlinthFresh-20260915-095645-b6112add` passed all 93 architecture
reports and exited 0 without Python errors. Before/after north views, component
inventory and fit/reload evidence are in
`Docs/Validation/AurelionZ09Plinths-2026-09-15/`. Actor count remains 3,140;
the alignment estimate remains unchanged.
