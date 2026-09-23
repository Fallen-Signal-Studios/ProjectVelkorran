"""Read-only camera-channel collision census for the two protagonist proxies."""
import json
import os
from pathlib import Path
import unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'companion-camera-defaults.json'
rows=[]
for name in ('BP_AurelionTarrikCompanion','BP_AurelionSeleneCompanion'):
    path='/Game/Aurelion/Characters/'+name
    bp=unreal.load_asset(path)
    assert bp
    cdo=unreal.get_default_object(bp.generated_class())
    capsule=cdo.get_component_by_class(unreal.CapsuleComponent)
    assert capsule
    rows.append(dict(asset=path,component=capsule.get_path_name(),
        profile=str(capsule.get_collision_profile_name()),
        camera=str(capsule.get_collision_response_to_channel(unreal.CollisionChannel.cast(4))),
        pawn=str(capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)),
        visibility=str(capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY))))
out.write_text(json.dumps(rows,indent=2),encoding='utf-8')
