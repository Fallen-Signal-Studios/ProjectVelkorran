"""Bind the Elite Core to the actual torso physics body; preserve all consequences."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
bp=unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionElite')
core=unreal.get_default_object(bp.generated_class()).get_core_weak_points()
zones=list(core.get_editor_property('weak_point_zones'))
assert len(zones)==1 and str(zones[0].get_editor_property('zone_id'))=='Core'
before=zones[0].export_text()
# Verified in SK_Fat_PhysicsAsset's ObjectExporterT3D body inventory.
zones[0].set_editor_property('hit_bones',['CATRigSpine1'])
core.set_editor_property('weak_point_zones',zones)
assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion/EclipseAnimation/core-body.json'
out.write_text(json.dumps({'before':before,'after':zones[0].export_text(),
    'physics_body':'CATRigSpine1','live_hit_qualified':False},indent=2),encoding='utf8')
