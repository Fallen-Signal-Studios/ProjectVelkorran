"""Fresh saved-asset verification; compile in memory, never save."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
# Common UI requests this configured class while compiling its action bar.
# Resolve it first to avoid a nested compile request inside the HUD compile.
for name in ('ControllerData_PC_KB', 'ControllerData_PC_Gamepad_Xbox'):
    controller_data = unreal.load_class(None, f'/NarrativePro/Pro/Core/UI/ControllerData/{name}.{name}_C')
    assert controller_data and unreal.get_default_object(controller_data)
bp = unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
minimap = author.find_widget_in_tree(bp, 'WBP_Navigator_Map_Minimap')
wrapper = author.find_widget_in_tree(bp, 'RetiredMinimapContainer')
assert minimap and isinstance(wrapper, unreal.Overlay)
assert minimap.get_parent() == wrapper
assert wrapper.get_parent().get_name() == 'CanvasPanel_Base'
assert minimap.get_visibility() == unreal.SlateVisibility.COLLAPSED
assert wrapper.get_visibility() == unreal.SlateVisibility.COLLAPSED
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
task = unreal.AssetExportTask()
for key, value in dict(object=bp, exporter=unreal.ObjectExporterT3D(),
        filename=str(out/'hud-saved.t3d'), automated=True, prompt=False,
        selected=False, replace_identical=True).items():
    task.set_editor_property(key, value)
assert unreal.Exporter.run_asset_export_task(task)
(out/'minimap-saved.json').write_text(json.dumps(dict(status='passed',
    scope='Saved nesting, collapsed defaults and in-memory compilation; runtime separate',
    tree=list(author.describe_widget_tree(bp))), indent=2))
