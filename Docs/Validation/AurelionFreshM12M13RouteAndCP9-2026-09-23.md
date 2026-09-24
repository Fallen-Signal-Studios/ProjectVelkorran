# Fresh M12→M13 route and earned CP9 recovery — 23 September 2026

The visible UE 5.7 PIE run `FreshM12M13E1SightRetest-20260923-113514-9d416c78` began at M12 CP0 and passed the entry plus all eight ordinary Enhanced Input continuation drivers. It cleared E1 without a death, then E2, E3 entry/rescue, E4 entry/E4A/E4B, native travel into M13, and M13 separate departures. The M13 report records all 22 prior M12 receipts and all 13 M13 receipts, including the paired physical lift and CP7/8/9. Its final native departure has 35 journal entries and separate Tarrik/Selene exits. The route report says `passed`; the M13 report says `passed`, `assets_unchanged=true`, and `retained_gameplay_references_cleared=true`.

The passive WallRunner observer recorded no errors. E3's runner completed its authored route; all 14 sampled traversal frames carried `/Game/Aurelion/Enemies/Animation/AM_EclipseWallRun`. E4's runner carried the montage in all eight sampled traversal frames, but did not complete that route before the encounter moved on. These are animation samples, not a full visual-quality sign-off. The four `wall-live-*.png` captures were reviewed in the real player viewport; they show the amber HUD and an oversized white world-space `BREACH RESCUE`/`AHEAD: CAPTURE GALLERY` sign still visible during combat. A subsequent read-only editor inventory identified it as `TextRenderActor_113` (`Aurelion_Art_Sign_Z06_2351aa`), a 24-unit white default-material text component at `(0, 7000, -360)`. This is an authored map object rather than HUD text. It and other standalone 24-unit route labels need an architectural wayfinding treatment; no map text was changed in this pass.

An earlier cold visible launch (`FreshM12M13PostZ11Companion-20260923-110746-c783fcaa`) stalled during async loading and was stopped without a gameplay result. The first filesystem-cache route (`FreshM12M13PostZ11CacheRetry-20260923-111722-894d8b0f`) passed CP0 but its E1 pilot died after three bounded native retries. A read-only map audit identified the target obstruction as an intentional colliding cover coffer, not stray collision. The successful repeat selected six E1 targets, used zero retries, and exercised no new occluded-target switch. It does not prove that the pilot change fixed the earlier E1 failure or that E1 difficulty is settled.

`FreshM12M13CP9Reload-20260923-115406-af0a5589` copied the two exact earned CP9 banks from the successful run into an isolated user directory, bootstrapped M12, and requested two public `LoadSlot(CHECKPOINT, 0)` operations. Both loaded into new M13 worlds and preserved the 35-entry journal, evidence provenance, stable identities, inventory/ammo, resources, lift state, and separate exit locations. Each world settled for four seconds with movement/look released, one visible Tarrik HUD, and no pause. Two native CP9 refreshes preserved the earned journal and evidence. `reload-0.png` and `reload-1.png` were visually reviewed: the same stable departure room and one HUD are present, with no duplicate overlay or cinematic residue. Both maps remained byte-identical to their pretest files.

This validates one fresh synthetic-input route and two earned reloads in editor PIE. Physical keyboard/mouse feel, audio, packaged build, sustained companion damage share, every ability under interruption/death, and the architecture's final rendered quality remain open. The run used `-DisablePlugins=Aura` and the filesystem DDC fallback. M12 SHA-256: `23555516889B0078EA29323F375B1E2D43E5EDF1132AD903E68989BB51DA61F1`; M13 SHA-256: `939BE896BD7FBE9C062FC11E641A41A987BBA9CDDE7218369373474C1ABFEC5D`.

## Repeat against the later local M12 build

`FreshM12M13CurrentMap-20260923-172049-7cc5e863` began again at CP0 in
visible UE 5.7 PIE. The E1 ordinary-input continuation passed without a native
retry, and all eight follow-on drivers passed E2, E3 entry/rescue, E4
entry/E4A/E4B, M13 travel, and M13 separate departures. The run recorded 20
incoming E1 damage receipts, both E1 holds, all 22 M12 and 13 M13 journal
entries, the paired lift, CP7/8/9, and separate exit positions. Its native
departure report says `m12_complete=true`, `m13_complete=true`, and
`campaign_completion_claim=false`. The route retained gameplay references
were cleared. This is a second full synthetic-input route on a later **local,
uncommitted** M12 asset; it does not supersede the earlier run's wall-runner
animation sample evidence.

`CurrentMapCP9Reload-20260923-174039-4c08d0e4` copied the two exact CP9 banks
earned by that run into an isolated save directory and made two public
checkpoint loads. Both callbacks succeeded in distinct fresh M13 worlds; both
retained all 35 journal entries, earned evidence, resources, inventory,
companion identity, lift state, and separate exits. Both CP9 refreshes preserved
the source journal and evidence. The report has no error and says
`maps_unchanged=true`; its machine status was `passed_requires_visual_review`.
I reviewed [reload 0](AurelionCurrentMap-2026-09-23/reload-0.png) and
[reload 1](AurelionCurrentMap-2026-09-23/reload-1.png): each shows one stable
Tarrik HUD and no duplicate/canceled cinematic overlay. The departure room
still appears sparse in these frames and is not visually approved.

The latest local map hashes through this review were M12
`64A4517BB88719694793A46AE859F0EB6FBC861EEF10E956E0574D8962B844A4`
and M13 `74DB49025DEE347F27A890377AE01DB053CADEDB68BDD3CDABE7AD8E1D1BAC8A`.
Neither test saved a map. As above, this is editor PIE with ordinary synthetic
Enhanced Input, not physical-device, audio or packaged-build acceptance.
