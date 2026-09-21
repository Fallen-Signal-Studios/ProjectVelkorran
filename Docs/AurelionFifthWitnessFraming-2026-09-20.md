# Fifth Witness speaker coverage

The previous close cut put Selene in the foreground and left Tarrik off-screen
during his line about the prison. The sequence used a generic target despite
this scene's different partner mark.

Both FifthWitness cameras now look across the two protagonist marks from the
east. The close cut retains Tarrik, Selene and the Fifth Witness's enclosure.
Only camera transform tracks in `LS_FifthWitness` changed. Focal lengths remain
26/32 mm; the new Tarrik idle, participant transforms, camera-cut timing, dialogue
and native story postconditions remain intact. The generator uses the same profile.

`FifthWitnessFraming-20260920-172633-800e8ccd` records 36 clear static Visibility
rays across camera drift and protagonist body heights. M13's map hash remains
`CCAF63F21731315B8A19644D455F439222D3AE4F2577A826FB0D1C807CCE52CB`.
The protected M12 map was not saved.

`FifthWitnessPlayback-20260920-172746-2b2eb2a6/route-15.png` and `route-17.png`
were inspected during ordinary full scene playback. Both protagonists are in
frame; the close cut includes Tarrik during his line, and the Witness enclosure
is visible between them. Subtitles remain readable. This verifies speaker
coverage, not final lighting, lip sync, gestures or overall AAA presentation.

Selene's face appears green in this run, compared with the pale/over-bright face
in earlier captures. No material or lighting change was made in this increment.
The current appearance/material inputs need inspection before attributing the
variation to lighting or changing an intended character skin tone.

The read-only `SeleneFaceMaterialAudit-20260920-173524-31895f5d` confirms the
current appearance still assigns MetaHuman's baked skin instances at LOD0–7.
The inspected LOD0/1 instances have no local vector-parameter overrides and use
the baked virtual textures; the unrelated `M_SeleneFace` material has no emissive
input and is not assigned by this appearance. This does not establish the live
dynamic-material state or virtual-texture residency. Inspect those before changing
skin color or lighting. No appearance or material assets were saved by the audit.

## Validation

The post-edit restored M13 route passed in 285.125 seconds, including all thirteen
receipts, both handoffs, the paired lift, complete scenes and CP7/8/9. Its asset
integrity check passed. The parent passed public E4B victory reload, M12 aftermath,
CP6 and travel. The intermittent Selene scene-startup failures from earlier runs
remain open; this pass does not constitute a repair of them.

Full gate `20260920-173645-fc5a4a24` ran without SkipBuild: build, all 722 matching
automation tests, coverage and source-integrity checks passed. Python compilation
and whitespace checks passed. Both map hashes are unchanged. No native code,
mission postconditions or packaged-build acceptance changed in this increment.
