# Z04 wayfinding and obsolete art cleanup

The relay room reuses the authored Aurelion route-register and destination-plaque meshes from the Z03 kit. Thirty-nine unit-scale light registers replace the thirty-four vendor gold-strip instances. The route passes east of the south approach cover, turns west after that cover, then continues west of the descending ramp toward the meeting atrium doorway. Small gaps at the two turns keep the perpendicular modules separate.

Seventeen layout annotations are hidden in game, including room dimensions, staging counts, checkpoint and sightline notes. Four native receiver, retry and encounter-hint text components retain their original text and visibility. The destination sign is resized and moved from its floating entry placement to a plaque beside the north doorway, reading `MEETING ATRIUM / RELAY OVERLOOK 04` on two lines.

Six obsolete decorations are hidden in game and explicitly assigned the NoCollision profile and mode: two canopy pylons, two upper spans and two broad gold floor channels. Their actors, transforms and meshes remain present for editor reference. The room's native walls, floor, ramps, rail guards, cover and gameplay components remain authoritative.

## Evidence

Fresh inventory `Z04PresentationBaseline-20260915-003135-8843c9ba` verified the saved guidance poses and recorded nearby annotation/decorative state. It exited 0 without Python errors and left the map clean. The existing custom module sources and source audits are in `Art/Source/Aurelion/Z03WayfindingKit/`; this pass does not duplicate those meshes.

The checker verifies all marker poses, thirty-nine native floor contacts, and unobstructed vertical point columns against the native cover and ramp components. These checks do not prove capsule clearance, navigation behavior or player understanding. It checks the retired decorations explicitly and updates the preceding wall checker to expect NoCollision only for these six named decorative actors; structural collision checks remain enforced.

Preview `Z04WayfindingPreview-20260915-003555-15837742` exited 0 without Python errors and all four views were inspected. It confirmed the label/decor cleanup and marker route, but the first plaque position was partly obscured by the ramp rail. The saved pass moves it to the west side of the north doorway, with its back at the wall relief face (world Y -9121.4 cm). This changes presentation only.

Saved run `Z04WayfindingSaved-20260915-004009-6569ce56` exited 0 without Python errors. All four saved views were inspected; the revised plaque is visible beside the doorway without the rail obstruction. The map was backed up before saving. Bright balcony exposure and lighting grain remain visible.

Fresh saved-map run `Z04WayfindingFresh-20260915-004301-9f4deab7` exited 0 without Python errors. All 84 architecture reports passed, with 3140 actors and a clean map. The retired decoration collision and annotation visibility persisted after reload. Final lighting, remaining piers/consoles/fixtures, live encounter guidance, supporting character likeness and campaign Chaos remain unfinished. The supported slice estimate remains 63.75%.
