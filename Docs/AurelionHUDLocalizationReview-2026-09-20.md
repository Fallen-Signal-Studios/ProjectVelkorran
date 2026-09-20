# HUD runtime localization review

The two explicitly approved native HUD fixes are already committed in
`606e4905`: magazine-free weapons hide ammo, and world labels avoid the
arranged subtitle/caption/objective geometry. This review adds runtime
localization evidence; it does not change that implementation.

## Verified mechanism

UE 5.7 registers the custom `LEET` culture under `ENABLE_LOC_TESTING` in
`Core/Private/Internationalization/ICUInternationalization.cpp`. The runtime
`InternationalizationLibrary.set_current_culture('LEET', False)` succeeds
in this development editor. No synthetic culture or localization resource
needs to be generated. The original `en` culture was restored afterward.

`FInternationalization::Leetify` substitutes characters and brackets text and
format arguments; it does **not** simulate a uniform 30–40% translation
expansion. Wrapping was therefore inspected separately with a synthetic
dialogue sample 45% longer than the previous English fixture. Those injected
dialogue/caption literals are layout inputs, not proof of gathered story text.

## Live evidence

Run: `Saved/Validation/Aurelion/HUDLocalization-20260920-110530-435f1474`.
Saved-only diagnostic: `Saved/Validation/review_hud_localization.py`.
The run ended normally, restored culture/settings, and ended PIE.

The existing actual firearm/radar/Blackout fixture passed first. Runtime
readbacks then confirmed requested scales 1, 1.5 and 2, identity `‡74RR1K‡`,
and formatted ammo `‡«32» / «218»‡`. The visible split numeric readout
remained 32 / 218. The gameplay viewport was **844 × 550**, DPI scale
0.50875; the larger full-editor screenshot size is not gameplay resolution.

The 1.5 and 2 scale images were inspected. The longer speech wrapped within
its panel, the simultaneous caption remained readable, and the pseudo-localized
objective waypoint stayed beneath the speech without overlapping it. The
identity, objective and speech-format wrappers visibly changed under LEET.
At 2 scale the left objective occupies substantial vertical space; this is
readability evidence, not approval of final screen density.

The 1 scale image captured an editor viewport without the HUD and is **rejected
as visual evidence**, despite valid widget readbacks. Screenshot creation and
process exit alone must not be treated as visual qualification. The accepted
images also contain a video-memory budget warning with two editor processes
open, so this run cannot qualify performance.

## Remaining scope

This confirms the engine mechanism and a controlled Tarrik HUD case. It does
not close localization for all story cues, real translated languages, Selene,
fullscreen resolutions, controller input, or the known Narrative gather-key
conflicts. It does not establish 90% TDD or final visual acceptance.

No source, widget, map, configuration or generated localization assets were
changed. The last unchanged-source full gate remains
`20260920-104320-f1997d15` (build, 720 tests, coverage and source integrity).
The creator's M12/grenade changes and GASPALS plugin remain untouched; M12
SHA256 is `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
