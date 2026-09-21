# Quarantine framing and restored route review

The actual E4B victory save now has a verified public-load path through both M12
aftermath scenes, physical CP6 and native travel to M13. The input driver accepts
an explicit restored-victory admission only after checking the load result, mission,
boundary, source victory roster, exact journal and live encounter attempt/victory.
Its existing same-process admission remains the default. The review does not edit
save-bank contents, actor positions, resources, inventory, abilities or story proof.

## Camera repair

The quarantine wide shot previously placed a wall across most of the screen.
Both cameras now stay inside the ramp, north of the quarantine gates. Only the
two camera transform tracks in `LS_SurvivorsClearAndQuarantine` were saved.
Focal lengths remain 26/32 mm; character tracks, dialogue, cuts and gameplay marks
were not edited. The scene generator uses the same shared camera profile.

`QuarantineCameraFraming-20260920-165522-ac75c927` records 36 clear static
Visibility rays (two cameras, three drift positions, two protagonists, three body
heights), unchanged participant transform values, and the protected M12 map hash
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
These rays do not prove visibility of non-colliding geometry or final animation.

`QuarantinePlaybackReview-20260920-165630-402de854/route-00.png` and `route-01.png`
show the saved wide shot during ordinary full scene playback: both protagonists
are unobstructed and subtitles are readable. The two captures precede the close
cut; rendered close-shot acceptance remains pending. Lighting, speaker coverage,
character blocking and animation still need a broader cinematic polish pass.

## M13 failure retained

`RestoredM13RouteChecked-20260920-164510-176b8ccf` passed M12 aftermath/travel and
then earned ContraryPosition, TarrikIndependentAssent and the native Selene handoff.
SeleneIndependentAssent failed during startup with the native message:
“Required cinematic participant became unavailable.” This is a failed M13 run.

The message originates in the Narrative sequence playback failure delegate.
Its readiness gate checks required bindings, character initialization, appearance
loading, ASC ownership and stop tags. The first run does not establish which
condition failed. No participant gate was bypassed and no C++ repair is claimed.
The restored-companion ability-grant issue is separate unless further evidence
connects it. The earlier fresh-route pass does not override this restored-route
failure. Read-only participant observations were added to the review for diagnosis.

`QuarantinePlaybackReview-20260920-165630-402de854` independently passed the M12
aftermath/travel again, then reproduced the same M13 failure at 35.203 seconds.
The observer sampled only the cameras before preparation and did not capture the
transient required-character bindings at failure; it does not identify the offending
participant. The sequence failure can occur between polling frames. Further diagnosis
should observe the synchronous failure boundary rather than treating these incomplete
samples as proof of a missing character.

`RestoredM13Route-20260920-164307-bcab30c8` stopped on an unavailable Python
controller accessor; it is neither gameplay failure evidence nor an acceptance pass.

## Validation

Full gate `20260920-170122-7883045f` ran without SkipBuild: native build, all 722
matching automation tests, coverage and source-integrity checks passed. The edited
Python files compile and `git diff --check` passes. These checks do not turn either
failed restored M13 gameplay run into a mission pass. No C++ changed in this camera
and route-observation increment.
