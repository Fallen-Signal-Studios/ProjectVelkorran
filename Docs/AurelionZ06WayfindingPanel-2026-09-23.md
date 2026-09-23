# M12 Z06 Breach Rescue wayfinding cassette

The original floating white `BREACH RESCUE / AHEAD: CAPTURE GALLERY` text at
`(0, 7000, -360)` has been replaced in the saved M12 map with a physical
Aurelion wall cassette. The retained native `TextRenderActor_113` now reads
`BREACH RESCUE / CAPTURE GALLERY`; the panel's fitted gold chevron carries the
forward direction. The mesh is on the east wall at `(1645, 7600, -425)`,
between structural piers. The text is centered at `(1631, 7600, -366)` with
32 cm glyphs. This clears the 4–6 m combat/traversal lane.

The design reference was generated **before** modeling and is in
`Art/References/Aurelion/Z06Wayfinding`. Blender source and export are in
`Art/Source/Aurelion/Z06WayfindingPanel`, built by
`Art/Source/Aurelion/build_z06_wayfinding_panel.py`. The cassette has a recessed
black-stone field, ivory octagonal arris, hairline gold conductors, cut index,
rear service cassettes, mechanical standoffs and non-emissive route chevron.
The joined mesh has 20,748 triangles, two authored UV channels and no simple
or convex collision. Its actual exterior is approximately
4.304 × 0.265 × 1.205 m, including the frame and rear fittings. Unreal uses
the established Aurelion PavingIvory, Gold and Reveal materials with Nanite;
the level actor has no collision or navigation effect.

The first unsaved Unreal preview at Y=7000 overlapped the pier at Y=7200 and
its 18 cm text was weak from the lane center. A second unsaved preview moved
the panel to Y=7600 and raised the text to 26 cm. The saved version shortens
the second line and uses 32 cm text. At 1600×900, the saved player-height
center-lane, oblique approach and detail views show the whole panel clear of
the pier and the wording on its dark face. The oblique and detail views read
well. From the far lane center, the text is visible but remains secondary to
the objective HUD; normal combat-camera readability still warrants a manual
playtest.

Saved review: `Saved/Validation/Aurelion/Z06WayfindingSaved-20260923-155633-c73d0d76`.
The authoring script compared every unrelated original actor's transform and
collision state, backed up the M12 map, then saved only the owned panel and
sign placement. A separate clean Unreal launch passed
`Scripts/Editor/check_z06_wayfinding_panel.py` in
`Saved/Validation/Aurelion/Z06WayfindingFreshLoad-20260923-160002-ab8bc1a1`:
the panel bounds lie within the Y=7200–8400 cm pier bay and outside the center
lane, the original text actor is retained, both UV channels load, and the mesh
and actor have no collision. M12's new SHA-256 is
`c7ff8e4369547be02ec36f1d4d882592ce95a8758f57f74fa02d87cfe12f298d`.
This is an editor visual and geometry acceptance, not a combat or packaged
build pass.
