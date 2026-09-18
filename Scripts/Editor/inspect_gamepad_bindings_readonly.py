"""Read-only: report every gamepad key IMC_Combat already uses, and which actions have no pad binding.

Nothing is opened for edit and nothing is saved. This exists so a proposed chord is checked against
what is actually bound rather than against an assumption.

UE 5.7 moved UInputMappingContext::Mappings to deprecated and it reads empty; the live rows are in
DefaultKeyMappings.Mappings. Reading the wrong one reports zero bindings on an asset that has dozens.
"""
import unreal

CONTEXT = "/Game/Input/IMC_Combat"
WANTED = ("IA_ThreatFocus", "IA_CycleTargetLeft", "IA_CycleTargetRight", "IA_Designate", "IA_SkipCinematic")


def live_mappings(context):
    default = context.get_editor_property("default_key_mappings")
    rows = default.get_editor_property("mappings") if default else None
    if rows:
        return list(rows)
    unreal.log_warning("default_key_mappings was empty; falling back to the deprecated array")
    return list(context.get_editor_property("mappings") or [])


context = unreal.EditorAssetLibrary.load_asset(CONTEXT)
if not context:
    unreal.log_error("MISSING %s" % CONTEXT)
    raise SystemExit(1)

rows = live_mappings(context)
unreal.log("IMC_Combat live mappings: %d" % len(rows))

gamepad, by_action = [], {}
for row in rows:
    action = row.get_editor_property("action")
    key = row.get_editor_property("key")
    name = action.get_name() if action else "<none>"
    key_name = str(key.get_editor_property("key_name")) if key else "<none>"
    by_action.setdefault(name, []).append(key_name)
    if key_name.startswith("Gamepad"):
        modifiers = row.get_editor_property("modifiers") or []
        gamepad.append((key_name, name, len(modifiers)))

unreal.log("")
unreal.log("--- gamepad keys already in use (%d) ---" % len(gamepad))
for key_name, action, modifier_count in sorted(gamepad):
    unreal.log("  %-40s -> %s%s" % (key_name, action, "  [%d modifier(s)]" % modifier_count if modifier_count else ""))

unreal.log("")
unreal.log("--- the five targeting actions ---")
for wanted in WANTED:
    keys = by_action.get(wanted)
    if keys is None:
        unreal.log_warning("  %-22s NOT PRESENT in this context" % wanted)
    else:
        pad = [k for k in keys if k.startswith("Gamepad")]
        unreal.log("  %-22s keys=%s  gamepad=%s" % (wanted, ", ".join(keys), ", ".join(pad) if pad else "NONE"))

unreal.log("")
unreal.log("GAMEPAD INSPECTION COMPLETE")
