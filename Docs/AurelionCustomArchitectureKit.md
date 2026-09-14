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

Surface baking, production materials, collision, variants, in-engine review and deployment remain pending. The existing 63.75% slice estimate is unchanged.
