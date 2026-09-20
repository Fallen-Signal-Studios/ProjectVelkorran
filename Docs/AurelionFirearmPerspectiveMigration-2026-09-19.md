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
