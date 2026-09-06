# Cinder Judgement: authoritative release geometry

Source repair for the Cinder Judgement obstruction/convergence finding in
`CodeErrorPass-2026-09-05-Combat.md`. This is engineering implementation evidence,
not a successful Unreal build or runtime test report.

## Existing behavior and failure

Judgement already owns one paid Echo activation, a native release timer/manual
release gate, Narrative weapon-channel traces, direct plus radial damage, and a
replicated presentation packet. Those systems are preserved.

The previous release discarded the eye origin after computing its aim point.
A weapon socket clipped through a thin wall therefore started its damage ray on
the far side. An eye aim point behind an offset forward muzzle also reversed the
shot. Fallback offsets were not bounded by the socket-distance limit, and invalid
eye/rotation data could enter trace/convergence math.

## Repair and compatibility policy

- Keep `UNarrativeCombatAbility::PerformTraceMulti` as the weapon-channel query;
  no separate targeting service, save state, or aim-assist policy was introduced.
- Resolve a finite authoritative eye, aim point, and normalized aim direction.
  Like Requiem, an eye beyond the configured muzzle envelope falls back to the
  avatar origin; non-finite eye/rotation data rejects release.
- Reject invalid avatar transforms. A non-finite, non-normalized, or displaced
  socket falls back to the existing avatar-local muzzle offset. The fallback must
  itself fit `MaximumMuzzleDistance`; broken authoring is not silently clamped.
- Bridge eye to muzzle with visibility and Narrative weapon-channel queries,
  ignoring the owner and its attachments. Honor the authored sweep radius.
  If obstructed, resolve the gameplay shot from the trusted eye rather than from
  a muzzle on the far side of the obstacle.
- Zero-length or reverse convergence uses the authoritative aim direction,
  never cosmetic socket forward. Range remains measured from the resolved shot
  origin; direct damage, radial damage, maximum-range blast choice, and cue packet
  values retain their existing meanings.
- Revalidate the original activation after authored eye/weapon accessors. A
  retired activation cannot publish a ray or cancel its replacement.
- Freeze the original ASC, avatar, world, source object, effect context, payload
  settings, and activation in a local shot snapshot before damage callbacks.
  The shared Echo execution guard is checked across spawn, target-attitude,
  direct-hit, radial iteration, physics, presentation, and recovery boundaries.
  Source cancellation/restart, avatar ABA, or zero-to-positive Health restoration
  stops remaining old-shot work. Already committed direct/radial damage is never
  refunded or rolled back; an obsolete deferred presentation actor is discarded.
  Recovery timers are correlated with the activation that created them.
- Payment remains in the existing Echo lifecycle: bad authored configuration
  fails admission; a release-time geometry failure ends an already-paid action
  without a refund, duplicate payment, or presentation packet. A wall impact is
  an ordinary paid shot, not a failed cost transaction.

## Regression coverage

Seven native tests register under
`ProjectVelkorran.Campaign.CinderJudgement.Geometry`:

| Test | Production behavior exercised |
| --- | --- |
| `ThinWall` | Real line/sphere release against a two-centimeter wall; origin resets to eye; no far-side direct/radial damage; one payment and one release |
| `ReverseConvergence` | A small eye-only obstacle lies behind an offset clear muzzle; shot remains forward and hits the legitimate target |
| `UnobstructedRelease` | Actual GAS activation, native direct plus blast damage, native packet, unchanged 50 Echo price |
| `MaximumRangePolicy` | Both authored dissipate and maximum-range blast cases preserve range from the resolved origin |
| `InvalidEyeAndFallback` | Non-finite eye and out-of-envelope fallback reject release without a shot, refund, or duplicate debit |
| `SocketValidation` | Valid socket accepted; displaced and non-normalized socket transforms select the safe fallback |
| `DisplacedEye` | A camera beyond the trusted envelope cannot bypass a near wall |

Two further registrations under `ProjectVelkorran.Campaign.CinderJudgement.Ownership`
exercise callback continuation ownership:

- `DirectHitRetiresOldContinuation`: a real direct damage callback cancels,
  restarts, rebinds the source avatar A-to-B-to-A, or restores its Health life.
  The direct hit stays paid/applied, no old radial damage or packet follows, and a
  restarted action survives the old recovery interval and owns its own release.
- `RadialCallbackStopsLaterTargets`: cancellation on the direct target's radial
  packet stops remaining radial candidates and old presentation publication.

The fixture changes authored values/socket outputs only; activation, payment,
collision queries, damage, and presentation run through production methods.
Physics worlds preserve the UE 5.7 Mac single-initialization fix. No new portable
math helper was required: these regressions specifically require Unreal collision
and GAS, not a disconnected approximation of them.

## Validation and remaining engine gates

Host `git diff --check` and the existing 30 Python source/report/layout checks pass.
The nine native registrations are authored and source-reviewed but **not run**:
this workspace has no UE 5.7/UHT/UBT installation. Required next validation is a
non-unity modular Editor build, then the geometry test filter above, followed by
the full combat automation suite. Exercise the actual Cinderline socket/animation
and authored visibility/weapon collision profiles in a packaged development map.
Console performance, cooked content, muzzle/beam presentation, and native engine
collision behavior are not certified by the host checks.
