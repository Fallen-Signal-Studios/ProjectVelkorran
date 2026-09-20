# M13 chamber balustrades

The chamber and approach railing artwork replaces 192 stock spaceship-rail instances with 80 fitted Aurelion sections. Four mesh sizes cover the measured combinations: 2.3398×0.22×1.3 m, 4.1231×0.22×1.3 m, 2.5×0.22×1.3 m and 5×0.7×1.8 m. They use dressed stone posts, recessed flutes, gold grip edging and open pointed metalwork consistent with the existing Z09 balustrade design.

Fresh measurements: `M13RailMeasure-20260920-024817-eaecb003`. Source and generation scripts live in `Art/Source/Aurelion/Z10RailingKit`, `fit_z10_railing_groups.py` and `build_z10_railing_kit.py`. Run the fitter with Python, then the builder with Blender 4.5 `--background --factory-startup --python`. The builder reuses the tracked Z09 design's construction block, varying bay count and fitted dimensions. Each owned FBX has two UV channels, Nanite with explicit position precision/full fallback, and no generated collision.

The fitter groups matching longitudinal axes and orientations only when their transverse envelopes touch. It checks a grid over the union to reject any group that would fill a gap. Every original instance appears exactly once in the resulting groups. General quaternion transforms preserve the sloped approach, rather than assuming every rail is level. Original transforms and mesh bounds remain in the retained baseline.

The editor validates every replacement's transformed bounding envelope against the corresponding original group. Preview `M13RailingPreviewChecked-20260920-025426-8b64e656` passed with maximum envelope error 0.001192 cm. Other actor transforms/collision and the art owner's non-instance primitive components are preserved. Only the decorative rail component is replaced and expanded into four mesh components; native barriers remain authoritative and the artwork does not affect navigation.

The first preview stopped at an overly strict single-primitive-component assertion before importing or changing the map. The corrected check explicitly preserves the owner's other primitives. No rejected map was saved.

The save wrapper backs up and saves only M13, reloads all 80 placements, checks collision/navigation settings and verifies the protected M12 hash. This presentation change does not alter mission-journal semantics or require a content revision bump. Fixed editor captures and automation do not establish moving-camera performance, a full mission replay or overall AAA completion.

Saved run `M13RailingSaved-20260920-025711-fb96bc36` passed reload checks for all four components and 80 instances, owner/native primitive state, other actor collision/transforms and the protected M12 hash. The preview rail detail/approach and saved taller bridge rail were inspected. Dark metalwork has weak contrast against the unlit background in the close views; lighting/readability remains a follow-up, not a completed visual-quality claim.

Full gate `20260920-025957-58a80bf9` passed build without SkipBuild, all 719 matching automation tests, report coverage and source integrity using the filesystem-cache flag. Pre-change full baseline: `20260920-024356-f4106c81`. No tracked edits occurred during validation.
