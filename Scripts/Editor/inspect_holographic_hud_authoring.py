"""Read-only widget inventory and reflection capability probe. Never saves assets."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
bp = unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
assert bp
author = unreal.SovWidgetTreeAuthoringLibrary
report = {'tree': list(author.describe_widget_tree(bp)),
          'native_bindings': list(author.describe_widget_bindings(bp)), 'probes': {}}
for name in ['bindings', 'status', 'ubergraph_pages', 'function_graphs']:
    try:
        report['probes'][name] = str(bp.get_editor_property(name))
    except Exception as exc:
        report['probes'][name] = str(exc)
for name in ['DelegateEditorBinding', 'EditorPropertyPath', 'EditorPropertyPathSegment']:
    cls = getattr(unreal, name, None)
    report['probes'][name] = {'available': cls is not None, 'doc': getattr(cls, '__doc__', '')}
    if cls:
        obj = cls()
        if name == 'EditorPropertyPathSegment':
            for prop, value in [('member_name', 'View'), ('is_property', True),
                                ('struct', unreal.load_class(None, '/Script/ProjectVelkorran.SovHolographicHUDSurface'))]:
                try:
                    obj.set_editor_property(prop, value)
                    report['probes'][name][prop] = 'writable'
                except Exception as exc:
                    report['probes'][name][prop] = str(exc)
report['dirty_maps'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
(out / 'hud-inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('HUD_INVENTORY_COMPLETE')
