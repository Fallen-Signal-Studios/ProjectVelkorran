# Z03 structural piers and rib cover

The generic 51-instance column batch is replaced by six full-size engaged piers and three complete rib-cover assemblies. Previously, each 2 x 4 x 3 m cover volume was filled by fifteen repeated columns. The new continuous stone core makes the visible obstruction agree with its solid native collision, with dressed panels, cut recesses, service registers and narrow gold conductors.

`Art/Source/Aurelion/build_z03_rib_kit.py` produces editable Blender source and two FBX modules in `Z03RibKit`. The six-metre pier derives from the established Aurelion pier vocabulary, with the height fitted and applied in Blender. Every engine placement uses unit scale. Three cover assemblies preserve the original grouped bounds; six piers preserve the original individual bounds. The original art actor remains, with one added HISM component to group the cover mesh. Native wall ribs and cover collision actors are unchanged.

Both meshes have two UV channels, Nanite precision 10, full fallback geometry and no added collision. The cover remains intact in this pass. Chaos fragmentation, checkpoint destruction state and live scanner concealment are not qualified.

## Evidence

- Clean Blender FBX round-trip passed for both modules.
- Scoped axis-aligned coplanar audit found zero overlaps for both source meshes.
- `check_z03_ribs.py` independently unions the original six single-pier bounds and three fifteen-piece cover bounds, compares the nine fitted envelopes, checks all poses and import settings, and runs the original native-room preservation checks.
- Twelve simple collision contacts test both horizontal axes at two heights on each native cover body. This verifies retained collision boundaries, not live AI vision or full player movement.
- Preview `Z03RibPreview-20260914-205550-fa41583c` completed without Python errors. The approach, middle and close views were inspected.
- Saved run `Z03RibsSaved-20260914-205849-1b899b26` backed up the map, saved and captured three reviewed views. Fresh run `Z03RibsFresh-20260914-210257-61fa778e` completed with 75 passing architecture reports, 3140 actors, twelve cover contacts, no Python errors and a clean map.

Z03 still needs its service-ramp and railing art, technical-signage cleanup, further surface treatment and final scene lighting review. Other generic art remains elsewhere in the map. Character likenesses, live gameplay, destruction and packaged performance remain broader goal items; the supported slice estimate stays 63.75%.
