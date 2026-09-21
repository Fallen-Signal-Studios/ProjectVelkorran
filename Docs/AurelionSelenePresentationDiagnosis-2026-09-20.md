# Selene presentation diagnosis — 20 September 2026

## Findings

The restored gameplay run `SeleneLiveFaceAudit-20260920-174328-8536de71`
passed M12 aftermath and travel, then **failed** at SeleneIndependentAssent.
Process exit 0 is not gameplay success. The synchronous failure observer captured
the real bound `BP_SovSelene_C_0`: alive, player-ready, correct PD_Selene and
AC_Selene, with a valid MetaHuman visual, but `IsCharacterPendingLoad()` was true.
The scene was still in Loading. This establishes a pending participant at the
instant playback was rejected; it does not identify which internal load handle
was outstanding. Private ASC/visual flags are explicitly unavailable in the report.

`USovCampaignCinematicComponent::StartPreparedPlayback` calls `Player->Play()`
directly. `ANarrativeLevelSequenceActor::OnPlay` rejects pending participants.
The actor's existing `PlaySequence()` instead waits for readiness with a bounded
timeout. Proposed native repair: use that readiness path, retain generation,
context, timeout and participant validation, and add campaign-start regression
coverage for pending visual completion, timeout and cancellation. Approval was
requested under the engineering handoff; no native change was made in this pass.
Earlier successful route runs do not resolve this intermittent failure.

## Skin audit

The read-only `SeleneTextureSettings-20260920-174553-36f1a732` audit completed.
The exported 8192-square baked basecolor has normal pink/tan skin coloring rather
than green. Live snapshots use the intended MetaHuman face skin instances;
LOD0 has no local vector override and inherits `M_skin_unified_baked`.
The unrelated `M_SeleneFace` material is not the assigned face skin.

All four inspected baked textures use virtual texture streaming. Basecolor is
sRGB/default compression; SRMF is linear/masks; normal is linear/normalmap;
scatter is sRGB/default. Basecolor uses the World texture group, unlike the
Character groups on the other maps. This difference is not a demonstrated cause.
The intermittent green face remains unresolved. No skin, lighting, texture-pool,
map or material edits were made to conceal it.

The first export attempt (`SeleneFaceTextureAudit-20260920-174114-b6d4903f`)
exported the basecolor but failed on an incorrect normal texture name. It is not
a completed audit. Correcting the name to `T_Head_N_VT` allowed the later audit
to finish. The texture editor audit overlapped part of the live gameplay run;
timing/performance observations should retain that qualification.

## Verification and scope

Full native validation without SkipBuild:
`Saved/Validation/20260920-175121-932f601f`: build, 722 matching automation tests,
coverage and source integrity passed. This does not supersede the gameplay failure.
Changes in this pass are read-only audit scripts and this record.
Protected M12 and current M13 map hashes remained unchanged:

- M12: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`
- M13: `CCAF63F21731315B8A19644D455F439222D3AE4F2577A826FB0D1C807CCE52CB`
