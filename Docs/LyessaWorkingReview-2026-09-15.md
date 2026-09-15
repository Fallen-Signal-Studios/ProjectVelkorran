# Lyessa and Lyric working MetaHumans

`MHC_Lyessa` and `MHC_Lyric` are independent working assets under `/Game/Aurelion/Art/Characters/MetaHumans/Working`. Neither is assigned to a campaign NPC.

Lyessa currently uses the Jelena preset and dark MediumBobCurly groom after comparing Celeste and Jelena in the editor. The reference remains Eva Green. This is a starting head, not a likeness-qualified character. Eyes, facial sculpt, texture sources, wardrobe, rigging and runtime integration remain unfinished. The asset was saved explicitly before the Chaos source build. The editor reported mismatched source-root UVs for eyelashes and peach fuzz and a missing hidden-face map on the default garment; those warnings remain open.

Lyric is a factory-created working asset with Florence Pugh reference metadata. Preset selection and visual authoring have not begun. Tharne and the existing Selene character assets were not changed.

The docked editor helper resolves the installed native `EditorStyleSettings` class and temporarily chooses MainWindow, restoring the prior preference after opening. Earlier attempts used unavailable Python bindings/property names and failed; the corrected helper opened Lyessa successfully. No map changes were saved during this work.
