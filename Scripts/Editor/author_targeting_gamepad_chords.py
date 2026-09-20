"""Give the targeting actions a gamepad layer held under the left shoulder (handoff task 5).

Every gamepad input in IMC_Combat was already bound, so targeting gets a chord layer rather than a
button: hold the left shoulder and the four targeting actions appear on buttons that already do
something else. Each chorded mapping carries an InputTriggerChordAction keyed to IA_WeaponWheel, and
each displaced base mapping gets an InputTriggerChordBlocker so it does NOT also fire while the
shoulder is held. Without the blocker both actions run and the chord is not a layer, it is a
double-fire.

IA_SkipCinematic is deliberately left unbound here. It fires during cinematics, where a chord built
on a combat-context weapon wheel is the wrong home for it.

UE 5.7 note: UInputMappingContext::Mappings is deprecated and reads empty. The live rows are in
DefaultKeyMappings.Mappings, and a script that writes the deprecated array reports success and
changes nothing.
"""
import json
import os
import shutil
from pathlib import Path
import unreal

CONTEXT_PATH = "/Game/Input/IMC_Combat"
CHORD_ACTION = "IA_WeaponWheel"

# action -> key that hosts it while the shoulder is held
CHORDS = {
    "IA_ThreatFocus": "Gamepad_FaceButton_Top",
    "IA_Designate": "Gamepad_FaceButton_Left",
    "IA_CycleTargetLeft": "Gamepad_DPad_Left",
    "IA_CycleTargetRight": "Gamepad_DPad_Right",
}


def fail(message):
    unreal.log_error("CHORD AUTHORING FAILED: %s" % message)
    raise SystemExit(1)


context = unreal.EditorAssetLibrary.load_asset(CONTEXT_PATH)
if not context:
    fail("missing %s" % CONTEXT_PATH)

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'TargetingChordAuthoring'
out.mkdir(exist_ok=False)
shutil.copy2(Path(unreal.Paths.project_dir()) / 'Content/Input/IMC_Combat.uasset', out / 'IMC_Combat.uasset')

def make_trigger(cls, action, name):
    # EditInstanceOnly rejects mutation after a trigger belongs to an asset.
    # Configure a transient instance, then move it into the owned context.
    trigger = unreal.new_object(cls)
    trigger.set_editor_property('chord_action', action)
    assert trigger.rename(name, context)
    assert trigger.get_editor_property('chord_action') == action
    return trigger

default_mappings = context.get_editor_property("default_key_mappings")
if not default_mappings:
    fail("the context has no DefaultKeyMappings; this build may predate the 5.7 layout")
rows = list(default_mappings.get_editor_property("mappings") or [])
if not rows:
    fail("DefaultKeyMappings.Mappings is empty, which is what the deprecated array looks like")
unreal.log("Live mappings before: %d" % len(rows))


def key_name(row):
    key = row.get_editor_property("key")
    return str(key.get_editor_property("key_name")) if key else ""


def action_name(row):
    action = row.get_editor_property("action")
    return action.get_name() if action else ""


def find_key(name):
    """Reuse an FKey from a row that already uses it. Python exposes no way to build one from a name,
    and every key we chord onto is by definition already bound to something."""
    for row in rows:
        if key_name(row) == name:
            return row.get_editor_property("key")
    return None


def find_action(name):
    for row in rows:
        if action_name(row) == name:
            return row.get_editor_property("action")
    return None


chord_source = find_action(CHORD_ACTION)
if not chord_source:
    fail("%s is not mapped in this context, so nothing can chord against it" % CHORD_ACTION)

# Refuse to run twice: a second pass would stack duplicate triggers rather than replace them.
for row in rows:
    for trigger in (row.get_editor_property("triggers") or []):
        if isinstance(trigger, unreal.InputTriggerChordAction):
            fail("this context already contains chord triggers; nothing was changed")

new_rows = []
blocked_keys = set(CHORDS.values())

for action_asset_name, host_key in CHORDS.items():
    action = find_action(action_asset_name)
    if not action:
        fail("%s is not mapped in this context; bind it to a key first" % action_asset_name)
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    host = find_key(host_key)
    if not host:
        fail("%s is not bound to anything in this context, so its FKey cannot be reused" % host_key)
    mapping.set_editor_property("key", host)
    chord = make_trigger(unreal.InputTriggerChordAction, chord_source, 'TargetingChord_' + action_asset_name)
    mapping.set_editor_property("triggers", [chord])
    new_rows.append(mapping)
    unreal.log("  chord: hold %s + %s -> %s" % (CHORD_ACTION, host_key, action_asset_name))

# Stop the displaced base actions firing underneath the chord.
blocked = 0
for row in rows:
    if key_name(row) not in blocked_keys or action_name(row) in CHORDS:
        continue
    triggers = list(row.get_editor_property("triggers") or [])
    # The handoff requires these base actions blocked throughout the LB hold,
    # including before a target button triggers. Engine-generated blockers only
    # mask a lower mapping once its matching chorded action triggers.
    triggers.append(make_trigger(unreal.InputTriggerChordBlocker, chord_source, 'TargetingBlock_' + action_name(row)))
    row.set_editor_property("triggers", triggers)
    blocked += 1
    unreal.log("  blocked while held: %s on %s" % (action_name(row), key_name(row)))

assert blocked == 5, 'Unexpected displaced base actions; refusing to save'
default_mappings.set_editor_property("mappings", new_rows + rows)
if not unreal.EditorAssetLibrary.save_asset(CONTEXT_PATH, only_if_is_dirty=False):
    fail("the context could not be saved")

# This is an in-memory post-save readback. A separate fresh editor must verify
# serialization and trigger references before this increment is qualified.
reloaded = unreal.EditorAssetLibrary.load_asset(CONTEXT_PATH)
after = list(reloaded.get_editor_property("default_key_mappings").get_editor_property("mappings") or [])
chorded = [r for r in after
           if any(type(t) == unreal.InputTriggerChordAction for t in (r.get_editor_property("triggers") or []))]
blockers = [r for r in after
            if any(isinstance(t, unreal.InputTriggerChordBlocker) for t in (r.get_editor_property("triggers") or []))]
unreal.log("")
unreal.log("Live mappings after: %d (was %d)" % (len(after), len(rows)))
unreal.log("Chorded mappings on disk: %d (expected %d)" % (len(chorded), len(CHORDS)))
unreal.log("Blocked base mappings on disk: %d (expected %d)" % (len(blockers), blocked))
if len(chorded) != len(CHORDS) or len(blockers) != blocked:
    fail("the readback does not match what was written")

assert all(t.get_editor_property('chord_action') == chord_source for r in chorded + blockers
           for t in r.triggers if isinstance(t, unreal.InputTriggerChordAction))
(out / 'authoring.json').write_text(json.dumps(dict(status='saved_requires_fresh_runtime_review',
    mappings_before=len(rows), mappings_after=len(after), chorded=len(chorded), blocked=len(blockers),
    qualification='In-memory readback after scoped save; fresh reload and actual gamepad input remain required.'), indent=2))

unreal.log("CHORD AUTHORING COMPLETE")
