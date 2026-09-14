"""Measured placements and retained collision for the Z02 vault assembly."""
import json
from pathlib import Path
import unreal

def placements(meshes,fit):
    x,y,z=fit['world_origin_cm']; outer=fit['outer_x_m']; inner=fit['inner_x_m']
    rows=[]
    for side,sign in (('West',-1),('East',1)):
        for i in range(4):
            rows.append((f'{side}_Bay_{i}', 'Z02SideVault_2m', (x+sign*(outer-1-i*2)*100,y,z)))
        rows.append((f'{side}_Edge','Z02SideVault_Edge',(x+sign*(inner+(outer-inner-8)/2)*100,y,z)))
        b=meshes['Z02ArchFace'].get_bounds()
        front=b.origin.x-sign*b.box_extent.x
        rows.append((f'{side}_Face','Z02ArchFace',(x+sign*(inner*100-1)-front,y,z)))
    rows.append(('CentralCeiling','Z02CentralCeiling',(x,y,955)))
    return rows

def check_vault(world,actors):
    root=Path(unreal.Paths.project_dir()); source=root/'Art/Source/Aurelion/Z02VaultKit'
    fit=json.loads((source/'measured-profile.json').read_text()); labels={a.get_actor_label():a for a in actors}
    specs=json.loads((source/'manifest.json').read_text())['modules']
    meshes={s['asset'].removeprefix('SM_Aurelion_KIT_'):unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+s['asset']) for s in specs}
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem); checked=[]
    for name,suffix,pos in placements(meshes,fit):
        a=labels['KIT_Z02_Vault_'+name]; c=a.static_mesh_component; p=a.get_actor_location(); scale=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z)) and abs(a.get_actor_rotation().yaw)<.01
        assert c.static_mesh==meshes[suffix] and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        assert sm.get_simple_collision_count(c.static_mesh)==sm.get_convex_collision_count(c.static_mesh)==0
        checked.append(a.get_actor_label())
    for name in ('Cube_2','Cube_4','Cube_5'):
        c=labels[name].static_mesh_component
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    for baseline in fit['retained_shell_baseline']:
        a=labels[baseline['actor']]
        assert a.get_actor_transform().export_text()==baseline['transform']
        assert a.static_mesh_component.static_mesh.get_path_name()==baseline['mesh']
    return dict(placements=checked,qualification='Authoring placement and collision preservation checks only; live movement, lighting and performance remain unqualified.')
