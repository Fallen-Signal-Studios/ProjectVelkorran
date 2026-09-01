# Campaign TDD v2 change log

The 14 August 2026 campaign TDD supersedes the December 2025 class/loot campaign design. The TDD's own source hierarchy also places explicit creator decisions made after the manuscript ahead of the TDD baseline. This file records those later decisions so implementation differences remain deliberate and reviewable.

| Decision | Campaign treatment | Systems affected |
|---|---|---|
| Revised Tarrik Echo ability roster | Approved replacement for the TDD's prototype ability names and payloads | Ability sets, input slots, animation, effects, balance |
| Revised Selene Echo ability roster | Approved replacement for the TDD's prototype ability names and payloads | Ability sets, weapon context, status effects, animation, balance |
| Player Health recharges after a no-hit delay | Approved survivability rule; player-only and unable to revive | Damage lifecycle, HUD, difficulty, checkpoints |
| Cinderline uses a magazine and finite reserve ammunition | Approved authored weapon rhythm, not a loot-economy feature | Weapon item, reload, HUD, combat pacing |
| Hostile kills may drop short-lived ammo or small Echo sustain | Approved narrow combat-sustain exception; no rarity, inventory, or persistent loot behavior | NPC death, pickups, ammo, Echo, encounter tuning |
| Ordinary Cinderline hits use deterministic range/variation | Approved tuning layer; signature abilities and explicit damage payloads remain fixed | Primary-fire damage, testing, balance |

These decisions do not reopen classes, randomized gear, vendors, crafting, co-op, or a loot treadmill for the launch campaign. Any further material change should record the player problem, affected TDD pillar and acceptance gate, implementation/content impact, test impact, and decision owner.
