"""Isolated PIE HUD layout preview; never qualifies narrative/gameplay progression.

Run after fresh entry without a combat input driver. This intentionally presents
QA text on the existing HUD, then lets its normal duration expire. It does not
write actors, resources, objectives, receipts, checkpoints or content assets.
"""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'dialogue-width-preview.json'
assert not out.exists(), 'Preserve the previous preview report'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'PIE must be running'
widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.SovAccessibilityPresentation, False)
assert len(widgets) == 1, 'Expected one actual native HUD presentation'
hud = widgets[0]
report = dict(status='previewing', scope='Synthetic HUD layout only; no story or gameplay qualification', stages=[])
state = dict(start=time.monotonic(), stage=0, handle=None)


def tick(_delta):
    elapsed = time.monotonic() - state['start']
    try:
        if state['stage'] == 0:
            hud.present_speech('Layout QA', 'Yes.', 3., unreal.Vector(), False)
            hud.present_caption('Short caption.', 3., unreal.Vector())
            report['stages'].append(dict(stage='short', elapsed=elapsed))
            state['stage'] = 1
        elif state['stage'] == 1 and elapsed > 4.:
            hud.present_speech('Layout QA', 'Keep moving toward the evacuation point and protect the wounded.', 20., unreal.Vector(), False)
            hud.present_caption('Movement beyond the doorway. Keep the evacuation route clear.', 20., unreal.Vector())
            report['stages'].append(dict(stage='long', elapsed=elapsed))
            state['stage'] = 2
        elif elapsed > 26.:
            report['status'] = 'preview_complete_requires_visual_review'
            unreal.unregister_slate_post_tick_callback(state['handle'])
    except Exception:
        report.update(status='failed', error=traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(state['handle'])
    out.write_text(json.dumps(report, indent=2), encoding='utf8')


state['handle'] = unreal.register_slate_post_tick_callback(tick)
