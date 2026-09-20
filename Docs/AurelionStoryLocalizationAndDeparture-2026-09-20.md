# Saved story text and corrected departure directions

## Story localization: existing content verified

`StoryLocalizationAudit-20260920-113621-96ce86ec` read both saved maps without
saving. All eight M12 scenes (30 cues) and ten M13 scenes (38 cues) already use
the owned Aurelion string table for both speaker and dialogue. Switching the
runtime culture to LEET changed all 136 text values; the original culture and
both map hashes were preserved. The existing handoff's statement that these
cues had no keys was stale, so no scenes were regenerated.

The saved cue references resolve to 78 unique `Scene.*`/`Speaker.*` keys.
Every reference's English source matches the existing Game gather manifest:
136 references checked, zero mismatches. Detailed asset readback and
`gather-crosscheck.json` are retained in the run. The read-only diagnostic is
`Saved/Validation/inspect_story_localization.py`.

This establishes saved text identities, gather coverage and runtime culture
conversion. It does not prove live sequence timing, translated-language layout,
voice playback or full localization acceptance. LEET changes characters, not
uniform text length; the separate HUD expansion review remains limited.

## Departure sign: player-facing correction

The saved sign said `< DOMINION   REFORMATION >` although the approaches lead
left to Reformation and right to Dominion. This was visible in the earlier
CP9 gameplay captures and confirmed by `DepartureDirectionAudit-20260920-113849-abd1e92d`:
both Reformation's dock and shuttle are at X=3800, and both Dominion's are at
X=-3800. Looking along the approach at yaw 90, view-right points toward -X.

The sign now says:

```text
DEPARTURE CONCOURSE
< REFORMATION   DOMINION >
```

The label uses `Sign.DepartureConcourse` in `ST_AurelionText`. No dock, shuttle,
character, camera, collision or route geometry was moved to fit the sign.

`DepartureSignPreview-20260920-114140-4f4c817b` was visually inspected; the
corrected line fits the existing housing. `DepartureSignSaved-20260920-114359-38284bb4`
backed up and saved only M13 and the text table, reloaded the map, and verified
the new text/table key. It preserved other text, existing table entries, actor
transforms/collision and the protected M12 hash.

`DepartureSignLive-20260920-114615-7e4c2475` restored the unchanged earned CP9
through the public save API. The fresh gameplay world contains the corrected
text and table key; dock projection checks place Reformation left and Dominion
right. Its ordinary **844 × 550** gameplay frame was inspected and shows the
sign matching the protagonists' separate departure sides. This is a camera-only
review, not a new physical route or controller-input test.

The targeted table gather in `DepartureSignGather-20260920-1150` exited zero
and produced 175 keys, including all 78 story/speaker keys and the exact new sign
source. Output remains under Saved; no generated `Content/Localization` files
were committed. The first command attempt in `DepartureSignGather-20260920-1148`
failed before gathering because its config argument split before `.ini`; the
retry used a quoted argument array. This targeted gather does not clear the
known unrelated Narrative source-key conflicts or compile translations.

## Validation and preservation

Prechange full gate: `20260920-113759-8fa6b051`.
Final full gate: `20260920-114628-0b4889fd`.
Both passed build, 720 automation tests, coverage and source integrity without
SkipBuild. Python syntax and diff checks passed; tracked files were frozen
during each gate. The later handoff/report edits are documentation only.

M12 SHA256 remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The creator's grenade edits and GASPALS plugin remain untouched. This closes a
specific misleading sign and verifies existing story localization; it does not
complete the character-behavior, visual-fidelity or overall Aurelion goal.
