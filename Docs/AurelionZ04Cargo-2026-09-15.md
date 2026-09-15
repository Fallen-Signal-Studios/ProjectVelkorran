# Z04 fitted cover stores

Five custom Blender modules provide nine complete cover assemblies. Seven replace thirteen vendor crate instances; two finish the previously exposed sides below the balcony cover caps. Each assembly matches one native cover owner, avoiding independent visual crates sharing a single obstruction. The original low/high cover heights and footprints remain authoritative.

The kit extends the authored Aurelion cargo language: ivory ceramic panels, recessed dark chassis, corner collars, separate handles, mechanical latches, ribbed service fields, small connector contacts and narrow gold routing registers. The meshes are authored at their actual dimensions and placed at unit scale.

| Module | Dimensions (metres) | Placements |
| --- | --- | --- |
| Low stores | 3 x 3 x 1.2 | South approach, southeast, north middle |
| Wide stores | 3 x 2 x 1.2 | Northwest |
| High stores | 3 x 3 x 2.2 | South flank, central, west |
| Balcony stores | 3 x 2 x 1.125 | Below retained 7.5 cm balcony cap |
| Foot stores | 3 x 3 x 2.125 | Below retained 7.5 cm balcony-foot cap |

## Verification

Fresh baseline `Z04CargoBaseline-20260915-001724-811cb41f` checked all thirteen vendor poses and nine native cover transforms, with two horizontal component traces per cover. It exited 0 without Python errors and left the map clean.

All five clean-FBX geometry/UV checks passed after correcting a degenerate face in the first short-variant export by limiting bevel size on thin details. Each final module has 24,388 triangles, two UV channels and no authored collision. Individual scoped coplanar audits report zero same-facing axis-aligned convex overlaps; these are not exhaustive intersection checks. The Blender studio render was inspected.

Engine preview `Z04CargoPreview-20260915-002056-fc07203c` and saved-map run `Z04CargoSaved-20260915-002403-ce9ccb77` exited 0 without Python errors. All four views from each run were inspected: approach, stores detail, balcony and balcony-foot cover. The two previously exposed cap undersides now have complete supporting bodies. Oversized legacy direction text, old gold strips and uneven room exposure remain visible. The map was backed up before saving.

Fresh saved-map run `Z04CargoFresh-20260915-002648-22dca251` exited 0 without Python errors. All 83 architecture reports passed, with 3140 actors and a clean map. The engine checker compares the new assembly envelopes with the original grouped vendor envelopes, verifies each native obstruction and its two contact probes, and runs the preceding rail/floor/wall checks.

## Destruction scope

The native cover actors still own collision. These are intact presentation assemblies, not destructible campaign actors. Future Chaos owners can use the documented one-to-one cover mapping, but the two cap-backed covers also need their separate cap presentation included in the broken state. Stable save identity, actual combat damage, collision retirement, navigation, debris and effects remain required before destruction rollout.

Remaining Z04 work includes piers, consoles, fixtures, guidance, obsolete art cleanup and final lighting. Live cover use, companion movement, packaged performance and full-map visual acceptance remain open. This pass does not change the supported 63.75% slice estimate.
