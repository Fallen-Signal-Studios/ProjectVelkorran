# Eclipse presentation workflow

Requires UE 5.7 with the compiled `ProjectVelkorranEditor` module and the locally licensed
`/Game/Parasites_Pack` dependency. The source pack is not copied into Git. Run authoring only
outside PIE, with the current user work preserved.

For a baseline that does not yet contain the new role assets:

1. `prepare_eclipse_presentation.py` creates the three owned AnimBP/blend-space pairs.
2. `assign_eclipse_presentations.py` assigns WallRunner, Weaver and Elite meshes and transforms.
3. `author_eclipse_native_melee.py` creates the four native melee ability/data/montage/config sets.
4. `refine_eclipse_readability.py` removes Linkbound's duplicate visible body and adjusts eight
   existing M12 encounter lights. It backs up the exact current map before saving.
5. `author_eclipse_weakpoint_bones.py` replaces the three roles' obsolete mannequin
   head/torso bone matchers with verified Parasites physics bodies. Set
   `SOV_AURELION_RUN_DIRECTORY` to a new evidence directory; backups and before/after
   values go in its `weakpoint-authoring` folder. Native zone IDs and consequences stay intact.

Creation scripts refuse to overwrite existing outputs. Inspect their JSON reports under
`Saved/Validation/Aurelion/EclipseAnimation`; a zero editor exit code is not an authoring or
gameplay pass. Existing assets should be edited deliberately rather than deleting them to rerun
the creation scripts. `correct_eclipse_core_body.py` records the one-time correction from the
initial Spine2 selection to the actual Fat torso physics body, Spine1; the assignment script
already contains that corrected selection.

The copied graphs retain the existing Narrative animation parent, locomotion state machine and
montage slot. Meshes and sequences use matching pack skeletons. Damage, melee sweeps, ability
selection, wall-route movement, links, Core consequences and mission receipts remain native.
Attack windows and damage values are provisional authored tuning until real combat is reviewed.

For a fresh route with read-only motion/montage sampling, use the existing validation launcher
with `start_eclipse_route_observed.py`, `-Visible -KeepEntryOpen -ContinueE1 -ContinueRoute`.
No authored assets are saved by that entry point. The observer is bounded to one hour and ends
when its PIE world disappears; it never activates abilities, moves actors or applies damage.
Its montage observations do not prove accepted damage. All normal route qualifications remain
in the existing independent route reports.
The wall-route observer records native traversal results and completion flags
separately from ordinary running poses; it never begins or advances a traversal.

The `inspect_*.py`, `probe_aurelion_visual_alignment.py` and `review_eclipse_camera.py` tools
provide read-only audits or an ejected review-camera move. They do not qualify gameplay.
See `Docs/AurelionEclipseAlignment-2026-09-13.md` for actual results and preserved failures.

The subsequent Cinderline audit uses `inspect_cinderline_damage.py` and the
read-only `observe_aurelion_combat.py`. Launch `start_aurelion_combat_observed.py`
with the same fresh-route flags to include native incoming/outgoing damage and
exertion observations. The observer unregisters its delegates when E1 ends.
That entry point also installs `observe_eclipse_damage.py`, which records actual
outgoing native damage from the four Eclipse roles and unregisters by E4B completion
before normal mission travel. The separate `audit_eclipse_weakpoints.py` and
`inspect_eclipse_bone_positions.py` read existing actors without changing poses or state.

For a baseline without the Thermal correction, run `author_cinderline_thermal.py`
outside PIE with `SOV_AURELION_RUN_DIRECTORY` set to the validation output folder.
It creates the owned effect/primary ability, backs up the weapon, and changes only
its primary grants. It refuses existing outputs. Shared Narrative damage assets
are preserved. The normal kit setup retains this owned primary when present.
