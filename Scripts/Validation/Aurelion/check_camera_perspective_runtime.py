"""Controlled camera API checks on each naturally possessed protagonist.

Run alongside a normal route. Checks restore the preference synchronously and
never advance mission state. Synthetic claims are explicitly boundary tests,
not evidence of physical input, serialized checkpoint loading or focus gameplay.
"""
import json
import os
import re
import time
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'camera-perspective-checks.json'
assert not out.exists()
report = dict(status='waiting', scope=__doc__, protagonists={}, errors=[])
started = time.monotonic()
seen = False


def write():
    out.write_text(json.dumps(report, indent=2))


def check(pawn):
    component = pawn.get_component_by_class(unreal.SovCameraControlComponent)
    previous = pawn.get_editor_property('CameraStyle')
    styles = type(previous)
    rows = []
    claim = None

    def sample(label, expected, index, mode=0):
        state = component.get_camera_state()
        supplied = pawn.call_method('Get_CharacterPropertiesForCamera').export_text()
        tags = unreal.GameplayTagLibrary.get_owned_gameplay_tags(pawn).export_text()
        claims = list(component.describe_claims())
        rows.append(dict(label=label, resolved=state.export_text(), supplied=supplied,
                         tags=tags, claims=claims))
        assert state.style == expected, label
        assert re.search(r'CameraStyle_[^=]+=NewEnumerator'+str(index)+r'[,)]', supplied), label
        assert re.search(r'CameraMode_[^=]+=NewEnumerator'+str(mode)+r'[,)]', supplied), label
        first = expected == unreal.SovCameraStyle.FIRST_PERSON
        assert ('Camera.Perspective.FirstPerson' in tags) == first, (label, tags)
        assert ('Camera.Perspective.ThirdPerson' in tags) != first, (label, tags)
        assert sum('reason=ManualCameraPreference ' in str(c) for c in claims) == 1, claims

    result = dict(status='running', samples=rows)
    report['protagonists'][pawn.get_class().get_name()] = result
    try:
        for label, index in [('FAR', 0), ('BALANCED', 1), ('CLOSE', 2), ('FIRST_PERSON', 3)]:
            pawn.call_method('SetCameraMode', (getattr(styles, label),))
            sample(label, getattr(unreal.SovCameraStyle, label), index)
        for iteration in range(3):
            claim = component.request_camera(unreal.SovCameraRequest(
                priority=unreal.SovCameraPriority.AIM, style=unreal.SovCameraStyle.CLOSE,
                mode=unreal.SovCameraMode.STRAFE, reason='ControlledPerspectiveCheck'))
            sample('Aim overrides first-person preference '+str(iteration), unreal.SovCameraStyle.CLOSE, 2, 1)
            assert component.release_camera(claim)
            claim = None
            sample('Release restores first-person presentation '+str(iteration), unreal.SovCameraStyle.FIRST_PERSON, 3)
        result['status'] = 'passed'
    except Exception:
        result.update(status='failed', error=traceback.format_exc())
    finally:
        if claim is not None:
            component.release_camera(claim)
        pawn.call_method('SetCameraMode', (previous,))
        result['restored_preference'] = str(previous)
        write()


def tick(delta):
    global seen
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (seen and not world) or time.monotonic()-started > 1800:
            report['status'] = ('passed' if len(report['protagonists']) == 2 and
                all(r['status'] == 'passed' for r in report['protagonists'].values()) else 'incomplete_or_failed')
            unreal.unregister_slate_post_tick_callback(handle)
            write()
            return
        if not world:
            return
        seen = True
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        name = pawn.get_class().get_name()
        if name not in ('BP_SovTarrik_C', 'BP_SovSelene_C') or name in report['protagonists']:
            return
        component = pawn.get_component_by_class(unreal.SovCameraControlComponent)
        if component.get_camera_state().priority != unreal.SovCameraPriority.PROFILE:
            return
        check(pawn)
    except Exception:
        report['errors'].append(traceback.format_exc())
        report['status'] = 'failed'
        unreal.unregister_slate_post_tick_callback(handle)
        write()


write()
handle = unreal.register_slate_post_tick_callback(tick)
