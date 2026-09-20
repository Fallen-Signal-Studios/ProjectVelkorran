# Companion mesh alignment

Selene and Tarrik's companion Blueprints now use the mesh frame authored on
their respective playable character: location Z -90 cm, yaw -90 degrees, unit
scale. Both companions previously inherited zero translation and rotation from
their native parent. Their appearance definitions already matched the players;
the missing Blueprint defaults placed and oriented those appearances incorrectly.

`align_companion_mesh_frames.py` copies only the mesh translation, rotation and
scale, backs up changed packages, compiles and checks the resulting defaults.
The full-route companion authoring recipe calls it after creating the companion
Blueprint, preventing regeneration from reintroducing the missing offsets.
The playable-character assets are read-only inputs to this repair.

The earlier Selene 100 cm attack-range experiment is retired. The native light
attack range is restored to 180 cm and the authoring override removed. Damage,
montages, attack windows and sweep geometry are unchanged by this repair.

## Evidence

`CompanionMeshDefaults-20260919-192613-0e0da601` contains the initial defaults
census, original package backups and saved repair receipts. In its
`ContactProbe-192922-804011` earned-checkpoint observation, Selene's runtime mesh
retained the corrected offsets and her blade swept forward during the attack.
No player or companion damage receipt occurred in that bounded checkpoint
probe; the encounter subsequently failed. This is presentation-frame evidence,
not proof of successful companion combat or checkpoint recovery.

The live collision census in that run confirmed the nearby Eclipse bodies had
physics assets and query meshes blocking the native weapon trace channel.
It does not establish intersection with an individual physics body or damage
admission. A normal fresh-route observation is required before claiming the
reported companion attack issue resolved.

Baseline full validation: `20260919-192204-16ecc078`, build without SkipBuild,
719 matching tests, report coverage and source integrity passed.

## Fresh campaign combat

`CompanionMeshFullRoute-20260919-194246-89efc6df` read back both saved companion
frames in a fresh editor and passed entry, E1, E2, meeting, rescue, E4 entry,
E4A, E4B and native M13 travel. The normal input chain earned final quarantine
victory with all protected people alive. Both protagonists' companion frames
matched the player defaults throughout the sampled runtime handoffs.

Selene produced ten native source-attributed damage receipts: eight applied
health damage, totaling 111.936358, and two applied zero. The first damaging
Verity hit dealt 29.721739 health damage and killed a Linkbound. Her
`AM_VerityTwin_01` montage and Tarrik's sword attack montage both played.
Tarrik produced no source damage receipt in this run; his damage contribution
is still unqualified. These results support the mesh-frame repair and Selene's
live melee contact, without claiming every companion behavior is resolved.

One native M12 WallRunner route completed with twelve sampled traversal frames
and no observer errors; the other remained unavailable. This establishes a
live traversal occurrence, not cinematic-quality foot contact or every route.
M13 passed its independent assents, co-action, scenes through GrammarPropagation,
CP7 and the paired lift. The input driver then failed at VoluntaryStay because
it did not sample the 0.35-second native hold countdown. Its recorded request
callback was accepted and the cinematic entered Loading. Subsequent live UI
review showed the scene had completed and the objective advanced to receiving
Tarrik's Cauldron recorder. This is a countdown-observation gap, not evidence of
a rejected mission interaction. The failed report is preserved: the remaining
M13 beats and the full route are **not** qualified by this run.

The earlier `CompanionMeshRoute-20260919-193955-138f31e3` was accidentally launched
with entry-only flags. Entry passed, then its retired input driver left the
player stationary in E1. That process was stopped and replaced with the fresh
full-route run above; it is excluded from combat qualification.

PIE ended explicitly after the M13 observation. Both companion observers,
Eclipse motion, traversal and damage observers finalized without errors. The
creator's M12 map retained SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
All eight changed/new Python files parsed successfully, and whitespace checks
passed. Post-change full validation `20260919-200148-2405d6cf` passed the build
invocation without SkipBuild (target up to date), all 719 matching automation
tests, report coverage and source integrity. No tracked edits occurred during
that gate. Packaged execution and the unresolved M13 countdown observation
remain outside this acceptance.
