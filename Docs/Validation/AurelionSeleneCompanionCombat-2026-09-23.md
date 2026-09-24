# Companion sustained combat: earned E4B replays — 23 September 2026

Three independent visible UE 5.7 PIE runs publicly loaded the same earned E4B checkpoint, used the authored retry and ordinary synthetic Enhanced Input to complete frost, heat/Poise, Core follow-up and conventional arena victory with the protected people alive. The passive contribution observer changed no actor, damage, resource or command state. All three E4B route drivers passed. This is in-engine functional evidence, not physical-device, packaged-build or multiplayer acceptance.

| Earned retry | Selene health damage | Tarrik health damage | Selene share | Selene hits | Verity Twin Blade montage samples | Observer errors |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `E4BSeleneLiveContribution-20260923-204259-a9b4cafb` | 114.55 | 550.05 | 17.2% | 6 | 44 | 0 |
| `E4BSeleneContributionRepeat-20260923-204826-ef4f4bed` | 100.75 | 563.85 | 15.2% | 9 | 63 | 0 |
| `E4BSeleneTwinBladeVisible-20260923-205458-17842961` | 120.91 | 543.69 | 18.2% | 10 | 43 | 0 |

These are native source-damage receipts, not estimated animation hits. Selene wielded Verity, focused enemies, played the authored Twin Blade attack montage and hit Linkbound, Weaver, WallRunner and Elite across the runs. The optional `SOV_COMPANION_CAPTURE=1` observer captured a real player-view [attack frame](AurelionSeleneCompanionCombat-2026-09-23.png) during the montage at about 3.8 seconds. Selene is small in that framing; the image confirms combat context and visible companion presence but is insufficient to sign off the close-up pose or blade contact. Both protagonists remained alive at the terminal state. M12 and M13 map hashes were unchanged.

An additional normal Selene-led E4A route, `E4ATarrikNormalContribution-20260923-210026-6348153f`, passed Weaver sever, WallRunner release, protected-survivor checks, Tarrik handoff and active E4B entry in 10.84 seconds. Tarrik wielded Velkorran and chased a living Linkbound from 12.0 m to about 4.0 m, then focused the protected Elite before the short phase ended; he landed no hit in this run. An earlier 9.22-second normal E4A replay landed one native Tarrik hit, and the separate no-player-input opening replay landed two Linkbound sword hits. This variation leaves Tarrik's contribution in short normal E4A phases unqualified; it does not negate the proved autonomous draw, montage and damage path. Continue testing sustained Tarrik combat during an active player encounter and inspect why ordinary targets disappear or leave his reach before the handoff.

The observer's new screenshot option is read-only and opt-in. `run_tarrik_companion_contact.py` can now include the same passive contribution audit with `SOV_E4A_COMPANION_AUDIT=1`. Neither production C++ nor map content changed for this qualification.
