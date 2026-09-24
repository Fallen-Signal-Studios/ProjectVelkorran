# M13 Z11 observation pier cladding, 24 September 2026

The previously plain, pale `Z11_Rib_1_-1` beside the observation window now
has an original Blender-authored visual shell. It retains the existing rib as
the sole structural collision owner and leaves the sealed panoramic
red-remnant / Wound / white-remnant view open. Fitted ivory courses, a narrow
black-stone inset, small keyed latches and restrained functional gold traces
make the support part of the same architectural family as the new ceiling.
This is a local visual improvement, not final room art or a 90% alignment claim.

The [focused reference](../ArtReferences/AurelionArchitecture-2026-09-23/Z11-Observation-Pier.png)
was generated from the saved Unreal room screenshot before modeling, with
the August TDD's material language, the September Z11 room layout and the
chapter-26 conversation/window composition as constraints. The
[prompt record](../ArtReferences/AurelionArchitecture-2026-09-23/prompts.md)
states the intended scope. The [editable procedural Blender source](../../Art/Source/Aurelion/build_z11_observation_pier.py)
and `.blend`, FBX, manifest and source render are in
[Z11ObservationPier](../../Art/Source/Aurelion/Z11ObservationPier/).

`Z11PierSurveyWide-20260924-053530-80677523` measured the native cube at
`(-1125, 43500, 300)` cm with half-extents `(50, 60, 300)` cm and active
query/physics collision. An independent Blender 4.5 FBX round trip passed
23,500 triangles, two UV channels, four named material slots, finite
positive-area geometry and 1.247 × 1.378 × 5.935 m bounds. No collision hull
was authored. The visual relief extends up to about 12 cm from the native
collision face, so close player movement remains a useful visual check.

`Z11PierPreview-20260924-054202-d87bda1c` imported the mesh and fit it in
visible UE 5.7 without saving M13. It verified unchanged pre-existing actors,
native collision, no new collision/nav influence and unchanged M12/M13 map
hashes. The player-height [room](AurelionZ11ObservationPier-2026-09-24/after-room.png)
and [near](AurelionZ11ObservationPier-2026-09-24/after-near.png) captures
were visually reviewed against the [prior room view](AurelionZ11ObservationPier-2026-09-24/before.png).
`Z11PierSave-20260924-054446-44eba4aa` then matched the reviewed placement,
source FBX and pre-save map hashes, saved M13, reloaded it and reverified the
visual shell plus native collision. M13 changed from
`091678448bd179b14630740368644c3c1ebbb4e98a11ab2de4e8b5accae149bc`
to `4cc2e71e8d0ae697c7a9cb08bc24b3992623413f03856d8f954879ca79d6598f`;
M12 stayed `64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4`.

The near view reads as a more deliberate support, but the white freestanding
information kiosks, very dark ceiling recesses, other wall faces and final
cinematic lighting remain open. Physical-input close navigation, audio,
packaged execution and target-PC performance remain separate qualifications.

On the saved map, the visible `Z11PierFreshRoute-20260924-054802-fa0e6da3`
ordinary-input PIE run passed fresh M12 entry, E1 on its first life, E2, E3
entry/rescue, E4 entry/A/B, actual M12-to-M13 travel and M13 entry. The M13
driver recorded nine of ten completed scenes and then stopped at a short
`SeparateDepartures` hold because it did not sample the 0.35-second native
countdown. The request callback had accepted that input. A separate read-only
inspection of the retained live world (`terminal-after-hold.json`) confirmed
that the final cinematic completed unskipped, the mission was complete, all
35 ordered journal receipts and three evidence records were present, the
separate exit positions were correct, and CP9 generation 39 was saved. Thus
the native route completed, but the uninterrupted input-driver report is
**failed**; this run is not counted as a clean all-stage validator pass. The
driver now permits the accepted request plus measured held-input frames and
elapsed game time to establish a missed short countdown; that revised branch
still needs a future full-route run.

The first public CP9 reload callback succeeded and its native snapshot matched
the earned final state, but the probe asserted on HUD validity 0.61 seconds
after world replacement. Its HUD wait was corrected. A second public CP9 load
from the still-completed game world passed after six stable seconds, preserving
all 35 receipts, evidence, facts, protagonist identities, inventory, resources,
lift state and both separate exit positions. It released movement/look input,
showed one valid Tarrik HUD and left both map hashes unchanged. The
[rendered CP9 frame](AurelionZ11ObservationPier-2026-09-24/cp9-reload.png)
was visually reviewed; the departure room remains sparse and does not resolve
the broader 90% visual target.
