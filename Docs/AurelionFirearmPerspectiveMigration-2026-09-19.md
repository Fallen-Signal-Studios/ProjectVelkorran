# Firearm animation perspective migration findings

The fresh runtime log `TargetingChordRuntime-20260919-213245-4a0cdd5e/Editor.log`
reports an invalid CameraStyle field in ABP_Biped_Overlay_FireArmBase.
`FirearmAnimationInspection-20260919-213749-114e7818` exported that Blueprint,
and `FirearmAnimationBindings-20260919-214105-0576d09b` exported ABP_Biped plus
actual Cinderline/Staccato item defaults. No packages were saved.

## Active fault

The authored node at
`Overlay.AnimGraphNode_StateMachine_1.SM_FireArm_3P.AnimStateTransitionNode_4.Transition.K2Node_PropertyAccess_2`
has GUID CD025CAB46E612A0F41385A81DFCD166 and path
`GetMainAnimBPThreadSafe.NarritiveCharacterGASPThreadSafe.CameraStyle`.
It has an explicit compiler warning and its output is wired into an enum
comparison. The same node in ExecuteUbergraph is compiler-generated; changing
that generated copy would not repair the authored transition.

A neighbouring authored transition reads `NarrativeGaspCharacter.CameraStyle`.
ABP_Biped's current export does not expose a CameraStyle variable, so replacing
the first path with GetMainAnimBPThreadSafe.CameraStyle is not supported by this
evidence. Both legacy character dependencies must be audited. Do not clear the
warning by disconnecting or forcing the transition.

The PropertyAccess node can be found through reflection, but Python's editor
property API rejects reading its protected Path. A graph edit must use the
Blueprint editor or an already-supported authoring operation; no C++ extension
was added. The existing native remapper handles hard references, not these path
segments or pin wiring.

## Required repair and proof

Use project-owned firearm overlay copies and migrate perspective-dependent
transitions to the resolved camera presentation shared by protagonists and
companions. Trace each weapon visual's DefaultWeaponAnimLayer/Weapon1PAnimLayer
and any form-specific/dual-wield overrides before remapping consumers. The item
CDO confirms Cinderline uses the NWV_Cinderline visual; item Blueprint inheritance
alone does not reveal its active animation overlay. Preserve weapon grip poses,
aim, fire, recoil, reload and state-machine transition direction.

Acceptance requires clean compilation of the owned overlay hierarchy and fresh
runtime observations of both protagonists and companions with firearms:
third-person hip/aim/reload, first-person entry/exit, and aim-override fallback.
A cleared compiler warning alone is insufficient. No animation repair or visual
improvement is claimed by this diagnostic increment.

## Implemented owned hierarchy

The two authored 3P transitions now use the local Boolean `ResolvedFirstPerson`.
Hip-to-ADS reads it directly; ADS-to-hip keeps the existing four-input AND,
including NOT firing, NOT aiming, NOT camera-inside-head and NOT first-person.
The 0.4-second entry and 1.5-second cubic return blends are preserved.

`ABP_SovFirearmBase`, `ABP_SovRifle` and `ABP_SovDualFirearm` live under
`/Game/Characters/Animation/Firearms`. Project-owned Cinderline and Staccato
visuals reference those overlays, and their project weapon items reference the
owned visuals. Vendor templates retain their reflected fingerprints.

The base maps `Camera.Perspective.FirstPerson` to the new Boolean. Reloading
retains its existing gameplay tag but uses the copied Blueprint variable GUID:
duplicating a Blueprint regenerates variable GUIDs, so retaining the vendor GUID
silently clears the reload property during compilation. The authoring script
now resolves both GUIDs from the owned base and verifies both bindings afterward.

Authoring receipt: `FirearmOverlayDocked-20260919-215802-81f3a089/firearm-repair.json`.
All seven affected Blueprints compile with zero errors and zero warnings.
Only those seven packages were saved; no mission map or vendor asset was saved.
The M12 creator worktree hash remains B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5.

Runtime qualification is separate from compilation. The initial controlled
probe called GetCharacterRef on a class default animation object and triggered
a native checked-cast failure. The probe now filters for PIE skeletal-mesh-owned
instances before querying their character. This was an inspection error, not
an observed ordinary-play failure. Full visual pose acceptance remains open.

Fresh-load inspection also found that reparenting preserves child CDO tag-map
overrides. The rifle retained the vendor reload GUID and the dual-firearm map
was empty. Both owned child maps therefore receive the two audited bindings
explicitly, followed by compilation and equality checks against the owned base.

The cold-editor source fingerprint guard initially withheld this save because
compilation garbage-collected ten already-unreferenced vendor graph objects.
The diff contained removals only, with no changed live objects or dirty state.
The authoring script now compiles existing owned dependencies and collects unreachable objects before taking its
baseline; the strict before/after comparison remains in place.


The final cold-editor authoring run
`FirearmChildBindings-20260919-222356-47cfac0b` passed the unchanged-vendor
fingerprint guard and saved all seven owned packages. Both child tag maps match
the corrected base exactly; all seven compiles have zero errors and warnings.

Fresh PIE `FirearmPerspectiveRuntime-20260919-222511-e507e0c8` passed five
controlled camera changes on Tarrik with the starting Cinderline explicitly
wielded through Narrative's normal wield API. Both live linked rifle instances
reported false/true/false/true/false for Far/FirstPerson/Balanced/FirstPerson/Close.
Their live maps resolved both actual Reloading and ResolvedFirstPerson properties.
All three saved overlay defaults retained the corrected map after a cold load.
This proves the active linked-instance binding on Tarrik, not physical-input
routing, Selene/companion coverage, reload playback, or visual pose quality.

Full gate `20260919-222729-f5af0b47` passed the build invocation (without SkipBuild),
all 719 automation tests, report coverage and before/after source integrity.
The fresh runtime still logs the original vendor base's CameraStyle warning when
that template is loaded elsewhere. The repaired owned hierarchy is clean and
Tarrik's sampled instances use it; global vendor-consumer migration is not claimed.

Next qualification: Selene/Staccato and firearm-equipped companions; ordinary
hip/aim/fire/reload poses; aim-priority entry and fallback. The broader HUD/M12/M13
visual and gameplay goal remains active, including the separately reproduced
M13 lift co-rider collision blocker.
