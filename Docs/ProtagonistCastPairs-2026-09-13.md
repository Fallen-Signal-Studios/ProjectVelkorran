# Protagonist cast pairs — 13 September 2026

The ten native protagonist Echo abilities now each reference two project-owned
FullBody cast montages. `USovGameplayAbility_EchoBase` plays them A/B/A/B after
successful commit. Each granted ability instance has its own counter; rejected
admission and failed montage playback do not advance it. Regranting an ability
starts at A. Successful instant payloads allow cosmetic recovery to finish;
cancellation stops only the montage still owned by that ability.

The missing presentation hook was demonstrated by the direct-native ability
Blueprints: their existing animation tags had no playback consumer. The native
change adds only cosmetic playback and cleanup. Payload timing, targeting,
damage, costs, recovery gates and progression are unchanged.

| Protagonist | Ability | A/B motion family | Duration |
| --- | --- | --- | --- |
| Tarrik | Cinder Sticky Grenade | Narrative combat toss / trimmed cinematic throw | 0.75 s |
| Tarrik | Velkorran's Hunger | Opposing one-handed sword releases | 0.75 s |
| Tarrik | Cinder Slam | Two heavy two-handed sword motions | 1.20 s |
| Tarrik | Cinder Judgement | Rifle stance with opposing brace/recoil offsets | 0.73 s |
| Tarrik | Cinderline Requiem | Longer rifle brace/recoil variants | 1.10 s |
| Selene | Stillpoint Grenade | Narrative combat toss / trimmed cinematic throw | 0.75 s |
| Selene | Verity's Wake | Retargeted Twin Blade attacks 01 / 03 | 0.75 s |
| Selene | Staccato Zero | Rifle stance with opposing brace/recoil offsets | 0.65 s |
| Selene | Axiom Null Pulse | Pistol stance with opposing brace/recoil offsets | 0.70 s |
| Selene | Dispatch | Retargeted Twin Blade attacks 02 / 04 | 0.85 s |

Content lives in `/Game/Characters/Animation/ProtagonistCasts`: twenty montages
and twenty independent clips. Every clip uses `SK_Mannequin_Narrative`. Cast
sequences and montages contain no notifies; sequence root motion is disabled and
root locking enabled. This prevents copied melee damage windows, motion warping,
or pack demo effects from firing during a cosmetic cast. Original pack assets are
unchanged. Guard and Deflection are held defensive stances, outside this Echo cast
table; their existing behavior and weapon animation remain intact.

Firearm variants are baked from existing Narrative weapon idles with small
opposing upper-body brace/recoil offsets, retaining the original grip. The
alternate throw uses source seconds 2.0–3.2 at 1.6x, after a read-only hand-motion
probe located the throw burst at 2.50–2.63 seconds. Its peak closely matches
Tarrik's existing 0.35-second grenade release. Combat Master Dynamic Great Sword
sources were excluded because their relocated skeleton references fail to load.

## Verification and limits

- Windows Development Editor builds passed, including the final editor helper.
- Ten `ProjectVelkorran.Campaign.Transactions.Actions` tests passed in
  `Saved/Validation/CastPlayback/20260913-170101-12e5b24f`.
  The new `EchoCastPlayback` test uses a real mannequin, AnimInstance, project
  montages and GAS activations in an isolated test world. It verifies A/B/A,
  rejected input, independent counters, successful recovery, cancellation and
  exactly five paid costs. This is not a live mission receipt.
- The focused playback test passed again after the final asset bake:
  `Saved/Validation/CastPlaybackFinal/20260913-171205-6bee9290`.
- Initial authoring saved ten ability bindings and forty animation assets:
  `Saved/Validation/Aurelion/CastPairAuthoring-20260913-165811-ffb17659`.
- Fresh editor reload verified twenty bindings. Final trim/brace edits and the
  all-twenty skeleton, root-lock and empty-notify audit are recorded in
  `Saved/Validation/Aurelion/CastVisualReview-20260913-170215-ff012b88`, including
  `FinalCastAudit/cast-pair-reload.json` and montage text exports.
- Limited editor previews showed the heavy cast's downward body motion and
  Axiom's intact braced grip. These previews do not qualify every montage on the
  dressed protagonists or companions, payload/pose alignment during real combat,
  or client prediction under rejection. Those remain live review items.

The broader Aurelion 90% alignment target is not reached or newly scored by this
content pass. Companion hit qualification, the E1 drone navigation blocker and
the Core lifecycle investigation remain open in the existing mission reports.

Authoring: `Scripts/Editor/setup_protagonist_cast_pairs.py` creates new assets and
refuses overwrites; `finalize_protagonist_cast_pairs.py` applies the scoped final
trim and clean brace-track bake. `review_protagonist_cast_pairs.py` is read-only.
