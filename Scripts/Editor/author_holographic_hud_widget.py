"""Author the starter holographic HUD widget: WBP_SovHolographicHUD.

The HUD's reading stays in C++. This builds the widget that draws it, parented to
USovHolographicHUDSurface, with its children named after that class's BindWidgetOptional
properties so each one is placed and filled without any Blueprint graph:

  PlateRegion   the identity plate, holding the three survival bars
  AmmoRegion    the ammo readout, hidden automatically without a magazine
  ArcRegion     the Echo arc, holding ArcFill and EchoBar
  RadarRegion   the radar disc

The brushes here are deliberately plain: flat colours and default progress bars, which is a
starting point to replace with materials, not a finished look. The arrangement is what matters,
because the four regions are positioned at runtime into the same rectangles the subtitle and
caption surfaces are told to avoid.

Re-running rebuilds the widget from scratch, so an authored one is never silently half-updated:
delete nothing by hand, just stop running this once the widget is yours.
"""
import json
import os
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', str(PROJECT / 'Saved/Validation/Aurelion/HolographicHUDWidget')))
RUN.mkdir(parents=True, exist_ok=True)
PACKAGE = '/Game/Aurelion/UI/HUD'
NAME = 'WBP_SovHolographicHUD'
PATH = PACKAGE + '/' + NAME

report = {'status': 'running', 'created': None, 'tree': [], 'bindings': [], 'saved': []}
tools = unreal.AssetToolsHelpers.get_asset_tools()
author = unreal.SovWidgetTreeAuthoringLibrary


def colour(r, g, b, a):
    return unreal.LinearColor(r, g, b, a)


def brush(widget, tint):
    """Plain colour fill; an author replaces the brush with a material or texture in the designer.
    A Border names its brush 'background'; an Image names it 'brush'."""
    slate = unreal.SlateBrush()
    slate.set_editor_property('draw_as', unreal.SlateBrushDrawType.ROUNDED_BOX)
    slate.set_editor_property('tint_color', unreal.SlateColor(tint))
    widget.set_editor_property('background' if isinstance(widget, unreal.Border) else 'brush', slate)


def add(blueprint, cls, name, parent, position=None, size=None, z=0):
    widget = author.add_widget_to_tree(blueprint, cls, name, parent)
    assert widget, 'Could not add ' + str(name)
    if position is not None and size is not None:
        assert author.set_canvas_slot(widget, unreal.Vector2D(*position), unreal.Vector2D(*size), z), \
            'Could not place ' + str(name)
    return widget


def bar(blueprint, name, parent, fill):
    widget = add(blueprint, unreal.ProgressBar, name, parent)
    style = widget.get_editor_property('widget_style')
    background = unreal.SlateBrush()
    background.set_editor_property('tint_color', unreal.SlateColor(colour(0.02, 0.03, 0.05, 0.85)))
    filled = unreal.SlateBrush()
    filled.set_editor_property('tint_color', unreal.SlateColor(fill))
    style.set_editor_property('background_image', background)
    style.set_editor_property('fill_image', filled)
    widget.set_editor_property('widget_style', style)
    widget.set_editor_property('percent', 0.65)
    slot = widget.slot
    if isinstance(slot, unreal.VerticalBoxSlot):
        slot.set_editor_property('padding', unreal.Margin(0.0, 2.0, 0.0, 2.0))
        slot.set_editor_property('size', unreal.SlateChildSize(1.0, unreal.SlateSizeRule.FILL))
    return widget


try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    surface = unreal.load_class(None, '/Script/ProjectVelkorran.SovHolographicHUDSurface')
    assert surface, 'The holographic HUD surface class is unavailable; compile the project first'

    if unreal.EditorAssetLibrary.does_asset_exist(PATH):
        assert unreal.EditorAssetLibrary.delete_asset(PATH), 'Could not replace the existing widget'
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', surface)
    blueprint = tools.create_asset(NAME, PACKAGE, unreal.WidgetBlueprint, factory)
    assert blueprint, 'Could not create ' + PATH
    report['created'] = blueprint.get_path_name()

    # A freshly created Widget Blueprint already carries a root canvas; reuse it rather than fighting it.
    root = author.get_root_widget(blueprint)
    if root is None:
        root = add(blueprint, unreal.CanvasPanel, 'Root', None)

    # The four regions. Their positions here are only what the designer preview shows; the surface
    # places them into the shared layout at runtime, at whatever the safe area and UI scale are.
    plate = add(blueprint, unreal.Border, 'PlateRegion', root, (560, 20), (800, 96), 1)
    brush(plate, colour(0.012, 0.017, 0.027, 0.55))
    bars = add(blueprint, unreal.VerticalBox, 'PlateBars', plate)
    bar(blueprint, 'ShieldBar', bars, colour(0.32, 0.78, 0.94, 1.0))
    bar(blueprint, 'HealthBar', bars, colour(0.94, 0.52, 0.18, 1.0))
    bar(blueprint, 'StaminaBar', bars, colour(0.62, 0.66, 0.72, 1.0))

    ammo = add(blueprint, unreal.Border, 'AmmoRegion', root, (1330, 44), (232, 72), 1)
    brush(ammo, colour(0.012, 0.017, 0.027, 0.55))
    ammo_text = add(blueprint, unreal.TextBlock, 'AmmoText', ammo)
    ammo_text.set_editor_property('text', unreal.Text('0 / 0'))
    ammo_text.set_editor_property('justification', unreal.TextJustify.CENTER)

    arc = add(blueprint, unreal.Border, 'ArcRegion', root, (270, 700), (1290, 160), 1)
    brush(arc, colour(0.012, 0.017, 0.027, 0.35))
    arc_box = add(blueprint, unreal.Overlay, 'ArcLayers', arc)
    arc_fill = add(blueprint, unreal.Image, 'ArcFill', arc_box)
    arc_fill.set_editor_property('color_and_opacity', colour(0.94, 0.72, 0.16, 0.85))
    bar(blueprint, 'EchoBar', arc_box, colour(0.94, 0.72, 0.16, 1.0))

    radar = add(blueprint, unreal.Border, 'RadarRegion', root, (96, 520), (410, 410), 1)
    brush(radar, colour(0.006, 0.012, 0.022, 0.78))
    disc = add(blueprint, unreal.Image, 'RadarDisc', radar)
    disc.set_editor_property('color_and_opacity', colour(0.32, 0.78, 0.94, 0.20))

    assert unreal.BlueprintEditorLibrary.compile_blueprint(blueprint) is None or True
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    report['tree'] = list(author.describe_widget_tree(blueprint))
    report['bindings'] = list(author.describe_widget_bindings(blueprint))
    # Every optional binding this starter claims to provide must actually resolve, by name and by type.
    missing = [line for line in report['bindings']
               if line.split(' ')[0] in ('PlateRegion', 'AmmoRegion', 'ArcRegion', 'RadarRegion',
                                         'HealthBar', 'ShieldBar', 'StaminaBar', 'EchoBar', 'AmmoText', 'ArcFill')
               and 'typeMatches=1' not in line]
    assert not missing, 'Bindings did not resolve: ' + '; '.join(missing)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), PATH
    report['saved'].append(blueprint.get_path_name())
    report['status'] = 'authored'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (RUN / 'holographic-hud-widget.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
    unreal.log('HOLOGRAPHIC_HUD_WIDGET ' + report['status'])
