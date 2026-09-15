"""Saved fill settings and preservation of both scanner and original room lights."""
from pathlib import Path
import json,unreal
def check_z03_ceiling_light(actors):
    root=Path(unreal.Paths.project_dir());baseline=json.loads((root/'Art/Source/Aurelion/Z03CeilingKit/light-baseline.json').read_text())['lights'];labels={a.get_actor_label():a for a in actors};preserved=[]
    for row in baseline:
        a=labels[row['actor']];c=next(c for c in a.get_components_by_class(unreal.LightComponent) if c.get_path_name()==row['component'])
        assert a.get_path_name()==row['path'] and c.get_world_transform().export_text()==row['transform']
        for key,value in row['properties'].items():assert str(c.get_editor_property(key))==value,(row['actor'],key)
        color=c.get_light_color();assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b,color.a),row['color']))<1e-6
        preserved.append(c.get_path_name())
    fills=[]
    for i,y in enumerate((-18840,-17400,-15960)):
        a=labels['ENVL_Z03_Key_'+str(i).zfill(2)];matches=[c for c in a.get_components_by_class(unreal.RectLightComponent) if c.get_name()=='CofferWash'];assert len(matches)==1;c=matches[0]
        assert (c.get_world_location()-unreal.Vector(7000,y,300)).length()<.001 and (c.get_forward_vector()-unreal.Vector(0,0,1)).length()<.001
        assert (c.get_world_scale()-unreal.Vector(1,1,1)).length()<.001 and c.get_editor_property('visible') and c.mobility==unreal.ComponentMobility.MOVABLE
        assert c.intensity==300 and c.intensity_units==unreal.LightUnits.LUMENS and c.attenuation_radius==2500 and c.source_width==600 and c.source_height==600
        assert c.cast_shadows and c.volumetric_scattering_intensity==0
        # Reviewed SetLightColor(..., False) uses QuantizeRound; GetLightColor
        # subsequently converts those stored sRGB bytes back to linear color.
        stored=c.get_editor_property('light_color');assert (stored.r,stored.g,stored.b,stored.a)==(226,233,255,255)
        color=c.get_light_color();expected=[((v/255+.055)/1.055)**2.4 for v in (226,233,255)]
        assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),expected))<1e-5
        fills.append(dict(component=c.get_path_name(),transform=c.get_world_transform().export_text(),lumens=c.intensity))
    assert len(actors)==3140
    return dict(fills=fills,preserved_original_lights=preserved,qualification='Fixed-camera readability and saved settings; live scanner readability, fixture art and packaged performance remain unqualified.')
