"""Read-only comparison of the player and companion presentation defaults."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
rows = []
for hero in ('Selene', 'Tarrik'):
    for role, path in [('player', '/Game/PlayerCharacters/BP_Sov' + hero),
                       ('companion', '/Game/Aurelion/Characters/BP_Aurelion' + hero + 'Companion')]:
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        row = dict(hero=hero, role=role, blueprint=path, meshes=[])
        for mesh in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
            data = dict(name=mesh.get_name())
            for prop in ('relative_location', 'relative_rotation', 'relative_scale3d', 'anim_class',
                         'skeletal_mesh_asset', 'visibility_based_anim_tick_option'):
                try:
                    value = mesh.get_editor_property(prop)
                    data[prop] = value.export_text() if hasattr(value, 'export_text') else str(value)
                except Exception as error:
                    data[prop] = dict(unavailable=str(error))
            row['meshes'].append(data)
        definition = unreal.load_asset('/Game/Characters/Definitions/PD_' + hero if role == 'player'
                                       else '/Game/Aurelion/Characters/NPC_Aurelion' + hero + 'Companion')
        appearance = definition.get_editor_property('default_appearance')
        row['appearance'] = appearance.get_path_name() if appearance else None
        rows.append(row)
(out / 'companion-mesh-defaults.json').write_text(json.dumps(rows, indent=2))
