"""Only standalone graybox annotations are hidden in game."""
from pathlib import Path
import json,unreal

def check_z08_technical_labels(actors):
    labels={a.get_actor_label():a for a in actors};fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08Presentation/technical-labels.json').read_text())
    for row in fit['labels']:
        a=labels[row['actor']];assert a.get_class()==unreal.TextRenderActor.static_class()
        c=a.get_component_by_class(unreal.TextRenderComponent)
        assert c.get_path_name()==row['component'] and str(c.text)==row['text']
        assert (c.get_world_location()-unreal.Vector(*row['location'])).length()<.001
        assert c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    for name in fit['preserved']:
        c=labels[name].get_component_by_class(unreal.TextRenderComponent)
        assert c and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    return dict(hidden_annotation_count=len(fit['labels']),retained_gameplay_and_wayfinding=fit['preserved'],qualification='Saved visibility on standalone annotations; native guidance retained, live gameplay visibility still requires review.')
