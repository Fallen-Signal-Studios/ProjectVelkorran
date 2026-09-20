"""Fresh-process verification of scoped campaign quality assets; no writes."""
import json
import os
from pathlib import Path
import unreal

report = dict(status='running', enemies=[])
for role in ('Enforcer', 'Linkbound', 'SecurityDrone', 'ContaminatedDrone',
             'WallRunner', 'Weaver', 'Elite'):
    bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion' + role)
    cdo = unreal.get_default_object(bp.generated_class())
    components = [c for c in cdo.get_components_by_class(unreal.NarrativeInteractableComponent)
                  if c.get_name() == 'NPCInteractable']
    assert len(components) == 1
    priority = components[0].get_editor_property('interaction_priority')
    assert priority == -2, (role, priority)
    report['enemies'].append(dict(role=role, priority=priority))
material = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ReceiverStatus')
assert material.get_editor_property('used_with_nanite')
report.update(status='passed', receiver_nanite_usage=True,
              qualification='Saved content readback; gameplay acceptance is recorded separately.')
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'campaign-quality-readback.json').write_text(json.dumps(report, indent=2))
