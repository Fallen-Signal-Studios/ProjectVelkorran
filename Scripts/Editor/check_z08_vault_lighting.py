"""Exact retained light settings and selected inward vault wash."""
import json
from pathlib import Path
import unreal
def check_z08_vault_lighting(actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
    fit=json.loads((root/'Art/Source/Aurelion/Z08VaultKit/lighting-fit.json').read_text());rows=[]
    for row in fit['lights']:
        a=labels[row['actor']];c=a.get_component_by_class(unreal.RectLightComponent);p=a.get_actor_location();direction=c.get_forward_vector()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),row['position']))<.001
        expected=unreal.MathLibrary.get_forward_vector(unreal.Rotator(pitch=fit['pitch'],yaw=row['yaw']))
        assert (direction-expected).length()<.0001 and a.get_actor_scale3d()==unreal.Vector(1,1,1)
        assert c.get_editor_property('source_width')==fit['source_width'] and c.get_editor_property('source_height')==fit['source_height']
        for key,value in row['properties'].items():
            if key not in ('source_width','source_height'):assert str(c.get_editor_property(key))==value,(row['actor'],key)
        color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['linear_color']))<.0001
        rows.append(dict(actor=row['actor'],forward=[direction.x,direction.y,direction.z],lumens=c.intensity))
    assert len(actors)==3140
    return dict(lights=rows,source_size_cm=[fit['source_width'],fit['source_height']],qualification='Saved settings and geometry regressions; live exposure transitions and packaged performance remain unqualified.')
