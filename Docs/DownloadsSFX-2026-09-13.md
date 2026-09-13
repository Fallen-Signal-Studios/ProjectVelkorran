# Downloads SFX library

The user offered the SFX folders in Downloads for the project. Inventory found
1,941 WAV files across nine named Sound FX libraries; macOS metadata sidecars
were excluded. The original Downloads files remain in place.

A first selection of 20 files is imported under
`/Game/Aurelion/Audio/DownloadsSFX`. This is a candidate library for audition and
mixing, not a change to live gameplay cues or an audio acceptance pass.

| Folder | Files | Intended audition use |
|---|---:|---|
| Weapons | 6 | Energy shots, reload/readiness, target lock, returning throw |
| Machines | 4 | Robot movement, mechanical swing/startup, energy pulse |
| Verity | 3 | Soft, electric and kinetic whoosh layers |
| Holographic | 2 | Subtle interface/glitch feedback |
| Ambience | 3 | Spaceship, damaged ship and factory interiors |
| Elemental | 2 | Flame burst and electric fire movement |

`Scripts/Editor/Manifests/DownloadsSFX-2026-09-13.json` records source paths relative
to Downloads, SHA-256 hashes, duration, channels, sample rate, bit depth and
destination names. `inventory_download_sfx.py` reproduces the shortlist;
`import_download_sfx.py` refuses to overwrite existing assets and checks the
imported SoundWave type, duration and channel count. It requires stopped PIE.

Import evidence:
`Saved/Validation/Aurelion/DownloadsSFXImport-20260913-164247-4b60829e/downloads-sfx-import.json`.
The shortlist was selected by file labels and format metadata. Audition, gain,
layering, spatial attenuation, concurrency and comfort checks are still pending
before assigning these sounds to production events.
