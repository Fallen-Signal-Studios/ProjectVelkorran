"""Fresh-load validation of prepared recoil assets, without enabling gameplay cues."""
from pathlib import Path
import json
import os
import re
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
folder = '/Game/Aurelion/Enemies/Animation/'
rows = []
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
for role in ('Linkbound', 'WallRunner', 'Weaver', 'Elite'):
    clip = unreal.load_asset(folder + 'AS_Eclipse' + role + '_HitReaction')
    montage = unreal.load_asset(folder + 'AM_Eclipse' + role + '_HitReaction')
    anim_set = unreal.load_asset(folder + 'DA_Eclipse' + role + '_HitReaction')
    bp = unreal.load_asset(folder + 'ABP_Eclipse' + role)
    assert clip and montage and anim_set and bp
    assert clip.get_editor_property('skeleton') == montage.get_editor_property('skeleton') == bp.get_editor_property('target_skeleton')
    assert not clip.get_editor_property('enable_root_motion')
    assert clip.get_editor_property('force_root_lock')
    assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
    assert not unreal.AnimationLibrary.get_animation_notify_events(montage)
    pairs = anim_set.get_editor_property('character_anims')
    assert len(pairs) == 1 and pairs[0].get_editor_property('Montage3P') == montage
    assert .3 < montage.get_play_length() < 2.
    export = unreal.AssetExportTask()
    export.object = montage
    export.exporter = unreal.ObjectExporterT3D()
    export.filename = str(out / (montage.get_name() + '.t3d'))
    export.automated = True
    export.prompt = False
    assert unreal.Exporter.run_asset_export_task(export)
    raw = Path(export.filename).read_bytes()
    text = raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
    # SlotAnimTracks is not Python-exposed. T3D omits the constructor's DefaultSlot value.
    tracks = [line for line in text.splitlines() if line.strip().startswith('SlotAnimTracks(')]
    assert len(tracks) == 1 and tracks[0].strip().startswith('SlotAnimTracks(0)=')
    slot = re.search(r'SlotName="([^"]+)"', tracks[0])
    assert slot is None or slot.group(1) == 'DefaultSlot'
    assert clip.get_path_name() in text
    rows.append(dict(role=role, duration=montage.get_play_length(), montage=montage.get_path_name()))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'hit-reaction-readback.json').write_text(json.dumps(dict(
    status='PASS', roles=rows, qualification='Fresh-load asset integrity only; no gameplay trigger or visible reaction acceptance.'
), indent=2))
