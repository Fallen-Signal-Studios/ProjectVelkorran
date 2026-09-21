# Fifth Witness skin comparison — 20 September 2026

## Actual chamber comparison

`FifthWitnessSkinBuffers-20260920-181536-525df227` restored the earned E4B exit,
completed native M12 aftermath/travel, and completed all thirteen M13 receipts,
the paired lift, evidence checks, and CP7/8/9. Its wrapper deliberately waits five
seconds before interacting with SeleneIndependentAssent. This is a paced route
and **not** a regression pass for the still-unfixed fast-interaction startup race.

The wrapper pauses FifthWitness via the public cinematic API for ten render
captures, then restores settings and resumes. It does not edit actor transforms,
materials, lighting, inventory or proof. The camera remains fixed; character idle
animation can continue while the sequence is paused. The full route report and
separate `fifth-witness-skin.json` both completed without error. Process exit 0,
no timeout.

Viewed the original lit shot, base color, post-VT-flush, SSS-disabled and restored
lit shots. Selene has a normal skin tone **before** any diagnostic command and
retains it throughout. Both protagonists remain correctly framed. The green
chamber rendering from the earlier FifthWitnessPlayback run was not reproduced;
this is comparison evidence, not a repair. The probe also collects roughness,
world normal, metallic, subsurface and shading-model buffers for comparison.

## Peach-fuzz isolation

`SelenePeachFuzzIsolation-20260920-182449-9f04d2a0` uses the existing public CP9
reload portrait harness. It records actual groom assets/materials and temporarily
hides only the peach-fuzz component, captures it, restores visibility and captures
again. The real component uses `Peachfuzz_M_Thin` with the expected
`/Game/MetaHumans/Common/Materials/MI_PeachFuzz` material.

Viewed original and fuzz-hidden frames: hiding fuzz removes the fine specular
stipple, but the underlying face remains dark in this checkpoint lighting. This
does not explain the earlier intermittent green chamber image. No groom removal,
material replacement, skin tint adjustment or lighting change was saved.
The process exited 0 without timeout, report error is null, and map hashes match.

## Verification and remaining work

Full build/automation/coverage/source-integrity gate without SkipBuild:
`Saved/Validation/20260920-182711-1bfdf3f4`, 722 matching tests passed.
No native or production content changed. The intermittent green rendering and
the native cinematic startup race remain unresolved; approval for the latter is
pending. The unusually dark CP9 character framing warrants a separate lighting
readability review instead of further speculative edits to the skin material.

- M12 hash: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`
- M13 hash: `CCAF63F21731315B8A19644D455F439222D3AE4F2577A826FB0D1C807CCE52CB`
