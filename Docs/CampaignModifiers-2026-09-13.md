# Optional campaign modifiers — September 13, 2026

Five independent, stackable toggles sit beside the difficulty preset in the native settings menu. All default off. Changing a preset preserves these choices; toggling them does not select Custom. This is additional campaign work outside the August TDD and does not change the evidence-supported alignment score.

| Modifier | Gameplay effect |
| --- | --- |
| Blackout | Hides native objective waypoints, navigation markers, threat markers and weakpoint outlines. Objective text, subtitles, captions, interaction bounds and combat vitals remain available. |
| Famine | Suppresses ammunition from hostile enemies' native combat-sustain death drops. Echo drops remain. Carried ammunition, existing pickups, authored caches and generic inventory loot tables are unchanged. |
| Frenzy | Adds two simultaneous attack tokens against the player and friendly campaign actors, capped at eight, and multiplies hostile bot attack cooldowns by 0.65. Montage speed and impact timing remain authored. |
| Ascendant | Gives enemies 50% more effective health/shield toughness against the campaign team by dividing routed body damage by 1.5. It does not change rank, mesh, maximum health, current health or Poise. |
| Glass Cannon | Doubles routed body damage between hostile enemies and the campaign team in both directions. Poise remains independent. |

Ascendant and Glass Cannon together yield an outgoing body-damage multiplier of 4/3 and an incoming multiplier of 2, layered with existing difficulty and damage rules. Canonical fatal damage bypasses modifier scaling. Unrelated/friendly/environmental damage is unchanged. Team classification uses the current primary campaign pawn; dedicated-server and cooperative play are not qualified by this pass. Existing cooldown deadlines and attack leases finish normally after toggling Frenzy; subsequent scheduling/admission uses the current choice.

## Existing difficulty audit

The presets already affect more than attacker count. Story/Standard/Veteran/Sovereign have incoming player-damage multipliers of 0.65/1/1.2/1.35 and enemy attack-cooldown multipliers of 1.4/1/0.85/0.7. Their base attack-token budgets are 1/2/4/6. Story and Standard allow companion rescue; Veteran and Sovereign do not. Sovereign retains its campaign-completion unlock requirement. The existing "enemy recovery" setting controls attack cadence, not stagger duration. These preset rules were preserved.

## Persistence

The five flags persist with the user-settings config snapshot. Portable gameplay schema 2 appends a validated five-bit mask to the original eleven-byte payload (twelve bytes total). Schema 1 imports remain supported and disable all modifiers. Unknown bits, sizes and versions are rejected atomically. Accessibility and device-specific output preferences remain excluded from portable gameplay data. Ordinary slot loading retains current user settings under the existing save policy; explicit portable settings import restores the modifier mask.

## Validation

- Unreal 5.7.4 Development Editor build passed.
- Settings: 7 tests passed, including independent toggles, stacking, legacy import, actual ASC damage/token changes and actual fatal-receipt ammo/Echo drop decisions. `Saved/Validation/CampaignModifiersFinal/20260913-180135-7f857901`.
- Accessibility: 6 tests passed, including native menu transactions for all five controls and preserving the preset. `Saved/Validation/CampaignModifierUI/20260913-180224-2378ab22`.
- Defense routing: 19 regression tests passed. `Saved/Validation/CampaignModifierDefense/20260913-180313-2eab9213`.
- Platform settings privacy: 1 test passed. `Saved/Validation/CampaignModifierPrivacy/20260913-180353-ddbda9fb`.

Combat tests use isolated worlds with initialized test teams, PlayerState and health values; they are not live mission completion evidence. Early fixture failures were corrected before the passing runs. Blackout's paint gates were reviewed in source; rendered HUD presentation, live attack cadence and campaign balance still require playtesting. No grenade-specific, enemy-rank upgrade or death-reset modifier was added.
