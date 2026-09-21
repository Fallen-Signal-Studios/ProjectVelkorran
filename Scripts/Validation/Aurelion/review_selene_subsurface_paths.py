"""Unsaved CP9 render-path isolation; compare each override against the same baseline.

No material or campaign changes. Restores console settings on success and failure.
Screenshots require visual review; a process exit is not a gameplay or visual pass.
"""
from pathlib import Path
import runpy,time,unreal

steps=(('baseline',{}),
       ('scattering-disabled',{'r.SubsurfaceScattering':0}),
       ('megalights-disabled',{'r.MegaLights.Allowed':0}),
       ('full-resolution',{'r.SSS.HalfRes':0}),
       ('checkerboard-disabled',{'r.SSS.Checkerboard':0}),
       ('profile-cache-disabled',{'r.SSS.Burley.EnableProfileIdCache':0}),
       ('restored',{}))
keys=sorted({key for _,values in steps for key in values})

def set_values(world,values):
    for key,value in values.items():
        unreal.SystemLibrary.execute_console_command(world,f'{key} {value}')
        assert unreal.SystemLibrary.get_console_variable_int_value(key)==value,key

def hook(world,state,report,capture,end):
    try:
        if 'path_originals' not in state:
            state['path_originals']={key:unreal.SystemLibrary.get_console_variable_int_value(key) for key in keys}
            report['render_path_originals']=state['path_originals']
            report['mega_lights_project']=unreal.SystemLibrary.get_console_variable_int_value('r.MegaLights.EnableForProject')
            report['scene_color_format']=unreal.SystemLibrary.get_console_variable_int_value('r.SceneColorFormat')
            state.update(path_step=0,path_pending=False,at=time.monotonic())
        if time.monotonic()-state['at']<(3 if state['path_pending'] else 15):return
        if not state['path_pending']:
            name,values=steps[state['path_step']]
            capture(world,name)
            report['frames'][-1]['overrides']=values
            state.update(path_pending=True,at=time.monotonic());return
        set_values(world,state['path_originals'])
        state['path_step']+=1
        if state['path_step']==len(steps):
            report['render_settings_restored']=True;end();return
        set_values(world,steps[state['path_step']][1])
        state.update(path_pending=False,at=time.monotonic())
    except Exception:
        if state.get('path_originals'):set_values(world,state['path_originals'])
        raise

runpy.run_path(str(Path(__file__).with_name('review_selene_face_streaming.py')),
              init_globals={'FACE_REVIEW_SCOPE':__doc__,'FACE_REVIEW_HOOK':hook})
