import json, os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
surface=next(w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport())
original=surface.get_holographic_hud_view()
view=surface.get_holographic_hud_view()
mid=surface.get_editor_property('AbilityPips').get_editor_property('brush').get_editor_property('resource_object')
states=unreal.SovHolographicHUDPipState
cases=[([states.ECHO_READY,states.ACTIVE,states.COOLDOWN,states.NEEDS_ECHO,states.INPUT_LOCKED,states.UNBOUND],[1,2,.2505,-1,-1,-1]),([states.WEAPON_REQUIRED,states.UNAVAILABLE],[-1]*6),([],[-1]*6)]
results=[]
try:
    for state_list,expected in cases:
        pips=[]
        for state in state_list:
            pip=unreal.SovHolographicHUDPip();pip.set_editor_property('State',state);pip.set_editor_property('CooldownFraction',.5);pips.append(pip)
        view.set_editor_property('Pips',pips)
        surface.set_editor_property('View',view)
        surface.call_method('OnHolographicHUDUpdated',(view,))
        actual=[mid.get_scalar_parameter_value('Pip'+str(i)) for i in range(6)]
        assert all(abs(a-e)<.001 for a,e in zip(actual,expected)),(actual,expected)
        results.append(dict(states=[str(s) for s in state_list],actual=actual))
    (out/'pips-controlled-states.json').write_text(json.dumps(dict(status='passed',scope='Transient presentation-only state injection; not gameplay activation coverage',cases=results),indent=2))
finally:
    surface.set_editor_property('View',original)
    surface.call_method('OnHolographicHUDUpdated',(original,))

