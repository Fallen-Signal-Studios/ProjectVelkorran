import json, os, time
from pathlib import Path
import unreal
pip_probe_out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
pip_probe_samples=[]
pip_probe_next=0.0
def pip_probe_tick(delta):
    global pip_probe_next
    if time.monotonic()<pip_probe_next:return
    try:
        surfaces=[w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport()]
        assert len(surfaces)==1,len(surfaces)
        surface=surfaces[0]; view=surface.get_holographic_hud_view()
        mid=surface.get_editor_property('AbilityPips').get_editor_property('brush').get_editor_property('resource_object')
        assert isinstance(mid,unreal.MaterialInstanceDynamic),str(mid)
        values=[]
        for i in range(6):
            expected=-1.0; state='missing'
            if i<len(view.pips):
                pip=view.pips[i]; state=str(pip.state)
                if pip.state==unreal.SovHolographicHUDPipState.ECHO_READY:expected=1.0
                elif pip.state==unreal.SovHolographicHUDPipState.ACTIVE:expected=2.0
                elif pip.state==unreal.SovHolographicHUDPipState.COOLDOWN:expected=.001+.499*pip.cooldown_fraction
            actual=mid.get_scalar_parameter_value('Pip'+str(i))
            assert abs(actual-expected)<.001,(i,state,actual,expected)
            values.append(dict(slot=i,state=state,actual=actual,expected=expected))
        accent=mid.get_vector_parameter_value('Accent'); expected=view.palette.accent
        assert max(abs(getattr(accent,k)-getattr(expected,k)) for k in 'rgba')<.001
        pip_probe_samples.append(dict(pips=values,accent=str(accent),protagonist=str(view.protagonist)))
        pip_probe_next=time.monotonic()+1
        if len(pip_probe_samples)==3:
            (pip_probe_out/'pips-live.json').write_text(json.dumps(dict(status='passed',samples=pip_probe_samples),indent=2))
            unreal.unregister_slate_post_tick_callback(pip_probe_handle)
            unreal.log('HUD_PIPS_LIVE_PASSED')
    except Exception as exc:
        unreal.unregister_slate_post_tick_callback(pip_probe_handle)
        (pip_probe_out/'pips-live.json').write_text(json.dumps(dict(status='failed',error=str(exc)),indent=2))
        raise
pip_probe_handle=unreal.register_slate_post_tick_callback(pip_probe_tick)
