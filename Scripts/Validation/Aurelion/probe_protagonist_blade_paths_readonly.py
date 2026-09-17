"""Read-only: sample each protagonist attack's blade path to derive native melee trace geometry and windows.

For each attack montage this evaluates the character animation (AnimPose library, no world, no PIE),
places the weapon at the character's wield socket, and records where the blade edges are over time.
Blade edges on the weapon come from the weapon mesh evaluated in its ready pose. The output gives
tip speed per sample plus the montage's own attack-notify window, so authored windows can follow
the visible swing instead of a guess. Nothing is saved.
"""
import json
import os
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'protagonist-blade-paths.json'
HZ = 60.0
WIELD_SOCKET = 'weapon_r'
SPACE = unreal.AnimPoseSpaces.WORLD  # component space for an evaluated pose

HEROES = {
    'Tarrik': {
        'montages': ['/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_1_Tarrik',
                     '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_2_Tarrik',
                     '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_2_Variation_1_Tarrik',
                     '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_1_Variation_1_Tarrik'],
        'weapon_mesh': '/NarrativePro/Pro/Core/Weapons/Sword/SK_Narrative_Sword_1h',
        'weapon_pose': None,
    },
    'Selene': {
        'montages': ['/Game/Characters/Animation/VerityTwinBlades/AM_VerityTwin_01',
                     '/Game/Characters/Animation/VerityTwinBlades/AM_VerityTwin_02',
                     '/Game/Characters/Animation/VerityTwinBlades/AM_VerityTwin_03',
                     '/Game/Characters/Animation/VerityTwinBlades/AM_VerityTwin_04'],
        'weapon_mesh': '/Game/WeaponMeshes/Verity',
        'weapon_pose': ['/Game/WeaponMeshes/A_Verity_Draw', '/Game/WeaponMeshes/A_Verity_Rest', '/Game/WeaponMeshes/A_Verity_Stow'],
    },
}
report = {'read_only': True, 'hz': HZ, 'heroes': {}, 'errors': []}


def vec(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def options(mesh=None):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    o.set_editor_property('extract_root_motion', False)
    o.set_editor_property('incorporate_root_motion_into_pose', False)
    if mesh:
        o.set_editor_property('optional_skeletal_mesh', mesh)
    return o


def pose_at(anim, time, mesh=None):
    pose = unreal.AnimPose()
    return unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, time, options(mesh))


def notify_window(montage, class_fragment):
    windows = []
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        exported = event.export_text()
        if class_fragment in exported:
            values = [float(x) for x in __import__('re').findall(r'LinkValue=([0-9.eE+-]+)', exported)]
            duration = float(__import__('re').search(r'Duration=([0-9.eE+-]+)', exported).group(1))
            start = values[-1] if values else None
            windows.append({'start': start, 'duration': duration, 'end': None if start is None else start + duration})
    return windows


def weapon_points(hero, cfg):
    """Blade reference points in weapon-mesh component space, per candidate pose."""
    mesh = unreal.load_asset(cfg['weapon_mesh'])
    out = {'mesh': cfg['weapon_mesh'], 'bounds': str(mesh.get_bounds()), 'poses': {}}
    names = [str(n) for n in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)]
    out['bones'] = names
    anims = cfg['weapon_pose'] or []
    evaluations = [('ref', None, 0.0)]
    for path in anims:
        anim = unreal.load_asset(path)
        if anim:
            length = float(anim.get_play_length())
            evaluations += [(path.split('/')[-1] + '@0', anim, 0.0), (path.split('/')[-1] + '@end', anim, length)]
    for label, anim, t in evaluations:
        try:
            if anim is None:
                pose = unreal.AnimPose()
                pose = unreal.AnimPoseExtensions.get_reference_pose(mesh.skeleton)
            else:
                pose = pose_at(anim, t, mesh)
            row = {'bones': {}, 'sockets': {}}
            for bone in names:
                row['bones'][bone] = vec(unreal.AnimPoseExtensions.get_bone_pose(pose, bone, SPACE).translation)
            for socket in unreal.AnimPoseExtensions.get_socket_names(pose):
                row['sockets'][str(socket)] = vec(unreal.AnimPoseExtensions.get_socket_pose(pose, socket, SPACE).translation)
            out['poses'][label] = row
        except Exception as exc:
            out['poses'][label] = {'error': str(exc)}
    return out


def sample_montage(path, blade_local):
    """blade_local: {edge_name: (start_vec, end_vec)} in weapon space; weapon space == wield socket space."""
    montage = unreal.load_asset(path)
    row = {'montage': path, 'length': float(montage.get_play_length()),
           'attack_notify': notify_window(montage, 'ANS_AttackAnimation'),
           'warp_notify': notify_window(montage, 'MotionWarping'), 'samples': []}
    # Slot tracks are not script-visible; every notify records its linked sequence and segment span.
    import re
    linked = None
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        exported = event.export_text()
        found = re.findall(r"LinkedSequence=\"[^']*'([^']+)'\"", exported)
        begins = re.findall(r'SegmentBeginTime=([0-9.]+)', exported)
        lengths = re.findall(r'SegmentLength=([0-9.]+)', exported)
        if found:
            linked = (found[-1], float(begins[-1]), float(lengths[-1]))
            break
    sequence = unreal.load_asset(linked[0].split('.')[0])
    start = 0.0
    rate = float(sequence.get_play_length()) / linked[2] if linked[2] > 0 else 1.0
    row['segment_begin'], row['segment_length'] = linked[1], linked[2]
    row['sequence'], row['play_rate'], row['anim_start_time'] = sequence.get_path_name(), rate, start
    skeleton = sequence.get_editor_property('skeleton')
    preview = None
    for owner, name in ((sequence, 'get_preview_mesh'), (skeleton, 'get_preview_mesh')):
        try:
            preview = getattr(owner, name)() if owner else None
        except Exception:
            preview = None
        if preview:
            break
    if not preview and skeleton:
        try:
            soft = skeleton.get_editor_property('preview_skeletal_mesh')
            preview = unreal.SystemLibrary.load_asset_blocking(soft) if soft else None
        except Exception as exc:
            row['preview_error'] = str(exc)
    row['preview_mesh'] = preview.get_path_name() if preview else None
    probe = pose_at(sequence, 0.0, preview)
    row['pose_sockets'] = [str(x) for x in unreal.AnimPoseExtensions.get_socket_names(probe)]
    bones = [str(x) for x in unreal.AnimPoseExtensions.get_bone_names(probe)]
    row['wield_source'] = ('socket', WIELD_SOCKET) if WIELD_SOCKET in row['pose_sockets'] else         ('bone', WIELD_SOCKET) if WIELD_SOCKET in bones else ('bone', 'hand_r')
    steps = int(row['length'] * HZ) + 1
    previous = None
    for i in range(steps):
        t = i / HZ
        pose = pose_at(sequence, start + t * rate, preview)
        kind, name = row['wield_source']
        wield = unreal.AnimPoseExtensions.get_socket_pose(pose, name, SPACE) if kind == 'socket' else             unreal.AnimPoseExtensions.get_bone_pose(pose, name, SPACE)
        sample = {'t': round(t, 4), 'edges': {}}
        for edge, (a, b) in blade_local.items():
            pa = wield.transform_location(unreal.Vector(*a))
            pb = wield.transform_location(unreal.Vector(*b))
            sample['edges'][edge] = [vec(pa), vec(pb)]
        if previous:
            speeds = {}
            for edge in blade_local:
                tip_now = unreal.Vector(*sample['edges'][edge][1]); tip_then = unreal.Vector(*previous['edges'][edge][1])
                speeds[edge] = round((tip_now - tip_then).length() * HZ, 1)
            sample['tip_speed'] = speeds
        row['samples'].append(sample)
        previous = sample
    return row


try:
    for hero, cfg in HEROES.items():
        entry = {}
        try:
            entry['weapon'] = weapon_points(hero, cfg)
        except Exception as exc:
            entry['weapon_error'] = str(exc)
        report['heroes'][hero] = entry
    # Blade edges are chosen after reading the weapon section; sampled here from explicit env overrides
    # or the defaults below so a second pass can refine them without editing code.
    defaults = json.loads(os.environ.get('SOV_BLADE_EDGES', 'null') or 'null') or {
        'Tarrik': {'blade': [[0, 0, 15], [0, 0, 95]]},
        'Selene': {'upper': [[0, 0, 18], [0, 0, 114]], 'lower': [[0, 0, -32], [0, 0, -129]]},
    }
    for hero, cfg in HEROES.items():
        edges = {k: (v[0], v[1]) for k, v in defaults.get(hero, {}).items()}
        rows = []
        if edges:
            for path in cfg['montages']:
                try:
                    rows.append(sample_montage(path, edges))
                except Exception as exc:
                    rows.append({'montage': path, 'error': str(exc)})
        report['heroes'][hero]['attacks'] = rows
    report['status'] = 'completed_readonly'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=1), encoding='utf-8')
    unreal.log('PROTAGONIST_BLADE_PATHS ' + report['status'])
