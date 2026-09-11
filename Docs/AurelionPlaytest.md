# Aurelion playtest — candidate handoff

This is a Development graybox of the M12–M13 Aurelion route. Full-route completion, packaged gameplay, and recovery through every checkpoint are still being verified. This guide describes the intended entry and the current controls; it does not certify that the route has passed.

## Start at M12

Extract the complete Windows archive. Place `Play-Aurelion.ps1` beside its `ProjectVelkorran` folder, then run it in PowerShell:

```powershell
.\Play-Aurelion.ps1
```

If the launcher is stored elsewhere, pass `-ArchiveDirectory` with the extracted `Windows` folder. The launcher opens a visible 1600 × 900 window at M12 and creates a separate save/log folder under `%LOCALAPPDATA%\ProjectVelkorran\AurelionPlaytest`. Each launch starts with a fresh profile and retains earlier folders.

Begin as Tarrik at the southern approach, use the first interaction prompt, and follow the current objective. Perspective changes and the move into M13 belong to the route. Do not start M13 directly: it expects the preceding mission's journal and companion state. Opening the game executable without a map argument currently goes to the stock main menu.

## Keyboard and mouse

| Control | Action |
| --- | --- |
| WASD / arrow keys; mouse | Move; look |
| Space; C; Shift | Jump; crouch; sprint |
| Q | Evade |
| Left mouse | Primary attack |
| Right mouse | Aim or the wielded weapon's defense |
| Hold T, move the mouse, release T | Select and equip a weapon from the wheel |
| R | Reload |
| Hold E at a prompt | Interact; the accessibility tap setting can change this |
| G; X; F | Ability 1; weapon technique; signature ability |
| Middle mouse | Bash / heavy attack |
| 1 | Cover |
| Tab; Escape | Inventory; pause |

Available abilities depend on the protagonist, the equipped weapon, and the displayed resources. The number keys are not a weapon-selection shortcut. Begin the drone fight with Cinderline selected through the weapon wheel. Use the visible prompts for story interactions and handoffs.

Settings and remapped keys take precedence over this default list. Controller support has authored bindings; this guide makes no claim of a completed Aurelion gamepad pass.

## Status to fill after validation

Record the final code revision, archive hash, packaged startup checks, and the exact route segments played before distributing this candidate as an accepted build. Visual presentation, voice, and choreography remain prototype work. A successful cook or a native test total alone does not establish that the route is playable from beginning to end.
