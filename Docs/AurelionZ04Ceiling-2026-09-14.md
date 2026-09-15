# Z04 roof and ceiling illumination

The relay overlook's 144 flat vendor ceiling panels are replaced by 144 fitted custom stone coffers across the complete 62 x 38 m footprint. Each bay measures 3.875 x 38/9 metres, with jointed beams, octagonal dressed courses, recessed gold conductors and corner bearings. Continuous backing and upper edge closures complete the roof perimeter. The lowest visual surface stays at 7 m; the roof top is 7.55 m.

The light inventory showed that the existing four ceiling-bounce actors point downward. Their settings remain unchanged. Four new upward washes on those actors use 500 lumens each, with two weaker 250-lumen centre washes on the existing central key actor. All six sit at Z=350 cm, point upward, use 800 x 800 cm sources and 3500 cm attenuation, cast shadows and contribute no volumetric scattering. They use pale blue-white color bytes 226/233/255. No new actors are added.

The four-light preview revealed a dark central band. Two centre washes were added for a second review instead of raising the bright side areas.

## Evidence and limits

- `Z04LightAudit-20260914-224057-3716223a` records original light transforms, intensity, color and relevant settings. The checker preserves those measured values.
- The clean FBX round trip passes geometry, dimensions, material slots and two UV channels. The scoped coplanar audit reports zero overlaps.
- The checker independently verifies the full 16 x 9 grid, unit scale, 7 m clearance, roof height, Nanite precision 10, full fallback geometry, no added mesh collision and the six wash settings. The preceding wall checker preserves native relay gameplay geometry.
- Git fetch during this pass found origin/main still at `ac3cc446`; no new upstream change required adapting this art pass.
- Revised preview `Z04CeilingBalanced-20260914-224905-455cfc2c` passed without Python errors. Entry, middle, balcony, close coffer and exterior frontage views were inspected. The centre washes improved continuity compared with `Z04CeilingPreview-20260914-224440-d265baf4`; visible grain and some local brightness variation remain.
- Saved run `Z04CeilingSaved-20260914-225316-e9a7751b` completed without Python errors; all five saved views were inspected and matched the reviewed treatment.
- Fresh editor run `Z04CeilingFresh-20260914-225819-a9f60e93` completed with exit 0 and no Python errors. All 80 architecture reports passed, including the persisted ceiling and light settings; the map remained clean with 3140 actors.

Live combat readability, final lighting balance, fixture art and packaged performance remain unqualified. Z04 floors, balcony, rails, piers, props and annotations remain unfinished. Full-map art, character likeness and Chaos destruction are still open; the supported slice estimate remains 63.75%.
