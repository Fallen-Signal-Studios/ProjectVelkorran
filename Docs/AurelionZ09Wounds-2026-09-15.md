# Authored wound-gallery fractures

Three floating decorative cubes are replaced by horizontal Eclipse fractures
mounted on the north wall panels and lintel. The change uses the August TDD's
existing-place corruption language: shallow dark branching fissures, chipped
ivory stone and interrupted restrained violet seams. The central marker moves
above the doorway rather than continuing to float inside its opening.

The source generator derives three distinct variants from the established
Eclipse scar construction, bakes the horizontal proportions into vertex
geometry and exports unit-scale meshes. Each is approximately 1.7 m wide and
2.6 cm deep, with two UV layers and three material slots. Their clean Blender
FBX round trip passed. Source, FBXs, original marker baseline and placement
manifest are retained in `Art/Source/Aurelion/Z09WoundKit/`.

The old actor/component identities remain. New centers are X=-620/0/620,
Y=30825.5 and Z=-1180/-980/-1180 cm, yaw 180 degrees. These intentionally replace
the original floating-marker poses. The original cube collision is removed;
the overlays use NoCollision and do not affect navigation. All other 3,137
actor transforms/collision states are compared before and after placement.
The floor verifier separately checks 25 native collision neighbors unchanged.

The established PavingIvory, EclipseIntrusion and EclipseSeam materials are
reused without modifying their shared assets. Nanite uses position precision
10 and full fallback geometry. The overlay geometry is not a fracture simulation
or campaign Chaos implementation, and it does not create a hole through the
native wall collision.

`Z09WoundPreview-20260915-100250-6fd95cb3` passed the identity, placement,
material and collision checks, exited 0 and reported no Python errors. North,
close-west and lintel images were inspected. The fractures now read against
the architecture and the old blocks no longer obscure the opening. Fine branch
tips still show limited pixel coverage. Final lighting, motion, narrative
response, live traversal and performance remain unqualified. The blue overhead
slit markers are still unfinished and need their own pass.

This is a first authored replacement, not final AAA acceptance or a 90% TDD
alignment claim. The alignment estimate remains unchanged.

`Z09WoundSaved-20260915-100622-dc376129` backed up and saved the map after the
same placement checks. Fresh reload `Z09WoundFresh-20260915-100907-4545cb78`
passed all 94 architecture reports. Both exited 0 without Python errors.
Saved context/detail images and fit/reload evidence are retained in
`Docs/Validation/AurelionZ09Wounds-2026-09-15/`. Actor count remains 3,140.
