# Player Echo interruption in PIE — September 23, 2026

Two focused, visible M12 PIE probes used the ready player pawn, wielded
signature weapon and normal semantic ability input. An editor-only accessor
located the **active instance** for that exact input so the probe could call
its existing cancellation path during the cast. Echo refill and a stationary
Enforcer target were test fixtures. The active ability cancelled before its
authored payload release; after observing cleanup, the probe refilled Echo and
sent the same input again. This is a direct cancellation test, not a claim that
an enemy strike or weapon wheel interruption has been exercised.

| Probe | Cancelled cast | Normal recast |
| --- | --- | --- |
| `TarrikHungerCancelPIE-20260923-143013-e16de6e9` | Twice, Hunger was active immediately after input, had spent exactly 50 Echo, then ended. No projectile or target damage appeared after its 0.35-second release deadline; neither Busy nor EchoAbility.Active remained. | Twice, the B cast appeared, spent 50 Echo, spawned its projectile and reduced the validation target's shield from 250 to 175. |
| `SeleneAxiomHeldCancelPIE-20260923-143730-1e25391d` | Twice, Axiom was held and active at 70 Echo. The probe cancelled it before release, then released the held input. No pulse damage, projectile, active instance or Busy tag remained. | Twice, a normal press/release played the B cast, spent 30 Echo and dealt a real target damage receipt. |

`SeleneWakeCancelPIE-20260923-143233-8448eaa4` was **inconclusive** as an interruption test: Wake spent 30 Echo and completed its instantaneous native release within the semantic input call, leaving no active instance to cancel. Its direct native implementation confirms this timing. The earlier full ability run qualifies Wake's normal A/B cast and target impact; no post-release cancellation claim is made here.

The UE 5.7 Development Editor build passed after adding the editor-only
accessor and held-input helpers (`20260923-143654-8d5e8a36`). The ten
`ProjectVelkorran.Campaign.Transactions.Actions` native automation tests passed
in `20260923-144006-f1d12643`. The M12 map was not saved and remains SHA-256
`23555516889B0078EA29323F375B1E2D43E5EDF1132AD903E68989BB51DA61F1`.

This narrows the open interruption work. Natural poise break, equipment change,
death, and checkpoint/handoff recovery during a live cast still need direct
play evidence; these two probes alone do not qualify those paths or the full
mission experience.
