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

## Uninterrupted gameplay result

`CrucibleLightingValidated-20260913-101809-9be0c273` reached E4B in actual order:

| Stage | Result | Seconds |
|---|---|---:|
| Fresh M12 entry | Passed | 49.625 |
| E1 combat, route hold, Selene handoff | Passed | 168.531 |
| E2 combat and receivers | Passed | 53.532 |
| E3 meeting and entry | Passed | 93.625 |
| E3 rescue | Passed | 76.235 |
| E4 entry, WestStretchers priority, seven protected people alive | Passed | 98.031 |
| E4A precision, two Axiom severs, native Tarrik handoff | Passed | 25.734 |
| E4B | Failed before Thermal Fracture | 35.954 |

The Axiom main-hand selection was confirmed with an actual UI click and checked
immediately. The chain started E4B directly after E4A; there was no operator idle
gap. This resolves the prior wheel-confirmation obstacle, not the E4B gameplay gap.

The first E4B sample placed the Elite at (-749.5, 19263.2, -1109.7), with 53.2 HP.
At 34.641 seconds it was inside the west recess at (-2864.7, 22209.0, -1109.7).
Tharne at (-2750, 22150, -1110) had fallen from 100 to 20 HP. The native damage
observer then recorded the Elite's fatal hit against that exact Tharne actor:
transaction `99EBC7A64A4B98E2634D84A48AA9A783`, 30 resolved damage, 20 applied
health damage and `bFatal=True`. The editor log confirms Tharne's native death.
The observer completed with eight damage events and zero observation errors.

Selene reached within 61.01 cm of the frost anchor, but the Elite remained outside
the 1500 cm setup range. No frost window or Thermal receipt was earned. The
driver waited at the frost setup instead of drawing the Elite back into the court.
The stale initialization error in the thermal snapshot is not independently a
proved binding defect; the out-of-range pilot did not issue a frost request.

This fresh result rules out the earlier idle delay as the sole cause. Next work
must address protected-recess access and readable player recovery/Elite positioning,
then test the complete partner sequence again. Do not substitute invulnerability,
range expansion, forced targets or synthetic receipts. Both-priority validation,
Thermal/Core victory and earned M13 travel remain pending. PIE was stopped after
the failed report and damage evidence were preserved. Alignment remains about 64%.
