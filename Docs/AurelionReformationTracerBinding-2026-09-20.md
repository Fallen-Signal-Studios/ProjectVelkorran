# Reformation tracer reader repair

Fixed the shared, already-tracked `NS_WeaponFire_Tracer_Reformation` Niagara asset. Its secondary sparks reader and corresponding cached system-update interface referenced a nonexistent emitter named `ref`. Both now reference the existing main `Tracer` emitter. No C++ changes were needed.

The actual referencers are `BP_SovStaccatoVisual`, Narrative `BP_Staccato_Visual`, and `BP_AxiomVisual`. The initial description as an enemy tracer was too broad: the warning appeared during E2 combat, but the asset reference inventory points to these protagonist weapon visuals.

## Evidence

- `TracerBindingInspect-20260920-205047-69456ca4`: original full text export and five-emitter inventory. Tracer, Sparks1, Sparks2, Sparks3 and Flash are enabled; `ref` does not exist.
- `TracerNativePropertyPreview-20260920-205548-d64730a4`: unsaved exact repair. The original simulation produced three missing-`ref` warnings. Both bindings read back as Tracer after the change. The initial snake-case Python property attempt in `TracerBindingPreview-20260920-205408-42ab299a` failed before editing; using reflected `EmitterBinding`/`EmitterName` names resolved access.
- `TracerBindingSave-20260920-205819-f0917cd2`: saves only the reviewed Niagara asset, with the original backed up as `NS_WeaponFire_Tracer_Reformation.before.uasset`. Emitter/renderer/user-parameter readback remains identical. Both mission map hashes are unchanged. Saved SHA256: `b7260164b83d29f1026ae4224076449b061e49ecf301fac0a43954c1c87a56ff`.
- `TracerBindingReadback-20260920-205945-356c42a4`: separate editor process verifies the saved authored and cached bindings, simulates the effect and exports the asset again. Zero missing-ref warnings, zero particle-read failures and zero Python errors. Process exited normally.

All run directories are under `Saved/Validation/Aurelion`.

The before/after studio images were inspected, including the fresh saved-asset image. They show only a faint horizontal tracer in this fixed-age setup. They are not sufficient evidence of final color, spark density, shot timing or mission readability. No material, scale, lifetime, damage, ammo or weapon binding was changed to make this isolated capture look better. Actual Staccato/Axiom fire in M12/M13 still needs visual acceptance.

Reproduction scripts: `inspect_reformation_tracer.py`, `preview_reformation_tracer_binding.py`, `fix_reformation_tracer_binding.py`, and `verify_reformation_tracer_binding.py` under `Scripts/Editor`. The repair refuses to reapply once the old binding is absent. The fresh verifier renders the already-fixed asset twice; its inherited before/after image filenames do not mean it restores the broken binding.

Baseline full build/test gate: `Saved/Validation/20260920-204835-2dc512b0` (722 tests). Final gate `Saved/Validation/20260920-210204-36466a31` passed the full build, all 722 matching tests, coverage and source integrity. These tests do not replace live visual acceptance. No protected map or creator grenade/GASPALS changes were saved or staged.
