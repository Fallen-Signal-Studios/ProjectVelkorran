# Crucible ceiling readability

The stopped-editor view at (0, 20300, -1000), pitch -5/yaw 90, showed the Z08
ceiling as a nearly black surface. Temporarily hiding its instanced actor proved
the obstruction was the authored enclosure. The actor was restored immediately.
The layout concept on page 23 calls for pale terminal architecture around local
corruption. A material-only preview had little effect without illumination.

The saved M12 change assigns the existing `M_Radiance_IvoryStone` material to
`Aurelion_Art_M12_Z08_89_b5e7cb` and adds two movable rect lights at
(±2000, 20800, -850), facing upward. Each uses 6000 lumens, 3000 cm attenuation,
600 × 300 cm source dimensions and linear RGB (1, 0.88, 0.7). Ceiling mesh,
instances, room dimensions, gameplay controls and collision were not edited.
The material has no connected world-position-offset input.

`preview_crucible_ceiling.py` reproduces the change without saving. The map was
saved through `LevelEditorSubsystem.save_current_level`; the older
`EditorLoadingAndSavingUtils.save_current_level` returned false.

Validation in `Saved/Validation/Aurelion/CrucibleCeiling-20260913` includes the
original map backup, before/after editor captures and a census comparison.
All 170 recorded actors retained their positions, mesh assignments and collision
settings. Physical bounds were unchanged. Four noncolliding visual bounds differed:
two HISM actors had initial placeholder bounds, the changed ceiling material
updated render bounds, and the dust effect's bounds expanded during simulation.
These differences are recorded rather than presented as identical scene bounds.

The after capture uses editor Game View to hide authoring overlays; it is not a
gameplay or performance test. Dynamic-light GPU cost, packaged appearance and
combat readability remain to be checked. No alignment-score increase is claimed.
This lighting pass does not deliver the concept's architectural detail or prove
safe survivor recesses. The Blender recess panel remains unplaced source art.

The initial attempt to launch the gameplay pilot inside the census editor was
rejected by its fresh-profile assertion before PIE started. That session exited
normally. The proper wrapper launched `CrucibleLightingFresh-20260913-101652-a9aa3ccf`
with an isolated profile, but it exited before map loading because the sandbox
left no writable derived-data cache nodes. The authorized launch with normal
cache access is `CrucibleLightingValidated-20260913-101809-9be0c273`. Neither the
profile assertion nor the cache startup failure counts as a gameplay result.
