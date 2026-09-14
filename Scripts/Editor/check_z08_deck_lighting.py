"""Saved deck illumination contract; no changes on import."""
import json
from pathlib import Path
import unreal

def check_z08_deck_lighting(actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08VaultKit/deck-lighting-fit.json').read_text(encoding='utf-8-sig'))
    labels={a.get_actor_label():a for a in actors};rows=[]
    for row in fit['lights']:
        a=labels[row['actor']];c=a.get_component_by_class(unreal.RectLightComponent);p=a.get_actor_location()
        assert (p-unreal.Vector(row['position'][0],row['position'][1],fit['height'])).length()<.001
        assert (c.get_forward_vector()-unreal.Vector(0,0,-1)).length()<.0001
        assert a.get_actor_scale3d()==unreal.Vector(1,1,1)
        for key,value in row['properties'].items():
            if key in ('intensity','attenuation_radius'):assert c.get_editor_property(key)==fit[key]
            else:assert str(c.get_editor_property(key))==value,(row['actor'],key)
        color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['linear_color']))<.0001
        rows.append(dict(actor=row['actor'],lumens=c.intensity,height_cm=p.z))
    assert len(actors)==3140
    return dict(lights=rows,qualification='Fixed-camera readability and saved settings; live exposure, fixture art and packaged performance remain unqualified.')
