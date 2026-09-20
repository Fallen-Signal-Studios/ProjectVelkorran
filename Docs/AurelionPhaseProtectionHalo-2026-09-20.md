# Independent phase-protection presentation

The existing lethal-floor lattice uses the character's single mesh-overlay slot.
The previous actual route recorded missing overlays while protection and the cue's
recovery timer remained active. This increment provides an independent visible cue;
it does not identify or change the caller that clears that overlay slot.

`GC_AurelionLethalFloor` now owns a noncolliding, movable sphere surface with
`M_AurelionPhaseHalo`. Only narrow segmented latitude rings and partial meridians
are visible, in static violet emissive strokes. The rest of the sphere is transparent.
Depth testing remains enabled; cover occludes the effect. It casts no shadow, generates
no overlaps and receives no decals. The cue attaches to its owner, retaining the
existing immediate removal/recycling behavior. No gameplay rules, damage, poise,
meshes, animations, map assets or C++ were changed.

Authoring script: `Scripts/Editor/author_aurelion_phase_halo.py`.
Saved authoring run: `PhaseHaloAuthor-20260920-143355-82a22981`.
The original cue is backed up there. An earlier attempt stopped at an unavailable
Python setter before either asset was saved.

`probe_phase_halo_lifecycle.py` initializes the real Elite and Weaver definitions in
the isolated Chaos review scene without saving it. It deliberately changes floor flags
and overlay slots. This is presentation testing, not campaign or damage acceptance.

Final controlled run: `PhaseHaloVisual-20260920-144300-b87e721e` under
`Saved/Validation/Aurelion`. Seven staged transitions and 1,161 active samples passed:

- Both live cues have the halo, correct material, owner attachment, zero relative
  translation and no collision.
- Continuously clearing character overlay slots does not remove either halo.
- A foreign overlay survives cue activity and removal.
- Both initial removal and reactivation/removal leave no active cue.

The active/cleared and removed images were inspected. Violet cages remain visible when
the character overlay is cleared; the cover crate occludes the left cage; removal leaves
neither cage visible. Initial screenshot attempts used a command unavailable without a
player controller, an incorrectly oriented test camera, or stale render-target content.
Those attempts are not visual acceptance. The final fixture explicitly captures, waits
for rendering, exports the target and rejects identical active/removed images.

Baseline `20260920-142155-d82ee9c9` and final full gate
`20260920-144459-b7bd2366` passed the editor build, 721 matching automation tests,
coverage and source integrity. The final render run has no Python errors, material
warnings or ensures. No packaged build or GPU profiling was performed.

Actual mission-phase acceptance remains open. `start_phase_halo_route.py` combines
the ordinary input route with passive protection and companion observers. The protection
observer now records halo visibility, attachment, cue-hidden state and game time alongside
the pre-existing floor, overlay and damage evidence. Its output must be inspected after
the phases and cleanup; starting a replay is not a pass. This does not close handoff Task 4,
the remaining character work, or the broader visual-fidelity goal.

Protected M12 SHA256 remains
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The user's grenade assets and local GASPALS plugin remain untouched.
