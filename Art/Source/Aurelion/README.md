# Aurelion recess wall candidate

Source-art candidate for the layout plan's white stone and narrow gold channels
(Aurelion_Level_Layout_Plan.pdf, pages 20 and 23). This is not installed in M12.
The panel does not resolve survivor targeting or prove a safe recess entrance.

`SM_Aurelion_RecessPanel_2m.blend` contains the editable mesh and studio scene;
the FBX contains only the panel and one named UCX box. The PNG is a Blender
studio preview, not an Unreal gameplay screenshot. Four material slots separate
stone, gold, shadow channels and warm light inlays. Unreal materials still need
authoring and review; FBX does not guarantee Blender shader equivalence.

Dimensions are 2 m wide, 0.4 m deep and 3 m tall, with a bottom-centre pivot.
The detailed front faces local -Y. UV0 uses packed islands; Unreal lightmap UVs
and final rendering settings remain pending. Simple collision fills the complete
envelope. Do not scale this panel onto existing wall segments without checking
their dimensions, passage clearance and both priority/cinematic exit paths.

Rebuild using Blender 4.5:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Aurelion/build_recess_panel.py
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Aurelion/verify_recess_panel.py
```

The clean-process FBX round-trip verifies two mesh objects, metre dimensions,
bottom-centre visual pivot, UVs, four material slots and eight collision vertices.
See the adjacent verification JSON for exported triangle count. The preview was
visually inspected on 2026-09-13. Unreal import, collision, lighting, navigation
and performance acceptance remain open. No alignment points awarded yet.
