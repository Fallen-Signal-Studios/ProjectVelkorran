# M13 rail and destination-sign readability

The M13 chamber rail infill now uses a local patinated-bronze material override, and the three existing destination signs use an owned unlit text material. No geometry, actor transforms, collision, text wording, sign placement or scene lights change.

Fresh audit `M13ReadabilityAudit-20260920-030336-56ebd86e` found the rail's shared Reveal material used dark base tint `(0.065,0.085,0.095)`, textured blend 0.18, roughness base 0.57 and metallic 0.35. `M_AurelionKit_RailBronze` duplicates it, using tint `(0.18,0.15,0.105)`, textured blend 0.14 and roughness 0.48 plus 0.12 texture variation. It remains lit and non-emissive. The original Reveal material and mesh defaults remain unchanged: the override applies only to the four M13 rail components.

`M_AurelionKit_WayfindingText` duplicates the engine's default opaque text material, retaining its font/mask graph while sending its existing base-color output to emissive under the Unlit shading model. It is assigned only to `Aurelion_Art_Sign_Z10_72eb71`, `Aurelion_Art_Sign_Z11_9bb415` and `Aurelion_Art_Sign_Z12_f79ad4`. This prevents the existing white lettering from turning dark under the chamber lighting; it does not brighten unrelated labels or change mission-journal text.

Preview `M13ReadabilityPreview-20260920-030652-2ca02432` produced four inspected captures. Comparing the same rail camera with the previous rail pass shows clearer metalwork against the black background. The chamber approach label, observation-gallery direction and Dominion/Reformation departure split are legible in the inspected views. The text remains world-space lettering in its existing placement; this does not claim a complete environmental-signage redesign.

Authoring is scoped to two new material assets and M13 component overrides. The save wrapper backs up M13, reloads both overrides and all three signs, verifies wording/transforms and actor/collision state, and checks that M12 is untouched. No C++ or journal semantics change is involved. Fixed editor captures do not qualify moving-camera readability, all display conditions or full mission playability.

Saved run `M13ReadabilitySaved-20260920-030944-8513f599` completed and reloaded all four rail overrides and all three sign overrides. Its report confirms unchanged scene lights and protected M12. Final validation `20260920-031153-0a7c4bbf` rebuilt successfully and passed all 719 matching automation tests (95 warnings), with source integrity unchanged. A packaged build was not run.
