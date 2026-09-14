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
