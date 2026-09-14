# Aurelion complete architectural rebuild

The active goal is to replace the grayboxed Aurelion level with a custom Blender-authored architectural kit, at the user's requested highly detailed AAA quality bar. This covers both M12 and M13, every playable room, transitions, vistas and cinematic backgrounds. Existing first-pass cladding and shuttle/vault assets are candidates for improvement, not final-quality acceptance. Supporting-cast work and the broader 90% TDD alignment objective remain active.

## Art direction and kit coverage

The August TDD's King-era inheritance and layout reference pages 21–24 govern the art: ivory stone, functional gold, deep architectural layering, responsive geometry and light conveying information. The dimensioned layout governs playable space. Corruption modifies this architecture without replacing it with an unrelated biome.

The kit must cover straight and corner wall bays; piers and buttresses; door/open passage surrounds; arches and vaults; cornices and capitals; floors, borders and threshold transitions; stairs and landings; balconies, railings and parapets; refuge interiors; terminal/briefing focal structures; exterior/dock structures; localized damaged and Eclipse-overlaid variants. Repetition, scale and seams must be reviewed as assemblies, not only as isolated models.

## Acceptance requirements

- Review silhouette, layered construction, edge highlights, surface detail and purposeful ornament against the reference in neutral lighting and production Unreal lighting.
- Author editable Blender sources, stable module dimensions, bottom/grid pivots, clean normals, material slots, UVs with consistent texel scale, appropriate collision and LOD/Nanite configuration. Baked/tiled surface maps must survive import; Blender procedural preview shaders alone do not qualify.
- Replace the visible graybox throughout both maps. Retained invisible gameplay proxies must have a documented purpose and verified correspondence with the final visible architecture. Preserve required route dimensions, cover, wall-run surfaces, interaction reach, camera composition and encounter readability while making deliberate geometry improvements where needed.
- Inspect every room at player height, in combat and in its cinematic views. Fix seams, floating pieces, intersection, repetition, material scale, exposure and missing rear/upper surfaces.
- Verify both campaign priority routes, companion/enemy movement, saves and representative packaged GPU/memory performance after the rebuild.
- Do not claim AAA acceptance or 90% alignment from polygon counts, asset import, a studio render or a partial room pass.

## First detailed module family

`Art/Source/Aurelion/build_architecture_kit.py` creates an editable assembly and separate FBX modules under `ArchitectureKit`: a four-metre/seven-metre wall bay, a seven-metre layered pier, a four-metre stepped cornice and a four-metre inlaid floor. The bay adds fitted masonry, deep oblique reveals, incised plaques, service louvers and a concentric mechanism register. This is a source prototype for reviewing the architectural language; it is not the completed kit or map replacement.

The existing 63.75% slice estimate is unchanged. Final surface authoring, module variants, campaign deployment and runtime qualification remain pending.

## Unreal integration

The first four modules are imported under `/Game/Aurelion/Environment/ArchitectureKit/Meshes`. UV0 now uses one texture tile per metre on dominant face planes; UV1 retains unique packed islands for lightmapping. The Blender FBX round-trip checks pass with two UV channels. Unreal import checks centimetre dimensions, three material slots, source-file identity and Nanite configuration. Solid bay, pier and floor assets have one generated box collision each; the ornamental cornice has none. These initial collision envelopes still need gameplay-context review before deployment.

Owned material copies under `ArchitectureKit/Materials` provide ivory stone, gold and dark reveals without changing existing campaign materials. The stone uses retained base-color/normal/roughness textures, restrained base-color variation, a 0.2 normal blend and roughness of 0.34 plus 0.2 times its roughness texture. This is a material candidate, not final surface acceptance.

`/Game/Aurelion/ArtReview/L_Aurelion_ArchitectureKit` contains a separate two-bay assembly and review camera. The imported detailed face is local +Y; review placement uses yaw 180 to face the camera. Initial backside captures were rejected and the orientation corrected. No M12/M13 actors or mission progression were changed by this import.

Import evidence: `Saved/Validation/Aurelion/ArchitectureKitImport-20260913-181657-2f21bf4d/kit-import.json`. The initial multi-view capture suffered overlapping screenshot tasks and does not establish distinct player/detail review. The capture helper now waits for task completion, pilots the review camera and verifies both imported UV channels. The runner accepts the dedicated review map for subsequent art iterations; retained gameplay-route runs still require M12.

The completed review is `Saved/Validation/Aurelion/ArchitectureKitThreeViews-20260913-182520-73516938`, with separate assembly, player-height and detail PNGs and `capture-complete.json`. A callback reentrancy guard was also required because screenshot preparation pumps editor ticks. All three images were inspected. Geometry and material assignments survive import, but prominent shadow noise remains in the offscreen captures. These images establish assembly/detail review, not final lighting or surface-quality acceptance.

Next art requirements include plain and mechanism-bearing bay variants, deeper joints and focal construction, controlled surface wear, continuous ring joints, corner/portal assemblies, and fitting the family into the actual rooms. Repeating the same concentric register on every bay is not the intended final architectural language. Production-lighting review, collision fit, Nanite fallback/platform behavior and measured GPU cost remain unqualified.

## Plain bay and portal expansion

The kit now has six modules. `SM_Aurelion_KIT_WallPlain_4x7` retains the construction and channels without the concentric register, allowing mechanism-bearing bays to be used selectively. Closed ring mouldings now share their wraparound topology instead of using overlapping end caps; this addresses the seam observed in the previous close-up.

`SM_Aurelion_KIT_Portal_6m` provides an eight-metre-wide, 7.8-metre-high frame with a nominal six-metre lower opening, layered stone, recessed dark channels, gold rails and stepped feet/collars. Six authored convex hulls follow its frame segments rather than filling its aperture. The Blender FBX round-trip validates all six modules and checks nine clear rays through the portal plus blocked rays at both sides. These checks are source-art evidence, not gameplay traversal.

The Unreal refresh in `Saved/Validation/Aurelion/ArchitectureKitExpansion-20260913-183123-7e564b07` imported both new modules and refreshed the owned existing meshes, preserving the review map. The importer verifies existing import-source ownership before replacement. The frame imported at approximately 800 x 130 x 779.85 cm with all six convex hulls. Its collision has no generated solid box across the opening. The separate review scene now pairs plain/mechanism bays and includes the portal. Neither campaign map has been refitted yet.

`Saved/Validation/Aurelion/ArchitecturePortalReview-20260913-183309-0bfe3cf6/capture-complete.json` records nine clear Unreal capsule sweeps through the aperture and two blocked side-frame controls, using a 42 cm radius and 88 cm half-height Pawn profile. Four rendered views were produced. The portal and revised ring detail were inspected: the frame silhouette reads, but its upper span still needs richer construction detail and the review lighting is too dark for final surface acceptance. The module remains a structural art prototype, not a final AAA doorway. These editor queries do not prove navigation, live player movement, companion movement or mission-route acceptance.

## Z01 lower-wall deployment

The first campaign deployment replaces the visible lower side walls of M12 Z01 with 28 plain bays and 26 piers at unit scale. This is a partial room pass, not completion of Z01 or the full rebuild. Roof, floors, end walls/doors, ramps, cover, transitions and production lighting still require the custom architecture pass. The existing 63.75% evidence-based slice estimate remains unchanged.

The current-room census is `Saved/Validation/Aurelion/Z01ArchitectureFit-20260913-183815-a2af11a7/z01-current.json`. The existing visible enclosure is a single mesh covering the lower walls and upper shell. Two unsaved cladding trials were rejected because the new panels intersected that enclosure. Eighty-four complex traces established the actual side-wall surface positions; hidden blockout bounds alone were insufficient.

`derive_z01_upper_enclosure.py` creates an editable Blender/FBX derivative of the existing enclosure above world Z=695 cm, retaining its four material assignments. This upper shell is temporary retained architecture, not newly authored final kit content. The original enclosure is hidden visually while its collision remains enabled. The new upper shell and 54 lower-wall modules have collision disabled, preserving the existing physical enclosure and hidden gameplay proxies. Visual/collision correspondence still requires live wall-contact and companion traversal review.

The imported upper derivative has 2,732 triangles. An FBX import warning was traced specifically to malformed normals on the original UCX collision object, which is excluded from the derivative export; it did not identify a visual-mesh normal or UV failure. Sources are under `Art/Source/Aurelion/Z01Upper`, and the owned Unreal asset is under `ArchitectureKit/Meshes`.

Both views in `Saved/Validation/Aurelion/Z01ReplacementPreview-20260913-185418-69f88598` were inspected before saving. `save_z01_lower_architecture.py` uses the same fit checks and backs up the map before saving. Save evidence is `Saved/Validation/Aurelion/Z01ArchitectureSaved-20260913-185934-fc1d115f/z01-fit.json`; all 1,733 original actor transforms and collision settings were checked unchanged before the save. The review camera is created after saving and is not stored in the map. These are authoring checks, not mission progression or runtime acceptance.

Fresh-load verification passed in `Saved/Validation/Aurelion/Z01SavedReloadCheck-20260913-190132-db47e278/z01-reload-verification.json`: 1,788 actors (1,733 original plus 55 architectural actors), all 54 expected lower-wall placements at unit scale and correct orientation, retained enclosure/proxy collision, hidden original enclosure, and upper-shell bottom at 695 cm. The read-only check left the map clean. Live traversal, combat visibility and performance remain pending.
