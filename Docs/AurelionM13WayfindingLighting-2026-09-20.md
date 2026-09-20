# M13 sign-housing light wash

The gallery and departure sign housings were almost black in the existing
ordinary gameplay views. Two short-range rect lights now reveal their ceramic
edges, bronze fittings and upper posts while retaining the dark lettering bed.
The chamber and all existing lights retain their settings.

Each new movable, shadow-casting light uses 12 lumens, a 480 cm attenuation
radius, a 340 × 8 cm source, warm neutral color, and specular scale 0.15. The
lights sit 70 cm in front of the header at Z=225 cm, yaw 90/pitch 25 degrees.
The reduced specular contribution avoids a broad reflection over the text.

## Visual decisions and persistence

- `WayfindingLightPreview-20260920-111201-ae236d26`: rejected unsaved 160-lumen
  preview; its reflection washed out the white lettering.
- `WayfindingLightSoft-20260920-111417-f73db8fc`: both approach images inspected;
  accepted the softer settings above.
- `WayfindingLightSaved-20260920-111717-dcec5764`: save failed with Windows
  sharing violation 32 while the retained interactive M13 editor was open.
  Exit zero was not accepted as success. The map hash matched its backup.
  The interactive editor showed **All Saved** and closed normally before retry.
- `WayfindingLightUnlocked-20260920-112007-043b490f`: saved M13 only, reloaded
  successfully and verified the two lights' transforms, intensity, range,
  dimensions, shadow setting and specular scale. Existing actor transforms and
  collision states were preserved. A prechange map backup is retained in the run.
- `WayfindingLightLive-20260920-112206-2e26e696`: restored the unchanged earned
  CP9 checkpoint through the public save API and found both lights at their
  expected runtime settings. All three ordinary gameplay frames were inspected.
  Gallery/departure housings have visible trim and clear text; the chamber is
  the unchanged control. These frames are **844 × 550**, not fullscreen evidence.

Authoring is idempotent by named light actor. The scoped save wrapper backs up
M13 and checks M12's hash. The existing wayfinding gameplay reviewer now verifies
the light inventory/settings in the restored world before capturing frames.

## Validation and limits

Prechange full gate: `20260920-110945-93e4f850`.
Final full gate: `20260920-112236-2f605927`.
Both passed build, 720 automation tests, coverage and source integrity without
SkipBuild. Python syntax and diff checks also passed. No tracked edits occurred
during either gate.

This is an architectural readability improvement, not character-behavior,
route-navigation, performance, AAA-quality or 90% TDD acceptance. Short light
ranges bound their influence but do not establish a measured GPU cost.
The camera-only gameplay review does not move the player or advance the journal.
No C++, mission definition, material, mesh or collision settings changed.

The creator's M12/grenade changes and GASPALS plugin remain untouched.
M12 SHA256: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
