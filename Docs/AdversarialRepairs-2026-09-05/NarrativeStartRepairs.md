# Dialogue start callback repairs

Source implemented; Unreal compilation and native automation execution remain required.

The final cross-review found that `UDialogue` continued its start pipeline after a designer callback exited the dialogue or restarted the same node. A retired start could create media, emit additional start notifications, query a replacement line's duration, or overwrite its completion timer. Owner-pointer checks alone did not identify same-node restarts.

`PlayNPCDialogueNode` and `PlayPlayerDialogueNode` now retain the exact Tales owner, current dialogue, node and presentation revision across node events, variable providers, designer media/start callbacks and duration calculation. Every continuation checks that receipt. Payload copies prevent nested starts from mutating an outer callback's line/speaker references. Both timer handles are retired at the next line start; weak completion delegates also validate the original receipt when dispatched.

Variable substitution previously rediscovered the first brace pair on every iteration: its first provider could run 50 times while subsequent distinct keys were never queried. The bounded scanner now advances through the template and queries each key once. It stops after a callback changes the current owner/node/revision, and only publishes its local result while the original formatting context survives. Candidate choice labels retain their existing current-node context.

Explicit suspension during a start callback delays media through one weak delegate owned by the existing dialogue. Resume consumes that delegate before callbacks, preserves the previous NPC speaker used for player listener resolution, and resumes the original timer. It does not replay node events, notifications or duration calculation. Newly created native audio inherits explicit suspension. Facial montages are excluded from the existing suspend-compatible dialogue contract because that contract does not own montage pause/resume; existing camera/body montage restrictions remain.

Five actual-owner native regressions were added to `Source/ProjectVelkorranTests/Private/Tests/SovDialogueStartRuntimeTests.cpp`: exit during player/NPC start; same-node reentry; replacement during duration calculation; distinct-variable/reentrant formatting; and suspension during player start followed by duplicate resume. Fixtures omit media assets and override designer callbacks while running the production Tales/dialogue/timer owners. These tests are authored, not executed in this workspace. Asset-backed audio, animation and sequence validation still requires UE 5.7.

The coordinated title-return follow-up adds `ASovPlayerController::NotifyTitleTravelFailed`: an accepted title travel that later fails publishes Failed while retaining the title pause, input lock and abandoned save ownership. The existing Save GI routes its abandoned-session engine travel failure to this action. No additional retry loop or save writer is introduced.

Validation performed: `git diff --check`; 57 host script regressions passed after Editor-module migration. Those host tests validate the build/report tooling, not these Unreal runtime paths.
