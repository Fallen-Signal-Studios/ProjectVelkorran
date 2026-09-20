# Tarrik opening-contact isolation

The previous fresh campaign capture proved companion rotation during root
motion after `0805af89`, but recorded no Tarrik damage. Before changing the
authored sword geometry or timings, two isolated PIE diagnostics exercised the
same native attack definition against the owned Linkbound enemy.

`TarrikLinkboundContact-20260920-000652-2160f7f1` used the existing protagonist
melee probe with only Tarrik and Linkbound selected. It recorded two native
light attack identities and one native heavy hit. Its original requirement of
three distinct light identities failed; the target died after the second hit.
The first attack did not register a hit in that run. Process exit 0 is not a
gameplay pass.

`probe_tarrik_opening_contact.py` isolates a single light press at four fixed
distances, retaining the existing probe's held target and heavy comparison.
`TarrikOpeningContact-20260920-000917-2e0e6e3b` completed all four trials:

| Held target distance | Opening light damage | Receipt time after input |
|---|---:|---:|
| 80 cm | 50 | .453 s |
| 110 cm | 50 | .407 s |
| 140 cm | 50 | .422 s |
| 170 cm | 50 | .422 s |

All light results identify `SovEchoAttackReceipt` and
`GA_Tarrik_MeleeLight_C`; each heavy comparison also registered native damage.
The diagnostic's `passed` field means it completed with the expected native
grants. Per-trial `opening_contact_observed` and raw receipts separately record
contact. The Linkbound target is repositioned continuously by this controlled
probe, which does not qualify ordinary AI motion, companion dispatch, or a
campaign encounter. No attack assets, damage, health or timing values changed.

This contradicts a universal opening-animation or Linkbound-collision failure,
and provides no basis for inflating radius, shortening bot range, or replacing
the opener. The remaining check must compare the properly initialized campaign
companion's live pose/trace dispatch with this working player path. Its short
fresh-route contact window remains insufficient to establish reliable attacks.

Both isolated editors stopped PIE and exited normally. The full baseline gate
`20260920-000625-43ab387d` passed build, 719 tests, coverage and source integrity.
Post-change full gate `20260920-001251-780ccf05` also passed build without
SkipBuild, all 719 tests, coverage and source integrity. No tracked edits occurred
during either gate. The creator's M12 map and grenade work are excluded, and the
protected M12 disk hash remains unchanged.
