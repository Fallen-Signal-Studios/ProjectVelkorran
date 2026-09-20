"""Read back the six owned shoulder rigs; never mutate or save assets.

This verifies authored content only. It does not prove director binding, input,
left/right gameplay, collision, or protagonist presentation.
"""
import json
import os
import re
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'ShoulderRigReadback'
out.mkdir(exist_ok=True)

def export(path, name):
    asset = unreal.load_asset(path)
    assert asset, path
    filename = out / (name + '.t3d')
    task = unreal.AssetExportTask()
    for key, value in dict(object=asset, exporter=unreal.ObjectExporterT3D(),
                          filename=str(filename), automated=True, prompt=False,
                          replace_identical=True).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task), path
    raw = filename.read_bytes()
    return raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')

def values(text, name):
    match = re.search(r'\b' + name + r'=\(Value=\((X=[^)]+)\)', text)
    assert match, name
    return match.group(1)

def scalar(text, name):
    match = re.search(r'\b' + name + r'=\(Value=([^,)]+)', text)
    assert match, name
    return match.group(1)

rows = []
for style in ('Far', 'Balanced', 'Close'):
    for mode in ('Aim', 'Strafe'):
        name = f'CR_Sov_{style}_{mode}'
        own = export('/Game/Aurelion/Camera/Rigs/' + name, name)
        vendor = export(f'/NarrativePro/Pro/Core/Character/Biped/Camera/{style}/'
                        f'CameraAsset_SandboxCharacter_{style}_{mode}', 'Source_' + name)
        assert 'InterfaceParameterName="ShoulderOffset"' in own, name
        assert 'TargetPropertyName="Offset"' in own, name
        assert 'ParameterName="ShoulderOffset"' in own, name
        assert values(own, 'ShoulderOffset') == values(vendor, 'Offset'), name
        assert values(own, 'Offset') == values(vendor, 'Offset'), name
        assert values(own, 'CrouchOffset') == values(vendor, 'CrouchOffset'), name
        for field in ('ForwardDampingFactor', 'LateralDampingFactor', 'VerticalDampingFactor'):
            assert scalar(own, field) == scalar(vendor, field), (name, field)
        for field in ('BlendType', 'BlendTime', 'EnterTransitions', 'ExitTransitions'):
            # Transition object identities vary, but count/type/duration must agree.
            source_lines = re.findall(r'^\s*' + field + r'(?:\([^)]*\))?=(.*)$', vendor, re.M)
            own_lines = re.findall(r'^\s*' + field + r'(?:\([^)]*\))?=(.*)$', own, re.M)
            assert len(source_lines) == len(own_lines), (name, field)
            if field in ('BlendType', 'BlendTime'):
                assert source_lines == own_lines, (name, field)
        if style == 'Balanced' and mode == 'Strafe':
            assert 'InterfaceParameterName="CrouchOffset"' in own
            assert 'ParameterName="CrouchOffset"' in own
        definition = re.search(r'ParameterName="ShoulderOffset"[^\n]*VariableID=\(Value=(\d+)\)', own)
        assert definition and definition.group(1) != '4294967295', name
        offset_binding = re.search(r'\bOffset=\(Value=\(X=[^)]+\),VariableID=\(Value=(\d+)\)', own)
        assert offset_binding and offset_binding.group(1) == definition.group(1), name
        rows.append(dict(asset=name, offset=values(own, 'ShoulderOffset'),
                         preserved_source_framing=True, public_parameter_built=True))
(out / 'shoulder-rigs.json').write_text(json.dumps(dict(
    status='passed_authored_content_only', rigs=rows,
    runtime_verification="not_performed", scope=__doc__), indent=2))
unreal.log('SOV_SHOULDER_RIG_READBACK_PASSED')
