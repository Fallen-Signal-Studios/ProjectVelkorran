# M13 departure lighting

Added two broad neutral RectLights to the departure concourse in `L_Aurelion_M13`.
They improve character illumination while retaining the existing amber and blue lighting.
This is a lighting increment, not a clean character visual acceptance or overall TDD sign-off.

## Authored change

`Scripts/Editor/aurelion_departure_fill.py` defines both movable, shadow-casting lights:
North `(0,48600,400)`, yaw -90; South `(0,46900,400)`, yaw 90; both pitch -15.
Each uses 1600 lumens, a 3000 cm radius, 1400 by 200 cm source, warm-neutral
`(1,.95,.88)` color and .3 specular scale. Existing lights are unchanged.
`save_m13_departure_fill.py` saved only M13 and verified the values after reload.
Other actors' transforms and collision were preserved; protected M12 was unchanged.

## Evidence and limitations

Evidence folders below are under `Saved/Validation/Aurelion`:

- `DepartureFaceFill-20260920-185322-4a9895f7`: accepted temporary PIE comparison,
  portrait/player before and after, four unobstructed character-to-light visibility rays.
  Selene's skin looked natural in this preview. Both temporary lights were removed.
- `DepartureFillSave-20260920-185758-6ebe234f`: saved and reloaded exact preview
  settings; includes the pre-change map backup and preservation report.
- `DepartureFillSavedPlayback-20260920-185910-b5855260`: earned public CP9 reload
  confirmed both saved lights without spawning replacements or changing their values.
  **The final `saved-portrait.png` shows strongly purple Selene skin. Visual acceptance
  failed despite successful persistence checks.** The player image retains readable
  armor edges but Tarrik remains dark. Do not describe either as AAA visual completion.

The previously intermittent green skin issue now also has a purple reproduction.
This pass does not establish its cause or fix it. Different tint across launches is
not evidence that the neutral fills caused it; equally, a successful earlier preview
does not prove the saved experience is visually correct. Follow up on character
rendering using this saved reproduction. No GPU cost benchmark or packaged build was run.

Earlier attempts were rejected: `DepartureFillPreview-20260920-183241-8f60a162`
was stopped and screenshot settling corrected; `DepartureFillCompare-20260920-183339-da2b72de`
failed a PIE world-path guard; `DepartureFillChecked-20260920-183616-b98db962`
used a Python-unavailable spawn API. `DepartureFillLive-20260920-184147-fea6977a`
had rejected static-light transforms (corrected by setting mobility first and asserting
placement). `DepartureFillPositionVerified-20260920-184611-d0a0ecdc` placed lights
correctly but did not illuminate characters sufficiently. None was saved or counted as a pass.

## Validation

Full native gate `Saved/Validation/20260920-190144-49587240`: build exit 0,
722 matching automation tests passed, 95 warnings; source unchanged throughout.
Ran without `-SkipBuild`. Earlier gate: `20260920-182711-1bfdf3f4`.
Automation success does not override the failed portrait visual check.

Saved M13 SHA256: `634FD9172C6B2A54CB2903BA3B1993914D2F6D0C6E218D87D0D61F563BB6E71E`.
Protected M12 SHA256: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
