"""Read-only authored ability, mesh, and encounter-light inventory."""
import json
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion/EclipseAnimation'
report={'roles':{},'lights':[]}
for role in ('Linkbound','WallRunner','Weaver','Elite'):
    definition=unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion'+role)
    config=definition.get_editor_property('ability_configuration')
    grants=list(config.get_editor_property('default_abilities'))
    report['roles'][role]={'config':config.get_path_name(),
        'grants':[g.get_path_name() for g in grants]}
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,unreal.Light):
        comp=actor.get_component_by_class(unreal.LightComponent)
        report['lights'].append({'label':actor.get_actor_label(),'class':actor.get_class().get_name(),
            'location':actor.get_actor_location().export_text(),'intensity':comp.get_editor_property('intensity')})
out.mkdir(parents=True,exist_ok=True)
(out/'combat-content.json').write_text(json.dumps(report,indent=2),encoding='utf8')
