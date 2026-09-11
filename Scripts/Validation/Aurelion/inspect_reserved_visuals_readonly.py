"""Call inspect(world, output_path) in an existing real Aurelion PIE; no mutations."""
import json
from pathlib import Path
import unreal

def inspect(world, output_path):
    assert world and '/Aurelion/Maps/UEDPIE_' in world.get_path_name()
    report={'read_only':True,'world':world.get_path_name(),'participants':[],
            'inspection_errors':[],'rendered_validation':False}
    def path(obj):
        return obj.get_path_name() if unreal.SystemLibrary.is_valid(obj) else None
    def actor_data(actor):
        row={'path':path(actor)}
        if not row['path']: return row
        row.update(class_path=path(actor.get_class()),hidden=bool(actor.get_editor_property('hidden')),
                   collision=actor.get_actor_enable_collision(),owner=path(actor.get_owner()),
                   attachment_parent=path(actor.get_attach_parent_actor()),meshes=[])
        for mesh in actor.get_components_by_class(unreal.MeshComponent):
            row['meshes'].append({'path':path(mesh),'visible':mesh.is_visible(),
                'hidden_in_game':bool(mesh.get_editor_property('hidden_in_game')),
                'collision':str(mesh.get_collision_enabled())})
        return row
    directors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovEncounterDirector)
    for director in directors:
        for entry in director.get_editor_property('participants'):
            row={'director':str(director.get_editor_property('encounter_id')),
                 'state':str(director.get_encounter_state()),
                 'participant_id':str(entry.get_editor_property('participant_id'))}
            try:
                actor=entry.get_editor_property('character')
                row['character']=actor_data(actor)
                if unreal.SystemLibrary.is_valid(actor):
                    row['alive']=actor.is_alive()
                    visual=actor.get_character_visual()
                    row['visual']=actor_data(visual)
                    row['attached']=[actor_data(child) for child in actor.get_attached_actors()]
                    row['visual_attachments']=[actor_data(child) for child in visual.get_attached_actors()] if unreal.SystemLibrary.is_valid(visual) else []
                    row['hidden_character_has_exposed_visual']=bool(row['character']['hidden'] and row['visual'].get('path') and not row['visual'].get('hidden'))
            except Exception as exc:
                row['error']=str(exc);report['inspection_errors'].append(row['participant_id']+': '+str(exc))
            report['participants'].append(row)
    report['status']='completed_readonly' if not report['inspection_errors'] else 'inspection_failed'
    report['exposed_hidden_characters']=[r['participant_id'] for r in report['participants'] if r.get('hidden_character_has_exposed_visual')]
    Path(output_path).write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report
