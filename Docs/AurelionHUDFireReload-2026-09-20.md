# Live HUD firing and reload check

Run `HUDFireReload-20260920-021315-9d71c2c8` used an isolated M12 PIE session and the actual Cinderline granted by the starting inventory. After the existing ammo/radar/Blackout fixture passed, `check_hud_fire_reload.py` pressed and released the ASC attack input, then the reload input. It did not set ammunition or inject display values.

| State | Native clip / reserve | Visible split fields |
|---|---|---|
| Loaded | 32 / 218 | 32 / 218 |
| After firing | 23 / 218 | 23 / 218 |
| After reloading | 32 / 209 | 32 / 209 |

All three states matched the native holographic view and the visible `AmmoClip`/`AmmoReserve` text widgets. Nine rounds were consumed; reloading transferred nine from reserve. Both `hud-after-fire.png` and `hud-after-reload.png` were inspected and show the same values, with the large clip count and smaller reserve hierarchy intact. The fixture passed and Unreal exited normally.

This closes the controlled Tarrik Cinderline fire/reload presentation check. It does not qualify physical input routing, Selene weapon handoff, empty reserve, fullscreen layouts or complete mission behavior. Captures include editor chrome around an approximately 845×550 gameplay viewport. No assets or maps were saved.

The captures also show the objective readout's legacy-looking typography beside the holographic surface. Its font is assigned at runtime in `USovAccessibilityPresentation::LayoutObjectives` using `FCoreStyle::GetDefaultFontStyle`, rather than an editable objective widget style. That remains a native presentation follow-up alongside waypoint/subtitle overlap; no C++ change was made under the content-only handoff.

Full validation `20260920-021610-06fce633` passed: build without SkipBuild, all 719 matching automation tests, coverage and source integrity. Pre-change baseline: `20260920-020925-7316a8b2`. M12 retained its protected SHA256 `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
