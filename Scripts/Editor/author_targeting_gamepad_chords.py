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
    chord = unreal.new_object(unreal.InputTriggerChordAction, outer=context)
    chord.set_editor_property("chord_action", chord_source)
    mapping.set_editor_property("triggers", [chord])
    new_rows.append(mapping)
    unreal.log("  chord: hold %s + %s -> %s" % (CHORD_ACTION, host_key, action_asset_name))

# Stop the displaced base actions firing underneath the chord.
blocked = 0
for row in rows:
    if key_name(row) not in blocked_keys or action_name(row) in CHORDS:
        continue
    triggers = list(row.get_editor_property("triggers") or [])
    triggers.append(unreal.new_object(unreal.InputTriggerChordBlocker, outer=context))
    row.set_editor_property("triggers", triggers)
    blocked += 1
    unreal.log("  blocked while held: %s on %s" % (action_name(row), key_name(row)))

default_mappings.set_editor_property("mappings", rows + new_rows)
if not unreal.EditorAssetLibrary.save_asset(CONTEXT_PATH, only_if_is_dirty=False):
    fail("the context could not be saved")

# Read back from disk; an assignment that did not take looks exactly like one that did.
unreal.EditorAssetLibrary.load_asset(CONTEXT_PATH)
reloaded = unreal.EditorAssetLibrary.load_asset(CONTEXT_PATH)
after = list(reloaded.get_editor_property("default_key_mappings").get_editor_property("mappings") or [])
chorded = [r for r in after
           if any(isinstance(t, unreal.InputTriggerChordAction) for t in (r.get_editor_property("triggers") or []))]
blockers = [r for r in after
            if any(isinstance(t, unreal.InputTriggerChordBlocker) for t in (r.get_editor_property("triggers") or []))]
unreal.log("")
unreal.log("Live mappings after: %d (was %d)" % (len(after), len(rows)))
unreal.log("Chorded mappings on disk: %d (expected %d)" % (len(chorded), len(CHORDS)))
unreal.log("Blocked base mappings on disk: %d (expected %d)" % (len(blockers), blocked))
if len(chorded) != len(CHORDS) or len(blockers) != blocked:
    fail("the readback does not match what was written")

unreal.log("CHORD AUTHORING COMPLETE")
