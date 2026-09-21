# M13 stationary dialogue performance

The actual restored M13 route showed Tarrik in a running/stepping pose while
Sequencer held his position during MeridianContainment. The three stationary
chamber conversations had character transform tracks but no skeletal performance.

MeridianContainment, FifthWitness and GrammarPropagation now give the Partner
binding (Tarrik) a ten-second breathing-idle loop for the full scene duration.
The clip is Narrative's `M_Neutral_Stand_Idle_Loop`, a non-additive animation on
the same skeleton as the character base mesh. Sequencer custom animation mode
owns this performance and the section requests RestoreState at completion.
The scene generator uses the same helper, so regeneration retains the change.

The edit adds skeletal tracks to these three sequences only. Existing transform
tracks remain, and neither map is saved. `M13PartnerIdleAuthor-20260920-171231-7fd7607f`
records duration, skeleton compatibility, restoration mode and both unchanged map
hashes. Dialogue, participant identities, camera cuts and native postconditions
are not edited. This does not claim facial animation, lip sync, authored gestures
or complete cinematic polish.

## Playback evidence

`M13PartnerIdleVerified-20260920-171534-fb7503a5/route-12.png` and `route-13.png`
show Tarrik standing naturally through MeridianContainment's opening and subsequent
dialogue, replacing the previous raised-leg gait pose. The matching observer rows
report his real mesh in `ANIMATION_CUSTOM_MODE` with `AnimSequencerInstance`;
Selene retains `ABP_Biped_C`. The FifthWitness wide capture (`route-15.png`) and
GrammarPropagation wide/close captures (`route-19.png`, `route-21.png`) also show
the standing performance, with matching live Sequencer-instance observations.
After these scenes, the same Tarrik companion is back in `ANIMATION_BLUEPRINT`
with `ABP_Biped_C` in the VoluntaryStay observations (`route-22.png`/`route-23.png`).

The FifthWitness close camera excludes Tarrik during his line, and Selene's face
is over-bright in the Grammar close shot. These are separate shot-coverage and
lighting polish items; the idle change does not claim to fix them.

`M13PartnerIdlePlayback-20260920-171334-3b135dd3` stopped because the observer
called an unavailable Python mesh accessor. It did not reach the edited scenes
and is not animation acceptance or a gameplay failure. The corrected observer
reads the reflected mesh property.

The independent pre-edit route `M13SynchronousFailure-20260920-170423-73f1a1bc`
passed all M13 receipts; it also confirmed the quarantine close-camera repair.
That pass does not repair the two earlier intermittent Selene startup failures.

The post-edit `M13PartnerIdleVerified-20260920-171534-fb7503a5` completed M13 in
297.969 seconds. All thirteen native receipts, both handoffs, the paired physical
lift, all complete scenes and CP7/8/9 passed, with the existing 22 M12 receipts
preserved and the asset-integrity check unchanged. Its parent also passed the
public victory reload, both M12 aftermath scenes, CP6 and travel. No gameplay
values or proof were injected. The earlier Selene startup failures remain open;
this successful replay is not evidence of their repair.

Full validation `20260920-172404-db056856` ran without SkipBuild: build succeeded,
all 722 matching tests passed, and coverage/source-integrity checks passed.
Edited Python files compile and the diff whitespace check passes. Native gameplay
code was not changed. This increment does not claim a packaged-build or broad
AAA presentation acceptance.
