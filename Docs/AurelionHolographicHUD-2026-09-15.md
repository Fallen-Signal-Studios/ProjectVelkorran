# Holographic HUD, 15 September 2026

Creator direction, with reference images for both protagonists: push the entire HUD holographic,
as minimal and beautiful as possible, and recreate it with the engine's own tools rather than
dropping the reference art in as textures.

This supersedes the Halo-style corner-mark direction in
[HUDHolographicDirection-2026-09-13.md](HUDHolographicDirection-2026-09-13.md). The later
behavioural work in [AurelionHUDRefinement-2026-09-13.md](AurelionHUDRefinement-2026-09-13.md)
— 24% threat-card fill, objective deferral under `SequencerControlled` — still stands.

## What the references specify

One layout in two palettes: Tarrik in amber, gold and red; Selene in cyan, teal and white. A named
plate at top centre with a thin shield bar over a heavier health bar, a six-segment pip row beneath
it, an angular ammo readout at top right, a radar at bottom left, a long segmented arc across the
bottom, and torn plasma edging framing an otherwise empty screen.

## What each element is bound to

Nothing is invented for looks. Every element reads a real system:

| Element | Source |
|---|---|
| Identity, shield, health | `USovCombatVitalsWidget::ReadCurrentVitals` |
| Six pips | `SovCombatReadiness::Read` ability entries and their states |
| Ammo | The wielded weapon's clip and reserve |
| Bottom arc | Echo, through `GetEcho` / `GetMaxEcho` |
| Radar | `USovProximityDetectionComponent` contacts |

Two mappings were creator decisions rather than deductions: the arc shows **Echo**, and the radar is
driven by **real proximity detection** rather than the encounter roster.

## The radar is gameplay, not decoration

The project had no source of *detection* for it. The only nearby-hostile system is an environmental
sensor feeding NPC relays, which explicitly never performs a nearby-actor search, and the existing
threat widget is a warning surface with no positional data. Narrative does ship a Navigator —
compass, minimap and screen-space markers, all present in `WBP_AurelionGameplayHUD` — but that is
marker *registration*: actors opt in through `NavigationMarkerComponent::RegisterMarker`, and it
performs no line-of-sight test, keeps no sighting memory and infers no hostility. None of the rules
below could be obtained from it. An earlier draft of this document said simply that no source
existed, which overstated a check that had only covered detection and never looked at the shipped
display stack. So detection was built, with its rules in `SovProximityDetectionPolicy` under
portable tests:

- A hostile never seen is never shown. There is no seeing through walls, and nothing appears merely
  for being close.
- Losing sight fades a contact over four seconds into a **memory that keeps its last sighted
  position**, rather than tracking a target through geometry.
- A glimpse under a fifth of a second does not register, so the display does not flicker at doorways.
- Leaving the radius behaves exactly like losing sight.

The surface draws that distinction: a live sighting is a filled mark, a memory is an open ring. The
component is read-only — it damages nothing, alerts nobody, and never tells an NPC where the player
is. Hostility is the owner's own team judgement, so allies and the Elite's summoned adds never
appear on their own side's display.

## Where the reference and accessibility conflict

The references are austere, and a faithful recreation could quietly discard behaviour the previous
HUD carried for accessibility. These are kept deliberately, and the conflict is recorded rather than
silently resolved:

- **Text labels stay.** Each resource keeps a short label, so identity and meaning survive without
  colour. The references show none.
- **UI scale is honoured** across the whole surface.
- **High contrast thickens rather than removes.** It swaps the translucent optical veil for an opaque
  backing and drops the torn glow and the radar sweep, which are decoration that costs legibility. No
  readout is removed to achieve contrast, and a test asserts the accent stays visible.

## The capture trap, and what it cost

The surface rendered correctly from the first build. Eight consecutive captures showed a blank
screen anyway, because `UAutomationBlueprintFunctionLibrary::TakeHighResScreenshot` routes through
`FHighResScreenshotConfig`, which exposes resolution, mask and HDR but **no UI flag**, and the
automation path calls `FScreenshotRequest::RequestScreenshot(false)` with `bShowUI` hardcoded off.
No screenshot taken that way can contain UMG.

Each blank frame was read as evidence about the widget, and five explanations were pursued and
disproved in turn: zero geometry, a paint that ran once, a transparent palette, an empty culling
rect, and occlusion by z-order. A `USafeZone` root was added to fix a zero-size problem that never
existed, and has been reverted. The widget's own diagnostics contradicted the picture the entire
time — `paints=61 drew=1 size=2126x1081 valid=1 inViewport=1 accentA=1.00` — and the instrument was
trusted over the measurements.

What settled it was a probe submitting one rectangle by three routes: the engine's own `MakeBox`
call as used by the sibling threat overlay, this file's box helper, and its line helper. None
appeared, which no drawing bug can explain. The same frame was then captured twice, seconds apart:

- `Shot showui -nosuffix filename=<path>` — the HUD, and all three probe marks, clearly present.
- `take_high_res_screenshot` — no UI whatsoever.

The corroborating signal had been in every frame and was missed: `WBP_PlayerInfo_HUD`,
`WBP_WeaponInfo`, `WBP_CrosshairContainer`, the compass and the minimap were all absent too. The
holographic surface was never uniquely invisible.

Two details in the capture path are load-bearing. The hyphen in `-nosuffix` is required, because
`FParse::Param` only matches a token preceded by `-` or `/`; a bare word is ignored and the engine
generates a numbered filename instead. And the two shutters must occupy different frames, because
`FScreenshotRequest` is a global holding one pending filename and one `bShowUI`, so issuing both
together lets the second discard the first.

## Limits of this pass

- **The capture includes the editor.** A UI-inclusive screenshot in PIE photographs the whole
  window, chrome included: enough to prove the surface renders and to judge layout, not a
  presentable frame. A clean image needs a standalone session.
- **Appearance is not yet matched to the references.** Rendering is proven; resemblance is not.
  The torn edging currently reads as loud scribble rather than plasma, and the identity plate is
  wider than the references want.
- **The torn edging is deterministic jittered polylines.** Flatter than the reference glow, which
  wants a noise material. That remains the better answer.
- **Ammo draws twice until an editor step runs.** The legacy `WBP_WeaponInfo` child of
  `WBP_AurelionGameplayHUD` is still live; collapsing it is a Content change through the existing
  authoring path.
- **The surface ignores the cinematic hide contract.** `UNarrativeGameplayHUD::SetHUDHidden`, with
  its `EssentialWidgets` exemption, *is* implemented in `WBP_AurelionGameplayHUD` — an earlier read
  of the C++ alone wrongly concluded it was not. What is true is narrower: nothing in C++ invokes
  it, and this surface sits outside the widget tree it hides, so it would stay on screen through
  cinematics where the HUD it replaces correctly vanishes. The project already has the signal to
  drive this (`CurrentSequences`, used by the haptics component). Unfixed, and needs a test.
- **The surface bypasses the registered UI layers.** The HUD registers `UI.Layer.Game`,
  `UI.Layer.Menu` and `UI.Layer.Modal`, and the project already pushes menus through `OpenMenu`.
  This widget calls `AddToPlayerScreen` directly. That is not why it appeared blank — the sibling
  threat overlay renders fine the same way — but the Game layer is the architecturally correct
  host for a persistent readout.
