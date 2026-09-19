"""Inspect project and stock HUD compass ownership without changing assets."""
import json
import os
from pathlib import Path
import unreal

rows = []
for path in ('/Game/Aurelion/UI/WBP_AurelionGameplayHUD', '/Game/UI/Narrative/Menus/GameHUD/WBP_DefaultGameplayHUD', '/NarrativePro/Pro/Core/UI/Menus/GameHUD/WBP_DefaultGameplayHUD'):
    bp = unreal.load_asset(path)
    if not bp:
        rows.append({'asset':path, 'missing':True})
        continue
    tree = list(unreal.SovWidgetTreeAuthoringLibrary.describe_widget_tree(bp))
    widgets = []
    for widget in unreal.ObjectIterator(unreal.Widget):
        parent = widget
        for _ in range(32):
            if parent == bp:
                break
            parent = parent.get_outer() if parent else None
        if parent != bp:
            continue
        slot = widget.slot
        item = {'name':widget.get_name(), 'class':widget.get_class().get_path_name(), 'visibility':str(widget.get_visibility()), 'parent':widget.get_parent().get_name() if widget.get_parent() else None, 'slot':slot.get_class().get_name() if slot else None}
        if isinstance(slot, unreal.CanvasPanelSlot):
            item['layout'] = slot.get_layout().export_text()
        elif isinstance(slot, unreal.OverlaySlot):
            item['padding'] = slot.get_editor_property('padding').export_text()
        widgets.append(item)
    rows.append({'asset':path,'tree':tree,'widgets':widgets})
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'compass-ownership.json').write_text(json.dumps(rows, indent=2))
unreal.log('HUD_COMPASS_OWNERSHIP_CAPTURED')
